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
#include <atomic>
#include <vector>
#include <bitset>

#include "btop_input.hpp"

using std::atomic;
using std::bitset;
using std::string;
using std::vector;

namespace Menu {

	extern atomic<bool> active;
	extern string output;
	extern int signalToSend;
	extern bool redraw;

	//? line, col, height, width
	extern std::unordered_map<string, Input::Mouse_loc> mouse_mappings;

	//* Creates a message box centered on screen
	//? Height of box is determined by size of content vector
	//? Boxtypes: 0 = OK button | 1 = YES and NO with YES selected | 2 = Same as 1 but with NO selected
	//? Strings in content vector is not checked for box width overflow
	class msgBox {
		string box_contents, button_left, button_right;
		int height{};
		int width{};
		int boxtype{};
		int selected{};
		int x{};
		int y{};
	public:
		enum BoxTypes { OK, YES_NO, NO_YES };
		enum msgReturn {
			Invalid,
			Ok_Yes,
			No_Esc,
			Select
		};
		msgBox();
		msgBox(int width, int boxtype, vector<string> content, string title);

		//? Draw and return box as a string
		string operator()();

		//? Process input and returns value from enum Ret
		int input(string key);

		//? Clears content vector and private strings
		void clear();
	};

	extern bitset<8> menuMask;

	//* Enum for functions in vector menuFuncs
	enum Menus {
		SizeError,
		SignalChoose,
		SignalSend,
		SignalReturn,
		Options,
		Help,
		Main
	};

	//* Handles redirection of input for menu functions and handles return codes
	void process(string key="");

	//* Show a menu from enum Menu::Menus
	void show(int menu, int signal=-1);

}
