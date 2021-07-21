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

#include <array>
#include <algorithm>
#include <cmath>

#include <btop_draw.hpp>
#include <btop_config.hpp>
#include <btop_theme.hpp>
#include <btop_shared.hpp>
#include <btop_tools.hpp>


using 	std::round, std::views::iota, std::string_literals::operator""s, std::clamp, std::array, std::floor, std::max, std::min,
		std::to_string;

using namespace Tools;

namespace Symbols {
	const string h_line			= "─";
	const string v_line			= "│";
	const string left_up		= "┌";
	const string right_up		= "┐";
	const string left_down		= "└";
	const string right_down		= "┘";
	const string title_left		= "┤";
	const string title_right	= "├";
	const string div_up			= "┬";
	const string div_down		= "┴";

	const string meter = "■";

	const array<string, 10> superscript = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };

	const unordered_flat_map<string, vector<string>> graph_symbols = {
		{ "braille_up", {
			" ", "⢀", "⢠", "⢰", "⢸",
			"⡀", "⣀", "⣠", "⣰", "⣸",
			"⡄", "⣄", "⣤", "⣴", "⣼",
			"⡆", "⣆", "⣦", "⣶", "⣾",
			"⡇", "⣇", "⣧", "⣷", "⣿"
		}},
		{"braille_down", {
			" ", "⠈", "⠘", "⠸", "⢸",
			"⠁", "⠉", "⠙", "⠹", "⢹",
			"⠃", "⠋", "⠛", "⠻", "⢻",
			"⠇", "⠏", "⠟", "⠿", "⢿",
			"⡇", "⡏", "⡟", "⡿", "⣿"
		}},
		{"block_up", {
			" ", "▗", "▗", "▐", "▐",
			"▖", "▄", "▄", "▟", "▟",
			"▖", "▄", "▄", "▟", "▟",
			"▌", "▙", "▙", "█", "█",
			"▌", "▙", "▙", "█", "█"
		}},
		{"block_down", {
			" ", "▝", "▝", "▐", "▐",
			"▘", "▀", "▀", "▜", "▜",
			"▘", "▀", "▀", "▜", "▜",
			"▌", "▛", "▛", "█", "█",
			"▌", "▛", "▛", "█", "█"
		}},
		{"tty_up", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}},
		{"tty_down", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}}
	};

}

namespace Draw {

	string createBox(int x, int y, int width, int height, string line_color, bool fill, string title, string title2, int num) {
		string out;
		string lcolor = (line_color.empty()) ? Theme::c("div_line") : line_color;
		string numbering = (num == 0) ? "" : Theme::c("hi_fg") + (Config::getB("tty_mode") ? std::to_string(num) : Symbols::superscript[num]);

		out = Fx::reset + lcolor;

		//? Draw horizontal lines
		for (int hpos : {y, y + height - 1}) {
			out += Mv::to(hpos, x) + Symbols::h_line * (width - 1);
		}

		//? Draw vertical lines and fill if enabled
		for (int hpos : iota(y + 1, y + height - 1)) {
			out += Mv::to(hpos, x) + Symbols::v_line +
				((fill) ? string(width - 2, ' ') : Mv::r(width - 2)) +
				Symbols::v_line;
		}

		//? Draw corners
		out += 	Mv::to(y, x) + Symbols::left_up +
				Mv::to(y, x + width - 1) + Symbols::right_up +
				Mv::to(y + height - 1, x) + Symbols::left_down +
				Mv::to(y + height - 1, x + width - 1) + Symbols::right_down;

		//? Draw titles if defined
		if (not title.empty()) {
			out += Mv::to(y, x + 2) + Symbols::title_left + Fx::b + numbering + Theme::c("title") + title +
			Fx::ub + lcolor + Symbols::title_right;
		}
		if (not title2.empty()) {
			out += Mv::to(y + height - 1, x + 2) + Symbols::title_left + Theme::c("title") + title2 +
			Fx::ub + lcolor + Symbols::title_right;
		}

		return out + Fx::reset + Mv::to(y + 1, x + 1);
	}

