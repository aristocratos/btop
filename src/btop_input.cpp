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

#include <limits>
#include <ranges>
#include <vector>
#include <thread>
#include <mutex>
#include <signal.h>
#include <sys/select.h>
#include <utility>

#include "btop_input.hpp"
#include "btop_tools.hpp"
#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_menu.hpp"
#include "btop_draw.hpp"

using namespace Tools;
using namespace std::literals; // for operator""s
namespace rng = std::ranges;

namespace Input {

	//* Map for translating key codes to readable values
	const std::unordered_map<string, string> Key_escapes = {
		{"\033",	"escape"},
		{"\x12",	"ctrl_r"},
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
		{"[4h",		"insert"},
		{"[3~",		"delete"},
		{"[P",		"delete"},
		{"[H",		"home"},
		{"[1~",		"home"},
		{"[F",		"end"},
		{"[4~",		"end"},
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

	sigset_t signal_mask;
	std::atomic<bool> polling (false);
	array<int, 2> mouse_pos;
	std::unordered_map<string, Mouse_loc> mouse_mappings;

	deque<string> history(50, "");
	string old_filter;
	string input;

	bool poll(const uint64_t timeout) {
		atomic_lock lck(polling);
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		struct timespec wait;
		struct timespec *waitptr = nullptr;

		if(timeout != std::numeric_limits<uint64_t>::max()) {
			wait.tv_sec = timeout / 1000;
			wait.tv_nsec = (timeout % 1000) * 1000000;
			waitptr = &wait;
		}

		if(pselect(STDIN_FILENO + 1, &fds, nullptr, nullptr, waitptr, &signal_mask) > 0) {
			input.clear();
			char buf[1024];
			ssize_t count = 0;
			while((count = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
				input.append(std::string_view(buf, count));
			}

			return true;
		}

		return false;
	}

	string get() {
		string key = input;
		if (not key.empty()) {
			//? Remove escape code prefix if present
			if (key.substr(0, 2) == Fx::e) {
				key.erase(0, 1);
			}
			//? Detect if input is an mouse event
			if (key.starts_with("[<")) {
				std::string_view key_view = key;
				string mouse_event;
				if (key_view.starts_with("[<0;") and key_view.find('M') != std::string_view::npos) {
					mouse_event = "mouse_click";
					key_view.remove_prefix(4);
				}
				// else if (key_view.starts_with("[<0;") and key_view.ends_with('m')) {
				// 	mouse_event = "mouse_release";
				// 	key_view.remove_prefix(4);
				// }
				else if (key_view.starts_with("[<64;")) {
					mouse_event = "mouse_scroll_up";
					key_view.remove_prefix(5);
				}
				else if (key_view.starts_with("[<65;")) {
					mouse_event = "mouse_scroll_down";
					key_view.remove_prefix(5);
				}
				else
					key.clear();

				if (Config::getB("proc_filtering")) {
					if (mouse_event == "mouse_click") return mouse_event;
					else return "";
				}

				//? Get column and line position of mouse and check for any actions mapped to current position
				if (not key.empty()) {
					try {
						const auto delim = key_view.find(';');
						mouse_pos[0] = stoi((string)key_view.substr(0, delim));
						mouse_pos[1] = stoi((string)key_view.substr(delim + 1, key_view.find('M', delim)));
					}
					catch (const std::invalid_argument&) { mouse_event.clear(); }
					catch (const std::out_of_range&) { mouse_event.clear(); }

					key = mouse_event;

					if (key == "mouse_click") {
						const auto& [col, line] = mouse_pos;

						for (const auto& [mapped_key, pos] : (Menu::active ? Menu::mouse_mappings : mouse_mappings)) {
							if (col >= pos.col and col < pos.col + pos.width and line >= pos.line and line < pos.line + pos.height) {
								key = mapped_key;
								break;
							}
						}
					}
				}

			}
			else if (Key_escapes.contains(key))
				key = Key_escapes.at(key);
			else if (ulen(key) > 1)
				key.clear();

			if (not key.empty()) {
				history.push_back(key);
				history.pop_front();
			}
		}
		return key;
	}

	string wait() {
		while(not poll(std::numeric_limits<uint64_t>::max())) {}
		return get();
	}

	void interrupt() {
		kill(getpid(), SIGUSR1);
	}

	void clear() {
		// do not need it, actually
	}

	void process(const string& key) {
		if (key.empty()) return;
		try {
			auto filtering = Config::getB("proc_filtering");
			auto vim_keys = Config::getB("vim_keys");
			auto help_key = (vim_keys ? "H" : "h");
			auto kill_key = (vim_keys ? "K" : "k");
			//? Global input actions
			if (not filtering) {
				bool keep_going = false;
				if (str_to_lower(key) == "q") {
					clean_quit(0);
				}
				else if (is_in(key, "escape", "m")) {
					Menu::show(Menu::Menus::Main);
					return;
				}
				else if (is_in(key, "F1", "?", help_key)) {
					Menu::show(Menu::Menus::Help);
					return;
				}
				else if (is_in(key, "F2", "o")) {
					Menu::show(Menu::Menus::Options);
					return;
				}
				else if (key.size() == 1 and isint(key)) {
					auto intKey = stoi(key);
				#ifdef GPU_SUPPORT
					static const array<string, 10> boxes = {"gpu5", "cpu", "mem", "net", "proc", "gpu0", "gpu1", "gpu2", "gpu3", "gpu4"};
					if ((intKey == 0 and Gpu::gpu_names.size() < 5) or (intKey >= 5 and std::cmp_less(Gpu::gpu_names.size(), intKey - 4)))
						return;
				#else
				static const array<string, 10> boxes = {"", "cpu", "mem", "net", "proc"};
					if (intKey == 0 or intKey > 4)
						return;
				#endif
					atomic_wait(Runner::active);
					Config::current_preset = -1;

					Config::toggle_box(boxes.at(intKey));
					Draw::calcSizes();
					Runner::run("all", false, true);
					return;
				}
				else if (is_in(key, "p", "P") and Config::preset_list.size() > 1) {
					if (key == "p") {
						if (++Config::current_preset >= (int)Config::preset_list.size()) Config::current_preset = 0;
					}
					else {
						if (--Config::current_preset < 0) Config::current_preset = Config::preset_list.size() - 1;
					}
					atomic_wait(Runner::active);
					Config::apply_preset(Config::preset_list.at(Config::current_preset));
					Draw::calcSizes();
					Runner::run("all", false, true);
					return;
				} else if (is_in(key, "ctrl_r")) {
					kill(getpid(), SIGUSR2);
					return;
				} else
					keep_going = true;

				if (not keep_going) return;
			}

			//? Input actions for proc box
			if (Proc::shown) {
				bool keep_going = false;
				bool no_update = true;
				bool redraw = true;
				if (filtering) {
					if (key == "enter" or key == "down") {
						Config::set("proc_filter", Proc::filter.text);
						Config::set("proc_filtering", false);
						old_filter.clear();
						if(key == "down"){
							process("down");
							return;
						}
					}
					else if (key == "escape" or key == "mouse_click") {
						Config::set("proc_filter", old_filter);
						Config::set("proc_filtering", false);
						old_filter.clear();
					}
					else if (Proc::filter.command(key)) {
						if (Config::getS("proc_filter") != Proc::filter.text)
							Config::set("proc_filter", Proc::filter.text);
					}
					else
						return;
				}
				else if (key == "left" or (vim_keys and key == "h")) {
					int cur_i = v_index(Proc::sort_vector, Config::getS("proc_sorting"));
					if (--cur_i < 0)
						cur_i = Proc::sort_vector.size() - 1;
					Config::set("proc_sorting", Proc::sort_vector.at(cur_i));
				}
				else if (key == "right" or (vim_keys and key == "l")) {
					int cur_i = v_index(Proc::sort_vector, Config::getS("proc_sorting"));
					if (std::cmp_greater(++cur_i, Proc::sort_vector.size() - 1))
						cur_i = 0;
					Config::set("proc_sorting", Proc::sort_vector.at(cur_i));
				}
				else if (is_in(key, "f", "/")) {
					Config::flip("proc_filtering");
					Proc::filter = { Config::getS("proc_filter") };
					old_filter = Proc::filter.text;
				}
				else if (key == "e") {
					Config::flip("proc_tree");
					no_update = false;
				}

				else if (key == "r")
					Config::flip("proc_reversed");

				else if (key == "c")
					Config::flip("proc_per_core");

				else if (key == "%")
					Config::flip("proc_mem_bytes");

				else if (key == "delete" and not Config::getS("proc_filter").empty())
					Config::set("proc_filter", ""s);

				else if (key.starts_with("mouse_")) {
					redraw = false;
					const auto& [col, line] = mouse_pos;
					const int y = (Config::getB("show_detailed") ? Proc::y + 8 : Proc::y);
					const int height = (Config::getB("show_detailed") ? Proc::height - 8 : Proc::height);
					if (col >= Proc::x + 1 and col < Proc::x + Proc::width and line >= y + 1 and line < y + height - 1) {
						if (key == "mouse_click") {
							if (col < Proc::x + Proc::width - 2) {
								const auto& current_selection = Config::getI("proc_selected");
								if (current_selection == line - y - 1) {
									redraw = true;
									if (Config::getB("proc_tree")) {
										const int x_pos = col - Proc::x;
										const int offset = Config::getI("selected_depth") * 3;
										if (x_pos > offset and x_pos < 4 + offset) {
											process("space");
											return;
										}
									}
									process("enter");
									return;
								}
								else if (current_selection == 0 or line - y - 1 == 0)
									redraw = true;
								Config::set("proc_selected", line - y - 1);
							}
							else if (line == y + 1) {
								if (Proc::selection("page_up") == -1) return;
							}
							else if (line == y + height - 2) {
								if (Proc::selection("page_down") == -1) return;
							}
							else if (Proc::selection("mousey" + to_string(line - y - 2)) == -1)
								return;
						}
						else
							goto proc_mouse_scroll;
					}
					else if (key == "mouse_click" and Config::getI("proc_selected") > 0) {
						Config::set("proc_selected", 0);
						redraw = true;
					}
					else
						keep_going = true;
				}
				else if (key == "enter") {
					if (Config::getI("proc_selected") == 0 and not Config::getB("show_detailed")) {
						return;
					}
					else if (Config::getI("proc_selected") > 0 and Config::getI("detailed_pid") != Config::getI("selected_pid")) {
						Config::set("detailed_pid", Config::getI("selected_pid"));
						Config::set("proc_last_selected", Config::getI("proc_selected"));
						Config::set("proc_selected", 0);
						Config::set("show_detailed", true);
					}
					else if (Config::getB("show_detailed")) {
						if (Config::getI("proc_last_selected") > 0) Config::set("proc_selected", Config::getI("proc_last_selected"));
						Config::set("proc_last_selected", 0);
						Config::set("detailed_pid", 0);
						Config::set("show_detailed", false);
					}
				}
				else if (is_in(key, "+", "-", "space") and Config::getB("proc_tree") and Config::getI("proc_selected") > 0) {
					atomic_wait(Runner::active);
					auto& pid = Config::getI("selected_pid");
					if (key == "+" or key == "space") Proc::expand = pid;
					if (key == "-" or key == "space") Proc::collapse = pid;
					no_update = false;
				}
				else if (is_in(key, "t", kill_key) and (Config::getB("show_detailed") or Config::getI("selected_pid") > 0)) {
					atomic_wait(Runner::active);
					if (Config::getB("show_detailed") and Config::getI("proc_selected") == 0 and Proc::detailed.status == "Dead") return;
					Menu::show(Menu::Menus::SignalSend, (key == "t" ? SIGTERM : SIGKILL));
					return;
				}
				else if (key == "s" and (Config::getB("show_detailed") or Config::getI("selected_pid") > 0)) {
					atomic_wait(Runner::active);
					if (Config::getB("show_detailed") and Config::getI("proc_selected") == 0 and Proc::detailed.status == "Dead") return;
					Menu::show(Menu::Menus::SignalChoose);
					return;
				}
				else if (is_in(key, "up", "down", "page_up", "page_down", "home", "end") or (vim_keys and is_in(key, "j", "k", "g", "G"))) {
					proc_mouse_scroll:
					redraw = false;
					auto old_selected = Config::getI("proc_selected");
					auto new_selected = Proc::selection(key);
					if (new_selected == -1)
						return;
					else if (old_selected != new_selected and (old_selected == 0 or new_selected == 0))
						redraw = true;
				}
				else keep_going = true;

				if (not keep_going) {
					Runner::run("proc", no_update, redraw);
					return;
				}
			}

			//? Input actions for cpu box
			if (Cpu::shown) {
				bool keep_going = false;
				bool no_update = true;
				bool redraw = true;
				static uint64_t last_press = 0;

				if (key == "+" and Config::getI("update_ms") <= 86399900) {
					int add = (Config::getI("update_ms") <= 86399000 and last_press >= time_ms() - 200
						and rng::all_of(Input::history, [](const auto& str){ return str == "+"; })
						? 1000 : 100);
					Config::set("update_ms", Config::getI("update_ms") + add);
					last_press = time_ms();
					redraw = true;
				}
				else if (key == "-" and Config::getI("update_ms") >= 200) {
					int sub = (Config::getI("update_ms") >= 2000 and last_press >= time_ms() - 200
						and rng::all_of(Input::history, [](const auto& str){ return str == "-"; })
						? 1000 : 100);
					Config::set("update_ms", Config::getI("update_ms") - sub);
					last_press = time_ms();
					redraw = true;
				}
				else keep_going = true;

				if (not keep_going) {
					Runner::run("cpu", no_update, redraw);
					return;
				}
			}

			//? Input actions for mem box
			if (Mem::shown) {
				bool keep_going = false;
				bool no_update = true;
				bool redraw = true;

				if (key == "i") {
					Config::flip("io_mode");
				}
				else if (key == "d") {
					Config::flip("show_disks");
					no_update = false;
					Draw::calcSizes();
				}
				else keep_going = true;

				if (not keep_going) {
					Runner::run("mem", no_update, redraw);
					return;
				}
			}

			//? Input actions for net box
			if (Net::shown) {
				bool keep_going = false;
				bool no_update = true;
				bool redraw = true;

				if (is_in(key, "b", "n")) {
					atomic_wait(Runner::active);
					int c_index = v_index(Net::interfaces, Net::selected_iface);
					if (c_index != (int)Net::interfaces.size()) {
						if (key == "b") {
							if (--c_index < 0) c_index = Net::interfaces.size() - 1;
						}
						else if (key == "n") {
							if (++c_index == (int)Net::interfaces.size()) c_index = 0;
						}
						Net::selected_iface = Net::interfaces.at(c_index);
						Net::rescale = true;
					}
				}
				else if (key == "y") {
					Config::flip("net_sync");
					Net::rescale = true;
				}
				else if (key == "a") {
					Config::flip("net_auto");
					Net::rescale = true;
				}
				else if (key == "z") {
					atomic_wait(Runner::active);
					auto& ndev = Net::current_net.at(Net::selected_iface);
					if (ndev.stat.at("download").offset + ndev.stat.at("upload").offset > 0) {
						ndev.stat.at("download").offset = 0;
						ndev.stat.at("upload").offset = 0;
					}
					else {
						ndev.stat.at("download").offset = ndev.stat.at("download").last + ndev.stat.at("download").rollover;
						ndev.stat.at("upload").offset = ndev.stat.at("upload").last + ndev.stat.at("upload").rollover;
					}
					no_update = false;
				}
				else keep_going = true;

				if (not keep_going) {
					Runner::run("net", no_update, redraw);
					return;
				}
			}
		}

		catch (const std::exception& e) {
			throw std::runtime_error("Input::process(\"" + key + "\") : " + string{e.what()});
		}
	}

}
