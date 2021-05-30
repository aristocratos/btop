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



#include <string>
#include <vector>
#include <array>
#include <robin_hood.h>
#include <ranges>
#include <algorithm>
#include <cmath>

#include <btop_config.h>
#include <btop_tools.h>

#ifndef _btop_draw_included_
#define _btop_draw_included_ 1

using std::string, std::vector, robin_hood::unordered_flat_map, std::round, std::views::iota,
	std::string_literals::operator""s, std::clamp, std::array, std::floor;

namespace Symbols {
	const string h_line			= "─";
	const string v_line			= "│";
	const string left_up		= "┌";
	const string right_up		= "┐";
	const string left_down		= "└";
	const string right_down		= "┘";
	const string title_left		= "┤";
	const string title_right	= "├";
	const string div_up			= "┬";
	const string div_down		= "┴";

	const string meter = "■";

	const array<string, 10> superscript = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };

	const unordered_flat_map<float, string> graph_up = {
		{0.0, " "}, {0.1, "⢀"}, {0.2, "⢠"}, {0.3, "⢰"}, {0.4, "⢸"},
		{1.0, "⡀"}, {1.1, "⣀"}, {1.2, "⣠"}, {1.3, "⣰"}, {1.4, "⣸"},
		{2.0, "⡄"}, {2.1, "⣄"}, {2.2, "⣤"}, {2.3, "⣴"}, {2.4, "⣼"},
		{3.0, "⡆"}, {3.1, "⣆"}, {3.2, "⣦"}, {3.3, "⣶"}, {3.4, "⣾"},
		{4.0, "⡇"}, {4.1, "⣇"}, {4.2, "⣧"}, {4.3, "⣷"}, {4.4, "⣿"}
	};

	const unordered_flat_map<float, string> graph_down = {
		{0.0, " "}, {0.1, "⠈"}, {0.2, "⠘"}, {0.3, "⠸"}, {0.4, "⢸"},
		{1.0, "⠁"}, {1.1, "⠉"}, {1.2, "⠙"}, {1.3, "⠹"}, {1.4, "⢹"},
		{2.0, "⠃"}, {2.1, "⠋"}, {2.2, "⠛"}, {2.3, "⠻"}, {2.4, "⢻"},
		{3.0, "⠇"}, {3.1, "⠏"}, {3.2, "⠟"}, {3.3, "⠿"}, {3.4, "⢿"},
		{4.0, "⡇"}, {4.1, "⡏"}, {4.2, "⡟"}, {4.3, "⡿"}, {4.4, "⣿"}
	};

	const unordered_flat_map<float, string> graph_up_small = {
		{0.0, Mv::r(1)}, {0.1, "⢀"}, {0.2, "⢠"}, {0.3, "⢰"}, {0.4, "⢸"},
		{1.0, "⡀"}, {1.1, "⣀"}, {1.2, "⣠"}, {1.3, "⣰"}, {1.4, "⣸"},
		{2.0, "⡄"}, {2.1, "⣄"}, {2.2, "⣤"}, {2.3, "⣴"}, {2.4, "⣼"},
		{3.0, "⡆"}, {3.1, "⣆"}, {3.2, "⣦"}, {3.3, "⣶"}, {3.4, "⣾"},
		{4.0, "⡇"}, {4.1, "⣇"}, {4.2, "⣧"}, {4.3, "⣷"}, {4.4, "⣿"}
	};

	const unordered_flat_map<float, string> graph_down_small = {
		{0.0, Mv::r(1)}, {0.1, "⠈"}, {0.2, "⠘"}, {0.3, "⠸"}, {0.4, "⢸"},
		{1.0, "⠁"}, {1.1, "⠉"}, {1.2, "⠙"}, {1.3, "⠹"}, {1.4, "⢹"},
		{2.0, "⠃"}, {2.1, "⠋"}, {2.2, "⠛"}, {2.3, "⠻"}, {2.4, "⢻"},
		{3.0, "⠇"}, {3.1, "⠏"}, {3.2, "⠟"}, {3.3, "⠿"}, {3.4, "⢿"},
		{4.0, "⡇"}, {4.1, "⡏"}, {4.2, "⡟"}, {4.3, "⡿"}, {4.4, "⣿"}
	};
}

