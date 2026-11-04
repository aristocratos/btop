// SPDX-License-Identifier: Apache-2.0

#include <clocale>
#include <vector>

#include <gtest/gtest.h>

#include "btop_tools.hpp"

TEST(tools, resize_wide) {
	struct LocaleRestore {
		std::string previous = std::setlocale(LC_CTYPE, nullptr);
		~LocaleRestore() { std::setlocale(LC_CTYPE, previous.c_str()); }
	} locale_restore;
	if (std::setlocale(LC_CTYPE, "C.UTF-8") == nullptr and std::setlocale(LC_CTYPE, "en_US.UTF-8") == nullptr)
		GTEST_SKIP() << "No UTF-8 locale available";

	EXPECT_EQ(Tools::uresize("中文测试", 4, true), "中文");
	EXPECT_EQ(Tools::uresize("中文", 4, true), "中文");
	EXPECT_EQ(Tools::uresize("plain", 8, true), "plain");
	EXPECT_EQ(Tools::uresize("中", 1, true), "");
}

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
