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

#include <cstddef>

#include <Availability.h>
#include <arpa/inet.h>
#include <libproc.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/mach_types.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
// BUGS
//     If both <net/if.h> and <ifaddrs.h> are being included, <net/if.h> must be
//     included before <ifaddrs.h>.
// from: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/getifaddrs.3.html
#include "../btop_config.hpp"
#include "../btop_log.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <netdb.h>
#include <netinet/in.h> // for inet_ntop
#include <netinet/tcp_fsm.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <fmt/format.h>

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
#    include "sensors.hpp"
#endif
#include "gpu.hpp"
#include "iokit.hpp"
#include "smc.hpp"

<<<<<<< HEAD
=======
#if defined(GPU_SUPPORT)
#include <dlfcn.h>
#include <mach/mach_time.h>

//? IOReport C function declarations for Apple Silicon GPU metrics
extern "C" {
	typedef struct IOReportSubscription* IOReportSubscriptionRef;

	CFDictionaryRef IOReportCopyChannelsInGroup(CFStringRef group, CFStringRef subgroup,
		uint64_t a, uint64_t b, uint64_t c);
	void IOReportMergeChannels(CFDictionaryRef a, CFDictionaryRef b, CFTypeRef cfnull);
	IOReportSubscriptionRef IOReportCreateSubscription(void* a, CFMutableDictionaryRef b,
		CFMutableDictionaryRef* c, uint64_t d, CFTypeRef cfnull);
	CFDictionaryRef IOReportCreateSamples(IOReportSubscriptionRef sub,
		CFMutableDictionaryRef chan, CFTypeRef cfnull);
	CFDictionaryRef IOReportCreateSamplesDelta(CFDictionaryRef a, CFDictionaryRef b, CFTypeRef cfnull);
	CFStringRef IOReportChannelGetGroup(CFDictionaryRef item);
	CFStringRef IOReportChannelGetSubGroup(CFDictionaryRef item);
	CFStringRef IOReportChannelGetChannelName(CFDictionaryRef item);
	int64_t IOReportSimpleGetIntegerValue(CFDictionaryRef item, int32_t idx);
	CFStringRef IOReportChannelGetUnitLabel(CFDictionaryRef item);
	int32_t IOReportStateGetCount(CFDictionaryRef item);
	CFStringRef IOReportStateGetNameForIndex(CFDictionaryRef item, int32_t idx);
	int64_t IOReportStateGetResidency(CFDictionaryRef item, int32_t idx);

	//? IOHIDEvent declarations for GPU temperature
	typedef struct __IOHIDEvent* IOHIDEventRef;
	typedef struct __IOHIDServiceClient* IOHIDServiceClientRef;
	typedef struct __IOHIDEventSystemClient* IOHIDEventSystemClientRef;
	#ifdef __LP64__
	typedef double IOHIDFloat;
	#else
	typedef float IOHIDFloat;
	#endif
	IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
	int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client, CFDictionaryRef match);
	CFArrayRef IOHIDEventSystemClientCopyServices(IOHIDEventSystemClientRef client);
	IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef sc, int64_t type, int32_t a, int64_t b);
	CFStringRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef property);
	IOHIDFloat IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);
}
#endif // GPU_SUPPORT

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

>>>>>>> official
using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? RAII wrapper for CoreFoundation types — releases via CFRelease() on destruction
template <typename T>
struct CFRef {
	T ref;
	CFRef() : ref(nullptr) {}
	CFRef(T ref) : ref(ref) {}
	~CFRef() { if (ref) CFRelease((CFTypeRef)ref); }
	CFRef(const CFRef&) = delete;
	CFRef& operator=(const CFRef&) = delete;
	CFRef(CFRef&& other) noexcept : ref(other.ref) { other.ref = nullptr; }
	CFRef& operator=(CFRef&& other) noexcept {
		if (this != &other) { reset(); ref = other.ref; other.ref = nullptr; }
		return *this;
	}
	operator T() const { return ref; }
	T get() const { return ref; }
	T* ptr() { return &ref; }
	void reset(T new_ref = nullptr) {
		if (ref) CFRelease((CFTypeRef)ref);
		ref = new_ref;
	}
	T release() { T r = ref; ref = nullptr; return r; }
};

//? RAII wrapper for IOKit object types — releases via IOObjectRelease() on destruction
struct IORef {
	io_object_t ref;
	IORef() : ref(0) {}
	IORef(io_object_t ref) : ref(ref) {}
	~IORef() { if (ref) IOObjectRelease(ref); }
	IORef(const IORef&) = delete;
	IORef& operator=(const IORef&) = delete;
	operator io_object_t() const { return ref; }
	io_object_t get() const { return ref; }
	io_object_t* ptr() { return &ref; }
};

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
    vector<long long> core_old_totals;
    vector<long long> core_old_idles;
    vector<string> available_fields = {"Auto", "total"};
    vector<string> available_sensors = {"Auto"};
    cpu_info current_cpu;
    bool got_sensors = false, cpu_temp_only = false, supports_watts = false;
    int core_offset = 0;

    //* Populate found_sensors map
    bool get_sensors();

    //* Get current cpu clock speed
    string get_cpuHz();

    //* Search /proc/cpuinfo for a cpu name
    string get_cpuName();

    struct Sensor {
        fs::path path;
        string label;
        int64_t temp = 0;
        int64_t high = 0;
        int64_t crit = 0;
    };

    string cpu_sensor;
    vector<string> core_sensors;
    std::unordered_map<int, int> core_mapping;
} // namespace Cpu

namespace Mem {
    double old_uptime;
}

<<<<<<< HEAD
class MachProcessorInfo {
  public:
    processor_info_array_t info_array;
    mach_msg_type_number_t info_count;
    MachProcessorInfo() {}
    virtual ~MachProcessorInfo() {
        vm_deallocate(mach_task_self(), (vm_address_t)info_array, (vm_size_t)sizeof(processor_info_array_t) * info_count);
    }
};

namespace Gpu {
    //? List of all GPU information
    std::vector<gpu_info> gpus;
#ifdef GPU_SUPPORT
    namespace IOAccelerator {
        bool initialized = false;

        size_t device_count = 0;
        //? IO_GPU contains the actual vector with the list of GPUs and their data to perform the query.
        IOGPU io_gpu;

        bool init();
        bool shutdown();
        template<bool is_init>
        bool collect(gpu_info* gpus_slice, size_t i = 0);

    } // namespace IOAccelerator
    auto collect(bool no_update) -> std::vector<gpu_info>&;
#endif // GPU_SUPPORT
} // namespace Gpu
=======
#if defined(GPU_SUPPORT)
namespace Gpu {
	vector<gpu_info> gpus;

	//? Stub shutdown for backends not available on macOS
	namespace Nvml { bool shutdown() { return false; } }
	namespace Rsmi { bool shutdown() { return false; } }

	//? Apple Silicon GPU data collection via IOReport
	namespace AppleSilicon {
		bool initialized = false;
		unsigned int device_count = 0;

		//? Forward declaration
		template <bool is_init>
		bool collect(gpu_info* gpus_slice);

		//? IOReport subscription state
		IOReportSubscriptionRef ior_sub = nullptr;
		CFMutableDictionaryRef ior_chan = nullptr;
		CFDictionaryRef prev_sample = nullptr;
		uint64_t prev_sample_time = 0;

		//? GPU frequency table from DVFS
		vector<uint32_t> gpu_freqs;

		static string cfstring_to_string(CFStringRef cfstr) {
			if (not cfstr) return "";
			char buf[256];
			if (CFStringGetCString(cfstr, buf, sizeof(buf), kCFStringEncodingUTF8))
				return string(buf);
			return "";
		}

		static string get_chip_name() {
			char buf[256];
			size_t size = sizeof(buf);
			if (sysctlbyname("machdep.cpu.brand_string", buf, &size, nullptr, 0) == 0)
				return string(buf);
			return "Apple Silicon GPU";
		}

		static uint64_t get_mach_time_ms() {
			static mach_timebase_info_data_t timebase = {0, 0};
			if (timebase.denom == 0) mach_timebase_info(&timebase);
			return (mach_absolute_time() * timebase.numer / timebase.denom) / 1000000;
		}

		//? Read GPU DVFS frequency table from IORegistry pmgr node
		static void get_gpu_freqs_from_pmgr() {
			io_iterator_t iter_raw;
			//? matchDict ownership is consumed by IOServiceGetMatchingServices
			CFMutableDictionaryRef matchDict = IOServiceMatching("AppleARMIODevice");
			if (IOServiceGetMatchingServices(kIOMainPortDefault, matchDict, &iter_raw) != kIOReturnSuccess)
				return;
			IORef iter(iter_raw);

			io_object_t entry_raw;
			while ((entry_raw = IOIteratorNext(iter)) != 0) {
				IORef entry(entry_raw);
				char name[128];
				if (IORegistryEntryGetName(entry, name) == kIOReturnSuccess and string(name) == "pmgr") {
					CFMutableDictionaryRef props_raw = nullptr;
					if (IORegistryEntryCreateCFProperties(entry, &props_raw, kCFAllocatorDefault, 0) == kIOReturnSuccess and props_raw) {
						CFRef<CFMutableDictionaryRef> props(props_raw);
						CFDataRef dvfs_data = (CFDataRef)CFDictionaryGetValue(props, CFSTR("voltage-states9"));
						if (dvfs_data) {
							auto len = CFDataGetLength(dvfs_data);
							auto ptr = CFDataGetBytePtr(dvfs_data);
							//? Pairs of (freq, voltage), 4 bytes each
							for (CFIndex i = 0; i + 7 < len; i += 8) {
								uint32_t freq = 0;
								memcpy(&freq, ptr + i, 4);
								if (freq > 0) gpu_freqs.push_back(freq / (1000 * 1000)); // Hz -> MHz
							}
						}
					}
				}
			}
		}

