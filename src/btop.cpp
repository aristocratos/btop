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

#include <csignal>
#include <clocale>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include <thread>
#include <numeric>
#include <ranges>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <exception>
#include <tuple>
#include <regex>
#include <chrono>
#ifdef __APPLE__
	#include <CoreFoundation/CoreFoundation.h>
#endif

#include <btop_shared.hpp>
#include <btop_tools.hpp>
#include <btop_config.hpp>
#include <btop_input.hpp>
#include <btop_theme.hpp>
#include <btop_draw.hpp>
#include <btop_menu.hpp>

using std::string, std::string_view, std::vector, std::atomic, std::endl, std::cout, std::min, std::flush, std::endl;
using std::string_literals::operator""s, std::to_string;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;
using namespace std::chrono_literals;

namespace Global {
	const vector<array<string, 2>> Banner_src = {
		{"#E62525", "██████╗ ████████╗ ██████╗ ██████╗"},
		{"#CD2121", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗   ██╗    ██╗"},
		{"#B31D1D", "██████╔╝   ██║   ██║   ██║██████╔╝ ██████╗██████╗"},
		{"#9A1919", "██╔══██╗   ██║   ██║   ██║██╔═══╝  ╚═██╔═╝╚═██╔═╝"},
		{"#801414", "██████╔╝   ██║   ╚██████╔╝██║        ╚═╝    ╚═╝"},
		{"#000000", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝"},
	};
	const string Version = "1.1.4";

	int coreCount;
	string overlay;
	string clock;

	string bg_black = "\x1b[0;40m";
	string fg_white = "\x1b[1;97m";
	string fg_green = "\x1b[1;92m";
	string fg_red = "\x1b[0;91m";

	uid_t real_uid, set_uid;

	fs::path self_path;

	string exit_error_msg;
	atomic<bool> thread_exception (false);

	bool debuginit = false;
	bool debug = false;
	bool utf_force = false;

	uint64_t start_time;

	atomic<bool> resized (false);
	atomic<bool> quitting (false);
	atomic<bool> should_quit (false);
	atomic<bool> should_sleep (false);
	atomic<bool> _runner_started (false);

	bool arg_tty = false;
	bool arg_low_color = false;
	int arg_preset = -1;
}

//* A simple argument parser
void argumentParser(const int& argc, char **argv) {
	for(int i = 1; i < argc; i++) {
		const string argument = argv[i];
		if (is_in(argument, "-h", "--help")) {
			cout 	<< "usage: btop [-h] [-v] [-/+t] [-p <id>] [--utf-force] [--debug]\n\n"
					<< "optional arguments:\n"
					<< "  -h, --help            show this help message and exit\n"
					<< "  -v, --version         show version info and exit\n"
					<< "  -lc, --low-color      disable truecolor, converts 24-bit colors to 256-color\n"
					<< "  -t, --tty_on          force (ON) tty mode, max 16 colors and tty friendly graph symbols\n"
					<< "  +t, --tty_off         force (OFF) tty mode\n"
					<< "  -p, --preset <id>     start with preset, integer value between 0-9\n"
					<< "  --utf-force           force start even if no UTF-8 locale was detected\n"
					<< "  --debug               start in DEBUG mode: shows microsecond timer for information collect\n"
					<< "                        and screen draw functions and sets loglevel to DEBUG\n"
					<< endl;
			exit(0);
		}
		else if (is_in(argument, "-v", "--version")) {
			cout << "btop version: " << Global::Version << endl;
			exit(0);
		}
		else if (is_in(argument, "-lc", "--low-color")) {
			Global::arg_low_color = true;
		}
		else if (is_in(argument, "-t", "--tty_on")) {
			Config::set("tty_mode", true);
			Global::arg_tty = true;
		}
		else if (is_in(argument, "+t", "--tty_off")) {
			Config::set("tty_mode", false);
			Global::arg_tty = true;
		}
		else if (is_in(argument, "-p", "--preset")) {
			if (++i >= argc) {
				cout << "ERROR: Preset option needs an argument." << endl;
				exit(1);
			}
			else if (const string val = argv[i]; isint(val) and val.size() == 1) {
				Global::arg_preset = std::clamp(stoi(val), 0, 9);
			}
			else {
				cout << "ERROR: Preset option only accepts an integer value between 0-9." << endl;
				exit(1);
			}
		}
		else if (argument == "--utf-force")
			Global::utf_force = true;
		else if (argument == "--debug")
			Global::debug = true;
		else {
			cout << " Unknown argument: " << argument << "\n" <<
			" Use -h or --help for help." <<  endl;
			exit(1);
		}
	}
}

//* Handler for SIGWINCH and general resizing events, does nothing if terminal hasn't been resized unless force=true
void term_resize(bool force) {
	static atomic<bool> resizing (false);
	atomic_lock lck(resizing, true);
	if (auto refreshed = Term::refresh(true); refreshed or force) {
		if (force and refreshed) force = false;
	}
	else return;

	static const array<string, 4> all_boxes = {"cpu", "mem", "net", "proc"};
	Global::resized = true;
	if (Runner::active) Runner::stop();
	Term::refresh();
	Config::unlock();

	auto boxes = Config::getS("shown_boxes");
	auto min_size = Term::get_min_size(boxes);

	while (not force or (Term::width < min_size.at(0) or Term::height < min_size.at(1))) {
		sleep_ms(100);
		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			cout << Term::clear << Global::bg_black << Global::fg_white << Mv::to((Term::height / 2) - 2, (Term::width / 2) - 11)
				 << "Terminal size too small:" << Mv::to((Term::height / 2) - 1, (Term::width / 2) - 10)
				 << " Width = " << (Term::width < min_size.at(1) ? Global::fg_red : Global::fg_green) << Term::width
				 << Global::fg_white << " Height = " << (Term::height < min_size.at(0) ? Global::fg_red : Global::fg_green) << Term::height
				 << Mv::to((Term::height / 2) + 1, (Term::width / 2) - 12) << Global::fg_white
				 << "Needed for current config:" << Mv::to((Term::height / 2) + 2, (Term::width / 2) - 10)
				 << "Width = " << min_size.at(0) << " Height = " << min_size.at(1) << flush;
			while (not Term::refresh() and not Input::poll()) sleep_ms(10);
			if (Input::poll()) {
				auto key = Input::get();
				if (key == "q")
					clean_quit(0);
				else if (is_in(key, "1", "2", "3", "4")) {
					Config::current_preset = -1;
					Config::toggle_box(all_boxes.at(std::stoi(key) - 1));
					boxes = Config::getS("shown_boxes");
				}
			}
			min_size = Term::get_min_size(boxes);
		}
		else if (not Term::refresh()) break;
	}

	Input::interrupt = true;
}

//* Exit handler; stops threads, restores terminal and saves config changes
void clean_quit(int sig) {
	if (Global::quitting) return;
	Global::quitting = true;
	Runner::stop();
	if (Global::_runner_started) {
	#ifdef __APPLE__
		if (pthread_join(Runner::runner_id, NULL) != 0) {
			Logger::error("Failed to join _runner thread!");
			pthread_cancel(Runner::runner_id);
		}
	#else
		struct timespec ts;
		ts.tv_sec = 5;
		if (pthread_timedjoin_np(Runner::runner_id, NULL, &ts) != 0) {
			Logger::error("Failed to join _runner thread!");
			pthread_cancel(Runner::runner_id);
		}
	#endif
	}

	Config::write();

	//? Wait for any remaining Tools::atomic_lock destructors to finish for max 1000ms
	for (int i = 0; Tools::active_locks > 0 and i < 100; i++) {
		sleep_ms(10);
	}

	if (Term::initialized) {
		Input::clear();
		Term::restore();
	}

	if (not Global::exit_error_msg.empty()) {
		sig = 1;
		Logger::error(Global::exit_error_msg);
		std::cerr << Global::fg_red << "ERROR: " << Global::fg_white << Global::exit_error_msg << Fx::reset << endl;
	}
	Logger::info("Quitting! Runtime: " + sec_to_dhms(time_s() - Global::start_time));

	const auto excode = (sig != -1 ? sig : 0);

	//? Assume error if still not cleaned up and call quick_exit to avoid a segfault from Tools::atomic_lock destructor
#ifdef __APPLE__
	_Exit(excode);
#else
	quick_exit(excode);
#endif
}

//* Handler for SIGTSTP; stops threads, restores terminal and sends SIGSTOP
void _sleep() {
	Runner::stop();
	Term::restore();
	std::raise(SIGSTOP);
}

//* Handler for SIGCONT; re-initialize terminal and force a resize event
void _resume() {
	Term::init();
	term_resize(true);
}

void _exit_handler() {
	clean_quit(-1);
}

void _signal_handler(const int sig) {
	switch (sig) {
		case SIGINT:
			if (Runner::active) {
				Global::should_quit = true;
				Runner::stopping = true;
				Input::interrupt = true;
			}
			else {
				clean_quit(0);
			}
			break;
		case SIGTSTP:
			if (Runner::active) {
				Global::should_sleep = true;
				Runner::stopping = true;
				Input::interrupt = true;
			}
			else {
				_sleep();
			}
			break;
		case SIGCONT:
			_resume();
			break;
		case SIGWINCH:
			term_resize();
			break;
	}
}

//* Manages secondary thread for collection and drawing of boxes
namespace Runner {
	atomic<bool> active (false);
	atomic<bool> stopping (false);
	atomic<bool> waiting (false);
	atomic<bool> redraw (false);

