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
#include <vector>
#include <unordered_map>
#include <ranges>
#include <algorithm>

#include <btop_tools.h>
#include <btop_config.h>
#include <btop_theme.h>

using 	std::round, std::vector, robin_hood::unordered_flat_map, std::stoi, std::views::iota, std::array,
		std::clamp, std::max, std::min, std::ceil, std::to_string;
using namespace Tools;
namespace rng = std::ranges;
namespace fs = std::filesystem;

namespace Theme {

	fs::path theme_dir;
	fs::path user_theme_dir;

	const unordered_flat_map<string, string> Default_theme = {
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
		int truecolor_to_256(int r, int g, int b){
			if (round((double)r / 11) == round((double)g / 11) && round((double)g / 11) == round((double)b / 11)) {
				return 232 + round((double)r / 11);
			} else {
				return round((double)r / 51) * 36 + round((double)g / 51) * 6 + round((double)b / 51) + 16;
			}
		}
	}

	string hex_to_color(string hexa, bool t_to_256, string depth){
		if (hexa.size() > 1){
			hexa.erase(0, 1);
			for (auto& c : hexa) if (!isxdigit(c)) {
				Logger::error("Invalid hex value: " + hexa);
				return "";
			}
			string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");

			if (hexa.size() == 2){
				int h_int = stoi(hexa, 0, 16);
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
			else Logger::error("Invalid size of hex value: " + hexa);
		}
		else Logger::error("Hex value missing: " + hexa);
		return "";
	}

	string dec_to_color(int r, int g, int b, bool t_to_256, string depth){
		string pre = Fx::e + (depth == "fg" ? "38" : "48") + ";" + (t_to_256 ? "5;" : "2;");
		r = std::clamp(r, 0, 255);
		g = std::clamp(g, 0, 255);
		b = std::clamp(b, 0, 255);
		if (t_to_256) return pre + to_string(truecolor_to_256(r, g, b)) + "m";
		else return pre + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
	}

	array<int, 3> esc_to_rgb(string c_string){
		array<int, 3> rgb = {-1, -1, -1};
		if (c_string.size() >= 14){
			c_string.erase(0, 7);
			auto c_split = ssplit(c_string, ';');
			if (c_split.size() == 3){
				rgb[0] = stoi(c_split[0]);
				rgb[1] = stoi(c_split[1]);
				rgb[2] = stoi(c_split[2].erase(c_split[2].size()));
			}
		}
		return rgb;
	}


	namespace {
		unordered_flat_map<string, string> colors;
		unordered_flat_map<string, array<int, 3>> rgbs;
		unordered_flat_map<string, array<string, 101>> gradients;

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
			return {-1 ,-1 ,-1};
		}

		//* Generate colors and rgb decimal vectors for the theme
		void generateColors(unordered_flat_map<string, string>& source){
			vector<string> t_rgb;
			string depth;
			bool t_to_256 = !Config::getB("truecolor");
			colors.clear(); rgbs.clear();
			for (auto& [name, color] : Default_theme) {
				depth = (name.ends_with("bg") && name != "meter_bg") ? "bg" : "fg";
				if (source.contains(name)) {
					if (source.at(name)[0] == '#') {
						colors[name] = hex_to_color(source.at(name), t_to_256, depth);
						rgbs[name] = hex_to_dec(source.at(name));
					}
					else {
						t_rgb = ssplit(source.at(name));
						if (t_rgb.size() != 3)
							Logger::error("Invalid RGB decimal value: \"" + source.at(name) + "\"");
						else {
							colors[name] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), t_to_256, depth);
							rgbs[name] = array<int, 3>{stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2])};
						}
					}
				}
				if (colors[name].empty()) {
					Logger::info("Missing color value for \"" + name + "\". Using value from default.");
					colors[name] = hex_to_color(color, t_to_256, depth);
					rgbs[name] = array<int, 3>{-1, -1, -1};
				}
			}
		}

		//* Generate color gradients from two or three colors, 101 values indexed 0-100
		void generateGradients(){
			gradients.clear();
			array<string, 101> c_gradient;
			bool t_to_256 = !Config::getB("truecolor");
			for (auto& [name, source_arr] : rgbs) {
				if (!name.ends_with("_start")) continue;
				array<array<int, 3>, 101> dec_arr;
				dec_arr[0][0] = -1;
				string wname = rtrim(name, "_start");
				array<array<int, 3>, 3> rgb_arr = {source_arr, rgbs[wname + "_mid"], rgbs[wname + "_end"]};

				//? Only start iteration if gradient has a _end color value defined
				if (rgb_arr[2][0] >= 0) {

					//? Split iteration in two passes of 50 + 51 instead of 101 if gradient has _start, _mid and _end values defined
					int rng = (rgb_arr[1][0] >= 0) ? 50 : 100;
					for (int rgb : iota(0, 3)){
						int arr1 = 0, offset = 0;
						int arr2 = (rng == 50) ? 1 : 2;
						for (int i : iota(0, 101)) {
							dec_arr[i][rgb] = rgb_arr[arr1][rgb] + (i - offset) * (rgb_arr[arr2][rgb] - rgb_arr[arr1][rgb]) / rng;

							//? Switch source arrays from _start/_mid to _mid/_end at 50 passes if _mid is defined
							if (i == rng) { ++arr1; ++arr2; offset = 50;}
						}
					}
				}
				if (dec_arr[0][0] != -1) {
					int y = 0;
					for (auto& arr : dec_arr) c_gradient[y++] = dec_to_color(arr[0], arr[1], arr[2], t_to_256);
				}
				else {
					//? If only _start was defined fill array with _start color
					c_gradient.fill(colors[name]);
				}
				gradients[wname].swap(c_gradient);
			}
		}
	}


	//* Set current theme using <source> map
	void set(unordered_flat_map<string, string> source){
		generateColors(source);
		generateGradients();
		Term::fg = colors.at("main_fg");
		Term::bg = colors.at("main_bg");
		Fx::reset = Fx::reset_base + Term::fg + Term::bg;
	}

	//* Return escape code for color <name>
	const string& c(string name){
		return colors.at(name);
	}

	//* Return array of escape codes for color gradient <name>
	const array<string, 101>& g(string name){
		return gradients.at(name);
	}

	//* Return array of red, green and blue in decimal for color <name>
	const std::array<int, 3>& dec(string name){
		return rgbs.at(name);
	}

	robin_hood::unordered_flat_map<string, string>& test_colors(){
		return colors;
	}
	robin_hood::unordered_flat_map<string, std::array<string, 101>>& test_gradients(){
		return gradients;
	}

}