		bool init() {
			if (initialized) return false;

			//? Get GPU frequency table
			get_gpu_freqs_from_pmgr();

			//? Set up IOReport channels for GPU Stats and Energy Model
			CFRef<CFStringRef> gpu_stats_group(CFStringCreateWithCString(kCFAllocatorDefault, "GPU Stats", kCFStringEncodingUTF8));
			CFRef<CFStringRef> gpu_perf_subgroup(CFStringCreateWithCString(kCFAllocatorDefault, "GPU Performance States", kCFStringEncodingUTF8));
			CFRef<CFStringRef> energy_group(CFStringCreateWithCString(kCFAllocatorDefault, "Energy Model", kCFStringEncodingUTF8));

			CFRef<CFDictionaryRef> gpu_chan(IOReportCopyChannelsInGroup(gpu_stats_group, gpu_perf_subgroup, 0, 0, 0));
			CFRef<CFDictionaryRef> energy_chan(IOReportCopyChannelsInGroup(energy_group, nullptr, 0, 0, 0));

			if (not gpu_chan.get() and not energy_chan.get()) {
				Logger::info("Apple Silicon GPU: No IOReport channels found, GPU monitoring unavailable");
				return false;
			}

			//? Merge channels into a single subscription
			if (gpu_chan.get() and energy_chan.get()) {
				IOReportMergeChannels(gpu_chan, energy_chan, nullptr);
			}
			CFDictionaryRef base_chan = gpu_chan.get() ? gpu_chan.get() : energy_chan.get();

			auto size = CFDictionaryGetCount(base_chan);
			ior_chan = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, size, base_chan);

			//? Create IOReport subscription
			CFMutableDictionaryRef sub_dict = nullptr;
			ior_sub = IOReportCreateSubscription(nullptr, ior_chan, &sub_dict, 0, nullptr);
			if (not ior_sub) {
				Logger::warning("Apple Silicon GPU: Failed to create IOReport subscription");
				CFRelease(ior_chan);
				ior_chan = nullptr;
				return false;
			}

			device_count = 1; //? Apple Silicon has one integrated GPU
			gpus.resize(gpus.size() + device_count);
			gpu_names.resize(gpu_names.size() + device_count);

			initialized = true;

			//? Take initial sample for delta computation
			prev_sample = IOReportCreateSamples(ior_sub, ior_chan, nullptr);
			prev_sample_time = get_mach_time_ms();

			//? Run init collect to populate names and supported functions
			collect<1>(gpus.data());

			return true;
		}

		bool shutdown() {
			if (not initialized) return false;
			if (prev_sample) { CFRelease(prev_sample); prev_sample = nullptr; }
			if (ior_chan) { CFRelease(ior_chan); ior_chan = nullptr; }
			if (ior_sub) { CFRelease((CFTypeRef)ior_sub); ior_sub = nullptr; }
			initialized = false;
			return true;
		}

		//? Read GPU temperature via IOHIDEventSystem thermal sensors
		static long long get_gpu_temp_iohid() {
			#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
			constexpr int kHIDPage_AppleVendor = 0xff00;
			constexpr int kHIDUsage_TemperatureSensor = 5;
			constexpr int64_t kIOHIDEventTypeTemperature = 15;

			CFStringRef keys[2] = { CFSTR("PrimaryUsagePage"), CFSTR("PrimaryUsage") };
			int page = kHIDPage_AppleVendor, usage = kHIDUsage_TemperatureSensor;
			CFRef<CFNumberRef> num0(CFNumberCreate(nullptr, kCFNumberSInt32Type, &page));
			CFRef<CFNumberRef> num1(CFNumberCreate(nullptr, kCFNumberSInt32Type, &usage));
			const void* values[] = { num0.get(), num1.get() };
			CFRef<CFDictionaryRef> match(CFDictionaryCreate(nullptr,
				(const void**)keys, values, 2,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

			CFRef<IOHIDEventSystemClientRef> system(IOHIDEventSystemClientCreate(kCFAllocatorDefault));
			if (not system.get()) return -1;
			IOHIDEventSystemClientSetMatching(system, match);
			CFRef<CFArrayRef> services(IOHIDEventSystemClientCopyServices(system));

			if (not services.get()) return -1;

			double gpu_temp_sum = 0;
			int gpu_temp_count = 0;
			long count = CFArrayGetCount(services);
			for (long i = 0; i < count; i++) {
				auto sc = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
				if (not sc) continue;
				CFRef<CFStringRef> name(IOHIDServiceClientCopyProperty(sc, CFSTR("Product")));
				if (not name.get()) continue;
				char buf[200];
				CFStringGetCString(name, buf, 200, kCFStringEncodingASCII);
				string n(buf);
				//? "GPU MTR Temp Sensor" is the standard Apple Silicon GPU temp sensor name
				if (n.find("GPU") != string::npos) {
					CFRef<IOHIDEventRef> event(IOHIDServiceClientCopyEvent(sc, kIOHIDEventTypeTemperature, 0, 0));
					if (event.get()) {
						double temp = IOHIDEventGetFloatValue(event, kIOHIDEventTypeTemperature << 16);
						if (temp > 0 and temp < 150) {
							gpu_temp_sum += temp;
							gpu_temp_count++;
						}
					}
				}
			}

			if (gpu_temp_count > 0)
				return static_cast<long long>(round(gpu_temp_sum / gpu_temp_count));
			#endif
			return -1;
		}

		template <bool is_init>
		bool collect(gpu_info* gpus_slice) {
			if (not initialized) return false;

			if constexpr (is_init) {
				//? Device name
				string chip = get_chip_name();
				gpu_names[0] = chip + " GPU";

				//? Power max (typical Apple Silicon GPU TDP ~15-20W)
				gpus_slice[0].pwr_max_usage = 20000; // 20W in mW
				gpu_pwr_total_max += gpus_slice[0].pwr_max_usage;

				//? Temperature max
				gpus_slice[0].temp_max = 110;

				//? Memory total (unified memory architecture — GPU shares system RAM)
				int64_t memsize = 0;
				size_t size = sizeof(memsize);
				if (sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0) == 0)
					gpus_slice[0].mem_total = memsize;

				//? Supported functions
				gpus_slice[0].supported_functions = {
					.gpu_utilization = true,
					.mem_utilization = true,
					.gpu_clock = not gpu_freqs.empty(),
					.mem_clock = false,
					.pwr_usage = true,
					.pwr_state = false,
					.temp_info = true,
					.mem_total = true,
					.mem_used = true,
					.pcie_txrx = false,
					.encoder_utilization = false,
					.decoder_utilization = false
				};
			}

			//? Take new IOReport sample and compute delta
			CFDictionaryRef cur_sample = IOReportCreateSamples(ior_sub, ior_chan, nullptr);
			if (not cur_sample) return false;

			uint64_t cur_time = get_mach_time_ms();
			uint64_t dt = cur_time - prev_sample_time;
			if (dt == 0) dt = 1;

			CFRef<CFDictionaryRef> delta;
			if (prev_sample) {
				delta.reset(IOReportCreateSamplesDelta(prev_sample, cur_sample, nullptr));
				CFRelease(prev_sample);
			}
			prev_sample = cur_sample;
			prev_sample_time = cur_time;

			if (not delta.get()) return false;

			//? Parse delta samples
			CFArrayRef channels = (CFArrayRef)CFDictionaryGetValue(delta, CFSTR("IOReportChannels"));
			if (not channels) return false;

			long long gpu_utilization = 0;
			bool got_gpu_util = false;
			double gpu_power_watts = 0;
			bool got_gpu_power = false;

			long chan_count = CFArrayGetCount(channels);
			for (long i = 0; i < chan_count; i++) {
				CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(channels, i);
				if (not item) continue;

				string group = cfstring_to_string(IOReportChannelGetGroup(item));
				string subgroup = cfstring_to_string(IOReportChannelGetSubGroup(item));
				string channel = cfstring_to_string(IOReportChannelGetChannelName(item));

				//? GPU utilization from residency states
				if (group == "GPU Stats" and subgroup == "GPU Performance States" and channel == "GPUPH") {
					int32_t state_count = IOReportStateGetCount(item);
					if (state_count <= 0) continue;

					int64_t total_residency = 0;
					int64_t active_residency = 0;
					double weighted_freq = 0;

					//? Find offset past IDLE/OFF/DOWN states
					int offset = 0;
					for (int32_t s = 0; s < state_count; s++) {
						string name = cfstring_to_string(IOReportStateGetNameForIndex(item, s));
						if (name == "IDLE" or name == "OFF" or name == "DOWN")
							offset = s + 1;
						total_residency += IOReportStateGetResidency(item, s);
					}

					int freq_count = static_cast<int>(gpu_freqs.size());
					for (int32_t s = offset; s < state_count; s++) {
						int64_t res = IOReportStateGetResidency(item, s);
						active_residency += res;
						int freq_idx = s - offset;
						if (freq_idx < freq_count and active_residency > 0)
							weighted_freq += static_cast<double>(res) * gpu_freqs[freq_idx];
					}

					if (total_residency > 0) {
						double usage_ratio = static_cast<double>(active_residency) / static_cast<double>(total_residency);
						gpu_utilization = clamp(static_cast<long long>(round(usage_ratio * 100.0)), 0ll, 100ll);
						got_gpu_util = true;

						//? Calculate average frequency
						if (active_residency > 0 and not gpu_freqs.empty()) {
							double avg_freq = weighted_freq / static_cast<double>(active_residency);
							gpus_slice[0].gpu_clock_speed = static_cast<unsigned int>(round(avg_freq));
						}
					}
				}

				//? GPU power from Energy Model
				if (group == "Energy Model" and channel == "GPU Energy") {
					string unit = cfstring_to_string(IOReportChannelGetUnitLabel(item));
					int64_t val = IOReportSimpleGetIntegerValue(item, 0);
					double energy = static_cast<double>(val);
					double divisor = static_cast<double>(dt) / 1000.0; // dt is in ms

					if (unit.find("nJ") != string::npos) energy /= 1e9;
					else if (unit.find("uJ") != string::npos or unit.find("\xc2\xb5J") != string::npos) energy /= 1e6;
					else if (unit.find("mJ") != string::npos) energy /= 1e3;
					//? energy is now in Joules

					if (divisor > 0) {
						gpu_power_watts = energy / divisor;
						got_gpu_power = true;
					}
				}
			}

			//? Store GPU utilization
			if (got_gpu_util) {
				gpus_slice[0].gpu_percent.at("gpu-totals").push_back(gpu_utilization);
				gpus_slice[0].mem_utilization_percent.push_back(gpu_utilization);
			}

			//? Store power usage (convert W to mW)
			if (got_gpu_power) {
				gpus_slice[0].pwr_usage = static_cast<long long>(round(gpu_power_watts * 1000.0));
				if (gpus_slice[0].pwr_usage > gpus_slice[0].pwr_max_usage)
					gpus_slice[0].pwr_max_usage = gpus_slice[0].pwr_usage;
				gpus_slice[0].gpu_percent.at("gpu-pwr-totals").push_back(
					clamp(static_cast<long long>(round(static_cast<double>(gpus_slice[0].pwr_usage) * 100.0 / static_cast<double>(gpus_slice[0].pwr_max_usage))), 0ll, 100ll));
			}

			//? GPU temperature
			if (gpus_slice[0].supported_functions.temp_info and Config::getB("check_temp")) {
				long long temp = get_gpu_temp_iohid();
				if (temp > 0)
					gpus_slice[0].temp.push_back(temp);
			}

			//? Memory usage (unified memory — report system memory usage)
			if (gpus_slice[0].supported_functions.mem_total) {
				vm_size_t page_size;
				mach_port_t mach_port = mach_host_self();
				vm_statistics64_data_t vm_stats;
				mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
				host_page_size(mach_port, &page_size);

				if (host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
					long long used = (static_cast<int64_t>(vm_stats.active_count)
						+ static_cast<int64_t>(vm_stats.inactive_count)
						+ static_cast<int64_t>(vm_stats.wire_count)
						+ static_cast<int64_t>(vm_stats.speculative_count)
						+ static_cast<int64_t>(vm_stats.compressor_page_count)
						- static_cast<int64_t>(vm_stats.purgeable_count)
						- static_cast<int64_t>(vm_stats.external_page_count)) * static_cast<int64_t>(page_size);
					if (used < 0) used = 0;
					gpus_slice[0].mem_used = used;
					if (gpus_slice[0].mem_total > 0) {
						auto used_pct = static_cast<long long>(round(static_cast<double>(used) * 100.0 / static_cast<double>(gpus_slice[0].mem_total)));
						gpus_slice[0].gpu_percent.at("gpu-vram-totals").push_back(clamp(used_pct, 0ll, 100ll));
					}
				}
			}

			return true;
		}

		//? Explicit template instantiations
		template bool collect<true>(gpu_info*);
		template bool collect<false>(gpu_info*);
	} // namespace AppleSilicon

