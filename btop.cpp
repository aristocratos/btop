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
#include <thread>
#include <future>
#include <atomic>
#include <numeric>
#include <ranges>
#include <filesystem>
#include <unistd.h>
#include <robin_hood.h>

namespace Global {
	const std::vector<std::array<std::string, 2>> Banner_src = {
		{"#E62525", "██████╗ ████████╗ ██████╗ ██████╗"},
		{"#CD2121", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗   ██╗    ██╗"},
		{"#B31D1D", "██████╔╝   ██║   ██║   ██║██████╔╝ ██████╗██████╗"},
		{"#9A1919", "██╔══██╗   ██║   ██║   ██║██╔═══╝  ╚═██╔═╝╚═██╔═╝"},
		{"#801414", "██████╔╝   ██║   ╚██████╔╝██║        ╚═╝    ╚═╝"},
		{"#000000", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝"},
	};

	const std::string Version = "0.0.10";
}

#include <btop_globs.h>
#include <btop_tools.h>
#include <btop_config.h>
#include <btop_input.h>
#include <btop_theme.h>
#include <btop_draw.h>

#if defined(__linux__)
	#define LINUX 1
	#include <btop_linux.h>
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
	#include <sys/param.h>
	#if defined(BSD)
		// #include <btop_bsd.h>
		#error BSD support not yet implemented!
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC == 1
		#define OSX 1
		// #include <btop_osx.h>
		#error OSX support not yet implemented!
    #endif
#else
	#error Platform not supported!
#endif

using std::string, std::vector, std::array, robin_hood::unordered_flat_map, std::atomic, std::endl, std::cout, std::views::iota, std::list, std::accumulate;
using std::flush, std::endl, std::future, std::string_literals::operator""s, std::future_status;
namespace fs = std::filesystem;
using namespace Tools;


namespace Global {
	string banner;
	const uint banner_width = 49;

	fs::path self_path;

	bool debuginit = false;
	bool debug = false;

	uint64_t start_time;

	bool quitting = false;
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
			cout << "help here" << endl;
			exit(0);
		}
		else if (argument == "--debug") Global::debug = true;
		else {
			cout << " Unknown argument: " << argument << "\n" <<
			" Use -h or --help for help." <<  endl;
			exit(1);
		}
	}
}

