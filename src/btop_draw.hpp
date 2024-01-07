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

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <deque>

using std::array;
using std::deque;
using std::string;
using std::vector;

namespace Symbols {
	const string h_line				= "─";
	const string v_line				= "│";
	const string dotted_v_line		= "╎";
	const string left_up			= "┌";
	const string right_up			= "┐";
	const string left_down			= "└";
	const string right_down			= "┘";
	const string round_left_up		= "╭";
	const string round_right_up		= "╮";
	const string round_left_down	= "╰";
	const string round_right_down	= "╯";
	const string title_left_down	= "┘";
	const string title_right_down	= "└";
	const string title_left			= "┐";
	const string title_right		= "┌";
	const string div_right			= "┤";
	const string div_left			= "├";
	const string div_up				= "┬";
	const string div_down			= "┴";


	const string up = "↑";
	const string down = "↓";
	const string left = "←";
	const string right = "→";
	const string enter = "↵";
}

namespace Draw {

	//* Generate if needed and return the btop++ banner
	string banner_gen(int y=0, int x=0, bool centered=false, bool redraw=false);

	//* An editable text field
	class TextEdit {
		size_t pos{};
		size_t upos{};
		bool numeric;
	public:
		string text;
		TextEdit();
		TextEdit(string text, bool numeric=false);
		bool command(const string& key);
		string operator()(const size_t limit=0);
		void clear();
	};

	//* Create a box and return as a string
	string createBox(const int x, const int y, const int width,
		const int height, string line_color = "", bool fill = false,
		const string title = "", const string title2 = "", const int num = 0);

	bool update_clock(bool force = false);

	//* Class holding a percentage meter
	class Meter {
		int width;
		string color_gradient;
		bool invert;
		array<string, 101> cache;
	public:
		Meter();
		Meter(const int width, const string& color_gradient, bool invert = false);

		//* Return a string representation of the meter with given value
		string operator()(int value);
	};

	//* Class holding a percentage graph
	class Graph {
		int width, height;
		string color_gradient;
		string out, symbol = "default";
		bool invert, no_zero;
		long long offset;
		long long last = 0, max_value = 0;
		bool current = true, tty_mode = false;
		std::unordered_map<bool, vector<string>> graphs = { {true, {}}, {false, {}}};

		//* Create two representations of the graph to switch between to represent two values for each braille character
		void _create(const deque<long long>& data, int data_offset);

	public:
		Graph();
		Graph(int width, int height,
			const string& color_gradient,
			const deque<long long>& data,
			const string& symbol="default",
			bool invert=false, bool no_zero=false,
			long long max_value=0, long long offset=0);

		//* Add last value from back of <data> and return string representation of graph
		string& operator()(const deque<long long>& data, bool data_same=false);

		//* Return string representation of graph
		string& operator()();
	};

	//* Calculate sizes of boxes, draw outlines and save to enabled boxes namespaces
	void calcSizes();
}

namespace Proc {
	extern Draw::TextEdit filter;
	extern std::unordered_map<size_t, Draw::Graph> p_graphs;
	extern std::unordered_map<size_t, int> p_counters;
}