	//? Collect data from Apple Silicon GPU
	auto collect(bool no_update) -> vector<gpu_info>& {
		if (Runner::stopping or (no_update and not gpus.empty())) return gpus;

		AppleSilicon::collect<0>(gpus.data());

		//* Calculate averages
		long long avg = 0;
		long long mem_usage_total = 0;
		long long mem_total = 0;
		long long pwr_total = 0;
		for (auto& gpu : gpus) {
			if (gpu.supported_functions.gpu_utilization and not gpu.gpu_percent.at("gpu-totals").empty())
				avg += gpu.gpu_percent.at("gpu-totals").back();
			if (gpu.supported_functions.mem_used)
				mem_usage_total += gpu.mem_used;
			if (gpu.supported_functions.mem_total)
				mem_total += gpu.mem_total;
			if (gpu.supported_functions.pwr_usage)
				pwr_total += gpu.pwr_usage;

			//* Trim vectors if there are more values than needed for graphs
			if (width != 0) {
				while (cmp_greater(gpu.gpu_percent.at("gpu-totals").size(), width * 2)) gpu.gpu_percent.at("gpu-totals").pop_front();
				while (cmp_greater(gpu.mem_utilization_percent.size(), width)) gpu.mem_utilization_percent.pop_front();
				while (cmp_greater(gpu.gpu_percent.at("gpu-pwr-totals").size(), width)) gpu.gpu_percent.at("gpu-pwr-totals").pop_front();
				while (cmp_greater(gpu.temp.size(), 18)) gpu.temp.pop_front();
				while (cmp_greater(gpu.gpu_percent.at("gpu-vram-totals").size(), width/2)) gpu.gpu_percent.at("gpu-vram-totals").pop_front();
			}
		}

		if (not gpus.empty()) {
			shared_gpu_percent.at("gpu-average").push_back(avg / static_cast<long long>(gpus.size()));
			if (mem_total != 0)
				shared_gpu_percent.at("gpu-vram-total").push_back(mem_usage_total * 100 / mem_total);
			if (gpu_pwr_total_max != 0)
				shared_gpu_percent.at("gpu-pwr-total").push_back(pwr_total * 100 / gpu_pwr_total_max);
		}

		if (width != 0) {
			while (cmp_greater(shared_gpu_percent.at("gpu-average").size(), width * 2)) shared_gpu_percent.at("gpu-average").pop_front();
			while (cmp_greater(shared_gpu_percent.at("gpu-vram-total").size(), width)) shared_gpu_percent.at("gpu-vram-total").pop_front();
			while (cmp_greater(shared_gpu_percent.at("gpu-pwr-total").size(), width)) shared_gpu_percent.at("gpu-pwr-total").pop_front();
		}

		return gpus;
	}
} // namespace Gpu
#endif // GPU_SUPPORT

	class MachProcessorInfo {
	public:
		processor_info_array_t info_array;
		mach_msg_type_number_t info_count;
		MachProcessorInfo() {}
		virtual ~MachProcessorInfo() {vm_deallocate(mach_task_self(), (vm_address_t)info_array, (vm_size_t)sizeof(processor_info_array_t) * info_count);}
	};
>>>>>>> official

namespace Shared {
    fs::path passwd_path;
    uint64_t totalMem;
    long pageSize, coreCount, clkTck, physicalCoreCount, arg_max;
    double machTck;
    int totalMem_len;
    void init() {
        //? Shared global variables init
        coreCount = sysconf(_SC_NPROCESSORS_ONLN); // this returns all logical cores (threads)
        if (coreCount < 1) {
            coreCount = 1;
            Logger::warning("Could not determine number of cores, defaulting to 1.");
        }

        size_t physicalCoreCountSize = sizeof(physicalCoreCount);
        if (sysctlbyname("hw.physicalcpu", &physicalCoreCount, &physicalCoreCountSize, nullptr, 0) < 0) {
            Logger::error("Could not get physical core count");
        }

        pageSize = sysconf(_SC_PAGE_SIZE);
        if (pageSize <= 0) {
            pageSize = 4096;
            Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
        }

        mach_timebase_info_data_t convf;
        if (mach_timebase_info(&convf) == KERN_SUCCESS) {
            machTck = convf.numer / convf.denom;
        } else {
            Logger::warning(
                "Could not get mach clock tick conversion factor. Defaulting to 100, processes cpu usage might be incorrect."
            );
            machTck = 100;
        }

        clkTck = sysconf(_SC_CLK_TCK);
        if (clkTck <= 0) {
            clkTck = 100;
            Logger::warning(
                "Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect."
            );
        }

        int64_t memsize = 0;
        size_t size = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0) < 0) {
            Logger::warning("Could not get memory size");
        }
        totalMem = memsize;

        //* Get maximum length of process arguments
        arg_max = sysconf(_SC_ARG_MAX);

        //? Init for namespace Cpu
        Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
        Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
        Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
        Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
        Cpu::collect();
        for (auto& [field, vec] : Cpu::current_cpu.cpu_percent) {
            if (not vec.empty() and not v_contains(Cpu::available_fields, field))
                Cpu::available_fields.push_back(field);
        }
        Cpu::cpuName = Cpu::get_cpuName();
        Cpu::got_sensors = Cpu::get_sensors();
        Cpu::core_mapping = Cpu::get_core_mapping();

        //? Init for namespace Mem
        Mem::old_uptime = system_uptime();
        Mem::collect();

#ifdef GPU_SUPPORT
        auto shown_gpus = Config::getS("shown_gpus");

        if (shown_gpus.contains("apple")) {
            Gpu::IOAccelerator::init();
        }

<<<<<<< HEAD
        if (not Gpu::gpu_names.empty()) {
            for (const auto& [key, _] : Gpu::gpus[0].gpu_percent)
                Cpu::available_fields.push_back(key);
            for (const auto& [key, _] : Gpu::shared_gpu_percent)
                Cpu::available_fields.push_back(key);
=======
		//? Init for namespace Gpu
	#ifdef GPU_SUPPORT
		auto shown_gpus = Config::getS("shown_gpus");
		if (shown_gpus.contains("apple")) {
			Gpu::AppleSilicon::init();
		}

		if (not Gpu::gpu_names.empty()) {
			for (auto const& [key, _] : Gpu::gpus[0].gpu_percent)
				Cpu::available_fields.push_back(key);
			for (auto const& [key, _] : Gpu::shared_gpu_percent)
				Cpu::available_fields.push_back(key);

			using namespace Gpu;
			count = gpus.size();
			gpu_b_height_offsets.resize(gpus.size());
			for (size_t i = 0; i < gpu_b_height_offsets.size(); ++i)
				gpu_b_height_offsets[i] = gpus[i].supported_functions.gpu_utilization
					+ gpus[i].supported_functions.pwr_usage
					+ (gpus[i].supported_functions.encoder_utilization or gpus[i].supported_functions.decoder_utilization)
					+ (gpus[i].supported_functions.mem_total or gpus[i].supported_functions.mem_used)
						* (1 + 2*(gpus[i].supported_functions.mem_total and gpus[i].supported_functions.mem_used) + 2*gpus[i].supported_functions.mem_utilization);
		}
	#endif

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Mem::collect();
	}
>>>>>>> official

            using namespace Gpu;
            count = gpus.size();
            gpu_b_height_offsets.resize(gpus.size());
            for (size_t i = 0; i < gpu_b_height_offsets.size(); ++i)
                gpu_b_height_offsets[i] =
                    gpus[i].supported_functions.gpu_utilization + gpus[i].supported_functions.pwr_usage +
                    (gpus[i].supported_functions.encoder_utilization or gpus[i].supported_functions.decoder_utilization) +
                    (gpus[i].supported_functions.mem_total or gpus[i].supported_functions.mem_used) *
                        (1 + 2 * (gpus[i].supported_functions.mem_total and gpus[i].supported_functions.mem_used) +
                         2 * gpus[i].supported_functions.mem_utilization);
        }
#endif
    }

} // namespace Shared

namespace Cpu {
    string cpuName;
    string cpuHz;
    bool has_battery = true;
    bool macM1 = false;
    tuple<int, float, long, string> current_bat;

    const array<string, 10> time_names = {"user", "nice", "system", "idle"};

    std::unordered_map<string, long long> cpu_old = {
        {"totals", 0}, {"idles", 0}, {"user", 0}, {"nice", 0}, {"system", 0}, {"idle", 0}
    };

