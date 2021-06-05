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

#ifndef _btop_input_included_
#define _btop_input_included_

#include <string>
#include <robin_hood.h>
#include <iostream>

#include <btop_tools.h>

using std::string, robin_hood::unordered_flat_map, std::cin;
using namespace Tools;

/* The input functions relies on the following std::cin options being set:
	cin.sync_with_stdio(false);
	cin.tie(NULL);
	These will automatically be set when running Term::init() from btop_tools.h
*/

//* Functions and variables for handling keyboard and mouse input
namespace Input {
	namespace {
		//* Map for translating key codes to readable values
		const unordered_flat_map<string, string> Key_escapes = {
			{"\033",	"escape"},
			{"\n",		"enter"},
			{" ",		"space"},
			{"\x7f",	"backspace"},
			{"\x08",	"backspace"},
			{"[A", 		"up"},
			{"OA",		"up"},
			{"[B", 		"down"},
			{"OB",		"down"},
			{"[D", 		"left"},
			{"OD",		"left"},
			{"[C", 		"right"},
			{"OC",		"right"},
			{"[2~",		"insert"},
			{"[3~",		"delete"},
			{"[H",		"home"},
			{"[F",		"end"},
			{"[5~",		"page_up"},
			{"[6~",		"page_down"},
			{"\t",		"tab"},
			{"[Z",		"shift_tab"},
			{"OP",		"f1"},
			{"OQ",		"f2"},
			{"OR",		"f3"},
			{"OS",		"f4"},
			{"[15~",	"f5"},
			{"[17~",	"f6"},
			{"[18~",	"f7"},
			{"[19~",	"f8"},
			{"[20~",	"f9"},
			{"[21~",	"f10"},
			{"[23~",	"f11"},
			{"[24~",	"f12"}
		};
	}

	//* Last entered key
	string last = "";

	//* Poll keyboard & mouse input for <timeout> ms and return input availabilty as a bool
	bool poll(int timeout=0){
		if (timeout < 1) return cin.rdbuf()->in_avail() > 0;
		while (timeout > 0) {
			if (cin.rdbuf()->in_avail() > 0) return true;
			sleep_ms(timeout < 10 ? timeout : 10);
			timeout -= 10;
		}
		return false;
	}

	//* Get a key or mouse action from input
	string get(){
		string key;
		while (cin.rdbuf()->in_avail() > 0 && key.size() < 100) key += cin.get();
		if (!key.empty()){
			if (key.substr(0,2) == Fx::e) key.erase(0, 1);
			if (Key_escapes.contains(key)) key = Key_escapes.at(key);
			else if (ulen(key) > 1) key = "";
			last = key;
		}
		return key;
	}

	//* Wait until input is available
	void wait(bool clear=false){
		while (cin.rdbuf()->in_avail() < 1) sleep_ms(10);
		if (clear) get();
	}

	void clear(){
		last.clear();
	}

}

#endif