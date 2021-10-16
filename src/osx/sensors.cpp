#include "sensors.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <btop_tools.hpp>
#include <iostream>
#include <map>
#include <string>

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
	CFRelease(keys);
	return dict;
}

double getValue(IOHIDServiceClientRef sc) {
	IOHIDEventRef event = IOHIDServiceClientCopyEvent(sc, kIOHIDEventTypeTemperature, 0, 0);  // here we use ...CopyEvent
	double temp = 0.0;
	if (event != 0) {
		temp = IOHIDEventGetFloatValue(event, IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
		CFRelease(event);
	}
	return temp;
}

} // extern C

unordered_flat_map<int, double> Cpu::ThermalSensors::getSensors() {
	unordered_flat_map<int, double> cpuValues;
	CFDictionaryRef thermalSensors = matching(0xff00, 5);  // 65280_10 = FF00_16
	                                                       // thermalSensors's PrimaryUsagePage should be 0xff00 for M1 chip, instead of 0xff05
	                                                       // can be checked by ioreg -lfx
	IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
	IOHIDEventSystemClientSetMatching(system, thermalSensors);
	CFArrayRef matchingsrvs = IOHIDEventSystemClientCopyServices(system);

	long count = CFArrayGetCount(matchingsrvs);
	for (int i = 0; i < count; i++) {
		IOHIDServiceClientRef sc = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(matchingsrvs, i);
		if (sc) {
			CFStringRef name = IOHIDServiceClientCopyProperty(sc, CFSTR("Product"));  // here we use ...CopyProperty
			if (name) {
				char buf[200];
				CFStringGetCString(name, buf, 200, kCFStringEncodingASCII);
				std::string n(buf);
				if (n.starts_with("PMU tdie")) {
					std::string indexString = n.substr(8, 1);
					int index = stoi(indexString);
					cpuValues[index - 1] = getValue(sc);
				} else if (n == "SOC MTR Temp Sensor0") {
					cpuValues[0] = getValue(sc);  // package T for Apple Silicon
				}
				CFRelease(name);
			}
		}
	}
    CFRelease(matchingsrvs);
    CFRelease(system);
	CFRelease(thermalSensors);
	return cpuValues;
}
