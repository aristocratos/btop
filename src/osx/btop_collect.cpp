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
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <arpa/inet.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <mach/mach_time.h>
// BUGS
//     If both <net/if.h> and <ifaddrs.h> are being included, <net/if.h> must be
//     included before <ifaddrs.h>.
// from: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/getifaddrs.3.html
#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <netdb.h>
#include <netinet/tcp_fsm.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <netinet/in.h> // for inet_ntop
#include <unistd.h>
#include <stdexcept>
#include <utility>

#include <cmath>
#include <fstream>
#include <mutex>
#include <numeric>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include "../btop_config.hpp"
#include "../btop_log.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
#include "sensors.hpp"
#endif
#include "smc.hpp"

#if defined(GPU_SUPPORT) && __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
#include "apple_silicon_gpu.hpp"
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

//? Get available disk space including purgeable space (macOS-specific)
//? Returns available bytes for "important" usage, which includes space that can be freed by purging
//? Falls back to statvfs if the API is unavailable
static int64_t get_avail_with_purgeable(const char* path) {
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8*)path, strlen(path), true);
	if (url == nullptr) return -1;

	CFNumberRef availCapacity = nullptr;
	// Try to get "available capacity for important usage" (includes purgeable space)
	// This key is available on macOS 10.13+
	Boolean success = CFURLCopyResourcePropertyForKey(url,
		kCFURLVolumeAvailableCapacityForImportantUsageKey, &availCapacity, nullptr);

	int64_t result = -1;
	if (success && availCapacity != nullptr) {
		CFNumberGetValue(availCapacity, kCFNumberSInt64Type, &result);
		CFRelease(availCapacity);
	}
	CFRelease(url);
	return result;
}

//? Get actual volume name from macOS using CFURL
static std::string get_volume_name(const char* path) {
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8*)path, strlen(path), true);
	if (url == nullptr) return "";

	CFStringRef nameRef = nullptr;
	Boolean success = CFURLCopyResourcePropertyForKey(url, kCFURLVolumeNameKey, &nameRef, nullptr);

	std::string result;
	if (success && nameRef != nullptr) {
		char buffer[256];
		if (CFStringGetCString(nameRef, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
			result = buffer;
		}
		CFRelease(nameRef);
	}
	CFRelease(url);
	return result;
}

//? Get disk connection type (USB, Thunderbolt, Internal, etc.) using IOKit
//? Returns short identifier like "USB", "TB3", "TB4", "SATA" or empty string if unknown
static std::string get_disk_type(const char* bsd_name) {
	// Extract base disk name (e.g., "disk7" from "/dev/disk7s1" or "disk7s1")
	std::string name = bsd_name;
	if (name.starts_with("/dev/")) name = name.substr(5);
	// Remove slice/partition suffix to get base disk
	size_t spos = name.find('s', 4);  // Find 's' after "disk"
	if (spos != std::string::npos) name = name.substr(0, spos);

	CFMutableDictionaryRef matching = IOBSDNameMatching(kIOMainPortDefault, 0, name.c_str());
	if (matching == nullptr) return "";

	io_service_t disk = IOServiceGetMatchingService(kIOMainPortDefault, matching);
	if (disk == IO_OBJECT_NULL) return "";

	// Walk up the IOKit tree to find the transport/protocol
	std::string result;
	bool isThunderbolt = false;
	int tbGeneration = 0;
	io_service_t current = disk;

	for (int depth = 0; depth < 20 && current != IO_OBJECT_NULL; depth++) {
		bool isExternal = false;
		std::string interconnect;
		bool foundConnectionInfo = false;

		// Check "Protocol Characteristics" property for connection info (some controllers)
		CFTypeRef protocolRef = IORegistryEntryCreateCFProperty(current,
			CFSTR("Protocol Characteristics"), kCFAllocatorDefault, 0);

		if (protocolRef != nullptr && CFGetTypeID(protocolRef) == CFDictionaryGetTypeID()) {
			CFDictionaryRef protocolDict = (CFDictionaryRef)protocolRef;

			// Check Physical Interconnect Location (Internal vs External)
			CFStringRef locationRef = (CFStringRef)CFDictionaryGetValue(protocolDict,
				CFSTR("Physical Interconnect Location"));
			CFStringRef interconnectRef = (CFStringRef)CFDictionaryGetValue(protocolDict,
				CFSTR("Physical Interconnect"));

			if (locationRef != nullptr && CFGetTypeID(locationRef) == CFStringGetTypeID()) {
				char locBuffer[64];
				if (CFStringGetCString(locationRef, locBuffer, sizeof(locBuffer), kCFStringEncodingUTF8)) {
					isExternal = (std::string(locBuffer) == "External");
					foundConnectionInfo = true;
				}
			}

			if (interconnectRef != nullptr && CFGetTypeID(interconnectRef) == CFStringGetTypeID()) {
				char intBuffer[64];
				if (CFStringGetCString(interconnectRef, intBuffer, sizeof(intBuffer), kCFStringEncodingUTF8)) {
					interconnect = intBuffer;
				}
			}

			CFRelease(protocolRef);
		} else if (protocolRef != nullptr) {
			CFRelease(protocolRef);
		}

		// Also check for direct properties (IONVMeController uses these instead of Protocol Characteristics)
		if (!foundConnectionInfo) {
			CFStringRef directLocationRef = (CFStringRef)IORegistryEntryCreateCFProperty(current,
				CFSTR("Physical Interconnect Location"), kCFAllocatorDefault, 0);
			CFStringRef directInterconnectRef = (CFStringRef)IORegistryEntryCreateCFProperty(current,
				CFSTR("Physical Interconnect"), kCFAllocatorDefault, 0);

			if (directLocationRef != nullptr && CFGetTypeID(directLocationRef) == CFStringGetTypeID()) {
				char locBuffer[64];
				if (CFStringGetCString(directLocationRef, locBuffer, sizeof(locBuffer), kCFStringEncodingUTF8)) {
					isExternal = (std::string(locBuffer) == "External");
					foundConnectionInfo = true;
				}
			}

			if (directInterconnectRef != nullptr && CFGetTypeID(directInterconnectRef) == CFStringGetTypeID()) {
				char intBuffer[64];
				if (CFStringGetCString(directInterconnectRef, intBuffer, sizeof(intBuffer), kCFStringEncodingUTF8)) {
					interconnect = intBuffer;
				}
			}

			if (directLocationRef != nullptr) CFRelease(directLocationRef);
			if (directInterconnectRef != nullptr) CFRelease(directInterconnectRef);
		}

		// Process connection info if found
		if (isExternal) {
			// Map interconnect type to short label
			if (interconnect == "USB") {
				result = "USB";
				break;
			} else if (interconnect == "PCI-Express" || interconnect == "Thunderbolt") {
				isThunderbolt = true;  // Mark as TB, continue to find generation
			} else if (interconnect == "SATA") {
				result = "SATA";
				break;
			} else if (!interconnect.empty()) {
				result = "EXT";  // Generic external
				break;
			}
		}

		// Check for Thunderbolt controller to get generation
		io_name_t className;
		IOObjectGetClass(current, className);
		std::string classStr = className;

		// IOThunderboltController has "Generation" property
		if (classStr.find("IOThunderboltController") != std::string::npos) {
			CFNumberRef genRef = (CFNumberRef)IORegistryEntryCreateCFProperty(current,
				CFSTR("Generation"), kCFAllocatorDefault, 0);
			if (genRef != nullptr) {
				CFNumberGetValue(genRef, kCFNumberIntType, &tbGeneration);
				CFRelease(genRef);
			}
			if (isThunderbolt) break;  // Found generation, done
		}

		// Check for disk image (DMG, ISO, IMG) - AppleDiskImageDevice
		if (classStr.find("DiskImage") != std::string::npos) {
			// Try to get the source file URL to determine image type
			CFStringRef urlRef = (CFStringRef)IORegistryEntryCreateCFProperty(current,
				CFSTR("DiskImageURL"), kCFAllocatorDefault, 0);
			if (urlRef != nullptr && CFGetTypeID(urlRef) == CFStringGetTypeID()) {
				char urlBuffer[512];
				if (CFStringGetCString(urlRef, urlBuffer, sizeof(urlBuffer), kCFStringEncodingUTF8)) {
					std::string url = urlBuffer;
					// Get file extension (case-insensitive)
					size_t dotPos = url.rfind('.');
					if (dotPos != std::string::npos) {
						std::string ext = url.substr(dotPos + 1);
						// Convert to uppercase for comparison
						for (auto& c : ext) c = toupper(c);
						if (ext == "ISO")
							result = "ISO";
						else if (ext == "IMG")
							result = "IMG";
						else
							result = "DMG";  // Default for .dmg and other disk images
					} else {
						result = "DMG";
					}
				} else {
					result = "DMG";
				}
				CFRelease(urlRef);
			} else {
				result = "DMG";
				if (urlRef != nullptr) CFRelease(urlRef);
			}
			break;
		}

		// Also check class names as fallback for USB devices
		if (classStr.find("USB") != std::string::npos &&
			classStr.find("USBHostDevice") == std::string::npos &&
			classStr.find("Thunderbolt") == std::string::npos) {
			// Found USB in tree (but not USB in Thunderbolt context)
			result = "USB";
			break;
		}

		// Move to parent
		io_service_t next = IO_OBJECT_NULL;
		if (IORegistryEntryGetParentEntry(current, kIOServicePlane, &next) != KERN_SUCCESS) {
			break;
		}
		if (current != disk) IOObjectRelease(current);
		current = next;
	}

	if (current != disk && current != IO_OBJECT_NULL) IOObjectRelease(current);
	IOObjectRelease(disk);

	// Build final result for Thunderbolt with generation
	if (isThunderbolt) {
		// If we didn't find TB generation in parent chain, search globally
		// Thunderbolt controller may be in a different branch of the IO tree
		if (tbGeneration == 0) {
			io_iterator_t iter = IO_OBJECT_NULL;
			CFMutableDictionaryRef tbMatching = IOServiceMatching("IOThunderboltController");
			if (tbMatching != nullptr) {
				if (IOServiceGetMatchingServices(kIOMainPortDefault, tbMatching, &iter) == KERN_SUCCESS && iter != IO_OBJECT_NULL) {
					io_service_t tbController;
					while ((tbController = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
						CFNumberRef genRef = (CFNumberRef)IORegistryEntryCreateCFProperty(tbController,
							CFSTR("Generation"), kCFAllocatorDefault, 0);
						if (genRef != nullptr) {
							CFNumberGetValue(genRef, kCFNumberIntType, &tbGeneration);
							CFRelease(genRef);
						}
						IOObjectRelease(tbController);
						if (tbGeneration > 0) break;  // Found generation, done
					}
					IOObjectRelease(iter);
				}
			}
		}

		if (tbGeneration >= 1 && tbGeneration <= 5) {
			result = "TB" + std::to_string(tbGeneration);
		} else {
			result = "TB";  // Fallback if generation unknown
		}
	}

	return result;
}

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

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
}  // namespace Cpu