    string get_cpuName() {
        string name;
        char buffer[1024];
        size_t size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, nullptr, 0) < 0) {
            Logger::error("Failed to get CPU name");
            return name;
        }
        return trim_name(string(buffer));
    }

    bool get_sensors() {
        Logger::debug("get_sensors(): show_coretemp={} check_temp={}", Config::getB("show_coretemp"), Config::getB("check_temp"));
        got_sensors = false;
        if (Config::getB("show_coretemp") and Config::getB("check_temp")) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
            ThermalSensors sensors;
            if (sensors.getSensors() > 0) {
                Logger::debug("M1 sensors found");
                got_sensors = true;
                cpu_temp_only = true;
                macM1 = true;
            } else {
#endif
                // try SMC (intel)
                Logger::debug("checking intel");
                try {
                    SMCConnection smcCon;
                    Logger::debug("SMC connection established");
                    long long t = smcCon.getTemp(-1); // check if we have package T
                    if (t > -1) {
                        Logger::debug("intel sensors found");
                        got_sensors = true;
                        t = smcCon.getTemp(0);
                        if (t == -1) {
                            // for some macs the core offset is 1 - check if we get a sane value with 1
                            if (smcCon.getTemp(1) > -1) {
                                Logger::debug("intel sensors with offset 1");
                                core_offset = 1;
                            }
                        }
                    } else {
                        Logger::debug("no intel sensors found");
                        got_sensors = false;
                    }
                } catch (std::runtime_error& e) {
                    Logger::debug("SMC not available: {}", e.what());
                    // ignore, we don't have temp (common in VMs)
                    got_sensors = false;
                }
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
            }
#endif
        }
        return got_sensors;
    }

    void update_sensors() {
        current_cpu.temp_max = 95; // we have no idea how to get the critical temp
        try {
            if (macM1) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
                ThermalSensors sensors;
                current_cpu.temp.at(0).push_back(sensors.getSensors());
                if (current_cpu.temp.at(0).size() > 20)
                    current_cpu.temp.at(0).pop_front();
#endif
            } else {
                SMCConnection smcCon;
                int threadsPerCore = Shared::coreCount / Shared::physicalCoreCount;
                long long packageT = smcCon.getTemp(-1); // -1 returns package T
                current_cpu.temp.at(0).push_back(packageT);

                for (int core = 0; core < Shared::coreCount; core++) {
                    long long temp =
                        smcCon.getTemp((core / threadsPerCore) + core_offset); // same temp for all threads of same physical core
                    if (cmp_less(core + 1, current_cpu.temp.size())) {
                        current_cpu.temp.at(core + 1).push_back(temp);
                        if (current_cpu.temp.at(core + 1).size() > 20)
                            current_cpu.temp.at(core + 1).pop_front();
                    }
                }
            }
        } catch (std::runtime_error& e) {
            got_sensors = false;
            Logger::error("failed getting CPU temp");
        }
    }

    std::string get_cpuHz() {
        unsigned int freq = 1;
        size_t size = sizeof(freq);
        int mib[] = {CTL_HW, HW_CPU_FREQ};

        if (sysctl(mib, 2, &freq, &size, nullptr, 0) < 0) {
            return "";
        }

        // Convert from Hz to GHz and return a numeric string without units
        const auto freq_ghz = static_cast<double>(freq) / 1'000'000'000.0;
        return fmt::format("{:.2f}", freq_ghz);
    }

    auto get_core_mapping() -> std::unordered_map<int, int> {
        std::unordered_map<int, int> core_map;
        if (cpu_temp_only)
            return core_map;

        natural_t cpu_count;
        natural_t i;
        MachProcessorInfo info {};
        kern_return_t error;

        error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info.info_array, &info.info_count);
        if (error != KERN_SUCCESS) {
            Logger::error("Failed getting CPU info");
            return core_map;
        }
        for (i = 0; i < cpu_count; i++) {
            core_map[i] = i;
        }

        //? If core mapping from cpuinfo was incomplete try to guess remainder, if missing completely, map 0-0 1-1 2-2 etc.
        if (cmp_less(core_map.size(), Shared::coreCount)) {
            if (Shared::coreCount % 2 == 0 and (long) core_map.size() == Shared::coreCount / 2) {
                for (int i = 0, n = 0; i < Shared::coreCount / 2; i++) {
                    if (std::cmp_greater_equal(n, core_sensors.size()))
                        n = 0;
                    core_map[Shared::coreCount / 2 + i] = n++;
                }
            } else {
                core_map.clear();
                for (int i = 0, n = 0; i < Shared::coreCount; i++) {
                    if (std::cmp_greater_equal(n, core_sensors.size()))
                        n = 0;
                    core_map[i] = n++;
                }
            }
        }

        //? Apply user set custom mapping if any
        const auto& custom_map = Config::getS("cpu_core_map");
        if (not custom_map.empty()) {
            try {
                for (const auto& split : ssplit(custom_map)) {
                    const auto vals = ssplit(split, ':');
                    if (vals.size() != 2)
                        continue;
                    int change_id = std::stoi(vals.at(0));
                    int new_id = std::stoi(vals.at(1));
                    if (not core_map.contains(change_id) or cmp_greater(new_id, core_sensors.size()))
                        continue;
                    core_map.at(change_id) = new_id;
                }
            } catch (...) {}
        }

        return core_map;
    }

    class IOPSInfo_Wrap {
        CFTypeRef data;

      public:
        IOPSInfo_Wrap() { data = IOPSCopyPowerSourcesInfo(); }
        CFTypeRef& operator()() { return data; }
        ~IOPSInfo_Wrap() { CFRelease(data); }
    };

    class IOPSList_Wrap {
        CFArrayRef data;

      public:
        IOPSList_Wrap(CFTypeRef cft_ref) { data = IOPSCopyPowerSourcesList(cft_ref); }
        CFArrayRef& operator()() { return data; }
        ~IOPSList_Wrap() { CFRelease(data); }
    };

    auto get_battery() -> tuple<int, float, long, string> {
        if (not has_battery)
            return {0, 0, 0, ""};

        uint32_t percent = -1;
        long seconds = -1;
        string status = "discharging";
        IOPSInfo_Wrap ps_info {};
        if (ps_info()) {
            IOPSList_Wrap one_ps_descriptor(ps_info());
            if (one_ps_descriptor()) {
                if (CFArrayGetCount(one_ps_descriptor())) {
                    CFDictionaryRef one_ps =
                        IOPSGetPowerSourceDescription(ps_info(), CFArrayGetValueAtIndex(one_ps_descriptor(), 0));
                    has_battery = true;

                    int32_t estimatedMinutesRemaining =
                        safe_cfdictionary_to_int64(one_ps, CFSTR(kIOPSTimeToEmptyKey)).value_or(0);
                    seconds = estimatedMinutesRemaining * 60;
                    percent = safe_cfdictionary_to_int64(one_ps, CFSTR(kIOPSCurrentCapacityKey)).value_or(0);
                    bool charging = safe_cfdictionary_to_bool(one_ps, CFSTR(kIOPSIsChargingKey)).value_or(false);
                    if (charging) {
                        status = "charging";
                    }
                    if (percent == 100) {
                        status = "full";
                    }
                } else {
                    has_battery = false;
                }
            } else {
                has_battery = false;
            }
        }
        return {percent, -1, seconds, status};
    }

    auto collect(bool no_update) -> cpu_info& {
        if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty()))
            return current_cpu;
        auto& cpu = current_cpu;

        if (getloadavg(cpu.load_avg.data(), cpu.load_avg.size()) < 0) {
            Logger::error("failed to get load averages");
        }

        natural_t cpu_count;
        natural_t i;
        kern_return_t error;
        processor_cpu_load_info_data_t* cpu_load_info = nullptr;

        MachProcessorInfo info {};
        error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info.info_array, &info.info_count);
        if (error != KERN_SUCCESS) {
            Logger::error("Failed getting CPU load info");
        }
        cpu_load_info = (processor_cpu_load_info_data_t*)info.info_array;
        long long global_totals = 0;
        long long global_idles = 0;
        vector<long long> times_summed = {0, 0, 0, 0};
        for (i = 0; i < cpu_count; i++) {
            vector<long long> times;
            //? 0=user, 1=nice, 2=system, 3=idle
            for (int x = 0; const unsigned int c_state : {CPU_STATE_USER, CPU_STATE_NICE, CPU_STATE_SYSTEM, CPU_STATE_IDLE}) {
                auto val = cpu_load_info[i].cpu_ticks[c_state];
                times.push_back(val);
                times_summed.at(x++) += val;
            }

            try {
                //? All values
                const long long totals = std::accumulate(times.begin(), times.end(), 0ll);

                //? Idle time
                const long long idles = times.at(3);

                global_totals += totals;
                global_idles += idles;

                //? Calculate cpu total for each core
                if (i > Shared::coreCount)
                    break;
                const long long calc_totals = max(0ll, totals - core_old_totals.at(i));
                const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
                core_old_totals.at(i) = totals;
                core_old_idles.at(i) = idles;

                cpu.core_percent.at(i).push_back(
                    clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll)
                );

                //? Reduce size if there are more values than needed for graph
                if (cpu.core_percent.at(i).size() > 40)
                    cpu.core_percent.at(i).pop_front();

            } catch (const std::exception& e) {
                Logger::error("Cpu::collect() : {}", e.what());
                throw std::runtime_error(fmt::format("collect() : {}", e.what()));
            }
        }

        const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
        const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));

        //? Populate cpu.cpu_percent with all fields from syscall
        for (int ii = 0; const auto& val : times_summed) {
            cpu.cpu_percent.at(time_names.at(ii))
                .push_back(
                    clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll)
                );
            cpu_old.at(time_names.at(ii)) = val;

            //? Reduce size if there are more values than needed for graph
            while (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), width * 2))
                cpu.cpu_percent.at(time_names.at(ii)).pop_front();

            ii++;
        }

        cpu_old.at("totals") = global_totals;
        cpu_old.at("idles") = global_idles;

        //? Total usage of cpu
        cpu.cpu_percent.at("total").push_back(
            clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll)
        );

        //? Reduce size if there are more values than needed for graph
        while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2))
            cpu.cpu_percent.at("total").pop_front();

        if (Config::getB("show_cpu_freq")) {
            auto hz = get_cpuHz();
            if (hz != "") {
                cpuHz = hz;
            }
        }

        if (Config::getB("check_temp") and got_sensors)
            update_sensors();

        if (Config::getB("show_battery") and has_battery)
            current_bat = get_battery();

        return cpu;
    }
} // namespace Cpu

namespace Mem {
    bool has_swap = false;
    vector<string> fstab;
    fs::file_time_type fstab_time;
    int disk_ios = 0;
    vector<string> last_found;
    static std::mutex iokit_mutex;     // Protect concurrent IOKit calls
    static std::mutex interface_mutex; // Protect concurrent interface access during USB device changes

