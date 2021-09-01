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

#include <deque>
#include <robin_hood.h>
#include <array>
#include <ranges>
#include <signal.h>
#include <errno.h>

#include <btop_menu.hpp>
#include <btop_tools.hpp>
#include <btop_config.hpp>
#include <btop_theme.hpp>
#include <btop_draw.hpp>
#include <btop_shared.hpp>

using std::deque, robin_hood::unordered_flat_map, std::array, std::views::iota, std::ref;
using namespace Tools;
namespace rng = std::ranges;

namespace Menu {

   atomic<bool> active (false);
   string bg;
   bool redraw = true;
   int currentMenu = -1;
   msgBox messageBox;
   int signalToSend = 0;
   int signalKillRet = 0;

   const array<string, 32> P_Signals = {
	   	"0",
		"SIGHUP", "SIGINT",	"SIGQUIT",	"SIGILL",
		"SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE",
		"SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2",
		"SIGPIPE", "SIGALRM", "SIGTERM", "16", "SIGCHLD",
		"SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN",
		"SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ",
		"SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
		"SIGPWR", "SIGSYS"
	};

  unordered_flat_map<string, Input::Mouse_loc> mouse_mappings;

   const unordered_flat_map<string, unordered_flat_map<string, vector<string>>> menus = {
		{ "options", {
			{ "normal", {
				"┌─┐┌─┐┌┬┐┬┌─┐┌┐┌┌─┐",
				"│ │├─┘ │ ││ ││││└─┐",
				"└─┘┴   ┴ ┴└─┘┘└┘└─┘"
				} },
			{ "selected", {
				"╔═╗╔═╗╔╦╗╦╔═╗╔╗╔╔═╗",
				"║ ║╠═╝ ║ ║║ ║║║║╚═╗",
				"╚═╝╩   ╩ ╩╚═╝╝╚╝╚═╝"
				} }
		} },
		{ "help", {
			{ "normal", {
				"┬ ┬┌─┐┬  ┌─┐",
				"├─┤├┤ │  ├─┘",
				"┴ ┴└─┘┴─┘┴  "
				} },
			{ "selected", {
				"╦ ╦╔═╗╦  ╔═╗",
				"╠═╣║╣ ║  ╠═╝",
				"╩ ╩╚═╝╩═╝╩  "
				} }
		} },
		{ "quit", {
			{ "normal", {
				"┌─┐ ┬ ┬ ┬┌┬┐",
				"│─┼┐│ │ │ │ ",
				"└─┘└└─┘ ┴ ┴ "
				} },
			{ "selected", {
				"╔═╗ ╦ ╦ ╦╔╦╗ ",
				"║═╬╗║ ║ ║ ║  ",
				"╚═╝╚╚═╝ ╩ ╩  "
				} }
		} }
	};
	msgBox::msgBox() {};
	msgBox::msgBox(int width, int boxtype, vector<string>& content, string title)
	: width(width), boxtype(boxtype) {
		const auto& tty_mode = Config::getB("tty_mode");
		const auto& rounded = Config::getB("rounded_corners");
		const auto& right_up = (tty_mode or not rounded ? Symbols::right_up : Symbols::round_right_up);
		const auto& left_up = (tty_mode or not rounded ? Symbols::left_up : Symbols::round_left_up);
		const auto& right_down = (tty_mode or not rounded ? Symbols::right_down : Symbols::round_right_down);
		const auto& left_down = (tty_mode or not rounded ? Symbols::left_down : Symbols::round_left_down);
		height = content.size() + 7;
		x = Term::width / 2 - width / 2;
		y = Term::height/2 - height/2;
		if (boxtype == 2) selected = 1;


		button_left = left_up + Symbols::h_line * 6 + Mv::l(7) + Mv::d(2) + left_down + Symbols::h_line * 6 + Mv::l(7) + Mv::u(1) + Symbols::v_line;
		button_right = Symbols::v_line + Mv::l(7) + Mv::u(1) + Symbols::h_line * 6 + right_up + Mv::l(7) + Mv::d(2) + Symbols::h_line * 6 + right_down + Mv::u(2);

		box_contents = Draw::createBox(x, y, width, height, Theme::c("hi_fg"), true, title) + Mv::d(1);
		for (const auto& line : content) {
			box_contents += Mv::save + Mv::r(width / 2 - Fx::uncolor(line).size() / 2) + line + Mv::restore + Mv::d(1);
		}
	}