namespace Mem {
	double old_uptime;
}

#ifdef GPU_SUPPORT
namespace Gpu {
	vector<gpu_info> gpus;

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
	namespace AppleSilicon {
		bool initialized = false;
		unsigned int device_count = 0;

		bool init() {
			if (initialized) return false;

			if (not apple_silicon_gpu.init()) {
				Logger::debug("Apple Silicon GPU not available");
				return false;
			}

			device_count = 1;
			gpus.resize(gpus.size() + device_count);
			gpu_names.push_back(apple_silicon_gpu.get_name());

			//? Set supported functions for Apple Silicon GPU
			gpu_info& gpu = gpus.back();
			gpu.supported_functions = {
				.gpu_utilization = true,
				.mem_utilization = false,  // Apple Silicon uses unified memory
				.gpu_clock = true,
				.mem_clock = false,
				.pwr_usage = true,
				.pwr_state = false,
				.temp_info = true,
				.mem_total = true,   // Unified memory (recommendedMaxWorkingSetSize)
				.mem_used = true,    // Unified memory (from IORegistry AGXAccelerator)
				.pcie_txrx = false,  // No PCIe on Apple Silicon
				.encoder_utilization = false,
				.decoder_utilization = false
			};

			//? Start with low max power - will auto-scale based on observed usage
			//? This ensures the braille graph shows meaningful data from the start
			gpu.pwr_max_usage = 1000;  // Start at 1W, auto-scales up as higher values observed

			initialized = true;

			//? Do initial collection
			collect();

			return true;
		}

		bool shutdown() {
			if (not initialized) return false;
			apple_silicon_gpu.shutdown();
			initialized = false;
			return true;
		}

		bool collect() {
			if (not initialized) return false;

			auto metrics = apple_silicon_gpu.collect();
			gpu_info& gpu = gpus[0];

			//? GPU utilization
			if (gpu.supported_functions.gpu_utilization) {
				gpu.gpu_percent.at("gpu-totals").push_back(static_cast<long long>(round(metrics.gpu_usage_percent)));
			}

			//? GPU clock speed
			if (gpu.supported_functions.gpu_clock) {
				gpu.gpu_clock_speed = static_cast<unsigned int>(round(metrics.gpu_freq_mhz));
			}

			//? Power usage
			if (gpu.supported_functions.pwr_usage) {
				gpu.pwr_usage = static_cast<long long>(round(metrics.gpu_power_watts * 1000));  // Convert W to mW
				gpu.pwr.push_back(gpu.pwr_usage);  //? Store for braille graph (auto-scales)
				if (gpu.pwr_usage > gpu.pwr_max_usage) {
					gpu.pwr_max_usage = gpu.pwr_usage;
				}
				if (gpu.pwr_max_usage > 0) {
					gpu.gpu_percent.at("gpu-pwr-totals").push_back(
						clamp(static_cast<long long>(round(static_cast<double>(gpu.pwr_usage) * 100.0 / static_cast<double>(gpu.pwr_max_usage))), 0ll, 100ll)
					);
				}
			}

			//? Temperature
			if (gpu.supported_functions.temp_info and Config::getB("check_temp")) {
				if (metrics.gpu_temp_celsius > 0) {
					gpu.temp.push_back(static_cast<long long>(round(metrics.gpu_temp_celsius)));
				}
			}

			//? Unified Memory (VRAM equivalent for Apple Silicon)
			if (gpu.supported_functions.mem_total and gpu.supported_functions.mem_used) {
				long long mem_used = Shared::gpuMemUsed.load(std::memory_order_acquire);
				long long mem_total = Shared::gpuMemTotal.load(std::memory_order_acquire);
				if (mem_total > 0) {
					gpu.mem_total = mem_total;
					gpu.mem_used = mem_used;
					long long mem_percent = (mem_used * 100) / mem_total;
					gpu.gpu_percent.at("gpu-vram-totals").push_back(clamp(mem_percent, 0ll, 100ll));
				}
			}

			return true;
		}
	}
#endif

	auto collect(bool no_update) -> vector<gpu_info>& {
		if (Runner::stopping or (no_update and not gpus.empty())) return gpus;

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
		AppleSilicon::collect();
#endif

		//* Calculate average usage and trim vectors
		if (not gpus.empty()) {
			long long avg = 0;
			for (auto& gpu : gpus) {
				if (gpu.supported_functions.gpu_utilization and not gpu.gpu_percent.at("gpu-totals").empty()) {
					avg += gpu.gpu_percent.at("gpu-totals").back();
				}

				//* Trim vectors if there are more values than needed for graphs
				if (width != 0) {
					while (cmp_greater(gpu.gpu_percent.at("gpu-totals").size(), width * 2)) gpu.gpu_percent.at("gpu-totals").pop_front();
					while (cmp_greater(gpu.gpu_percent.at("gpu-pwr-totals").size(), width)) gpu.gpu_percent.at("gpu-pwr-totals").pop_front();
					while (cmp_greater(gpu.temp.size(), 18)) gpu.temp.pop_front();
					while (cmp_greater(gpu.pwr.size(), 18)) gpu.pwr.pop_front();  //? Trim power history
					while (cmp_greater(gpu.gpu_percent.at("gpu-vram-totals").size(), width/2)) gpu.gpu_percent.at("gpu-vram-totals").pop_front();
				}
			}

			shared_gpu_percent.at("gpu-average").push_back(gpus.empty() ? 0 : avg / static_cast<long long>(gpus.size()));

			if (width != 0) {
				//? Cache map references to avoid repeated lookups in while loops
				auto& gpu_avg = shared_gpu_percent.at("gpu-average");
				auto& gpu_pwr = shared_gpu_percent.at("gpu-pwr-total");
				auto& gpu_vram = shared_gpu_percent.at("gpu-vram-total");
				while (cmp_greater(gpu_avg.size(), width * 2)) gpu_avg.pop_front();
				while (cmp_greater(gpu_pwr.size(), width * 2)) gpu_pwr.pop_front();
				while (cmp_greater(gpu_vram.size(), width * 2)) gpu_vram.pop_front();
			}

			//? Update ANE activity history for Apple Silicon split graph (key "6")
			if (Shared::aneCoreCount > 0) {
				// Convert ANE activity (C/s) to percentage (0-100), dynamic max
				double ane_max = std::max(1.0, Shared::aneActivityPeak.load(std::memory_order_acquire));
				long long ane_percent = static_cast<long long>(std::min(100.0, (Shared::aneActivity / ane_max) * 100.0));
				auto& ane_activity = shared_gpu_percent.at("ane-activity");
				ane_activity.push_back(ane_percent);
				if (width != 0) {
					while (cmp_greater(ane_activity.size(), width * 2)) ane_activity.pop_front();
				}
			}
		}

		count = gpus.size();
		return gpus;
	}

