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
#include <exception>

#include <btop_shared.hpp>
#include <btop_tools.hpp>
#include <btop_config.hpp>
#include <btop_input.hpp>
#include <btop_theme.hpp>
#include <btop_draw.hpp>
#include <btop_menu.hpp>

#if defined(__linux__)
	#define LINUX
#elif defined(__unix__) or not defined(__APPLE__) and defined(__MACH__)
	#include <sys/param.h>
	#if defined(BSD)
		#error BSD support not yet implemented!
	#endif
#elif defined(__APPLE__) and defined(__MACH__)
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
	const std::string Version = "0.0.30";
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

	atomic<bool> resized (false);
	atomic<bool> quitting (false);

	bool arg_tty = false;
	bool arg_low_color = false;
}


//* A simple argument parser
void argumentParser(int argc, char **argv){
	string argument;
	for(int i = 1; i < argc; i++) {
		argument = argv[i];
		if (argument == "-v" or argument == "--version") {
			cout << "btop version: " << Global::Version << endl;
			exit(0);
		}
		else if (argument == "-h" or argument == "--help") {
			cout 	<< "usage: btop [-h] [-v] [-/+t] [--debug]\n\n"
					<< "optional arguments:\n"
					<< "  -h, --help            show this help message and exit\n"
					<< "  -v, --version         show version info and exit\n"
					<< "  -lc, --low-color      disable truecolor, converts 24-bit colors to 256-color\n"
					<< "  -t, --tty_on          force (ON) tty mode, max 16 colors and tty friendly graph symbols\n"
					<< "  +t, --tty_off         force (OFF) tty mode\n"
					<< "  --debug               start with loglevel set to DEBUG, overriding value set in config\n"
					<< endl;
			exit(0);
		}
		else if (argument == "--debug")
			Global::debug = true;
		else if (argument == "-t" or argument == "--tty_on") {
			Config::set("tty_mode", true);
			Global::arg_tty = true;
		}
		else if (argument == "+t" or argument == "--tty_off") {
			Config::set("tty_mode", false);
			Global::arg_tty = true;
		}
		else if (argument == "-lc" or argument == "--low-color") {
			Global::arg_low_color = true;
		}
		else {
			cout << " Unknown argument: " << argument << "\n" <<
			" Use -h or --help for help." <<  endl;
			exit(1);
		}
	}
}

//* Handler for SIGWINCH and general resizing events
void _resize(bool force=false){
	if (Term::refresh(false) or force) {
		Global::resized = true;
		if (Runner::active) {
			Runner::stop = true;
			atomic_wait(Runner::active);
		}
		Term::refresh();
	}
	else return;

	while (true) {
		sleep_ms(100);
		if (not Term::refresh()) break;
	}

	Input::interrupt = true;
	Draw::calcSizes();
}

//* Exit handler; stops threads, restores terminal and saves config changes
void clean_quit(int sig=-1){
	if (Global::quitting) return;
	Global::quitting = true;
	if (Runner::active) {
		Runner::stop = true;
		atomic_wait(Runner::active);
	}
	if (Term::initialized) {
		Term::restore();
		if (not Global::debuginit) cout << Term::normal_screen << Term::show_cursor << flush;
	}

	Config::write();
	Logger::info("Quitting! Runtime: " + sec_to_dhms(time_s() - Global::start_time));
	if (sig != -1) exit(sig);
}

//* Handler for SIGTSTP; stops threads, restores terminal and sends SIGSTOP
void _sleep(){
	if (Runner::active) {
		Runner::stop = true;
		atomic_wait(Runner::active);
	}
	if (Term::initialized) {
		Term::restore();
		if (not Global::debuginit) cout << Term::normal_screen << Term::show_cursor << flush;
	}
	std::raise(SIGSTOP);
}

//* Handler for SIGCONT; re-initialize terminal and force a resize event
void _resume(){
	Term::init();
	if (not Global::debuginit) cout << Term::alt_screen << Term::hide_cursor << flush;
	_resize(true);
}

void _exit_handler() {
	clean_quit(-1);
}

void _signal_handler(int sig) {
	switch (sig) {
		case SIGINT:
			clean_quit(0);
			break;
		case SIGTSTP:
			_sleep();
			break;
		case SIGCONT:
			_resume();
			break;
		case SIGWINCH:
			_resize();
			break;
	}
}

