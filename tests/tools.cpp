// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include <gtest/gtest.h>

#include "btop_tools.hpp"

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

TEST(tools, command_basename_offset) {
	EXPECT_EQ(Tools::command_basename_offset(""), 0);
	EXPECT_EQ(Tools::command_basename_offset("firefox"), 0);
	EXPECT_EQ(Tools::command_basename_offset("/"), 0);
	EXPECT_EQ(Tools::command_basename_offset("/usr/bin/"), 0);
	EXPECT_EQ(Tools::command_basename_offset("./firefox"), 2);
	EXPECT_EQ(Tools::command_basename_offset("../bin/firefox"), 7);
	EXPECT_EQ(Tools::command_basename_offset("/usr/bin/firefox"), 9);
}

TEST(tools, command_basename_preserves_arguments) {
	const std::vector<std::pair<string_view, string_view>> cases = {
		{"/nix/store/hash-firefox/bin/firefox", "firefox"},
		{"/path with spaces/my program", "my program"},
		{"/路徑/程式", "程式"},
		{"firefox", "firefox"},
		{"", ""},
		{"/usr/bin/", "/usr/bin/"},
	};
	for (const auto& [executable, basename] : cases) {
		const auto command = fmt::format("{} --profile /home/user/profile --url https://example.org/a/b", executable);
		const auto offset = Tools::command_basename_offset(executable);
		EXPECT_EQ(command.substr(offset), fmt::format("{} --profile /home/user/profile --url https://example.org/a/b", basename));
	}
}
