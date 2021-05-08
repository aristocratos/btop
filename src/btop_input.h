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
#include <map>
#include <chrono>
#include <thread>
#include <iostream>

#include <btop_globs.h>
#include <btop_tools.h>

using std::string, std::map, std::cin;


//* Class for handling keyboard and mouse input
class C_Input {

	string last = "";

	//* Map for translating key codes to readable values
	const map<string, string> Key_escapes = {
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

	bool wait(int timeout=0){
		if (timeout == 0) return cin.rdbuf()->in_avail() > 0;
		auto timer = 0;
		while (timeout == -1 || timer * 10 <= timeout) {
			if (cin.rdbuf()->in_avail() > 0) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (timeout >= 0) ++timer;
		}
		return false;
	}

	string get(){
		string key = "";
		while (cin.rdbuf()->in_avail() > 0 && key.size() < 100) key += cin.get();
		if (key != ""){
			if (key.substr(0,2) == Fx::e) key.erase(0, 1);
			if (Key_escapes.contains(key)) key = Key_escapes.at(key);
			else if (ulen(key) > 1) key = "";
		}
		return key;
	}

public:

	//* Wait <timeout> ms for input on stdin and return true if available
	//* 0 to just check for input
	//* -1 for infinite wait
	bool operator()(int timeout){
		if (wait(timeout)) {
			last = get();
			return last != "";
		} else {
			last = "";
			return false;
		}
	}

	//* Return last entered key
	string operator()(){
		return last;
	}
};

#endif