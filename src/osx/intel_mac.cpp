// SPDX-License-Identifier: Apache-2.0

#include "intel_mac.hpp"

#if defined(__APPLE__) && defined(GPU_SUPPORT) && defined(__x86_64__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../btop_log.hpp"
#include "smc.hpp"

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

namespace IntelMac::Gpu {
	namespace {
		enum class Vendor {
			Intel,
			Amd,
			Other,
		};

		struct Device {
			io_registry_entry_t accelerator{};
			std::size_t gpu_index{};
			Vendor vendor{Vendor::Other};
			std::string name;

			Device() = default;
			Device(const Device&) = delete;
			auto operator=(const Device&) -> Device& = delete;
			Device(Device&& other) noexcept
				: accelerator(std::exchange(other.accelerator, 0)),
				  gpu_index(other.gpu_index),
				  vendor(other.vendor),
				  name(std::move(other.name)) {}
			auto operator=(Device&& other) noexcept -> Device& {
				if (this != &other) {
					if (accelerator) IOObjectRelease(accelerator);
					accelerator = std::exchange(other.accelerator, 0);
					gpu_index = other.gpu_index;
					vendor = other.vendor;
					name = std::move(other.name);
				}
				return *this;
			}
			~Device() {
				if (accelerator) IOObjectRelease(accelerator);
			}
		};

		std::vector<Device> devices;
		std::unique_ptr<Cpu::SMCConnection> smc;
		bool initialized = false;

		template <typename T>
		class CFRef {
		   public:
			explicit CFRef(T value = nullptr) : value(value) {}
			~CFRef() {
				if (value) CFRelease(value);
			}
			CFRef(const CFRef&) = delete;
			auto operator=(const CFRef&) -> CFRef& = delete;
			CFRef(CFRef&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
			auto operator=(CFRef&& other) noexcept -> CFRef& {
				if (this != &other) {
					if (value) CFRelease(value);
					value = std::exchange(other.value, nullptr);
				}
				return *this;
			}
			auto get() const -> T { return value; }
			operator bool() const { return value != nullptr; }

		   private:
			T value;
		};

		auto integer_value(CFTypeRef value) -> std::optional<int64_t> {
			if (value == nullptr) return std::nullopt;
			if (CFGetTypeID(value) == CFNumberGetTypeID()) {
				int64_t result = 0;
				if (CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberSInt64Type, &result))
					return result;
			}
			if (CFGetTypeID(value) == CFDataGetTypeID()) {
				const auto data = static_cast<CFDataRef>(value);
				const auto length = std::min<CFIndex>(CFDataGetLength(data), sizeof(uint64_t));
				if (length <= 0) return std::nullopt;
				uint64_t result = 0;
				memcpy(&result, CFDataGetBytePtr(data), static_cast<std::size_t>(length));
				if (result <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
					return static_cast<int64_t>(result);
			}
			return std::nullopt;
		}

		auto dictionary_integer(CFDictionaryRef dictionary, CFStringRef key) -> std::optional<int64_t> {
			return dictionary ? integer_value(CFDictionaryGetValue(dictionary, key)) : std::nullopt;
		}

		auto dictionary_has(CFDictionaryRef dictionary, CFStringRef key) -> bool {
			return dictionary && CFDictionaryContainsKey(dictionary, key);
		}

		auto registry_property(io_registry_entry_t entry, CFStringRef key) -> CFTypeRef {
			return IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
		}

		auto inherited_property(io_registry_entry_t entry, CFStringRef key) -> CFTypeRef {
			return IORegistryEntrySearchCFProperty(
				entry,
				kIOServicePlane,
				key,
				kCFAllocatorDefault,
				kIORegistryIterateParents | kIORegistryIterateRecursively
			);
		}

		auto string_value(CFTypeRef value) -> std::string {
			if (value == nullptr) return {};
			if (CFGetTypeID(value) == CFStringGetTypeID()) {
				char buffer[256]{};
				return CFStringGetCString(
					static_cast<CFStringRef>(value),
					buffer,
					sizeof(buffer),
					kCFStringEncodingUTF8
				) ? std::string{buffer} : std::string{};
			}
			if (CFGetTypeID(value) == CFDataGetTypeID()) {
				const auto data = static_cast<CFDataRef>(value);
				const auto length = std::min<CFIndex>(CFDataGetLength(data), 255);
				std::string result(
					reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
					static_cast<std::size_t>(std::max<CFIndex>(length, 0))
				);
				if (const auto terminator = result.find('\0'); terminator != std::string::npos)
					result.resize(terminator);
				return result;
			}
			return {};
		}