    mem_info current_mem {};

    uint64_t get_totalMem() {
        return Shared::totalMem;
    }

    bool isWhole(io_registry_entry_t volumeRef) {
        CFBooleanRef isWhole = (CFBooleanRef)IORegistryEntryCreateCFProperty(volumeRef, CFSTR("Whole"), kCFAllocatorDefault, 0);
        Boolean val = CFBooleanGetValue(isWhole);
        CFRelease(isWhole);
        return bool(val);
    }

    class IOObject {
      public:
        IOObject(string name, io_object_t& obj) : name(name), object(obj) {}
        virtual ~IOObject() { IOObjectRelease(object); }

      private:
        string name;
        io_object_t& object;
    };

    void collect_disk(std::unordered_map<string, disk_info>& disks, std::unordered_map<string, string>& mapping) {
        // Lock mutex to prevent concurrent IOKit access
        std::lock_guard<std::mutex> lock(iokit_mutex);

        io_registry_entry_t drive;
        io_iterator_t drive_list;

        /* Get the list of all drive objects. */
        if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOMediaBSDClient"), &drive_list)) {
            Logger::error("Error in IOServiceGetMatchingServices()");
            return;
        }
        auto d = IOObject("drive list", drive_list); // dummy var so it gets destroyed
        while ((drive = IOIteratorNext(drive_list)) != 0) {
            auto dr = IOObject("drive", drive);
            io_registry_entry_t volumeRef;
            IORegistryEntryGetParentEntry(drive, kIOServicePlane, &volumeRef);
            if (volumeRef) {
                if (!isWhole(volumeRef)) {
                    string bsdName;
                    string device;

                    CFMutableDictionaryRef properties = nullptr;
                    if (IORegistryEntryCreateCFProperties(volumeRef, &properties, kCFAllocatorDefault, 0) == KERN_SUCCESS) {
                        bsdName = safe_cfdictionary_to_std_string(properties, CFSTR("BSD Name")).value_or("");
                        device = safe_cfdictionary_to_std_string(properties, CFSTR("VolGroupMntFromName")).value_or("");
                    }

                    if (!mapping.contains(device)) {
                        device = "/dev/" +
                                 bsdName; // try again with BSD name - not all volumes seem to have VolGroupMntFromName property
                    }
                    if (device != "") {
                        if (mapping.contains(device)) {
                            string mountpoint = mapping.at(device);
                            if (disks.contains(mountpoint)) {
                                auto& disk = disks.at(mountpoint);
                                if (properties) {
                                    CFDictionaryRef statistics =
                                        (CFDictionaryRef)CFDictionaryGetValue(properties, CFSTR("Statistics"));
                                    if (statistics) {
                                        disk_ios++;
                                        int64_t readBytes =
                                            safe_cfdictionary_to_int64(statistics, CFSTR("Bytes read from block device"))
                                                .value_or(0);
                                        if (disk.io_read.empty())
                                            disk.io_read.push_back(0);
                                        else
                                            disk.io_read.push_back(max((int64_t)0, (readBytes - disk.old_io.at(0))));
                                        disk.old_io.at(0) = readBytes;
                                        while (cmp_greater(disk.io_read.size(), width * 2))
                                            disk.io_read.pop_front();

                                        int64_t writeBytes =
                                            safe_cfdictionary_to_int64(statistics, CFSTR("Bytes written to block device"))
                                                .value_or(0);
                                        if (disk.io_write.empty())
                                            disk.io_write.push_back(0);
                                        else
                                            disk.io_write.push_back(max((int64_t)0, (writeBytes - disk.old_io.at(1))));
                                        disk.old_io.at(1) = writeBytes;
                                        while (cmp_greater(disk.io_write.size(), width * 2))
                                            disk.io_write.pop_front();

                                        // IOKit does not give us IO times, (use IO read + IO write with 1 MiB being 100% to get
                                        // some activity indication)
                                        if (disk.io_activity.empty())
                                            disk.io_activity.push_back(0);
                                        else
                                            disk.io_activity.push_back(clamp(
                                                (long)round((double)(disk.io_write.back() + disk.io_read.back()) / (1 << 20)),
                                                0l,
                                                100l
                                            ));
                                        while (cmp_greater(disk.io_activity.size(), width * 2))
                                            disk.io_activity.pop_front();
                                    }
                                }
                            }
                        }
                    }

                    if (properties)
                        CFRelease(properties);
                }
            }
        }
    }

    auto collect(bool no_update) -> mem_info& {
        if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
            return current_mem;

        auto show_swap = Config::getB("show_swap");
        auto show_disks = Config::getB("show_disks");
        auto swap_disk = Config::getB("swap_disk");
        auto& mem = current_mem;
        static bool snapped = (getenv("BTOP_SNAPPED") != nullptr);

        vm_statistics64 p;
        mach_msg_type_number_t info_size = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&p, &info_size) == 0) {
            mem.stats.at("free") = p.free_count * Shared::pageSize;
            mem.stats.at("cached") = p.external_page_count * Shared::pageSize;
            mem.stats.at("used") = (p.active_count + p.wire_count) * Shared::pageSize;
            mem.stats.at("available") = Shared::totalMem - mem.stats.at("used");
        }

        int mib[2] = {CTL_VM, VM_SWAPUSAGE};

        struct xsw_usage swap;
        size_t len = sizeof(struct xsw_usage);
        if (sysctl(mib, 2, &swap, &len, nullptr, 0) == 0) {
            mem.stats.at("swap_total") = swap.xsu_total;
            mem.stats.at("swap_free") = swap.xsu_avail;
            mem.stats.at("swap_used") = swap.xsu_used;
        }

        if (show_swap and mem.stats.at("swap_total") > 0) {
            for (const auto& name : swap_names) {
                mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
                while (cmp_greater(mem.percent.at(name).size(), width * 2))
                    mem.percent.at(name).pop_front();
            }
            has_swap = true;
        } else
            has_swap = false;
        //? Calculate percentages
        for (const auto& name : mem_names) {
            mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / Shared::totalMem));
            while (cmp_greater(mem.percent.at(name).size(), width * 2))
                mem.percent.at(name).pop_front();
        }

        if (show_disks) {
            std::unordered_map<string, string>
                mapping; // keep mapping from device -> mountpoint, since IOKit doesn't give us the mountpoint
            double uptime = system_uptime();
            auto& disks_filter = Config::getS("disks_filter");
            bool filter_exclude = false;
            // auto only_physical = Config::getB("only_physical");
            auto& disks = mem.disks;
            vector<string> filter;
            if (not disks_filter.empty()) {
                filter = ssplit(disks_filter);
                if (filter.at(0).starts_with("exclude=")) {
                    filter_exclude = true;
                    filter.at(0) = filter.at(0).substr(8);
                }
            }

            struct statfs* stfs;
            int count = getmntinfo(&stfs, MNT_WAIT);
            vector<string> found;
            found.reserve(last_found.size());
            for (int i = 0; i < count; i++) {
                std::error_code ec;
                string mountpoint = stfs[i].f_mntonname;
                string dev = stfs[i].f_mntfromname;
                mapping[dev] = mountpoint;

                if (string(stfs[i].f_fstypename) == "autofs") {
                    continue;
                }

                //? Match filter if not empty
                if (not filter.empty()) {
                    bool match = v_contains(filter, mountpoint);
                    if ((filter_exclude and match) or (not filter_exclude and not match))
                        continue;
                }

                found.push_back(mountpoint);
                if (not disks.contains(mountpoint)) {
                    disks[mountpoint] = disk_info {fs::canonical(dev, ec), fs::path(mountpoint).filename()};

                    if (disks.at(mountpoint).dev.empty())
                        disks.at(mountpoint).dev = dev;

                    if (disks.at(mountpoint).name.empty())
                        disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
                }

                if (not v_contains(last_found, mountpoint))
                    redraw = true;

                disks.at(mountpoint).free = stfs[i].f_bfree;
                disks.at(mountpoint).total = stfs[i].f_iosize;
            }

            //? Remove disks no longer mounted or filtered out
            if (swap_disk and has_swap)
                found.push_back("swap");
            for (auto it = disks.begin(); it != disks.end();) {
                if (not v_contains(found, it->first))
                    it = disks.erase(it);
                else
                    it++;
            }
            if (found.size() != last_found.size())
                redraw = true;
            last_found = std::move(found);

            //? Get disk/partition stats
            for (auto& [mountpoint, disk] : disks) {
                if (std::error_code ec; not fs::exists(mountpoint, ec))
                    continue;
                struct statvfs vfs;
                if (statvfs(mountpoint.c_str(), &vfs) < 0) {
                    Logger::warning("Failed to get disk/partition stats with statvfs() for: {}", mountpoint);
                    continue;
                }
                disk.total = vfs.f_blocks * vfs.f_frsize;
                disk.free = vfs.f_bfree * vfs.f_frsize;
                disk.used = disk.total - disk.free;
                if (disk.total != 0) {
                    disk.used_percent = round((double)disk.used * 100 / disk.total);
                    disk.free_percent = 100 - disk.used_percent;
                } else {
                    disk.used_percent = 0;
                    disk.free_percent = 0;
                }
            }

            //? Setup disks order in UI and add swap if enabled
            mem.disks_order.clear();
            if (snapped and disks.contains("/mnt"))
                mem.disks_order.push_back("/mnt");
            else if (disks.contains("/"))
                mem.disks_order.push_back("/");
            if (swap_disk and has_swap) {
                mem.disks_order.push_back("swap");
                if (not disks.contains("swap"))
                    disks["swap"] = {"", "swap"};
                disks.at("swap").total = mem.stats.at("swap_total");
                disks.at("swap").used = mem.stats.at("swap_used");
                disks.at("swap").free = mem.stats.at("swap_free");
                disks.at("swap").used_percent = mem.percent.at("swap_used").back();
                disks.at("swap").free_percent = mem.percent.at("swap_free").back();
            }
            for (const auto& name : last_found)
                if (not is_in(name, "/", "swap", "/dev"))
                    mem.disks_order.push_back(name);

            disk_ios = 0;
            collect_disk(disks, mapping);

            old_uptime = uptime;
        }
        return mem;
    }

} // namespace Mem

