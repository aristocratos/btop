// SPDX-License-Identifier: Apache-2.0

#include "osx/intel_mac.hpp"

#include <gtest/gtest.h>

TEST(intel_mac_cpu, selects_package_die_temperature_without_proximity_fallback) {
	// Given Intel SMC temperature capabilities, when package keys are selected,
	// then PECI/die sensors are preferred and TC0P proximity is never presented as package temperature.
	EXPECT_EQ(IntelMac::cpu_package_temperature_keys.at(0), "TC0F");
	EXPECT_EQ(IntelMac::cpu_package_temperature_keys.at(1), "TC0D");
	EXPECT_EQ(IntelMac::cpu_package_temperature_keys.at(2), "TC0E");
	EXPECT_EQ(
		std::ranges::find(IntelMac::cpu_package_temperature_keys, "TC0P"),
		IntelMac::cpu_package_temperature_keys.end()
	);
}

TEST(intel_mac_cpu, titles_the_cpu_with_physical_cores_only) {
	// Given runtime-discovered physical core counts, when the CPU title is built,
	// then it reports cores without presenting logical threads as CPU units.
	EXPECT_EQ(IntelMac::format_cpu_title("Intel CPU", 4), "Intel CPU 4C");
	EXPECT_EQ(IntelMac::format_cpu_title("Intel CPU", 6), "Intel CPU 6C");
}

TEST(intel_mac_cpu, maps_xnu_processor_slots_from_sorted_apic_topology) {
	// Given APIC IDs in device-tree or arbitrary enumeration order, when mapped to XNU processor slots,
	// then adjacent APIC siblings land on the same physical core instead of becoming alternating rows.
	EXPECT_EQ(
		IntelMac::physical_core_map_from_apic_ids({0, 2, 4, 6, 1, 3, 5, 7}, 4),
		(std::vector<std::size_t>{0, 0, 1, 1, 2, 2, 3, 3})
	);
	EXPECT_EQ(
		IntelMac::physical_core_map_from_apic_ids({4, 0, 6, 2, 5, 1, 7, 3}, 4),
		(std::vector<std::size_t>{0, 0, 1, 1, 2, 2, 3, 3})
	);
	EXPECT_TRUE(IntelMac::physical_core_map_from_apic_ids({0, 0, 0, 0}, 2).empty());
}

TEST(intel_mac_cpu, falls_back_to_adjacent_xnu_processor_slots) {
	// Given an Intel Mac where device-tree APIC IDs are unavailable, when the fallback is built,
	// then XNU-adjacent SMT siblings are grouped without a model-specific topology table.
	EXPECT_EQ(
		IntelMac::physical_core_map_from_xnu_order(8, 4),
		(std::vector<std::size_t>{0, 0, 1, 1, 2, 2, 3, 3})
	);
	EXPECT_EQ(
		IntelMac::physical_core_map_from_xnu_order(12, 6),
		(std::vector<std::size_t>{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5})
	);
	EXPECT_TRUE(IntelMac::physical_core_map_from_xnu_order(7, 4).empty());
}

TEST(intel_mac_cpu, maps_other_intel_mac_core_counts_without_model_tables) {
	// Given six-core SMT and non-SMT topologies, when mapped,
	// then runtime counts and APIC IDs alone produce the physical-core labels.
	EXPECT_EQ(
		IntelMac::physical_core_map_from_apic_ids(
			{0, 2, 4, 6, 8, 10, 1, 3, 5, 7, 9, 11},
			6
		),
		(std::vector<std::size_t>{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5})
	);
	EXPECT_EQ(
		IntelMac::physical_core_map_from_apic_ids({0, 2, 4, 6, 8, 10}, 6),
		(std::vector<std::size_t>{0, 1, 2, 3, 4, 5})
	);
}

TEST(intel_mac_cpu, lays_out_four_physical_cores_in_reading_order) {
	// Given a two-column CPU panel, when its column-major drawing slots are assigned,
	// then the visible rows read C0/C1 followed by C2/C3.
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(0, 2, 4), 0);
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(1, 2, 4), 2);
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(2, 2, 4), 1);
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(3, 2, 4), 3);
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(4, 4, 6), 2);
	EXPECT_EQ(IntelMac::physical_core_for_layout_slot(5, 4, 6), 5);
}

TEST(intel_mac_cpu, aggregates_logical_cpu_ticks_into_physical_usage) {
	// Given complementary load on each pair of SMT siblings,
	// when usage is aggregated, then btop exposes four physical-core percentages.
	const std::vector<long long> totals(8, 100);
	const std::vector<long long> idles = {20, 80, 40, 60, 60, 40, 80, 20};

	EXPECT_EQ(
		IntelMac::aggregate_physical_usage(
			totals,
			idles,
			4,
			{0, 0, 1, 1, 2, 2, 3, 3}
		),
		(std::vector<long long>{50, 50, 50, 50})
	);
}

TEST(intel_mac_cpu, handles_zero_tick_intervals_without_dividing_by_zero) {
	// Given a scheduler interval with no tick movement,
	// when usage is aggregated, then every physical core reports zero.
	EXPECT_EQ(
		IntelMac::aggregate_physical_usage({0, 0, 0, 0}, {0, 0, 0, 0}, 4),
		(std::vector<long long>{0, 0, 0, 0})
	);
}

TEST(intel_mac_cpu, parses_the_hardware_frequency_reported_by_powermetrics) {
	// Given package and per-thread powermetrics output, when parsed,
	// then only the package-average hardware frequency is accepted.
	const auto package = IntelMac::parse_powermetrics_cpu_mhz(
		"CPU Average frequency as fraction of nominal: 128.15% (3460.07 Mhz)"
	);
	EXPECT_TRUE(package.has_value());
	EXPECT_DOUBLE_EQ(package.value(), 3460.07);
	EXPECT_FALSE(IntelMac::parse_powermetrics_cpu_mhz(
		"CPU 0 Average frequency as fraction of nominal: 131.00% (3537.00 Mhz)"
	).has_value());
}

TEST(intel_mac_cpu, formats_frequency_in_ghz_without_claiming_a_false_value) {
	// Given a valid hardware sample or no sample, when rendered,
	// then the field carries the GHz unit and never falls back to a load-derived estimate.
	EXPECT_EQ(IntelMac::format_cpu_frequency_ghz(3460.07), "3.46GHz");
	EXPECT_EQ(IntelMac::format_cpu_frequency_ghz(0.0), "N/A GHz");
}

TEST(intel_mac_cpu, expires_frequency_samples_by_the_helper_sampling_interval) {
	// Given the helper's production cadence, when sample age is checked,
	// then three missed samples are tolerated without coupling freshness to btop's display interval.
	const std::time_t now = 100;
	EXPECT_TRUE(IntelMac::cpu_frequency_sample_is_fresh(now, now));
	EXPECT_TRUE(IntelMac::cpu_frequency_sample_is_fresh(now, now - 3));
	EXPECT_FALSE(IntelMac::cpu_frequency_sample_is_fresh(now, now - 4));
	EXPECT_FALSE(IntelMac::cpu_frequency_sample_is_fresh(now, now + 1));
}