	//* Meter class ------------------------------------------------------------------------------------------------------------>
	void Meter::operator()(int width, string color_gradient, bool invert) {
		this->width = width;
		this->color_gradient = color_gradient;
		this->invert = invert;
		cache.clear();
		cache.insert(cache.begin(), 101, "");
	}

	string Meter::operator()(int value) {
		if (width < 1) return "";
		value = clamp(value, 0, 100);
		if (not cache.at(value).empty()) return cache.at(value);
		string& out = cache.at(value);
		for (int i : iota(1, width + 1)) {
			int y = round((double)i * 100.0 / width);
			if (value >= y)
				out += Theme::g(color_gradient)[invert ? 100 - y : y] + Symbols::meter;
			else {
				out += Theme::c("meter_bg") + Symbols::meter * (width + 1 - i);
				break;
			}
		}
		out += Fx::reset;
		return out;
	}

	//* Graph class ------------------------------------------------------------------------------------------------------------>
	void Graph::_create(const deque<long long>& data, int data_offset) {
		const bool mult = (data.size() - data_offset > 1);
		if (mult and (data.size() - data_offset) % 2 != 0) data_offset--;
		auto& graph_symbol = Symbols::graph_symbols.at(symbol + '_' + (invert ? "down" : "up"));
		array<int, 2> result;
		const float mod = (height == 1) ? 0.3 : 0.1;
		long long data_value = 0;
		if (mult and data_offset > 0) {
			last = data[data_offset - 1];
			if (max_value > 0) last = clamp((last + offset) * 100 / max_value, 0ll, 100ll);
		}

		//? Horizontal iteration over values in <data>
		for (int i : iota(data_offset, (int)data.size())) {
			if (tty_mode and mult and i % 2 != 0) continue;
			else if (not tty_mode) current = not current;
			if (i == -1) { data_value = 0; last = 0; }
			else data_value = data[i];
			if (max_value > 0) data_value = clamp((data_value + offset) * 100 / max_value, 0ll, 100ll);
			//? Vertical iteration over height of graph
			for (int horizon : iota(0, height)) {
				int cur_high = (height > 1) ? round(100.0 * (height - horizon) / height) : 100;
				int cur_low = (height > 1) ? round(100.0 * (height - (horizon + 1)) / height) : 0;
				//? Calculate previous + current value to fit two values in 1 braille character
				int ai = 0;
				for (auto value : {last, data_value}) {
					if (value >= cur_high)
						result[ai++] = 4;
					else if (value <= cur_low)
						result[ai++] = 0;
					else {
						result[ai++] = round((float)(value - cur_low) * 4 / (cur_high - cur_low) + mod);
						if (no_zero and horizon == height - 1 and i != -1 and result[ai] == 0) result[ai] = 1;
					}
				}
				//? Generate graph symbol from 5x5 2D vector
				graphs[current][horizon] += (height == 1 and result[0] + result[1] == 0) ? Mv::r(1) : graph_symbol[(result[0] * 5 + result[1])];
			}
			if (mult and i > data_offset) last = data_value;
		}
		last = data_value;
		if (height == 1)
			out = (last < 1 ? Theme::c("inactive_fg") : Theme::g(color_gradient)[last]) + graphs[current][0];
		else {
			out.clear();
			for (int i : iota(0, height)) {
				if (i > 0) out += Mv::d(1) + Mv::l(width);
				out += (invert) ? Theme::g(color_gradient)[i * 100 / (height - 1)] : Theme::g(color_gradient)[100 - (i * 100 / (height - 1))];
				out += (invert) ? graphs[current][ (height - 1) - i] : graphs[current][i];
			}
		}
		out += Fx::reset;
	}

	void Graph::operator()(int width, int height, string color_gradient, const deque<long long>& data, string symbol, bool invert, bool no_zero, long long max_value, long long offset) {
		graphs[true].clear(); graphs[false].clear();
		this->width = width; this->height = height;
		this->invert = invert; this->offset = offset;
		this->no_zero = no_zero;
		this->color_gradient = color_gradient;
		if (Config::getB("tty_mode") or symbol == "tty") {
			tty_mode = true;
			this->symbol = "tty";
		}
		else if (symbol != "default") this->symbol = symbol;
		else this->symbol = Config::getS("graph_symbol");
		if (max_value == 0 and offset > 0) max_value = 100;
		this->max_value = max_value;
		int value_width = ceil((float)data.size() / 2);
		int data_offset = 0;
		if (value_width > width) data_offset = data.size() - width * 2;

		//? Populate the two switching graph vectors and fill empty space if data size < width
		for (int i : iota(0, height * 2)) {
			graphs[(i % 2 != 0)].push_back((value_width < width) ? ((height == 1) ? Mv::r(1) : " "s) * (width - value_width) : "");
		}
		if (data.size() == 0) return;
		this->_create(data, data_offset);
	}