//* Generate the btop++ banner
string createBanner(){
	size_t z = 0;
	string b_color, bg, fg, out, oc, letter;
	bool truecolor = Config::getB("truecolor");
	int bg_i;
	for (auto line: Global::Banner_src) {
		fg = Theme::hex_to_color(line[0], !truecolor);
		bg_i = 120-z*12;
		bg = Theme::dec_to_color(bg_i, bg_i, bg_i, !truecolor);
		for (size_t i = 0; i < line[1].size(); i += 3) {
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
		if (++z < Global::Banner_src.size()) out += Mv::l(ulen(line[1])) + Mv::d(1);
	}
	out += Mv::r(18 - Global::Version.size()) + Fx::i + Theme::dec_to_color(0,0,0, !truecolor, "bg") +
			Theme::dec_to_color(150, 150, 150, !truecolor) + "v" + Global::Version + Fx::ui;
	return out;
}

void clean_quit(int signal){
	if (Global::quitting) return;
	if (Term::initialized) {
		Term::restore();
		if (!Global::debuginit) cout << Term::normal_screen << Term::show_cursor << flush;
	}
	if (Global::debug) Logger::debug("Quitting! Runtime: " + sec_to_dhms(time_s() - Global::start_time));
	Global::quitting = true;
	if (signal != -1) exit(signal);
}

void _exit_handler() { clean_quit(-1); }

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

	//? Init

	Global::start_time = time_s();

	cout.setf(std::ios::boolalpha);
	if (argc > 1) argumentParser(argc, argv);

	std::atexit(_exit_handler);

	#if defined(LINUX)
		//? Linux paths init
		Global::proc_path = (fs::is_directory(fs::path("/proc")) && access("/proc", R_OK) != -1) ? "/proc" : "";
		if (Global::proc_path.empty()) {
			cout << "ERROR: Proc filesystem not found or no permission to read from it!" << endl;
			exit(1);
		}
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
		std::error_code ec;
		if (!fs::is_directory(Config::conf_dir) && !fs::create_directories(Config::conf_dir, ec)) {
			cout << "WARNING: Could not create or access btop config directory. Logging and config saving disabled." << endl;
			cout << "Make sure your $HOME environment variable is correctly set to fix this." << endl;
		}
		else {
			std::error_code ec;
			Config::conf_file = Config::conf_dir / "btop.conf";
			Logger::logfile = Config::conf_dir / "btop.log";
			Theme::user_theme_dir = Config::conf_dir / "themes";
			if (!fs::exists(Theme::user_theme_dir) && !fs::create_directory(Theme::user_theme_dir, ec)) Theme::user_theme_dir.clear();
		}
	}
	if (!Global::self_path.empty()) {
			std::error_code ec;
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

	Global::debug = true;
	if (Global::debug) { Logger::loglevel = 4; Logger::debug("Starting in debug mode");}

	if (!string(getenv("LANG")).ends_with("UTF-8") && !string(getenv("LANG")).ends_with("utf-8")) {
		string err_msg = "No UTF-8 locale was detected! Symbols might not look as intended.";
		Logger::warning(err_msg);
		cout << "WARNING: " << err_msg << endl;
	}

	//? Initialize terminal and set options
	if (!Term::init()) {
		string err_msg = "No tty detected!";
		Logger::error(err_msg + " Quitting.");
		cout << "ERROR: " << err_msg << endl;
		cout << "btop++ needs an interactive shell to run." << endl;
		clean_quit(1);
	}

	//? Read config file if present
	Config::load("____");
	Config::setB("truecolor", false);

	auto thts = time_ms();

	//? Generate the theme
	Theme::set(Theme::Default_theme);

	//? Create the btop++ banner
	Global::banner = createBanner();


	//* ------------------------------------------------ TESTING ------------------------------------------------------


	Global::debuginit = false;

	// cout << Theme("main_bg") << Term::clear << flush;
	bool thread_test = false;

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

	if (false) {
		string a = "⣿ ⣿\n⣿⣿⣿⣿ ⣿\n⣿⣿⣿⣿ ⣿\n⣿⣿⣿⣿ ⣿\n⣿⣿⣿";
		cout << a << endl;
		exit(0);
	}


	if (thread_test){

		unordered_flat_map<int, future<string>> runners;
		unordered_flat_map<int, string> outputs;

		for (int i : iota(0, 10)){
			runners[i] = async(my_worker, i);
		}
		// uint i = 0;
		while (outputs.size() < 10){

			for (int i : iota(0, 10)){
				if (runners[i].valid() && runners[i].wait_for(std::chrono::milliseconds(10)) == future_status::ready) {
					outputs[i] = runners[i].get();
					cout << "Thread " << i << " : " << outputs[i] << endl;
				}
			}

			// if (++i >= 10) i = 0;
			if (outputs.size() >= 8) Global::stop_all.store(true);
		}
	}



	cout << "Up for " << sec_to_dhms(round(system_uptime())) << endl;


//*------>>>>>> Proc testing


	auto timestamp = time_ms();
	Proc::init();



	uint lc;
	string ostring;
	uint64_t tsl, timestamp2, rcount = 0;
	list<uint64_t> avgtimes = {0};
	uint timer = 1000;
	bool filtering = false;
	bool reversing = false;
	int sortint = Proc::sort_map["cpu lazy"];
	vector<string> greyscale;
	string filter;
	string filter_cur;
	string key;

	int xc;
	for (uint i : iota(0, (int)Term::height - 19)){
		xc = 230 - i * 150 / (Term::height - 20);
		greyscale.push_back(Theme::dec_to_color(xc, xc, xc));
	}

	string pbox = Draw::createBox({.x = 0, .y = 10, .width = Term::width, .height = Term::height - 16, .line_color = Theme::c("proc_box"), .title = "testbox", .title2 = "below", .fill = false, .num = 7});
	pbox += rjust("Pid:", 8) + " " + ljust("Program:", 16) + " " + ljust("Command:", Term::width - 69) + " Threads: " +
			ljust("User:", 10) + " " + rjust("MemB", 5) + " " + rjust("Cpu%", 14) + "\n";

	while (key != "q") {
		timestamp = time_micros();
		tsl = time_ms() + timer;
		auto plist = Proc::collect(Proc::sort_array[sortint], reversing, filter);
		timestamp2 = time_micros();
		timestamp = timestamp2 - timestamp;
		ostring.clear();
		lc = 0;
		filter_cur = (filtering) ? Fx::bl + "█" + Fx::reset : "";
		ostring = Mv::save + Mv::u(2) + Mv::r(20) + trans(rjust("Filter: " + filter + filter_cur + string(Term::width / 3, ' ') +
			"Sorting: " + string(Proc::sort_array[sortint]), Term::width - 25, true, filtering)) + Mv::restore;

		for (auto& p : plist){
			ostring += 	Mv::r(1) + greyscale[lc] + rjust(to_string(p.pid), 8) + " " + ljust(p.name, 16) + " " + ljust(p.cmd, Term::width - 66, true) + " " +
						rjust(to_string(p.threads), 5) + " " + ljust(p.user, 10) + " " + rjust(floating_humanizer(p.mem, true), 5) + string(11, ' ');
			ostring += (p.cpu_p > 100) ? rjust(to_string(p.cpu_p), 3) + " " : rjust(to_string(p.cpu_p), 4);
			ostring += "\n";
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
				break;
			}
			else if (key == "q") break;
			else if (key == "left") { if (--sortint < 0) sortint = (int)Proc::sort_array.size() - 1; }
			else if (key == "right") { if (++sortint > (int)Proc::sort_array.size() - 1) sortint = 0; }
			else if (key == "f") filtering = true;
			else if (key == "r") reversing = !reversing;
			else if (key == "delete") filter.clear();
			else continue;
			break;
		}
	}

	// cout << "Found " << plist.size() << " pids\n" << endl;

//*-----<<<<<

	return 0;
}
