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
#include <map>
#include <ranges>

#include <btop_globs.h>
#include <btop_config.h>
#include <btop_tools.h>

#ifndef _btop_draw_included_
#define _btop_draw_included_ 1

using std::string, std::vector, std::map, std::round, std::views::iota;

namespace Box {

	struct Conf {
		uint x=0, y=0;
		uint width=0, height=0;
		string line_color = "", title = "", title2 = "";
		bool fill = true;
		uint num=0;
	};


	string draw(Conf c){
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

		return out + Fx::reset + Mv::to(c.y + 1, c.x + 1);
	}

}

namespace Proc {

	// Box::Conf box;

}



#endif