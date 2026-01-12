/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
   Copyright 2025 btop contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#include <Availability.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504

#include "apple_silicon_gpu.hpp"

#include <dlfcn.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <algorithm>
#include <cstring>

#include "../btop_log.hpp"
#include "../btop_shared.hpp"

//? IOHIDSensor declarations for GPU temperature
extern "C" {
	typedef struct __IOHIDEvent* IOHIDEventRef;
	typedef struct __IOHIDServiceClient* IOHIDServiceClientRef;
#ifdef __LP64__
	typedef double IOHIDFloat;
#else
	typedef float IOHIDFloat;
#endif

#define IOHIDEventFieldBase(type) (type << 16)
#define kIOHIDEventTypeTemperature 15

	IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
	int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client, CFDictionaryRef match);
	CFArrayRef IOHIDEventSystemClientCopyServices(IOHIDEventSystemClientRef);
	IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef, int64_t, int32_t, int64_t);
	CFStringRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef property);
	IOHIDFloat IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);
}

namespace Gpu {

	//? Function pointer definitions
	CFDictionaryRef (*IOReportCopyChannelsInGroup)(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t) = nullptr;
	void (*IOReportMergeChannels)(CFDictionaryRef, CFDictionaryRef, CFTypeRef) = nullptr;
	IOReportSubscriptionRef (*IOReportCreateSubscription)(void*, CFMutableDictionaryRef, CFMutableDictionaryRef*, uint64_t, CFTypeRef) = nullptr;
	CFDictionaryRef (*IOReportCreateSamples)(IOReportSubscriptionRef, CFMutableDictionaryRef, CFTypeRef) = nullptr;
	CFDictionaryRef (*IOReportCreateSamplesDelta)(CFDictionaryRef, CFDictionaryRef, CFTypeRef) = nullptr;
	CFStringRef (*IOReportChannelGetGroup)(CFDictionaryRef) = nullptr;
	CFStringRef (*IOReportChannelGetSubGroup)(CFDictionaryRef) = nullptr;
	CFStringRef (*IOReportChannelGetChannelName)(CFDictionaryRef) = nullptr;
	CFStringRef (*IOReportChannelGetUnitLabel)(CFDictionaryRef) = nullptr;
	int32_t (*IOReportStateGetCount)(CFDictionaryRef) = nullptr;
	CFStringRef (*IOReportStateGetNameForIndex)(CFDictionaryRef, int32_t) = nullptr;
	int64_t (*IOReportStateGetResidency)(CFDictionaryRef, int32_t) = nullptr;
	int64_t (*IOReportSimpleGetIntegerValue)(CFDictionaryRef, int32_t) = nullptr;

	//? Global Apple Silicon GPU instance
	AppleSiliconGpu apple_silicon_gpu;

	//? Helper to get current time in nanoseconds
	static uint64_t get_time_ns() {
		static mach_timebase_info_data_t timebase = {0, 0};
		if (timebase.denom == 0) {
			mach_timebase_info(&timebase);
		}
		return mach_absolute_time() * timebase.numer / timebase.denom;
	}

	//? Helper to convert CFString to std::string
	static std::string cf_string_to_string(CFStringRef cf_str) {
		if (cf_str == nullptr) return "";
		CFIndex length = CFStringGetLength(cf_str);
		CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
		std::string result(max_size, '\0');
		if (CFStringGetCString(cf_str, result.data(), max_size, kCFStringEncodingUTF8)) {
			result.resize(std::strlen(result.c_str()));
			return result;
		}
		return "";
	}

	//? Helper to create matching dictionary for IOHIDSensors
	static CFDictionaryRef create_hid_matching(int page, int usage) {
		CFNumberRef nums[2];
		CFStringRef keys[2];

		keys[0] = CFStringCreateWithCString(kCFAllocatorDefault, "PrimaryUsagePage", kCFStringEncodingUTF8);
		keys[1] = CFStringCreateWithCString(kCFAllocatorDefault, "PrimaryUsage", kCFStringEncodingUTF8);
		nums[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &page);
		nums[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage);

		CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault,
			(const void**)keys, (const void**)nums, 2,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		CFRelease(keys[0]);
		CFRelease(keys[1]);
		CFRelease(nums[0]);
		CFRelease(nums[1]);

		return dict;
	}

	AppleSiliconGpu::AppleSiliconGpu() = default;

	AppleSiliconGpu::~AppleSiliconGpu() {
		shutdown();
	}

	//? Helper to get GPU core count from IORegistry
	static int get_gpu_core_count() {
		io_iterator_t iterator;
		CFMutableDictionaryRef matching = IOServiceMatching("AGXAccelerator");
		if (matching == nullptr) {
			matching = IOServiceMatching("AppleAGXHW");
		}
		if (matching == nullptr) {
			return 0;
		}

		if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != kIOReturnSuccess) {
			return 0;
		}