namespace Net {
    std::unordered_map<string, net_info> current_net;
    net_info empty_net = {};
    vector<string> interfaces;
    string selected_iface;
    int errors = 0;
    std::unordered_map<string, uint64_t> graph_max = {{"download", {}}, {"upload", {}}};
    std::unordered_map<string, array<int, 2>> max_count = {{"download", {}}, {"upload", {}}};
    bool rescale = true;
    uint64_t timestamp = 0;

    //* RAII wrapper for getifaddrs
    class getifaddr_wrapper {
        struct ifaddrs* ifaddr;

      public:
        int status;
        getifaddr_wrapper() { status = getifaddrs(&ifaddr); }
        ~getifaddr_wrapper() { freeifaddrs(ifaddr); }
        auto operator()() -> struct ifaddrs* { return ifaddr; }
    };

    auto collect(bool no_update) -> net_info& {
        // Lock mutex to prevent concurrent interface access during USB device changes
        std::lock_guard<std::mutex> lock(Mem::interface_mutex);
        auto& net = current_net;
        auto& config_iface = Config::getS("net_iface");
        auto net_sync = Config::getB("net_sync");
        auto net_auto = Config::getB("net_auto");
        auto new_timestamp = time_ms();

        if (not no_update and errors < 3) {
            //? Get interface list using getifaddrs() wrapper
            getifaddr_wrapper if_wrap {};
            if (if_wrap.status != 0) {
                errors++;
                Logger::error("Net::collect() -> getifaddrs() failed with id {}", if_wrap.status);
                redraw = true;
                return empty_net;
            }
            int family = 0;
            static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN); // 46 >= 16, compile-time assurance.
            enum {
                IPBUFFER_MAXSIZE = INET6_ADDRSTRLEN
            }; // manually using the known biggest value, guarded by the above static_assert
            char ip[IPBUFFER_MAXSIZE];
            interfaces.clear();
            string ipv4, ipv6;

            //? Iteration over all items in getifaddrs() list
            for (auto* ifa = if_wrap(); ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr)
                    continue;
                family = ifa->ifa_addr->sa_family;
                const auto& iface = ifa->ifa_name;
                //? Update available interfaces vector and get status of interface
                if (not v_contains(interfaces, iface)) {
                    interfaces.push_back(iface);
                    net[iface].connected = (ifa->ifa_flags & IFF_RUNNING);
                    // An interface can have more than one IP of the same family associated with it,
                    // but we pick only the first one to show in the NET box.
                    // Note: Interfaces without any IPv4 and IPv6 set are still valid and monitorable!
                    net[iface].ipv4.clear();
                    net[iface].ipv6.clear();
                }
                //? Get IPv4 address
                if (family == AF_INET) {
                    if (net[iface].ipv4.empty()) {
                        if (nullptr !=
                            inet_ntop(
                                family, &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr), ip, IPBUFFER_MAXSIZE
                            )) {
                            net[iface].ipv4 = ip;
                        } else {
                            int errsv = errno;
                            Logger::error(
                                "Net::collect() -> Failed to convert IPv4 to string for iface {}, errno: {}",
                                iface,
                                strerror(errsv)
                            );
                        }
                    }
                }
                //? Get IPv6 address
                else if (family == AF_INET6) {
                    if (net[iface].ipv6.empty()) {
                        if (nullptr !=
                            inet_ntop(
                                family, &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr), ip, IPBUFFER_MAXSIZE
                            )) {
                            net[iface].ipv6 = ip;
                        } else {
                            int errsv = errno;
                            Logger::error(
                                "Net::collect() -> Failed to convert IPv6 to string for iface {}, errno: {}",
                                iface,
                                strerror(errsv)
                            );
                        }
                    }
                } // else, ignoring family==AF_LINK (see man 3 getifaddrs)
            }

            std::unordered_map<string, std::tuple<uint64_t, uint64_t>> ifstats;
            int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
            size_t len;
            if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0) {
                Logger::error("failed getting network interfaces");
            } else {
                std::unique_ptr<char[]> buf(new char[len]);
                if (sysctl(mib, 6, buf.get(), &len, nullptr, 0) < 0) {
                    Logger::error("failed getting network interfaces");
                } else {
                    char* lim = buf.get() + len;
                    char* next = nullptr;
                    for (next = buf.get(); next < lim;) {
                        struct if_msghdr* ifm = (struct if_msghdr*)next;
                        next += ifm->ifm_msglen;
                        if (ifm->ifm_type == RTM_IFINFO2) {
                            struct if_msghdr2* if2m = (struct if_msghdr2*)ifm;
                            struct sockaddr_dl* sdl = (struct sockaddr_dl*)(if2m + 1);
                            char iface[32];
                            strncpy(iface, sdl->sdl_data, sdl->sdl_nlen);
                            iface[sdl->sdl_nlen] = 0;
                            ifstats[iface] = std::tuple(if2m->ifm_data.ifi_ibytes, if2m->ifm_data.ifi_obytes);
                        }
                    }
                }
            }

            //? Get total received and transmitted bytes + device address if no ip was found
            for (const auto& iface : interfaces) {
                for (const string dir : {"download", "upload"}) {
                    auto& saved_stat = net.at(iface).stat.at(dir);
                    auto& bandwidth = net.at(iface).bandwidth.at(dir);
                    uint64_t val = dir == "download" ? std::get<0>(ifstats[iface]) : std::get<1>(ifstats[iface]);

                    //? Update speed, total and top values
                    if (val < saved_stat.last) {
                        saved_stat.rollover += saved_stat.last;
                        saved_stat.last = 0;
                    }
                    if (cmp_greater(
                            (unsigned long long)saved_stat.rollover + (unsigned long long)val, numeric_limits<uint64_t>::max()
                        )) {
                        saved_stat.rollover = 0;
                        saved_stat.last = 0;
                    }
                    saved_stat.speed = round((double)(val - saved_stat.last) / ((double)(new_timestamp - timestamp) / 1000));
                    if (saved_stat.speed > saved_stat.top)
                        saved_stat.top = saved_stat.speed;
                    if (saved_stat.offset > val + saved_stat.rollover)
                        saved_stat.offset = 0;
                    saved_stat.total = (val + saved_stat.rollover) - saved_stat.offset;
                    saved_stat.last = val;

                    //? Add values to graph
                    bandwidth.push_back(saved_stat.speed);
                    while (cmp_greater(bandwidth.size(), width * 2))
                        bandwidth.pop_front();

                    //? Set counters for auto scaling
                    if (net_auto and selected_iface == iface) {
                        if (saved_stat.speed > graph_max[dir]) {
                            ++max_count[dir][0];
                            if (max_count[dir][1] > 0)
                                --max_count[dir][1];
                        } else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
                            ++max_count[dir][1];
                            if (max_count[dir][0] > 0)
                                --max_count[dir][0];
                        }
                    }
                }
            }

            //? Clean up net map if needed
            if (net.size() > interfaces.size()) {
                for (auto it = net.begin(); it != net.end();) {
                    if (not v_contains(interfaces, it->first))
                        it = net.erase(it);
                    else
                        it++;
                }
            }

            timestamp = new_timestamp;
        }
        //? Return empty net_info struct if no interfaces was found
        if (net.empty())
            return empty_net;

        //? Find an interface to display if selected isn't set or valid
        if (selected_iface.empty() or not v_contains(interfaces, selected_iface)) {
            max_count["download"][0] = max_count["download"][1] = max_count["upload"][0] = max_count["upload"][1] = 0;
            redraw = true;
            if (net_auto)
                rescale = true;
            if (not config_iface.empty() and v_contains(interfaces, config_iface))
                selected_iface = config_iface;
            else {
                //? Sort interfaces by total upload + download bytes
                auto sorted_interfaces = interfaces;
                rng::sort(sorted_interfaces, [&](const auto& a, const auto& b) {
                    return cmp_greater(
                        net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
                        net.at(b).stat["download"].total + net.at(b).stat["upload"].total
                    );
                });
                selected_iface.clear();
                //? Try to set to a connected interface
                for (const auto& iface : sorted_interfaces) {
                    if (net.at(iface).connected)
                        selected_iface = iface;
                    break;
                }
                //? If no interface is connected set to first available
                if (selected_iface.empty() and not sorted_interfaces.empty())
                    selected_iface = sorted_interfaces.at(0);
                else if (sorted_interfaces.empty())
                    return empty_net;
            }
        }

        //? Calculate max scale for graphs if needed
        if (net_auto) {
            bool sync = false;
            for (const auto& dir : {"download", "upload"}) {
                for (const auto& sel : {0, 1}) {
                    if (rescale or max_count[dir][sel] >= 5) {
                        const long long avg_speed =
                            (net[selected_iface].bandwidth[dir].size() > 5
                                 ? std::accumulate(
                                       net.at(selected_iface).bandwidth.at(dir).rbegin(),
                                       net.at(selected_iface).bandwidth.at(dir).rbegin() + 5,
                                       0ll
                                   ) / 5
                                 : net[selected_iface].stat[dir].speed);
                        graph_max[dir] = max(uint64_t(avg_speed * (sel == 0 ? 1.3 : 3.0)), (uint64_t)10 << 10);
                        max_count[dir][0] = max_count[dir][1] = 0;
                        redraw = true;
                        if (net_sync)
                            sync = true;
                        break;
                    }
                }
                //? Sync download/upload graphs if enabled
                if (sync) {
                    const auto other = (string(dir) == "upload" ? "download" : "upload");
                    graph_max[other] = graph_max[dir];
                    max_count[other][0] = max_count[other][1] = 0;
                    break;
                }
            }
        }

        rescale = false;
        return net.at(selected_iface);
    }
} // namespace Net

namespace Proc {

    vector<proc_info> current_procs;
    std::unordered_map<string, string> uid_user;
    string current_sort;
    string current_filter;
    bool current_rev = false;
    bool is_tree_mode;

    fs::file_time_type passwd_time;

    uint64_t cputimes;
    int collapse = -1, expand = -1, toggle_children = -1;
    uint64_t old_cputimes = 0;
    atomic<int> numpids = 0;
    int filter_found = 0;

    detail_container detailed;
    static std::unordered_set<size_t> dead_procs;

    string get_status(char s) {
        if (s & SRUN)
            return "Running";
        if (s & SSLEEP)
            return "Sleeping";
        if (s & SIDL)
            return "Idle";
        if (s & SSTOP)
            return "Stopped";
        if (s & SZOMB)
            return "Zombie";
        return "Unknown";
    }

