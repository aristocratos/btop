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
#include <robin_hood.h>

using std::string, std::vector, robin_hood::unordered_flat_map;

namespace Symbols {
	extern const string h_line;
	extern const string v_line;
	extern const string left_up;
	extern const string right_up;
	extern const string left_down;
	extern const string right_down;
	extern const string title_left;
	extern const string title_right;
	extern const string div_up;
	extern const string div_down;
}

namespace Draw {

	struct BoxConf {
		uint x=0, y=0;
		uint width=0, height=0;
		string line_color = "", title = "", title2 = "";
		bool fill = true;
		uint num=0;
		uint w_percent=0, h_percent=0;
	};

	//* Create a box using values from a BoxConf struct and return as a string
	string createBox(BoxConf c);

	//* Class holding a percentage meter
	class Meter {
		string color_gradient;
		int width = 0;
		bool invert = false;
		vector<string> cache;
	public:
		//* Set meter options
		void operator()(int width, string color_gradient, bool invert = false);

		//* Return a string representation of the meter with given value
		string operator()(int value);
	};

	//* Class holding a graph
	class Graph {
		string out, color_gradient, symbol = "default";
		int width = 0, height = 0;
		long long last = 0, max_value = 0, offset = 0;
		bool current = true, no_zero = false, invert = false, tty_mode = false;
		unordered_flat_map<bool, vector<string>> graphs = { {true, {}}, {false, {}}};

		//* Create two representations of the graph to switch between to represent two values for each braille character
		void _create(const vector<long long>& data, int data_offset);

	public:
		//* Set graph options and initialize with data
		void operator()(int width, int height, string color_gradient, const vector<long long>& data, string symbol = "default", bool invert = false, bool no_zero = false, long long max_value = 0, long long offset = 0);

		//* Add last value from back of <data> and return string representation of graph
		string& operator()(const vector<long long>& data, bool data_same = false);

		//* Return string representation of graph
		string& operator()();
	};

}

namespace Box {




}

namespace Proc {

	// Draw::BoxConf box;

}