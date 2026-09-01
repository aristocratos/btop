// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "btop_config.hpp"
#include "btop_shared.hpp"

TEST(proc_tree, restores_collapse_for_recreated_process_name_ancestry) {
	Config::set("proc_tree_persist_state", true);
	Config::set("proc_tree_state", std::string {});

	auto before = std::vector<Proc::proc_info> {
		{.pid = 1, .name = "systemd"},
		{.pid = 10, .name = "chromium", .ppid = 1},
		{.pid = 11, .name = "chromium", .ppid = 10},
	};
	Proc::remember_tree_state(before, 11, true);
	EXPECT_FALSE(Config::getS("proc_tree_state").empty());

	auto recreated = std::vector<Proc::proc_info> {
		{.pid = 1, .name = "systemd"},
		{.pid = 50, .name = "chromium", .ppid = 1},
		{.pid = 51, .name = "chromium", .ppid = 50},
		{.pid = 52, .name = "chromium", .ppid = 1},
	};
	Proc::restore_tree_state(recreated);

	EXPECT_TRUE(recreated.at(2).collapsed);
	EXPECT_FALSE(recreated.at(1).collapsed);
	EXPECT_FALSE(recreated.at(3).collapsed);
	Config::set("proc_tree_persist_state", false);
}

TEST(proc_tree, sorts_expanded_branches_by_total_resources_without_aggregating_display) {
	Config::set("proc_aggregate", false);
	auto processes = std::vector<Proc::proc_info> {
		{.pid = 10, .name = "chromium"},
		{.pid = 11, .name = "renderer", .ppid = 10},
		{.pid = 20, .name = "qs"},
	};
	processes.at(0).mem = 86;
	processes.at(1).mem = 939;
	processes.at(2).mem = 239;
	auto tree = std::vector<Proc::tree_proc> {
		{processes.at(0), {}},
		{processes.at(2), {}},
	};
	tree.at(0).children.push_back({processes.at(1), {}});

	int index = 0;
	Proc::tree_sort(tree, "memory", false, false, index, processes.size());

	EXPECT_EQ(tree.at(0).entry.get().pid, 10u);
	EXPECT_EQ(tree.at(0).entry.get().mem, 86u);
	EXPECT_EQ(tree.at(0).tree_mem, 1025u);
	EXPECT_EQ(tree.at(1).entry.get().pid, 20u);

	// A collapsed node already exposes the same branch total in its entry.
	processes.at(0).collapsed = true;
	processes.at(0).mem = 1025;
	index = 0;
	Proc::tree_sort(tree, "memory", false, false, index, processes.size());
	EXPECT_EQ(tree.at(0).tree_mem, 1025u);
}
