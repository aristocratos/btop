// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include <gtest/gtest.h>

#include "btop_tools.hpp"
#ifdef __APPLE__
#include <mach/mach.h>
#include "btop_shared.hpp"
#endif

TEST(tools, string_split) {
	EXPECT_EQ(Tools::ssplit(""), std::vector<std::string> {});
	EXPECT_EQ(Tools::ssplit("foo"), std::vector<std::string> { "foo" });
	{
		auto actual = Tools::ssplit("foo       bar         baz    ");
		auto expected = std::vector<std::string> { "foo", "bar", "baz" };
		EXPECT_EQ(actual, expected);
	}

	{
		auto actual = Tools::ssplit("foobo  oho  barbo  bo  bazbo", 'o');
		auto expected = std::vector<std::string> { "f", "b", "  ", "h", "  barb", "  b", "  bazb" };
		EXPECT_EQ(actual, expected);
	}
}

#ifdef __APPLE__
TEST(memory, activity_monitor_accounting) {
	for (const uint64_t page_size : {4096, 16384}) {
		Mem::mem_info mem;
		vm_statistics64 vm{};
		vm.free_count = 80;
		vm.speculative_count = 20;
		vm.external_page_count = 200;
		vm.purgeable_count = 30;
		const uint64_t total = 1000 * page_size;
		ASSERT_TRUE(mem.set_activity_monitor_memory(vm, HOST_VM_INFO64_COUNT, total, page_size));
		EXPECT_EQ(mem.stats.at("free"), 60 * page_size);
		EXPECT_EQ(mem.stats.at("cached"), 230 * page_size);
		EXPECT_EQ(mem.stats.at("available"), 290 * page_size);
		EXPECT_EQ(mem.stats.at("used"), 710 * page_size);

		EXPECT_FALSE(mem.set_activity_monitor_memory(vm, HOST_VM_INFO64_REV0_COUNT, total, page_size));
		EXPECT_EQ(mem.stats.at("used"), 710 * page_size);
		EXPECT_FALSE(mem.set_activity_monitor_memory(vm, HOST_VM_INFO64_COUNT, total, 0));
		EXPECT_EQ(mem.stats.at("available"), 290 * page_size);

		vm.speculative_count = 90;
		ASSERT_TRUE(mem.set_activity_monitor_memory(vm, HOST_VM_INFO64_COUNT, total, page_size));
		EXPECT_EQ(mem.stats.at("free"), 0);
		EXPECT_EQ(mem.stats.at("used"), 770 * page_size);

		vm.external_page_count = 1100;
		ASSERT_TRUE(mem.set_activity_monitor_memory(vm, HOST_VM_INFO64_COUNT, total, page_size));
		EXPECT_EQ(mem.stats.at("cached"), total);
		EXPECT_EQ(mem.stats.at("available"), total);
		EXPECT_EQ(mem.stats.at("used"), 0);
	}
}
#endif
