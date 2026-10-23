// SPDX-License-Identifier: Apache-2.0
// Shared GPU memory contribution by ajfero.

#include "btop_shared.hpp"
#include "btop_draw.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

#include <limits>
#include <regex>
#include <gtest/gtest.h>

#ifdef GPU_SUPPORT
namespace {
	Gpu::gpu_info amd_gpu() {
		Gpu::gpu_info gpu;
		gpu.mem_total = 1LL << 30;
		gpu.mem_used = 993LL << 20;
		gpu.shared_mem_total = 48LL << 30;
		gpu.shared_mem_used = 12LL << 30;
		return gpu;
	}

	string visible(const string& text) {
		return std::regex_replace(text, std::regex("\x1b\\[[0-9;]*m"), "");
	}
}

TEST(gpu_shared_memory, keeps_vram_and_shared_ram_separate) {
	const auto gpu = amd_gpu();
	EXPECT_EQ(gpu.shared_mem_percent(), 25);
	EXPECT_EQ(gpu.mem_total, 1LL << 30);
	EXPECT_EQ(gpu.mem_used, 993LL << 20);
}

TEST(gpu_shared_memory, distinguishes_missing_samples_from_zero_usage) {
	auto gpu = amd_gpu();
	gpu.shared_mem_used = 0;
	EXPECT_EQ(gpu.shared_mem_percent(), 0);
	gpu.shared_mem_used.reset();
	EXPECT_FALSE(gpu.shared_mem_percent());
	gpu.shared_mem_used = -1;
	EXPECT_FALSE(gpu.shared_mem_percent());
	gpu.shared_mem_used = 1;
	gpu.shared_mem_total = 0;
	EXPECT_FALSE(gpu.shared_mem_percent());
}

TEST(gpu_shared_memory, clamps_over_budget_samples_without_integer_overflow) {
	auto gpu = amd_gpu();
	gpu.shared_mem_used = 60LL << 30;
	EXPECT_EQ(gpu.shared_mem_percent(), 100);
	gpu.shared_mem_total = std::numeric_limits<long long>::max();
	gpu.shared_mem_used = gpu.shared_mem_total / 2;
	EXPECT_GE(*gpu.shared_mem_percent(), 49);
	EXPECT_LE(*gpu.shared_mem_percent(), 50);
}

TEST(gpu_shared_memory, reserves_rows_only_for_visible_supported_devices) {
	const vector<Gpu::gpu_info> devices {amd_gpu(), Gpu::gpu_info{}, amd_gpu()};
	EXPECT_EQ(Gpu::brief_info_rows(devices, "On", {}, true), 5);
	EXPECT_EQ(Gpu::brief_info_rows(devices, "On", {0}, false), 3);
	EXPECT_EQ(Gpu::brief_info_rows(devices, "Auto", {0}, true), 3);
	EXPECT_EQ(Gpu::brief_info_rows(devices, "Auto", {0, 1, 2}, true), 0);
	EXPECT_EQ(Gpu::brief_info_rows(devices, "Off", {}, true), 0);
	EXPECT_EQ(Gpu::brief_info_rows({}, "On", {}, true), 0);
}

TEST(gpu_shared_memory, failed_sample_keeps_its_row_and_reports_na) {
	Theme::setTheme();
	auto gpu = amd_gpu();
	gpu.shared_mem_used.reset();
	EXPECT_EQ(Gpu::brief_info_rows({gpu}, "On", {}, true), 2);
	const auto line = visible(Draw::gpu_shared_memory(gpu, 80));
	EXPECT_NE(line.find("N/A"), string::npos);
	EXPECT_NE(line.find("48"), string::npos);
	EXPECT_EQ(Tools::ulen(line), 80);
}

TEST(gpu_shared_memory, rendering_fits_narrow_and_wide_panels) {
	Theme::setTheme();
	const auto gpu = amd_gpu();
	for (int width : {1, 10, 17, 27, 39, 40, 60, 100}) {
		const auto line = visible(Draw::gpu_shared_memory(gpu, width));
		EXPECT_EQ(Tools::ulen(line), width) << "width=" << width;
	}
	EXPECT_TRUE(Draw::gpu_shared_memory(gpu, 0).empty());
	EXPECT_TRUE(Draw::gpu_shared_memory(Gpu::gpu_info{}, 80).empty());
	const auto line = visible(Draw::gpu_shared_memory(gpu, 80));
	EXPECT_EQ(line.find("GTT "), 0);
	EXPECT_NE(line.find("12"), string::npos);
	EXPECT_NE(line.find("48"), string::npos);
}
#endif
