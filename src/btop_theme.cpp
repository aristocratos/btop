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

#include <cmath>
#include <fstream>
#include <unistd.h>

#include "btop_tools.hpp"
#include "btop_config.hpp"
#include "btop_theme.hpp"

using std::round;
using std::stoi;
using std::to_string;
using std::vector;
using std::views::iota;

using namespace Tools;

namespace fs = std::filesystem;

string Term::fg, Term::bg;
string Fx::reset = reset_base;

namespace Theme {

	fs::path theme_dir;
	fs::path user_theme_dir;
	vector<string> themes;
	std::unordered_map<string, string> colors;
	std::unordered_map<string, array<int, 3>> rgbs;
	std::unordered_map<string, array<string, 101>> gradients;

	const std::unordered_map<string, string> Default_theme = {
		{ "main_bg", "#00" },
		{ "main_fg", "#cc" },
		{ "title", "#ee" },
		{ "hi_fg", "#b54040" },
		{ "selected_bg", "#6a2f2f" },
		{ "selected_fg", "#ee" },
		{ "inactive_fg", "#40" },
		{ "graph_text", "#60" },
		{ "meter_bg", "#40" },
		{ "proc_misc", "#0de756" },
		{ "cpu_box", "#556d59" },
		{ "mem_box", "#6c6c4b" },
		{ "net_box", "#5c588d" },
		{ "proc_box", "#805252" },
		{ "div_line", "#30" },
		{ "temp_start", "#4897d4" },
		{ "temp_mid", "#5474e8" },
		{ "temp_end", "#ff40b6" },
		{ "cpu_start", "#77ca9b" },
		{ "cpu_mid", "#cbc06c" },
		{ "cpu_end", "#dc4c4c" },
		{ "free_start", "#384f21" },
		{ "free_mid", "#b5e685" },
		{ "free_end", "#dcff85" },
		{ "cached_start", "#163350" },
		{ "cached_mid", "#74e6fc" },
		{ "cached_end", "#26c5ff" },
		{ "available_start", "#4e3f0e" },
		{ "available_mid", "#ffd77a" },
		{ "available_end", "#ffb814" },
		{ "used_start", "#592b26" },
		{ "used_mid", "#d9626d" },
		{ "used_end", "#ff4769" },
		{ "download_start", "#291f75" },
		{ "download_mid", "#4f43a3" },
		{ "download_end", "#b0a9de" },
		{ "upload_start", "#620665" },
		{ "upload_mid", "#7d4180" },
		{ "upload_end", "#dcafde" },
		{ "process_start", "#80d0a3" },
		{ "process_mid", "#dcd179" },
		{ "process_end", "#d45454" }
	};

	const std::unordered_map<string, string> TTY_theme = {
		{ "main_bg", "\x1b[0;40m" },
		{ "main_fg", "\x1b[37m" },
		{ "title", "\x1b[97m" },
		{ "hi_fg", "\x1b[91m" },
		{ "selected_bg", "\x1b[41m" },
		{ "selected_fg", "\x1b[97m" },
		{ "inactive_fg", "\x1b[90m" },
		{ "graph_text", "\x1b[90m" },
		{ "meter_bg", "\x1b[90m" },
		{ "proc_misc", "\x1b[92m" },
		{ "cpu_box", "\x1b[32m" },
		{ "mem_box", "\x1b[33m" },
		{ "net_box", "\x1b[35m" },
		{ "proc_box", "\x1b[31m" },
		{ "div_line", "\x1b[90m" },
		{ "temp_start", "\x1b[94m" },
		{ "temp_mid", "\x1b[96m" },
		{ "temp_end", "\x1b[95m" },
		{ "cpu_start", "\x1b[92m" },
		{ "cpu_mid", "\x1b[93m" },
		{ "cpu_end", "\x1b[91m" },
		{ "free_start", "\x1b[32m" },
		{ "free_mid", "" },
		{ "free_end", "\x1b[92m" },
		{ "cached_start", "\x1b[36m" },
		{ "cached_mid", "" },
		{ "cached_end", "\x1b[96m" },
		{ "available_start", "\x1b[33m" },
		{ "available_mid", "" },
		{ "available_end", "\x1b[93m" },
		{ "used_start", "\x1b[31m" },
		{ "used_mid", "" },
		{ "used_end", "\x1b[91m" },
		{ "download_start", "\x1b[34m" },
		{ "download_mid", "" },
		{ "download_end", "\x1b[94m" },
		{ "upload_start", "\x1b[35m" },
		{ "upload_mid", "" },
		{ "upload_end", "\x1b[95m" },
		{ "process_start", "\x1b[32m" },
		{ "process_mid", "\x1b[33m" },
		{ "process_end", "\x1b[31m" }
	};

	namespace {
		//* Convert 24-bit colors to 256 colors
		int truecolor_to_256(const int& r, const int& g, const int& b) {
			//? Use upper 232-255 greyscale values if the downscaled red, green and blue are the same value
			if (const int red = round((double)r / 11); red == round((double)g / 11) and red == round((double)b / 11)) {
				return 232 + red;
			}
			//? Else use 6x6x6 color cube to calculate approximate colors
			else {
				return round((double)r / 51) * 36 + round((double)g / 51) * 6 + round((double)b / 51) + 16;
			}
		}
	}

