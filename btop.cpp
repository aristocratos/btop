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

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>

using namespace std;

//? ------------------------------------------------- GLOBALS ---------------------------------------------------------

const vector<vector<string>> BANNER_SRC = {
	{"#E62525", "██████╗ ████████╗ ██████╗ ██████╗"},
	{"#CD2121", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗   ██╗    ██╗"},
	{"#B31D1D", "██████╔╝   ██║   ██║   ██║██████╔╝ ██████╗██████╗"},
	{"#9A1919", "██╔══██╗   ██║   ██║   ██║██╔═══╝  ╚═██╔═╝╚═██╔═╝"},
	{"#801414", "██████╔╝   ██║   ╚██████╔╝██║        ╚═╝    ╚═╝"},
	{"#000000", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝"},
};

const string VERSION = "0.0.1";

const map<string, string> DEFAULT_THEME = {
	{ "main_bg", "#00" },
	{ "main_fg", "#cc" },
	{ "title", "#ee" },
	{ "hi_fg", "#969696" },
	{ "selected_bg", "#7e2626" },
	{ "selected_fg", "#ee" },
	{ "inactive_fg", "#40" },
	{ "graph_text", "#60" },
	{ "meter_bg", "#40" },
	{ "proc_misc", "#0de756" },
	{ "cpu_box", "#3d7b46" },
	{ "mem_box", "#8a882e" },
	{ "net_box", "#423ba5" },
	{ "proc_box", "#923535" },
	{ "div_line", "#30" },
	{ "temp_start", "#4897d4" },
	{ "temp_mid", "#5474e8" },
	{ "temp_end", "#ff40b6" },
	{ "cpu_start", "#50f095" },
	{ "cpu_mid", "#f2e266" },
	{ "cpu_end", "#fa1e1e" },
	{ "free_start", "#223014" },
	{ "free_mid", "#b5e685" },
	{ "free_end", "#dcff85" },
	{ "cached_start", "#0b1a29" },
	{ "cached_mid", "#74e6fc" },
	{ "cached_end", "#26c5ff" },
	{ "available_start", "#292107" },
	{ "available_mid", "#ffd77a" },
	{ "available_end", "#ffb814" },
	{ "used_start", "#3b1f1c" },
	{ "used_mid", "#d9626d" },
	{ "used_end", "#ff4769" },
	{ "download_start", "#231a63" },
	{ "download_mid", "#4f43a3" },
	{ "download_end", "#b0a9de" },
	{ "upload_start", "#510554" },
	{ "upload_mid", "#7d4180" },
	{ "upload_end", "#dcafde" },
	{ "process_start", "#80d0a3" },
	{ "process_mid", "#dcd179" },
	{ "process_end", "#d45454" }
};

map<string, map<string, vector<string>>> MENUS = {
	{ "options", {
		{ "normal",
			{ "┌─┐┌─┐┌┬┐┬┌─┐┌┐┌┌─┐",
			  "│ │├─┘ │ ││ ││││└─┐",
			  "└─┘┴   ┴ ┴└─┘┘└┘└─┘"
			} },
		{ "selected",
			{ "╔═╗╔═╗╔╦╗╦╔═╗╔╗╔╔═╗",
			  "║ ║╠═╝ ║ ║║ ║║║║╚═╗",
			  "╚═╝╩   ╩ ╩╚═╝╝╚╝╚═╝"
			} }
	} },
	{ "help", {
		{ "normal",
			{ "┬ ┬┌─┐┬  ┌─┐",
			  "├─┤├┤ │  ├─┘",
			  "┴ ┴└─┘┴─┘┴  "
			} },
		{ "selected",
			{ "╦ ╦╔═╗╦  ╔═╗",
			  "╠═╣║╣ ║  ╠═╝",
			  "╩ ╩╚═╝╩═╝╩  "
			} }
	} },
	{ "quit", {
		{ "normal",
			{ "┌─┐ ┬ ┬ ┬┌┬┐",
			  "│─┼┐│ │ │ │ ",
			  "└─┘└└─┘ ┴ ┴ "
			} },
		{ "selected",
			{ "╔═╗ ╦ ╦ ╦╔╦╗ ",
			  "║═╬╗║ ║ ║ ║  ",
			  "╚═╝╚╚═╝ ╩ ╩  "
			} }
	} }
};

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

namespace State {
	atomic<bool> MenuActive(false);
};

namespace Fx { //? Collection of escape codes for text style and formatting
	const string e = "\x1b[";		//* Escape sequence start
	const string r = e + "0m";		//* Reset foreground/background color and text effects
	const string b = e + "1m";		//* Bold on
	const string ub = e + "22m";	//* Bold off
	const string d = e + "2m";		//* Dark on
	const string ud = e + "22m";	//* Dark off
	const string i = e + "3m";		//* Italic on
	const string ui = e + "23m";	//* Italic off
	const string ul = e + "4m";		//* Underline on
	const string uul = e + "24m";	//* Underline off
	const string bl = e + "5m";		//* Blink on
	const string ubl = e + "25m";	//* Blink off
	const string s = e + "9m";		//* Strike / crossed-out on
	const string us = e + "29m";	//* Strike / crossed-out off
};

namespace Mv { //? Collection of escape codes and functions for cursor manipulation
	string to(int line, int col){ return Fx::e + to_string(line) + ";" + to_string(col) + "f";}	//* Move cursor to line, column
	string r(int x){ return Fx::e + to_string(x) + "C";}											//* Move cursor right x columns
	string l(int x){ return Fx::e + to_string(x) + "D";}											//* Move cursor left x columns
	string u(int x){ return Fx::e + to_string(x) + "A";}											//* Move cursor up x lines
	string d(int x) { return Fx::e + to_string(x) + "B";}										//* Move cursor down x lines
	const string save = Fx::e + "s";																	//* Save cursor position
	const string restore = Fx::e + "u";																//* Restore saved cursor postion
};

//? --------------------------------------- FUNCTIONS, STRUCTS & CLASSES ----------------------------------------------

inline size_t ulen(const string& str){ //? Return number of UTF8 characters in a string
		size_t len = 0;
		for (char c : str) if ((c & 0xC0) != 0x80) ++len;
		return len;
}

string color_to_string(string hexa="", unsigned r=0, unsigned g=0, unsigned b=0, string depth="fg"){
	//? Generate escape sequence for 24-bit color and return as a string
	//? Accepted arguments: hexa="#000000" to "#ffffff" for color, or "#00" to "#ff" for greyscale
	//? Or: r=0-255, g=0-255, b=0-255
	//? depth=string "fg" or "bg" for either a foreground color or a background color
	depth = (depth == "fg") ? "38" : "48";
	if (hexa.size() > 1){
		hexa.erase(0, 1);
		for (auto& c : hexa) if (!isxdigit(c)) return "";

		if (hexa.size() == 2){
			string h_str = ";" + to_string(stoi(hexa, 0, 16));
			return Fx::e + depth + ";2" + h_str + h_str + h_str + "m";
		}
		else if (hexa.size() == 6){
			return Fx::e + depth + ";2" + ";" +
			to_string(stoi(hexa.substr(0, 2), 0, 16)) + ";" +
			to_string(stoi(hexa.substr(2, 2), 0, 16)) + ";" +
			to_string(stoi(hexa.substr(4, 2), 0, 16)) + "m";
		}
	} else if (r && g && b){
		r = (r > 255) ? 255 : r;
		g = (g > 255) ? 255 : g;
		b = (b > 255) ? 255 : b;
		return Fx::e + depth + ";2" + ";" + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
	}
	return "";
}

string ltrim(string str, string t_str = " "){ //? Left-trim <t_str> from <str> and return string
	size_t t_str_size = t_str.size();
	while (str.substr(0, t_str_size) == t_str) str.erase(0, t_str_size);
	return str;
}

string rtrim(string str, string t_str = " "){ //? Right-trim <t_str> from <str> and return string
	size_t t_str_size = t_str.size();
	while (str.substr(str.size() - t_str_size, t_str_size) == t_str) str.erase(str.size() - t_str_size, t_str_size);
	return str;
}

string trim(string str, string t_str = " "){ //? Left-right-trim <t_str> from <str> and return string
	return ltrim(rtrim(str, t_str), t_str);
}

vector<string> ssplit(string str, string delim = " ", int times = 0){ //? Split <string> at <delim> <times> (0 for unlimited) times and return vector
	vector<string> out;
	if (str != "" && delim != ""){
		size_t pos = 0;
		int x = 0;
		string tmp;
		while ((pos = str.find(delim)) != string::npos){
			tmp = str.substr(0, pos);
			if (tmp != delim && tmp != "") out.push_back(tmp);
			str.erase(0, pos + delim.size());
			if (times > 0 && ++x >= times) break;
		}
	}
	out.push_back(str);
	return out;
}

map<string, int> c_to_rgb(string c_string){ //? Return a map of "r", "g", "b", 0-255 values for a 24-bit color escape string
	map<string, int> rgb = {{"r", 0}, {"g", 0}, {"b", 0}};
	if (c_string.size() >= 14){
		c_string.erase(0, 7);
		auto c_split = ssplit(c_string, ";");
		if (c_split.size() == 3){
			rgb["r"] = stoi(c_split[0]);
			rgb["g"] = stoi(c_split[1]);
			rgb["b"] = stoi(c_split[2].erase(c_split[2].size()));
		}
	}
	return rgb;
}

class C_Term { //? Collection of escape codes and functions for terminal manipulation
	bool initialized = false;
	struct termios initial_settings;
public:
	int width = 0;
	int height = 0;
	bool resized = false;
	string fg = "" ;																//* Default foreground color
	string bg = "";																	//* Default background color
	const string hide_cursor = Fx::e + "?25l";										//* Hide terminal cursor
	const string show_cursor = Fx::e + "?25h";										//* Show terminal cursor
	const string alt_screen = Fx::e + "?1049h";										//* Switch to alternate screen
	const string normal_screen = Fx::e + "?1049l";									//* Switch to normal screen
	const string clear = Fx::e + "2J" + Fx::e + "0;0f";								//* Clear screen and set cursor to position 0,0
	const string mouse_on = Fx::e + "?1002h" + Fx::e + "?1015h" + Fx::e + "?1006h"; 	//* Enable reporting of mouse position on click and release
	const string mouse_off = "\033[?1002l"; 										//* Disable mouse reporting
	const string mouse_direct_on = "\033[?1003h";									//* Enable reporting of mouse position at any movement
	const string mouse_direct_off = "\033[?1003l";									//* Disable direct mouse reporting

	bool init(){ //? Save terminal options and check valid tty
		if (!this->initialized){
			this->initialized = (bool)isatty(STDIN_FILENO);
			if (this->initialized) this->initialized = (0 == tcgetattr(STDIN_FILENO, &this->initial_settings));
			if (this->initialized) cin.sync_with_stdio();
		}
		return initialized;
	}

	void restore(){ //? Restore terminal options
		if (this->initialized) tcsetattr(STDIN_FILENO, TCSANOW, &this->initial_settings);
	}

	bool linebuffered(bool on=true){ //? Toggle need for return key when reading input
		struct termios settings;
		if (tcgetattr(STDIN_FILENO, &settings)) return false;
		if (on) settings.c_lflag |= ICANON;
		else settings.c_lflag &= ~(ICANON);
		if (tcsetattr(STDIN_FILENO, TCSANOW, &settings)) return false;
		if (on) setlinebuf(stdin);
		else setbuf(stdin, NULL);
		return true;
	}

	bool echo(bool on=true){ //? Toggle terminal input echo
		struct termios settings;
		if (tcgetattr(STDIN_FILENO, &settings)) return false;
		if (on) settings.c_lflag |= ECHO;
		else settings.c_lflag &= ~(ECHO);
		return 0 == tcsetattr(STDIN_FILENO, TCSANOW, &settings);
	}

	bool refresh(){ //? Refresh variables holding current terminal width and height and return true if resized
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		this->resized = (this->width != w.ws_col || this->height != w.ws_row) ? true : false;
		this->width = w.ws_col;
		this->height = w.ws_row;
		return resized;
	}

	C_Term() {
		this->init();
		this->refresh();
		this->resized = false;
	}

};

C_Term Term; //* Make C_Term globally available as Term

struct C_Key {
	string last = "";

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

	bool wait(unsigned timeout_ms=0){
		struct pollfd pls[1];
		pls[ 0 ].fd = STDIN_FILENO;
		pls[ 0 ].events = POLLIN | POLLPRI;
		return poll(pls, 1, timeout_ms) > 0;
	}

	string get(){
		string key = "";
		while (this->wait() && key.size() < 100) key += cin.get();
		if (key != ""){
			if (key.substr(0,2) == Fx::e) key.erase(0, 1);
			if (this->KEY_ESCAPES.count(key)) key = this->KEY_ESCAPES.at(key);
			else if (ulen(key) > 1) key = "";

			this->last = key;
		}
		return key;
	}

	string operator()(unsigned timeout_ms=0){
		if (this->wait(timeout_ms)) {
			return this->get();
		} else {
		return "";
		}
	}
};

C_Key Key;

class C_Theme {
	map<string, string> c;
	map<string, vector<string>> g;

	map<string,string> generate(map<string, string>& source){
		//? Generate theme from <source> map, default to DEFAULT_THEME on missing or malformatted values
		map<string, string> out;
		for (auto& item : DEFAULT_THEME) {
			if (source.count(item.first)) out[item.first] = color_to_string(source.at(item.first));
			else out[item.first] = "";
			if (out[item.first] == "") out[item.first] = color_to_string(item.second);
		}
		return out;
	}

public:

	void swap(map<string, string> source){
		this->c = this->generate(source);
	}

	C_Theme(map<string, string> source){
		this->c = this->generate(source);
	}

	auto operator()(string name){
		return this->c.at(name);
	}

	auto gradient(string name){
		return this->g.at(name);
	}

	auto rgb(string name){
		return c_to_rgb(this->c.at(name));
	}

};

struct C_Banner {
	string banner_str;

	C_Banner(){
		size_t z = 0;
		string b_color, bg, fg, out, oc, letter;
		this->banner_str = "";
		for (auto line: BANNER_SRC) {
			fg = color_to_string(line[0]);
			bg = color_to_string("#" + to_string(80-z*6));
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
			if (z < BANNER_SRC.size()) out += Mv::l(ulen(line[1])) + Mv::d(1);
		}
		this->banner_str = out;
	}

	string operator() (){
		return this->banner_str + Fx::r;
	}
};

C_Banner Banner;

//? ------------------------------------------------- FUNCTIONS -------------------------------------------------------

void argumentParser(int argc, char **argv){ //? A simple argument parser
	string argument;
	for(int i = 1; i < argc; i++) {
		argument = argv[i];
		if (argument == "-v" || argument == "--version") {
			cout << "btop version: " << VERSION << endl;
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

uint64_t time_ms(){
	return chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
}

//? --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv){
	int debug = 2;
	int tests = 0;

	// bool thread_test = false;

	//? Init

	if (argc > 1) argumentParser(argc, argv);

	if (!Term.init()) {
		cout << "No terminal detected!" << endl;
		exit(1);
	}
	Term.echo(false);
	Term.linebuffered(false);


	if (debug < 2) cout << Term.alt_screen << Term.clear << Term.hide_cursor << flush;

	C_Theme Theme(DEFAULT_THEME);


	//* Test MENUS
	for (auto& outer : MENUS){
		for (auto& inner : outer.second){
			for (auto& item : inner.second){
				cout << item << endl;
			}
		}
	}


	string korv5 = "hejsan";
	if (korv5.starts_with("hej")) cout << "hej" << endl;


	State::MenuActive.store(true);

	if (State::MenuActive.load()) cout << "WHUUUU and time: " << time_ms() << endl;

	//cout << korv2.size() << " " << ulen(korv2) << endl;

	//* Test theme
	if (tests>0) for(auto& item : DEFAULT_THEME) cout << Theme(item.first) << item.first << endl;

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


	// if (tests>2){
		cout << "        " << Banner() << endl;
	// }

	if (tests>3){
		auto nbcolor = color_to_string(DEFAULT_THEME.at("net_box"));
		auto nbcolor_rgb = c_to_rgb(nbcolor);
		auto nbcolor_man = ssplit(nbcolor, ";");
		cout << nbcolor << "Some color" << Fx::r << " Normal Color" << endl;
		cout << "nbcolor_rgb size=" << nbcolor_rgb.size() << endl;
		cout << "R:" << nbcolor_rgb.at("r") << " G:" << nbcolor_rgb.at("g") << " B:" << nbcolor_rgb.at("b") << endl;
		cout << "MANUAL R:" << nbcolor_man.at(2) << " G:" << nbcolor_man.at(3) << " B:" << nbcolor_man.at(4) << endl;

		auto ccc = color_to_string("", 100, 255, 100);
		cout << "\n" << ccc << "Testing..." << Fx::r << " normal." << endl;
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
		cout << Mv::to(Term.height - 1, 0) << "Press q to exit! Timeout" << flush;
		string chr;
		string full;
		int wt = 90;
		bool qp = false;
		while (!qp && wt >= 0){
			int wtm = wt / 60;
			int wts = wt - wtm * 60;
			wt--;
			cout << Mv::to(Term.height - 1, 26) << "(" << wtm << ":" << wts << ")    " << flush;
			if (Key.wait(1000)) chr = Key.get();
			if (chr != ""){
				cout << Mv::to(Term.height - 2, 1) << "Last key: LEN=" << chr.size() << " ULEN=" << ulen(chr) << " KEY=\"" << chr << "\" CODE=" << (int)chr.at(0) << "        " << flush;
				full += chr;
				cout << Mv::to(Term.height - 5, 1) << full << flush;
				if (chr == "q") qp = true;
				chr = "";
				wt++;
			}
		}
	}

	if (debug == 1) Key(-1);
	Term.restore();
	if (debug < 2) cout << Term.normal_screen << Term.show_cursor << flush;
	return 0;
}