namespace Draw {

	struct BoxConf {
		uint x=0, y=0;
		uint width=0, height=0;
		string line_color = "", title = "", title2 = "";
		bool fill = true;
		uint num=0;
	};

	//* Create a box using values from a BoxConf struct and return as a string
	string createBox(BoxConf c){
		string out;
		string lcolor = (c.line_color.empty()) ? Theme::c("div_line") : c.line_color;
		string numbering = (c.num == 0) ? "" : Theme::c("hi_fg") + Symbols::superscript[c.num];

		out = Fx::reset + lcolor;

		//* Draw horizontal lines
		for (uint hpos : {c.y, c.y + c.height - 1}){
			out += Mv::to(hpos, c.x) + Symbols::h_line * (c.width - 1);
		}

		//* Draw vertical lines and fill if enabled
		for (uint hpos : iota(c.y + 1, c.y + c.height - 1)){
			out += Mv::to(hpos, c.x) + Symbols::v_line +
				((c.fill) ? string(c.width - 2, ' ') : Mv::r(c.width - 2)) +
				Symbols::v_line;
		}

		//* Draw corners
		out += 	Mv::to(c.y, c.x) + Symbols::left_up +
				Mv::to(c.y, c.x + c.width) + Symbols::right_up +
				Mv::to(c.y + c.height - 1, c.x) + Symbols::left_down +
				Mv::to(c.y + c.height - 1, c.x + c.width) + Symbols::right_down;

		//* Draw titles if defined
		if (!c.title.empty()){
			out += Mv::to(c.y, c.x + 2) + Symbols::title_left + Fx::b + numbering + Theme::c("title") + c.title +
			Fx::ub + lcolor + Symbols::title_right;
		}
		if (!c.title2.empty()){
			out += Mv::to(c.y + c.height - 1, c.x + 2) + Symbols::title_left + Theme::c("title") + c.title2 +
			Fx::ub + lcolor + Symbols::title_right;
		}

		return out + Fx::reset + Mv::to(c.y + 1, c.x + 2);
	}

	//* Class holding a percentage meter
	class Meter {
		string out, color_gradient;
		int width = 0;
		bool invert = false;
		vector<string> cache;
	public:
		//* Set meter options
		void operator()(int width, string color_gradient, bool invert = false) {
			this->width = width;
			this->color_gradient = color_gradient;
			this->invert = invert;
			out.clear();
			cache.clear();
			cache.insert(cache.begin(), 101, "");
		}

		//* Return a string representation of the meter with given value
		string operator()(int value) {
			if (width < 1) return out;
			value = clamp(value, 0, 100);
			if (!cache.at(value).empty()) return out = cache.at(value);
			out.clear();
			int y;
			for (int i : iota(1, width + 1)) {
				y = round((double)i * 100.0 / width);
				if (value >= y)
					out += Theme::g(color_gradient)[invert ? 100 - y : y] + Symbols::meter;
				else {
					out += Theme::c("meter_bg") + Symbols::meter * (width + 1 - i);
					break;
				}
			}
			out += Fx::reset;
			return cache.at(value) = out;
		}

		string operator()() {
			return out;
		}

	};

	//* Class holding a graph
	class Graph {
		string out, color_gradient;
		int width = 0, height = 0, lowest = 0;
		long long last = 0, max_value = 0, offset = 0;
		bool current = true, no_zero = false, invert = false, data_same = true;
		unordered_flat_map<bool, vector<string>> graphs = { {true, {}}, {false, {}}};
		unordered_flat_map<float, string> graph_symbol;

