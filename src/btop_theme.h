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

#ifndef _btop_theme_included_
#define _btop_theme_included_ 1

#include <string>
#include <cmath>
#include <vector>
#include <map>
#include <unordered_map>
#include <ranges>

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>

using std::string, std::round, std::vector, std::map, std::stoi, std::views::iota, std::array, std::unordered_map;
using namespace Tools;

namespace Theme {

	const unordered_map<string, string> Default_theme = {
		{ "main_bg", "#00" },
		{ "main_fg", "#cc" },
		{ "title", "#ee" },
		{ "hi_fg", "#969696" },
		{ "selected_bg", "#7e2626" },
		{ "selected_fg", "#ee" },
		{ "inactive_fg", "#40" },
		{ "graph_text", "#60" },
		{ "meter_bg", "#40" },
		{ "proc_misc", "#0de756" },
		{ "cpu_box", "#3d7b46" },
		{ "mem_box", "#8a882e" },
		{ "net_box", "#423ba5" },
		{ "proc_box", "#923535" },
		{ "div_line", "#30" },
		{ "temp_start", "#4897d4" },
		{ "temp_mid", "#5474e8" },
		{ "temp_end", "#ff40b6" },
		{ "cpu_start", "#50f095" },
		{ "cpu_mid", "#f2e266" },
		{ "cpu_end", "#fa1e1e" },
		{ "free_start", "#223014" },
		{ "free_mid", "#b5e685" },
		{ "free_end", "#dcff85" },
		{ "cached_start", "#0b1a29" },
		{ "cached_mid", "#74e6fc" },
		{ "cached_end", "#26c5ff" },
		{ "available_start", "#292107" },
		{ "available_mid", "#ffd77a" },
		{ "available_end", "#ffb814" },
		{ "used_start", "#3b1f1c" },
		{ "used_mid", "#d9626d" },
		{ "used_end", "#ff4769" },
		{ "download_start", "#231a63" },
		{ "download_mid", "#4f43a3" },
		{ "download_end", "#b0a9de" },
		{ "upload_start", "#510554" },
		{ "upload_mid", "#7d4180" },
		{ "upload_end", "#dcafde" },
		{ "process_start", "#80d0a3" },
		{ "process_mid", "#dcd179" },
		{ "process_end", "#d45454" }
	};

	namespace {
		//* Convert 24-bit colors to 256 colors using 6x6x6 color cube
		int truecolor_to_256(uint r, uint g, uint b){
			if (r / 11 == g / 11 && g / 11 == b / 11) {
				return 232 + r / 11;
			} else {
				return round((float)(r / 51)) * 36 + round((float)(g / 51)) * 6 + round((float)(b / 51)) + 16;
			}
		}
	}

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	hexa: ["#000000"-"#ffffff"] for color, ["#00"-"#ff"] for greyscale
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string hex_to_color(string hexa, bool t_to_256=false, string depth="fg"){
		if (hexa.size() > 1){
			hexa.erase(0, 1);
			for (auto& c : hexa) if (!isxdigit(c)) return "";
			depth = (depth == "fg") ? "38" : "48";
			string pre = Fx::e + depth + ";";
			pre += (t_to_256) ? "5;" : "2;";

			if (hexa.size() == 2){
				uint h_int = stoi(hexa, 0, 16);
				if (t_to_256){
					return pre + to_string(truecolor_to_256(h_int, h_int, h_int)) + "m";
				} else {
					string h_str = to_string(h_int);
					return pre + h_str + ";" + h_str + ";" + h_str + "m";
				}
			}
			else if (hexa.size() == 6){
				if (t_to_256){
					return pre + to_string(truecolor_to_256(
						stoi(hexa.substr(0, 2), 0, 16),
						stoi(hexa.substr(2, 2), 0, 16),
						stoi(hexa.substr(4, 2), 0, 16))) + "m";
				} else {
					return pre +
						to_string(stoi(hexa.substr(0, 2), 0, 16)) + ";" +
						to_string(stoi(hexa.substr(2, 2), 0, 16)) + ";" +
						to_string(stoi(hexa.substr(4, 2), 0, 16)) + "m";
				}
			}
		}
		return "";
	}

	//* Generate escape sequence for 24-bit or 256 color and return as a string
	//* Args	r: [0-255], g: [0-255], b: [0-255]
	//*			t_to_256: [true|false] convert 24bit value to 256 color value
	//* 		depth: ["fg"|"bg"] for either a foreground color or a background color
	string dec_to_color(uint r, uint g, uint b, bool t_to_256=false, string depth="fg"){
		depth = (depth == "fg") ? "38" : "48";
		string pre = Fx::e + depth + ";";
		pre += (t_to_256) ? "5;" : "2;";
		r = (r > 255) ? 255 : r;
		g = (g > 255) ? 255 : g;
		b = (b > 255) ? 255 : b;
		if (t_to_256) return pre + to_string(truecolor_to_256(r, g, b)) + "m";
		else return pre + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
	}

