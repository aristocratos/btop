// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

#ifdef GPU_SUPPORT

namespace {

struct GpuConfigState {
	int count = Gpu::count;
	vector<string> boxes = Config::current_boxes;
	vector<string> presets = Config::preset_list;
	string shown_boxes = Config::strings.at("shown_boxes");
	int term_width = Term::width.load();
	int term_height = Term::height.load();

	~GpuConfigState() {
		Gpu::count = count;
		Config::current_boxes = boxes;
		Config::preset_list = presets;
		Config::strings.at("shown_boxes") = shown_boxes;
		Term::width = term_width;
		Term::height = term_height;
	}
};

}

TEST(gpu_boxes, parse_gpu_box_index) {
	const auto gpu0 = Config::gpu_box_index("gpu0");
	const auto gpu7 = Config::gpu_box_index("gpu7");
	const auto gpu10 = Config::gpu_box_index("gpu10");

	ASSERT_TRUE(gpu0.has_value());
	ASSERT_TRUE(gpu7.has_value());
	ASSERT_TRUE(gpu10.has_value());
	EXPECT_EQ(*gpu0, 0u);
	EXPECT_EQ(*gpu7, 7u);
	EXPECT_EQ(*gpu10, 10u);

	EXPECT_FALSE(Config::gpu_box_index("gpu").has_value());
	EXPECT_FALSE(Config::gpu_box_index("gpu-1").has_value());
	EXPECT_FALSE(Config::gpu_box_index("gpu1x").has_value());
	EXPECT_FALSE(Config::gpu_box_index("cpu").has_value());
}

TEST(gpu_boxes, set_boxes_accepts_dynamic_gpu_indices) {
	GpuConfigState state;
	Gpu::count = 8;

	EXPECT_TRUE(Config::set_boxes("cpu gpu6 gpu7"));
	EXPECT_EQ(Config::current_boxes, (vector<string>{"cpu", "gpu6", "gpu7"}));

	EXPECT_FALSE(Config::set_boxes("gpu8"));
	EXPECT_FALSE(Config::set_boxes("gpu0 gpu1 gpu2 gpu3 gpu4 gpu5 gpu6"));
}

TEST(gpu_boxes, presets_reject_more_than_max_gpu_panels) {
	GpuConfigState state;

	EXPECT_TRUE(Config::presetsValid(
		"gpu0:0:default,gpu1:0:default,gpu2:0:default,gpu3:0:default,gpu4:0:default,gpu5:0:default"));
	EXPECT_FALSE(Config::presetsValid(
		"gpu0:0:default,gpu1:0:default,gpu2:0:default,gpu3:0:default,gpu4:0:default,gpu5:0:default,gpu6:0:default"));
}

TEST(gpu_boxes, switch_gpu_box_wraps_and_skips_visible_gpus) {
	GpuConfigState state;
	Gpu::count = 8;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(Config::set_boxes("cpu gpu0 gpu1 gpu2"));
	Config::set("shown_boxes", string{"cpu gpu0 gpu1 gpu2"});

	EXPECT_TRUE(Config::switch_gpu_box(0, -1));
	EXPECT_EQ(Config::current_boxes, (vector<string>{"cpu", "gpu7", "gpu1", "gpu2"}));
	EXPECT_EQ(Config::getS("shown_boxes"), "cpu gpu7 gpu1 gpu2");

	EXPECT_TRUE(Config::switch_gpu_box(1, 1));
	EXPECT_EQ(Config::current_boxes, (vector<string>{"cpu", "gpu7", "gpu3", "gpu2"}));
	EXPECT_EQ(Config::getS("shown_boxes"), "cpu gpu7 gpu3 gpu2");
}

TEST(gpu_boxes, switch_gpu_box_returns_false_when_all_other_gpus_are_visible) {
	GpuConfigState state;
	Gpu::count = 3;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(Config::set_boxes("gpu0 gpu1 gpu2"));
	Config::set("shown_boxes", string{"gpu0 gpu1 gpu2"});

	EXPECT_FALSE(Config::switch_gpu_box(0, 1));
	EXPECT_EQ(Config::current_boxes, (vector<string>{"gpu0", "gpu1", "gpu2"}));
	EXPECT_EQ(Config::getS("shown_boxes"), "gpu0 gpu1 gpu2");
}

#endif