		auto active_gpu_name() -> std::string {
			const auto mux = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleMuxControl"));
			if (mux == 0) return {};
			CFRef<CFTypeRef> value(registry_property(mux, CFSTR("ActiveGPU")));
			IOObjectRelease(mux);
			return string_value(value.get());
		}

		auto has_ancestor_named(const io_registry_entry_t entry, const std::string& target) -> bool {
			if (target.empty()) return true;

			io_registry_entry_t current = entry;
			IOObjectRetain(current);
			while (current != 0) {
				io_name_t name{};
				if (IORegistryEntryGetName(current, name) == kIOReturnSuccess and target == name) {
					IOObjectRelease(current);
					return true;
				}

				io_registry_entry_t parent = 0;
				const auto result = IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
				IOObjectRelease(current);
				if (result != kIOReturnSuccess) break;
				current = parent;
			}
			return false;
		}

		auto detect_vendor(const std::string& match) -> Vendor {
			if (match.find("8086") != std::string::npos) return Vendor::Intel;
			if (match.find("1002") != std::string::npos) return Vendor::Amd;
			return Vendor::Other;
		}

		auto vendor_enabled(const Vendor vendor, const std::string& shown_vendors) -> bool {
			if (vendor == Vendor::Intel) return shown_vendors.find("intel") != std::string::npos;
			if (vendor == Vendor::Amd) return shown_vendors.find("amd") != std::string::npos;
			return false;
		}

		auto read_performance_statistics(io_registry_entry_t accelerator) -> CFRef<CFDictionaryRef> {
			auto property = registry_property(accelerator, CFSTR("PerformanceStatistics"));
			if (property == nullptr or CFGetTypeID(property) != CFDictionaryGetTypeID()) {
				if (property) CFRelease(property);
				return CFRef<CFDictionaryRef>{};
			}
			return CFRef<CFDictionaryRef>{static_cast<CFDictionaryRef>(property)};
		}

		auto read_memory_total(io_registry_entry_t accelerator) -> int64_t {
			for (const auto key : {CFSTR("VRAM,totalMB"), CFSTR("vram,totalMB")}) {
				CFRef<CFTypeRef> value(registry_property(accelerator, key));
				if (not value) value = CFRef<CFTypeRef>{inherited_property(accelerator, key)};
				if (const auto megabytes = integer_value(value.get()); megabytes.has_value() and megabytes.value() > 0)
					return megabytes.value() * 1024 * 1024;
			}

			CFRef<CFTypeRef> bytes(inherited_property(accelerator, CFSTR("ATY,memsize")));
			const auto value = integer_value(bytes.get());
			return value.has_value() and value.value() > 0 ? value.value() : 0;
		}

		auto read_memory_used(CFDictionaryRef stats, const Vendor vendor, const int64_t total) -> int64_t {
			const auto preferred = vendor == Vendor::Intel
				? dictionary_integer(stats, CFSTR("gartUsedBytes"))
				: dictionary_integer(stats, CFSTR("inUseVidMemoryBytes"));
			if (preferred.has_value() and preferred.value() >= 0 and (total <= 0 or preferred.value() <= total))
				return preferred.value();

			const auto fallback = dictionary_integer(stats, CFSTR("inUseSysMemoryBytes"));
			if (fallback.has_value() and fallback.value() >= 0 and (total <= 0 or fallback.value() <= total))
				return fallback.value();
			return 0;
		}

		auto read_utilization(CFDictionaryRef stats) -> std::optional<int64_t> {
			auto value = dictionary_integer(stats, CFSTR("Device Utilization %"));
			if (not value.has_value())
				value = dictionary_integer(stats, CFSTR("GPU Activity(%)"));
			if (not value.has_value()) return std::nullopt;
			return std::clamp(value.value(), 0ll, 100ll);
		}

		auto smc_value(const char *key) -> std::optional<double> {
			return smc ? smc->getValue(key) : std::nullopt;
		}