	string& Graph::operator()(const deque<long long>& data, bool data_same) {
		if (data_same) return out;

		//? Make room for new characters on graph
		bool select_graph = (tty_mode ? current : not current);
		for (int i : iota(0, height)) {
			if (graphs[select_graph][i].starts_with(Fx::e)) graphs[current][i].erase(0, 4);
			else graphs[select_graph][i].erase(0, 3);
		}
		this->_create(data, (int)data.size() - 1);
		return out;
	}

	string& Graph::operator()() {
		return out;
	}
	//*------------------------------------------------------------------------------------------------------------------------->

}

namespace Cpu {
	int width_p = 100, height_p = 32;
	int min_w = 60, min_h = 8;
	int x = 1, y = 1, width, height;
	int b_columns, b_column_size;
	int b_x, b_y, b_width, b_height;
	bool shown = true, redraw = true;
	string box;

	string draw(const cpu_info& cpu, bool force_redraw) {
		(void)cpu;
		string out;
		if (redraw or force_redraw) {
			redraw = false;
			out += box;
		}
		return out;
	}

}

namespace Mem {
	int width_p = 45, height_p = 38;
	int min_w = 36, min_h = 10;
	int x = 1, y, width, height;
	int mem_width, disks_width, divider, item_height, mem_size, mem_meter, graph_height, disk_meter;
	bool shown = true, redraw = true;
	string box;

	string draw(const mem_info& mem, bool force_redraw) {
		(void)mem;
		string out;
		if (redraw or force_redraw) {
			redraw = false;
			out += box;
		}
		return out;
	}

}

namespace Net {
	int width_p = 45, height_p = 30;
	int min_w = 3, min_h = 6;
	int x = 1, y, width, height;
	int b_x, b_y, b_width, b_height, d_graph_height, u_graph_height;
	int graph_height;
	bool shown = true, redraw = true;
	string box;

	string draw(const net_info& net, bool force_redraw) {
		(void)net;
		string out;
		if (redraw or force_redraw) {
			redraw = false;
			out += box;
		}
		return out;
	}

}

namespace Proc {
	int width_p = 55, height_p = 68;
	int min_w = 44, min_h = 16;
	int x, y, width, height;
	int start, selected, select_max;
	bool shown = true, redraw = true;
	int selected_pid = 0;

	string box;

	void selection(const string& cmd_key) {
		auto start = Config::getI("proc_start");
		auto selected = Config::getI("proc_selected");
		int numpids = Proc::numpids;
		if (cmd_key == "up" and selected > 0) {
			if (start > 0 and selected == 1) start--;
			else selected--;
		}
		else if (cmd_key == "down") {
			if (start < numpids - select_max and selected == select_max) start++;
			else selected++;
		}
		else if (cmd_key == "page_up") {
			if (selected > 0 and start == 0) selected = 0;
			else start = max(0, start - (height - 3));
		}
		else if (cmd_key == "page_down") {
			if (selected > 0 and start >= numpids - select_max) selected = select_max;
			else start = clamp(start + select_max, 0, max(0, numpids - select_max));
		}
		else if (cmd_key == "home") {
			start = 0;
			if (selected > 0) selected = 1;
		}
		else if (cmd_key == "end") {
			start = max(0, numpids - select_max);
			if (selected > 0) selected = select_max;
		}

		Config::set("proc_start", start);
		Config::set("proc_selected", selected);
	}