	string hex_to_color(string hexa, bool t_to_256, const string& depth) {
		if (hexa.size() > 1) {
			hexa.erase(0, 1);
			for (auto& c : hexa) {
				if (not isxdigit(c)) {
					Logger::error("Invalid hex value: " + hexa);
					return "";
				}
			}
			string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");

			if (hexa.size() == 2) {
				int h_int = stoi(hexa, nullptr, 16);
				if (t_to_256) {
					return pre + to_string(truecolor_to_256(h_int, h_int, h_int)) + "m";
				} else {
					string h_str = to_string(h_int);
					return pre + h_str + ";" + h_str + ";" + h_str + "m";
				}
			}
			else if (hexa.size() == 6) {
				if (t_to_256) {
					return pre + to_string(truecolor_to_256(
						stoi(hexa.substr(0, 2), nullptr, 16),
						stoi(hexa.substr(2, 2), nullptr, 16),
						stoi(hexa.substr(4, 2), nullptr, 16))) + "m";
				} else {
					return pre +
						to_string(stoi(hexa.substr(0, 2), nullptr, 16)) + ";" +
						to_string(stoi(hexa.substr(2, 2), nullptr, 16)) + ";" +
						to_string(stoi(hexa.substr(4, 2), nullptr, 16)) + "m";
				}
			}
			else Logger::error("Invalid size of hex value: " + hexa);
		}
		else Logger::error("Hex value missing: " + hexa);
		return "";
	}

	string dec_to_color(int r, int g, int b, bool t_to_256, const string& depth) {
		string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");
		r = std::clamp(r, 0, 255);
		g = std::clamp(g, 0, 255);
		b = std::clamp(b, 0, 255);
		if (t_to_256) return pre + to_string(truecolor_to_256(r, g, b)) + "m";
		else return pre + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
	}

	namespace {
		//* Convert hex color to a array of decimals
		array<int, 3> hex_to_dec(string hexa) {
			if (hexa.size() > 1) {
				hexa.erase(0, 1);
				for (auto& c : hexa) {
					if (not isxdigit(c))
						return array{-1, -1, -1};
				}

				if (hexa.size() == 2) {
					int h_int = stoi(hexa, nullptr, 16);
					return array{h_int, h_int, h_int};
				}
				else if (hexa.size() == 6) {
						return array{
							stoi(hexa.substr(0, 2), nullptr, 16),
							stoi(hexa.substr(2, 2), nullptr, 16),
							stoi(hexa.substr(4, 2), nullptr, 16)
						};
				}
			}
			return {-1 ,-1 ,-1};
		}