		void trim_name(std::string& name, const Vendor vendor) {
			const std::string prefix = vendor == Vendor::Intel ? "Intel " : vendor == Vendor::Amd ? "AMD " : "";
			if (not prefix.empty() and name.starts_with(prefix))
				name.erase(0, prefix.size());
		}
	}

	bool init(std::vector<::Gpu::gpu_info>& gpus, const std::string& shown_vendors) {
		if (initialized) return false;

		try {
			smc = std::make_unique<Cpu::SMCConnection>();
		}
		catch (const std::runtime_error& error) {
			Logger::debug("Intel Mac GPU: SMC unavailable: {}", error.what());
		}

		const auto active_gpu = active_gpu_name();
		if (not active_gpu.empty())
			Logger::info("Intel Mac GPU: AppleMuxControl active GPU is {}", active_gpu);

		io_iterator_t iterator = 0;
		auto matching = IOServiceMatching("IOAccelerator");
		if (matching == nullptr
		or IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != kIOReturnSuccess) {
			Logger::warning("Intel Mac GPU: Failed to enumerate IOAccelerator devices");
			return false;
		}

		io_registry_entry_t accelerator = 0;
		while ((accelerator = IOIteratorNext(iterator)) != 0) {
			if (not has_ancestor_named(accelerator, active_gpu)) {
				IOObjectRelease(accelerator);
				continue;
			}

			CFRef<CFTypeRef> primary_match(registry_property(accelerator, CFSTR("IOPCIPrimaryMatch")));
			CFRef<CFTypeRef> pci_match(registry_property(accelerator, CFSTR("IOPCIMatch")));
			const auto match = not string_value(primary_match.get()).empty()
				? string_value(primary_match.get())
				: string_value(pci_match.get());
			const auto vendor = detect_vendor(match);
			if (not vendor_enabled(vendor, shown_vendors)) {
				IOObjectRelease(accelerator);
				continue;
			}

			CFRef<CFTypeRef> model(inherited_property(accelerator, CFSTR("model")));
			auto name = string_value(model.get());
			if (name.empty())
				name = vendor == Vendor::Intel ? "Intel GPU" : vendor == Vendor::Amd ? "AMD GPU" : "Intel Mac GPU";
			trim_name(name, vendor);

			auto stats = read_performance_statistics(accelerator);
			if (not stats) {
				Logger::debug("Intel Mac GPU: {} has no PerformanceStatistics", name);
				IOObjectRelease(accelerator);
				continue;
			}

			const auto gpu_index = gpus.size();
			gpus.emplace_back();
			::Gpu::gpu_names.push_back(name);
			auto& gpu = gpus.back();
			gpu.mem_total = read_memory_total(accelerator);

			const auto has_usage = dictionary_has(stats.get(), CFSTR("Device Utilization %"))
				or dictionary_has(stats.get(), CFSTR("GPU Activity(%)"));
			const auto has_memory_used = dictionary_has(stats.get(), CFSTR("inUseVidMemoryBytes"))
				or dictionary_has(stats.get(), CFSTR("inUseSysMemoryBytes"))
				or dictionary_has(stats.get(), CFSTR("gartUsedBytes"));
			const auto smc_power = vendor == Vendor::Intel ? smc_value("PCPG") : std::nullopt;
			const auto smc_temp = vendor == Vendor::Intel ? smc_value("TCGC") : std::nullopt;
			const auto has_power = dictionary_has(stats.get(), CFSTR("Total Power(W)")) or smc_power.has_value();
			const auto has_temp = dictionary_has(stats.get(), CFSTR("Temperature(C)")) or smc_temp.has_value();

			gpu.supported_functions = {
				.gpu_utilization = has_usage,
				.mem_utilization = gpu.mem_total > 0 and has_memory_used,
				.gpu_clock = dictionary_has(stats.get(), CFSTR("Core Clock(MHz)")),
				.mem_clock = dictionary_has(stats.get(), CFSTR("Memory Clock(MHz)")),
				.pwr_usage = has_power,
				.pwr_state = false,
				.temp_info = has_temp,
				.mem_total = gpu.mem_total > 0,
				.mem_used = has_memory_used,
				.pcie_txrx = false,
				.encoder_utilization = false,
				.decoder_utilization = false,
			};
			if (has_power) {
				gpu.pwr_max_usage = 0;
			}

			Device device;
			device.accelerator = accelerator;
			device.gpu_index = gpu_index;
			device.vendor = vendor;
			device.name = name;
			devices.push_back(std::move(device));
			Logger::info("Intel Mac GPU: Found {} through IOAccelerator", name);
		}
		IOObjectRelease(iterator);

		initialized = not devices.empty();
		if (not initialized) smc.reset();
		return initialized;
	}

