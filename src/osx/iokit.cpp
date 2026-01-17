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

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "iokit.hpp"

//? IOReport non public objects
namespace IOReport {
    void *lib_handle = nullptr;
    bool try_loaded = false;

    CFDictionaryRef (*CopyChannelsInGroup)(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t) = nullptr;

    void (*MergeChannels)(CFDictionaryRef, CFDictionaryRef, CFTypeRef) = nullptr;

    IOReportSubscriptionRef (*CreateSubscription)(void *, CFMutableDictionaryRef,CFMutableDictionaryRef *, uint64_t, CFTypeRef) = nullptr;

    CFDictionaryRef (*CreateSamples)(IOReportSubscriptionRef, CFMutableDictionaryRef, CFTypeRef) = nullptr;

    CFDictionaryRef (*CreateSamplesDelta)(CFDictionaryRef, CFDictionaryRef, CFTypeRef) = nullptr;

    CFStringRef (*ChannelGetGroup)(CFDictionaryRef) = nullptr;
    CFStringRef (*ChannelGetSubGroup)(CFDictionaryRef) = nullptr;
    CFStringRef (*ChannelGetChannelName)(CFDictionaryRef) = nullptr;
    CFStringRef (*ChannelGetUnitLabel)(CFDictionaryRef) = nullptr;
    CFStringRef (*ChannelGetDriverName)(CFDictionaryRef) = nullptr;

    int32_t (*StateGetCount)(CFDictionaryRef) = nullptr;
    CFStringRef (*StateGetNameForIndex)(CFDictionaryRef, int32_t) = nullptr;
    int64_t (*StateGetResidency)(CFDictionaryRef, int32_t) = nullptr;
    int64_t (*SimpleGetIntegerValue)(CFDictionaryRef, int32_t) = nullptr;

    void try_load() {
        if (not try_loaded){ 
            try_loaded = true;
        }

        //? This file does not actually exist if searched for in the Mac OS path,
        // however its functions are in the Mac OS libs cache.
        lib_handle = dlopen("/usr/lib/libIOReport.dylib", RTLD_NOW);
        if (lib_handle == nullptr) {
            return;
        }

    //? Load all required functions
    #define LOAD_FUNC(sym)                               \
        sym = reinterpret_cast<decltype(IOReport::sym)>( \
            dlsym(lib_handle, "IOReport" #sym));         \
        if (not sym) {                                   \
            dlclose(lib_handle);                         \
            lib_handle = nullptr;                        \
            return;                                      \
        }

        LOAD_FUNC(CopyChannelsInGroup)
        LOAD_FUNC(MergeChannels)
        LOAD_FUNC(CreateSubscription)
        LOAD_FUNC(CreateSamples)
        LOAD_FUNC(CreateSamplesDelta)
        LOAD_FUNC(ChannelGetGroup)
        LOAD_FUNC(ChannelGetSubGroup)
        LOAD_FUNC(ChannelGetChannelName)
        LOAD_FUNC(ChannelGetUnitLabel)
        LOAD_FUNC(StateGetCount)
        LOAD_FUNC(StateGetNameForIndex)
        LOAD_FUNC(StateGetResidency)
        LOAD_FUNC(SimpleGetIntegerValue)
        LOAD_FUNC(ChannelGetDriverName)

    #undef LOAD_FUNC
    }
}  // namespace IOReport

//? Tools for converting CF to std
//? Below, the items process CF objects for their std equivalents. Here, the
// type is not verified, so make sure it is of the type called

std::optional<std::string> safe_cfstring_to_std_string(CFStringRef string_ref) {
    if (not string_ref) {
        return std::nullopt;
    }

    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string_ref),kCFStringEncodingUTF8) + 1;

    std::string result(maxSize, '\0');