	//* Setup semaphore for triggering thread to do work
#if __GNUC__ < 11
	#include <semaphore.h>
	sem_t do_work;
	inline void thread_sem_init() { sem_init(&do_work, 0, 0); }
	inline void thread_wait() { sem_wait(&do_work); }
	inline void thread_trigger() { sem_post(&do_work); }
#else
	#include <semaphore>
	std::binary_semaphore do_work(0);
	inline void thread_sem_init() { ; }
	inline void thread_wait() { do_work.acquire(); }
	inline void thread_trigger() { do_work.release(); }
#endif

	//* RAII wrapper for pthread_mutex locking
	class thread_lock {
		pthread_mutex_t& pt_mutex;
	public:
		int status;
		thread_lock(pthread_mutex_t& mtx) : pt_mutex(mtx) { pthread_mutex_init(&pt_mutex, NULL); status = pthread_mutex_lock(&pt_mutex); }
		~thread_lock() { if (status == 0) pthread_mutex_unlock(&pt_mutex); }
	};

	//* Wrapper for raising priviliges when using SUID bit
	class gain_priv {
		int status = -1;
	public:
		gain_priv() {
			if (Global::real_uid != Global::set_uid) this->status = seteuid(Global::set_uid);
		}
		~gain_priv() {
			if (status == 0) status = seteuid(Global::real_uid);
		}
	};