	//? Stub implementations for Nvml, Rsmi, and Intel (not available on macOS)
	namespace Nvml {
		bool shutdown() { return false; }
	}
	namespace Rsmi {
		bool shutdown() { return false; }
	}
	namespace Intel {
		bool shutdown() { return false; }
	}
}  // namespace Gpu
#endif

	class MachProcessorInfo {
	public:
		processor_info_array_t info_array = nullptr;
		mach_msg_type_number_t info_count = 0;
		MachProcessorInfo() = default;
		virtual ~MachProcessorInfo() {
			if (info_array != nullptr) {
				vm_deallocate(mach_task_self(), (vm_address_t)info_array, (vm_size_t)sizeof(processor_info_array_t) * info_count);
			}
		}
	};

namespace Shared {

	fs::path passwd_path;
	uint64_t totalMem;
	long pageSize, coreCount, clkTck, physicalCoreCount, arg_max;
	long eCoreCount = 0, pCoreCount = 0;  // Apple Silicon E-core/P-core counts
	long gpuCoreCount = 0;  // Apple Silicon GPU core count
	long aneCoreCount = 0;  // Apple Silicon ANE core count

	// Apple Silicon power metrics (atomic for thread-safety)
	atomic<double> cpuPower{0}, gpuPower{0}, anePower{0};
	atomic<double> cpuPowerAvg{0}, gpuPowerAvg{0}, anePowerAvg{0};
	atomic<double> cpuPowerPeak{0}, gpuPowerPeak{0}, anePowerPeak{0};

	// Apple Silicon ANE activity (commands per second, atomic for thread-safety)
	atomic<double> aneActivity{0};
	atomic<double> aneActivityPeak{1};  // Start at 1 to avoid division by zero

	// Shared temperature values for Pwr panel (atomic for thread-safety)
	atomic<long long> cpuTemp{0}, gpuTemp{0};

	// Shared fan RPM values for Pwr panel (atomic for thread-safety)
	atomic<long long> fanRpm{0};
	atomic<int> fanCount{0};

	// GPU VRAM/Unified Memory usage (atomic for thread-safety)
	// For Apple Silicon: GPU's share of unified memory from IORegistry AGXAccelerator
	atomic<long long> gpuMemUsed{0};   // Currently in-use GPU memory (bytes)
	atomic<long long> gpuMemTotal{0};  // Maximum/recommended GPU memory (bytes)

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

		//? Detect Apple Silicon E-cores and P-cores
		int nperflevels = 0;
		size_t nperflevels_size = sizeof(nperflevels);
		if (sysctlbyname("hw.nperflevels", &nperflevels, &nperflevels_size, nullptr, 0) == 0 && nperflevels >= 2) {
			//? Apple Silicon detected with multiple performance levels
			//? perflevel0 = P-cores (performance), perflevel1 = E-cores (efficiency)
			int p_cores = 0, e_cores = 0;
			size_t core_size = sizeof(p_cores);
			if (sysctlbyname("hw.perflevel0.logicalcpu", &p_cores, &core_size, nullptr, 0) == 0) {
				pCoreCount = p_cores;
			}
			core_size = sizeof(e_cores);
			if (sysctlbyname("hw.perflevel1.logicalcpu", &e_cores, &core_size, nullptr, 0) == 0) {
				eCoreCount = e_cores;
			}
			Logger::info("Apple Silicon detected: {} E-cores, {} P-cores", eCoreCount, pCoreCount);
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
			Logger::warning("Could not get mach clock tick conversion factor. Defaulting to 100, processes cpu usage might be incorrect.");
			machTck = 100;
		}

		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0) {
			clkTck = 100;
			Logger::warning("Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect.");
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
		for (auto &[field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty() and not v_contains(Cpu::available_fields, field)) Cpu::available_fields.push_back(field);
		}
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();
		Cpu::core_mapping = Cpu::get_core_mapping();

#ifdef GPU_SUPPORT
		//? Init for namespace Gpu
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
		Gpu::AppleSilicon::init();

		if (not Gpu::gpu_names.empty()) {
			for (auto const& [key, _] : Gpu::gpus[0].gpu_percent)
				Cpu::available_fields.push_back(key);
			for (auto const& [key, _] : Gpu::shared_gpu_percent)
				Cpu::available_fields.push_back(key);

			Gpu::count = Gpu::gpus.size();
			Gpu::gpu_b_height_offsets.resize(Gpu::gpus.size());
			for (size_t i = 0; i < Gpu::gpu_b_height_offsets.size(); ++i)
				Gpu::gpu_b_height_offsets[i] = Gpu::gpus[i].supported_functions.gpu_utilization
					+ Gpu::gpus[i].supported_functions.pwr_usage
					+ (Gpu::gpus[i].supported_functions.encoder_utilization or Gpu::gpus[i].supported_functions.decoder_utilization)
					+ (Gpu::gpus[i].supported_functions.mem_total or Gpu::gpus[i].supported_functions.mem_used)
					* (1 + 2*(Gpu::gpus[i].supported_functions.mem_total and Gpu::gpus[i].supported_functions.mem_used) + 2*Gpu::gpus[i].supported_functions.mem_utilization)
					+ (Shared::aneCoreCount > 0 ? 1 : 0);  //? Add row for ANE on Apple Silicon
		}
#endif
#endif

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Mem::collect();
	}

}  // namespace Shared

namespace Cpu {
	string cpuName;
	string cpuHz;
	bool has_battery = true;
	bool macM1 = false;
	tuple<int, float, long, string> current_bat;

	const array<string, 10> time_names = {"user", "nice", "system", "idle"};

	std::unordered_map<string, long long> cpu_old = {
		{"totals", 0},
		{"idles", 0},
		{"user", 0},
		{"nice", 0},
		{"system", 0},
		{"idle", 0}
	};

