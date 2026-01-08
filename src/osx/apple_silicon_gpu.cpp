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

		//? Set GPU name based on chip
		if (brand.find("M1") != std::string::npos) {
			if (brand.find("Max") != std::string::npos) gpu_name = "Apple M1 Max GPU";
			else if (brand.find("Pro") != std::string::npos) gpu_name = "Apple M1 Pro GPU";
			else if (brand.find("Ultra") != std::string::npos) gpu_name = "Apple M1 Ultra GPU";
			else gpu_name = "Apple M1 GPU";
		} else if (brand.find("M2") != std::string::npos) {
			if (brand.find("Max") != std::string::npos) gpu_name = "Apple M2 Max GPU";
			else if (brand.find("Pro") != std::string::npos) gpu_name = "Apple M2 Pro GPU";
			else if (brand.find("Ultra") != std::string::npos) gpu_name = "Apple M2 Ultra GPU";
			else gpu_name = "Apple M2 GPU";
		} else if (brand.find("M3") != std::string::npos) {
			if (brand.find("Max") != std::string::npos) gpu_name = "Apple M3 Max GPU";
			else if (brand.find("Pro") != std::string::npos) gpu_name = "Apple M3 Pro GPU";
			else gpu_name = "Apple M3 GPU";
		} else if (brand.find("M4") != std::string::npos) {
			if (brand.find("Max") != std::string::npos) gpu_name = "Apple M4 Max GPU";
			else if (brand.find("Pro") != std::string::npos) gpu_name = "Apple M4 Pro GPU";
			else gpu_name = "Apple M4 GPU";
		} else {
			gpu_name = "Apple Silicon GPU";
		}

		Logger::debug("AppleSiliconGpu: Detected {}", gpu_name);

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

		//? Get Energy Model channels (for power consumption)
		CFStringRef energy_group = CFStringCreateWithCString(kCFAllocatorDefault, "Energy Model", kCFStringEncodingUTF8);
		CFDictionaryRef energy_channels = IOReportCopyChannelsInGroup(energy_group, nullptr, 0, 0, 0);
		CFRelease(energy_group);

		if (energy_channels == nullptr) {
			Logger::debug("AppleSiliconGpu: Failed to get Energy Model channels");
			CFRelease(gpu_stats_channels);
			return false;
		}

		//? Merge channels
		channels = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, gpu_stats_channels);
		IOReportMergeChannels(channels, energy_channels, nullptr);
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
		//? Try IOHIDSensors first (works on M1 and some M2/M3)
		CFDictionaryRef matching = create_hid_matching(0xff00, 5);
		IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
		IOHIDEventSystemClientSetMatching(system, matching);
		CFArrayRef services = IOHIDEventSystemClientCopyServices(system);

		double gpu_temp = 0.0;
		int temp_count = 0;

		if (services != nullptr) {
			CFIndex count = CFArrayGetCount(services);
			for (CFIndex i = 0; i < count; i++) {
				IOHIDServiceClientRef service = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
				if (service != nullptr) {
					CFStringRef name = IOHIDServiceClientCopyProperty(service, CFSTR("Product"));
					if (name != nullptr) {
						std::string sensor_name = cf_string_to_string(name);
						CFRelease(name);

						//? Look for GPU temperature sensors (e.g., "GPU MTR Temp Sensor1")
						//? or general GPU temp sensors
						if (sensor_name.find("GPU") != std::string::npos) {
							IOHIDEventRef event = IOHIDServiceClientCopyEvent(service,
								kIOHIDEventTypeTemperature, 0, 0);
							if (event != nullptr) {
								double temp = IOHIDEventGetFloatValue(event,
									IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
								if (temp > 0 and temp < 150) {  // Sanity check
									gpu_temp += temp;
									temp_count++;
									Logger::debug("AppleSiliconGpu: Found GPU temp sensor '{}' = {:.1f}C", sensor_name, temp);
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

		if (temp_count > 0) {
			return gpu_temp / temp_count;
		}

		//? Fallback: Try SMC keys starting with "Tg" for GPU temperature
		// This is handled by the existing SMC infrastructure in btop
		return 0.0;
	}

	void AppleSiliconGpu::parse_channels(CFDictionaryRef delta, double elapsed_seconds,
	                                     double& out_freq_mhz, double& out_usage_percent,
	                                     double& out_power_watts, double& out_temp_celsius) {
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
		out_power_watts = 0.0;
		out_temp_celsius = 0.0;

		double gpu_energy_joules = 0.0;
		bool found_gpuph = false;
		bool found_gpu_energy = false;

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

			//? Log GPU-related channels for debugging (only on first call)
			if (first_call and (group.find("GPU") != std::string::npos or channel_name.find("GPU") != std::string::npos)) {
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

			//? GPU power from "Energy Model" / "GPU Energy" (or "DIE_X_GPU Energy" for multi-die)
			if (group == "Energy Model" and
			    (channel_name == "GPU Energy" or
			     channel_name.find("GPU Energy") != std::string::npos)) {
				found_gpu_energy = true;
				std::string unit = cf_string_to_string(IOReportChannelGetUnitLabel(channel));
				int64_t energy_value = IOReportSimpleGetIntegerValue(channel, 0);

				if (do_debug) Logger::debug("AppleSiliconGpu: GPU Energy channel: value={} unit='{}'", energy_value, unit);

				//? Convert to joules based on unit
				if (unit == "mJ") {
					gpu_energy_joules += energy_value / 1000.0;
				} else if (unit == "uJ") {
					gpu_energy_joules += energy_value / 1000000.0;
				} else if (unit == "nJ") {
					gpu_energy_joules += energy_value / 1000000000.0;
				} else {
					//? Assume nanojoules if unit is unknown
					gpu_energy_joules += energy_value / 1000000000.0;
					if (do_debug) Logger::debug("AppleSiliconGpu: Unknown energy unit '{}', assuming nJ", unit);
				}
			}
		}

		if (do_debug and not found_gpuph) {
			Logger::debug("AppleSiliconGpu: GPUPH channel not found");
		}
		if (do_debug and not found_gpu_energy) {
			Logger::debug("AppleSiliconGpu: GPU Energy channel not found");
		}

		//? Convert energy to power: watts = joules / seconds
		if (elapsed_seconds > 0 and gpu_energy_joules > 0) {
			out_power_watts = gpu_energy_joules / elapsed_seconds;
			if (do_debug) Logger::debug("AppleSiliconGpu: Power = {} W (energy = {} J, elapsed = {} s)",
			              out_power_watts, gpu_energy_joules, elapsed_seconds);
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
					double power_watts = 0.0;
					double temp_celsius = 0.0;

					parse_channels(delta, elapsed_seconds, freq_mhz, usage_percent, power_watts, temp_celsius);

					metrics.gpu_freq_mhz = freq_mhz;
					metrics.gpu_usage_percent = usage_percent;
					metrics.gpu_power_watts = power_watts;
					metrics.gpu_temp_celsius = temp_celsius;
					metrics.gpu_freq_max_mhz = max_gpu_freq_mhz;

					if (do_debug) {
						Logger::debug("AppleSiliconGpu: collect() - freq={}MHz usage={}% power={}W temp={}C elapsed={}s",
						              freq_mhz, usage_percent, power_watts, temp_celsius, elapsed_seconds);
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

		//? If IOReport didn't provide temperature, fall back to IOHIDSensors
		if (metrics.gpu_temp_celsius <= 0) {
			metrics.gpu_temp_celsius = get_gpu_temperature();
			if (do_debug and metrics.gpu_temp_celsius > 0) {
				Logger::debug("AppleSiliconGpu: Got temperature from IOHIDSensors: {}C", metrics.gpu_temp_celsius);
			}
		}

		return metrics;
	}

}  // namespace Gpu

#endif  // __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
