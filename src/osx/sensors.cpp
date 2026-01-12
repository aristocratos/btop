/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

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
#include "sensors.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <string>
#include <numeric>
#include <vector>

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

IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client, CFDictionaryRef match);
int IOHIDEventSystemClientSetMatchingMultiple(IOHIDEventSystemClientRef client, CFArrayRef match);
IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef, int64_t, int32_t, int64_t);
CFStringRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef property);
IOHIDFloat IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);

// create a dict ref, like for temperature sensor {"PrimaryUsagePage":0xff00, "PrimaryUsage":0x5}
CFDictionaryRef matching(int page, int usage) {
	CFNumberRef nums[2];
	CFStringRef keys[2];

	keys[0] = CFStringCreateWithCString(0, "PrimaryUsagePage", 0);
	keys[1] = CFStringCreateWithCString(0, "PrimaryUsage", 0);
	nums[0] = CFNumberCreate(0, kCFNumberSInt32Type, &page);
	nums[1] = CFNumberCreate(0, kCFNumberSInt32Type, &usage);

	CFDictionaryRef dict = CFDictionaryCreate(0, (const void **)keys, (const void **)nums, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(keys[0]);
	CFRelease(keys[1]);
	CFRelease(nums[0]);
	CFRelease(nums[1]);
	return dict;
}

double getValue(IOHIDServiceClientRef sc) {
	IOHIDEventRef event = IOHIDServiceClientCopyEvent(sc, kIOHIDEventTypeTemperature, 0, 0);  // here we use ...CopyEvent
	IOHIDFloat temp = 0.0;
	if (event != 0) {
		temp = IOHIDEventGetFloatValue(event, IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
		CFRelease(event);
	}
	return temp;
}

}  // extern C

long long Cpu::ThermalSensors::getSensors() {
	CFDictionaryRef thermalSensors = matching(0xff00, 5);  // 65280_10 = FF00_16
														   // thermalSensors's PrimaryUsagePage should be 0xff00 for M1 chip, instead of 0xff05
														   // can be checked by ioreg -lfx
	IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
	IOHIDEventSystemClientSetMatching(system, thermalSensors);
	CFArrayRef matchingsrvs = IOHIDEventSystemClientCopyServices(system);
	std::vector<double> temps;
	if (matchingsrvs) {
		long count = CFArrayGetCount(matchingsrvs);
		for (int i = 0; i < count; i++) {
			IOHIDServiceClientRef sc = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(matchingsrvs, i);
			if (sc) {
				CFStringRef name = IOHIDServiceClientCopyProperty(sc, CFSTR("Product"));  // here we use ...CopyProperty
				if (name) {
					char buf[200];
					CFStringGetCString(name, buf, 200, kCFStringEncodingASCII);
					std::string n(buf);
					// CPU temperature sensors vary by Apple Silicon generation
					// M1/M2 typically have eACC/pACC, M3/M4 use different naming
					// Broader patterns to support all Apple Silicon chips
					bool is_cpu_sensor = (
						n.starts_with("eACC") or      // Efficiency cores (M1/M2)
						n.starts_with("pACC") or      // Performance cores (M1/M2)
						n.starts_with("PMU TP") or    // PMU temperature sensors
						n.starts_with("Tp") or        // Direct CPU temp sensors
						n.find("CPU") != std::string::npos or  // Any CPU-related sensor
						n.starts_with("SOC MTR") or   // SoC thermal sensors
						n.starts_with("PMU tdie")     // Die temperature sensors
					);
					if (is_cpu_sensor) {
						double temp = getValue(sc);
						// Filter out unreasonable values (some sensors report negative or very high temps)
						if (temp > 0.0 and temp < 150.0) {
							temps.push_back(temp);
						}
					}
					CFRelease(name);
				}
			}
		}
		CFRelease(matchingsrvs);
	}
	CFRelease(system);
	CFRelease(thermalSensors);
	if (temps.empty()) return 0ll;
	return round(std::accumulate(temps.begin(), temps.end(), 0ll) / temps.size());
}
#endif