	string get_cpuName() {
		string name;
		char buffer[1024];
		size_t size = sizeof(buffer);
		if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, nullptr, 0) < 0) {
			Logger::error("Failed to get CPU name");
			return name;
		}
		name = string(buffer);
		//? For Apple Silicon, format as "Apple MX [Variant] XX CPUs"
		if (name.find("Apple") != string::npos && Shared::coreCount > 0) {
			return name + " " + to_string(Shared::coreCount) + " CPUs";
		}
		return trim_name(name);
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
					long long t = smcCon.getTemp(-1);  // check if we have package T
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
				} catch (std::runtime_error &e) {
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
		current_cpu.temp_max = 95;  // we have no idea how to get the critical temp
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
					long long temp = smcCon.getTemp((core / threadsPerCore) + core_offset); // same temp for all threads of same physical core
					if (cmp_less(core + 1, current_cpu.temp.size())) {
						current_cpu.temp.at(core + 1).push_back(temp);
						if (current_cpu.temp.at(core + 1).size() > 20)
							current_cpu.temp.at(core + 1).pop_front();
					}
				}
			}
			//? Update shared CPU temp for Pwr panel
			if (not current_cpu.temp.at(0).empty()) {
				Shared::cpuTemp = current_cpu.temp.at(0).back();
			}
		} catch (std::runtime_error &e) {
			got_sensors = false;
			Logger::error("failed getting CPU temp");
		}

		//? Get fan RPM via SMC for Pwr panel (works for both Intel and Apple Silicon)
		try {
			SMCConnection smcFan;
			int fans = smcFan.getFanCount();
			Shared::fanCount.store(fans, std::memory_order_release);
			if (fans > 0) {
				long long totalRpm = 0;
				int validFans = 0;
				for (int i = 0; i < fans; i++) {
					long long rpm = smcFan.getFanRPM(i);
					if (rpm > 0) {
						totalRpm += rpm;
						validFans++;
					}
				}
				if (validFans > 0) {
					Shared::fanRpm.store(totalRpm / validFans, std::memory_order_release);
				}
			}
		} catch (std::runtime_error &e) {
			// Fan reading not available - silently ignore
		}
	}

	string get_cpuHz() {
		unsigned int freq = 1;
		size_t size = sizeof(freq);

		int mib[] = {CTL_HW, HW_CPU_FREQ};

		if (sysctl(mib, 2, &freq, &size, nullptr, 0) < 0) {
			// this fails on Apple Silicon macs. Apparently you're not allowed to know
			return "";
		}
		return std::to_string(freq / 1000.0 / 1000.0 / 1000.0).substr(0, 3);
	}

	auto get_core_mapping() -> std::unordered_map<int, int> {
		std::unordered_map<int, int> core_map;
		if (cpu_temp_only) return core_map;

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
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[Shared::coreCount / 2 + i] = n++;
				}
			} else {
				core_map.clear();
				for (int i = 0, n = 0; i < Shared::coreCount; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[i] = n++;
				}
			}
		}

		//? Apply user set custom mapping if any
		const auto &custom_map = Config::getS("cpu_core_map");
		if (not custom_map.empty()) {
			for (const auto &split : ssplit(custom_map)) {
				const auto vals = ssplit(split, ':');
				if (vals.size() != 2) continue;
				int change_id = stoi_safe(vals.at(0), -1);
				int new_id = stoi_safe(vals.at(1), -1);
				if (change_id < 0 or new_id < 0) continue;
				if (not core_map.contains(change_id) or cmp_greater(new_id, core_sensors.size())) continue;
				core_map.at(change_id) = new_id;
			}
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
		if (not has_battery) return {0, 0, 0, ""};

		uint32_t percent = -1;
		long seconds = -1;
		string status = "discharging";
		IOPSInfo_Wrap ps_info{};
		if (ps_info()) {
			IOPSList_Wrap one_ps_descriptor(ps_info());
			if (one_ps_descriptor()) {
				if (CFArrayGetCount(one_ps_descriptor())) {
					CFDictionaryRef one_ps = IOPSGetPowerSourceDescription(ps_info(), CFArrayGetValueAtIndex(one_ps_descriptor(), 0));
					has_battery = true;
					CFNumberRef remaining = (CFNumberRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSTimeToEmptyKey));
					int32_t estimatedMinutesRemaining;
					if (remaining) {
						CFNumberGetValue(remaining, kCFNumberSInt32Type, &estimatedMinutesRemaining);
						seconds = estimatedMinutesRemaining * 60;
					}
					CFNumberRef charge = (CFNumberRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSCurrentCapacityKey));
					if (charge) {
						CFNumberGetValue(charge, kCFNumberSInt32Type, &percent);
					}
					CFBooleanRef charging = (CFBooleanRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSIsChargingKey));
					if (charging) {
						bool isCharging = CFBooleanGetValue(charging);
						if (isCharging) {
							status = "charging";
						}
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

	auto collect(bool no_update) -> cpu_info & {
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty()))
			return current_cpu;
		auto &cpu = current_cpu;

		if (getloadavg(cpu.load_avg.data(), cpu.load_avg.size()) < 0) {
			Logger::error("failed to get load averages");
		}

		natural_t cpu_count;
		natural_t i;
		kern_return_t error;
		processor_cpu_load_info_data_t *cpu_load_info = nullptr;

		MachProcessorInfo info{};
		error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info.info_array, &info.info_count);
		if (error != KERN_SUCCESS) {
			Logger::error("Failed getting CPU load info, using cached values");
			return cpu;  //? Return cached cpu data instead of using invalid pointer
		}
		cpu_load_info = (processor_cpu_load_info_data_t *)info.info_array;
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
				if (i > Shared::coreCount) break;
				// Use max(1ll, ...) to prevent division by zero when CPU deltas are very small
				const long long calc_totals = max(1ll, totals - core_old_totals.at(i));
				const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
				core_old_totals.at(i) = totals;
				core_old_idles.at(i) = idles;

				cpu.core_percent.at(i).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

				//? Reduce size if there are more values than needed for graph
				if (cpu.core_percent.at(i).size() > 40) cpu.core_percent.at(i).pop_front();

			} catch (const std::exception &e) {
				Logger::error("Cpu::collect() : {}", e.what());
				throw std::runtime_error(fmt::format("collect() : {}", e.what()));
			}
		}

		const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
		const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));

		//? Populate cpu.cpu_percent with all fields from syscall
		for (int ii = 0; const auto &val : times_summed) {
			cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
			cpu_old.at(time_names.at(ii)) = val;

			//? Reduce size if there are more values than needed for graph
			while (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), width * 2)) cpu.cpu_percent.at(time_names.at(ii)).pop_front();

			ii++;
		}

		cpu_old.at("totals") = global_totals;
		cpu_old.at("idles") = global_idles;

		//? Total usage of cpu
		cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

		//? Reduce size if there are more values than needed for graph
		while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

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
}  // namespace Cpu

namespace Mem {
	bool has_swap = false;
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios = 0;
	vector<string> last_found;
	static std::mutex iokit_mutex;  // Protect concurrent IOKit calls
	static std::mutex interface_mutex;  // Protect concurrent interface access during USB device changes

	mem_info current_mem{};

	uint64_t get_totalMem() {
		return Shared::totalMem;
	}

	int64_t getCFNumber(CFDictionaryRef dict, const void *key) {
		CFNumberRef ref = (CFNumberRef)CFDictionaryGetValue(dict, key);
		if (ref) {
			int64_t value;
			CFNumberGetValue(ref, kCFNumberSInt64Type, &value);
			return value;
		}
		return 0;
	}

	string getCFString(io_registry_entry_t volumeRef, CFStringRef key) {
		CFStringRef bsdNameRef = (CFStringRef)IORegistryEntryCreateCFProperty(volumeRef, key, kCFAllocatorDefault, 0);
		if (bsdNameRef) {
			char buf[200];
			CFStringGetCString(bsdNameRef, buf, 200, kCFStringEncodingASCII);
			CFRelease(bsdNameRef);
			return string(buf);
		}
		return "";
	}

	bool isWhole(io_registry_entry_t volumeRef) {
		CFBooleanRef isWholeRef = (CFBooleanRef)IORegistryEntryCreateCFProperty(volumeRef, CFSTR("Whole"), kCFAllocatorDefault, 0);
		if (isWholeRef == nullptr) {
			return false;  // Property doesn't exist, assume not whole disk
		}
		Boolean val = CFBooleanGetValue(isWholeRef);
		CFRelease(isWholeRef);
		return static_cast<bool>(val);
	}

	class IOObject {
		public:
			IOObject(string name, io_object_t& obj) : name(name), object(obj) {}
			virtual ~IOObject() { IOObjectRelease(object); }
		private:
			string name;
			io_object_t &object;
	};

	void collect_disk(std::unordered_map<string, disk_info> &disks, std::unordered_map<string, string> &mapping) {
		// Lock mutex to prevent concurrent IOKit access
		std::lock_guard<std::mutex> lock(iokit_mutex);

		io_registry_entry_t drive;
		io_iterator_t drive_list;

		/* Get the list of all drive objects. */
		if (IOServiceGetMatchingServices(kIOMainPortDefault,
										 IOServiceMatching("IOMediaBSDClient"), &drive_list)) {
			Logger::error("Error in IOServiceGetMatchingServices()");
			return;
		}
		auto d = IOObject("drive list", drive_list); // dummy var so it gets destroyed
		while ((drive = IOIteratorNext(drive_list)) != 0) {
			auto dr = IOObject("drive", drive);
			io_registry_entry_t volumeRef;
			IORegistryEntryGetParentEntry(drive, kIOServicePlane, &volumeRef);
			if (volumeRef) {
				auto vol = IOObject("volume", volumeRef);  //? RAII wrapper to prevent leak
				if (!isWhole(volumeRef)) {
					string bsdName = getCFString(volumeRef, CFSTR("BSD Name"));
					string device = getCFString(volumeRef, CFSTR("VolGroupMntFromName"));
					if (!mapping.contains(device)) {
						device = "/dev/" + bsdName; // try again with BSD name - not all volumes seem to have VolGroupMntFromName property
					}
					if (device != "") {
						if (mapping.contains(device)) {
							string mountpoint = mapping.at(device);
							if (disks.contains(mountpoint)) {
								auto& disk = disks.at(mountpoint);
								//? Skip IO collection for disk images (DMG, ISO, IMG) - they don't have meaningful IO stats
								if (disk.name.find("(DMG)") != string::npos or
								    disk.name.find("(ISO)") != string::npos or
								    disk.name.find("(IMG)") != string::npos) continue;
								CFMutableDictionaryRef properties = nullptr;
								kern_return_t kr = IORegistryEntryCreateCFProperties(volumeRef, &properties, kCFAllocatorDefault, 0);
								if (kr == KERN_SUCCESS and properties != nullptr) {
									CFDictionaryRef statistics = (CFDictionaryRef)CFDictionaryGetValue(properties, CFSTR("Statistics"));
									if (statistics) {
										disk_ios++;
										int64_t readBytes = getCFNumber(statistics, CFSTR("Bytes read from block device"));
										if (disk.io_read.empty())
											disk.io_read.push_back(0);
										else
											disk.io_read.push_back(max((int64_t)0, (readBytes - disk.old_io.at(0))));
										disk.old_io.at(0) = readBytes;
										while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

										int64_t writeBytes = getCFNumber(statistics, CFSTR("Bytes written to block device"));
										if (disk.io_write.empty())
											disk.io_write.push_back(0);
										else
											disk.io_write.push_back(max((int64_t)0, (writeBytes - disk.old_io.at(1))));
										disk.old_io.at(1) = writeBytes;
										while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

										// IOKit does not give us IO times, (use IO read + IO write with 1 MiB being 100% to get some activity indication)
										if (disk.io_activity.empty())
											disk.io_activity.push_back(0);
										else
											disk.io_activity.push_back(clamp((long)round((double)(disk.io_write.back() + disk.io_read.back()) / (1 << 20)), 0l, 100l));
										while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
									}
									CFRelease(properties);
								}
							}
						}
					}
				}
			}
		}
	}