    //* Get detailed info for selected process
    void _collect_details(const size_t pid, vector<proc_info>& procs) {
        if (pid != detailed.last_pid) {
            detailed = {};
            detailed.last_pid = pid;
            detailed.skip_smaps = not Config::getB("proc_info_smaps");
        }

        //? Copy proc_info for process from proc vector
        auto p_info = rng::find(procs, pid, &proc_info::pid);
        detailed.entry = *p_info;

        //? Update cpu percent deque for process cpu graph
        if (not Config::getB("proc_per_core"))
            detailed.entry.cpu_p *= Shared::coreCount;
        detailed.cpu_percent.push_back(clamp((long long)round(detailed.entry.cpu_p), 0ll, 100ll));
        while (cmp_greater(detailed.cpu_percent.size(), width))
            detailed.cpu_percent.pop_front();

        //? Process runtime : current time - start time (both in unix time - seconds since epoch)
        struct timeval currentTime;
        gettimeofday(&currentTime, nullptr);
        //? Get elapsed time if process isn't dead
        if (detailed.entry.state != 'X')
            detailed.elapsed = sec_to_dhms(currentTime.tv_sec - (detailed.entry.cpu_s / 1'000'000));
        else
            detailed.elapsed = sec_to_dhms(detailed.entry.death_time);
        if (detailed.elapsed.size() > 8)
            detailed.elapsed.resize(detailed.elapsed.size() - 3);

        //? Get parent process name
        if (detailed.parent.empty()) {
            auto p_entry = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
            if (p_entry != procs.end())
                detailed.parent = p_entry->name;
        }

        //? Expand process status from single char to explanative string
        detailed.status = get_status(detailed.entry.state);

        detailed.mem_bytes.push_back(detailed.entry.mem);
        detailed.memory = floating_humanizer(detailed.entry.mem);

        if (detailed.first_mem == -1 or detailed.first_mem < detailed.mem_bytes.back() / 2 or
            detailed.first_mem > detailed.mem_bytes.back() * 4) {
            detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Mem::get_totalMem());
            redraw = true;
        }

        while (cmp_greater(detailed.mem_bytes.size(), width))
            detailed.mem_bytes.pop_front();

        rusage_info_current rusage;
        if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, (void**)&rusage) == 0) {
            // this fails for processes we don't own - same as in Linux
            detailed.io_read = floating_humanizer(rusage.ri_diskio_bytesread);
            detailed.io_write = floating_humanizer(rusage.ri_diskio_byteswritten);
        }
    }

    //* Collects and sorts process information from /proc
    auto collect(bool no_update) -> vector<proc_info>& {
        const auto& sorting = Config::getS("proc_sorting");
        auto reverse = Config::getB("proc_reversed");
        const auto& filter = Config::getS("proc_filter");
        auto per_core = Config::getB("proc_per_core");
        auto tree = Config::getB("proc_tree");
        auto show_detailed = Config::getB("show_detailed");
        const auto pause_proc_list = Config::getB("pause_proc_list");
        const size_t detailed_pid = Config::getI("detailed_pid");
        bool should_filter = current_filter != filter;
        if (should_filter)
            current_filter = filter;
        bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
        bool tree_mode_change = tree != is_tree_mode;
        if (sorted_change) {
            current_sort = sorting;
            current_rev = reverse;
        }
        if (tree_mode_change)
            is_tree_mode = tree;

        const int cmult = (per_core) ? Shared::coreCount : 1;
        bool got_detailed = false;

        static vector<size_t> found;

        //* Use pids from last update if only changing filter, sorting or tree options
        if (no_update and not current_procs.empty()) {
            if (show_detailed and detailed_pid != detailed.last_pid)
                _collect_details(detailed_pid, current_procs);
        } else {
            //* ---------------------------------------------Collection start----------------------------------------------

            { //* Get CPU totals
                natural_t cpu_count;
                kern_return_t error;
                processor_cpu_load_info_data_t* cpu_load_info = nullptr;
                MachProcessorInfo info {};
                error = host_processor_info(
                    mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info.info_array, &info.info_count
                );
                if (error != KERN_SUCCESS) {
                    Logger::error("Failed getting CPU load info");
                }
                cpu_load_info = (processor_cpu_load_info_data_t*)info.info_array;
                cputimes = 0;
                for (natural_t i = 0; i < cpu_count; i++) {
                    cputimes +=
                        (cpu_load_info[i].cpu_ticks[CPU_STATE_USER] + cpu_load_info[i].cpu_ticks[CPU_STATE_NICE] +
                         cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM] + cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE]);
                }
            }

            should_filter = true;
            int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
            found.clear();
            size_t size = 0;
            const auto timeNow = time_micros();

            if (sysctl(mib, 4, nullptr, &size, nullptr, 0) < 0 || size == 0) {
                Logger::error("Unable to get size of kproc_infos");
            }
            uint64_t cpu_t = 0;

            std::unique_ptr<kinfo_proc[]> processes(new kinfo_proc[size / sizeof(kinfo_proc)]);
            if (sysctl(mib, 4, processes.get(), &size, nullptr, 0) == 0) {
                size_t count = size / sizeof(struct kinfo_proc);
                for (size_t i = 0; i < count; i++) { //* iterate over all processes in kinfo_proc
                    struct kinfo_proc& kproc = processes.get()[i];
                    const size_t pid = (size_t)kproc.kp_proc.p_pid;
                    if (pid < 1)
                        continue;
                    found.push_back(pid);

                    //? Check if pid already exists in current_procs
                    bool no_cache = false;
                    auto find_old = rng::find(current_procs, pid, &proc_info::pid);
                    //? Only add new processes if not paused
                    if (find_old == current_procs.end()) {
                        if (not pause_proc_list) {
                            current_procs.push_back({pid});
                            find_old = current_procs.end() - 1;
                            no_cache = true;
                        } else
                            continue;
                    } else if (dead_procs.contains(pid))
                        continue;

                    auto& new_proc = *find_old;

                    //? Get program name, command, username, parent pid, nice and status
                    if (no_cache) {
                        char fullname[PROC_PIDPATHINFO_MAXSIZE];
                        int rc = proc_pidpath(pid, fullname, sizeof(fullname));
                        string f_name = "<defunct>";
                        if (rc != 0) {
                            f_name = std::string(fullname);
                            size_t lastSlash = f_name.find_last_of('/');
                            f_name = f_name.substr(lastSlash + 1);
                        }
                        new_proc.name = f_name;
                        //? Get process arguments if possible, fallback to process path in case of failure
                        if (Shared::arg_max > 0) {
                            std::unique_ptr<char[]> proc_chars(new char[Shared::arg_max]);
                            int mib[] = {CTL_KERN, KERN_PROCARGS2, (int)pid};
                            size_t argmax = Shared::arg_max;
                            if (sysctl(mib, 3, proc_chars.get(), &argmax, nullptr, 0) == 0) {
                                int argc = 0;
                                memcpy(&argc, &proc_chars.get()[0], sizeof(argc));
                                std::string_view proc_args(proc_chars.get(), argmax);
                                if (size_t null_pos = proc_args.find('\0', sizeof(argc)); null_pos != string::npos) {
                                    if (size_t start_pos = proc_args.find_first_not_of('\0', null_pos);
                                        start_pos != string::npos) {
                                        while (argc-- > 0 and null_pos != string::npos and cmp_less(new_proc.cmd.size(), 1000)) {
                                            null_pos = proc_args.find('\0', start_pos);
                                            new_proc.cmd += (string)proc_args.substr(start_pos, null_pos - start_pos) + ' ';
                                            start_pos = null_pos + 1;
                                        }
                                    }
                                }
                                if (not new_proc.cmd.empty())
                                    new_proc.cmd.pop_back();
                            }
                        }
                        if (new_proc.cmd.empty())
                            new_proc.cmd = f_name;
                        if (new_proc.cmd.size() > 1000) {
                            new_proc.cmd.resize(1000);
                            new_proc.cmd.shrink_to_fit();
                        }
                        new_proc.ppid = kproc.kp_eproc.e_ppid;
                        new_proc.cpu_s = kproc.kp_proc.p_starttime.tv_sec * 1'000'000 + kproc.kp_proc.p_starttime.tv_usec;
                        struct passwd* pwd = getpwuid(kproc.kp_eproc.e_ucred.cr_uid);
                        if (pwd != nullptr) {
                            new_proc.user = pwd->pw_name;
                        } else {
                            new_proc.user = std::to_string(kproc.kp_eproc.e_ucred.cr_uid);
                        }
                    }
                    new_proc.p_nice = kproc.kp_proc.p_nice;
                    new_proc.state = kproc.kp_proc.p_stat;

                    //? Get threads, mem and cpu usage
                    struct proc_taskinfo pti {};
                    if (sizeof(pti) == proc_pidinfo(new_proc.pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti))) {
                        new_proc.threads = pti.pti_threadnum;
                        new_proc.mem = pti.pti_resident_size;
                        cpu_t = pti.pti_total_user + pti.pti_total_system;

                        if (new_proc.cpu_t == 0)
                            new_proc.cpu_t = cpu_t;
                    } else {
                        // Reset memory value if process info cannot be accessed (bad permissions or zombie processes)
                        new_proc.threads = 0;
                        new_proc.mem = 0;
                        cpu_t = new_proc.cpu_t;
                    }

                    //? Process cpu usage since last update
                    new_proc.cpu_p = clamp(
                        round(((cpu_t - new_proc.cpu_t) * Shared::machTck) / ((cputimes - old_cputimes) * Shared::clkTck)) *
                            cmult / 1000.0,
                        0.0,
                        100.0 * Shared::coreCount
                    );

                    //? Process cumulative cpu usage since process start
                    new_proc.cpu_c = (double)(cpu_t * Shared::machTck) / (timeNow - new_proc.cpu_s);

                    //? Update cached value with latest cpu times
                    new_proc.cpu_t = cpu_t;

                    if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
                        got_detailed = true;
                    }
                }

                //? Clear dead processes from current_procs if not paused
                if (not pause_proc_list) {
                    auto eraser =
                        rng::remove_if(current_procs, [&](const auto& element) { return not v_contains(found, element.pid); });
                    current_procs.erase(eraser.begin(), eraser.end());
                    if (!dead_procs.empty())
                        dead_procs.clear();
                }
                //? Set correct state of dead processes if paused
                else {
                    const bool keep_dead_proc_usage = Config::getB("keep_dead_proc_usage");
                    for (auto& r : current_procs) {
                        if (rng::find(found, r.pid) == found.end()) {
                            if (r.state != 'X') {
                                struct timeval currentTime;
                                gettimeofday(&currentTime, nullptr);
                                r.death_time = currentTime.tv_sec - (r.cpu_s / 1'000'000);
                            }
                            r.state = 'X';
                            dead_procs.emplace(r.pid);
                            //? Reset cpu usage for dead processes if paused and option is set
                            if (!keep_dead_proc_usage) {
                                r.cpu_p = 0.0;
                                r.mem = 0;
                            }
                        }
                    }
                }

                //? Update the details info box for process if active
                if (show_detailed and got_detailed) {
                    _collect_details(detailed_pid, current_procs);
                } else if (show_detailed and not got_detailed and detailed.status != "Dead") {
                    detailed.status = "Dead";
                    redraw = true;
                }

                old_cputimes = cputimes;
            }
        }

        //* ---------------------------------------------Collection done-----------------------------------------------

        //* Match filter if defined
        if (should_filter) {
            filter_found = 0;
            for (auto& p : current_procs) {
                if (not tree and not filter.empty()) {
                    if (!matches_filter(p, filter)) {
                        p.filtered = true;
                        filter_found++;
                    } else {
                        p.filtered = false;
                    }
                } else {
                    p.filtered = false;
                }
            }
        }

        //* Sort processes
        if ((sorted_change or tree_mode_change) or (not no_update and not pause_proc_list)) {
            proc_sorter(current_procs, sorting, reverse, tree);
        }

        //* Generate tree view if enabled
        if (tree and (not no_update or should_filter or sorted_change)) {
            bool locate_selection = false;

            if (toggle_children != -1) {
                auto collapser = rng::find(current_procs, toggle_children, &proc_info::pid);
                if (collapser != current_procs.end()) {
                    for (auto& p : current_procs) {
                        if (p.ppid == collapser->pid) {
                            auto child = rng::find(current_procs, p.pid, &proc_info::pid);
                            if (child != current_procs.end()) {
                                child->collapsed = not child->collapsed;
                            }
                        }
                    }
                    if (Config::ints.at("proc_selected") > 0)
                        locate_selection = true;
                }
                toggle_children = -1;
            }

            if (auto find_pid = (collapse != -1 ? collapse : expand); find_pid != -1) {
                auto collapser = rng::find(current_procs, find_pid, &proc_info::pid);
                if (collapser != current_procs.end()) {
                    if (collapse == expand) {
                        collapser->collapsed = not collapser->collapsed;
                    } else if (collapse > -1) {
                        collapser->collapsed = true;
                    } else if (expand > -1) {
                        collapser->collapsed = false;
                    }
                    if (Config::ints.at("proc_selected") > 0)
                        locate_selection = true;
                }
                collapse = expand = -1;
            }
            if (should_filter or not filter.empty())
                filter_found = 0;

            vector<tree_proc> tree_procs;
            tree_procs.reserve(current_procs.size());

            if (!pause_proc_list) {
                for (auto& p : current_procs) {
                    if (not v_contains(found, p.ppid))
                        p.ppid = 0;
                }
            }

            //? Stable sort to retain selected sorting among processes with the same parent
            rng::stable_sort(current_procs, rng::less {}, &proc_info::ppid);

            //? Start recursive iteration over processes with the lowest shared parent pids
            for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less {}, &proc_info::ppid)) {
                _tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
            }

            //? Recursive sort over tree structure to account for collapsed processes in the tree
            int index = 0;
            tree_sort(
                tree_procs,
                sorting,
                reverse,
                (pause_proc_list and not(sorted_change or tree_mode_change)),
                index,
                current_procs.size()
            );

            //? Recursive construction of ASCII tree prefixes.
            for (auto t = tree_procs.begin(); t != tree_procs.end(); ++t) {
                _collect_prefixes(*t, t == tree_procs.end() - 1);
            }

            //? Final sort based on tree index
            rng::stable_sort(current_procs, rng::less {}, &proc_info::tree_index);

            //? Move current selection/view to the selected process when collapsing/expanding in the tree
            if (locate_selection) {
                int loc = rng::find(current_procs, Proc::selected_pid, &proc_info::pid)->tree_index;
                if (Config::ints.at("proc_start") >= loc or Config::ints.at("proc_start") <= loc - Proc::select_max)
                    Config::ints.at("proc_start") = max(0, loc - 1);
                Config::ints.at("proc_selected") = loc - Config::ints.at("proc_start") + 1;
            }
        }

        numpids = (int)current_procs.size() - filter_found;
        return current_procs;
    }
} // namespace Proc

