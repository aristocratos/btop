/* Copyright 2026 Ruaneri Portela (ruaneriportela@outlook.com)

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
#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

using IOServiceCallback = bool (*)(io_object_t object, void *data);

std::optional<std::string> safe_cfstring_to_std_string(CFStringRef strRef);

std::optional<int64_t> safe_cfnumber_to_int64(CFNumberRef numberRef);

std::optional<std::vector<uint8_t>> safe_cfdata_to_raw_vector(
    CFDataRef dataRef);

std::optional<bool> safe_cfbool_to_bool(CFBooleanRef boolRef);

std::optional<std::string> safe_cfdictionary_to_std_string(
    CFDictionaryRef dictionary, CFStringRef key);

std::optional<int64_t> safe_cfdictionary_to_int64(CFDictionaryRef dictionary,
                                                  CFStringRef key);

std::optional<std::vector<uint8_t>> safe_cfdictionary_to_raw_vector(
    CFDictionaryRef dictionary, CFStringRef key);

std::optional<bool> safe_cfdictionary_to_bool(CFDictionaryRef dictionary,
                                              CFStringRef key);

io_registry_entry_t io_service_get_parent(io_registry_entry_t entry,
                                          const io_name_t plane);

bool io_service_class_interator(const std::string &className,
                                IOServiceCallback callback, void *data);

bool io_service_children_interator(io_object_t parent, const io_name_t plane,
                                   IOServiceCallback callback, void *data);

namespace IOReport {
using IOReportSubscriptionRef = CFTypeRef;
        extern void *lib_handle;
        extern bool try_loaded;

        extern CFDictionaryRef (*CopyChannelsInGroup)(CFStringRef, CFStringRef,
                                                uint64_t, uint64_t, uint64_t);

        extern void (*MergeChannels)(CFDictionaryRef, CFDictionaryRef, CFTypeRef);

        extern IOReportSubscriptionRef (*CreateSubscription)(void *,
                                                        CFMutableDictionaryRef,
                                                        CFMutableDictionaryRef *,
                                                        uint64_t, CFTypeRef);

        extern CFDictionaryRef (*CreateSamples)(IOReportSubscriptionRef,
                                                CFMutableDictionaryRef, CFTypeRef);

        extern CFDictionaryRef (*CreateSamplesDelta)(CFDictionaryRef, CFDictionaryRef,
                                                CFTypeRef);

        extern CFStringRef (*ChannelGetGroup)(CFDictionaryRef);
        extern CFStringRef (*ChannelGetSubGroup)(CFDictionaryRef);
        extern CFStringRef (*ChannelGetChannelName)(CFDictionaryRef);
        extern CFStringRef (*ChannelGetUnitLabel)(CFDictionaryRef);
        extern CFStringRef (*ChannelGetDriverName)(CFDictionaryRef);

        extern int32_t (*StateGetCount)(CFDictionaryRef);
        extern CFStringRef (*StateGetNameForIndex)(CFDictionaryRef, int32_t);
        extern int64_t (*StateGetResidency)(CFDictionaryRef, int32_t);
        extern int64_t (*SimpleGetIntegerValue)(CFDictionaryRef, int32_t);

        void try_load();
}  // namespace IOReport

// IOHIDSensor declarations for GPU temperature
extern "C" {
typedef struct __IOHIDEvent *IOHIDEventRef;
typedef struct __IOHIDServiceClient *IOHIDServiceClientRef;
#ifdef __LP64__
typedef double IOHIDFloat;
#else
typedef float IOHIDFloat;
#endif

#define IOHIDEventFieldBase(type) (type << 16)
#define kIOHIDEventTypeTemperature 15

IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(
    CFAllocatorRef allocator);
int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client,
                                      CFDictionaryRef match);
CFArrayRef IOHIDEventSystemClientCopyServices(IOHIDEventSystemClientRef);
IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef, int64_t,
                                          int32_t, int64_t);
CFStringRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service,
                                           CFStringRef property);
IOHIDFloat IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);

CFDictionaryRef create_hid_matching(int page, int usage);
}