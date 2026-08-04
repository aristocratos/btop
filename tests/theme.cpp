// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "btop_tools.hpp"

//? Regression test for the static initialization order fiasco in issue #1748.
//? Fx::reset lives in btop_theme.cpp and used to be initialized from Fx::reset_base,
//? which is defined in btop_tools.cpp. Ordering between translation units is
//? unspecified, so on some toolchains reset was constructed from an empty
//? reset_base and btop crashed before main().
TEST(theme, reset_matches_reset_base) {
	//? Both are read here after static initialization has finished, so this only
	//? catches a reset that was left empty or truncated, not the ordering itself.
	EXPECT_FALSE(Fx::reset.empty());
	EXPECT_FALSE(Fx::reset_base.empty());
	EXPECT_EQ(Fx::reset, Fx::reset_base);
	EXPECT_EQ(Fx::reset, Fx::e + "0m");
}
