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
#include <array>
#include <list>
#include <vector>
#include <csignal>
#include <thread>
#include <future>
#include <atomic>
#include <numeric>
#include <ranges>
#include <filesystem>
#include <unistd.h>
#include <robin_hood.h>
#include <cmath>
#include <iostream>

#include <btop_shared.hpp>
#include <btop_tools.hpp>
#include <btop_config.hpp>
#include <btop_input.hpp>
#include <btop_theme.hpp>
#include <btop_draw.hpp>
#include <btop_menu.hpp>

#if defined(__linux__)
	#define LINUX
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
	#include <sys/param.h>
	#if defined(BSD)
		#error BSD support not yet implemented!
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC == 1
		#define OSX
		#error OSX support not yet implemented!
    #endif
#else
	#error Platform not supported!
#endif

namespace Global {
	const std::vector<std::array<std::string, 2>> Banner_src = {
		{"#E62525", "██████╗ ████████╗ ██████╗ ██████╗"},
		{"#CD2121", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗   ██╗    ██╗"},
		{"#B31D1D", "██████╔╝   ██║   ██║   ██║██████╔╝ ██████╗██████╗"},
		{"#9A1919", "██╔══██╗   ██║   ██║   ██║██╔═══╝  ╚═██╔═╝╚═██╔═╝"},
		{"#801414", "██████╔╝   ██║   ╚██████╔╝██║        ╚═╝    ╚═╝"},
		{"#000000", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝"},
	};
	const std::string Version = "0.0.21";
	int coreCount;
}

using std::string, std::vector, std::array, robin_hood::unordered_flat_map, std::atomic, std::endl, std::cout, std::views::iota, std::list, std::accumulate;
using std::flush, std::endl, std::future, std::string_literals::operator""s, std::future_status, std::to_string, std::round;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;


namespace Global {
	string banner;
	size_t banner_width = 0;

	fs::path self_path;

	bool debuginit = false;
	bool debug = false;

	uint64_t start_time;

	bool quitting = false;

	bool arg_tty = false;
}


//* A simple argument parser
void argumentParser(int argc, char **argv){
	string argument;
	for(int i = 1; i < argc; i++) {
		argument = argv[i];
		if (argument == "-v" || argument == "--version") {
			cout << "btop version: " << Global::Version << endl;
			exit(0);
		}
		else if (argument == "-h" || argument == "--help") {
			cout 	<< "usage: btop [-h] [-v] [-/+t] [--debug]\n\n"
					<< "optional arguments:\n"
					<< "  -h, --help			show this help message and exit\n"
					<< "  -v, --version			show version info and exit\n"
					<< "  -t, --tty_on			force (ON) tty mode, max 16 colors and tty friendly graph symbols\n"
					<< "  +t, --tty_off			force (OFF) tty mode\n"
					<< "  --debug			start with loglevel set to DEBUG, overriding value set in config\n"
					<< endl;
			exit(0);
		}
		else if (argument == "--debug")
			Global::debug = true;
		else if (argument == "-t" || argument == "--tty_on") {
			Config::set("tty_mode", true);
			Global::arg_tty = true;
		}
		else if (argument == "+t" || argument == "--tty_off") {
			Config::set("tty_mode", false);
			Global::arg_tty = true;
		}
		else {
			cout << " Unknown argument: " << argument << "\n" <<
			" Use -h or --help for help." <<  endl;
			exit(1);
		}
	}
}

void clean_quit(int sig){
	if (Global::quitting) return;
	if (Term::initialized) {
		Term::restore();
		if (!Global::debuginit) cout << Term::normal_screen << Term::show_cursor << flush;
	}
	Global::quitting = true;
	Config::write();
	Logger::info("Quitting! Runtime: " + sec_to_dhms(time_s() - Global::start_time));
	if (sig != -1) exit(sig);
}

void sleep_now(){
	if (Term::initialized) {
		Term::restore();
		if (!Global::debuginit) cout << Term::normal_screen << Term::show_cursor << flush;
	}
	std::raise(SIGSTOP);
}

void resume_now(){
	Term::init();
	if (!Global::debuginit) cout << Term::alt_screen << Term::hide_cursor << flush;
}

void _exit_handler() { clean_quit(-1); }

void _signal_handler(int sig) {
	switch (sig) {
		case SIGINT:
			clean_quit(0);
			break;
		case SIGTSTP:
			sleep_now();
			break;
		case SIGCONT:
			resume_now();
			break;
	}
}

//? Generate the btop++ banner
void banner_gen() {
	size_t z = 0;
	string b_color, bg, fg, oc, letter;
	auto& truecolor = Config::getB("truecolor");
	int bg_i;
	Global::banner.clear();
	Global::banner_width = 0;
	auto tty_mode = (Config::getB("tty_mode"));
	for (auto line: Global::Banner_src) {
		if (auto w = ulen(line[1]); w > Global::banner_width) Global::banner_width = w;
		fg = Theme::hex_to_color(line[0], !truecolor);
		bg_i = 120 - z * 12;
		bg = Theme::dec_to_color(bg_i, bg_i, bg_i, !truecolor);
		for (size_t i = 0; i < line[1].size(); i += 3) {
			if (line[1][i] == ' ') {
				letter = ' ';
				i -= 2;
			}
			else
				letter = line[1].substr(i, 3);

			if (tty_mode && letter != "█" && letter != " ") letter = "░";
			b_color = (letter == "█") ? fg : bg;
			if (b_color != oc) Global::banner += b_color;
			Global::banner += letter;
			oc = b_color;
		}
		if (++z < Global::Banner_src.size()) Global::banner += Mv::l(ulen(line[1])) + Mv::d(1);
	}
	Global::banner += Mv::r(18 - Global::Version.size()) + Fx::i + Theme::dec_to_color(0,0,0, !truecolor, "bg") +
			Theme::dec_to_color(150, 150, 150, !truecolor) + "v" + Global::Version + Fx::ui;
}

//* Threading test function
// string my_worker(int x){
// 	for (int i = 0; i < 100 + (x * 100); i++){
// 		sleep_ms(10);
// 		if (Global::stop_all.load()) return "Thread stopped! x=" + to_string(x);
// 	}
// 	return "Thread done! x=" + to_string(x);
// }


//? --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv){

	//? Init

	Global::start_time = time_s();

	cout.setf(std::ios::boolalpha);
	if (argc > 1) argumentParser(argc, argv);

	std::atexit(_exit_handler);
	std::at_quick_exit(_exit_handler);
	std::signal(SIGINT, _signal_handler);
	std::signal(SIGTSTP, _signal_handler);
	std::signal(SIGCONT, _signal_handler);

	//? Linux init
	#if defined(LINUX)
		Global::coreCount = sysconf(_SC_NPROCESSORS_ONLN);
		if (Global::coreCount < 1) Global::coreCount = 1;
		{
			std::error_code ec;
			Global::self_path = fs::read_symlink("/proc/self/exe", ec).remove_filename();
		}
	#endif

	//? Setup paths for config, log and themes
	for (auto env : {"XDG_CONFIG_HOME", "HOME"}) {
		if (getenv(env) != NULL && access(getenv(env), W_OK) != -1) {
			Config::conf_dir = fs::path(getenv(env)) / (((string)env == "HOME") ? ".config/btop" : "btop");
			break;
		}
	}
	if (!Config::conf_dir.empty()) {
		if (std::error_code ec; !fs::is_directory(Config::conf_dir) && !fs::create_directories(Config::conf_dir, ec)) {
			cout << "WARNING: Could not create or access btop config directory. Logging and config saving disabled." << endl;
			cout << "Make sure your $HOME environment variable is correctly set to fix this." << endl;
		}
		else {
			Config::conf_file = Config::conf_dir / "btop.conf";
			Logger::logfile = Config::conf_dir / "btop.log";
			Theme::user_theme_dir = Config::conf_dir / "themes";
			if (!fs::exists(Theme::user_theme_dir) && !fs::create_directory(Theme::user_theme_dir, ec)) Theme::user_theme_dir.clear();
		}
	}
	if (std::error_code ec; !Global::self_path.empty()) {
			Theme::theme_dir = fs::canonical(Global::self_path / "../share/btop/themes", ec);
			if (ec || access(Theme::theme_dir.c_str(), R_OK) == -1) Theme::theme_dir.clear();
		}

	if (Theme::theme_dir.empty()) {
		for (auto theme_path : {"/usr/local/share/btop/themes", "/usr/share/btop/themes"}) {
			if (fs::exists(fs::path(theme_path)) && access(theme_path, R_OK) != -1) {
				Theme::theme_dir = fs::path(theme_path);
				break;
			}
		}
	}

	//? Config init
	{	vector<string> load_errors;
		Config::load(Config::conf_file, load_errors);

		if (Global::debug) Logger::set("DEBUG");
		else Logger::set(Config::getS("log_level"));

		Logger::debug("Logger set to DEBUG");

		for (auto& err_str : load_errors) Logger::warning(err_str);
	}

	if (!string(getenv("LANG")).ends_with("UTF-8") && !string(getenv("LANG")).ends_with("utf-8")) {
		string err_msg = "No UTF-8 locale was detected! Symbols might not look as intended.\n"
						 "Make sure your $LANG evironment variable is set and with a UTF-8 locale.";
		Logger::warning(err_msg);
		cout << "WARNING: " << err_msg << endl;
	}

	//? Initialize terminal and set options
	if (!Term::init()) {
		string err_msg = "No tty detected!\nbtop++ needs an interactive shell to run.";
		Logger::error(err_msg);
		cout << "ERROR: " << err_msg << endl;
		clean_quit(1);
	}

	Logger::debug("Running on " + Term::current_tty);
	if (!Global::arg_tty && Config::getB("force_tty")) {
		Config::set("tty_mode", true);
		Logger::info("Forcing tty mode: setting 16 color mode and using tty friendly graph symbols");
	}
	else if (!Global::arg_tty && Term::current_tty.starts_with("/dev/tty")) {
		Config::set("tty_mode", true);
		Logger::info("Real tty detected, setting 16 color mode and using tty friendly graph symbols");
	}


	#if defined(LINUX)
		//? Linux init
		Proc::init();
	#endif


	// Config::set("truecolor", false);

	auto thts = time_ms();

	//? Generate the theme
	Theme::set("Default");

	//? Create the btop++ banner
	banner_gen();


	//* ------------------------------------------------ TESTING ------------------------------------------------------


	Global::debuginit = true;

	// cout << Theme("main_bg") << Term::clear << flush;
	// bool thread_test = false;

	if (!Global::debuginit) cout << Term::alt_screen << Term::hide_cursor << flush;

	cout << Theme::c("main_fg") << Theme::c("main_bg") << Term::clear << endl;

	cout << Mv::r(Term::width / 2 - Global::banner_width / 2) << Global::banner << endl;
	// cout << string(Term::width - 1, '-') << endl;
	size_t blen = (Term::width > 200) ? 200 : Term::width;
	if (Term::width > 203) cout << Mv::r(Term::width / 2 - blen / 2) << flush;
	int ill = 0;
	for (int i : iota(0, (int)blen)){
		ill = (i <= (int)blen / 2) ? i : ill - 1;
		cout << Theme::g("used")[ill] << Symbols::h_line;
	}
	cout << Fx::reset << endl;

	//* Test theme
	if (false) {
		cout << "Theme generation took " << time_ms() - thts << "ms" << endl;

		cout << "Colors:" << endl;
		uint i = 0;
		for(auto& item : Theme::test_colors()) {
			cout << rjust(item.first, 15) << ":" << item.second << "■"s * 10 << Fx::reset << "  ";
			// << Theme::dec(item.first)[0] << ":" << Theme::dec(item.first)[1] << ":" << Theme::dec(item.first)[2] << ;
			if (++i == 4) {
				i = 0;
				cout << endl;
			}
		}
		cout << Fx::reset << endl;


		cout << "Gradients:";
		for (auto& [name, cvec] : Theme::test_gradients()) {
			cout << endl << rjust(name + ":", 10);
			for (auto& color : cvec) {
				cout << color << "■";
			}

			cout << Fx::reset << endl;
		}


		exit(0);
	}


	if (false) {
		Draw::Meter kmeter;
		kmeter(Term::width - 2, "cpu", false);
		cout << kmeter(25) << endl;
		cout << kmeter(0) << endl;
		cout << kmeter(50) << endl;
		cout << kmeter(100) << endl;
		cout << kmeter(50) << endl;
		exit(0);
	}

	if (false) {
		cout << fs::absolute(fs::current_path() / "..") << endl;
		cout << Global::self_path << endl;
		cout << Theme::theme_dir << endl;
		cout << Config::conf_dir << endl;
		exit(0);
	}

	if (true) {

		vector<long long> mydata;
		for (long long i = 0; i <= 100; i++) mydata.push_back(i);
		for (long long i = 100; i >= 0; i--) mydata.push_back(i);
		// mydata.push_back(0);
		// mydata.push_back(0);
		mydata.push_back(50);


		// for (long long i = 0; i <= 100; i++) mydata.push_back(i);
		// for (long long i = 100; i >= 0; i--) mydata.push_back(i);

		Draw::Graph kgraph {};
		Draw::Graph kgraph2 {};
		Draw::Graph kgraph3 {};

		cout << Draw::createBox({.x = 5, .y = 10, .width = Term::width - 10, .height = 12, .line_color = Theme::c("proc_box"), .title = "braille", .fill = false, .num = 1}) << Mv::save;
		cout << Draw::createBox({.x = 5, .y = 23, .width = Term::width - 10, .height = 12, .line_color = Theme::c("proc_box"), .title = "block", .fill = false, .num = 2});
		cout << Draw::createBox({.x = 5, .y = 36, .width = Term::width - 10, .height = 12, .line_color = Theme::c("proc_box"), .title = "tty", .fill = false, .num = 3}) << flush;
		// Draw::Meter kmeter {};
		// Draw::Graph kgraph2 {};
		// Draw::Graph kgraph3 {};

		auto kts = time_micros();
		kgraph(Term::width - 12, 10, "cpu", mydata, "braille", false, false);
		kgraph2(Term::width - 12, 10, "cpu", mydata, "block", false, false);
		kgraph3(Term::width - 12, 10, "cpu", mydata, "tty", false, false);

		// kmeter(Term::width - 12, "process");
		// cout << Mv::save << kgraph(mydata) << "\n\nInit took " << time_micros() - kts << " μs.       " << endl;

		// exit(0);
		// kgraph2(Term::width, 10, "process", mydata, true, false);
		// kgraph3(Term::width, 1, "process", mydata, false, false);
		// cout << kgraph() << endl;
		// cout << kgraph2() << endl;
		// exit(0);

		cout 	<< Mv::restore << kgraph(mydata, true)
				<< Mv::restore << Mv::d(13) << kgraph2(mydata, true)
				<< Mv::restore << Mv::d(26) << kgraph3(mydata, true) << endl
				<< Mv::d(1) << "Init took " << time_micros() - kts << " μs.       " << endl;
		// cout << Mv::save << kgraph(mydata, true) << "\n" << kgraph2(mydata, true) << "\n" << kgraph3(mydata, true) << "\n" << kmeter(mydata.back()) << "\n\nInit took " << time_micros() - kts << " μs.       " << endl;
		// sleep_ms(1000);
		// mydata.push_back(50);
		// cout << Mv::restore << kgraph(mydata) << "\n" << kgraph2(mydata) << "\n\nInit took " << time_micros() - kts << " μs.       " << endl;
		// exit(0);

		// long long y = 0;
		// bool flip = false;
		list<uint64_t> ktavg;
		while (true) {
			mydata.back() = std::rand() % 101;
			// mydata.back() = y;
			kts = time_micros();
			// cout << Mv::restore << " "s * Term::width << "\n" << " "s * Term::width << endl;
			cout 	<< Mv::restore << kgraph(mydata)
					<< Mv::restore << Mv::d(13) << kgraph2(mydata)
					<< Mv::restore << Mv::d(26) << kgraph3(mydata)
					<< endl;
			// cout << Mv::restore << kgraph(mydata) << "\n" << kgraph2(mydata) << "\n" << " "s * Term::width << Mv::l(Term::width) << kgraph3(mydata) << "\n" << kmeter(mydata.back()) << endl;
			ktavg.push_front(time_micros() - kts);
			if (ktavg.size() > 100) ktavg.pop_back();
			cout << Mv::d(1) << "Time: " << ktavg.front() << " μs.  Avg: " << accumulate(ktavg.begin(), ktavg.end(), 0) / ktavg.size() << "  μs.     " << flush;
			// if (flip) y--;
			// else y++;
			// if (y == 100 || y == 0) flip = !flip;
			if (Input::poll()) {
				if (Input::get() == "space") Input::wait(true);
				else break;
			}
			sleep_ms(100);
		}
		Input::get();

		exit(0);

	}


	if (false) {
		cout << Config::getS("log_level") << endl;

		vector<string> vv = {"hej", "vad", "du"};
		vector<int> vy;

		cout << v_contains(vv, "vad"s) << endl;
		cout << v_index(vv, "hej"s) << endl;
		cout << v_index(vv, "du"s) << endl;
		cout << v_index(vv, "kodkod"s) << endl;
		cout << v_index(vy, 4) << endl;


		exit(0);
	}


	// if (thread_test){

	// 	unordered_flat_map<int, future<string>> runners;
	// 	unordered_flat_map<int, string> outputs;

	// 	for (int i : iota(0, 10)){
	// 		runners[i] = async(my_worker, i);
	// 	}
	// 	// uint i = 0;
	// 	while (outputs.size() < 10){

	// 		for (int i : iota(0, 10)){
	// 			if (runners[i].valid() && runners[i].wait_for(std::chrono::milliseconds(10)) == future_status::ready) {
	// 				outputs[i] = runners[i].get();
	// 				cout << "Thread " << i << " : " << outputs[i] << endl;
	// 			}
	// 		}

	// 		// if (++i >= 10) i = 0;
	// 		if (outputs.size() >= 8) Global::stop_all.store(true);
	// 	}
	// }



	cout << "Up for " << sec_to_dhms(round(system_uptime())) << endl;


//*------>>>>>> Proc testing


	auto timestamp = time_ms();



	uint lc;
	string ostring;
	uint64_t tsl, timestamp2, rcount = 0;
	list<uint64_t> avgtimes = {0};
	uint timer = 2000;
	bool filtering = false;
	vector<string> greyscale;
	string filter;
	string filter_cur;
	string key;

	int xc;
	for (uint i : iota(0, (int)Term::height - 19)){
		xc = 230 - i * 150 / (Term::height - 20);
		greyscale.push_back(Theme::dec_to_color(xc, xc, xc));
	}

	string pbox = Draw::createBox({.x = 1, .y = 10, .width = Term::width, .height = Term::height - 16, .line_color = Theme::c("proc_box"), .title = "testbox", .title2 = "below", .fill = false, .num = 7});
	pbox += Mv::r(1) + Theme::c("title") + Fx::b + rjust("Pid:", 8) + " " + ljust("Program:", 16) + " " + ljust("Command:", Term::width - 70) + " Threads: " +
			ljust("User:", 10) + " " + rjust("MemB", 5) + " " + rjust("Cpu%", 14) + "\n" + Fx::reset + Mv::save;

	while (key != "q") {
		timestamp = time_micros();
		tsl = time_ms() + timer;
		auto plist = Proc::collect();
		timestamp2 = time_micros();
		timestamp = timestamp2 - timestamp;
		ostring.clear();
		lc = 0;

		ostring = Mv::u(2) + Mv::l(Term::width) + Mv::r(12)
			+ trans("Filter: " + filter + (filtering ? Fx::bl + "█" + Fx::reset : " "))
			+ trans(rjust("Per core: " + (Config::getB("proc_per_core") ? "On "s : "Off"s) + "   Sorting: "
				+ string(Config::getS("proc_sorting")), Term::width - 23 - ulen(filter)))
			+ Mv::restore;

		for (auto& p : plist){
			if (!Config::getB("proc_tree")) {
				ostring += Mv::r(1) + greyscale[lc] + rjust(to_string(p.pid), 8) + " " + ljust(p.name, 16) + " " + ljust(p.cmd, Term::width - 66, true) + " "
						+ rjust(to_string(p.threads), 5) + " " + ljust(p.user, 10) + " " + rjust(floating_humanizer(p.mem, true), 5) + string(11, ' ')
						+ (p.cpu_p < 10 || p.cpu_p >= 100 ? rjust(to_string(p.cpu_p), 3) + " " : rjust(to_string(p.cpu_p), 4))
						+ "\n";
			}
			else {
				string cmd_cond;
				if (!p.cmd.empty()) {
					cmd_cond = p.cmd.substr(0, std::min(p.cmd.find(' '), p.cmd.size()));
					cmd_cond = cmd_cond.substr(std::min(cmd_cond.find_last_of('/') + 1, cmd_cond.size()));
				}
				ostring += Mv::r(1) + (Config::getB("tty_mode") ? " " : greyscale[lc]) + ljust(p.prefix + to_string(p.pid) + " " + p.name + " " + (!cmd_cond.empty() && cmd_cond != p.name ? "(" + cmd_cond + ")" : ""), Term::width - 40, true) + " "
						+ rjust(to_string(p.threads), 5) + " " + ljust(p.user, 10) + " " + rjust(floating_humanizer(p.mem, true), 5) + string(11, ' ')
						+ (p.cpu_p < 10 || p.cpu_p >= 100 ? rjust(to_string(p.cpu_p), 3) + " " : rjust(to_string(p.cpu_p), 4))
						+ "\n";
			}
			if (lc++ > Term::height - 21) break;
		}

		while (lc++ < Term::height - 19) ostring += Mv::r(1) + string(Term::width - 2, ' ') + "\n";



		if (rcount > 0) avgtimes.push_front(timestamp);
		if (avgtimes.size() > 10) avgtimes.pop_back();
		cout << pbox << ostring << Fx::reset << "\n" << endl;
		cout << Mv::to(Term::height - 4, 1) << "Processes call took: " << rjust(to_string(timestamp), 5) << " μs. Average: " <<
			rjust(to_string(accumulate(avgtimes.begin(), avgtimes.end(), 0) / avgtimes.size()), 5) << " μs of " << avgtimes.size() <<
			" samples. Drawing took: " << time_micros() - timestamp2 << " μs.\nNumber of processes: " << Proc::numpids << ". Run count: " <<
			++rcount << ". Time: " << strf_time("%X   ") << endl;

		while (time_ms() < tsl) {
			if (Input::poll(tsl - time_ms())) key = Input::get();
			else { key.clear() ; continue; }
			if (filtering) {
				if (key == "enter") filtering = false;
				else if (key == "backspace") {if (!filter.empty()) filter = uresize(filter, ulen(filter) - 1);}
				else if (key == "space") filter.push_back(' ');
				else if (ulen(key) == 1 ) filter.append(key);
				else { key.clear(); continue; }
				if (filter != Config::getS("proc_filter")) Config::set("proc_filter", filter);
				break;
			}
			else if (key == "q") break;
			else if (key == "left") {
				int cur_i = v_index(Proc::sort_vector, Config::getS("proc_sorting"));
				if (--cur_i < 0) cur_i = Proc::sort_vector.size() - 1;
				Config::set("proc_sorting", Proc::sort_vector.at(cur_i));
			}
			else if (key == "right") {
				int cur_i = v_index(Proc::sort_vector, Config::getS("proc_sorting"));
				if (++cur_i > (int)Proc::sort_vector.size() - 1) cur_i = 0;
				Config::set("proc_sorting", Proc::sort_vector.at(cur_i));
			}
			else if (key == "f") filtering = true;
			else if (key == "t") Config::flip("proc_tree");
			else if (key == "r") Config::flip("proc_reversed");
			else if (key == "c") Config::flip("proc_per_core");
			else if (key == "delete") { filter.clear(); Config::set("proc_filter", filter); }
			else continue;
			break;
		}
	}

	// cout << "Found " << plist.size() << " pids\n" << endl;

//*-----<<<<<

	return 0;
}
