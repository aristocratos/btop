#include "sensors.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

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

CFArrayRef getProductNames(CFDictionaryRef sensors) {
	IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);  // in CFBase.h = NULL
	// ... this is the same as using kCFAllocatorDefault or the return value from CFAllocatorGetDefault()
	IOHIDEventSystemClientSetMatching(system, sensors);
	CFArrayRef matchingsrvs = IOHIDEventSystemClientCopyServices(system);  // matchingsrvs = matching services

	long count = CFArrayGetCount(matchingsrvs);
	CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for (int i = 0; i < count; i++) {
		IOHIDServiceClientRef sc = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(matchingsrvs, i);
		CFStringRef name = IOHIDServiceClientCopyProperty(sc, CFSTR("Product"));  // here we use ...CopyProperty
		if (name) {
			CFArrayAppendValue(array, name);
		} else {
			CFArrayAppendValue(array, CFSTR("noname"));  // @ gives a Ref like in "CFStringRef name"
		}
		CFRelease(name);
	}
	return array;
}

CFArrayRef getThermalValues(CFDictionaryRef sensors) {
	IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
	IOHIDEventSystemClientSetMatching(system, sensors);
	CFArrayRef matchingsrvs = IOHIDEventSystemClientCopyServices(system);

	long count = CFArrayGetCount(matchingsrvs);
	CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

	for (int i = 0; i < count; i++) {
		IOHIDServiceClientRef sc = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(matchingsrvs, i);
		IOHIDEventRef event = IOHIDServiceClientCopyEvent(sc, kIOHIDEventTypeTemperature, 0, 0);  // here we use ...CopyEvent

		CFNumberRef value;
		if (event != 0) {
			double temp = IOHIDEventGetFloatValue(event, IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
			value = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &temp);
		} else {
			double temp = 0;
			value = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &temp);
		}
		CFArrayAppendValue(array, value);
		CFRelease(value);
	}
	CFRelease(system);
	return array;
}
}
unordered_flat_map<int, double> Cpu::ThermalSensors::getSensors() {
	unordered_flat_map<int, double> cpuValues;
	CFDictionaryRef thermalSensors = matching(0xff00, 5);  // 65280_10 = FF00_16
	// thermalSensors's PrimaryUsagePage should be 0xff00 for M1 chip, instead of 0xff05
	// can be checked by ioreg -lfx
	CFArrayRef thermalNames = getProductNames(thermalSensors);
	CFArrayRef thermalValues = getThermalValues(thermalSensors);
	long count = CFArrayGetCount(thermalNames);
	for (int i = 0; i < count; i++) {
		CFStringRef nameRef = (CFStringRef)CFArrayGetValueAtIndex(thermalNames, i);
		char buf[200];
		CFStringGetCString(nameRef, buf, 200, kCFStringEncodingASCII);
		std::string n(buf);
        CFNumberRef value = (CFNumberRef)CFArrayGetValueAtIndex(thermalValues, i);
        double temp = 0.0;
        CFNumberGetValue(value, kCFNumberDoubleType, &temp);
		if (n.starts_with("PMU tdie")) {
            // Apple Silicon
			std::string indexString = n.substr(8, 1);
			int index = stoi(indexString);
			cpuValues[index - 1] = temp;
		} else if (n.starts_with("TC") && n[3] == 'c') {
            // intel mac
			std::string indexString = n.substr(2, 1);
			int index = stoi(indexString);
            cpuValues[index] = temp;
        } else if (n == "TCAD") {
            cpuValues[0] = temp; // package T for intel
        } else if (n == "SOC MTR Temp Sensor0") {
            cpuValues[0] = temp; // package T for Apple Silicon
        }
	}
	CFRelease(thermalSensors);
	CFRelease(thermalNames);
	CFRelease(thermalValues);

	return cpuValues;
}
