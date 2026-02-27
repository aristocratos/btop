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

/* IOReport CPU Frequency Monitoring for Apple Silicon

Provides sudoless CPU frequency monitoring via Apple's IOReport framework.
Library: /usr/lib/libIOReport.dylib

We use the ECPM/PCPM channels from "CPU Stats" -> "CPU Complex Performance States"
to get cluster-level frequency data. Frequency tables are read from the pmgr device
(voltage-states1-sram for E-CPU, voltage-states5-sram for P-CPU).

State names follow "VxPy" pattern where x is voltage level. We parse x and map to
frequency table index. The mapping is dynamic to handle variations across chip models.

Frequency calculation: weighted average of (residency × frequency) for each state.

*/

#include "ioreport.hpp"

#include <IOKit/IOKitLib.h>
#include <dlfcn.h>
#include <cstring>
#include <mutex>
#include <vector>
#include <string>

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

// IOReport types
struct IOReportSubscription;
typedef struct IOReportSubscription* IOReportSubscriptionRef;
typedef CFDictionaryRef IOReportSampleRef;

// IOReport function pointers (loaded dynamically from libIOReport.dylib)
static CFMutableDictionaryRef (*IOReportCopyChannelsInGroup)(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t) = nullptr;
static IOReportSubscriptionRef (*IOReportCreateSubscription)(void*, CFMutableDictionaryRef, CFMutableDictionaryRef*, uint64_t, CFTypeRef) = nullptr;
static CFDictionaryRef (*IOReportCreateSamples)(IOReportSubscriptionRef, CFMutableDictionaryRef, CFTypeRef) = nullptr;
static CFDictionaryRef (*IOReportCreateSamplesDelta)(CFDictionaryRef, CFDictionaryRef, CFTypeRef) = nullptr;
static CFStringRef (*IOReportChannelGetChannelName)(IOReportSampleRef) = nullptr;
static int32_t (*IOReportStateGetCount)(IOReportSampleRef) = nullptr;
static int64_t (*IOReportStateGetResidency)(IOReportSampleRef, int32_t) = nullptr;
static CFStringRef (*IOReportStateGetNameForIndex)(IOReportSampleRef, int32_t) = nullptr;
static int (*IOReportChannelGetFormat)(IOReportSampleRef) = nullptr;

// IOReport format types
enum {
	kIOReportFormatState = 2
};

namespace {
	// Mutex protecting all module state (thread safety for concurrent access)
	std::mutex g_mutex;

	// Module state
	bool g_initialized = false;
	bool g_available = false;
	void* g_iokit_handle = nullptr;
	IOReportSubscriptionRef g_subscription = nullptr;
	CFMutableDictionaryRef g_channels = nullptr;
	// g_sub_channels is an out-parameter from IOReportCreateSubscription.
	// Based on observed behavior (similar to powermetrics), caller owns this reference.
	CFMutableDictionaryRef g_sub_channels = nullptr;

	// Frequency tables from pmgr
	std::vector<uint32_t> g_ecpu_freqs;
	std::vector<uint32_t> g_pcpu_freqs;

	// Last sample for delta calculation
	CFDictionaryRef g_last_sample = nullptr;

	// Helper to convert CFString to std::string
	std::string cfstring_to_string(CFStringRef str) {
		if (str == nullptr) return "";
		const char* cstr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
		if (cstr != nullptr) return std::string(cstr);

		CFIndex length = CFStringGetLength(str);
		CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
		std::vector<char> buffer(max_size);
		if (CFStringGetCString(str, buffer.data(), max_size, kCFStringEncodingUTF8)) {
			return std::string(buffer.data());
		}
		return "";
	}

	// Parse frequency data from voltage-states property
	void parse_freq_data(CFDataRef data, std::vector<uint32_t>& freqs, uint32_t scale) {
		if (data == nullptr) return;
		CFIndex len = CFDataGetLength(data);
		const uint8_t* bytes = CFDataGetBytePtr(data);
		int total_entries = static_cast<int>(len / 8);

		freqs.clear();
		freqs.reserve(total_entries);
		for (int i = 0; i < total_entries; i++) {
			uint32_t freq = 0;
			memcpy(&freq, bytes + (i * 8), 4);
			uint32_t freq_mhz = freq / scale;
			// Keep zero entries to preserve voltage state index mapping.
			freqs.push_back(freq_mhz);
		}
	}

	// RAII wrapper for IOObjectRelease
	struct ScopedIOObject {
		io_object_t obj;
		ScopedIOObject(io_object_t o) : obj(o) {}
		~ScopedIOObject() { if (obj) IOObjectRelease(obj); }
		ScopedIOObject(const ScopedIOObject&) = delete;
		ScopedIOObject& operator=(const ScopedIOObject&) = delete;
	};

