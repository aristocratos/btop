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
#include <robin_hood.h>
#include <ranges>
#include <algorithm>
#include <cmath>

#include <btop_config.h>
#include <btop_tools.h>

#ifndef _btop_draw_included_
#define _btop_draw_included_ 1

using std::string, std::vector, robin_hood::unordered_flat_map, std::round, std::views::iota, std::string_literals::operator""s, std::clamp;

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

	const unordered_flat_map<bool, string> graph_00 = { {true, Mv::r(1)}, {false, " "} };

	unordered_flat_map<float, string> graph_up = {
		{0.0, " "}, {0.1, "⢀"}, {0.2, "⢠"}, {0.3, "⢰"}, {0.4, "⢸"},
		{1.0, "⡀"}, {1.1, "⣀"}, {1.2, "⣠"}, {1.3, "⣰"}, {1.4, "⣸"},
		{2.0, "⡄"}, {2.1, "⣄"}, {2.2, "⣤"}, {2.3, "⣴"}, {2.4, "⣼"},
		{3.0, "⡆"}, {3.1, "⣆"}, {3.2, "⣦"}, {3.3, "⣶"}, {3.4, "⣾"},
		{4.0, "⡇"}, {4.1, "⣇"}, {4.2, "⣧"}, {4.3, "⣷"}, {4.4, "⣿"}
	};

	unordered_flat_map<float, string> graph_down = {
		{0.0, " "}, {0.1, "⠈"}, {0.2, "⠘"}, {0.3, "⠸"}, {0.4, "⢸"},
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
		int width = 10;
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

}

namespace Box {



}

namespace Proc {

	// Draw::BoxConf box;

}



#endif