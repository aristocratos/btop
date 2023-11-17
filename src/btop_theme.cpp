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

#include <cctype>
#include <fstream>
#include <unistd.h>

#include "btop_tools.hpp"
#include "btop_config.hpp"
#include "btop_theme.hpp"

using std::vector;

using namespace Tools;

namespace fs = std::filesystem;

string Term::fg, Term::bg;
string Fx::reset = reset_base;

namespace Theme {

	fs::path theme_dir;
	fs::path user_theme_dir;
	vector<string> themes;

	const unordered_flat_map<string, uint32_t> parse_table = {
#define X(name, fg, required, v, escape) { #name, THEME_OFFSET(name) },
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
		THEME_FIELDS_PUBLIC
#undef GRADIENT
#undef X
	};

	EscapeTheme escapes;
	GradientTheme gradients;
	bool tty_theme{};

	const EscapeTheme& c() {
		return escapes;
	}

	const GradientTheme& g() {
		return gradients;
	}

	bool is_tty() {
		return tty_theme;
	}

	const std::array<uint8_t, 256> grayscale_lut = {
		232, 232, 232, 232, 232, 232, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233,
		233, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 236,
		236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238,
		239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241,
		241, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 244,
		244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246,
		247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249,
		249, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 252,
		252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254,
		255, 255, 255, 255, 255, 255, 255, 231,
	};