		int gpu_cores = 0;
		io_object_t device;
		while ((device = IOIteratorNext(iterator)) != 0) {
			CFMutableDictionaryRef properties = nullptr;
			if (IORegistryEntryCreateCFProperties(device, &properties, kCFAllocatorDefault, 0) == kIOReturnSuccess) {
				//? Try to get gpu-core-count property
				CFNumberRef core_count = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("gpu-core-count"));
				if (core_count != nullptr && CFGetTypeID(core_count) == CFNumberGetTypeID()) {
					CFNumberGetValue(core_count, kCFNumberIntType, &gpu_cores);
				}
				CFRelease(properties);
			}
			IOObjectRelease(device);
			if (gpu_cores > 0) break;
		}
		IOObjectRelease(iterator);
		return gpu_cores;
	}

	bool AppleSiliconGpu::init() {
		if (initialized) return true;

		//? Check if running on Apple Silicon
		char cpu_brand[256] = {0};
		size_t size = sizeof(cpu_brand);
		if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &size, nullptr, 0) != 0) {
			Logger::debug("AppleSiliconGpu: Failed to get CPU brand string");
			return false;
		}

		std::string brand(cpu_brand);
		is_apple_silicon = (brand.find("Apple") != std::string::npos);

		if (not is_apple_silicon) {
			Logger::debug("AppleSiliconGpu: Not running on Apple Silicon");
			return false;
		}

		//? Get GPU core count
		int gpu_cores = get_gpu_core_count();
		Shared::gpuCoreCount = gpu_cores;  //? Store in shared namespace for CPU box display
		std::string cores_str = (gpu_cores > 0) ? " " + std::to_string(gpu_cores) + " GPUs" : " GPU";

		//? Set GPU name based on chip (use brand string + core count)
		gpu_name = brand + cores_str;

		//? Determine ANE core count based on chip
		//? All M1/M2/M3/M4 variants have 16-core Neural Engine, Ultra has 32 (2x fused)
		if (brand.find("Ultra") != std::string::npos) {
			Shared::aneCoreCount = 32;
		} else if (brand.find("Apple M") != std::string::npos) {
			Shared::aneCoreCount = 16;
		}

		Logger::debug("AppleSiliconGpu: Detected {} ({} GPU cores, {} ANE cores)", gpu_name, gpu_cores, Shared::aneCoreCount);

		//? Load IOReport library
		if (not load_ioreport_library()) {
			Logger::warning("AppleSiliconGpu: Failed to load IOReport library");
			return false;
		}

		//? Read GPU frequency table from IORegistry
		if (not read_gpu_freq_table()) {
			Logger::warning("AppleSiliconGpu: Failed to read GPU frequency table");
			// Continue anyway - we can still get other metrics
		}

		//? Setup IOReport subscriptions
		if (not setup_subscriptions()) {
			Logger::warning("AppleSiliconGpu: Failed to setup IOReport subscriptions");
			shutdown();
			return false;
		}

		initialized = true;
		Logger::info("AppleSiliconGpu: Successfully initialized GPU monitoring");
		return true;
	}

	void AppleSiliconGpu::shutdown() {
		if (prev_sample != nullptr) {
			CFRelease(prev_sample);
			prev_sample = nullptr;
		}
		if (channels != nullptr) {
			CFRelease(channels);
			channels = nullptr;
		}
		// subscription is managed by CoreFoundation, don't release manually
		subscription = nullptr;

		if (ioreport_lib_handle != nullptr) {
			dlclose(ioreport_lib_handle);
			ioreport_lib_handle = nullptr;
		}

		initialized = false;
	}

	bool AppleSiliconGpu::load_ioreport_library() {
		ioreport_lib_handle = dlopen("/usr/lib/libIOReport.dylib", RTLD_NOW);
		if (ioreport_lib_handle == nullptr) {
			Logger::debug("AppleSiliconGpu: dlopen failed: {}", dlerror());
			return false;
		}

		//? Load all required functions
		#define LOAD_FUNC(name) \
			name = reinterpret_cast<decltype(name)>(dlsym(ioreport_lib_handle, #name)); \
			if (name == nullptr) { \
				Logger::debug("AppleSiliconGpu: Failed to load {}", #name); \
				return false; \
			}

		LOAD_FUNC(IOReportCopyChannelsInGroup)
		LOAD_FUNC(IOReportMergeChannels)
		LOAD_FUNC(IOReportCreateSubscription)
		LOAD_FUNC(IOReportCreateSamples)
		LOAD_FUNC(IOReportCreateSamplesDelta)
		LOAD_FUNC(IOReportChannelGetGroup)
		LOAD_FUNC(IOReportChannelGetSubGroup)
		LOAD_FUNC(IOReportChannelGetChannelName)
		LOAD_FUNC(IOReportChannelGetUnitLabel)
		LOAD_FUNC(IOReportStateGetCount)
		LOAD_FUNC(IOReportStateGetNameForIndex)
		LOAD_FUNC(IOReportStateGetResidency)
		LOAD_FUNC(IOReportSimpleGetIntegerValue)

		#undef LOAD_FUNC

		Logger::debug("AppleSiliconGpu: Successfully loaded IOReport library");
		return true;
	}

	bool AppleSiliconGpu::setup_subscriptions() {
		//? Get GPU Stats channels (for frequency/utilization)
		CFStringRef gpu_stats_group = CFStringCreateWithCString(kCFAllocatorDefault, "GPU Stats", kCFStringEncodingUTF8);
		CFDictionaryRef gpu_stats_channels = IOReportCopyChannelsInGroup(gpu_stats_group, nullptr, 0, 0, 0);
		CFRelease(gpu_stats_group);

		if (gpu_stats_channels == nullptr) {
			Logger::debug("AppleSiliconGpu: Failed to get GPU Stats channels");
			return false;
		}

		//? Get Energy Model channels (for power consumption - CPU, GPU, ANE)
		CFStringRef energy_group = CFStringCreateWithCString(kCFAllocatorDefault, "Energy Model", kCFStringEncodingUTF8);
		CFDictionaryRef energy_channels = IOReportCopyChannelsInGroup(energy_group, nullptr, 0, 0, 0);
		CFRelease(energy_group);

		if (energy_channels == nullptr) {
			Logger::debug("AppleSiliconGpu: Failed to get Energy Model channels");
			CFRelease(gpu_stats_channels);
			return false;
		}

		//? Get H11ANE channels (for ANE activity tracking)
		CFStringRef ane_group = CFStringCreateWithCString(kCFAllocatorDefault, "H11ANE", kCFStringEncodingUTF8);
		CFDictionaryRef ane_channels = IOReportCopyChannelsInGroup(ane_group, nullptr, 0, 0, 0);
		CFRelease(ane_group);

		//? Merge channels
		channels = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, gpu_stats_channels);
		IOReportMergeChannels(channels, energy_channels, nullptr);
		if (ane_channels != nullptr) {
			IOReportMergeChannels(channels, ane_channels, nullptr);
			CFRelease(ane_channels);
			Logger::debug("AppleSiliconGpu: Added H11ANE channels for ANE tracking");
		}
		CFRelease(gpu_stats_channels);
		CFRelease(energy_channels);

		//? Create subscription
		CFMutableDictionaryRef sub_channels = nullptr;
		subscription = IOReportCreateSubscription(nullptr, channels, &sub_channels, 0, nullptr);

		if (subscription == nullptr) {
			Logger::debug("AppleSiliconGpu: Failed to create IOReport subscription");
			return false;
		}

		//? Take initial sample
		prev_sample = IOReportCreateSamples(subscription, channels, nullptr);
		prev_sample_time = get_time_ns();

		if (prev_sample == nullptr) {
			Logger::debug("AppleSiliconGpu: Failed to take initial sample");
			return false;
		}

		Logger::debug("AppleSiliconGpu: Successfully setup IOReport subscriptions");
		return true;
	}

	bool AppleSiliconGpu::read_gpu_freq_table() {
		io_iterator_t iterator;
		CFMutableDictionaryRef matching = IOServiceMatching("AppleARMIODevice");

		if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != kIOReturnSuccess) {
			return false;
		}

		io_object_t device;
		bool found = false;

		while ((device = IOIteratorNext(iterator)) != 0) {
			io_name_t name;
			IORegistryEntryGetName(device, name);

			if (std::strcmp(name, "pmgr") == 0) {
				CFMutableDictionaryRef properties = nullptr;
				if (IORegistryEntryCreateCFProperties(device, &properties, kCFAllocatorDefault, 0) == kIOReturnSuccess) {
					//? Look for voltage-states9 which contains GPU frequency/voltage pairs
					CFDataRef voltage_states = (CFDataRef)CFDictionaryGetValue(properties, CFSTR("voltage-states9"));

					if (voltage_states != nullptr and CFGetTypeID(voltage_states) == CFDataGetTypeID()) {
						CFIndex length = CFDataGetLength(voltage_states);
						const uint8_t* bytes = CFDataGetBytePtr(voltage_states);

						//? Parse as 8-byte chunks: [freq(4), voltage(4)]
						gpu_freq_table.clear();
						for (CFIndex i = 0; i + 8 <= length; i += 8) {
							uint32_t freq_hz;
							std::memcpy(&freq_hz, bytes + i, sizeof(freq_hz));
							double freq_mhz = freq_hz / 1000000.0;
							if (freq_mhz > 0) {
								gpu_freq_table.push_back(freq_mhz);
							}
						}

						if (not gpu_freq_table.empty()) {
							std::sort(gpu_freq_table.begin(), gpu_freq_table.end());
							max_gpu_freq_mhz = gpu_freq_table.back();
							found = true;
							std::string freq_list;
							for (auto f : gpu_freq_table) {
								if (not freq_list.empty()) freq_list += ", ";
								freq_list += std::to_string(static_cast<int>(f));
							}
							Logger::debug("AppleSiliconGpu: Found {} GPU frequencies: {} MHz (max = {} MHz)",
							              gpu_freq_table.size(), freq_list, max_gpu_freq_mhz);
						}
					}
					CFRelease(properties);
				}
			}
			IOObjectRelease(device);
			if (found) break;
		}

		IOObjectRelease(iterator);
		return found;
	}

	double AppleSiliconGpu::get_gpu_temperature() {
		//? Try IOHIDSensors for GPU temperature
		CFDictionaryRef matching = create_hid_matching(0xff00, 5);
		IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
		IOHIDEventSystemClientSetMatching(system, matching);
		CFArrayRef services = IOHIDEventSystemClientCopyServices(system);

		double max_gpu_temp = 0.0;  //? Track maximum temperature (hottest sensor)

		if (services != nullptr) {
			CFIndex count = CFArrayGetCount(services);
			for (CFIndex i = 0; i < count; i++) {
				IOHIDServiceClientRef service = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
				if (service != nullptr) {
					CFStringRef name = IOHIDServiceClientCopyProperty(service, CFSTR("Product"));
					if (name != nullptr) {
						std::string sensor_name = cf_string_to_string(name);
						CFRelease(name);

						//? Look for GPU/SoC temperature sensors:
						//? - "GPU MTR Temp Sensor" - GPU metal temperature (older chips)
						//? - "PMU tdie" - SoC die temperature (M4+, GPU integrated in SoC)
						//? - Sensors with "GPU" in name
						//? - "Tg" prefixed sensors (GPU die temperatures on some chips)
						bool is_gpu_sensor = (sensor_name.find("GPU") != std::string::npos) or
						                     (sensor_name.find("gpu") != std::string::npos) or
						                     (sensor_name.find("PMU tdie") != std::string::npos) or
						                     (sensor_name.substr(0, 2) == "Tg");

						if (is_gpu_sensor) {
							IOHIDEventRef event = IOHIDServiceClientCopyEvent(service,
								kIOHIDEventTypeTemperature, 0, 0);
							if (event != nullptr) {
								double temp = IOHIDEventGetFloatValue(event,
									IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
								if (temp > 0 and temp < 150) {  // Sanity check
									if (temp > max_gpu_temp) {
										max_gpu_temp = temp;
									}
									Logger::debug("AppleSiliconGpu: Found GPU temp sensor '{}' = {:.1f}C (max so far: {:.1f}C)",
									              sensor_name, temp, max_gpu_temp);
								}
								CFRelease(event);
							}
						}
					}
				}
			}
			CFRelease(services);
		}

		CFRelease(system);
		CFRelease(matching);

		if (max_gpu_temp > 0) {
			return max_gpu_temp;
		}

		return 0.0;
	}

	double AppleSiliconGpu::get_cpu_temperature() {
		//? Use IOHIDSensors directly to find CPU temperature sensors
		//? Similar approach to get_gpu_temperature() but looking for CPU-related sensors
		CFDictionaryRef matching = create_hid_matching(0xff00, 5);
		IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
		IOHIDEventSystemClientSetMatching(system, matching);
		CFArrayRef services = IOHIDEventSystemClientCopyServices(system);

		std::vector<double> cpu_temps;
		static bool first_call = true;

		if (services != nullptr) {
			CFIndex count = CFArrayGetCount(services);
			for (CFIndex i = 0; i < count; i++) {
				IOHIDServiceClientRef service = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
				if (service != nullptr) {
					CFStringRef name = IOHIDServiceClientCopyProperty(service, CFSTR("Product"));
					if (name != nullptr) {
						std::string sensor_name = cf_string_to_string(name);
						CFRelease(name);

						//? Look for CPU temperature sensors:
						//? - "PMU TP*" - CPU thermal protection sensors (M1/M2/M3)
						//? - "pACC*" / "eACC*" - Performance/Efficiency core temps (some chips)
						//? - "Tp*" prefix - CPU die temperatures
						//? - "SOC MTR Temp Sensor*" - SoC (includes CPU) temp
						//? - "PMU tdie*" - Die temperature (CPU is part of die)
						//? - Sensors with "CPU" in name
						bool is_cpu_sensor = false;

						//? Check prefixes: PMU TP, Tp, pACC, eACC
						if (sensor_name.substr(0, 6) == "PMU TP" or
						    sensor_name.substr(0, 2) == "Tp" or
						    sensor_name.substr(0, 4) == "pACC" or
						    sensor_name.substr(0, 4) == "eACC") {
							is_cpu_sensor = true;
						}
						//? Check for CPU in name
						else if (sensor_name.find("CPU") != std::string::npos or
						         sensor_name.find("cpu") != std::string::npos) {
							is_cpu_sensor = true;
						}
						//? SOC temperature (CPU is integrated in SoC)
						else if (sensor_name.find("SOC MTR") != std::string::npos) {
							is_cpu_sensor = true;
						}
						//? PMU tdie sensors (die temp which includes CPU)
						//? Note: Exclude GPU-related ones (already handled by get_gpu_temperature)
						else if (sensor_name.find("PMU tdie") != std::string::npos and
						         sensor_name.find("GPU") == std::string::npos) {
							is_cpu_sensor = true;
						}

						if (is_cpu_sensor) {
							IOHIDEventRef event = IOHIDServiceClientCopyEvent(service,
								kIOHIDEventTypeTemperature, 0, 0);
							if (event != nullptr) {
								double temp = IOHIDEventGetFloatValue(event,
									IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
								if (temp > 0 and temp < 150) {  // Sanity check
									cpu_temps.push_back(temp);
									if (first_call) {
										Logger::debug("AppleSiliconGpu: Found CPU temp sensor '{}' = {:.1f}C",
										              sensor_name, temp);
									}
								}
								CFRelease(event);
							}
						}
					}
				}
			}
			CFRelease(services);
		}

		CFRelease(system);
		CFRelease(matching);

		first_call = false;

		//? Return average of all CPU temperature sensors
		if (not cpu_temps.empty()) {
			double sum = 0.0;
			for (double t : cpu_temps) {
				sum += t;
			}
			return sum / static_cast<double>(cpu_temps.size());
		}

		return 0.0;
	}

	void AppleSiliconGpu::parse_channels(CFDictionaryRef delta, double elapsed_seconds,
	                                     double& out_freq_mhz, double& out_usage_percent,
	                                     double& out_gpu_power_watts, double& out_temp_celsius,
	                                     double& out_cpu_power_watts, double& out_ane_power_watts,
	                                     double& out_ane_activity_cmds) {
		//? Only log every Nth call to avoid spam
		static int parse_count = 0;
		static bool first_call = true;
		bool do_debug = first_call or (parse_count++ % 100 == 0);

		if (delta == nullptr) {
			if (do_debug) Logger::debug("AppleSiliconGpu: parse_channels called with null delta");
			return;
		}

		CFArrayRef channel_array = (CFArrayRef)CFDictionaryGetValue(delta, CFSTR("IOReportChannels"));
		if (channel_array == nullptr or CFGetTypeID(channel_array) != CFArrayGetTypeID()) {
			if (do_debug) Logger::debug("AppleSiliconGpu: No IOReportChannels array in delta");
			return;
		}

		out_freq_mhz = 0.0;
		out_usage_percent = 0.0;
		out_gpu_power_watts = 0.0;
		out_temp_celsius = 0.0;
		out_cpu_power_watts = 0.0;
		out_ane_power_watts = 0.0;
		out_ane_activity_cmds = 0.0;

		double gpu_energy_joules = 0.0;
		double cpu_energy_joules = 0.0;
		double ane_energy_joules = 0.0;
		int64_t ane_commands_delta = 0;
		bool found_gpuph = false;
		bool found_gpu_energy = false;
		bool found_cpu_energy = false;
		bool found_ane_energy = false;
		bool found_ane_commands = false;

		//? For temperature: accumulate sum and count from IOReport
		double temp_sum = 0.0;
		int64_t temp_count = 0;

		CFIndex count = CFArrayGetCount(channel_array);
		if (do_debug) Logger::debug("AppleSiliconGpu: Processing {} channels", count);

		for (CFIndex i = 0; i < count; i++) {
			CFDictionaryRef channel = (CFDictionaryRef)CFArrayGetValueAtIndex(channel_array, i);
			if (channel == nullptr or CFGetTypeID(channel) != CFDictionaryGetTypeID()) {
				continue;
			}

			std::string group = cf_string_to_string(IOReportChannelGetGroup(channel));
			std::string subgroup = cf_string_to_string(IOReportChannelGetSubGroup(channel));
			std::string channel_name = cf_string_to_string(IOReportChannelGetChannelName(channel));

			//? Log relevant channels for debugging (only on first call)
			if (first_call and (group.find("GPU") != std::string::npos or
			                    group.find("ANE") != std::string::npos or
			                    group == "H11ANE" or
			                    channel_name.find("GPU") != std::string::npos or
			                    channel_name.find("ANE") != std::string::npos or
			                    channel_name.find("CPU") != std::string::npos)) {
				Logger::debug("AppleSiliconGpu: Found channel group='{}' subgroup='{}' name='{}'",
				              group, subgroup, channel_name);
			}

			//? GPU frequency and utilization from "GPU Stats" / "GPU Performance States" / "GPUPH"
			if (group == "GPU Stats" and subgroup == "GPU Performance States" and channel_name == "GPUPH") {
				found_gpuph = true;
				int32_t state_count = IOReportStateGetCount(channel);
				int64_t total_time = 0;
				int64_t active_time = 0;
				double weighted_freq = 0.0;

				if (do_debug) Logger::debug("AppleSiliconGpu: GPUPH has {} states, freq_table has {} entries",
				                            state_count, gpu_freq_table.size());

				for (int32_t s = 0; s < state_count; s++) {
					std::string state_name = cf_string_to_string(IOReportStateGetNameForIndex(channel, s));
					int64_t residency_ns = IOReportStateGetResidency(channel, s);

					if (do_debug and first_call) {
						Logger::debug("AppleSiliconGpu: State[{}] '{}' = {} ns", s, state_name, residency_ns);
					}

					total_time += residency_ns;

					//? Skip idle states (OFF, IDLE)
					if (state_name == "OFF" or state_name == "IDLE" or state_name.empty()) {
						continue;
					}

					//? Map P-state to frequency from table
					//? P-states are named "P1", "P2", etc. - map P1 -> freq_table[0], P2 -> freq_table[1]
					double freq = 0.0;
					if (state_name.length() > 1 and state_name[0] == 'P') {
						try {
							int pstate_idx = std::stoi(state_name.substr(1)) - 1;  // P1 = index 0
							if (pstate_idx >= 0 and static_cast<size_t>(pstate_idx) < gpu_freq_table.size()) {
								freq = gpu_freq_table[pstate_idx];
							} else if (do_debug) {
								Logger::debug("AppleSiliconGpu: P-state index {} out of freq_table range", pstate_idx);
							}
						} catch (...) {
							if (do_debug) Logger::debug("AppleSiliconGpu: Could not parse P-state index from '{}'", state_name);
						}
					} else {
						//? Try parsing as direct frequency value (fallback for older systems)
						try {
							freq = std::stod(state_name);
						} catch (...) {
							if (do_debug) Logger::debug("AppleSiliconGpu: Could not parse '{}' as frequency", state_name);
						}
					}

					if (freq > 0 and residency_ns > 0) {
						weighted_freq += freq * static_cast<double>(residency_ns);
						active_time += residency_ns;
					}
				}

				if (active_time > 0) {
					out_freq_mhz = weighted_freq / static_cast<double>(active_time);
					if (do_debug) Logger::debug("AppleSiliconGpu: Weighted freq = {} MHz (active_time = {} ns)", out_freq_mhz, active_time);
				}

				//? Calculate GPU usage as percentage of active time (non-OFF state)
				if (total_time > 0) {
					out_usage_percent = (static_cast<double>(active_time) / static_cast<double>(total_time)) * 100.0;
					out_usage_percent = std::min(100.0, std::max(0.0, out_usage_percent));
					if (do_debug) Logger::debug("AppleSiliconGpu: Usage = {}% (active={} total={} ns)",
					                            out_usage_percent, active_time, total_time);
				}
			}

			//? GPU temperature from "GPU Stats" / "Temperature"
			//? Look for "Average Sum" or individual sensor "TgXXa Sum" channels
			if (group == "GPU Stats" and subgroup == "Temperature") {
				int64_t value = IOReportSimpleGetIntegerValue(channel, 0);

				//? "Average Sum" gives cumulative temperature, "Average Sum Count" gives sample count
				//? Similarly for individual sensors like "Tg0a Sum" / "Tg0a Sum Count"
				if (channel_name == "Average Sum") {
					temp_sum = static_cast<double>(value);
					if (do_debug) Logger::debug("AppleSiliconGpu: Temperature Average Sum = {}", value);
				} else if (channel_name == "Average Sum Count") {
					temp_count = value;
					if (do_debug) Logger::debug("AppleSiliconGpu: Temperature Average Sum Count = {}", value);
				}
			}

			//? Energy Model channels for power consumption
			if (group == "Energy Model") {
				std::string unit = cf_string_to_string(IOReportChannelGetUnitLabel(channel));
				int64_t energy_value = IOReportSimpleGetIntegerValue(channel, 0);

				//? Helper lambda to convert energy to joules
				auto to_joules = [&](int64_t value) -> double {
					if (unit == "mJ") {
						return value / 1000.0;
					} else if (unit == "uJ") {
						return value / 1000000.0;
					} else if (unit == "nJ") {
						return value / 1000000000.0;
					} else {
						//? Assume nanojoules if unit is unknown
						return value / 1000000000.0;
					}
				};

				//? GPU power from "GPU Energy" (or "DIE_X_GPU Energy" for multi-die)
				if (channel_name == "GPU Energy" or
				    channel_name.find("GPU Energy") != std::string::npos) {
					found_gpu_energy = true;
					gpu_energy_joules += to_joules(energy_value);
					if (do_debug) Logger::debug("AppleSiliconGpu: GPU Energy channel: value={} unit='{}'", energy_value, unit);
				}

				//? ANE power from "ANE" subgroup channels
				if (subgroup == "ANE" or channel_name == "ANE" or
				    channel_name.find("ANE Energy") != std::string::npos) {
					found_ane_energy = true;
					ane_energy_joules += to_joules(energy_value);
					if (do_debug) Logger::debug("AppleSiliconGpu: ANE Energy channel: value={} unit='{}'", energy_value, unit);
				}

				//? CPU power from CPU core channels (EACC_CPU*, PACC0_CPU*, etc.)
				//? CPU cores show up as individual channels or aggregate "CPU Energy"
				if (channel_name.find("CPU") != std::string::npos and
				    channel_name.find("GPU") == std::string::npos) {
					found_cpu_energy = true;
					cpu_energy_joules += to_joules(energy_value);
					if (do_debug and first_call) {
						Logger::debug("AppleSiliconGpu: CPU Energy channel '{}': value={} unit='{}'",
						              channel_name, energy_value, unit);
					}
				}
			}

			//? ANE activity from H11ANE / "H11ANE Events" / "ANECPU Commands Sent"
			if (group == "H11ANE" and subgroup == "H11ANE Events" and
			    channel_name == "ANECPU Commands Sent") {
				found_ane_commands = true;
				int64_t commands = IOReportSimpleGetIntegerValue(channel, 0);
				ane_commands_delta = commands;  // This is already a delta from IOReportCreateSamplesDelta
				if (do_debug) Logger::debug("AppleSiliconGpu: ANE Commands Sent delta = {}", commands);
			}
		}

		if (do_debug and not found_gpuph) {
			Logger::debug("AppleSiliconGpu: GPUPH channel not found");
		}
		if (do_debug and not found_gpu_energy) {
			Logger::debug("AppleSiliconGpu: GPU Energy channel not found");
		}
		if (do_debug and not found_cpu_energy) {
			Logger::debug("AppleSiliconGpu: CPU Energy channel not found");
		}
		if (do_debug and not found_ane_energy) {
			Logger::debug("AppleSiliconGpu: ANE Energy channel not found");
		}
		if (do_debug and not found_ane_commands) {
			Logger::debug("AppleSiliconGpu: ANE Commands channel not found");
		}

		//? Convert energy to power: watts = joules / seconds
		if (elapsed_seconds > 0) {
			if (gpu_energy_joules > 0) {
				out_gpu_power_watts = gpu_energy_joules / elapsed_seconds;
				if (do_debug) Logger::debug("AppleSiliconGpu: GPU Power = {} W (energy = {} J, elapsed = {} s)",
				                            out_gpu_power_watts, gpu_energy_joules, elapsed_seconds);
			}
			if (cpu_energy_joules > 0) {
				out_cpu_power_watts = cpu_energy_joules / elapsed_seconds;
				if (do_debug) Logger::debug("AppleSiliconGpu: CPU Power = {} W", out_cpu_power_watts);
			}
			if (ane_energy_joules > 0) {
				out_ane_power_watts = ane_energy_joules / elapsed_seconds;
				if (do_debug) Logger::debug("AppleSiliconGpu: ANE Power = {} W", out_ane_power_watts);
			}

			//? ANE activity: commands per second
			if (ane_commands_delta > 0) {
				out_ane_activity_cmds = static_cast<double>(ane_commands_delta) / elapsed_seconds;
				if (do_debug) Logger::debug("AppleSiliconGpu: ANE Activity = {} C/s", out_ane_activity_cmds);
			}
		}

		//? Calculate average temperature
		//? IOReport temperature values are in centiCelsius (hundredths of a degree)
		if (temp_count > 0 and temp_sum > 0) {
			out_temp_celsius = (temp_sum / static_cast<double>(temp_count)) / 100.0;
			if (do_debug) Logger::debug("AppleSiliconGpu: Temperature = {} C (sum={}, count={})",
			                            out_temp_celsius, temp_sum, temp_count);
		}

		first_call = false;
	}

	AppleSiliconGpuMetrics AppleSiliconGpu::collect() {
		static int collect_count = 0;
		bool do_debug = (collect_count++ % 100 == 0);  // Log every 100th collection

		AppleSiliconGpuMetrics metrics;

		if (not initialized or subscription == nullptr) {
			if (do_debug) Logger::debug("AppleSiliconGpu: collect() - not initialized or no subscription");
			return metrics;
		}

		//? Take new sample
		CFDictionaryRef current_sample = IOReportCreateSamples(subscription, channels, nullptr);
		uint64_t current_time = get_time_ns();

		if (current_sample == nullptr) {
			if (do_debug) Logger::debug("AppleSiliconGpu: collect() - IOReportCreateSamples returned null");
			return metrics;
		}

		//? Calculate delta
		if (prev_sample != nullptr) {
			double elapsed_seconds = (current_time - prev_sample_time) / 1e9;

			if (elapsed_seconds > 0.01) {  // At least 10ms between samples
				CFDictionaryRef delta = IOReportCreateSamplesDelta(prev_sample, current_sample, nullptr);

				if (delta != nullptr) {
					double freq_mhz = 0.0;
					double usage_percent = 0.0;
					double gpu_power_watts = 0.0;
					double temp_celsius = 0.0;
					double cpu_power_watts = 0.0;
					double ane_power_watts = 0.0;
					double ane_activity_cmds = 0.0;

					parse_channels(delta, elapsed_seconds, freq_mhz, usage_percent,
					               gpu_power_watts, temp_celsius,
					               cpu_power_watts, ane_power_watts, ane_activity_cmds);

					metrics.gpu_freq_mhz = freq_mhz;
					metrics.gpu_usage_percent = usage_percent;
					metrics.gpu_power_watts = gpu_power_watts;
					metrics.gpu_temp_celsius = temp_celsius;
					metrics.gpu_freq_max_mhz = max_gpu_freq_mhz;
					metrics.cpu_power_watts = cpu_power_watts;
					metrics.ane_power_watts = ane_power_watts;
					metrics.ane_activity_cmds = ane_activity_cmds;

					//? Update power history for averaging
					if (cpu_power_history.size() < static_cast<size_t>(POWER_AVG_SAMPLES)) {
						cpu_power_history.push_back(cpu_power_watts);
						gpu_power_history.push_back(gpu_power_watts);
						ane_power_history.push_back(ane_power_watts);
					} else {
						cpu_power_history[power_history_idx] = cpu_power_watts;
						gpu_power_history[power_history_idx] = gpu_power_watts;
						ane_power_history[power_history_idx] = ane_power_watts;
						power_history_idx = (power_history_idx + 1) % POWER_AVG_SAMPLES;
					}

					//? Calculate average power
					double cpu_avg = 0.0, gpu_avg = 0.0, ane_avg = 0.0;
					for (size_t i = 0; i < cpu_power_history.size(); i++) {
						cpu_avg += cpu_power_history[i];
						gpu_avg += gpu_power_history[i];
						ane_avg += ane_power_history[i];
					}
					if (not cpu_power_history.empty()) {
						cpu_avg /= static_cast<double>(cpu_power_history.size());
						gpu_avg /= static_cast<double>(gpu_power_history.size());
						ane_avg /= static_cast<double>(ane_power_history.size());
					}

					//? Update Shared namespace power variables
					Shared::cpuPower = cpu_power_watts;
					Shared::gpuPower = gpu_power_watts;
					Shared::anePower = ane_power_watts;
					Shared::cpuPowerAvg = cpu_avg;
					Shared::gpuPowerAvg = gpu_avg;
					Shared::anePowerAvg = ane_avg;

					//? Update peak power
					if (cpu_power_watts > Shared::cpuPowerPeak) Shared::cpuPowerPeak = cpu_power_watts;
					if (gpu_power_watts > Shared::gpuPowerPeak) Shared::gpuPowerPeak = gpu_power_watts;
					if (ane_power_watts > Shared::anePowerPeak) Shared::anePowerPeak = ane_power_watts;

					//? Update ANE activity
					Shared::aneActivity = ane_activity_cmds;

					//? Update power history for Pwr panel graphs (in mW for precision)
					long long cpu_pwr_mw = static_cast<long long>(cpu_power_watts * 1000);
					long long gpu_pwr_mw = static_cast<long long>(gpu_power_watts * 1000);
					long long ane_pwr_mw = static_cast<long long>(ane_power_watts * 1000);

					//? Push current values to history deques
					Pwr::cpu_pwr_history.push_back(cpu_pwr_mw);
					Pwr::gpu_pwr_history.push_back(gpu_pwr_mw);
					Pwr::ane_pwr_history.push_back(ane_pwr_mw);

					//? Limit history size (match graph width, ~100 points max)
					while (Pwr::cpu_pwr_history.size() > 100) Pwr::cpu_pwr_history.pop_front();
					while (Pwr::gpu_pwr_history.size() > 100) Pwr::gpu_pwr_history.pop_front();
					while (Pwr::ane_pwr_history.size() > 100) Pwr::ane_pwr_history.pop_front();

					//? Update max values for auto-scaling (in mW)
					if (cpu_pwr_mw > Pwr::cpu_pwr_max) Pwr::cpu_pwr_max = cpu_pwr_mw;
					if (gpu_pwr_mw > Pwr::gpu_pwr_max) Pwr::gpu_pwr_max = gpu_pwr_mw;
					if (ane_pwr_mw > Pwr::ane_pwr_max) Pwr::ane_pwr_max = ane_pwr_mw;

					if (do_debug) {
						Logger::debug("AppleSiliconGpu: collect() - GPU: freq={}MHz usage={}% power={}W temp={}C",
						              freq_mhz, usage_percent, gpu_power_watts, temp_celsius);
						Logger::debug("AppleSiliconGpu: collect() - CPU power={}W, ANE power={}W, ANE activity={} C/s",
						              cpu_power_watts, ane_power_watts, ane_activity_cmds);
					}

					CFRelease(delta);
				} else {
					if (do_debug) Logger::debug("AppleSiliconGpu: collect() - IOReportCreateSamplesDelta returned null");
				}
			} else {
				if (do_debug) Logger::debug("AppleSiliconGpu: collect() - elapsed time too short: {}s", elapsed_seconds);
			}

			CFRelease(prev_sample);
		} else {
			if (do_debug) Logger::debug("AppleSiliconGpu: collect() - first sample, no delta yet");
		}

		prev_sample = current_sample;
		prev_sample_time = current_time;

		//? Try IOHIDSensors first (more accurate - reads actual GPU temp sensors)
		//? Fall back to IOReport if IOHIDSensors fails
		double hid_temp = get_gpu_temperature();
		if (hid_temp > 0) {
			metrics.gpu_temp_celsius = hid_temp;
			if (do_debug) {
				Logger::debug("AppleSiliconGpu: Using IOHIDSensors temperature: {}C", hid_temp);
			}
		} else if (metrics.gpu_temp_celsius <= 0) {
			if (do_debug) {
				Logger::debug("AppleSiliconGpu: No temperature from IOHIDSensors or IOReport");
			}
		}

		//? Update shared GPU temp for Pwr panel
		Shared::gpuTemp = static_cast<long long>(metrics.gpu_temp_celsius);

		//? Also get CPU temp via IOHIDSensors for Pwr panel
		double cpu_temp = get_cpu_temperature();
		Shared::cpuTemp = static_cast<long long>(cpu_temp);
		if (do_debug) {
			Logger::debug("AppleSiliconGpu: CPU temperature: {}C", cpu_temp);
		}

		return metrics;
	}

}  // namespace Gpu

#endif  // __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
