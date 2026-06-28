// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

#ifdef GPU_SUPPORT

namespace {

struct GpuConfigState {
	int count = Gpu::count;
	vector<size_t> panel_slots = Config::current_gpu_panel_slots;
	vector<string> boxes = Config::current_boxes;
	vector<string> presets = Config::preset_list;
	string shown_boxes = Config::strings.at("shown_boxes");
	int term_width = Term::width.load();
	int term_height = Term::height.load();

	~GpuConfigState() {
		Gpu::count = count;
		Config::current_gpu_panel_slots = panel_slots;
		Config::current_boxes = boxes;
		Config::preset_list = presets;
		Config::strings.at("shown_boxes") = shown_boxes;
		Term::width = term_width;
		Term::height = term_height;
	}
};

testing::AssertionResult set_gpu_boxes(const string& boxes) {
	if (not Config::set_boxes(boxes)) {
		return testing::AssertionFailure() << "Config::set_boxes(\"" << boxes << "\") failed";
	}
	Config::set("shown_boxes", boxes);
	return testing::AssertionSuccess();
}

void expect_gpu_state(const vector<string>& boxes, const vector<size_t>& slots, const string& shown_boxes) {
	EXPECT_EQ(Config::current_boxes, boxes);
	for (size_t i = 0; i < slots.size(); ++i) {
		const auto slot = Config::gpu_panel_slot(i);
		ASSERT_TRUE(slot.has_value()) << "missing GPU panel slot " << i;
		EXPECT_EQ(*slot, slots[i]) << "GPU panel slot " << i;
	}
	EXPECT_FALSE(Config::gpu_panel_slot(slots.size()).has_value());
	EXPECT_EQ(Config::getS("shown_boxes"), shown_boxes);
}

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

TEST(gpu_boxes, set_boxes_validates_dynamic_gpu_panels) {
	GpuConfigState state;
	Gpu::count = 8;

	ASSERT_TRUE(set_gpu_boxes("cpu gpu6 gpu7"));
	expect_gpu_state({"cpu", "gpu6", "gpu7"}, {0, 1}, "cpu gpu6 gpu7");

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

	ASSERT_TRUE(set_gpu_boxes("cpu gpu0 gpu1 gpu2"));

	EXPECT_TRUE(Config::switch_gpu_box(0, -1));
	expect_gpu_state({"cpu", "gpu7", "gpu1", "gpu2"}, {0, 1, 2}, "cpu gpu7 gpu1 gpu2");

	EXPECT_TRUE(Config::switch_gpu_box(1, 1));
	expect_gpu_state({"cpu", "gpu7", "gpu3", "gpu2"}, {0, 1, 2}, "cpu gpu7 gpu3 gpu2");
}

TEST(gpu_boxes, switch_gpu_box_returns_false_when_all_other_gpus_are_visible) {
	GpuConfigState state;
	Gpu::count = 3;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(set_gpu_boxes("gpu0 gpu1 gpu2"));

	EXPECT_FALSE(Config::switch_gpu_box(0, 1));
	expect_gpu_state({"gpu0", "gpu1", "gpu2"}, {0, 1, 2}, "gpu0 gpu1 gpu2");
}

TEST(gpu_boxes, toggle_gpu_box_closes_slot_after_target_switch) {
	GpuConfigState state;
	Gpu::count = 8;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(set_gpu_boxes("gpu0 gpu1"));
	expect_gpu_state({"gpu0", "gpu1"}, {0, 1}, "gpu0 gpu1");

	ASSERT_TRUE(Config::switch_gpu_box(1, -1));
	expect_gpu_state({"gpu0", "gpu7"}, {0, 1}, "gpu0 gpu7");

	EXPECT_TRUE(Config::toggle_gpu_box(1));
	expect_gpu_state({"gpu0"}, {0}, "gpu0");
}

TEST(gpu_boxes, toggle_gpu_box_opens_requested_slot) {
	GpuConfigState state;
	Gpu::count = 8;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(set_gpu_boxes("gpu0"));
	expect_gpu_state({"gpu0"}, {0}, "gpu0");

	EXPECT_TRUE(Config::toggle_gpu_box(2));
	expect_gpu_state({"gpu0", "gpu2"}, {0, 2}, "gpu0 gpu2");

	EXPECT_TRUE(Config::toggle_gpu_box(1));
	expect_gpu_state({"gpu0", "gpu1", "gpu2"}, {0, 1, 2}, "gpu0 gpu1 gpu2");
}

TEST(gpu_boxes, toggle_gpu_box_keeps_slot_order_when_opened_out_of_order) {
	GpuConfigState state;
	Gpu::count = 8;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(set_gpu_boxes(""));

	EXPECT_TRUE(Config::toggle_gpu_box(2));
	expect_gpu_state({"gpu2"}, {2}, "gpu2");

	EXPECT_TRUE(Config::toggle_gpu_box(0));
	expect_gpu_state({"gpu0", "gpu2"}, {0, 2}, "gpu0 gpu2");
}

TEST(gpu_boxes, toggle_gpu_box_removes_the_box_for_the_requested_slot) {
	GpuConfigState state;
	Gpu::count = 8;
	Term::width = 200;
	Term::height = 80;

	ASSERT_TRUE(set_gpu_boxes("gpu0 gpu1 gpu2"));
	expect_gpu_state({"gpu0", "gpu1", "gpu2"}, {0, 1, 2}, "gpu0 gpu1 gpu2");

	EXPECT_TRUE(Config::toggle_gpu_box(1));
	expect_gpu_state({"gpu0", "gpu2"}, {0, 2}, "gpu0 gpu2");
}

#endif
