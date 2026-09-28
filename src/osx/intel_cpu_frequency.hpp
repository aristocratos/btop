/* Licensed under the Apache License, Version 2.0.
 * See the project's LICENSE file for details.
 *
 * indent = tab
 * tab-size = 4
 */

#pragma once

#if defined(__APPLE__) && defined(__x86_64__)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <vector>
#include <sys/sysctl.h>

namespace Cpu {

	// Intel powermetrics uses XNU's dgPowerStat diagnostic call to read APERF
	// and MPERF. This private version-1 layout was verified on macOS 15.8:
	// https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/i386/Diagnostics.c
	// Restrict it to native Intel macOS 15; other systems keep the sysctl fallback.
	class IntelCpuFrequency {
		static constexpr std::size_t header_words = 0x1a0 / sizeof(std::uint64_t);
		static constexpr std::size_t core_words = 0x130 / sizeof(std::uint64_t);
		static constexpr std::size_t cpu_count_word = 0x198 / sizeof(std::uint64_t);

		std::mutex mutex;
		std::vector<std::uint64_t> previous, current;
		std::uint64_t reference_hz = 0;
		int max_cpus = 0;
		bool available = false, has_baseline = false;

		template <typename T>
		static bool read_sysctl(const char* name, T& value) {
			std::size_t size = sizeof(value);
			return sysctlbyname(name, &value, &size, nullptr, 0) == 0 and size == sizeof(value);
		}

		bool take_snapshot() {
			current[0] = ~std::uint64_t{0};
			current[cpu_count_word] = ~std::uint64_t{0};

			// Use the diagnostic syscall class directly, not libc's BSD syscall().
			// RSI points to writable storage; this call takes no buffer-length argument.
			std::uint64_t result = 0x04000000;
			std::uint64_t selector = 17; // dgPowerStat
			void* buffer = current.data();
			__asm__ volatile("syscall"
				: "+a"(result), "+D"(selector), "+S"(buffer)
				:
				: "rcx", "r11", "cc", "memory");

			const auto ncpus = static_cast<std::uint32_t>(current[cpu_count_word]);
			return result == 1 and current[0] == 1 and ncpus > 0
				and ncpus <= static_cast<std::uint32_t>(max_cpus);
		}

	public:
		IntelCpuFrequency() {
			char release[64]{};
			std::size_t size = sizeof(release);
			if (sysctlbyname("kern.osrelease", release, &size, nullptr, 0) != 0
				or size < 3 or std::strncmp(release, "24.", 3) != 0)
				return;

			int translated = 0;
			if (read_sysctl("sysctl.proc_translated", translated) and translated != 0)
				return;

			char vendor[32]{};
			size = sizeof(vendor);
			if (sysctlbyname("machdep.cpu.vendor", vendor, &size, nullptr, 0) != 0
				or std::strncmp(vendor, "GenuineIntel", sizeof(vendor)) != 0)
				return;

			if (not read_sysctl("hw.logicalcpu_max", max_cpus) or max_cpus < 1 or max_cpus > 4096
				or not read_sysctl("machdep.tsc.frequency", reference_hz) or reference_hz == 0)
				return;

			// Leave room beyond the verified layout, as the kernel accepts no size.
			// This does not make the private ABI safe to use on unverified releases.
			const auto words = (65536 + static_cast<std::size_t>(max_cpus) * 1024) / sizeof(std::uint64_t);
			previous.resize(words);
			current.resize(words);
			available = true;
		}

		// Called at btop's refresh interval: no sleeping or subprocess is needed.
		// Empty means warm-up, reset, no active cycles, or an unavailable interface.
		std::optional<double> sample_hz() {
			// get_cpuHz() is also queried by the layout code, outside CPU collection.
			const std::scoped_lock lock(mutex);
			if (not available)
				return std::nullopt;
			if (not take_snapshot()) {
				available = false;
				return std::nullopt;
			}

			const auto ncpus = static_cast<std::uint32_t>(current[cpu_count_word]);
			bool valid = has_baseline
				and ncpus == static_cast<std::uint32_t>(previous[cpu_count_word]);
			long double aperf = 0, mperf = 0;
			for (std::uint32_t cpu = 0; valid and cpu < ncpus; ++cpu) {
				const auto index = header_words + static_cast<std::size_t>(cpu) * core_words;
				const auto a = current[index], m = current[index + 1];
				if (a < previous[index] or m < previous[index + 1]) {
					// For example, a counter reset after sleep: take a new baseline.
					valid = false;
					break;
				}
				aperf += a - previous[index];
				mperf += m - previous[index + 1];
			}

			previous.swap(current);
			has_baseline = true;
			if (not valid or mperf == 0 or aperf == 0)
				return std::nullopt;

			// Reference-cycle-weighted active frequency, including Turbo Boost.
			return static_cast<double>(reference_hz * aperf / mperf);
		}
	};

} // namespace Cpu

#endif