	// RAII wrapper for CFRelease
	struct ScopedCFType {
		CFTypeRef obj;
		ScopedCFType(CFTypeRef o) : obj(o) {}
		~ScopedCFType() { if (obj) CFRelease(obj); }
		ScopedCFType(const ScopedCFType&) = delete;
		ScopedCFType& operator=(const ScopedCFType&) = delete;
		void release() { obj = nullptr; } // Stop managing the object (if ownership transferred)
	};

	// Load CPU frequency tables from pmgr device
	bool load_cpu_frequencies() {
		io_iterator_t iterator = 0;
		io_object_t entry = 0;

		CFMutableDictionaryRef matching = IOServiceMatching("AppleARMIODevice");
		if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != kIOReturnSuccess) {
			return false;
		}
		ScopedIOObject scoped_iter(iterator);

		bool found = false;
		constexpr uint32_t M4_FREQ_THRESHOLD_HZ = 10'000'000;

		while ((entry = IOIteratorNext(iterator)) != 0) {
			ScopedIOObject scoped_entry(entry);
			io_name_t name = {};
			if (IORegistryEntryGetName(entry, name) != kIOReturnSuccess) {
				continue;
			}

			if (strcmp(name, "pmgr") == 0) {
				CFMutableDictionaryRef properties = nullptr;
				if (IORegistryEntryCreateCFProperties(entry, &properties, kCFAllocatorDefault, 0) == kIOReturnSuccess) {
					ScopedCFType scoped_props(properties);
					// Determine scale based on chip (M4+ uses KHz, earlier uses MHz)
					// We'll try to detect by checking frequency magnitudes
					uint32_t scale = 1000000; // Default to MHz (pre-M4)

					CFDataRef e_data = (CFDataRef)CFDictionaryGetValue(properties, CFSTR("voltage-states1-sram"));
					CFDataRef p_data = (CFDataRef)CFDictionaryGetValue(properties, CFSTR("voltage-states5-sram"));

					// Check if this is M4+ by examining raw frequency values
					if (e_data != nullptr) {
						CFIndex len = CFDataGetLength(e_data);
						if (len >= 8) {
							const uint8_t* bytes = CFDataGetBytePtr(e_data);
							uint32_t first_freq = 0;
							memcpy(&first_freq, bytes, 4);
							// M4+ uses KHz (values like 912000 for 912 MHz)
							// M1-M3 uses Hz (values like 600000000 for 600 MHz)
							if (first_freq < M4_FREQ_THRESHOLD_HZ) {
								scale = 1000; // M4+ uses KHz
							}
						}
					}

					if (e_data != nullptr) {
						parse_freq_data(e_data, g_ecpu_freqs, scale);
					}
					if (p_data != nullptr) {
						parse_freq_data(p_data, g_pcpu_freqs, scale);
					}

					found = not g_ecpu_freqs.empty() and not g_pcpu_freqs.empty();
				}
			}
			if (found) break;
		}
		return found;
	}

	bool load_ioreport_functions() {
		g_iokit_handle = dlopen("/usr/lib/libIOReport.dylib", RTLD_NOW);
		if (g_iokit_handle == nullptr) return false;

		#define LOAD_SYM(name) name = (decltype(name))dlsym(g_iokit_handle, #name)
		LOAD_SYM(IOReportCopyChannelsInGroup);
		LOAD_SYM(IOReportCreateSubscription);
		LOAD_SYM(IOReportCreateSamples);
		LOAD_SYM(IOReportCreateSamplesDelta);
		LOAD_SYM(IOReportChannelGetChannelName);
		LOAD_SYM(IOReportStateGetCount);
		LOAD_SYM(IOReportStateGetResidency);
		LOAD_SYM(IOReportStateGetNameForIndex);
		LOAD_SYM(IOReportChannelGetFormat);
		#undef LOAD_SYM

		return IOReportCopyChannelsInGroup and IOReportCreateSubscription and
		       IOReportCreateSamples and IOReportCreateSamplesDelta and
		       IOReportChannelGetChannelName and IOReportChannelGetFormat and
		       IOReportStateGetCount and IOReportStateGetResidency and
		       IOReportStateGetNameForIndex;
	}

	// Initialize IOReport subscription
	bool init_subscription() {
		CFMutableDictionaryRef cpu_chan = IOReportCopyChannelsInGroup(CFSTR("CPU Stats"), nullptr, 0, 0, 0);
		if (cpu_chan == nullptr) return false;

		g_channels = cpu_chan;
		g_subscription = IOReportCreateSubscription(nullptr, g_channels, &g_sub_channels, 0, nullptr);

		return g_subscription != nullptr;
	}
}

namespace IOReport {

