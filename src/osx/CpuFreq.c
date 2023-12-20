#include <Availability.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504 && defined (__clang__) && defined (__aarch64__)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "CpuFreq.h"

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

/* Implementations */
int CpuFreq_init(CpuFreqData *data) {
	data->cluster_type_per_cpu = (unsigned int *)malloc(data->existingCPUs * sizeof(uint32_t));
	data->frequencies = (double *)malloc(data->existingCPUs * sizeof(double));

	/* Determine the cluster type for all CPUs */
	char buf[128];

	for (uint32_t num_cpus = 0U; num_cpus < data->existingCPUs; num_cpus++) {
		snprintf(buf, sizeof(buf), "IODeviceTree:/cpus/cpu%u", num_cpus);
		io_registry_entry_t cpu_io_registry_entry =
		    IORegistryEntryFromPath(kIOMainPortDefault, buf);
		if (cpu_io_registry_entry == MACH_PORT_NULL) {
			return -1;
		}

		CFDataRef cluster_type_ref = (CFDataRef)IORegistryEntryCreateCFProperty(
		    cpu_io_registry_entry, CFSTR("cluster-type"), kCFAllocatorDefault, 0U);
		if (cluster_type_ref == NULL) {
			IOObjectRelease(cpu_io_registry_entry);
			return -1;
		}
		if (CFDataGetLength(cluster_type_ref) != 2) {
			CFRelease(cluster_type_ref);
			IOObjectRelease(cpu_io_registry_entry);
			return -1;
		}
		UniChar cluster_type_char;
		CFDataGetBytes(cluster_type_ref, CFRangeMake(0, 2),
		               (uint8_t *)&cluster_type_char);
		CFRelease(cluster_type_ref);

		uint32_t cluster_type = 0U;
		switch (cluster_type_char) {
			case L'E':
				cluster_type = 0U;
				break;
			case L'P':
				cluster_type = 1U;
				break;
			default:
				/* Unknown cluster type */
				IOObjectRelease(cpu_io_registry_entry);
				return -1;
		}

		data->cluster_type_per_cpu[num_cpus] = cluster_type;

		IOObjectRelease(cpu_io_registry_entry);
	}

	/*
	 * Determine frequencies for per-cluster-type performance states
	 * Frequencies for the "E" cluster type are stored in voltage-states1,
	 * frequencies for the "P" cluster type are stored in voltage-states5.
	 * This seems to be hardcoded.
	 */
	const CFStringRef voltage_states_key_per_cluster[CPUFREQ_NUM_CLUSTER_TYPES] =
	    {CFSTR("voltage-states1"), CFSTR("voltage-states5")};

	io_registry_entry_t pmgr_registry_entry =
	    IORegistryEntryFromPath(kIOMainPortDefault, "IODeviceTree:/arm-io/pmgr");
	if (pmgr_registry_entry == MACH_PORT_NULL) {
		return -1;
	}
	for (size_t i = 0U; i < CPUFREQ_NUM_CLUSTER_TYPES; i++) {
		CFDataRef voltage_states_ref = (CFDataRef)IORegistryEntryCreateCFProperty(
		    pmgr_registry_entry, voltage_states_key_per_cluster[i],
		    kCFAllocatorDefault, 0U);
		if (voltage_states_ref == NULL) {
			IOObjectRelease(pmgr_registry_entry);
			return -1;
		}

		CpuFreqPowerStateFrequencies *cluster_frequencies =
		    &data->cpu_frequencies_per_cluster_type[i];
		cluster_frequencies->num_frequencies =
		    CFDataGetLength(voltage_states_ref) / 8;
		cluster_frequencies->frequencies =
		    (double *)malloc(cluster_frequencies->num_frequencies * sizeof(double));
		const uint8_t *voltage_states_data = CFDataGetBytePtr(voltage_states_ref);
		for (size_t j = 0U; j < cluster_frequencies->num_frequencies; j++) {
			uint32_t freq_value;
			memcpy(&freq_value, voltage_states_data + j * 8, 4);
			cluster_frequencies->frequencies[j] = (65536000.0 / freq_value) * 1000000;
		}
		CFRelease(voltage_states_ref);
	}
	IOObjectRelease(pmgr_registry_entry);

	/* Create subscription for CPU performance states */
	CFMutableDictionaryRef channels = IOReportCopyChannelsInGroup(
	    CFSTR("CPU Stats"), CFSTR("CPU Core Performance States"), NULL, NULL);
	if (channels == NULL) {
		return -1;
	}

	data->subscribed_channels = NULL;
	data->subscription = IOReportCreateSubscription(
	    NULL, channels, &data->subscribed_channels, 0U, NULL);

	CFRelease(channels);

	if (data->subscription == NULL) {
		return -1;
	}

	data->prev_samples = NULL;

	return 0;
}

void CpuFreq_update(CpuFreqData *data) {
	CFDictionaryRef samples = IOReportCreateSamples(
	    data->subscription, data->subscribed_channels, NULL);
	if (samples == NULL) {
		return;
	}

	if (data->prev_samples == NULL) {
		data->prev_samples = samples;
		return;
	}

	/* Residency is cumulative, we have to diff two samples to get a current view
	 */
	CFDictionaryRef samples_delta =
	    IOReportCreateSamplesDelta(data->prev_samples, samples, NULL);

	/* Iterate through performance state residencies. Index 0 is the idle
	 * residency, index 1-n is the per-performance state residency. */
	__block uint32_t cpu_i = 0U;
	IOReportIterate(samples_delta, ^(IOReportSampleRef ch) {
	  if (cpu_i >= data->existingCPUs) {
		  /* The report contains more CPUs than we know about. This should not
		   * happen. */
		  return kIOReportIterOk;  // TODO: find way to possibly stop iteration early
			                       // on error
	  }
	  const CpuFreqPowerStateFrequencies *cpu_frequencies =
		  &data->cpu_frequencies_per_cluster_type
		       [data->cluster_type_per_cpu[cpu_i]];
	  uint32_t state_count = IOReportStateGetCount(ch);
	  if (state_count != cpu_frequencies->num_frequencies + 1) {
		  /* The number of reported states does not match the number of previously
		   * queried frequencies. This should not happen. */
		  return kIOReportIterOk;  // TODO: find way to possibly stop iteration early
			                       // on error
	  }
	  double freq = 0.0;
	  double highest_residency = 0.0;
	  uint32_t highest_i = 0U;
	  for (uint32_t i = 0U; i < state_count; i++) {
		  const int64_t residency = IOReportStateGetResidency(ch, i);
		  if (residency > highest_residency) {
			  highest_i = i;
			  highest_residency = residency;
		  }
	  }
	  freq = cpu_frequencies->frequencies[highest_i == 0 ? 0 : highest_i - 1];
	  data->frequencies[cpu_i] = round(freq);

	  cpu_i++;
	  return kIOReportIterOk;
	});
	CFRelease(samples_delta);

	CFRelease(data->prev_samples);
	data->prev_samples = samples;
}

void CpuFreq_cleanup(CpuFreqData *data) {
	if (data->subscription != NULL) {
		CFRelease(data->subscription);
	}
	if (data->subscribed_channels != NULL) {
		CFRelease(data->subscribed_channels);
	}
	if (data->prev_samples != NULL) {
		CFRelease(data->prev_samples);
	}
	free(data->cluster_type_per_cpu);
	free(data->frequencies);
	for (size_t i = 0U; i < CPUFREQ_NUM_CLUSTER_TYPES; i++) {
		free(data->cpu_frequencies_per_cluster_type[i].frequencies);
	}
}
#endif