	bool collect(std::vector<::Gpu::gpu_info>& gpus) {
		if (not initialized) return false;

		for (const auto& device : devices) {
			if (device.gpu_index >= gpus.size()) continue;
			auto stats = read_performance_statistics(device.accelerator);
			if (not stats) continue;
			auto& gpu = gpus[device.gpu_index];

			if (gpu.supported_functions.gpu_utilization) {
				if (const auto utilization = read_utilization(stats.get()); utilization.has_value())
					gpu.gpu_percent.at("gpu-totals").push_back(utilization.value());
			}

			if (gpu.supported_functions.mem_used) {
				gpu.mem_used = read_memory_used(stats.get(), device.vendor, gpu.mem_total);
				if (gpu.mem_total > 0) {
					const auto percent = std::clamp(
						static_cast<long long>(std::llround(
							static_cast<double>(gpu.mem_used) * 100.0 / static_cast<double>(gpu.mem_total)
						)),
						0ll,
						100ll
					);
					gpu.mem_utilization_percent.push_back(percent);
					gpu.gpu_percent.at("gpu-vram-totals").push_back(percent);
				}
			}

			if (gpu.supported_functions.gpu_clock) {
				if (const auto clock = dictionary_integer(stats.get(), CFSTR("Core Clock(MHz)"));
				    clock.has_value() and clock.value() >= 0)
					gpu.gpu_clock_speed = static_cast<unsigned int>(clock.value());
			}
			if (gpu.supported_functions.mem_clock) {
				if (const auto clock = dictionary_integer(stats.get(), CFSTR("Memory Clock(MHz)"));
				    clock.has_value() and clock.value() >= 0)
					gpu.mem_clock_speed = clock.value();
			}

			if (gpu.supported_functions.pwr_usage) {
				std::optional<double> power;
				if (const auto watts = dictionary_integer(stats.get(), CFSTR("Total Power(W)")); watts.has_value())
					power = static_cast<double>(watts.value());
				else if (device.vendor == Vendor::Intel)
					power = smc_value("PCPG");

				if (power.has_value() and power.value() >= 0) {
					gpu.pwr_usage = static_cast<long long>(std::llround(power.value() * 1000.0));
					if (gpu.pwr_usage > gpu.pwr_max_usage) {
						::Gpu::gpu_pwr_total_max += gpu.pwr_usage - gpu.pwr_max_usage;
						gpu.pwr_max_usage = gpu.pwr_usage;
					}
					const auto power_scale = std::max(1ll, gpu.pwr_max_usage);
					gpu.gpu_percent.at("gpu-pwr-totals").push_back(std::clamp(
						static_cast<long long>(std::llround(
							static_cast<double>(gpu.pwr_usage) * 100.0 / static_cast<double>(power_scale)
						)),
						0ll,
						100ll
					));
				}
			}

			if (gpu.supported_functions.temp_info) {
				std::optional<double> temperature;
				if (const auto value = dictionary_integer(stats.get(), CFSTR("Temperature(C)")); value.has_value())
					temperature = static_cast<double>(value.value());
				else if (device.vendor == Vendor::Intel)
					temperature = smc_value("TCGC");
				if (temperature.has_value() and temperature.value() > 0 and temperature.value() < 150)
					gpu.temp.push_back(static_cast<long long>(std::llround(temperature.value())));
			}
		}
		return true;
	}

	bool shutdown() {
		if (not initialized and devices.empty()) return false;
		devices.clear();
		smc.reset();
		initialized = false;
		return true;
	}
} // namespace IntelMac::Gpu

#elif defined(__APPLE__) && defined(GPU_SUPPORT)

namespace IntelMac::Gpu {
	bool init(std::vector<::Gpu::gpu_info>&, const std::string&) { return false; }
	bool collect(std::vector<::Gpu::gpu_info>&) { return false; }
	bool shutdown() { return false; }
}

#endif

#if defined(__APPLE__) && defined(GPU_SUPPORT)
namespace Gpu::IntelMac {
	bool shutdown() {
		return ::IntelMac::Gpu::shutdown();
	}
}
#endif