	// Internal cleanup (caller must hold g_mutex)
	static void cleanup_internal() {
		// Release CF objects first, before closing the library handle
		if (g_last_sample != nullptr) {
			CFRelease(g_last_sample);
			g_last_sample = nullptr;
		}
		if (g_channels != nullptr) {
			CFRelease(g_channels);
			g_channels = nullptr;
		}
		if (g_subscription != nullptr) {
			CFRelease(g_subscription);
			g_subscription = nullptr;
		}
		if (g_sub_channels != nullptr) {
			CFRelease(g_sub_channels);
			g_sub_channels = nullptr;
		}
		// Close library handle last, after all CF objects are released
		if (g_iokit_handle != nullptr) {
			dlclose(g_iokit_handle);
			g_iokit_handle = nullptr;
		}
		g_initialized = false;
		g_available = false;
	}

	bool init() {
		std::lock_guard<std::mutex> lock(g_mutex);
		if (g_initialized) return g_available;
		g_initialized = true;

		// Load IOReport functions
		if (not load_ioreport_functions()) {
			cleanup_internal();
			return false;
		}

		// Load frequency tables
		if (not load_cpu_frequencies()) {
			cleanup_internal();
			return false;
		}

		// Initialize subscription
		if (not init_subscription()) {
			cleanup_internal();
			return false;
		}

		g_available = true;
		return true;
	}

	void cleanup() {
		std::lock_guard<std::mutex> lock(g_mutex);
		cleanup_internal();
	}

	bool is_available() {
		std::lock_guard<std::mutex> lock(g_mutex);
		return g_available;
	}

	//? Process a single IOReport channel sample and extract frequency if it's ECPM/PCPM
	//? Must be called with g_mutex held
	void process_channel_sample(IOReportSampleRef sample, uint32_t& e_freq, uint32_t& p_freq) {
		if (IOReportChannelGetFormat and IOReportChannelGetFormat(sample) != kIOReportFormatState)
			return;

		std::string channel = cfstring_to_string(IOReportChannelGetChannelName(sample));
		bool is_ecpm = (channel == "ECPM"), is_pcpm = (channel == "PCPM");
		if (not is_ecpm and not is_pcpm) return;

		const auto& freq_table = is_ecpm ? g_ecpu_freqs : g_pcpu_freqs;
		if (freq_table.empty()) return;

		int64_t total_residency = 0;
		double weighted_freq = 0;

		// Calculate weighted average frequency (matches powermetrics "HW active frequency")
		for (int32_t s = 0; s < IOReportStateGetCount(sample); s++) {
			int64_t residency = IOReportStateGetResidency(sample, s);
			if (residency <= 0) continue;

			std::string name = cfstring_to_string(IOReportStateGetNameForIndex(sample, s));
			if (name.empty() or name[0] != 'V') continue;
			size_t p_pos = name.find('P');
			if (p_pos == std::string::npos or p_pos <= 1) continue;

			try {
				size_t freq_idx = static_cast<size_t>(std::stoi(name.substr(1, p_pos - 1)));
				if (freq_idx < freq_table.size()) {
					total_residency += residency;
					weighted_freq += residency * freq_table[freq_idx];
				}
			} catch (const std::invalid_argument&) {
				// Ignore: state name couldn't be parsed as integer
			} catch (const std::out_of_range&) {
				// Ignore: parsed value exceeded int range
			}
		}

		if (total_residency > 0) {
			(is_ecpm ? e_freq : p_freq) = static_cast<uint32_t>(weighted_freq / total_residency);
		}
	}

	std::pair<uint32_t, uint32_t> get_cpu_frequencies() {
		std::lock_guard<std::mutex> lock(g_mutex);
		if (not g_available) return {0, 0};

		CFDictionaryRef current_sample = IOReportCreateSamples(g_subscription, g_sub_channels, nullptr);
		if (current_sample == nullptr) return {0, 0};

		// RAII for current_sample
		ScopedCFType scoped_sample(current_sample);

		uint32_t e_freq = 0, p_freq = 0;

		if (g_last_sample != nullptr) {
			CFDictionaryRef delta = IOReportCreateSamplesDelta(g_last_sample, current_sample, nullptr);
			if (delta != nullptr) {
				ScopedCFType scoped_delta(delta);

				// Manually iterate over IOReport channels (avoids Objective-C blocks for GCC compat)
				// Validate type before casting to guard against IOReport API changes
				CFTypeRef channels_ref = CFDictionaryGetValue(delta, CFSTR("IOReportChannels"));
				if (channels_ref != nullptr and CFGetTypeID(channels_ref) == CFArrayGetTypeID()) {
					CFArrayRef channels = (CFArrayRef)channels_ref;
					CFIndex count = CFArrayGetCount(channels);
					for (CFIndex i = 0; i < count; i++) {
						IOReportSampleRef sample = (IOReportSampleRef)CFArrayGetValueAtIndex(channels, i);
						if (sample != nullptr) {
							process_channel_sample(sample, e_freq, p_freq);
						}
					}
				}
			}

			// Release last sample as we will replace it with current
			CFRelease(g_last_sample);
			g_last_sample = nullptr;
		}

		// Keep current_sample for next iteration
		scoped_sample.release(); // Don't release, we store it
		g_last_sample = current_sample;

		return {e_freq, p_freq};
	}

}
