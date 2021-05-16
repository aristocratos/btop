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
#include <thread>
#include <future>
#include <atomic>
#include <ranges>

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>
#include <btop_input.h>
#include <btop_theme.h>
#include <btop_draw.h>

#if defined(__linux__)
	#include <btop_linux.h>
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
	#include <sys/param.h>
	#if defined(BSD)
		// #include <btop_bsd.h>
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC == 1
		// #include <btop_osx.h>
    #endif
#endif

using std::string, std::vector, std::map, std::atomic, std::endl, std::cout, std::views::iota;
using namespace Tools;


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

	string banner;
	uint banner_width;
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

//* Generate the btop++ banner
auto createBanner(){
	struct out_vals {
		uint w;
		string s;
	};
	size_t z = 0;
	uint width=0, new_len=0;
	string b_color, bg, fg, out, oc, letter;
	bool truecolor = Config::getB("truecolor");
	int bg_i;
	for (auto line: Global::Banner_src) {
		if ((new_len = ulen(line[1])) > width) width = new_len;
		fg = Theme::hex_to_color(line[0], !truecolor);
		bg_i = 120-z*12;
		bg = Theme::dec_to_color(bg_i, bg_i, bg_i, !truecolor);
		for (uint i = 0; i < line[1].size(); i += 3) {
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
		if (z < Global::Banner_src.size()) out += Mv::l(new_len) + Mv::d(1);
	}
	out += Mv::r(18 - Global::Version.size()) + Fx::i + Theme::dec_to_color(0,0,0, !truecolor, "bg") +
			Theme::dec_to_color(150, 150, 150, !truecolor) + "v" + Global::Version + Fx::reset;
	return out_vals {width, out};
}


//* Threading test function
string my_worker(int x){
	for (int i = 0; i < 100 + (x * 100); i++){
		sleep_ms(10);
		if (Global::stop_all.load()) return "Thread stopped! x=" + to_string(x);
	}
	return "Thread done! x=" + to_string(x);
}


//? --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv){

	using namespace std;

	//? Init

	cout.setf(std::ios::boolalpha);
	if (argc > 1) argumentParser(argc, argv);

	//? Init for Linux
	if (Global::System == "linux") {
		Global::proc_path = (fs::is_directory(fs::path("/proc"))) ? fs::path("/proc") : Global::proc_path;
		if (Global::proc_path.empty()) {
			cout << "ERROR: Proc filesystem not detected!" << endl;
			exit(1);
		}
	}

	//? Initialize terminal and set options
	if (!Term::init()) {
		cout << "ERROR: No tty detected!" << endl;
		cout << "Sorry, btop++ needs an interactive shell to run." << endl;
		exit(1);
	}

	//? Read config file if present
	Config::load("____");

	auto thts = time_ms();

	//? Generate the theme
	Theme::set(Global::Default_theme);

	//? Create the btop++ banner
	auto [banner_width, banner] = createBanner();
	Global::banner_width = move(banner_width);
	Global::banner = move(banner);


	//* ------------------------------------------------ TESTING ------------------------------------------------------

	int debug = 0;
	int tests = 0;

	// cout << Theme("main_bg") << Term::clear << flush;
	bool thread_test = false;

	if (debug < 2) cout << Term::alt_screen << Term::hide_cursor << flush;

	cout << Theme::c("main_fg") << Theme::c("main_bg") << Term::clear << endl;

	cout << Mv::r(Term::width / 2 - Global::banner_width / 2) << Global::banner << endl;
	cout << string(Term::width - 1, '-') << endl;


	//* Test boxes
	if (true){
		cout << Box::draw(Box::Conf(10, 5, 50, 10, Theme::c("title"), "testing", "testagain", true, 7)) << Mv::d(12) << endl;
		exit(0);
	}


	//* Test theme

	if (false) {
		cout << "Theme generation took " << time_ms() - thts << "ms" << endl;

		cout << "Colors:" << endl;
		uint i = 0;
		for(auto& item : Theme::colors) {
			cout << rjust(item.first, 15) << ":" << item.second << "■"s * 10 << Fx::reset << "  ";
			// << Theme::dec(item.first)[0] << ":" << Theme::dec(item.first)[1] << ":" << Theme::dec(item.first)[2] << ;
			if (++i == 4) {
				i = 0;
				cout << endl;
			}
		}
		cout << Fx::reset << endl;


		cout << "Gradients:";
		for (auto& [name, cvec] : Theme::gradients) {
			cout << endl << rjust(name + ":", 10);
			for (auto& color : cvec) {
				cout << color << "■";
			}

			cout << Fx::reset << endl;
		}


		exit(0);
	}



	if (thread_test){

		map<int, future<string>> runners;
		map<int, string> outputs;

		for (int i : iota(0, 10)){
			runners[i] = async(my_worker, i);
		}
		// uint i = 0;
		while (outputs.size() < 10){

			for (int i : iota(0, 10)){
				if (runners[i].valid() && runners[i].wait_for(chrono::milliseconds(10)) == future_status::ready) {
					outputs[i] = runners[i].get();
					cout << "Thread " << i << " : " << outputs[i] << endl;
				}
			}

			// if (++i >= 10) i = 0;
			if (outputs.size() >= 8) Global::stop_all.store(true);
		}
	}



	cout << "Up for " << sec_to_dhms(round(system_uptime())) << endl;



//------>>>>>>


	auto timestamp = time_ms();
	Proc::init();

	cout << "Total Processes init: " << time_ms() - timestamp << "ms" << endl;
	cout << "Press any key to start!" << Mv::l(100) << flush;

	// sleep_ms(1000);
	// Input::wait();
	// Input::clear();


	// insert Processes call here


	uint lc;
	string ostring;
	uint64_t tsl, timestamp2;
	uint timer = 2000;
	bool filtering = false;
	vector<string> sorting;
	bool reversing = false;
	int sortint = Proc::sort_map["cpu lazy"];
	string filter;
	string filter_cur;
	string key;
	cout << rjust("Pid:", 8) << " " << ljust("Program:", 16) << " " << ljust("Command:", Term::width - 69) << " Threads: " <<
			ljust("User:", 10) << " " << rjust("MemB", 5) << " " << rjust("Cpu%", 14) << "\n" << Mv::save << flush;

	while (key != "q") {
		timestamp = time_ms();
		tsl = timestamp + timer;
		auto plist = Proc::collect(Proc::sort_vector[sortint], reversing, filter);
		timestamp2 = time_ms();
		timestamp = timestamp2 - timestamp;
		ostring.clear();
		lc = 0;
		filter_cur = (filtering) ? Fx::bl + "█" + Fx::reset : "";
		cout << Mv::restore << Mv::u(2) << Mv::r(20) << rjust("Filter: " + filter + filter_cur + string(Term::width / 3, ' ') +
				 "Sorting: " + Proc::sort_vector[sortint], Term::width - 22, true, filtering) <<  Mv::restore << flush;

		for (Proc::proc_info& procs : plist){
			ostring += 	rjust(to_string(procs.pid), 8) + " " + ljust(procs.name, 16) + " " + ljust(procs.cmd, Term::width - 66, true) + " " +
						rjust(to_string(procs.threads), 5) + " " + ljust(procs.user, 10) + " " + rjust(floating_humanizer(procs.mem, true), 5) + string(11, ' ');
			ostring += (procs.cpu_p > 100) ? rjust(to_string(procs.cpu_p), 3) + " " : rjust(to_string(procs.cpu_p), 4);
			ostring += "\n";
			if (lc++ > Term::height - 20) break;
		}

		cout << Mv::restore << ostring << Term::clear_end << endl;
		cout << "Processes call took: " << timestamp << "ms. Drawing took: " << time_ms() - timestamp2 << "ms." << endl;

		while (time_ms() < tsl) {
			if (Input::poll(tsl - time_ms())) key = Input::get();
			else { key.clear() ; continue; }
			// if (key != "") continue;
			if (filtering) {
				if (key == "enter") filtering = false;
				else if (key == "backspace") {if (!filter.empty()) filter = uresize(filter, ulen(filter) - 1);}
				else if (key == "space") filter.push_back(' ');
				else if (ulen(key) == 1 ) filter.append(key);
				else { key.clear() ; continue; }
				break;
			}
			else if (key == "q") break;
			else if (key == "left") { if (--sortint < 0) sortint = (int)Proc::sort_vector.size() - 1; }
			else if (key == "right") { if (++sortint > (int)Proc::sort_vector.size() - 1) sortint = 0; }
			else if (key == "f") filtering = true;
			else if (key == "r") reversing = !reversing;
			else if (key == "delete") filter.clear();
			else continue;
			break;
		}
	}

	// cout << "Found " << plist.size() << " pids\n" << endl;

//-----<<<<<

	//cout << pw->pw_name << "/" << gr->gr_name << endl;




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



	if (debug == 3){
		cout << Theme::c("main_fg");
		cout << Mv::to(Term::height - 1, 0) << "Press q to exit! Timeout" << flush;
		string full, key;
		int wt = 90;
		bool qp = false;
		while (!qp && wt >= 0){
			int wtm = wt / 60;
			int wts = wt - wtm * 60;
			wt--;
			cout << Mv::to(Term::height - 1, 26) << "(" << wtm << ":" << wts << ")    " << flush;
			//chr = Key(1000);
			if (Input::poll(1000)) {
				key = Input::get();
				cout << Mv::to(Term::height - 2, 1) << "Last key: LEN=" << key.size() << " ULEN=" << ulen(key) << " KEY=\"" << key << "\" CODE=" << (int)key.at(0) << "        " << flush;
				full += key;
				cout << Mv::to(Term::height - 5, 1) << full << flush;
				if (key == "q") qp = true;
				key = "";
				wt++;
			}
		}
	}

	if (debug == 1) Input::wait();
	Term::restore();
	if (debug < 2) cout << Term::normal_screen << Term::show_cursor << flush;
	return 0;
}