    if (not CFStringGetCString(string_ref, result.data(), maxSize, kCFStringEncodingUTF8)){
        return std::nullopt;
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::optional<int64_t> safe_cfnumber_to_int64(CFNumberRef number_ref) {
    if (not number_ref) { 
        return std::nullopt;
    }

    int64_t value = 0;
    if (not CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value)){
        return std::nullopt;
    }

    return value;
}

std::optional<std::vector<uint8_t>> safe_cfdata_to_raw_vector(CFDataRef data_ref) {
    if (not data_ref) { 
        return std::nullopt;
    }

    CFIndex length = CFDataGetLength(data_ref);
    if (length <= 0) { 
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(length));

    CFDataGetBytes(data_ref, CFRangeMake(0, length), buffer.data());

    return buffer;
}

std::optional<bool> safe_cfbool_to_bool(CFBooleanRef bool_ref) {
    if (not bool_ref) { 
        return std::nullopt;
    }

    return CFBooleanGetValue(bool_ref);
}

/*
Below are functions that help with dictionary searches. They return compatible
types in std if everything goes well. If not, they return nullopt if:

The dictionary is invalid.
The entry does not exist.
It is not a compatible type.

It can be used as a quick test of the dictionary type.
*/

std::optional<std::string> safe_cfdictionary_to_std_string(CFDictionaryRef dictionary, CFStringRef key) {
    if (not dictionary or not key) {
        return std::nullopt;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (not value or CFGetTypeID(value) != CFStringGetTypeID()) {
        return std::nullopt;
    }
        
    return safe_cfstring_to_std_string(static_cast<CFStringRef>(value));
}

std::optional<int64_t> safe_cfdictionary_to_int64(CFDictionaryRef dictionary, CFStringRef key) {
    if (not dictionary or not key) {
        return std::nullopt;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (not value or CFGetTypeID(value) != CFNumberGetTypeID()) {
        return std::nullopt;
    }
        
    return safe_cfnumber_to_int64(static_cast<CFNumberRef>(value));
}

std::optional<std::vector<uint8_t>> safe_cfdictionary_to_raw_vector(CFDictionaryRef dictionary, CFStringRef key) {
    if (not dictionary or not key) {
        return std::nullopt;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (not value or CFGetTypeID(value) != CFDataGetTypeID()){
        return std::nullopt;
    }

    return safe_cfdata_to_raw_vector(static_cast<CFDataRef>(value));
}

std::optional<bool> safe_cfdictionary_to_bool(CFDictionaryRef dictionary, CFStringRef key) {
    if (not dictionary or not key) {
        return std::nullopt;
    }

    CFTypeRef value = CFDictionaryGetValue(dictionary, key);
    if (not value or CFGetTypeID(value) != CFBooleanGetTypeID()) {
        return std::nullopt;
    }

    return safe_cfbool_to_bool(static_cast<CFBooleanRef>(value));
}

/*
Helps obtain the parent item of the object in the IOKit tree.
Returns the entry of the object that now belongs to the caller's management.
*/

io_registry_entry_t io_service_get_parent(io_registry_entry_t entry, const io_name_t plane) {
    io_iterator_t iterator = IO_OBJECT_NULL;

    if (IORegistryEntryGetParentIterator(entry, plane, &iterator) != KERN_SUCCESS) {
        return IO_OBJECT_NULL;
    }
        
    io_registry_entry_t parent = IOIteratorNext(iterator);
    IOObjectRelease(iterator);

    return parent;  // caller must IOObjectRelease
}

/*
Given a class, we look at all the items that belong to it. For example,
IOAccelerator returns all GPUs. It works by giving a callback with any pointer
that enters the function. When returning true, it iterates to the next one; when
returning false, it iterates to the previous one.
*/

bool io_service_class_interator(const std::string &className, IOServiceCallback callback, void *data) {
    CFDictionaryRef matching = IOServiceMatching(className.c_str());
    if (not matching) {
        return false;
    }

    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS){
         return false;
    }
       

    io_object_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        bool result = false;
        if (callback) {
            result = callback(service, data);
        }

        IOObjectRelease(service);
        if (not result) { 
            break;
        }
    }

    IOObjectRelease(iterator);
    return true;
}

/*
Helps visualize the objects below the item in the IOkit tree.
Requires a callback and a pointer that will be passed to each item found.
When returning true, it iterates to the next one; when returning false, it
iterates to the previous one.
*/

bool io_service_children_interator(io_object_t parent, const io_name_t plane, IOServiceCallback callback, void *data) {
    io_iterator_t iterator = IO_OBJECT_NULL;

    if (IORegistryEntryGetChildIterator(parent, plane, &iterator) != KERN_SUCCESS){
        return false;
    }
        
    io_object_t child;
    while ((child = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        bool result = false;
        if (callback) { 
            result = callback(child, data);
        }

        IOObjectRelease(child);
        if (not result) {
            break;
        }
    }

    IOObjectRelease(iterator);
    return true;
}

extern "C" {
//? create a dict ref, like for temperature sensor {"PrimaryUsagePage":0xff00,
//"PrimaryUsage":0x5}
CFDictionaryRef create_hid_matching(int page, int usage) {
    CFNumberRef nums[2];
    CFStringRef keys[2];

    keys[0] = CFStringCreateWithCString(0, "PrimaryUsagePage", 0);
    keys[1] = CFStringCreateWithCString(0, "PrimaryUsage", 0);
    nums[0] = CFNumberCreate(0, kCFNumberSInt32Type, &page);
    nums[1] = CFNumberCreate(0, kCFNumberSInt32Type, &usage);

    CFDictionaryRef dict = CFDictionaryCreate(
        0, (const void **)keys, (const void **)nums, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(keys[0]);
    CFRelease(keys[1]);
    CFRelease(nums[0]);
    CFRelease(nums[1]);
    return dict;
}
}