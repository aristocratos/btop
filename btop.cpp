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

#include <cstdlib>
#include <cstdio>
#include <limits>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <vector>
#include <map>
#include <thread>
#include <future>
#include <atomic>
#include <string_view>
#include <chrono>
#include <ranges>

#include <unistd.h>
#include <poll.h>

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>

#if defined(__linux__)
	#define SYSTEM "linux"
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
	#include <sys/param.h>
	#if defined(BSD)
		#define SYSTEM "bsd"
	#else
		#define SYSTEM "unknown"
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC == 1
		#define SYSTEM "osx"
	#else
		#define SYSTEM "unknown"
    #endif
#else
    #define SYSTEM "unknown"
#endif

using namespace std;


//? ------------------------------------------------- GLOBALS ---------------------------------------------------------

namespace Global {
	const vector<vector<string>> Banner_src = {
		{"#E62525", "██████╗ ████████╗ ██████╗ ██████╗"},
		{"#CD2121", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗   ██╗    ██╗"},
		{"#B31D1D", "██████╔╝   ██║   ██║   ██║██████╔╝ ██████╗██████╗"},
		{"#9A1919", "██╔══██╗   ██║   ██║   ██║██╔═══╝  ╚═██╔═╝╚═██╔═╝"},
		{"#801414", "██████╔╝   ██║   ╚██████╔╝██║        ╚═╝    ╚═╝"},
		{"#000000", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝"},
	};

	const string Version = "0.0.1";
}


//? --------------------------------------- FUNCTIONS, STRUCTS & CLASSES ----------------------------------------------

//* A simple argument parser
void argumentParser(int argc, char **argv){
	string argument;
	for(int i = 1; i < argc; i++) {
		argument = argv[i];
		if (argument == "-v" || argument == "--version") {
			cout << "btop version: " << Global::Version << endl;
			exit(0);
		} else if (argument == "-h" || argument == "--help") {
			cout << "help here" << endl;
			exit(0);
		} else {
			cout << " Unknown argument: " << argument << "\n" <<
			" Use -h or --help for help." <<  endl;
			exit(1);
		}
	}
}

//* Functions and variables for handling keyboard and mouse input
class C_Key {
	const map<string, string> KEY_ESCAPES = {
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
		struct pollfd pls[1];
		pls[ 0 ].fd = STDIN_FILENO;
		pls[ 0 ].events = POLLIN | POLLPRI;
		return poll(pls, 1, timeout) > 0;
	}

	string get(){
		string key = "";
		while (wait() && key.size() < 100) key += cin.get();
		if (key != ""){
			if (key.starts_with(Fx::e)) key.erase(0, 1);
			if (KEY_ESCAPES.count(key)) key = KEY_ESCAPES.at(key);
			else if (ulen(key) > 1) key = "";
		}
		return key;
	}
public:
	string last = "";

	//* Wait <timeout> ms for input on stdin and return true if available
	//* 0 to just check for input
	//* -1 for infinite wait
	bool operator()(int timeout){
		if (wait(timeout)) {
			last = get();
			return true;
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

class C_Theme {
	map<string, string> c;
	map<string, vector<string>> g;
	C_Config conf;

	map<string,string> generate(map<string, string>& source){
		map<string, string> out;
		vector<string> t_rgb;
		string depth;
		for (auto& item : Global::Default_theme) {
			depth = (item.first.ends_with("bg")) ? "bg" : "fg";
			if (source.count(item.first)) {
				if (source.at(item.first)[0] == '#') out[item.first] = hex_to_color(source.at(item.first), !State::truecolor, depth);
				else {
					t_rgb = ssplit(source.at(item.first), " ");
					out[item.first] = dec_to_color(stoi(t_rgb[0]), stoi(t_rgb[1]), stoi(t_rgb[2]), !State::truecolor, depth);
				}
			}
			else out[item.first] = "";
			if (out[item.first] == "") out[item.first] = hex_to_color(item.second, !State::truecolor, depth);
		}
		return out;
	}

public:

	//* Change to theme using <source> map
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

struct C_Banner {
	string banner_str;
	int width;

	C_Banner(){
		size_t z = 0;
		string b_color, bg, fg, out, oc, letter;
		int bg_i;
		int new_len;
		banner_str = "";
		for (auto line: Global::Banner_src) {
			new_len = ulen(line[1]);
			if (new_len > width) width = new_len;
			fg = hex_to_color(line[0]);
			bg_i = 120-z*12;
			bg = dec_to_color(bg_i, bg_i, bg_i);
			for (unsigned i = 0; i < line[1].size(); i += 3) {
				if (line[1][i] == ' '){
					letter = ' ';
					i -= 2;
				} else{
					letter = line[1].substr(i, 3);
				}
				b_color = (letter == "█") ? fg : bg;
				if (b_color != oc) out += b_color;
				out += letter;
				oc = b_color;
			}
			z++;
			if (z < Global::Banner_src.size()) out += Mv::l(ulen(line[1])) + Mv::d(1);
		}
		banner_str = out + Mv::r(18 - Global::Version.size()) + Fx::i + dec_to_color(0,0,0, State::truecolor, "bg") + dec_to_color(150, 150, 150) + "v" + Global::Version;
	}

	//* Returns the pre-generated btop++ banner
	string operator() (){
		return banner_str + Fx::reset;
	}
};



//? --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv){
	int debug = 0;
	int tests = 0;

	//? Init

	cout.setf(std::ios::boolalpha);

	if (argc > 1) argumentParser(argc, argv);

	C_Term Term;

	if (!Term.initialized) {
		cout << "No terminal detected!" << endl;
		exit(1);
	}


	C_Config Config;
	C_Theme Theme(Global::Default_theme);
	C_Banner Banner;
	C_Key Key;

	cout << Theme("main_bg") << Term.clear << flush;
	// bool thread_test = false;


	if (debug < 2) cout << Term.alt_screen << Term.clear << Term.hide_cursor << flush;

	cout << Theme("main_fg") << endl;

	cout << Mv::r(Term.width / 2 - Banner.width / 2) << Banner() << endl;


	//* Test MENUS
	for (auto& outer : Global::Menus){
		for (auto& inner : outer.second){
			for (auto& item : inner.second){
				cout << item << endl;
			}
		}
	}


	string korv5 = "hejsan";
	if (korv5.starts_with("hej")) cout << "hej" << endl;


	//cout << korv2.size() << " " << ulen(korv2) << endl;

	cout << Config(Bool, "truecolor") << endl;
	cout << Config(Int, "tree_depth") << endl;
	cout << Config(String, "color_theme") << endl;

	//* Test theme
	int i = 0;
	if (tests==0) for(auto& item : Global::Default_theme) {
		cout << Theme(item.first) << item.first << ":" << Theme("main_fg") << Theme(item.first).erase(0, 2) << Fx::reset << "  ";
		if (++i == 4) {
			i = 0;
			cout << endl;
		}
	}

	cout << Fx::reset << endl;

	// if (thread_test){
	// 	int max = 50000;
	// 	int count = max / 100;
	// 	atomic<bool> running;
	// 	running = true;
	// 	thread ttg1(C_Theme::generate, DEFAULT_THEME);

	// 	for (int i = 0; i < max; i++) {
	// 		// C_Theme tt(DEFAULT_THEME);
	// 		// tt.del();
	// 		auto ttg1 = async(C_Theme::generate, DEFAULT_THEME);
	// 		auto ttg2 = async(C_Theme::generate, DEFAULT_THEME);
	// 		auto ttg3 = async(C_Theme::generate, DEFAULT_THEME);
	// 		auto ttg4 = async(C_Theme::generate, DEFAULT_THEME);
	// 		// ttg1.wait();
	// 		// ttg2.wait();
	// 		map<string, string> tt1 = ttg1.get();
	// 		map<string, string> tt2 = ttg2.get();
	// 		map<string, string> tt3 = ttg3.get();
	// 		map<string, string> tt4 = ttg4.get();
	// 		if (i >= count) {
	// 			cout << Mv::restore << "(" << i * 100 / max << "%)" << flush;
	// 			count += max / 100;
	// 		}
	// 	}
	// }



	if (tests>1){
		string lk = "first second another lastone";

		for (auto& it : ssplit(lk)){
			cout << it << flush;
			switch (it.front()) {
				case 's': cout << " <=" << endl; break;
				default: cout << endl;
			}
		}
	}


	if (tests>3){
		auto nbcolor = hex_to_color(Global::Default_theme.at("net_box"));
		auto nbcolor_rgb = c_to_rgb(nbcolor);
		auto nbcolor_man = ssplit(nbcolor, ";");
		cout << nbcolor << "Some color" << endl;
		cout << "nbcolor_rgb size=" << nbcolor_rgb.size() << endl;
		cout << "R:" << nbcolor_rgb.at("r") << " G:" << nbcolor_rgb.at("g") << " B:" << nbcolor_rgb.at("b") << endl;
		cout << "MANUAL R:" << nbcolor_man.at(2) << " G:" << nbcolor_man.at(3) << " B:" << nbcolor_man.at(4) << endl;

		auto ccc = dec_to_color(100, 255, 100);
		cout << "\n" << ccc << "Testing..." << endl;
	}


	if (tests>4){
		string trim_test1 = "-*vad ";
		string trim_test2 = " vad*-";
		string trim_test3 = trim_test1 + trim_test2;

		cout << "\"" << ltrim(trim_test1, "-*") << "\" \"" << rtrim(trim_test2, "*-") << "\" \"" << trim(trim_test3, "-") << "\"" << endl;


		string testie = "Does this work as intended?    Or?";
		auto t_vec = ssplit(testie);
		for(auto& tp : t_vec){
			cout << "\"" << tp << "\" " << flush;
		}
	}


	//if (tests>5){
		cout << "Width=" << Term.width << " Height=" << Term.height << endl;
	//}

	// map<string, string> dict = {
	// 	{"Korv", "14"},
	// 	{"Vad", "13"}
	// };

	// cout << dict["Korv"] << ", " << dict["Vad"] << endl;

	// vector<map<string, int>> test = {
	// 	{{"first", 1}, {"second", 2}},
	// 	{{"first", 11}, {"second", 22}}
	// };

	//cout << test[0]["first"] << " " << test[1]["second"] << endl;

	// for (auto& m : test) {
	// 	cout << endl;
	// 	for (auto& item : m) {
	// 		cout << item.first << " " << item.second << endl;
	// 	}
	// }



	if (debug == 0){
		cout << Theme("main_fg");
		cout << Mv::to(Term.height - 1, 0) << "Press q to exit! Timeout" << flush;
		string full;
		int wt = 90;
		bool qp = false;
		while (!qp && wt >= 0){
			int wtm = wt / 60;
			int wts = wt - wtm * 60;
			wt--;
			cout << Mv::to(Term.height - 1, 26) << "(" << wtm << ":" << wts << ")    " << flush;
			//chr = Key(1000);
			if (Key(1000)) {
				cout << Mv::to(Term.height - 2, 1) << "Last key: LEN=" << Key().size() << " ULEN=" << ulen(Key()) << " KEY=\"" << Key() << "\" CODE=" << (int)Key().at(0) << "        " << flush;
				full += Key();
				cout << Mv::to(Term.height - 5, 1) << full << flush;
				if (Key() == "q") qp = true;
				wt++;
			}
		}
	}

	if (debug == 1) Key(-1);
	Term.restore();
	if (debug < 2) cout << Term.normal_screen << Term.show_cursor << flush;
	return 0;
}
