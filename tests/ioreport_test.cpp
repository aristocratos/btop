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
*/

#include <gtest/gtest.h>
#include "../src/osx/ioreport.hpp"

// Test IOReport initialization and cleanup
TEST(IOReportTest, InitAndCleanup) {
	// Ensure clean state
	IOReport::cleanup();

	bool available = IOReport::init();

	if (available) {
		EXPECT_TRUE(IOReport::is_available());
		IOReport::cleanup();
		EXPECT_FALSE(IOReport::is_available());
	} else {
		// init() failed (not on Apple Silicon or missing privileges)
		EXPECT_FALSE(IOReport::is_available());
	}
}

// Test that double init is safe
TEST(IOReportTest, DoubleInitIsSafe) {
	IOReport::cleanup();

	bool first = IOReport::init();
	bool second = IOReport::init();

	// Both calls should return the same result
	EXPECT_EQ(first, second);
	EXPECT_EQ(first, IOReport::is_available());

	IOReport::cleanup();
}

// Test that double cleanup is safe
TEST(IOReportTest, DoubleCleanupIsSafe) {
	IOReport::cleanup();
	IOReport::init();

	// Should not crash or cause issues
	IOReport::cleanup();
	IOReport::cleanup();

	EXPECT_FALSE(IOReport::is_available());
}

// Test frequency values are sensible when available
TEST(IOReportTest, FrequencyValuesAreSensible) {
	IOReport::cleanup();

	if (!IOReport::init()) {
		GTEST_SKIP() << "IOReport not available on this system";
	}

	auto freqs = IOReport::get_cpu_frequencies();
	uint32_t e_freq = freqs.first;
	uint32_t p_freq = freqs.second;

	// Frequencies should either be 0 (no sample yet) or within reasonable bounds
	// Apple Silicon CPUs run between ~600 MHz and ~4500 MHz
	constexpr uint32_t MIN_FREQ = 500;
	constexpr uint32_t MAX_FREQ = 9000;

	if (e_freq > 0) {
		EXPECT_GE(e_freq, MIN_FREQ) << "E-cluster frequency too low: " << e_freq << " MHz";
		EXPECT_LE(e_freq, MAX_FREQ) << "E-cluster frequency too high: " << e_freq << " MHz";
	}

	if (p_freq > 0) {
		EXPECT_GE(p_freq, MIN_FREQ) << "P-cluster frequency too low: " << p_freq << " MHz";
		EXPECT_LE(p_freq, MAX_FREQ) << "P-cluster frequency too high: " << p_freq << " MHz";
	}

	IOReport::cleanup();
}

// Test CPU Frequency fetch when not available
TEST(IOReportTest, GetCpuFrequenciesWhenNotAvailable) {
	// Ensure cleanup
	IOReport::cleanup();
	
	auto freqs = IOReport::get_cpu_frequencies();
	EXPECT_EQ(freqs.first, 0u);
	EXPECT_EQ(freqs.second, 0u);
}
