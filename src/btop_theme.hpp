// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

using std::array;
using std::string;
using std::vector;

namespace Theme {
    extern std::filesystem::path theme_dir;
    extern std::filesystem::path user_theme_dir;
    extern std::filesystem::path custom_theme_dir;

    //* Contains "Default" and "TTY" at indices 0 and 1, otherwise full paths to theme files
    extern vector<string> themes;

    //* Generate escape sequence for 24-bit or 256 color and return as a string
    //* Args	hexa: ["#000000"-"#ffffff"] for color, ["#00"-"#ff"] for greyscale
    //*			t_to_256: [true|false] convert 24bit value to 256 color value
    //* 		depth: ["fg"|"bg"] for either a foreground color or a background color
    string hex_to_color(string hexa, bool t_to_256 = false, const string& depth = "fg");

    //* Generate escape sequence for 24-bit or 256 color and return as a string
    //* Args	r: [0-255], g: [0-255], b: [0-255]
    //*			t_to_256: [true|false] convert 24bit value to 256 color value
    //* 		depth: ["fg"|"bg"] for either a foreground color or a background color
    string dec_to_color(int r, int g, int b, bool t_to_256 = false, const string& depth = "fg");

    //* Update list of paths for available themes
    void updateThemes();

    //* Set current theme from current "color_theme" value in config
    void setTheme();

    extern std::unordered_map<string, string> colors;
    extern std::unordered_map<string, array<int, 3>> rgbs;
    extern std::unordered_map<string, array<string, 101>> gradients;

    //* Return escape code for color <name>
    inline const string& c(const string& name) {
        return colors.at(name);
    }

    //* Return array of escape codes for color gradient <name>
    inline const array<string, 101>& g(const string& name) {
        return gradients.at(name);
    }

    //* Return array of red, green and blue in decimal for color <name>
    inline const std::array<int, 3>& dec(const string& name) {
        return rgbs.at(name);
    }

} // namespace Theme