	const std::array<uint8_t, 256> color_lut = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	};

	const std::array<int8_t, 256> hex_lut = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};

	const std::array<int8_t, 256> decimal_lut = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};

	namespace {
		//* Convert 24-bit colors to 256 colors
		int truecolor_to_256(const int& r, const int& g, const int& b) {
			//? Use upper 232-255 greyscale values if the downscaled red, green and blue are the same value
			if (grayscale_lut[r] == grayscale_lut[g] && grayscale_lut[g] == grayscale_lut[b]) {
				return grayscale_lut[r];
			}
			//? Else use 6x6x6 color cube to calculate approximate colors
			return color_lut[r] * 36 + color_lut[g] * 6 + color_lut[b] + 16;
		}
	}

	inline const char* skip_whitespace(const char* s) {
		while(isspace(*s)) ++s;

		return s;
	}

	inline size_t parse_decimal(const char* s, uint16_t& output) {
		size_t i = 0;
		for(; i < 3; ++i) {
			int8_t digit = decimal_lut[(size_t)s[i]];
			if(digit == -1) break;
			output = output * 10 + digit;
		}

		return i;
	}

	int32_t parse_color(const std::string& color) {
		const char* s = skip_whitespace(color.c_str());

		if(*s == '#') { // #ab or #abcdef
			++s;
			uint32_t n = 0;
			size_t i = 0;
			for(; i < 6; ++i) {
				int8_t digit = hex_lut[(size_t)s[i]];
				if(digit == -1) break;
				n = n * 16 + digit;
			}

			s = skip_whitespace(s + i);
			if(*s != '\0') goto error;

			switch(i) {
				case 2:
					return n << 16 | n << 8 | n;
				case 6:
					return n;
				default:
					Logger::error("Invalid size of hex value: " + color);
					return -1;
			}
		} else if(decimal_lut[(size_t)*s] >= 0) { // rgb numbers separated with commas
			uint16_t r = 0, g = 0, b = 0;
			size_t i = parse_decimal(s, r);

			s = skip_whitespace(s + i);
			if(*s != ',') goto error;

			s = skip_whitespace(s + 1);
			i = parse_decimal(s, g);
			if(i == 0) goto error;

			s = skip_whitespace(s + i);
			if(*s != ',') goto error;

			s = skip_whitespace(s + 1);
			i = parse_decimal(s, b);
			if(i == 0) goto error;

			s = skip_whitespace(s + i);
			if(*s != '\0') goto error;

			r = r > 0xff ? 0xff : r;
			g = g > 0xff ? 0xff : g;
			b = b > 0xff ? 0xff : b;

			return r << 16 | g << 8 | b;
		} else if(*s == '\0') { // string empty or whitespace only
			return -1;
		}

error:
		Logger::error("Invalid color value: " + color);
		return -1;
	}

	string color_to_escape(int32_t color, bool t_to_256, ThemeLayer fg) {
		if(color < 0) return "";

		uint8_t sgr = fg == ThemeLayer::Foreground ? 38 : 48;
		uint8_t r = color >> 16;
		uint8_t g = (color >> 8) & 0xff;
		uint8_t b = color & 0xff;

		if (t_to_256) {
			return fmt::format("\x1b[{};5;{}m", sgr, truecolor_to_256(r, g, b));
		}

		return fmt::format("\x1b[{};2;{};{};{}m", sgr, r, g, b);
	}

	namespace {
		inline void required_not_found(
				const robin_hood::unordered_flat_set<size_t>& offsets,
				const ThemeOptional required,
				const size_t offset,
				const std::string_view name,
				int32_t& field,
				const int32_t value) {
			if(required == ThemeOptional::Required and offsets.contains(offset)) {
				field = value;
				Logger::debug(fmt::format("Missing color value for {}. Using value from default.", name));
			}
		}
	}

		//* Generate colors and rgb decimal vectors for the theme
		EscapeTheme generateColors(RGBTheme& source) {
			EscapeTheme output_theme;
			bool t_to_256 = Config::getB("lowcolor");

			if(not Config::getB("theme_background") || source.main_bg == -1) {
				source.main_bg = -1;
				output_theme.main_bg = "\x1b[49m";
			}

#define X(name, layer, required, v, escape) \
			if(source.name >= 0) output_theme.name = color_to_escape(source.name, t_to_256, ThemeLayer::layer);
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
			THEME_FIELDS_PUBLIC
#undef GRADIENT
#undef X

			//? Set fallback values for optional colors not defined in theme file
			if (source.meter_bg == -1) {
				output_theme.meter_bg = output_theme.inactive_fg;
				source.meter_bg = source.inactive_fg;
			}

			if (source.process_start == -1) {
				output_theme.process_start = output_theme.cpu_start;
				output_theme.process_mid = output_theme.cpu_mid;
				output_theme.process_end = output_theme.cpu_end;
				source.process_start = source.cpu_start;
				source.process_mid = source.cpu_mid;
				source.process_end = source.cpu_end;
			}
			if (source.graph_text == -1) {
				output_theme.graph_text = output_theme.inactive_fg;
				source.graph_text = source.inactive_fg;
			}

			return output_theme;
		}

		inline uint8_t lerp_color(uint8_t start, double t, uint8_t end) {
			return (uint8_t)((1 - t) * start + t * end);
		}

		inline array<string, 101> make_gradient(const int32_t start, const int32_t mid, const int32_t end, const bool to_256) {
			array<string, 101> output_colors;

			// Assumption: start >= 0
			if(end >= 0 && mid >= 0) {
				for (size_t i = 0; i <= 50; ++i) {
					double t = i/50.0;
					output_colors[i] = rgb_to_escape(lerp_color(start >> 16, t, mid >> 16),
						lerp_color(start >> 8 & 0xff, t, mid >> 8 & 0xff),
						lerp_color(start & 0xff, t, mid & 0xff), to_256);
				}

				for (size_t i = 1; i <= 50; ++i) {
					double t = i/50.0;
					output_colors[i + 50] = rgb_to_escape(lerp_color(mid >> 16, t, end >> 16),
						lerp_color(mid >> 8 & 0xff, t, end >> 8 & 0xff),
						lerp_color(mid & 0xff, t, end & 0xff), to_256);
				}
			} else if(end >= 0) {
				for (size_t i = 0; i <= 100; ++i) {
					double t = i/100.0;
					output_colors[i] = rgb_to_escape(lerp_color(start >> 16, t, end >> 16),
						lerp_color(start >> 8 & 0xff, t, end >> 8 & 0xff),
						lerp_color(start & 0xff, t, end & 0xff), to_256);
				}
			} else {
				output_colors.fill(color_to_escape(start, to_256));
			}

			return output_colors;
		}

		//* Generate color gradients from two or three colors, 101 values indexed 0-100
		GradientTheme generateGradients(RGBTheme& source) {
			GradientTheme output;
			bool t_to_256 = Config::getB("lowcolor");

			//? Set values for processes greyscale gradient and processes color gradient
			source.proc_start = source.main_fg;
			source.proc_end = source.inactive_fg;
			source.proc_color_start = source.inactive_fg;
			source.proc_color_end = source.process_start;

			return {
#define GRADIENT(name, required, ...) .name = make_gradient(source.name##_start, source.name##_mid, source.name##_end, t_to_256),
				THEME_GRADIENTS
#undef GRADIENT
			};
		}

		array<string, 101> make_tty_gradient(const string& start, const string& mid, const string& end) {
			array<string, 101> gradient;
			if(mid.empty()) {
				for(size_t i = 0; i <= 50; ++i) {
					gradient[i] = start;
				}

				for(size_t i = 51; i < gradient.size(); ++i) {
					gradient[i] = end;
				}

				return gradient;
			}

			for(size_t i = 0; i <= 33; ++i) {
				gradient[i] = start;
			}

			for(size_t i = 34; i <= 66; ++i) {
				gradient[i] = mid;
			}

			for(size_t i = 67; i < gradient.size(); ++i) {
				gradient[i] = end;
			}

			return gradient;
		}

		//* Set colors and generate gradients for the TTY theme
		void generateTTYColors() {
			tty_theme = true;
			escapes = EscapeTheme::get_tty();
			if (not Config::getB("theme_background"))
				escapes.main_bg = "\x1b[49m";

#define GRADIENT(name, required, ...) gradients.name = make_tty_gradient(escapes.name##_start, escapes.name##_mid, escapes.name##_end);
			THEME_GRADIENTS
#undef GRADIENT
		}

		//* Load a .theme file from disk
		auto loadFile(const string& filename) {
			RGBTheme theme_out;
			const fs::path filepath = filename;
			if (not fs::exists(filepath))
				return RGBTheme::get_default();

			std::ifstream themefile(filepath);
			robin_hood::unordered_flat_set<size_t> unread_offsets {
#define X(name, fg, required, v, tty) THEME_OFFSET(name),
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
				THEME_FIELDS_PUBLIC
#undef GRADIENT
#undef X
			};
			if (themefile.good()) {
				Logger::debug("Loading theme file: " + filename);
				while (not themefile.bad()) {
					themefile.ignore(SSmax, '[');
					if (themefile.eof()) break;
					string name, value;
					getline(themefile, name, ']');
					auto found = parse_table.find(name);
					if (found == parse_table.end()) {
						continue;
					}
					themefile.ignore(SSmax, '=');
					themefile >> std::ws;
					if (themefile.eof()) break;
					if (themefile.peek() == '"') {
						themefile.ignore(1);
						getline(themefile, value, '"');
					}
					else getline(themefile, value, '\n');

					*(int32_t*)((char*)&theme_out + found->second) = parse_color(value);
					unread_offsets.erase(found->second);
				}

#define X(name, fg, required, v, tty) \
				required_not_found(unread_offsets, \
						ThemeOptional::required, \
						THEME_OFFSET(name), \
						#name, \
						theme_out.name, \
						v);
#define GRADIENT(...) GRADIENT_FIELDS(__VA_ARGS__)
				THEME_FIELDS_PUBLIC
#undef GRADIENT
#undef X
				return theme_out;
			}
			return RGBTheme::get_default();
		}

	void updateThemes() {
		themes.clear();
		themes.push_back("Default");
		themes.push_back("TTY");

		for (const auto& path : { user_theme_dir, theme_dir } ) {
			if (path.empty()) continue;
			for (auto& file : fs::directory_iterator(path)) {
				if (file.path().extension() == ".theme" and access(file.path().c_str(), R_OK) != -1 and not v_contains(themes, file.path().c_str())) {
					themes.push_back(file.path().c_str());
				}
			}
		}

	}

	void setTheme() {
		const auto& theme = Config::getS("color_theme");
		fs::path theme_path;
		for (const fs::path p : themes) {
			if (p == theme or p.stem() == theme or p.filename() == theme) {
				theme_path = p;
				break;
			}
		}
		if (theme == "TTY" or Config::getB("tty_mode")) {
			generateTTYColors();
		} else {
			tty_theme = false;
			RGBTheme colors = theme == "Default" or theme_path.empty() ? RGBTheme::get_default() : loadFile(theme_path);
			escapes = generateColors(colors);
			gradients = generateGradients(colors);
		}
		Term::fg = escapes.main_fg;
		Term::bg = escapes.main_bg;
		Fx::reset = Fx::reset_base + Term::fg + Term::bg;
	}
}
