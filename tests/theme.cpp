// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "btop_theme.hpp"

//* Test rgb_to_ansi16 color conversion
TEST(theme, rgb_to_ansi16_pure_colors) {
	//? Test pure red (foreground)
	EXPECT_EQ(Theme::rgb_to_ansi16(255, 0, 0, "fg"), "\x1b[91m");  // Bright red
	EXPECT_EQ(Theme::rgb_to_ansi16(128, 0, 0, "fg"), "\x1b[31m");  // Dark red

	//? Test pure green (foreground)
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 255, 0, "fg"), "\x1b[92m");  // Bright green
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 128, 0, "fg"), "\x1b[32m");  // Dark green

	//? Test pure blue (foreground)
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 0, 255, "fg"), "\x1b[94m");  // Bright blue
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 0, 128, "fg"), "\x1b[34m");  // Dark blue
}

TEST(theme, rgb_to_ansi16_mixed_colors) {
	//? Test yellow (red + green)
	EXPECT_EQ(Theme::rgb_to_ansi16(255, 255, 0, "fg"), "\x1b[93m");  // Bright yellow
	EXPECT_EQ(Theme::rgb_to_ansi16(128, 128, 0, "fg"), "\x1b[33m");  // Dark yellow

	//? Test cyan (green + blue)
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 255, 255, "fg"), "\x1b[96m");  // Bright cyan
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 128, 128, "fg"), "\x1b[36m");  // Dark cyan

	//? Test magenta (red + blue)
	EXPECT_EQ(Theme::rgb_to_ansi16(255, 0, 255, "fg"), "\x1b[95m");  // Bright magenta
	EXPECT_EQ(Theme::rgb_to_ansi16(128, 0, 128, "fg"), "\x1b[35m");  // Dark magenta
}

TEST(theme, rgb_to_ansi16_grayscale) {
	//? Test black
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 0, 0, "fg"), "\x1b[30m");  // Black

	//? Test white/gray
	EXPECT_EQ(Theme::rgb_to_ansi16(255, 255, 255, "fg"), "\x1b[97m");  // Bright white

	//? Test gray tones (low saturation)
	EXPECT_EQ(Theme::rgb_to_ansi16(128, 128, 128, "fg"), "\x1b[90m");  // Grey (color 8)
	EXPECT_EQ(Theme::rgb_to_ansi16(64, 64, 64, "fg"), "\x1b[30m");     // Black (color 0) - closer to 0,0,0
}

TEST(theme, rgb_to_ansi16_background_colors) {
	//? Test background color codes (40-47 for dark, 100-107 for bright)
	EXPECT_EQ(Theme::rgb_to_ansi16(255, 0, 0, "bg"), "\x1b[101m");  // Bright red bg
	EXPECT_EQ(Theme::rgb_to_ansi16(128, 0, 0, "bg"), "\x1b[41m");   // Dark red bg
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 255, 0, "bg"), "\x1b[102m");  // Bright green bg
	EXPECT_EQ(Theme::rgb_to_ansi16(0, 0, 255, "bg"), "\x1b[104m");  // Bright blue bg
}

TEST(theme, rgb_to_ansi16_unified_borders) {
	//? Test the specific use case from issue #1384:
	//? Same color (#556d59) should map to same ANSI code
	const std::string color1 = Theme::rgb_to_ansi16(85, 109, 89, "fg");
	const std::string color2 = Theme::rgb_to_ansi16(85, 109, 89, "fg");
	const std::string color3 = Theme::rgb_to_ansi16(85, 109, 89, "fg");
	const std::string color4 = Theme::rgb_to_ansi16(85, 109, 89, "fg");

	//? All four should be identical
	EXPECT_EQ(color1, color2);
	EXPECT_EQ(color2, color3);
	EXPECT_EQ(color3, color4);

	//? The color #556d59 is a muted green/gray - Euclidean distance maps to Grey
	EXPECT_EQ(color1, "\x1b[90m");  // Grey (color 8) - closest match
}

TEST(theme, rgb_to_ansi16_nord_theme_colors) {
	//? Test colors from nord theme (used in test_unified_borders.theme)
	//? #4C566A (76,86,106) -> Grey (color 8) via Euclidean distance
	EXPECT_EQ(Theme::rgb_to_ansi16(76, 86, 106, "fg"), "\x1b[90m");  // Grey

	//? #81A1C1 (129,161,193) -> Silver (color 7) via Euclidean distance
	EXPECT_EQ(Theme::rgb_to_ansi16(129, 161, 193, "fg"), "\x1b[37m");  // Silver

	//? #88C0D0 (136,192,208) -> Silver (color 7) via Euclidean distance
	EXPECT_EQ(Theme::rgb_to_ansi16(136, 192, 208, "fg"), "\x1b[37m");  // Silver
}

TEST(theme, hex_to_color_basic) {
	//? Test basic hex color conversion (sanity check for existing functionality)
	auto red = Theme::hex_to_color("#ff0000", false, "fg");
	EXPECT_FALSE(red.empty());
	EXPECT_TRUE(red.find("38;2;255;0;0") != std::string::npos);

	//? Test grayscale hex
	auto gray = Theme::hex_to_color("#80", false, "fg");
	EXPECT_FALSE(gray.empty());
	EXPECT_TRUE(gray.find("38;2;128;128;128") != std::string::npos);
}

TEST(theme, dec_to_color_basic) {
	//? Test RGB decimal to color conversion (sanity check for existing functionality)
	auto green = Theme::dec_to_color(0, 255, 0, false, "fg");
	EXPECT_FALSE(green.empty());
	EXPECT_TRUE(green.find("38;2;0;255;0") != std::string::npos);

	//? Test with 256-color mode
	auto blue_256 = Theme::dec_to_color(0, 0, 255, true, "fg");
	EXPECT_FALSE(blue_256.empty());
	EXPECT_TRUE(blue_256.find("38;5;") != std::string::npos);
}