	string draw(const vector<proc_info>& plist, bool force_redraw) {
		auto& filter = Config::getS("proc_filter");
		auto& filtering = Config::getB("proc_filtering");
		auto& proc_tree = Config::getB("proc_tree");
		bool show_detailed = (Config::getB("show_detailed") and Proc::detailed.last_pid == (size_t)Config::getI("detailed_pid"));
		bool proc_gradient = (Config::getB("proc_gradient") and not Config::getB("tty_mode"));
		auto& proc_colors = Config::getB("proc_colors");
		start = Config::getI("proc_start");
		selected = Config::getI("proc_selected");
		uint64_t total_mem = 16328872 << 10;
		int y = show_detailed ? Proc::y + 9 : Proc::y;
		int height = show_detailed ? Proc::height - 9 : Proc::height;
		int numpids = Proc::numpids;
		string out;
		//* If true, redraw elements not needed to be updated every cycle
		if (redraw or force_redraw) {
			redraw = false;
			out = box;
			out += Mv::to(y, x) + Mv::r(12)
				+ trans("Filter: " + filter + (filtering ? Fx::bl + "█"s + Fx::reset : " "))
				+ trans(rjust("Per core: " + (Config::getB("proc_per_core") ? "On "s : "Off"s) + "   Sorting: "
				+ string(Config::getS("proc_sorting")), width - 23 - ulen(filter)));

			if (not proc_tree)
				out += Mv::to(y+1, x+1) + Theme::c("title") + Fx::b
					+ rjust("Pid:", 8) + " "
					+ ljust("Program:", (width < 70 ? width - 45 : 16))
					+ (width >= 70 ? ljust("Command:", width - 61) : "")
					+ (width >= 77 ? Mv::l(4) + "Threads: " : " Tr: ")
					+ ljust("User:", 10) + " " + rjust("MemB", 5)
					+ " " + rjust("Cpu%", 10) + Fx::ub;
			else
				out += Mv::to(y+1, x+1) + Theme::c("title") + Fx::b + ljust("Tree:", width - 40)
					+ "Threads: " + ljust("User:", 10) + " " + rjust("MemB", 5)
					+ " " + rjust("Cpu%", 10) + Fx::ub;
		}

		//* Check bounds of current selection and view
		if (start > 0 and numpids <= select_max)
			start = 0;
		if (start > numpids - select_max)
			start = max(0, numpids - select_max);
		if (selected > select_max)
			selected = select_max;
		if (selected > numpids)
			selected = numpids;

		//* Iteration over processes
		int lc = 0;
		for (int n=0; auto& p : plist) {
			if (n++ < start) continue;
			bool is_selected = (lc + 1 == selected);
			if (is_selected) selected_pid = (int)p.pid;

			out += Fx::reset;

			//? Set correct gradient colors if enabled
			string c_color, m_color, t_color, g_color, end;
			if (is_selected) {
				c_color = m_color = t_color = g_color = Fx::b;
				end = Fx::ub;
				out += Theme::c("selected_bg") + Theme::c("selected_fg") + Fx::b;
			}
			else {
				int calc = (selected > lc) ? selected - lc : lc - selected;
				if (proc_colors) {
					end = Theme::c("main_fg") + Fx::ub;
					array<string, 3> colors;
					for (int i = 0; int v : {(int)round(p.cpu_p), (int)round(p.mem * 100 / total_mem), (int)p.threads / 3}) {
						if (proc_gradient) {
							int val = (min(v, 100) + 100) - calc * 100 / select_max;
							if (val < 100) colors[i++] = Theme::g("proc_color")[val];
							else colors[i++] = Theme::g("process")[val - 100];
						}
						else
							colors[i++] = Theme::g("process")[min(v, 100)];
					}
					c_color = colors[0]; m_color = colors[1]; t_color = colors[2];
				}
				else {
					c_color = m_color = t_color = Fx::b;
					end = Fx::ub;
				}
				if (proc_gradient) {
					g_color = Theme::g("proc")[calc * 100 / select_max];
				}
			}

			//? Normal view line
			if (not proc_tree) {
				out += Mv::to(y+2+lc, x+1)
					+ g_color + rjust(to_string(p.pid), 8) + ' '
					+ c_color + ljust(p.name, (width < 70 ? width - 46 : 15)) + end
					+ (width >= 70 ? g_color + ' ' + ljust(p.cmd, width - 62, true) : "");
			}
			//? Tree view line
			else {
				string prefix_pid = p.prefix + to_string(p.pid);
				int width_left = width - 38;
				out += Mv::to(y+2+lc, x+1) + g_color + uresize(prefix_pid, width_left) + ' ';
				width_left -= ulen(prefix_pid);
				if (width_left > 0) {
					out += c_color + uresize(p.name, width_left - 1) + end + ' ';
					width_left -= (ulen(p.name) + 1);
				}
				if (width_left > 7 and not p.cmd.empty()) {
					out += g_color + uresize(p.cmd, width_left - 1) + ' ';
					width_left -= (ulen(p.cmd) + 1);
				}
				out += string(max(0, width_left), ' ');
			}
			//? Common end of line
			string cpu_str = to_string(p.cpu_p);
			if (p.cpu_p < 10 or p.cpu_p >= 100) cpu_str.resize(3);
			out += t_color + rjust(to_string(p.threads), 5) + end + ' '
				+ g_color + ljust(p.user, 10) + ' '
				+ m_color + rjust(floating_humanizer(p.mem, true), 5) + end + ' '
				+ (is_selected ? "" : Theme::c("inactive_fg")) +  "⡀"s * 5 + ' ' + end
				+ c_color + rjust(cpu_str, 4) + ' ' + end;
			if (lc++ > height - 5) break;
		}

		out += Fx::reset;
		while (lc++ < height - 4) out += Mv::to(y+lc+2, x+1) + string(width - 3, ' ');

		//* Current selection and number of processes
		string location = to_string(start + selected) + '/' + to_string(numpids);
		string loc_clear = Symbols::h_line * max(0ul, 9 - location.size());
		out += Mv::to(y+height, x+width - 3 - max(9, (int)location.size())) + Theme::c("proc_box") + loc_clear
			+ Symbols::title_left + Theme::c("title") + Fx::b + location + Fx::ub + Theme::c("proc_box") + Symbols::title_right;

		if (selected == 0 and selected_pid != 0) selected_pid = 0;
		return out + Fx::reset;
	}

}

