// SPDX-License-Identifier: Apache-2.0

#include <clocale>
#include <vector>

#include <gtest/gtest.h>

#include "btop_tools.hpp"

TEST(tools, justify_wide_chars) {
	// Wide (double-width) characters need a UTF-8 locale for mbstowcs() to
	// decode them; skip on platforms where no such locale is installed.
	if (std::setlocale(LC_ALL, "C.UTF-8") == nullptr and std::setlocale(LC_ALL, "en_US.UTF-8") == nullptr) {
		GTEST_SKIP() << "No UTF-8 locale available to test wide-character justification";
	}

	// "你好" is two CJK characters: 2 codepoints, but 4 display columns wide.
	const std::string wide_str = "\xe4\xbd\xa0\xe5\xa5\xbd";
	ASSERT_EQ(Tools::ulen(wide_str, true), 4u);

	// ljust/rjust/cjust with wide=true must pad using the *display* width,
	// not the codepoint count, so the result is exactly <x> columns wide.
	EXPECT_EQ(Tools::ulen(Tools::ljust(wide_str, 10, true, true), true), 10u);
	EXPECT_EQ(Tools::ulen(Tools::rjust(wide_str, 10, true, true), true), 10u);
	EXPECT_EQ(Tools::ulen(Tools::cjust(wide_str, 10, true, true), true), 10u);

	std::setlocale(LC_ALL, "C");
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