		//* Create two representations of the graph to switch between to represent two values for each braille character
		void _create(vector<long long>& data, int data_offset) {
			bool mult = (data.size() - data_offset > 1);
			if (mult && (data.size() - data_offset) % 2 != 0) data_offset--;
			unordered_flat_map<string, long long> shifter;
			unordered_flat_map<string, int> result;
			long long data_value = 0;
			if (mult && data_offset > 0) {
				last = data[data_offset - 1];
				if (max_value > 0) last = clamp((last + offset) * 100 / max_value, 0ll, 100ll);
			}
			for (int horizon : iota(0, height)){
				long long cur_high = (height > 1) ? round(100.0 * (height - horizon) / height) : 100;
				long long cur_low = (height > 1) ? round(100.0 * (height - (horizon + 1)) / height) : 0;
				for (int i = data_offset; i < (int)data.size(); i++) {
					if (mult) current = !current;
					if (i == -1) { data_value = 0; last = 0; }
					else data_value = data[i];
					if (max_value > 0) data_value = clamp((data_value + offset) * 100 / max_value, 0ll, 100ll);
					shifter = { {"left", last}, {"right", data_value} };
					for (auto [side, value] : shifter) {
						if (value >= cur_high)
							result[side] = 4;
						else if (value <= cur_low)
							result[side] = 0;
						else {
							if (height == 1) result[side] = round((float)value * 4 / 100 + 0.3);
							else result[side] = round((float)(value - cur_low) * 4 / (cur_high - cur_low) + 0.1);
						}
						if (no_zero && horizon == height - 1 && i != -1 && result[side] == 0) result[side] = 1;
					}
					if (mult && i > data_offset) last = data_value;
					graphs[current][horizon] += graph_symbol[(float)result["left"] + (float)result["right"] / 10];
				}
			}
			last = data_value;
			if (height == 1)
				out = (last < 1 ? Theme::c("inactive_fg") : Theme::g(color_gradient)[last]) + graphs[current][0];
			else {
				out.clear();
				for (int i : iota(0, height)) {
					if (i > 0) out += Mv::d(1) + Mv::l(width);
					out += (invert) ? Theme::g(color_gradient)[i * 100 / (height - 1)] : Theme::g(color_gradient)[100 - (i * 100 / (height - 1))];
					out += (invert) ? graphs[current][ (height - 1) - i] : graphs[current][i];
				}
			}
			out += Fx::reset;
		}

	public:
		//* Set graph options and initialize with data
		void operator()(int width, int height, string color_gradient, vector<long long> data, bool invert = false, bool no_zero = false, long long max_value = 0, long long offset = 0, bool data_same = true) {
			graphs[true].clear(); graphs[false].clear();
			this->width = width; this->height = height;
			this->invert = invert; this->offset = offset;
			this->no_zero = no_zero; this->max_value = max_value;
			this->color_gradient = color_gradient;
			this->data_same = data_same;
			if (height == 1) graph_symbol = (invert) ? Symbols::graph_down_small : Symbols::graph_up_small;
			else graph_symbol = (invert) ? Symbols::graph_down : Symbols::graph_up;
			if (no_zero) lowest = 1;
			current = true;
			int value_width = ceil((float)data.size() / 2);
			int data_offset = 0;
			if (value_width > width) data_offset = data.size() - width * 2;

			//? Populate the two switching graph vectors and fill empty space if width > data size
			for (int i : iota(0, height)) {
				(void) i;
				graphs[true].push_back((value_width < width) ? graph_symbol[0.0] * (width - value_width) : "");
				graphs[false].push_back((value_width < width) ? graph_symbol[0.0] * (width - value_width) : "");
			}
			if (data.size() == 0) return;
			this->_create(data, data_offset);
		}

		//* Add <num> number of values from back of <data> and return string representation of graph
		string operator()(vector<long long>& data, int num = 1) {
			if (data_same) {data_same = false; return out;}
			current = !current;

			//? Make room for new character(s) on graph
			for (int i : iota(0, height)) {
				int y = 0;
				while (y++ < num) {
					if (graphs[current][i].starts_with(Fx::e)) graphs[current][i].erase(0, 4);
					else graphs[current][i].erase(0, 3);
				}
			}
			this->_create(data, (int)data.size() - num);
			return out;
		}

		//* Return string representation of graph
		string operator()() {
			data_same = false;
			return out;
		}
	};

}

namespace Box {



}

namespace Proc {

	// Draw::BoxConf box;

}



#endif