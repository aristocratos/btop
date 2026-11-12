// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "btop_shared.hpp"

TEST(proc, strip_cmd_empty_pattern) {
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop --utf-force", ""), "/usr/bin/btop --utf-force");
	EXPECT_EQ(Proc::strip_cmd("", ""), "");
}

TEST(proc, strip_cmd_nix_store) {
	const std::string nix_pattern = "/nix/store/[^/]+/";
	EXPECT_EQ(
		Proc::strip_cmd("/nix/store/8g4nb1p0gxrsbh7mv1qkcnm0a8ig7bhh-firefox-130.0/bin/.firefox-wrapped --name firefox", nix_pattern),
		"bin/.firefox-wrapped --name firefox"
	);
	//? All matches are removed, not only the first
	EXPECT_EQ(
		Proc::strip_cmd("/nix/store/aaa-bash-5.2/bin/bash /nix/store/bbb-script/bin/run.sh", nix_pattern),
		"bin/bash bin/run.sh"
	);
	//? Commands without a match are left unchanged
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", nix_pattern), "/usr/bin/btop");
}

TEST(proc, strip_cmd_invalid_pattern) {
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", "(["), "/usr/bin/btop");
}

TEST(proc, strip_cmd_pattern_change) {
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", "/usr/"), "bin/btop");
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", "/usr/bin/"), "btop");
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", "(["), "/usr/bin/btop");
	EXPECT_EQ(Proc::strip_cmd("/usr/bin/btop", "/usr/"), "bin/btop");
}