	string output;
	string empty_bg;
	bool pause_output = false;
	sigset_t mask;
	pthread_t runner_id;
	pthread_mutex_t mtx;

	enum debug_actions {
		collect_begin,
		draw_begin,
		draw_done
	};

	enum debug_array {
		collect,
		draw
	};

	string debug_bg;
	unordered_flat_map<string, array<uint64_t, 2>> debug_times;

	struct runner_conf {
		vector<string> boxes;
		bool no_update;
		bool force_redraw;
		bool background_update;
		string overlay;
		string clock;
	};

	struct runner_conf current_conf;

	void debug_timer(const char* name, const int action) {
		switch (action) {
			case collect_begin:
				debug_times[name].at(collect) = time_micros();
				return;
			case draw_begin:
				debug_times[name].at(draw) = time_micros();
				debug_times[name].at(collect) = debug_times[name].at(draw) - debug_times[name].at(collect);
				debug_times["total"].at(collect) += debug_times[name].at(collect);
				return;
			case draw_done:
				debug_times[name].at(draw) = time_micros() - debug_times[name].at(draw);
				debug_times["total"].at(draw) += debug_times[name].at(draw);
				return;
		}
	}

	//? ------------------------------- Secondary thread: async launcher and drawing ----------------------------------
	void * _runner(void * _) {
		(void)_;
		//? Block some signals in this thread to avoid deadlock from any signal handlers trying to stop this thread
		sigemptyset(&mask);
		// sigaddset(&mask, SIGINT);
		// sigaddset(&mask, SIGTSTP);
		sigaddset(&mask, SIGWINCH);
		sigaddset(&mask, SIGTERM);
		pthread_sigmask(SIG_BLOCK, &mask, NULL);

		//? pthread_mutex_lock to lock thread and monitor health from main thread
		thread_lock pt_lck(mtx);
		if (pt_lck.status != 0) {
			Global::exit_error_msg = "Exception in runner thread -> pthread_mutex_lock error id: " + to_string(pt_lck.status);
			Global::thread_exception = true;
			Input::interrupt = true;
			stopping = true;
		}

		//* ----------------------------------------------- THREAD LOOP -----------------------------------------------
		while (not Global::quitting) {
			thread_wait();
			atomic_wait_for(active, true, 5000);
			if (active) {
				Global::exit_error_msg = "Runner thread failed to get active lock!";
				Global::thread_exception = true;
				Input::interrupt = true;
				stopping = true;
			}
			if (stopping or Global::resized) {
				sleep_ms(1);
				continue;
			}

			//? Atomic lock used for blocking non thread-safe actions in main thread
			atomic_lock lck(active);

			//? Set effective user if SUID bit is set
			gain_priv powers{};

			auto& conf = current_conf;

			//! DEBUG stats
			if (Global::debug) {
				if (debug_bg.empty() or redraw) Runner::debug_bg = Draw::createBox(2, 2, 32, 8, "", true, "debug");
				debug_times.clear();
				debug_times["total"] = {0, 0};
			}

			output.clear();

			//* Run collection and draw functions for all boxes
			try {
				//? CPU
				if (v_contains(conf.boxes, "cpu")) {
					try {
						if (Global::debug) debug_timer("cpu", collect_begin);

						//? Start collect
						auto cpu = Cpu::collect(conf.no_update);

						if (Global::debug) debug_timer("cpu", draw_begin);

						//? Draw box
						if (not pause_output) output += Cpu::draw(cpu, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("cpu", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Cpu:: -> " + (string)e.what());
					}
				}

				//? MEM
				if (v_contains(conf.boxes, "mem")) {
					try {
						if (Global::debug) debug_timer("mem", collect_begin);

						//? Start collect
						auto mem = Mem::collect(conf.no_update);

						if (Global::debug) debug_timer("mem", draw_begin);

						//? Draw box
						if (not pause_output) output += Mem::draw(mem, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("mem", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Mem:: -> " + (string)e.what());
					}
				}

				//? NET
				if (v_contains(conf.boxes, "net")) {
					try {
						if (Global::debug) debug_timer("net", collect_begin);

						//? Start collect
						auto net = Net::collect(conf.no_update);

						if (Global::debug) debug_timer("net", draw_begin);

						//? Draw box
						if (not pause_output) output += Net::draw(net, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("net", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Net:: -> " + (string)e.what());
					}
				}

				//? PROC
				if (v_contains(conf.boxes, "proc")) {
					try {
						if (Global::debug) debug_timer("proc", collect_begin);

						//? Start collect
						auto proc = Proc::collect(conf.no_update);

						if (Global::debug) debug_timer("proc", draw_begin);

						//? Draw box
						if (not pause_output) output += Proc::draw(proc, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("proc", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Proc:: -> " + (string)e.what());
					}
				}
			}
			catch (const std::exception& e) {
				Global::exit_error_msg = "Exception in runner thread -> " + (string)e.what();
				Global::thread_exception = true;
				Input::interrupt = true;
				stopping = true;
			}

			if (stopping) {
				continue;
			}

			if (redraw or conf.force_redraw) {
				empty_bg.clear();
				redraw = false;
			}

			if (not pause_output) output += conf.clock;
			if (not conf.overlay.empty() and not conf.background_update) pause_output = true;
			if (output.empty() and not pause_output) {
				if (empty_bg.empty()) {
					const int x = Term::width / 2 - 10, y = Term::height / 2 - 10;
					output += Term::clear;
					empty_bg += Draw::banner_gen(y, 0, true)
						+ Mv::to(y+6, x) + Theme::c("title") + Fx::b + "No boxes shown!"
						+ Mv::to(y+8, x) + Theme::c("hi_fg") + "1" + Theme::c("main_fg") + " | Show CPU box"
						+ Mv::to(y+9, x) + Theme::c("hi_fg") + "2" + Theme::c("main_fg") + " | Show MEM box"
						+ Mv::to(y+10, x) + Theme::c("hi_fg") + "3" + Theme::c("main_fg") + " | Show NET box"
						+ Mv::to(y+11, x) + Theme::c("hi_fg") + "4" + Theme::c("main_fg") + " | Show PROC box"
						+ Mv::to(y+12, x-2) + Theme::c("hi_fg") + "esc" + Theme::c("main_fg") + " | Show menu"
						+ Mv::to(y+13, x) + Theme::c("hi_fg") + "q" + Theme::c("main_fg") + " | Quit";
				}
				output += empty_bg;
			}

			//! DEBUG stats -->
			if (Global::debug and not Menu::active) {
				output += debug_bg + Theme::c("title") + Fx::b + ljust(" Box", 9) + ljust("Collect μs", 12, true) + ljust("Draw μs", 9, true) + Theme::c("main_fg") + Fx::ub;
				for (const string name : {"cpu", "mem", "net", "proc", "total"}) {
					if (not debug_times.contains(name)) debug_times[name] = {0,0};
					const auto& [time_collect, time_draw] = debug_times.at(name);
					if (name == "total") output += Fx::b;
					output += Mv::l(29) + Mv::d(1) + ljust(name, 8) + ljust(to_string(time_collect), 12) + ljust(to_string(time_draw), 9);
				}
			}

			//? If overlay isn't empty, print output without color and then print overlay on top
			cout << Term::sync_start << (conf.overlay.empty()
					? output
					: (output.empty() ? "" : Fx::ub + Theme::c("inactive_fg") + Fx::uncolor(output)) + conf.overlay)
				<< Term::sync_end << flush;
		}
		//* ----------------------------------------------- THREAD LOOP -----------------------------------------------
		pthread_exit(NULL);
	}
	//? ------------------------------------------ Secondary thread end -----------------------------------------------

	//* Runs collect and draw in a secondary thread, unlocks and locks config to update cached values
	void run(const string& box, const bool no_update, const bool force_redraw) {
		atomic_wait_for(active, true, 5000);
		if (active) {
			Logger::error("Stall in Runner thread, restarting!");
			active = false;
			// exit(1);
			pthread_cancel(Runner::runner_id);
			if (pthread_create(&Runner::runner_id, NULL, &Runner::_runner, NULL) != 0) {
				Global::exit_error_msg = "Failed to re-create _runner thread!";
				clean_quit(1);
			}
		}
		if (stopping or Global::resized) return;

		if (box == "overlay") {
			cout << Term::sync_start << Global::overlay << Term::sync_end << flush;
		}
		else if (box == "clock") {
			cout << Term::sync_start << Global::clock << Term::sync_end << flush;
		}
		else {
			Config::unlock();
			Config::lock();

			current_conf = {
				(box == "all" ? Config::current_boxes : vector{box}),
				no_update, force_redraw,
				(not Config::getB("tty_mode") and Config::getB("background_update")),
				Global::overlay,
				Global::clock
			};

			if (Menu::active and not current_conf.background_update) Global::overlay.clear();

			thread_trigger();
			atomic_wait_for(active, false, 10);
		}


	}

	//* Stops any work being done in runner thread and checks for thread errors
	void stop() {
		stopping = true;
		int ret = pthread_mutex_trylock(&mtx);
		if (ret != EBUSY and not Global::quitting) {
			if (active) active = false;
			Global::exit_error_msg = "Runner thread died unexpectedly!";
			clean_quit(1);
		}
		else if (ret == EBUSY) {
			atomic_wait_for(active, true, 5000);
			if (active) {
				active = false;
				if (Global::quitting) {
					return;
				}
				else {
					Global::exit_error_msg = "No response from Runner thread, quitting!";
					clean_quit(1);
				}
			}
			thread_trigger();
			atomic_wait_for(active, false, 100);
			atomic_wait_for(active, true, 100);
		}
		stopping = false;
	}

}


//* --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv) {

	//? ------------------------------------------------ INIT ---------------------------------------------------------

	Global::start_time = time_s();

	//? Save real and effective userid's and drop priviliges until needed if running with SUID bit set
	Global::real_uid = getuid();
	Global::set_uid = geteuid();
	if (Global::real_uid != Global::set_uid) {
		if (seteuid(Global::real_uid) != 0) {
			Global::real_uid = Global::set_uid;
			Global::exit_error_msg = "Failed to change effective user ID. Unset btop SUID bit to ensure security on this system. Quitting!";
			clean_quit(1);
		}
	}

	//? Call argument parser if launched with arguments
	if (argc > 1) argumentParser(argc, argv);

	//? Setup paths for config, log and user themes
	for (const auto& env : {"XDG_CONFIG_HOME", "HOME"}) {
		if (std::getenv(env) != NULL and access(std::getenv(env), W_OK) != -1) {
			Config::conf_dir = fs::path(std::getenv(env)) / (((string)env == "HOME") ? ".config/btop" : "btop");
			break;
		}
	}
	if (Config::conf_dir.empty()) {
		cout 	<< "WARNING: Could not get path user HOME folder.\n"
				<< "Make sure $XDG_CONFIG_HOME or $HOME environment variables is correctly set to fix this." << endl;
	}
	else {
		if (std::error_code ec; not fs::is_directory(Config::conf_dir) and not fs::create_directories(Config::conf_dir, ec)) {
			cout 	<< "WARNING: Could not create or access btop config directory. Logging and config saving disabled.\n"
					<< "Make sure $XDG_CONFIG_HOME or $HOME environment variables is correctly set to fix this." << endl;
		}
		else {
			Config::conf_file = Config::conf_dir / "btop.conf";
			Logger::logfile = Config::conf_dir / "btop.log";
			Theme::user_theme_dir = Config::conf_dir / "themes";
			if (not fs::exists(Theme::user_theme_dir) and not fs::create_directory(Theme::user_theme_dir, ec)) Theme::user_theme_dir.clear();
		}
	}
	//? Try to find global btop theme path relative to binary path
#if defined(__linux__)
	{ 	std::error_code ec;
		Global::self_path = fs::read_symlink("/proc/self/exe", ec).remove_filename();
	}
#endif
	if (std::error_code ec; not Global::self_path.empty()) {
		Theme::theme_dir = fs::canonical(Global::self_path / "../share/btop/themes", ec);
		if (ec or not fs::is_directory(Theme::theme_dir) or access(Theme::theme_dir.c_str(), R_OK) == -1) Theme::theme_dir.clear();
	}
	//? If relative path failed, check two most common absolute paths
	if (Theme::theme_dir.empty()) {
		for (auto theme_path : {"/usr/local/share/btop/themes", "/usr/share/btop/themes"}) {
			if (fs::is_directory(fs::path(theme_path)) and access(theme_path, R_OK) != -1) {
				Theme::theme_dir = fs::path(theme_path);
				break;
			}
		}
	}

	//? Config init
	{	vector<string> load_warnings;
		Config::load(Config::conf_file, load_warnings);

		if (Config::current_boxes.empty()) Config::check_boxes(Config::getS("shown_boxes"));
		Config::set("lowcolor", (Global::arg_low_color ? true : not Config::getB("truecolor")));

		if (Global::debug) {
			Logger::set("DEBUG");
			Logger::debug("Starting in DEBUG mode!");
		}
		else Logger::set(Config::getS("log_level"));

		Logger::info("Logger set to " + (Global::debug ? "DEBUG" : Config::getS("log_level")));

		for (const auto& err_str : load_warnings) Logger::warning(err_str);
	}

	//? Try to find and set a UTF-8 locale
	if (std::setlocale(LC_ALL, "") != NULL and not s_contains((string)std::setlocale(LC_ALL, ""), ";")
	and str_to_upper(s_replace((string)std::setlocale(LC_ALL, ""), "-", "")).ends_with("UTF8")) {
		Logger::debug("Using locale " + (string)std::setlocale(LC_ALL, ""));
	}
	else {
		string found;
		bool set_failure = false;
		for (const auto loc_env : array{"LANG", "LC_ALL"}) {
			if (std::getenv(loc_env) != NULL and str_to_upper(s_replace((string)std::getenv(loc_env), "-", "")).ends_with("UTF8")) {
				found = std::getenv(loc_env);
				if (std::setlocale(LC_ALL, found.c_str()) == NULL) {
					set_failure = true;
					Logger::warning("Failed to set locale " + found + " continuing anyway.");
				}
			}
		}
		if (found.empty()) {
			if (setenv("LC_ALL", "", 1) == 0 and setenv("LANG", "", 1) == 0) {
				try {
					if (const auto loc = std::locale("").name(); not loc.empty() and loc != "*") {
						for (auto& l : ssplit(loc, ';')) {
							if (str_to_upper(s_replace(l, "-", "")).ends_with("UTF8")) {
								found = l.substr(l.find('=') + 1);
								if (std::setlocale(LC_ALL, found.c_str()) != NULL) {
									break;
								}
							}
						}
					}
				}
				catch (...) { found.clear(); }
			}
		}

	#ifdef __APPLE__
		if (found.empty()) {
			CFLocaleRef cflocale = CFLocaleCopyCurrent();
			CFStringRef id_value = (CFStringRef)CFLocaleGetValue(cflocale, kCFLocaleIdentifier);
			auto loc_id = CFStringGetCStringPtr(id_value, kCFStringEncodingUTF8);
			CFRelease(cflocale);
			std::string cur_locale = (loc_id != nullptr ? loc_id : "");
			if (cur_locale.empty()) {
				Logger::warning("No UTF-8 locale detected! Some symbols might not display correctly.");
			}
			else if (std::setlocale(LC_ALL, string(cur_locale + ".UTF-8").c_str()) != NULL) {
				Logger::debug("Setting LC_ALL=" + cur_locale + ".UTF-8");
			}
			else if(std::setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
				Logger::debug("Setting LC_ALL=en_US.UTF-8");
			}
			else {
				Logger::warning("Failed to set macos locale, continuing anyway.");
			}
		}
	#else
		if (found.empty() and Global::utf_force)
			Logger::warning("No UTF-8 locale detected! Forcing start with --utf-force argument.");
		else if (found.empty()) {
			Global::exit_error_msg = "No UTF-8 locale detected!\nUse --utf-force argument to force start if you're sure your terminal can handle it.";
			clean_quit(1);
		}
	#endif
		else if (not set_failure)
			Logger::debug("Setting LC_ALL=" + found);
	}

	//? Initialize terminal and set options
	if (not Term::init()) {
		Global::exit_error_msg = "No tty detected!\nbtop++ needs an interactive shell to run.";
		clean_quit(1);
	}

	if (Term::current_tty != "unknown") Logger::info("Running on " + Term::current_tty);
	if (not Global::arg_tty and Config::getB("force_tty")) {
		Config::set("tty_mode", true);
		Logger::info("Forcing tty mode: setting 16 color mode and using tty friendly graph symbols");
	}
#ifndef __APPLE__
	else if (not Global::arg_tty and Term::current_tty.starts_with("/dev/tty")) {
		Config::set("tty_mode", true);
		Logger::info("Real tty detected: setting 16 color mode and using tty friendly graph symbols");
	}
#endif

	//? Check for valid terminal dimensions
	{
		int t_count = 0;
		while (Term::width <= 0 or Term::width > 10000 or Term::height <= 0 or Term::height > 10000) {
			sleep_ms(10);
			Term::refresh();
			if (++t_count == 100) {
				Global::exit_error_msg = "Failed to get size of terminal!";
				clean_quit(1);
			}
		}
	}

	//? Platform dependent init and error check
	try {
		Shared::init();
	}
	catch (const std::exception& e) {
		Global::exit_error_msg = "Exception in Shared::init() -> " + (string)e.what();
		clean_quit(1);
	}

	//? Update list of available themes and generate the selected theme
	Theme::updateThemes();
	Theme::setTheme();

	//? Setup signal handlers for CTRL-C, CTRL-Z, resume and terminal resize
	std::atexit(_exit_handler);
	std::signal(SIGINT, _signal_handler);
	std::signal(SIGTSTP, _signal_handler);
	std::signal(SIGCONT, _signal_handler);
	std::signal(SIGWINCH, _signal_handler);

	//? Start runner thread
	Runner::thread_sem_init();
	if (pthread_create(&Runner::runner_id, NULL, &Runner::_runner, NULL) != 0) {
		Global::exit_error_msg = "Failed to create _runner thread!";
		clean_quit(1);
	}
	else {
		Global::_runner_started = true;
	}

	//? Calculate sizes of all boxes
	Config::presetsValid(Config::getS("presets"));
	if (Global::arg_preset >= 0) {
		Config::current_preset = min(Global::arg_preset, (int)Config::preset_list.size() - 1);
		Config::apply_preset(Config::preset_list.at(Config::current_preset));
	}

	{
		const auto [x, y] = Term::get_min_size(Config::getS("shown_boxes"));
		if (Term::height < y or Term::width < x) {
			term_resize(true);
			Global::resized = false;
			Input::interrupt = false;
		}

	}

	Draw::calcSizes();

	//? Print out box outlines
	cout << Term::sync_start << Cpu::box << Mem::box << Net::box << Proc::box << Term::sync_end << flush;


	//? ------------------------------------------------ MAIN LOOP ----------------------------------------------------

	uint64_t update_ms = Config::getI("update_ms");
	auto future_time = time_ms();

	try {
		while (not true not_eq not false) {
			//? Check for exceptions in secondary thread and exit with fail signal if true
			if (Global::thread_exception) clean_quit(1);
			else if (Global::should_quit) clean_quit(0);
			else if (Global::should_sleep) { Global::should_sleep = false; _sleep(); }

			//? Make sure terminal size hasn't changed (in case of SIGWINCH not working properly)
			term_resize();

			//? Trigger secondary thread to redraw if terminal has been resized
			if (Global::resized) {
				Draw::calcSizes();
				Draw::update_clock(true);
				Global::resized = false;
				if (Menu::active) Menu::process();
				else Runner::run("all", true, true);
				atomic_wait_for(Runner::active, true, 1000);
			}

			//? Update clock if needed
			if (Draw::update_clock() and not Menu::active) {
				Runner::run("clock");
			}

			//? Start secondary collect & draw thread at the interval set by <update_ms> config value
			if (time_ms() >= future_time and not Global::resized) {
				Runner::run("all");
				update_ms = Config::getI("update_ms");
				future_time = time_ms() + update_ms;
			}

			//? Loop over input polling and input action processing
			for (auto current_time = time_ms(); current_time < future_time; current_time = time_ms()) {

				//? Check for external clock changes and for changes to the update timer
				if (std::cmp_not_equal(update_ms, Config::getI("update_ms"))) {
					update_ms = Config::getI("update_ms");
					future_time = time_ms() + update_ms;
				}
				else if (future_time - current_time > update_ms)
					future_time = current_time;

				//? Poll for input and process any input detected
				else if (Input::poll(min((uint64_t)1000, future_time - current_time))) {
					if (not Runner::active) Config::unlock();

					if (Menu::active) Menu::process(Input::get());
					else Input::process(Input::get());
				}

				//? Break the loop at 1000ms intervals or if input polling was interrupted
				else break;

			}

		}
	}
	catch (const std::exception& e) {
		Global::exit_error_msg = "Exception in main loop -> " + (string)e.what();
		clean_quit(1);
	}

}
