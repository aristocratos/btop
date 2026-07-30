// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#if defined(GPU_SUPPORT)
#include "../btop_shared.hpp"
#endif

namespace IntelMac {

	inline constexpr auto cpu_frequency_path = "/var/run/btop-cpu-frequency-mhz";
	inline constexpr unsigned int cpu_frequency_sample_interval_ms = 1000;
	inline constexpr unsigned int cpu_frequency_stale_intervals = 3;
	inline constexpr std::array<std::string_view, 3> cpu_package_temperature_keys = {
		"TC0F",
		"TC0D",
		"TC0E",
	};

	inline auto format_cpu_title(
		const std::string_view name,
		const std::size_t physical_count
	) -> std::string {
		return std::string{name} + " " + std::to_string(physical_count) + "C";
	}

	inline auto physical_core_map_from_apic_ids(
		std::vector<unsigned int> apic_ids,
		const std::size_t physical_count
	) -> std::vector<std::size_t> {
		if (physical_count == 0 or apic_ids.empty() or apic_ids.size() % physical_count != 0)
			return {};

		const auto threads_per_core = apic_ids.size() / physical_count;
		if ((threads_per_core & (threads_per_core - 1)) != 0) return {};

		// XNU numbers processor_array slots in ascending local APIC-ID order,
		// regardless of the CPU0..CPUn device-tree node order.
		std::ranges::sort(apic_ids);
		if (std::ranges::unique(apic_ids).begin() != apic_ids.end()) return {};

		// Intel reserves the low APIC-ID bits for SMT siblings. Compress the
		// remaining, potentially sparse, core IDs into btop's C0..Cn labels.
		std::vector<unsigned int> core_ids;
		core_ids.reserve(apic_ids.size());
		for (const auto apic_id : apic_ids)
			core_ids.push_back(apic_id / threads_per_core);

		auto unique_core_ids = core_ids;
		std::ranges::sort(unique_core_ids);
		const auto duplicate_start = std::ranges::unique(unique_core_ids).begin();
		unique_core_ids.erase(duplicate_start, unique_core_ids.end());
		if (unique_core_ids.size() != physical_count) return {};

		std::vector<std::size_t> mapping;
		mapping.reserve(core_ids.size());
		std::vector<std::size_t> thread_counts(physical_count, 0);
		for (const auto core_id : core_ids) {
			const auto physical = static_cast<std::size_t>(
				std::ranges::lower_bound(unique_core_ids, core_id) - unique_core_ids.begin()
			);
			mapping.push_back(physical);
			++thread_counts[physical];
		}
		if (std::ranges::any_of(thread_counts, [threads_per_core](const auto count) {
			return count != threads_per_core;
		})) return {};
		return mapping;
	}

	inline auto physical_core_map_from_xnu_order(
		const std::size_t logical_count,
		const std::size_t physical_count
	) -> std::vector<std::size_t> {
		if (physical_count == 0 or logical_count == 0 or logical_count % physical_count != 0)
			return {};

		const auto threads_per_core = logical_count / physical_count;
		std::vector<std::size_t> mapping(logical_count);
		for (std::size_t logical = 0; logical < logical_count; ++logical)
			mapping[logical] = logical / threads_per_core;
		return mapping;
	}

	inline auto physical_core_for_layout_slot(
		const std::size_t slot,
		const std::size_t column_count,
		const std::size_t physical_count
	) -> std::size_t {
		if (column_count == 0 or physical_count == 0) return 0;
		const auto row_count = (physical_count + column_count - 1) / column_count;
		const auto populated_columns = (physical_count + row_count - 1) / row_count;
		return (slot % row_count) * populated_columns + slot / row_count;
	}

	inline auto aggregate_physical_usage(
		const std::vector<long long>& total_deltas,
		const std::vector<long long>& idle_deltas,
		const std::size_t physical_count,
		const std::vector<std::size_t>& logical_to_physical = {}
	) -> std::vector<long long> {
		if (physical_count == 0) return {};

		std::vector<long long> totals(physical_count, 0);
		std::vector<long long> idles(physical_count, 0);
		const auto count = std::min(total_deltas.size(), idle_deltas.size());
		for (std::size_t logical = 0; logical < count; ++logical) {
			const auto physical = logical < logical_to_physical.size()
				? logical_to_physical[logical]
				: logical % physical_count;
			if (physical >= physical_count) continue;
			totals[physical] += std::max(0ll, total_deltas[logical]);
			idles[physical] += std::max(0ll, idle_deltas[logical]);
		}

		std::vector<long long> result(physical_count, 0);
		for (std::size_t physical = 0; physical < physical_count; ++physical) {
			if (totals[physical] > 0) {
				result[physical] = std::clamp(
					static_cast<long long>(std::llround(
						static_cast<double>(totals[physical] - std::min(totals[physical], idles[physical]))
						* 100.0 / static_cast<double>(totals[physical])
					)),
					0ll,
					100ll
				);
			}
		}
		return result;
	}

	inline auto parse_powermetrics_cpu_mhz(const std::string_view line) -> std::optional<double> {
		constexpr std::string_view prefix = "CPU Average frequency as fraction of nominal:";
		const auto prefix_position = line.find(prefix);
		if (prefix_position == std::string_view::npos) return std::nullopt;

		const auto value_start = line.find('(', prefix_position + prefix.size());
		const auto value_end = value_start == std::string_view::npos
			? std::string_view::npos
			: line.find(" Mhz)", value_start + 1);
		if (value_start == std::string_view::npos or value_end == std::string_view::npos)
			return std::nullopt;

		const std::string value(line.substr(value_start + 1, value_end - value_start - 1));
		char* parsed_end = nullptr;
		const auto mhz = std::strtod(value.c_str(), &parsed_end);
		if (parsed_end != value.c_str() + value.size() or not std::isfinite(mhz) or mhz <= 0.0)
			return std::nullopt;
		return mhz;
	}

	inline auto format_cpu_frequency_ghz(const double mhz) -> std::string {
		if (not std::isfinite(mhz) or mhz <= 0.0) return "N/A GHz";
		char output[16]{};
		std::snprintf(output, sizeof(output), "%.2fGHz", mhz / 1000.0);
		return output;
	}

	inline auto cpu_frequency_sample_is_fresh(
		const std::time_t now,
		const std::time_t modified
	) -> bool {
		constexpr auto max_age_seconds =
			(cpu_frequency_sample_interval_ms * cpu_frequency_stale_intervals + 999) / 1000;
		return modified <= now and now - modified <= max_age_seconds;
	}

#if defined(GPU_SUPPORT)
	namespace Gpu {
		bool init(std::vector<::Gpu::gpu_info>& gpus, const std::string& shown_vendors);
		bool collect(std::vector<::Gpu::gpu_info>& gpus);
		bool shutdown();
	}
#endif

} // namespace IntelMac