//? Generate the btop++ banner
void banner_gen() {
	size_t z = 0;
	string b_color, bg, fg, oc, letter;
	auto& lowcolor = Config::getB("lowcolor");
	int bg_i;
	Global::banner.clear();
	Global::banner_width = 0;
	auto tty_mode = (Config::getB("tty_mode"));
	for (auto line: Global::Banner_src) {
		if (auto w = ulen(line[1]); w > Global::banner_width) Global::banner_width = w;
		if (tty_mode) {
			fg = (z > 2) ? "\x1b[31m" : "\x1b[91m";
			bg = (z > 2) ? "\x1b[90m" : "\x1b[37m";
		}
		else {
			fg = Theme::hex_to_color(line[0], lowcolor);
			bg_i = 120 - z * 12;
			bg = Theme::dec_to_color(bg_i, bg_i, bg_i, lowcolor);
		}
		for (size_t i = 0; i < line[1].size(); i += 3) {
			if (line[1][i] == ' ') {
				letter = ' ';
				i -= 2;
			}
			else
				letter = line[1].substr(i, 3);

			// if (tty_mode and letter != "█" and letter != " ") letter = "░";
			b_color = (letter == "█") ? fg : bg;
			if (b_color != oc) Global::banner += b_color;
			Global::banner += letter;
			oc = b_color;
		}
		if (++z < Global::Banner_src.size()) Global::banner += Mv::l(ulen(line[1])) + Mv::d(1);
	}
	Global::banner += Mv::r(18 - Global::Version.size())
			+ (tty_mode ? "\x1b[0;40;37m" : Theme::dec_to_color(0,0,0, lowcolor, "bg") + Theme::dec_to_color(150, 150, 150, lowcolor))
			+ Fx::i + "v" + Global::Version + Fx::ui;
}

