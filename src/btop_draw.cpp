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

#include <array>
#include <map>
#include <ranges>
#include <algorithm>
#include <cmath>

#include <btop_draw.hpp>
#include <btop_config.hpp>
#include <btop_theme.hpp>
#include <btop_tools.hpp>

using 	robin_hood::unordered_flat_map, std::round, std::views::iota,
		std::string_literals::operator""s, std::clamp, std::array, std::floor;

namespace rng = std::ranges;

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

	const unordered_flat_map<string, vector<string>> graph_symbols = {
		{ "braille_up", {
			" ", "⢀", "⢠", "⢰", "⢸",
			"⡀", "⣀", "⣠", "⣰", "⣸",
			"⡄", "⣄", "⣤", "⣴", "⣼",
			"⡆", "⣆", "⣦", "⣶", "⣾",
			"⡇", "⣇", "⣧", "⣷", "⣿"
		}},
		{"braille_down", {
			" ", "⠈", "⠘", "⠸", "⢸",
			"⠁", "⠉", "⠙", "⠹", "⢹",
			"⠃", "⠋", "⠛", "⠻", "⢻",
			"⠇", "⠏", "⠟", "⠿", "⢿",
			"⡇", "⡏", "⡟", "⡿", "⣿"
		}},
		{"block_up", {
			" ", "▗", "▗", "▐", "▐",
			"▖", "▄", "▄", "▟", "▟",
			"▖", "▄", "▄", "▟", "▟",
			"▌", "▙", "▙", "█", "█",
			"▌", "▙", "▙", "█", "█"
		}},
		{"block_down", {
			" ", "▝", "▝", "▐", "▐",
			"▘", "▀", "▀", "▜", "▜",
			"▘", "▀", "▀", "▜", "▜",
			"▌", "▛", "▛", "█", "█",
			"▌", "▛", "▛", "█", "█"
		}},
		{"tty_up", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}},
		{"tty_down", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}}
	};

}

namespace Draw {

	using namespace Tools;

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
				Mv::to(c.y, c.x + c.width - 1) + Symbols::right_up +
				Mv::to(c.y + c.height - 1, c.x) + Symbols::left_down +
				Mv::to(c.y + c.height - 1, c.x + c.width - 1) + Symbols::right_down;

		//* Draw titles if defined
		if (!c.title.empty()){
			out += Mv::to(c.y, c.x + 2) + Symbols::title_left + Fx::b + numbering + Theme::c("title") + c.title +
			Fx::ub + lcolor + Symbols::title_right;
		}
		if (!c.title2.empty()){
			out += Mv::to(c.y + c.height - 1, c.x + 2) + Symbols::title_left + Theme::c("title") + c.title2 +
			Fx::ub + lcolor + Symbols::title_right;
		}

		return out + Fx::reset + Mv::to(c.y + 1, c.x + 1);
	}


	void Meter::operator()(int width, string color_gradient, bool invert) {
		this->width = width;
		this->color_gradient = color_gradient;
		this->invert = invert;
		cache.clear();
		cache.insert(cache.begin(), 101, "");
	}

	string Meter::operator()(int value) {
		if (width < 1) return "";
		value = clamp(value, 0, 100);
		if (!cache.at(value).empty()) return cache.at(value);
		string& out = cache.at(value);
		for (int i : iota(1, width + 1)) {
			int y = round((double)i * 100.0 / width);
			if (value >= y)
				out += Theme::g(color_gradient)[invert ? 100 - y : y] + Symbols::meter;
			else {
				out += Theme::c("meter_bg") + Symbols::meter * (width + 1 - i);
				break;
			}
		}
		out += Fx::reset;
		return out;
	}

	void Graph::_create(const vector<long long>& data, int data_offset) {
		const bool mult = (data.size() - data_offset > 1);
		if (mult && (data.size() - data_offset) % 2 != 0) data_offset--;
		auto& graph_symbol = Symbols::graph_symbols.at(symbol + '_' + (invert ? "down" : "up"));
		array<int, 2> result;
		const float mod = (height == 1) ? 0.3 : 0.1;
		long long data_value = 0;
		if (mult && data_offset > 0) {
			last = data[data_offset - 1];
			if (max_value > 0) last = clamp((last + offset) * 100 / max_value, 0ll, 100ll);
		}

		//? Horizontal iteration over values in <data>
		for (int i : iota(data_offset, (int)data.size())) {
			if (tty_mode && mult && i % 2 != 0) continue;
			else if (!tty_mode) current = !current;
			if (i == -1) { data_value = 0; last = 0; }
			else data_value = data[i];
			if (max_value > 0) data_value = clamp((data_value + offset) * 100 / max_value, 0ll, 100ll);
			//? Vertical iteration over height of graph
			for (int horizon : iota(0, height)){
				int cur_high = (height > 1) ? round(100.0 * (height - horizon) / height) : 100;
				int cur_low = (height > 1) ? round(100.0 * (height - (horizon + 1)) / height) : 0;
				//? Calculate previous + current value to fit two values in 1 braille character
				int ai = 0;
				for (auto value : {last, data_value}) {
					if (value >= cur_high)
						result[ai++] = 4;
					else if (value <= cur_low)
						result[ai++] = 0;
					else {
						result[ai++] = round((float)(value - cur_low) * 4 / (cur_high - cur_low) + mod);
						if (no_zero && horizon == height - 1 && i != -1 && result[ai] == 0) result[ai] = 1;
					}
				}
				//? Generate braille symbol from 5x5 2D vector
				graphs[current][horizon] += (height == 1 && result[0] + result[1] == 0) ? Mv::r(1) : graph_symbol[(result[0] * 5 + result[1])];
			}
			if (mult && i > data_offset) last = data_value;

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


	void Graph::operator()(int width, int height, string color_gradient, const vector<long long>& data, string symbol, bool invert, bool no_zero, long long max_value, long long offset) {
		graphs[true].clear(); graphs[false].clear();
		this->width = width; this->height = height;
		this->invert = invert; this->offset = offset;
		this->no_zero = no_zero;
		this->color_gradient = color_gradient;
		if (Config::getB("tty_mode") || symbol == "tty") {
			tty_mode = true;
			this->symbol = "tty";
		}
		else if (symbol != "default" && v_contains(Config::valid_graph_symbols, symbol)) this->symbol = symbol;
		else this->symbol = Config::getS("graph_symbol");
		if (max_value == 0 && offset > 0) max_value = 100;
		this->max_value = max_value;
		int value_width = ceil((float)data.size() / 2);
		int data_offset = 0;
		if (value_width > width) data_offset = data.size() - width * 2;

		//? Populate the two switching graph vectors and fill empty space if data size < width
		for (int i : iota(0, height * 2)) {
			graphs[(i % 2 != 0)].push_back((value_width < width) ? ((height == 1) ? Mv::r(1) : " "s) * (width - value_width) : "");
		}
		if (data.size() == 0) return;
		this->_create(data, data_offset);
	}


	string& Graph::operator()(const vector<long long>& data, bool data_same) {
		if (data_same) return out;

		//? Make room for new characters on graph
		bool select_graph = (tty_mode ? current : !current);
		for (int i : iota(0, height)) {
			if (graphs[select_graph][i].starts_with(Fx::e)) graphs[current][i].erase(0, 4);
			else graphs[select_graph][i].erase(0, 3);
		}
		this->_create(data, (int)data.size() - 1);
		return out;
	}

	string& Graph::operator()() {
		return out;
	}

}

namespace Box {




}

namespace Proc {

	// Draw::BoxConf box;

}