	auto collect(bool no_update) -> mem_info & {
		if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
			return current_mem;

		auto show_swap = Config::getB("show_swap");
		auto show_disks = Config::getB("show_disks");
		auto swap_disk = Config::getB("swap_disk");
		auto &mem = current_mem;
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
			for (const auto &name : swap_names) {
				mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
				while (cmp_greater(mem.percent.at(name).size(), width * 2))
					mem.percent.at(name).pop_front();
			}
			has_swap = true;
		} else
			has_swap = false;
		//? Calculate percentages
		for (const auto &name : mem_names) {
			mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / Shared::totalMem));
			while (cmp_greater(mem.percent.at(name).size(), width * 2))
				mem.percent.at(name).pop_front();
		}

		//? VRAM (GPU unified memory) - only if available
		long long vram_total = Shared::gpuMemTotal.load(std::memory_order_acquire);
		long long vram_used = Shared::gpuMemUsed.load(std::memory_order_acquire);
		if (vram_total > 0) {
			mem.stats["vram"] = vram_used;
			mem.stats["vram_total"] = vram_total;
			mem.percent["vram"].push_back(round((double)vram_used * 100 / vram_total));
			while (cmp_greater(mem.percent["vram"].size(), width * 2))
				mem.percent["vram"].pop_front();
		}

		if (show_disks) {
			std::unordered_map<string, string> mapping;  // keep mapping from device -> mountpoint, since IOKit doesn't give us the mountpoint
			double uptime = system_uptime();
			auto &disks_filter = Config::getS("disks_filter");
			bool filter_exclude = false;
			auto show_network_drives = Config::getB("show_network_drives");
			auto &disks = mem.disks;
			vector<string> filter;
			if (not disks_filter.empty()) {
				filter = ssplit(disks_filter);
				if (filter.at(0).starts_with("exclude=")) {
					filter_exclude = true;
					filter.at(0) = filter.at(0).substr(8);
				}
			}

			struct statfs *stfs;
			int count = getmntinfo(&stfs, MNT_WAIT);
			vector<string> found;
			found.reserve(last_found.size());
			for (int i = 0; i < count; i++) {
				std::error_code ec;
				string mountpoint = stfs[i].f_mntonname;
				string dev = stfs[i].f_mntfromname;
				string fstype = stfs[i].f_fstypename;
				uint32_t flags = stfs[i].f_flags;
				mapping[dev] = mountpoint;

				//? Skip autofs
				if (fstype == "autofs") {
					continue;
				}

				//? Skip volumes with nobrowse flag (internal macOS APFS volumes like VM, Preboot, Update, Data, etc.)
				if (flags & MNT_DONTBROWSE) {
					continue;
				}

				//? Skip devfs
				if (fstype == "devfs") {
					continue;
				}

				//? Handle remote/network filesystems (SMB, NFS, AFP, WebDAV)
				bool is_network_drive = not (flags & MNT_LOCAL);
				if (is_network_drive and not show_network_drives) {
					continue;  // Skip non-local (remote) filesystems unless enabled
				}

				//? Match filter if not empty
				if (not filter.empty()) {
					bool match = v_contains(filter, mountpoint);
					if ((filter_exclude and match) or (not filter_exclude and not match))
						continue;
				}

				found.push_back(mountpoint);
				if (not disks.contains(mountpoint)) {
					disks[mountpoint] = disk_info{fs::canonical(dev, ec), "", fstype};

					if (disks.at(mountpoint).dev.empty())
						disks.at(mountpoint).dev = dev;

					// Get actual volume name from macOS
					string vol_name = get_volume_name(mountpoint.c_str());

					if (vol_name.empty()) {
						// Fallback if volume name query fails
						if (mountpoint.starts_with("/Volumes/"))
							vol_name = fs::path(mountpoint).filename();
						else
							vol_name = mountpoint;
					}

					// Append connection/protocol type
					if (is_network_drive) {
						// Map filesystem type to protocol label
						string proto;
						if (fstype == "smbfs" or fstype == "cifs")
							proto = "SMB";
						else if (fstype == "nfs" or fstype == "nfs4")
							proto = "NFS";
						else if (fstype == "afpfs")
							proto = "AFP";
						else if (fstype == "webdav")
							proto = "WebDAV";
						else
							proto = "NET";  // Generic network
						vol_name += " (" + proto + ")";
					} else {
						// Get connection type for local drives (USB, TB, SATA)
						string conn_type = get_disk_type(dev.c_str());
						if (not conn_type.empty())
							vol_name += " (" + conn_type + ")";
					}

					disks.at(mountpoint).name = vol_name;
				}


				if (not v_contains(last_found, mountpoint))
					redraw = true;

				//? Set initial disk stats from statfs (fallback if statvfs fails later)
				disks.at(mountpoint).free = (uint64_t)stfs[i].f_bfree * stfs[i].f_bsize;
				disks.at(mountpoint).total = (uint64_t)stfs[i].f_blocks * stfs[i].f_bsize;
			}

			//? Remove disks no longer mounted or filtered out
			if (swap_disk and has_swap) found.push_back("swap");
			for (auto it = disks.begin(); it != disks.end();) {
				if (not v_contains(found, it->first))
					it = disks.erase(it);
				else
					it++;
			}
			if (found.size() != last_found.size()) redraw = true;
			last_found = std::move(found);

			//? Get disk/partition stats
			for (auto &[mountpoint, disk] : disks) {
				if (std::error_code ec; not fs::exists(mountpoint, ec))
					continue;
				struct statvfs vfs;
				if (statvfs(mountpoint.c_str(), &vfs) < 0) {
					Logger::warning("Failed to get disk/partition stats with statvfs() for: {}", mountpoint);
					continue;
				}
				//? Skip statvfs total for network filesystems (AFP, SMB, NFS report incorrect totals)
				//? Keep the statfs values set earlier which are more accurate for network shares
				bool is_network_fs = (disk.fstype == "afpfs" or disk.fstype == "smbfs" or
				                      disk.fstype == "nfs" or disk.fstype == "nfs4" or
				                      disk.fstype == "cifs" or disk.fstype == "webdav");
				if (not is_network_fs)
					disk.total = vfs.f_blocks * vfs.f_frsize;

				//? Use macOS API to get available space including purgeable (like Finder shows)
				int64_t avail_with_purgeable = get_avail_with_purgeable(mountpoint.c_str());
				if (avail_with_purgeable > 0) {
					disk.free = static_cast<uint64_t>(avail_with_purgeable);
				} else {
					disk.free = vfs.f_bfree * vfs.f_frsize;
				}
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
			for (const auto &name : last_found)
				if (not is_in(name, "/", "swap", "/dev"))
					mem.disks_order.push_back(name);

			disk_ios = 0;
			collect_disk(disks, mapping);

			old_uptime = uptime;
		}
		return mem;
	}

}  // namespace Mem

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
		struct ifaddrs *ifaddr;

	   public:
		int status;
		getifaddr_wrapper() { status = getifaddrs(&ifaddr); }
		~getifaddr_wrapper() { freeifaddrs(ifaddr); }
		auto operator()() -> struct ifaddrs * { return ifaddr; }
	};

	auto collect(bool no_update) -> net_info & {
		// Lock mutex to prevent concurrent interface access during USB device changes
		std::lock_guard<std::mutex> lock(Mem::interface_mutex);
		auto &net = current_net;
		auto &config_iface = Config::getS("net_iface");
		auto net_sync = Config::getB("net_sync");
		auto net_auto = Config::getB("net_auto");
		auto new_timestamp = time_ms();

		if (not no_update and errors < 3) {
			//? Get interface list using getifaddrs() wrapper
			getifaddr_wrapper if_wrap{};
			if (if_wrap.status != 0) {
				errors++;
				Logger::error("Net::collect() -> getifaddrs() failed with id {}", if_wrap.status);
				redraw = true;
				return empty_net;
			}
			int family = 0;
			static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN); // 46 >= 16, compile-time assurance.
			enum { IPBUFFER_MAXSIZE = INET6_ADDRSTRLEN }; // manually using the known biggest value, guarded by the above static_assert
			char ip[IPBUFFER_MAXSIZE];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all items in getifaddrs() list
			for (auto *ifa = if_wrap(); ifa != nullptr; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == nullptr) continue;
				family = ifa->ifa_addr->sa_family;
				const auto &iface = ifa->ifa_name;
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
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv4 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv4 to string for iface {}, errno: {}", iface, strerror(errsv));
						}
					}
				}
				//? Get IPv6 address
				else if (family == AF_INET6) {
					if (net[iface].ipv6.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv6 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv6 to string for iface {}, errno: {}", iface, strerror(errsv));
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
					char *lim = buf.get() + len;
					char *next = nullptr;
					for (next = buf.get(); next < lim;) {
						struct if_msghdr *ifm = (struct if_msghdr *)next;
						next += ifm->ifm_msglen;
						if (ifm->ifm_type == RTM_IFINFO2) {
							struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
							struct sockaddr_dl *sdl = (struct sockaddr_dl *)(if2m + 1);
							char iface[32];
							strncpy(iface, sdl->sdl_data, sdl->sdl_nlen);
							iface[sdl->sdl_nlen] = 0;
							ifstats[iface] = std::tuple(if2m->ifm_data.ifi_ibytes, if2m->ifm_data.ifi_obytes);
						}
					}
				}
			}

			//? Get total received and transmitted bytes + device address if no ip was found
			for (const auto &iface : interfaces) {
				for (const string dir : {"download", "upload"}) {
					auto &saved_stat = net.at(iface).stat.at(dir);
					auto &bandwidth = net.at(iface).bandwidth.at(dir);
					uint64_t val = dir == "download" ? std::get<0>(ifstats[iface]) : std::get<1>(ifstats[iface]);

					//? Update speed, total and top values
					if (val < saved_stat.last) {
						saved_stat.rollover += saved_stat.last;
						saved_stat.last = 0;
					}
					if (cmp_greater((unsigned long long)saved_stat.rollover + (unsigned long long)val, numeric_limits<uint64_t>::max())) {
						saved_stat.rollover = 0;
						saved_stat.last = 0;
					}
					saved_stat.speed = round((double)(val - saved_stat.last) / ((double)(new_timestamp - timestamp) / 1000));
					if (saved_stat.speed > saved_stat.top) saved_stat.top = saved_stat.speed;
					if (saved_stat.offset > val + saved_stat.rollover) saved_stat.offset = 0;
					saved_stat.total = (val + saved_stat.rollover) - saved_stat.offset;
					saved_stat.last = val;

					//? Add values to graph
					bandwidth.push_back(saved_stat.speed);
					while (cmp_greater(bandwidth.size(), width * 2)) bandwidth.pop_front();

					//? Set counters for auto scaling
					if (net_auto and selected_iface == iface) {
						if (saved_stat.speed > graph_max[dir]) {
							++max_count[dir][0];
							if (max_count[dir][1] > 0) --max_count[dir][1];
						} else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
							++max_count[dir][1];
							if (max_count[dir][0] > 0) --max_count[dir][0];
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
			if (net_auto) rescale = true;
			if (not config_iface.empty() and v_contains(interfaces, config_iface))
				selected_iface = config_iface;
			else {
				//? Sort interfaces by total upload + download bytes
				auto sorted_interfaces = interfaces;
				rng::sort(sorted_interfaces, [&](const auto &a, const auto &b) {
					return cmp_greater(net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
									   net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
				});
				selected_iface.clear();
				//? Try to set to a connected interface
				for (const auto &iface : sorted_interfaces) {
					if (net.at(iface).connected) selected_iface = iface;
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
			for (const auto &dir : {"download", "upload"}) {
				for (const auto &sel : {0, 1}) {
					if (rescale or max_count[dir][sel] >= 5) {
						const long long avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
														? std::accumulate(net.at(selected_iface).bandwidth.at(dir).rbegin(), net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0ll) / 5
														: net[selected_iface].stat[dir].speed);
						graph_max[dir] = max(uint64_t(avg_speed * (sel == 0 ? 1.3 : 3.0)), (uint64_t)10 << 10);
						max_count[dir][0] = max_count[dir][1] = 0;
						redraw = true;
						if (net_sync) sync = true;
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
}  // namespace Net

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

	//? GPU per-process monitoring for Apple Silicon
	namespace GpuProc {
		struct GpuClientInfo {
			uint64_t accumulated_gpu_time = 0;  // nanoseconds
		};

		static std::unordered_map<size_t, GpuClientInfo> old_gpu_times;
		static std::unordered_map<size_t, GpuClientInfo> current_gpu_times;
		static std::chrono::steady_clock::time_point last_collection_time;
		static std::chrono::steady_clock::time_point prev_collection_time;
		static bool initialized = false;
		static int64_t elapsed_ns = 1000000000;  // Default 1 second

		// Helper to extract string from CFString
		string cfstring_to_string(CFStringRef cf_str) {
			if (!cf_str) return "";
			CFIndex length = CFStringGetLength(cf_str);
			CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
			string result(max_size, '\0');
			if (CFStringGetCString(cf_str, &result[0], max_size, kCFStringEncodingUTF8)) {
				result.resize(strlen(result.c_str()));
				return result;
			}
			return "";
		}

		// Parse "pid XXX, processname" format
		bool parse_creator_string(const string& creator, size_t& pid) {
			size_t pid_pos = creator.find("pid ");
			if (pid_pos == string::npos) return false;
			size_t comma_pos = creator.find(',', pid_pos);
			if (comma_pos == string::npos) return false;
			try {
				pid = static_cast<size_t>(std::stoul(creator.substr(pid_pos + 4, comma_pos - pid_pos - 4)));
				return true;
			} catch (...) {
				return false;
			}
		}

		// Extract accumulated GPU time from AppUsage array
		uint64_t extract_gpu_time(CFArrayRef app_usage) {
			if (!app_usage) return 0;
			uint64_t total_time = 0;
			CFIndex count = CFArrayGetCount(app_usage);
			for (CFIndex i = 0; i < count; i++) {
				CFDictionaryRef usage_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(app_usage, i);
				if (!usage_dict || CFGetTypeID(usage_dict) != CFDictionaryGetTypeID()) continue;
				CFNumberRef gpu_time_ref = (CFNumberRef)CFDictionaryGetValue(usage_dict, CFSTR("accumulatedGPUTime"));
				if (gpu_time_ref && CFGetTypeID(gpu_time_ref) == CFNumberGetTypeID()) {
					int64_t gpu_time = 0;
					CFNumberGetValue(gpu_time_ref, kCFNumberSInt64Type, &gpu_time);
					total_time += static_cast<uint64_t>(gpu_time);
				}
			}
			return total_time;
		}

		// Collect GPU client information from IORegistry
		void collect_gpu_clients() {
			// Swap old and current
			old_gpu_times = std::move(current_gpu_times);
			current_gpu_times.clear();

			// Update timing
			prev_collection_time = last_collection_time;
			auto current_time = std::chrono::steady_clock::now();
			if (initialized) {
				elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(current_time - prev_collection_time).count();
				if (elapsed_ns <= 0) elapsed_ns = 1000000000;  // Default to 1 second
			}

			// Find the AGXAccelerator (GPU device)
			CFMutableDictionaryRef matching = IOServiceMatching("IOAccelerator");
			if (!matching) return;

			io_iterator_t accel_iterator;
			kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &accel_iterator);
			if (kr != KERN_SUCCESS) return;

			io_service_t accelerator;
			while ((accelerator = IOIteratorNext(accel_iterator)) != 0) {
				// Get children (AGXDeviceUserClient instances)
				io_iterator_t child_iterator;
				kr = IORegistryEntryGetChildIterator(accelerator, kIOServicePlane, &child_iterator);
				if (kr != KERN_SUCCESS) {
					IOObjectRelease(accelerator);
					continue;
				}

				io_service_t child;
				while ((child = IOIteratorNext(child_iterator)) != 0) {
					// Get the class name
					io_name_t class_name;
					IOObjectGetClass(child, class_name);

					// Only process AGXDeviceUserClient
					if (strcmp(class_name, "AGXDeviceUserClient") != 0) {
						IOObjectRelease(child);
						continue;
					}

					// Get properties of the user client
					CFMutableDictionaryRef properties = nullptr;
					kr = IORegistryEntryCreateCFProperties(child, &properties, kCFAllocatorDefault, 0);

					if (kr == KERN_SUCCESS && properties) {
						// Get IOUserClientCreator
						CFStringRef creator_ref = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("IOUserClientCreator"));
						if (creator_ref && CFGetTypeID(creator_ref) == CFStringGetTypeID()) {
							string creator = cfstring_to_string(creator_ref);
							size_t pid;
							if (parse_creator_string(creator, pid)) {
								// Get AppUsage array
								CFArrayRef app_usage = (CFArrayRef)CFDictionaryGetValue(properties, CFSTR("AppUsage"));
								uint64_t gpu_time = 0;
								if (app_usage && CFGetTypeID(app_usage) == CFArrayGetTypeID()) {
									gpu_time = extract_gpu_time(app_usage);
								}

								// Aggregate by PID (a process may have multiple clients)
								current_gpu_times[pid].accumulated_gpu_time += gpu_time;
							}
						}
						CFRelease(properties);
					}
					IOObjectRelease(child);
				}
				IOObjectRelease(child_iterator);
				IOObjectRelease(accelerator);
			}
			IOObjectRelease(accel_iterator);

			if (!initialized) {
				// First run - copy current to old for valid delta calculation next time
				old_gpu_times = current_gpu_times;
				initialized = true;
			}

			last_collection_time = current_time;
		}

		// Get GPU usage percentage for a given PID
		double get_gpu_percent(size_t pid) {
			if (!initialized) return 0.0;

			auto it_new = current_gpu_times.find(pid);
			if (it_new == current_gpu_times.end()) return 0.0;

			auto it_old = old_gpu_times.find(pid);
			if (it_old == old_gpu_times.end()) return 0.0;

			// Calculate delta (handle potential wraparound by treating as unsigned)
			uint64_t new_time = it_new->second.accumulated_gpu_time;
			uint64_t old_time = it_old->second.accumulated_gpu_time;
			if (new_time <= old_time) return 0.0;
			uint64_t delta = new_time - old_time;

			// GPU time / elapsed time * 100 = percentage
			double percent = static_cast<double>(delta) / static_cast<double>(elapsed_ns) * 100.0;
			return std::min(100.0, std::max(0.0, percent));  // Clamp to 0-100
		}

		// Get accumulated GPU time in nanoseconds for a given PID
		uint64_t get_gpu_time(size_t pid) {
			auto it = current_gpu_times.find(pid);
			if (it == current_gpu_times.end()) return 0;
			return it->second.accumulated_gpu_time;
		}
	}  // namespace GpuProc

	string get_status(char s) {
		//? Use equality comparison, not bitwise AND
		//? State values: SIDL=1, SRUN=2, SSLEEP=3, SSTOP=4, SZOMB=5
		switch (s) {
			case SRUN: return "Running";
			case SSLEEP: return "Sleeping";
			case SIDL: return "Idle";
			case SSTOP: return "Stopped";
			case SZOMB: return "Zombie";
			case 'X': return "Dead";
			default: return "Unknown";
		}
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, vector<proc_info> &procs) {
		if (pid != detailed.last_pid) {
			detailed = {};
			detailed.last_pid = pid;
			detailed.skip_smaps = not Config::getB("proc_info_smaps");
		}

		//? Copy proc_info for process from proc vector
		auto p_info = rng::find(procs, pid, &proc_info::pid);
		detailed.entry = *p_info;

		//? Update cpu percent deque for process cpu graph
		if (not Config::getB("proc_per_core")) detailed.entry.cpu_p *= Shared::coreCount;
		detailed.cpu_percent.push_back(clamp((long long)round(detailed.entry.cpu_p), 0ll, 100ll));
		while (cmp_greater(detailed.cpu_percent.size(), width)) detailed.cpu_percent.pop_front();

		//? Update gpu percent deque for process gpu graph (Apple Silicon)
		detailed.gpu_percent.push_back(clamp((long long)round(detailed.entry.gpu_p), 0ll, 100ll));
		while (cmp_greater(detailed.gpu_percent.size(), width)) detailed.gpu_percent.pop_front();

		//? Process runtime : current time - start time (both in unix time - seconds since epoch)
		struct timeval currentTime;
		gettimeofday(&currentTime, nullptr);
		//? Get elapsed time if process isn't dead
		if (detailed.entry.state != 'X') detailed.elapsed = sec_to_dhms(currentTime.tv_sec - (detailed.entry.cpu_s / 1'000'000));
		else detailed.elapsed = sec_to_dhms(detailed.entry.death_time);
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

		//? Get parent process name
		if (detailed.parent.empty()) {
			auto p_entry = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
			if (p_entry != procs.end()) detailed.parent = p_entry->name;
		}

		//? Expand process status from single char to explanative string
		detailed.status = get_status(detailed.entry.state);

		detailed.mem_bytes.push_back(detailed.entry.mem);
		detailed.memory = floating_humanizer(detailed.entry.mem);

		if (detailed.first_mem == -1 or detailed.first_mem < detailed.mem_bytes.back() / 2 or detailed.first_mem > detailed.mem_bytes.back() * 4) {
			detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Mem::get_totalMem());
			redraw = true;
		}

		while (cmp_greater(detailed.mem_bytes.size(), width)) detailed.mem_bytes.pop_front();

		rusage_info_current rusage;
		if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, (void **)&rusage) == 0) {
			// this fails for processes we don't own - same as in Linux
			detailed.io_read = floating_humanizer(rusage.ri_diskio_bytesread);
			detailed.io_write = floating_humanizer(rusage.ri_diskio_byteswritten);
		}
	}

	//* Collects and sorts process information from /proc
	auto collect(bool no_update) -> vector<proc_info> & {
		const auto &sorting = Config::getS("proc_sorting");
		auto reverse = Config::getB("proc_reversed");
		const auto &filter = Config::getS("proc_filter");
		auto per_core = Config::getB("proc_per_core");
		auto tree = Config::getB("proc_tree");
		auto show_detailed = Config::getB("show_detailed");
		const auto pause_proc_list = Config::getB("pause_proc_list");
		const size_t detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		bool tree_mode_change = tree != is_tree_mode;
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}
		if (tree_mode_change) is_tree_mode = tree;

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		static std::unordered_set<size_t> found;

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, current_procs);
		} else {
			//* ---------------------------------------------Collection start----------------------------------------------

			{  //* Get CPU totals
				natural_t cpu_count;
				kern_return_t error;
				processor_cpu_load_info_data_t *cpu_load_info = nullptr;
				MachProcessorInfo info{};
				error = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info.info_array, &info.info_count);
				if (error != KERN_SUCCESS) {
					Logger::error("Failed getting CPU load info");
				}
				cpu_load_info = (processor_cpu_load_info_data_t *)info.info_array;
				cputimes = 0;
				for (natural_t i = 0; i < cpu_count; i++) {
					cputimes 	+= (cpu_load_info[i].cpu_ticks[CPU_STATE_USER]
								+ cpu_load_info[i].cpu_ticks[CPU_STATE_NICE]
								+ cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM]
								+ cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE]);
				}
			}

			//* Collect GPU per-process usage data (Apple Silicon)
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
			GpuProc::collect_gpu_clients();