namespace Runner {
	atomic<bool> active (false);
	atomic<bool> stop (false);
}


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
	std::signal(SIGWINCH, _signal_handler);

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
		if (getenv(env) != NULL and access(getenv(env), W_OK) != -1) {
			Config::conf_dir = fs::path(getenv(env)) / (((string)env == "HOME") ? ".config/btop" : "btop");
			break;
		}
	}
	if (not Config::conf_dir.empty()) {
		if (std::error_code ec; not fs::is_directory(Config::conf_dir) and not fs::create_directories(Config::conf_dir, ec)) {
			cout << "WARNING: Could not create or access btop config directory. Logging and config saving disabled." << endl;
			cout << "Make sure your $HOME environment variable is correctly set to fix this." << endl;
		}
		else {
			Config::conf_file = Config::conf_dir / "btop.conf";
			Logger::logfile = Config::conf_dir / "btop.log";
			Theme::user_theme_dir = Config::conf_dir / "themes";
			if (not fs::exists(Theme::user_theme_dir) and not fs::create_directory(Theme::user_theme_dir, ec)) Theme::user_theme_dir.clear();
		}
	}
	if (std::error_code ec; not Global::self_path.empty()) {
			Theme::theme_dir = fs::canonical(Global::self_path / "../share/btop/themes", ec);
			if (ec or not fs::is_directory(Theme::theme_dir) or access(Theme::theme_dir.c_str(), R_OK) == -1) Theme::theme_dir.clear();
		}

	if (Theme::theme_dir.empty()) {
		for (auto theme_path : {"/usr/local/share/btop/themes", "/usr/share/btop/themes"}) {
			if (fs::is_directory(fs::path(theme_path)) and access(theme_path, R_OK) != -1) {
				Theme::theme_dir = fs::path(theme_path);
				break;
			}
		}
	}

	//? Config init
	{	vector<string> load_errors;
		Config::load(Config::conf_file, load_errors);

		Config::set("lowcolor", (Global::arg_low_color ? true : not Config::getB("truecolor")));

		if (Global::debug) Logger::set("DEBUG");
		else Logger::set(Config::getS("log_level"));

		Logger::info("Logger set to " + Config::getS("log_level"));

		for (auto& err_str : load_errors) Logger::warning(err_str);
	}

	if (not string(getenv("LANG")).ends_with("UTF-8") and not string(getenv("LANG")).ends_with("utf-8")) {
		string err_msg = "No UTF-8 locale was detected! Symbols might not look as intended.\n"
						 "Make sure your $LANG evironment variable is set and with a UTF-8 locale.";
		Logger::warning(err_msg);
		cout << "WARNING: " << err_msg << endl;
	}

	//? Initialize terminal and set options
	if (not Term::init()) {
		string err_msg = "No tty detected!\nbtop++ needs an interactive shell to run.";
		Logger::error(err_msg);
		cout << "ERROR: " << err_msg << endl;
		clean_quit(1);
	}

	Logger::info("Running on " + Term::current_tty);
	if (not Global::arg_tty and Config::getB("force_tty")) {
		Config::set("tty_mode", true);
		Logger::info("Forcing tty mode: setting 16 color mode and using tty friendly graph symbols");
	}
	else if (not Global::arg_tty and Term::current_tty.starts_with("/dev/tty")) {
		Config::set("tty_mode", true);
		Logger::info("Real tty detected, setting 16 color mode and using tty friendly graph symbols");
	}



	//? Platform init and error check
	Shared::init();



	// Config::set("truecolor", false);

	auto thts = time_micros();

	//? Update theme list and generate the theme
	Theme::updateThemes();
	Theme::setTheme();

	//? Create the btop++ banner
	banner_gen();


	//* ------------------------------------------------ TESTING ------------------------------------------------------


	Global::debuginit = false;

	Draw::calcSizes();


	if (not Global::debuginit) cout << Term::alt_screen << Term::hide_cursor << Term::clear << endl;

	cout << Cpu::box << Mem::box << Net::box << Proc::box << flush;


	if (false) {
		Draw::calcSizes();
		cout << Cpu::box << Mem::box << Net::box << Proc::box << flush;
		Input::wait();
		exit(0);
	}

	// if (true) {
	// 	cout << Term::clear << flush;
	// 	unordered_flat_map<string, string(*)(string)> korvs = {
	// 		{"korv1", korv1},
	// 		{"korv2", korv2},
	// 	};

	// 	// auto hej = korv1;

	// 	cout << korvs["korv1"]("hejsan") << endl;
	// 	cout << korvs["korv2"]("hejsan igen") << endl;
	// 	exit(0);
	// }

	//* Test theme
	if (false) {
		string key;
		bool no_redraw = false;
		auto theme_index = v_index(Theme::themes, Config::getS("color_theme"));
		while (key != "q") {
			key.clear();

			if (not no_redraw) {
				cout << "\nTheme generation of " << fs::path(Config::getS("color_theme")).filename().replace_extension("") << " took " << time_micros() - thts << "μs" << endl;

				cout << "Colors:" << endl;
				size_t i = 0;
				for(auto& item : Theme::test_colors()) {
					cout << rjust(item.first, 15) << ":" << item.second << "■"s * 10 << Fx::reset << "  ";
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
			}

			no_redraw = true;
			key = Input::wait();
			if (key.empty()) continue;
			thts = time_micros();
			if (key == "right") {
				if (theme_index == Theme::themes.size() - 1) theme_index = 0;
				else theme_index++;
			}
			else if (key == "left") {
				if (theme_index == 0) theme_index = Theme::themes.size() - 1;
				else theme_index--;
			}
			else continue;
			no_redraw = false;
			Config::set("color_theme", Theme::themes.at(theme_index));
			Theme::setTheme();

		}


		exit(0);
	}


	if (false) {

		deque<long long> mydata;
		for (long long i = 0; i <= 100; i++) mydata.push_back(i);
		for (long long i = 100; i >= 0; i--) mydata.push_back(i);
		mydata.push_back(50);

		Draw::Graph kgraph {};
		Draw::Graph kgraph2 {};
		Draw::Graph kgraph3 {};

		cout << Draw::createBox(5, 10, Term::width - 10, 12, Theme::c("proc_box"), false, "braille", "", 1) << Mv::save;
		cout << Draw::createBox(5, 23, Term::width - 10, 12, Theme::c("proc_box"), false, "block", "", 2);
		cout << Draw::createBox(5, 36, Term::width - 10, 12, Theme::c("proc_box"), false, "tty", "", 3) << flush;
		auto kts = time_micros();
		kgraph(Term::width - 13, 10, "cpu", mydata, "braille", false, false);
		kgraph2(Term::width - 13, 10, "cpu", mydata, "block", false, false);
		kgraph3(Term::width - 13, 10, "cpu", mydata, "tty", false, false);


		cout 	<< Mv::restore << kgraph(mydata, true)
				<< Mv::restore << Mv::d(13) << kgraph2(mydata, true)
				<< Mv::restore << Mv::d(26) << kgraph3(mydata, true) << '\n'
				<< Mv::d(1) << "Init took " << time_micros() - kts << " μs.       " << endl;

		list<uint64_t> ktavg;
		while (true) {
			mydata.back() = std::rand() % 101;
			kts = time_micros();
			cout 	<< Term::sync_start << Mv::restore << kgraph(mydata)
					<< Mv::restore << Mv::d(13) << kgraph2(mydata)
					<< Mv::restore << Mv::d(26) << kgraph3(mydata)
					<< Term::sync_end << endl;
			ktavg.push_front(time_micros() - kts);
			if (ktavg.size() > 100) ktavg.pop_back();
			cout << Mv::d(1) << "Time: " << ktavg.front() << " μs.  Avg: " << accumulate(ktavg.begin(), ktavg.end(), 0) / ktavg.size() << "  μs.     " << flush;
			if (Input::poll()) {
				if (Input::get() == "space") Input::wait();
				else break;
			}
			sleep_ms(50);
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


//*------>>>>>> Proc testing


	auto timestamp = time_ms();



	string ostring;
	uint64_t tsl, timestamp2, rcount = 0;
	list<uint64_t> avgtimes;
	size_t timer = 2000;
	vector<string> greyscale;
	string filter;
	string filter_cur;
	string key;
	vector<Proc::proc_info> plist;

	int xc;
	for (size_t i : iota(0, (int)Term::height - 19)){
		xc = 230 - i * 150 / (Term::height - 20);
		greyscale.push_back(Theme::dec_to_color(xc, xc, xc));
	}

	while (key != "q") {
		timestamp = time_micros();
		tsl = time_ms() + timer;
		Config::lock();
		try {
			plist = Proc::collect();
		}
		catch (std::exception const& e) {
			Logger::error("Caught exception in Proc::collect() : "s + e.what());
			exit(1);
		}
		timestamp2 = time_micros();
		timestamp = timestamp2 - timestamp;
		ostring.clear();

		ostring = Proc::draw(plist);
		Config::unlock();

		avgtimes.push_front(timestamp);
		if (avgtimes.size() > 30) avgtimes.pop_back();
		cout << Term::sync_start << ostring << Fx::reset << Mv::to(2, 2) << '\n';
		cout << "  Details for " << Proc::detailed.entry.name << " (" << Proc::detailed.entry.pid << ")  Status: " << Proc::detailed.status << "  Elapsed: " << Proc::detailed.elapsed
			 << "  Mem: " << floating_humanizer(Proc::detailed.entry.mem) << "         "
			 << "\n  Parent: " << Proc::detailed.parent << "  IO in/out: " << Proc::detailed.io_read << "/" << Proc::detailed.io_write << "        ";
		cout << Mv::to(4, 2) << "Processes call took: " << rjust(to_string(timestamp), 5) << " μs. Average: " <<
			rjust(to_string(accumulate(avgtimes.begin(), avgtimes.end(), 0) / avgtimes.size()), 5) << " μs of " << avgtimes.size() <<
			" samples. Drawing took: " << time_micros() - timestamp2 << " μs.\nNumber of processes: " << Proc::numpids << ". Number in vector: " << plist.size() << ". Run count: " << ++rcount << ". Time: " << strf_time("%X   ") << Term::sync_end << flush;

		while (time_ms() < tsl and not Global::resized) {
			if (Input::poll(tsl - time_ms())) key = Input::get();
			else { key.clear() ; continue; }
			if (Config::getB("proc_filtering")) {
				if (key == "enter") Config::set("proc_filtering", false);
				else if (key == "backspace" and not filter.empty()) filter = uresize(filter, ulen(filter) - 1);
				else if (key == "space") filter.push_back(' ');
				else if (ulen(key) == 1 ) filter.append(key);
				else { key.clear(); continue; }
				if (filter != Config::getS("proc_filter")) Config::set("proc_filter", filter);
				key.clear();
				Proc::redraw = true;
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
			else if (key == "f") Config::flip("proc_filtering");
			else if (key == "t") Config::flip("proc_tree");
			else if (key == "r") Config::flip("proc_reversed");
			else if (key == "c") Config::flip("proc_per_core");
			else if (key == "delete") { filter.clear(); Config::set("proc_filter", filter); }
			else if (is_in(key, "up", "down", "page_up", "page_down", "home", "end")) {
				Proc::selection(key);
				cout << Proc::draw(plist) << flush;
				continue;
			}

			else continue;
			Proc::redraw = true;
			break;
		}
		if (Global::resized) {
			cout << Cpu::box << Mem::box << Net::box << Proc::box << flush;
			Global::resized = false;
		}
		cout << Mv::to(Term::height - 3, 1) << flush;
	}


//*-----<<<<<

	return 0;
}
