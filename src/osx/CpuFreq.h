#ifndef HEADER_CpuFreq
#define HEADER_CpuFreq

#include <Availability.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504 && defined(__clang__) && defined (__aarch64__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOTypes.h>

/* Private API definitions from libIOReport*/
enum {
  kIOReportIterOk = 0,
};
typedef struct IOReportSubscription *IOReportSubscriptionRef;
typedef CFDictionaryRef IOReportSampleRef;
typedef CFDictionaryRef IOReportChannelRef;
typedef int (^io_report_iterate_callback_t)(IOReportSampleRef ch);
extern void IOReportIterate(CFDictionaryRef samples,
                            io_report_iterate_callback_t callback);
extern CFMutableDictionaryRef
IOReportCopyChannelsInGroup(CFStringRef, CFStringRef, void *, void *);
extern IOReportSubscriptionRef
IOReportCreateSubscription(void *a, CFMutableDictionaryRef desiredChannels,
                           CFMutableDictionaryRef *subbedChannels,
                           uint64_t channel_id, CFTypeRef b);
extern CFDictionaryRef
IOReportCreateSamples(IOReportSubscriptionRef iorsub,
                      CFMutableDictionaryRef subbedChannels, CFTypeRef a);
extern uint32_t IOReportStateGetCount(IOReportChannelRef ch);
extern uint64_t IOReportStateGetResidency(IOReportChannelRef ch,
                                          uint32_t index);
extern CFDictionaryRef IOReportCreateSamplesDelta(CFDictionaryRef prev,
                                                  CFDictionaryRef current,
                                                  CFTypeRef a);

/* Definitions */
typedef struct {
  uint32_t num_frequencies;
  double *frequencies;
} CpuFreqPowerStateFrequencies;

/*
 * Seems to be hardcoded for now on all Apple Silicon platforms, no way to get
 * it dynamically. Current cluster types are "E" for efficiency cores and "P"
 * for performance cores.
 */
#define CPUFREQ_NUM_CLUSTER_TYPES 2

typedef struct {
  /* Number of CPUs */
  unsigned int existingCPUs;

  /* existingCPUs records, containing which CPU belongs to which cluster type
   * ("E": 0, "P": 1) */
  uint32_t *cluster_type_per_cpu;

  /* Frequencies for all power states per cluster type */
  CpuFreqPowerStateFrequencies
      cpu_frequencies_per_cluster_type[CPUFREQ_NUM_CLUSTER_TYPES];

  /* IOReport subscription handlers */
  IOReportSubscriptionRef subscription;
  CFMutableDictionaryRef subscribed_channels;

  /* Last IOReport sample */
  CFDictionaryRef prev_samples;

  /* existingCPUs records, containing last determined frequency per CPU in MHz
   */
  double *frequencies;
} CpuFreqData;

int CpuFreq_init(CpuFreqData *data);
void CpuFreq_update(CpuFreqData *data);
void CpuFreq_cleanup(CpuFreqData *data);
#endif
#endif