namespace Draw {
	void calcSizes() {
		auto& boxes = Config::getS("shown_boxes");

		Cpu::box.clear();
		Mem::box.clear();
		Net::box.clear();
		Proc::box.clear();

		Cpu::width = Mem::width = Net::width = Proc::width = 0;
		Cpu::height = Mem::height = Net::height = Proc::height = 0;
		Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true;

		Cpu::shown = s_contains(boxes, "cpu");
		Mem::shown = s_contains(boxes, "mem");
		Net::shown = s_contains(boxes, "net");
		Proc::shown = s_contains(boxes, "proc");

		//* Calculate and draw cpu box outlines
		if (Cpu::shown) {
			using namespace Cpu;
			width = round(Term::width * width_p / 100);
			height = max(8, (int)round(Term::height * (trim(boxes) == "cpu" ? 100 : height_p) / 100));

			b_columns = max(1, (int)ceil((Global::coreCount + 1) / (height - 5)));
			if (b_columns * (21 + 12 * got_sensors) < width - (width / 3)) {
				b_column_size = 2;
				b_width = (21 + 12 * got_sensors) * b_columns - (b_columns - 1);
			}
			else if (b_columns * (15 + 6 * got_sensors) < width - (width / 3)) {
				b_column_size = 1;
				b_width = (15 + 6 * got_sensors) * b_columns - (b_columns - 1);
			}
			else if (b_columns * (8 + 6 * got_sensors) < width - (width / 3)) {
				b_column_size = 0;
			}
			else {
				b_columns = (width - width / 3) / (8 + 6 * got_sensors);
				b_column_size = 0;
			}

			if (b_column_size == 0) b_width = (8 + 6 * got_sensors) * b_columns + 1;
			b_height = min(height - 2, (int)ceil(Global::coreCount / b_columns) + 4);

			b_x = width - b_width - 1;
			b_y = y + ceil((height - 2) / 2) - ceil(b_height / 2) + 1;

			box = createBox(x, y, width, height, Theme::c("cpu_box"), true, "cpu", "", 1);
			box += Mv::to(y, x + 10) + Theme::c("cpu_box") + Symbols::title_left + Fx::b + Theme::c("hi_fg")
				+ 'M' + Theme::c("title") + "enu" + Fx::ub + Theme::c("cpu_box") + Symbols::title_right;

			auto& custom = Config::getS("custom_cpu_name");
			box += createBox(b_x, b_y, b_width, b_height, "", false, uresize((custom.empty() ? cpuName : custom) , b_width - 14));
		}

		//* Calculate and draw mem box outlines
		if (Mem::shown) {
			using namespace Mem;
			auto& show_disks = Config::getB("show_disks");
			auto& swap_disk = Config::getB("swap_disk");
			auto& mem_graphs = Config::getB("mem_graphs");

			width = round(Term::width * (Proc::shown ? width_p : 100) / 100);
			height = round(Term::height * (100 - Cpu::height_p * Cpu::shown - Net::height_p * Net::shown) / 100) + 1;
			if (height + Cpu::height > Term::height) height = Term::height - Cpu::height;
			y = Cpu::height + 1;

			if (show_disks) {
				mem_width = ceil((width - 3) / 2);
				disks_width = width - mem_width - 3;
				mem_width += mem_width % 2;
				divider = x + mem_width;
			}
			else
				mem_width = width - 1;

			item_height = has_swap and not swap_disk ? 6 : 4;
			if (height - (has_swap and not swap_disk ? 3 : 2) > 2 * item_height)
				mem_size = 3;
			else if (mem_width > 25)
				mem_size = 2;
			else
				mem_size = 1;

			mem_meter = max(0, width - (disks_width * show_disks) - (mem_size > 2 ? 9 : 20));
			if (mem_size == 1) mem_meter += 6;

			if (mem_graphs) {
				graph_height = max(1, (int)round(((height - (has_swap and not swap_disk ? 2 : 1)) - (mem_size == 3 ? 2 : 1) * item_height) / item_height));
				if (graph_height > 1) mem_meter += 6;
			}
			else
				graph_height = 0;

			if (show_disks) {
				disk_meter = max(0, width - mem_width - 23);
				if (disks_width < 25) disk_meter += 10;
			}

			box = createBox(x, y, width, height, Theme::c("mem_box"), true, "mem", "", 2);
			box += Mv::to(y, (show_disks ? divider + 2 : x + width - 9)) + Theme::c("mem_box") + Symbols::title_left + (show_disks ? Fx::b : "")
				+ Theme::c("hi_fg") + 'd' + Theme::c("title") + "isks" + Fx::ub + Theme::c("mem_box") + Symbols::title_right;
			if (show_disks) {
				box += Mv::to(y, divider) + Symbols::div_up + Mv::to(y + height - 1, divider) + Symbols::div_down + Theme::c("div_line");
				for (auto i : iota(1, height - 1))
					box += Mv::to(y + i, divider) + Symbols::v_line;
			}
		}

		//* Calculate and draw net box outlines
		if (Net::shown) {
			using namespace Net;
			width = round(Term::width * (Proc::shown ? width_p : 100) / 100);
			height = Term::height - Cpu::height - Mem::height;
			y = Term::height - height + 1;
			b_width = (width > 45) ? 27 : 19;
			b_height = (height > 10) ? 9 : height - 2;
			b_x = width - b_width - 1;
			b_y = y + ((height - 2) / 2) - b_height / 2 + 1;
			d_graph_height = round((height - 2) / 2);
			u_graph_height = height - 2 - d_graph_height;

			box = createBox(x, y, width, height, Theme::c("net_box"), true, "net", "", 3);
			box += createBox(b_x, b_y, b_width, b_height, "", false, "download", "upload");
		}

		//* Calculate and draw proc box outlines
		if (Proc::shown) {
			using namespace Proc;
			width = Term::width - (Mem::shown ? Mem::width : (Net::shown ? Net::width : 0));
			height = Term::height - Cpu::height;
			x = Term::width - width + 1;
			y = Cpu::height + 1;
			select_max = height - 3;
			box = createBox(x, y, width, height, Theme::c("proc_box"), true, "proc", "", 4);
		}

	}
}