#endif

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

				//? Build hash map for O(1) process lookup instead of O(n) linear search
				std::unordered_map<size_t, size_t> pid_to_index;
				pid_to_index.reserve(current_procs.size());
				for (size_t idx = 0; idx < current_procs.size(); ++idx) {
					pid_to_index[current_procs[idx].pid] = idx;
				}

				for (size_t i = 0; i < count; i++) {  //* iterate over all processes in kinfo_proc
					struct kinfo_proc& kproc = processes.get()[i];
					const size_t pid = (size_t)kproc.kp_proc.p_pid;
					if (pid < 1) continue;
					found.insert(pid);

					//? Check if pid already exists in current_procs using O(1) hash lookup
					bool no_cache = false;
					auto pid_it = pid_to_index.find(pid);
					auto find_old = (pid_it != pid_to_index.end())
						? current_procs.begin() + static_cast<ptrdiff_t>(pid_it->second)
						: current_procs.end();
					//? Only add new processes if not paused
					if (find_old == current_procs.end()) {
						if (not pause_proc_list) {
							current_procs.push_back({pid});
							find_old = current_procs.end() - 1;
							no_cache = true;
						}
						else continue;
					}
					else if (dead_procs.contains(pid)) continue;

					auto &new_proc = *find_old;

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
									if (size_t start_pos = proc_args.find_first_not_of('\0', null_pos); start_pos != string::npos) {
										while (argc-- > 0 and null_pos != string::npos and cmp_less(new_proc.cmd.size(), 1000)) {
											null_pos = proc_args.find('\0', start_pos);
											new_proc.cmd += (string)proc_args.substr(start_pos, null_pos - start_pos) + ' ';
											start_pos = null_pos + 1;
										}
									}
								}
								if (not new_proc.cmd.empty()) new_proc.cmd.pop_back();
							}
						}
						if (new_proc.cmd.empty()) new_proc.cmd = f_name;
						if (new_proc.cmd.size() > 1000) {
							new_proc.cmd.resize(1000);
							new_proc.cmd.shrink_to_fit();
						}
						new_proc.ppid = kproc.kp_eproc.e_ppid;
						new_proc.cpu_s = kproc.kp_proc.p_starttime.tv_sec * 1'000'000 + kproc.kp_proc.p_starttime.tv_usec;
						struct passwd *pwd = getpwuid(kproc.kp_eproc.e_ucred.cr_uid);
                        if (pwd != nullptr) {
                            new_proc.user = pwd->pw_name;
                        } else {
                            new_proc.user = std::to_string(kproc.kp_eproc.e_ucred.cr_uid);
                        }
					}
					new_proc.p_nice = kproc.kp_proc.p_nice;
					new_proc.p_priority = kproc.kp_proc.p_priority;

					//? Get threads, mem and cpu usage
					struct proc_taskinfo pti{};
					if (sizeof(pti) == proc_pidinfo(new_proc.pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti))) {
						new_proc.threads = pti.pti_threadnum;
						new_proc.mem = pti.pti_resident_size;
						new_proc.res_mem = pti.pti_resident_size;
						new_proc.virt_mem = pti.pti_virtual_size;
						cpu_t = pti.pti_total_user + pti.pti_total_system;

						if (new_proc.cpu_t == 0) new_proc.cpu_t = cpu_t;

						//? Determine process state using pti_numrunning for accuracy
						//? macOS p_stat is unreliable - use actual running thread count instead
						char p_stat = kproc.kp_proc.p_stat;
						if (p_stat == SZOMB) {
							new_proc.state = SZOMB;       // Zombie
						} else if (p_stat == SSTOP) {
							new_proc.state = SSTOP;       // Stopped (debugger/signal)
						} else if (pti.pti_numrunning > 0) {
							new_proc.state = SRUN;        // Actually running
						} else {
							new_proc.state = SSLEEP;      // Sleeping/waiting
						}
					} else {
						// Reset memory value if process info cannot be accessed (bad permissions or zombie processes)
						new_proc.threads = 0;
						new_proc.mem = 0;
						new_proc.res_mem = 0;
						new_proc.virt_mem = 0;
						cpu_t = new_proc.cpu_t;
						//? Can't get task info - use p_stat as fallback
						new_proc.state = kproc.kp_proc.p_stat;
					}

					//? Calculate process runtime in seconds
					struct timeval now;
					gettimeofday(&now, nullptr);
					uint64_t now_us = now.tv_sec * 1'000'000ULL + now.tv_usec;
					if (new_proc.cpu_s > 0 and now_us > new_proc.cpu_s) {
						new_proc.runtime = (now_us - new_proc.cpu_s) / 1'000'000ULL;
					}

					//? Process cpu usage since last update
					new_proc.cpu_p = clamp(round(((cpu_t - new_proc.cpu_t) * Shared::machTck) / ((cputimes - old_cputimes) * Shared::clkTck)) * cmult / 1000.0, 0.0, 100.0 * Shared::coreCount);

					//? Process cumulative cpu usage since process start
					new_proc.cpu_c = (double)(cpu_t * Shared::machTck) / (timeNow - new_proc.cpu_s);

					//? Update cached value with latest cpu times
					new_proc.cpu_t = cpu_t;

					//? Process GPU usage (Apple Silicon only)
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
					new_proc.gpu_p = GpuProc::get_gpu_percent(pid);
					new_proc.gpu_time = GpuProc::get_gpu_time(pid);