	//* Return an array of red, green and blue, 0-255 values for a 24-bit color escape string
	auto esc_to_rgb(string c_string){
		array<int, 3> rgb = {-1, -1, -1};
		if (c_string.size() >= 14){
			c_string.erase(0, 7);
			auto c_split = ssplit(c_string, ";");
			if (c_split.size() == 3){
				rgb[0] = stoi(c_split[0]);
				rgb[1] = stoi(c_split[1]);
				rgb[2] = stoi(c_split[2].erase(c_split[2].size()));
			}
		}
		return rgb;
	}


	namespace {
		unordered_map<string, string> colors;
		unordered_map<string, array<int, 3>> rgbs;
		unordered_map<string, array<string, 101>> gradients;

		//* Convert hex color to a array of decimals
		array<int, 3> hex_to_dec(string hexa){
			if (hexa.size() > 1){
				hexa.erase(0, 1);
				for (auto& c : hexa) if (!isxdigit(c)) return array<int, 3>{-1, -1, -1};

				if (hexa.size() == 2){
					int h_int = stoi(hexa, 0, 16);
					return array<int, 3>{h_int, h_int, h_int};
				}
				else if (hexa.size() == 6){
						return array<int, 3>{
							stoi(hexa.substr(0, 2), 0, 16),
							stoi(hexa.substr(2, 2), 0, 16),
							stoi(hexa.substr(4, 2), 0, 16)
						};
				}
			}
			return array<int, 3>{-1 ,-1 ,-1};
		}

		//* Generate colors and rgb decimal vectors for the theme
		void generateColors(unordered_map<string, string>& source){
			vector<string> t_rgb;
			string depth;
			colors.clear(); rgbs.clear();
			for (auto& [name, color] : Default_theme) {
				depth = (name.ends_with("bg") && name != "meter_bg") ? "bg" : "fg";
				if (source.contains(name)) {
					if (source.at(name)[0] == '#') {
						colors[name] = hex_to_color(source.at(name), !Config::getB("truecolor"), depth);
						rgbs[name] = hex_to_dec(source.at(name));
					}
					else {
						t_rgb = ssplit(source.at(name), " ");
						colors[name] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), !Config::getB("truecolor"), depth);
						rgbs[name] = array<int, 3>{stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2])};
					}
				}
				else colors[name] = "";
				if (colors[name].empty()) {
					colors[name] = hex_to_color(color, !Config::getB("truecolor"), depth);
					rgbs[name] = array<int, 3>{-1, -1, -1};
				}
			}
		}

		//* Generate color gradients from one, two or three colors, 101 values indexed 0-100
		void generateGradients(){
			gradients.clear();
			array<string, 101> c_gradient;
			string wname;
			array<array<int, 3>, 3> rgb_arr;
			array<array<int, 3>, 101> dec_arr;
			int f, s, r, o, y;
			for (auto& [name, source_arr] : rgbs){
				if (!name.ends_with("_start")) continue;
				dec_arr[0][0] = -1;
				wname = rtrim(name, "_start");
				rgb_arr = {source_arr, rgbs[wname + "_mid"], rgbs[wname + "_end"]};

				//? Only start iteration if gradient has a _end color value defined
				if (rgb_arr[2][0] >= 0) {

					//? Split iteration in two passes of 50 + 51 instead of 101 if gradient has _start, _mid and _end values defined
					r = (rgb_arr[1][0] >= 0) ? 50 : 100;
					for (int i : iota(0, 3)){
						f = 0; s = (r == 50) ? 1 : 2; o = 0;
						for (int c : iota(0, 101)){
							dec_arr[c][i] = rgb_arr[f][i] + (c - o) * (rgb_arr[s][i] - rgb_arr[f][i]) / r;

							//? Switch source arrays from _start/_mid to _mid/_end at 50 passes if _mid is defined
							if (c == r) { ++f; ++s; o = 50;}
						}
					}
				}
				y = 0;
				if (dec_arr[0][0] != -1) {
					for (auto& vec : dec_arr) c_gradient[y++] = dec_to_color(vec[0], vec[1], vec[2], !Config::getB("truecolor"));
				} else {
					//? If only _start was defined fill array with _start color
					c_gradient.fill(colors[name]);
				}
				gradients[wname] = c_gradient;
			}
		}
	}


	//* Set current theme using <source> map
	void set(unordered_map<string, string> source){
		generateColors(source);
		generateGradients();
		Term::fg = colors.at("main_fg");
		Term::bg = colors.at("main_bg");
		Fx::reset = Fx::reset_base + Term::fg + Term::bg;
	}

	//* Return escape code for color <name>
	auto c(string name){
		return colors.at(name);
	}

	//* Return array of escape codes for color gradient <name>
	auto g(string name){
		return gradients.at(name);
	}

	//* Return array of red, green and blue in decimal for color <name>
	auto dec(string name){
		return rgbs.at(name);
	}

}

#endif