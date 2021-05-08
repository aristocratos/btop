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

#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <map>
#include <thread>
#include <future>
#include <atomic>
#include <ranges>

#include <unistd.h>

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>
#include <btop_input.h>
#include <btop_theme.h>

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

class C_Banner {
	string banner_str;

public:
	int width;

	C_Banner(){
		size_t z = 0;
		string b_color, bg, fg, out, oc, letter;
		int bg_i;
		int new_len;
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

	//? Init

	cout.setf(std::ios::boolalpha);
	if (argc > 1) argumentParser(argc, argv);

	//? Initialize terminal and set options
	C_Term Term;
	if (!Term.initialized) {
		cout << "ERROR: No tty detected!" << endl;
		cout << "Sorry, btop++ needs an interactive shell to run." << endl;
		exit(1);
	}

	//? Read config file if present
	C_Config Config;

	//? Generate the theme
	C_Theme Theme(Global::Default_theme);

	//? Create the btop++ banner
	C_Banner Banner;

	//? Initialize the Input class
	C_Input Input;


	//* ------------------------------------------------ TESTING ------------------------------------------------------

	int debug = 2;
	int tests = 0;

	// cout << Theme("main_bg") << Term.clear << flush;
	// bool thread_test = false;

	if (debug < 2) cout << Term.alt_screen << Term.clear << Term.hide_cursor << flush;

	cout << Theme("main_fg") << endl;

	cout << Mv::r(Term.width / 2 - Banner.width / 2) << Banner() << endl;
	cout << string(50, '-') << Mv::l(50) << flush;
	cout << rjustify("Terminal  Width=", 20) << trans("Korven s   kommer ") << Term.width << " Height=" << Term.height << endl;


	//* Test MENUS
	// for (auto& outer : Global::Menus){
	// 	for (auto& inner : outer.second){
	// 		for (auto& item : inner.second){
	// 			cout << item << endl;
	// 		}
	// 	}
	// }



	// cout << Config(Bool, "truecolor") << endl;
	// cout << Config(Int, "tree_depth") << endl;
	// cout << Config(String, "color_theme") << endl;

	//* Test theme
	int i = 0;
	if (tests>0) for(auto& item : Global::Default_theme) {
		cout << Theme(item.first) << item.first << ":" << Theme("main_fg") << Theme(item.first).erase(0, 2) << Fx::reset << "  ";
		if (++i == 4) {
			i = 0;
			cout << endl;
		}
		cout << Fx::reset << endl;
	}



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
			if (Input(1000)) {
				cout << Mv::to(Term.height - 2, 1) << "Last key: LEN=" << Input().size() << " ULEN=" << ulen(Input()) << " KEY=\"" << Input() << "\" CODE=" << (int)Input().at(0) << "        " << flush;
				full += Input();
				cout << Mv::to(Term.height - 5, 1) << full << flush;
				if (Input() == "q") qp = true;
				wt++;
			}
		}
	}

	if (debug == 1) Input(-1);
	Term.restore();
	if (debug < 2) cout << Term.normal_screen << Term.show_cursor << flush;
	return 0;
}
