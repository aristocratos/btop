/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#pragma once

#include <string>
#include <robin_hood.h>
#include <array>
#include <filesystem>

using std::string, robin_hood::unordered_flat_map, std::array;

namespace Theme {
	extern std::filesystem::path theme_dir;
	extern std::filesystem::path user_theme_dir;

	//* Contains "Default" and "TTY" at indeces 0 and 1, otherwise full paths to theme files
	extern vector<string> themes;

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	hexa: ["#000000"-"#ffffff"] for color, ["#00"-"#ff"] for greyscale
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string hex_to_color(string hexa, const bool& t_to_256=false, const string& depth="fg");

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	r: [0-255], g: [0-255], b: [0-255]
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string dec_to_color(int r, int g, int b, const bool& t_to_256=false, const string& depth="fg");

	//* Update list of paths for available themes
	void updateThemes();

	//* Set current theme from current "color_theme" value in config
	void setTheme();

	extern unordered_flat_map<string, string> colors;
	extern unordered_flat_map<string, array<int, 3>> rgbs;
	extern unordered_flat_map<string, array<string, 101>> gradients;

	//* Return escape code for color <name>
	inline const string& c(const string& name) { return colors.at(name); }

	//* Return array of escape codes for color gradient <name>
	inline const array<string, 101>& g(string name) { return gradients.at(name); }

	//* Return array of red, green and blue in decimal for color <name>
	inline const std::array<int, 3>& dec(string name) { return rgbs.at(name); }

}