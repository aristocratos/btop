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

#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <robin_hood.h>

using std::array;
using std::string;
using std::vector;
using robin_hood::unordered_flat_map;

namespace Theme {
	extern std::filesystem::path theme_dir;
	extern std::filesystem::path user_theme_dir;

#define GRADIENT_FIELDS(name, required, c1, e1, c2, e2, c3, e3) \
	X(name##_start, Foreground, required, c1, e1) \
	X(name##_mid, Foreground, Optional, c2, e2) \
	X(name##_end, Foreground, Optional, c3, e3) \

	enum class ThemeLayer {
		Foreground,
		Background
	};

	enum class ThemeOptional {
		Optional,
		Required,
	};

// X(name, ThemeOptional, Required/Optional, hex color, tty escape code)
#define THEME_COLORS \
	X(main_bg, Background, Required, 0x000000, "\x1b[0;40m") \
	X(main_fg, Foreground, Required, 0xcccccc, "\x1b[37m") \
	X(title, Foreground, Required, 0xeeeeee, "\x1b[97m") \
	X(hi_fg, Foreground, Required, 0xb54040, "\x1b[91m") \
	X(selected_bg, Background, Required, 0x6a2f2f, "\x1b[41m") \
	X(selected_fg, Foreground, Required, 0xeeeeee, "\x1b[97m") \
	X(inactive_fg, Foreground, Required, 0x404040, "\x1b[90m") \
	X(graph_text, Foreground, Optional, 0x606060, "\x1b[90m") \
	X(meter_bg, Foreground, Optional, 0x404040, "\x1b[90m") \
	X(proc_misc, Foreground, Required, 0x0de756, "\x1b[92m") \
	X(cpu_box, Foreground, Required, 0x556d59, "\x1b[32m") \
	X(mem_box, Foreground, Required, 0x6c6c4b, "\x1b[33m") \
	X(net_box, Foreground, Required, 0x5c588d, "\x1b[35m") \
	X(proc_box, Foreground, Required, 0x805252, "\x1b[31m") \
	X(div_line, Foreground, Required, 0x303030, "\x1b[90m") \

#define THEME_GRADIENTS_PUBLIC \
	GRADIENT(temp, Required, 0x4897d4, "\x1b[94m", \
						 0x5474e8, "\x1b[96m", \
						 0xff40b6, "\x1b[95m") \
	GRADIENT(cpu, Required, 0x77ca9b, "\x1b[92m", \
						0xcbc06c, "\x1b[93m", \
					    0xdc4c4c, "\x1b[91m") \
	GRADIENT(free, Required, 0x384f21, "\x1b[32m", \
						 0xb5e685, "", \
						 0xdcff85, "\x1b[92m") \
	GRADIENT(cached, Required, 0x163350, "\x1b[36m", \
						   0x74e6fc, "", \
						   0x26c5ff, "\x1b[96m") \
	GRADIENT(available, Required, 0x4e3f0e, "\x1b[33m", \
							  0xffd77a, "", \
							  0xffb814, "\x1b[93m") \
	GRADIENT(used, Required, 0x592b26, "\x1b[31m", \
						 0xd9626d, "", \
						 0xff4769, "\x1b[91m") \
	GRADIENT(download, Required, 0x291f75, "\x1b[34m", \
							 0x4f43a3, "", \
							 0xb0a9de, "\x1b[94m") \
	GRADIENT(upload, Required, 0x620665, "\x1b[35m", \
						   0x7d4180, "", \
						   0xdcafde, "\x1b[95m") \
	GRADIENT(process, Optional, 0x80d0a3, "\x1b[32m", \
							 0xdcd179, "\x1b[33m", \
							 0xd45454, "\x1b[31m")  \

#define THEME_GRADIENTS_INTERNAL \
	GRADIENT(proc, Optional, -1, "", -1, "", -1, "") \
	GRADIENT(proc_color, Optional, -1, "", -1, "", -1, "") \

#define THEME_GRADIENTS THEME_GRADIENTS_PUBLIC THEME_GRADIENTS_INTERNAL
#define THEME_FIELDS_PUBLIC THEME_COLORS THEME_GRADIENTS_PUBLIC
#define THEME_FIELDS THEME_FIELDS_PUBLIC THEME_GRADIENTS_INTERNAL

	struct RGBTheme {
#define X(name, ...) int32_t name{-1};
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
		THEME_FIELDS
#undef GRADIENT
#undef X

		static RGBTheme get_default() {
			return {
#define X(name, fg, required, v, escape) .name = v,
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
				THEME_FIELDS
#undef GRADIENT
#undef X
			};
		}
	};

#define THEME_OFFSET(name) offsetof(RGBTheme, name)

	struct EscapeTheme {
#define X(name, fg, required, v, escape) string name{};
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
		THEME_FIELDS
#undef GRADIENT
#undef X

		static EscapeTheme get_tty() {
			return {
#define X(name, fg, required, v, escape) .name = escape,
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
				THEME_FIELDS
#undef GRADIENT
#undef X
			};
		}
	};

	struct GradientTheme {
#define GRADIENT(name, required, ...) array<string, 101> name;
		THEME_GRADIENTS
#undef GRADIENT
	};

	//* Contains "Default" and "TTY" at indeces 0 and 1, otherwise full paths to theme files
	extern vector<string> themes;

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	color: [0x000000-0xffffff] for color
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		layer: [ThemeLayer] for either a foreground color or a background color
	string color_to_escape(int32_t color, bool t_to_256=false, const ThemeLayer layer = ThemeLayer::Foreground);

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	r: [0-255], g: [0-255], b: [0-255]
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		layer: [ThemeLayer] for either a foreground color or a background color
	inline string rgb_to_escape(uint8_t r, uint8_t g, uint8_t b, bool t_to_256=false, const ThemeLayer layer = ThemeLayer::Foreground) {
		return color_to_escape(r << 16 | g << 8 | b, t_to_256, layer);
	}

	const EscapeTheme& c();
	const GradientTheme& g();

	//* Update list of paths for available themes
	void updateThemes();

	//* Set current theme from current "color_theme" value in config
	void setTheme();
}