	string msgBox::operator()() {
		string out;
		int pos = width / 2 - (boxtype == 0 ? 6 : 14);
		auto& first_color = (selected == 0 ? Theme::c("hi_fg") : Theme::c("div_line"));
		out = Mv::d(1) + Mv::r(pos) + Fx::b + first_color + button_left + (selected == 0 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
			+ (boxtype == 0 ? "    Ok    " : "    Yes    ") + first_color + button_right;
		mouse_mappings["button1"] = Input::Mouse_loc{y + height - 4, x + pos + 1, 3, 12 + (boxtype > 0 ? 1 : 0)};
		if (boxtype > 0) {
			auto& second_color = (selected == 1 ? Theme::c("hi_fg") : Theme::c("div_line"));
			out += Mv::r(2) + second_color + button_left + (selected == 1 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
				+ "    No    " + second_color + button_right;
			mouse_mappings["button2"] = Input::Mouse_loc{y + height - 4, x + pos + 15 + (boxtype > 0 ? 1 : 0), 3, 12};
		}
		return box_contents + out + Fx::reset;
	}

	//? Process input
	int msgBox::input(string key) {
		if (key.empty()) return Invalid;

		if (is_in(key, "escape", "backspace", "q") or key == "button2") {
			return No_Esc;
		}
		else if (key == "button1" or (boxtype == 0 and str_to_upper(key) == "O")) {
			return Ok_Yes;
		}
		else if (is_in(key, "enter", "space")) {
			return selected + 1;
		}
		else if (boxtype == 0) {
			return Invalid;
		}
		else if (str_to_upper(key) == "Y") {
			return Ok_Yes;
		}
		else if (str_to_upper(key) == "N") {
			return No_Esc;
		}
		else if (is_in(key, "right", "tab")) {
			if (++selected > 1) selected = 0;
			return Select;
		}
		else if (is_in(key, "left", "shift_tab")) {
			if (--selected < 0) selected = 1;
			return Select;
		}

		return Invalid;
	}

	void msgBox::clear() {
		box_contents.clear();
		box_contents.shrink_to_fit();
		button_left.clear();
		button_left.shrink_to_fit();
		button_right.clear();
		button_right.shrink_to_fit();
		if (mouse_mappings.contains("button1")) mouse_mappings.erase("button1");
		if (mouse_mappings.contains("button2")) mouse_mappings.erase("button2");
	}

	enum menuReturnCodes {
		NoChange,
		Changed,
		Closed
	};

	int signalChoose(const string& key) {
		auto& s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		static int x = 0, y = 0, selected_signal = -1;
		if (bg.empty()) selected_signal = -1;
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 40;
			y = Term::height/2 - 9;
			bg = Draw::createBox(x, y, 80, 18, Theme::c("hi_fg"), true, "signals");
			bg += Mv::to(y+2, x+1) + Theme::c("title") + Fx::b + cjust("Send signal to PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name")), 30) + ")", 78);
		}

		if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (is_in(key, "enter", "space") and selected_signal >= 0) {
			signalKillRet = 0;
			if (s_pid < 1) {
				signalKillRet = ESRCH;
				menuMask.set(SignalReturn);
			}
			else if (kill(s_pid, selected_signal) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			return Closed;
		}
		else if (key.size() == 1 and isdigit(key.at(0)) and selected_signal < 10) {
			selected_signal = std::min(std::stoi((selected_signal < 1 ? key : to_string(selected_signal) + key)), 64);
		}
		else if (key == "backspace" and selected_signal != -1) {
			selected_signal = (selected_signal < 10 ? -1 : selected_signal / 10);
		}
		else if (key == "up" and selected_signal != 16) {
			if (selected_signal < 6) selected_signal += 25;
			else {
				bool offset = (selected_signal > 16);
				selected_signal -= 5;
				if (selected_signal <= 16 and offset) selected_signal--;
			}
		}
		else if (key == "down") {
			if (selected_signal < 1 or selected_signal == 16) selected_signal = 1;
			else if (selected_signal > 26) selected_signal -= 25;
			else {
				bool offset = (selected_signal < 16);
				selected_signal += 5;
				if (selected_signal >= 16 and offset) selected_signal++;
				if (selected_signal > 31) selected_signal = 31;
			}
		}
		else if (key == "left" and selected_signal > 1 and selected_signal != 16) {
			selected_signal--;
			if (selected_signal == 16) selected_signal--;
		}
		else if (key == "right" and selected_signal < 31 and selected_signal != 16) {
			selected_signal++;
			if (selected_signal == 16) selected_signal++;
		}
		else {
			retval = NoChange;
		}

		int cy = y+3;
		out = bg + Mv::to(cy++, x+1) + Theme::c("main_fg") + Fx::ub
			+ rjust("Enter signal number: ", 48) + (selected_signal >= 0 ? to_string(selected_signal) : "") + Fx::bl + "█" + Fx::ubl;

		out += Mv::to(++cy, x+4);
		auto sig_str = to_string(selected_signal);
		for (int count = 0, i = 0; const auto& sig : P_Signals) {
			if (count == 0 or count == 16) { count++; continue; }
			if (i++ % 5 == 0) out += Mv::to(++cy, x+4);
			if (count == selected_signal) out += Theme::c("selected_bg") + Theme::c("selected_fg") + Fx::b + ljust(to_string(count), 3) + ljust('(' + sig + ')', 12) + Fx::reset;
			else out += Theme::c("hi_fg") + ljust(to_string(count), 3) + Theme::c("main_fg") + ljust('(' + sig + ')', 12);
			count++;
		}

		cy++;
		out += Mv::to(++cy, x+1) + Fx::b + rjust("ENTER | ", 35) + Fx::ub + "To send signal.";
		out += Mv::to(++cy, x+1) + Fx::b + rjust( "↑ ↓ ← → | ", 35, true) + Fx::ub + "To choose signal.";
		out += Mv::to(++cy, x+1) + Fx::b + rjust("ESC or \"q\" | ", 35) + Fx::ub + "To abort.";

		out += Fx::reset;





		return (redraw ? Changed : retval);
	}

	int signalSend(const string& key) {
		auto& s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		if (s_pid == 0) return Closed;
		if (redraw) {
			atomic_wait(Runner::active);
			auto& p_name = (s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name"));
			vector<string> cont_vec = {
				Fx::b + Theme::c("main_fg") + "Send signal: " + Fx::ub + Theme::c("hi_fg") + to_string(signalToSend)
				+ (signalToSend > 0 and signalToSend <= 32 ? Theme::c("main_fg") + " (" + P_Signals.at(signalToSend) + ')' : ""),

				Fx::b + Theme::c("main_fg") + "To PID: " + Fx::ub + Theme::c("hi_fg") + to_string(s_pid) + Theme::c("main_fg") + " ("
				+ uresize(p_name, 16) + ')' + Fx::reset,
			};
			messageBox = Menu::msgBox{50, 1, cont_vec, (signalToSend > 1 and signalToSend <= 32 and signalToSend != 17 ? P_Signals.at(signalToSend) : "signal")};
			Global::overlay = messageBox();
		}
		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes) {
			signalKillRet = 0;
			if (kill(s_pid, signalToSend) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::Select) {
			Global::overlay = messageBox();
			return Changed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	int signalReturn(const string& key) {
		if (redraw) {
			vector<string> cont_vec;
			cont_vec.push_back(Fx::b + Theme::g("used")[100] + "Failure:" + Theme::c("main_fg") + Fx::ub);
			if (signalKillRet == EINVAL) {
				cont_vec.push_back("Unsupported signal!" + Fx::reset);
			}
			else if (signalKillRet == EPERM) {
				cont_vec.push_back("Insufficient permissions to send signal!" + Fx::reset);
			}
			else if (signalKillRet == ESRCH) {
				cont_vec.push_back("Process not found!" + Fx::reset);
			}
			else {
				cont_vec.push_back("Unknown error! (errno: " + to_string(signalKillRet) + ')' + Fx::reset);
			}

			messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	int mainMenu(const string& key) {
		(void)key;
		return NoChange;
	}
	int optionsMenu(const string& key) {
		(void)key;
		return NoChange;
	}
	int helpMenu(const string& key) {
		(void)key;
		return NoChange;
	}

	//* Add menus here and update enum Menus in header
	const auto menuFunc = vector{
		ref(signalChoose),
		ref(signalSend),
		ref(signalReturn),
		ref(optionsMenu),
		ref(helpMenu),
		ref(mainMenu),
	};
	bitset<8> menuMask;

	void process(string key) {
		if (menuMask.none()) {
			Menu::active = false;
			Global::overlay.clear();
			Global::overlay.shrink_to_fit();
			bg.clear();
			bg.shrink_to_fit();
			currentMenu = -1;
			Runner::run("all", true, true);
			return;
		}

		if (currentMenu < 0 or not menuMask.test(currentMenu)) {
			Menu::active = true;
			redraw = true;
			for (const auto& i : iota(0, (int)menuMask.size())) {
				if (menuMask.test(i)) currentMenu = i;
			}
		}

		auto retCode = menuFunc.at(currentMenu)(key);
		if (retCode == Closed) {
			menuMask.reset(currentMenu);
			process();
		}
		else if (redraw) {
			redraw = false;
			Runner::run("all", true, true);
		}
		else if (retCode == Changed)
			Runner::run("overlay");
	}
}