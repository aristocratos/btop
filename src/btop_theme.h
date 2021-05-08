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
#define _btop_theme_included_

#include <string>
#include <cmath>
#include <vector>
#include <map>

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>

using std::string, std::to_string, std::round, std::vector, std::map;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

//* Convert 24-bit colors to 256 colors using 6x6x6 color cube
int truecolor_to_256(unsigned r, unsigned g, unsigned b){
	if (r / 11 == g / 11 && g / 11 == b / 11) {
		return 232 + r / 11;
	} else {
		return round((float)(r / 51)) * 36 + round((float)(g / 51)) * 6 + round((float)(b / 51)) + 16;
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
			unsigned h_int = stoi(hexa, 0, 16);
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
string dec_to_color(unsigned r, unsigned g, unsigned b, bool t_to_256=false, string depth="fg"){
	depth = (depth == "fg") ? "38" : "48";
	string pre = Fx::e + depth + ";";
	pre += (t_to_256) ? "5;" : "2;";
	r = (r > 255) ? 255 : r;
	g = (g > 255) ? 255 : g;
	b = (b > 255) ? 255 : b;
	if (t_to_256) return pre + to_string(truecolor_to_256(r, g, b)) + "m";
	else return pre + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
}

//* Return a map of "r", "g", "b", 0-255 values for a 24-bit color escape string
map<string, int> c_to_rgb(string c_string){
	map<string, int> rgb = {{"r", 0}, {"g", 0}, {"b", 0}};
	if (c_string.size() >= 14){
		c_string.erase(0, 7);
		auto c_split = ssplit(c_string, ";");
		if (c_split.size() == 3){
			rgb["r"] = stoi(c_split[0]);
			rgb["g"] = stoi(c_split[1]);
			rgb["b"] = stoi(c_split[2].erase(c_split[2].size()));
		}
	}
	return rgb;
}

//? --------------------------------------------------- CLASSES -----------------------------------------------------

class C_Theme {
	map<string, string> c;
	map<string, vector<string>> g;

	map<string,string> generate(map<string, string>& source){
		map<string, string> out;
		vector<string> t_rgb;
		string depth;
		for (auto& item : Global::Default_theme) {
			depth = (item.first.ends_with("bg")) ? "bg" : "fg";
			if (source.contains(item.first)) {
				if (source.at(item.first)[0] == '#') out[item.first] = hex_to_color(source.at(item.first), !State::truecolor, depth);
				else {
					t_rgb = ssplit(source.at(item.first), " ");
					out[item.first] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), !State::truecolor, depth);
				}
			}
			else out[item.first] = "";
			if (out[item.first].empty()) out[item.first] = hex_to_color(item.second, !State::truecolor, depth);
		}
		return out;
	}

public:

	//* Change to theme <source>
	void change(map<string, string> source){
		c = generate(source);
		State::fg = c.at("main_fg");
		State::bg = c.at("main_bg");
		Fx::reset = Fx::reset_base + State::fg + State::bg;
	}

	//* Generate theme from <source> map, default to DEFAULT_THEME on missing or malformatted values
	C_Theme(map<string, string> source){
		change(source);
	}

	//* Return escape code for color <name>
	auto operator()(string name){
		return c.at(name);
	}

	//* Return vector of escape codes for color gradient <name>
	auto gradient(string name){
		return g.at(name);
	}

	//* Return map of decimal int's (r, g, b) for color <name>
	auto rgb(string name){
		return c_to_rgb(c.at(name));
	}

};

#endif