namespace Tools {
    double system_uptime() {
        struct timeval ts, currTime;
        std::size_t len = sizeof(ts);
        int mib[2] = {CTL_KERN, KERN_BOOTTIME};
        if (sysctl(mib, 2, &ts, &len, nullptr, 0) != -1) {
            gettimeofday(&currTime, nullptr);
            return currTime.tv_sec - ts.tv_sec;
        }
        return 0.0;
    }
} // namespace Tools

namespace Gpu {

#ifdef GPU_SUPPORT
    namespace IOAccelerator {
        //? Initialize Apple Silicon GPU monitoring Although the chips always have 1 GPU, I assume we can reuse them later on
        // Intel Macs.
        bool init() {
            const auto index = gpus.size();
            auto& io_gpus = io_gpu.get_gpus();
            device_count += io_gpus.size();

            if (io_gpus.empty()) {
                return false;
            }

            for (size_t i = 0; i < io_gpus.size(); i++) {
                gpus.emplace_back();
                auto name = fmt::format("{} ({})", io_gpus[i].get_name(), io_gpus[i].get_core_count());
                gpu_names.emplace_back(name);
                collect<true>(&gpus[index], i);
            }
            return true;
        }

        //? Shutdown Apple Silicon GPU monitoring
        bool shutdown() {
            return true;
        }

        //? Collect GPU metrics into the provided slice
        template<bool is_init>
        bool collect(gpu_info* gpus_slice, size_t index) {
            if (not initialized) {
                return false;
            }

            auto& io_gpus = io_gpu.get_gpus();
            if (io_gpus.empty() || index >= io_gpus.size()) {
                return false;
            }

            if constexpr (is_init) {
                gpus_slice->supported_functions = {
                    .gpu_utilization = true,
                    .mem_utilization = false,
                    .gpu_clock = IOReport::lib_handle ? true : false,
                    .mem_clock = false,
                    .pwr_usage = IOReport::lib_handle ? true : false,
                    .pwr_state = false,
                    .temp_info = false, // IOReport::LibHandle ? true : false
                    .mem_total = true,
                    .mem_used = true,
                    .pcie_txrx = false,
                    .encoder_utilization = false,
                    .decoder_utilization = false
                };
                gpus_slice->pwr_max_usage = 30'000; //? 30w
            }

            io_gpus[index].refresh();
            auto gpu_data = io_gpus[index].get_statistics();

            gpus_slice->gpu_percent.at("gpu-totals").push_back(gpu_data.device_utilization);

            gpus_slice->mem_total = gpu_data.alloc_system_memory;
            gpus_slice->mem_used = gpu_data.in_use_system_memory;
            auto mem_percent = 0LL;
            if (gpus_slice->mem_total > 0) {
                mem_percent = static_cast<long long>(
                    (static_cast<double>(gpus_slice->mem_used) / static_cast<double>(gpus_slice->mem_total)) * 100.0
                );
            }
            gpus_slice->gpu_percent.at("gpu-vram-totals").push_back(mem_percent);
            // gpus_slice->mem_utilization_percent.push_back(mem_percent);

            if (IOReport::lib_handle) {
                gpus_slice->gpu_percent.at("gpu-pwr-totals").push_back(gpu_data.milliwatts);
                gpus_slice->pwr_usage = gpu_data.milliwatts;
                gpus_slice->gpu_clock_speed = gpu_data.gpu_frequency / 1'000'000;
                // gpus_slice->temp.push_back( gpu_data.temp_c);
            }

            return true;
        }

    } // namespace IOAccelerator

    //? Full based (copied) on linux (intel/amd) gpu collect
    auto collect(bool no_update) -> std::vector<gpu_info>& {
        if (Runner::stopping || (no_update && !gpus.empty()))
            return gpus;

        IOAccelerator::collect<false>(gpus.data());

        //* Calculate average metrics
        long long avg = 0;
        long long mem_usage_total = 0;
        long long mem_total = 0;
        long long pwr_total = 0;

        for (auto& gpu : gpus) {
            if (gpu.supported_functions.gpu_utilization)
                avg += gpu.gpu_percent.at("gpu-totals").back();
            if (gpu.supported_functions.mem_used)
                mem_usage_total += gpu.mem_used;
            if (gpu.supported_functions.mem_total)
                mem_total += gpu.mem_total;
            if (gpu.supported_functions.pwr_usage)
                pwr_total += gpu.pwr_usage;

            //* Trim vectors if they exceed graph width
            if (width != 0) {
                while (cmp_greater(gpu.gpu_percent.at("gpu-totals").size(), width * 2))
                    gpu.gpu_percent.at("gpu-totals").pop_front();
                while (cmp_greater(gpu.mem_utilization_percent.size(), width))
                    gpu.mem_utilization_percent.pop_front();
                while (cmp_greater(gpu.gpu_percent.at("gpu-pwr-totals").size(), width))
                    gpu.gpu_percent.at("gpu-pwr-totals").pop_front();
                while (cmp_greater(gpu.temp.size(), 18))
                    gpu.temp.pop_front();
                while (cmp_greater(gpu.gpu_percent.at("gpu-vram-totals").size(), width / 2))
                    gpu.gpu_percent.at("gpu-vram-totals").pop_front();
            }
        }

        // Update shared GPU metrics
        shared_gpu_percent.at("gpu-average").push_back(avg / gpus.size());
        if (mem_total != 0)
            shared_gpu_percent.at("gpu-vram-total").push_back(mem_usage_total / mem_total);
        if (gpu_pwr_total_max != 0)
            shared_gpu_percent.at("gpu-pwr-total").push_back(pwr_total / gpu_pwr_total_max);

        if (width != 0) {
            while (cmp_greater(shared_gpu_percent.at("gpu-average").size(), width * 2))
                shared_gpu_percent.at("gpu-average").pop_front();
            while (cmp_greater(shared_gpu_percent.at("gpu-pwr-total").size(), width * 2))
                shared_gpu_percent.at("gpu-pwr-total").pop_front();
            while (cmp_greater(shared_gpu_percent.at("gpu-vram-total").size(), width * 2))
                shared_gpu_percent.at("gpu-vram-total").pop_front();
        }

        count = gpus.size();
        return gpus;
    }

#endif // GPU_SUPPORT

} // namespace Gpu