#endif

					//? Process disk I/O and memory details from rusage
					rusage_info_current rusage{};
					if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, (void **)&rusage) == 0) {
						// Convert bytes to estimated I/O operation counts (divide by 4KB block size)
						new_proc.io_read = rusage.ri_diskio_bytesread / 4096;
						new_proc.io_write = rusage.ri_diskio_byteswritten / 4096;
						// Physical footprint gives actual private memory usage
						// Shared = resident - private (physical footprint)
						if (rusage.ri_phys_footprint > 0) {
							new_proc.shared_mem = (new_proc.res_mem > rusage.ri_phys_footprint)
								? (new_proc.res_mem - rusage.ri_phys_footprint) : 0;
						}
						// Wired size is memory that can't be paged out
						new_proc.send_bytes = rusage.ri_diskio_bytesread;   // Raw disk read bytes
						new_proc.recv_bytes = rusage.ri_diskio_byteswritten; // Raw disk write bytes
					} else {
						// Keep previous values if rusage fails (may fail for processes we don't own)
					}

					//? Get Mach port count for process
					int portCount = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
					if (portCount > 0) {
						new_proc.ports = portCount / sizeof(struct proc_fdinfo);
					}

					if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
						got_detailed = true;
					}
				}

				//? Clear dead processes from current_procs if not paused
				if (not pause_proc_list) {
					auto eraser = rng::remove_if(current_procs, [&](const auto& element) { return not found.contains(element.pid); });
					current_procs.erase(eraser.begin(), eraser.end());
					if (!dead_procs.empty()) dead_procs.clear();
				}
				//? Set correct state of dead processes if paused
				else {
					for (auto& r : current_procs) {
						if (not found.contains(r.pid)) {
							if (r.state != 'X') {
								struct timeval currentTime;
								gettimeofday(&currentTime, nullptr);
								r.death_time = currentTime.tv_sec - (r.cpu_s / 1'000'000);
							}
							r.state = 'X';
							dead_procs.emplace(r.pid);
							//? Reset cpu usage for dead processes if paused and option is set
							if (!Config::getB("keep_dead_proc_usage")) {
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
			for (auto &p : current_procs) {
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
				if (collapser != current_procs.end()){
					for (auto& p : current_procs) {
						if (p.ppid == collapser->pid) {
							auto child = rng::find(current_procs, p.pid, &proc_info::pid);
							if (child != current_procs.end()){
								child->collapsed = not child->collapsed;
							}
						}
					}
					if (Config::ints.at("proc_selected") > 0) locate_selection = true;
				}
				toggle_children = -1;
			}
			
			if (auto find_pid = (collapse != -1 ? collapse : expand); find_pid != -1) {
				auto collapser = rng::find(current_procs, find_pid, &proc_info::pid);
				if (collapser != current_procs.end()) {
					if (collapse == expand) {
						collapser->collapsed = not collapser->collapsed;
					}
					else if (collapse > -1) {
						collapser->collapsed = true;
					}
					else if (expand > -1) {
						collapser->collapsed = false;
					}
					if (Config::ints.at("proc_selected") > 0) locate_selection = true;
				}
				collapse = expand = -1;
			}
			if (should_filter or not filter.empty()) filter_found = 0;

			vector<tree_proc> tree_procs;
			tree_procs.reserve(current_procs.size());

			if (!pause_proc_list) {
				for (auto& p : current_procs) {
					if (not found.contains(p.ppid)) p.ppid = 0;
				}
			}

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, & proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			//? Recursive sort over tree structure to account for collapsed processes in the tree
			int index = 0;
			tree_sort(tree_procs, sorting, reverse, (pause_proc_list and not (sorted_change or tree_mode_change)), index, current_procs.size());

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
}  // namespace Proc

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
}  // namespace Tools