		//* Generate colors and rgb decimal vectors for the theme
		void generateColors(const std::unordered_map<string, string>& source) {
			vector<string> t_rgb;
			string depth;
			bool t_to_256 = Config::getB("lowcolor");
			colors.clear(); rgbs.clear();
			for (const auto& [name, color] : Default_theme) {
				if (name == "main_bg" and not Config::getB("theme_background")) {
						colors[name] = "\x1b[49m";
						rgbs[name] = {-1, -1, -1};
						continue;
				}
				depth = (name.ends_with("bg") and name != "meter_bg") ? "bg" : "fg";
				if (source.contains(name)) {
					if (name == "main_bg" and source.at(name).empty()) {
						colors[name] = "\x1b[49m";
						rgbs[name] = {-1, -1, -1};
						continue;
					}
					else if (source.at(name).empty() and (name.ends_with("_mid") or name.ends_with("_end"))) {
						colors[name] = "";
						rgbs[name] = {-1, -1, -1};
						continue;
					}
					else if (source.at(name).starts_with('#')) {
						colors[name] = hex_to_color(source.at(name), t_to_256, depth);
						rgbs[name] = hex_to_dec(source.at(name));
					}
					else if (not source.at(name).empty()) {
						t_rgb = ssplit(source.at(name));
						if (t_rgb.size() != 3) {
							Logger::error("Invalid RGB decimal value: \"" + source.at(name) + "\"");
						} else {
							colors[name] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), t_to_256, depth);
							rgbs[name] = array{stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2])};

						}
					}
				}
				if (not colors.contains(name) and not is_in(name, "meter_bg", "process_start", "process_mid", "process_end", "graph_text")) {
					Logger::debug("Missing color value for \"" + name + "\". Using value from default.");
					colors[name] = hex_to_color(color, t_to_256, depth);
					rgbs[name] = hex_to_dec(color);
				}
			}
			//? Set fallback values for optional colors not defined in theme file
			if (not colors.contains("meter_bg")) {
				colors["meter_bg"] = colors.at("inactive_fg");
				rgbs["meter_bg"] = rgbs.at("inactive_fg");
			}
			if (not colors.contains("process_start")) {
				colors["process_start"] = colors.at("cpu_start");
				colors["process_mid"] = colors.at("cpu_mid");
				colors["process_end"] = colors.at("cpu_end");
				rgbs["process_start"] = rgbs.at("cpu_start");
				rgbs["process_mid"] = rgbs.at("cpu_mid");
				rgbs["process_end"] = rgbs.at("cpu_end");
			}
			if (not colors.contains("graph_text")) {
				colors["graph_text"] = colors.at("inactive_fg");
				rgbs["graph_text"] = rgbs.at("inactive_fg");
			}
		}

		//* Generate color gradients from two or three colors, 101 values indexed 0-100
		void generateGradients() {
			gradients.clear();
			bool t_to_256 = Config::getB("lowcolor");

			//? Insert values for processes greyscale gradient and processes color gradient
			rgbs.insert({
				{ "proc_start", 		rgbs["main_fg"]			},
				{ "proc_mid", 			{-1, -1, -1}			},
				{ "proc_end", 			rgbs["inactive_fg"]		},
				{ "proc_color_start", 	rgbs["inactive_fg"]		},
				{ "proc_color_mid", 	{-1, -1, -1}			},
				{ "proc_color_end", 	rgbs["process_start"]	},
			});

			for (const auto& [name, source_arr] : rgbs) {
				if (not name.ends_with("_start")) continue;
				const string color_name = rtrim(name, "_start");

				//? input_colors[start,mid,end][red,green,blue]
				const array<array<int, 3>, 3> input_colors = {
					source_arr,
					rgbs[color_name + "_mid"],
					rgbs[color_name + "_end"]
				};

				//? output_colors[red,green,blue][0-100]
				array<array<int, 3>, 101> output_colors;
				output_colors[0][0] = -1;

				//? Only start iteration if gradient has an end color defined
				if (input_colors[2][0] >= 0) {

					//? Split iteration in two passes of 50 + 51 instead of one pass of 101 if gradient has start, mid and end values defined
					int current_range = (input_colors[1][0] >= 0) ? 50 : 100;
					for (int rgb : iota(0, 3)) {
						int start = 0, offset = 0;
						int end = (current_range == 50) ? 1 : 2;
						for (int i : iota(0, 101)) {
							output_colors[i][rgb] = input_colors[start][rgb] + (i - offset) * (input_colors[end][rgb] - input_colors[start][rgb]) / current_range;

							//? Switch source arrays from start->mid to mid->end at 50 passes if mid is defined
							if (i == current_range) { ++start; ++end; offset = 50; }
						}
					}
				}
				//? Generate color escape codes for the generated rgb decimals
				array<string, 101> color_gradient;
				if (output_colors[0][0] != -1) {
					for (int y = 0; const auto& [red, green, blue] : output_colors)
						color_gradient[y++] = dec_to_color(red, green, blue, t_to_256);
				}
				else {
					//? If only start was defined fill array with start color
					color_gradient.fill(colors[name]);
				}
				gradients[color_name] = std::move(color_gradient);
			}
		}

		//* Set colors and generate gradients for the TTY theme
		void generateTTYColors() {
			rgbs.clear();
			gradients.clear();
			colors = TTY_theme;
			if (not Config::getB("theme_background"))
				colors["main_bg"] = "\x1b[49m";

			for (const auto& c : colors) {
				if (not c.first.ends_with("_start")) continue;
				const string base_name = rtrim(c.first, "_start");
				string section = "_start";
				int split = colors.at(base_name + "_mid").empty() ? 50 : 33;
				for (int i : iota(0, 101)) {
					gradients[base_name][i] = colors.at(base_name + section);
					if (i == split) {
						section = (split == 33) ? "_mid" : "_end";
						split *= 2;
					}
				}
			}
		}

		//* Load a .theme file from disk
		auto loadFile(const string& filename) {
			std::unordered_map<string, string> theme_out;
			const fs::path filepath = filename;
			if (not fs::exists(filepath))
				return Default_theme;

			std::ifstream themefile(filepath);
			if (themefile.good()) {
				Logger::debug("Loading theme file: " + filename);
				while (not themefile.bad()) {
					if (themefile.peek() == '#') {
						themefile.ignore(SSmax, '\n');
						continue;
					}
					themefile.ignore(SSmax, '[');
					if (themefile.eof()) break;
					string name, value;
					getline(themefile, name, ']');
					if (not Default_theme.contains(name)) {
						themefile.ignore(SSmax, '\n');
						continue;
					}
					themefile.ignore(SSmax, '=');
					themefile >> std::ws;
					if (themefile.eof()) break;
					if (themefile.peek() == '"') {
						themefile.ignore(1);
						getline(themefile, value, '"');
						themefile.ignore(SSmax, '\n');
					}
					else getline(themefile, value, '\n');

					theme_out[name] = value;
				}
				return theme_out;
			}
			return Default_theme;
		}
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
		if (theme == "TTY" or Config::getB("tty_mode"))
			generateTTYColors();
		else {
			generateColors((theme == "Default" or theme_path.empty() ? Default_theme : loadFile(theme_path)));
			generateGradients();
		}
		Term::fg = colors.at("main_fg");
		Term::bg = colors.at("main_bg");
		Fx::reset = Fx::reset_base + Term::fg + Term::bg;
	}

}
