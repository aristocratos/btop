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
#include "iokit.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <string>
#include <numeric>
#include <vector>

extern "C" {
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
	CFDictionaryRef thermalSensors = create_hid_matching(0xff00, 5);  // 65280_10 = FF00_16
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
					std::string n = safe_cfstring_to_std_string(name).value_or("");
					
					if (n.starts_with("eACC") or n.starts_with("pACC")) {
						temps.push_back(getValue(sc));
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
