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
#include <ranges>

#include <btop_draw.hpp>
#include <btop_config.hpp>
#include <btop_theme.hpp>
#include <btop_shared.hpp>
#include <btop_tools.hpp>
#include <btop_input.hpp>


using 	std::round, std::views::iota, std::string_literals::operator""s, std::clamp, std::array, std::floor, std::max, std::min,
		std::to_string, std::cmp_equal, std::cmp_less, std::cmp_greater, std::cmp_less_equal;

using namespace Tools;
namespace rng = std::ranges;

namespace Symbols {
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

	TextEdit::TextEdit() {}
	TextEdit::TextEdit(string text) : text(text) {
		pos = this->text.size();
		upos = ulen(this->text);
	}

	bool TextEdit::command(const string& key) {
		if (key == "left" and upos > 0) {
			upos--;
			pos = uresize(text, upos).size();
		}
		else if (key == "right" and pos < text.size()) {
			upos++;
			pos = uresize(text, upos).size();
		}
		else if (key == "home" and pos > 0) {
			pos = upos = 0;
		}
		else if (key == "end" and pos < text.size()) {
			pos = text.size();
			upos = ulen(text);
		}
		else if (key == "backspace" and pos > 0) {
			if (pos == text.size()) {
				text = uresize(text, --upos);
				pos = text.size();
			}
			else {
				const string first = uresize(text, --upos);
				pos = first.size();
				text = first + luresize(text.substr(pos), ulen(text) - upos - 1);
			}
		}
		else if (key == "delete" and pos < text.size()) {
			const string first = uresize(text, upos + 1);
			text = uresize(first, ulen(first) - 1) + text.substr(first.size());
		}
		else if (key == "space") {
			text.insert(pos++, 1, ' ');
			upos++;
		}
		else if (ulen(key) == 1) {
			if (key.size() == 1) {
				text.insert(pos++, 1, key.at(0));
				upos++;
			}
			else {
				const string first = uresize(text, upos) + key;
				text = first + text.substr(pos);
				upos++;
				pos = first.size();
			}
		}
		else
			return false;

		return true;
	}

	string TextEdit::operator()(const size_t limit) {
		if (limit > 0 and ulen(text) + 1 > limit) {
			try {
				const size_t half = (size_t)round((double)limit / 2);
				string first;

				if (upos + half > ulen(text))
					first = luresize(text.substr(0, pos), limit - (ulen(text) - upos));
				else if (upos - half < 1)
					first = text.substr(0, pos);
				else
					first = luresize(text.substr(0, pos), half);

				return first + Fx::bl + "█" + Fx::ubl + uresize(text.substr(pos), limit - ulen(first));
			}
			catch (const std::exception& e) {
				Logger::error("In TextEdit::operator() : " + (string)e.what());
			}
		}
		return text.substr(0, pos) + Fx::bl + "█" + Fx::ubl + text.substr(pos);
	}

	string createBox(const int x, const int y, const int width, const int height, string line_color, const bool fill, const string title, const string title2, const int num) {
		string out;
		if (line_color.empty()) line_color = Theme::c("div_line");
		const auto& tty_mode = Config::getB("tty_mode");
		const auto& rounded = Config::getB("rounded_corners");
		const string numbering = (num == 0) ? "" : Theme::c("hi_fg") + (tty_mode ? std::to_string(num) : Symbols::superscript.at(clamp(num, 0, 9)));
		const auto& right_up = (tty_mode or not rounded ? Symbols::right_up : Symbols::round_right_up);
		const auto& left_up = (tty_mode or not rounded ? Symbols::left_up : Symbols::round_left_up);
		const auto& right_down = (tty_mode or not rounded ? Symbols::right_down : Symbols::round_right_down);
		const auto& left_down = (tty_mode or not rounded ? Symbols::left_down : Symbols::round_left_down);

		out = Fx::reset + line_color;

		//? Draw horizontal lines
		for (const int& hpos : {y, y + height - 1}) {
			out += Mv::to(hpos, x) + Symbols::h_line * (width - 1);
		}

		//? Draw vertical lines and fill if enabled
		for (const int& hpos : iota(y + 1, y + height - 1)) {
			out += Mv::to(hpos, x) + Symbols::v_line
				+  ((fill) ? string(width - 2, ' ') : Mv::r(width - 2))
				+  Symbols::v_line;
		}

		//? Draw corners
		out += 	Mv::to(y, x) + left_up
			+	Mv::to(y, x + width - 1) + right_up
			+	Mv::to(y + height - 1, x) +left_down
			+	Mv::to(y + height - 1, x + width - 1) + right_down;

		//? Draw titles if defined
		if (not title.empty()) {
			out += Mv::to(y, x + 2) + Symbols::title_left + Fx::b + numbering + Theme::c("title") + title
				+  Fx::ub + line_color + Symbols::title_right;
		}
		if (not title2.empty()) {
			out += Mv::to(y + height - 1, x + 2) + Symbols::title_left_down + Fx::b + numbering + Theme::c("title") + title2
				+  Fx::ub + line_color + Symbols::title_right_down;
		}

		return out + Fx::reset + Mv::to(y + 1, x + 1);
	}

	//* Meter class ------------------------------------------------------------------------------------------------------------>
	Meter::Meter() {}

	Meter::Meter(const int width, const string& color_gradient, const bool invert) : width(width), color_gradient(color_gradient), invert(invert) {
		cache.insert(cache.begin(), 101, "");
	}

	string Meter::operator()(int value) {
		if (width < 1) return "";
		value = clamp(value, 0, 100);
		if (not cache.at(value).empty()) return cache.at(value);
		string& out = cache.at(value);
		for (const int& i : iota(1, width + 1)) {
			int y = round((double)i * 100.0 / width);
			if (value >= y)
				out += Theme::g(color_gradient).at(invert ? 100 - y : y) + Symbols::meter;
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
		const auto& graph_symbol = Symbols::graph_symbols.at(symbol + '_' + (invert ? "down" : "up"));
		array<int, 2> result;
		const float mod = (height == 1) ? 0.3 : 0.1;
		long long data_value = 0;
		if (mult and data_offset > 0) {
			last = data.at(data_offset - 1);
			if (max_value > 0) last = clamp((last + offset) * 100 / max_value, 0ll, 100ll);
		}

		//? Horizontal iteration over values in <data>
		for (const int& i : iota(data_offset, (int)data.size())) {
			// if (tty_mode and mult and i % 2 != 0) continue;
			if (not tty_mode and mult) current = not current;
			if (i < 0) {
				data_value = 0;
				last = 0;
			}
			else {
				data_value = data.at(i);
				if (max_value > 0) data_value = clamp((data_value + offset) * 100 / max_value, 0ll, 100ll);
			}

			//? Vertical iteration over height of graph
			for (const int& horizon : iota(0, height)) {
				const int cur_high = (height > 1) ? round(100.0 * (height - horizon) / height) : 100;
				const int cur_low = (height > 1) ? round(100.0 * (height - (horizon + 1)) / height) : 0;
				//? Calculate previous + current value to fit two values in 1 braille character
				for (int ai = 0; const auto& value : {last, data_value}) {
					const int clamp_min = (no_zero and horizon == height - 1 and not (mult and i == data_offset and ai == 0)) ? 1 : 0;
					if (value >= cur_high)
						result[ai++] = 4;
					else if (value <= cur_low)
						result[ai++] = clamp_min;
					else {
						result[ai++] = clamp((int)round((float)(value - cur_low) * 4 / (cur_high - cur_low) + mod), clamp_min, 4);
					}
				}
				//? Generate graph symbol from 5x5 2D vector
				graphs.at(current).at(horizon) += (height == 1 and result.at(0) + result.at(1) == 0) ? Mv::r(1) : graph_symbol.at((result.at(0) * 5 + result.at(1)));
			}
			if (mult and i >= 0) last = data_value;
		}
		last = data_value;
		out.clear();
		if (height == 1) {
			if (not color_gradient.empty())
				out += (last < 1 and not color_gradient.empty() ? Theme::c("inactive_fg") : Theme::g(color_gradient).at(last));
			out += graphs.at(current).at(0);
		}
		else {
			for (const int& i : iota(0, height)) {
				if (i > 0) out += Mv::d(1) + Mv::l(width);
				if (not color_gradient.empty())
					out += (invert) ? Theme::g(color_gradient).at(i * 100 / (height - 1)) : Theme::g(color_gradient).at(100 - (i * 100 / (height - 1)));
				out += (invert) ? graphs.at(current).at((height - 1) - i) : graphs.at(current).at(i);
			}
		}
		if (not color_gradient.empty()) out += Fx::reset;
	}

	Graph::Graph() {}

	Graph::Graph(int width, int height, const string& color_gradient, const deque<long long>& data, const string& symbol, bool invert, bool no_zero, long long max_value, long long offset)
	: width(width), height(height), color_gradient(color_gradient), invert(invert), no_zero(no_zero), offset(offset) {
		if (Config::getB("tty_mode") or symbol == "tty") this->symbol = "tty";
		else if (symbol != "default") this->symbol = symbol;
		else this->symbol = Config::getS("graph_symbol");
		if (this->symbol == "tty") tty_mode = true;

		if (max_value == 0 and offset > 0) max_value = 100;
		this->max_value = max_value;
		const int value_width = (tty_mode ? data.size() : ceil((double)data.size() / 2));
		int data_offset = (value_width > width) ? data.size() - width * (tty_mode ? 1 : 2) : 0;

		if (not tty_mode and (data.size() - data_offset) % 2 != 0) {
			data_offset--;
		}

		//? Populate the two switching graph vectors and fill empty space if data size < width
		for (const int& i : iota(0, height * 2)) {
			if (tty_mode and i % 2 != current) continue;
			graphs[(i % 2 != 0)].push_back((value_width < width) ? ((height == 1) ? Mv::r(1) : " "s) * (width - value_width) : "");
		}
		if (data.size() == 0) return;
		this->_create(data, data_offset);
	}

	string& Graph::operator()(const deque<long long>& data, const bool data_same) {
		if (data_same) return out;

		//? Make room for new characters on graph
		if (not tty_mode) current = not current;
		for (const int& i : iota(0, height)) {
			if (graphs.at(current).at(i).at(1) == '[') graphs.at(current).at(i).erase(0, 4);
			else graphs.at(current).at(i).erase(0, 3);
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
	int min_width = 60, min_height = 8;
	int x = 1, y = 1, width = 20, height;
	int b_columns, b_column_size;
	int b_x, b_y, b_width, b_height;
	int graph_up_height;
	bool shown = true, redraw = true, mid_line = false;
	string box;
	Draw::Graph graph_upper;
	Draw::Graph graph_lower;
	Draw::Meter cpu_meter;
	vector<Draw::Graph> core_graphs;
	vector<Draw::Graph> temp_graphs;

	string draw(const cpu_info& cpu, const bool force_redraw, const bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		const bool show_temps = (Config::getB("check_temp") and got_sensors);
		auto& single_graph = Config::getB("cpu_single_graph");
		const bool hide_cores = show_temps and (cpu_temp_only or not Config::getB("show_coretemp"));
		const int extra_width = (hide_cores ? max(6, 6 * b_column_size) : 0);
		auto& graph_up_field = Config::getS("cpu_graph_upper");
		auto& graph_lo_field = Config::getS("cpu_graph_lower");
		auto& tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_cpu"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(1);
		auto& temp_scale = Config::getS("temp_scale");
		string out;
		out.reserve(width * height);
		//* Redraw elements not needed to be updated every cycle
		if (redraw or force_redraw) {
			auto& cpu_bottom = Config::getB("cpu_bottom");
			mid_line = (not single_graph and graph_up_field != graph_lo_field);
			graph_up_height = (single_graph ? height - 2 : ceil((double)(height - 2) / 2) - (mid_line and height % 2 != 0 ? 1 : 0));
			const int graph_low_height = height - 2 - graph_up_height - (mid_line ? 1 : 0);
			const int button_y = cpu_bottom ? y + height - 1 : y;
			out += box;
			const string title_left = Theme::c("cpu_box") + (cpu_bottom ? Symbols::title_left_down : Symbols::title_left);
			const string title_right = Theme::c("cpu_box") + (cpu_bottom ? Symbols::title_right_down : Symbols::title_right);

			//? Buttons on title
			out += Mv::to(button_y, x + 10) + title_left + Theme::c("hi_fg") + Fx::b + 'm' + Theme::c("title") + "enu" + Fx::ub + title_right;
			Input::mouse_mappings["m"] = {button_y, x + 11, 1, 4};
			const string update = to_string(Config::getI("update_ms")) + "ms";
			out += Mv::to(button_y, x + width - update.size() - 8) + title_left + Fx::b + Theme::c("hi_fg") + "- " + Theme::c("title") + update
				+ Theme::c("hi_fg") + " +" + Fx::ub + title_right;
			Input::mouse_mappings["-"] = {button_y, x + width - (int)update.size() - 7, 1, 2};
			Input::mouse_mappings["+"] = {button_y, x + width - 5, 1, 2};

			//? Graphs & meters
			graph_upper = Draw::Graph{x + width - b_width - 3, graph_up_height, "cpu", cpu.cpu_percent.at(graph_up_field), graph_symbol, false, true};
			cpu_meter = Draw::Meter{b_width - (show_temps ? 23 - (b_column_size <= 1 and b_columns == 1 ? 6 : 0) : 11), "cpu"};
			if (not single_graph)
				graph_lower = Draw::Graph{x + width - b_width - 3, graph_low_height, "cpu", cpu.cpu_percent.at(graph_lo_field), graph_symbol, Config::getB("cpu_invert_lower"), true};
			if (mid_line) {
				out += Mv::to(y + graph_up_height + 1, x) + Fx::ub + Theme::c("cpu_box") + Symbols::div_left + Theme::c("div_line")
					+ Symbols::h_line * (width - b_width - 2) + Symbols::div_right
					+ Mv::to(y + graph_up_height + 1, x + ((width - b_width) / 2) - ((graph_up_field.size() + graph_lo_field.size()) / 2) - 4)
					+ Theme::c("main_fg") + graph_up_field + Mv::r(1) + "▲▼" + Mv::r(1) + graph_lo_field;
			}
			if (b_column_size > 0 or extra_width > 0) {
				core_graphs.clear();
				for (const auto& core_data : cpu.core_percent) {
					core_graphs.emplace_back(5 * b_column_size + extra_width, 1, "", core_data, graph_symbol);
				}
			}
			if (show_temps) {
				temp_graphs.clear();
				temp_graphs.emplace_back(5, 1, "", cpu.temp.at(0), graph_symbol, false, false, cpu.temp_max, -23);
				if (not hide_cores and b_column_size > 1) {
					for (const auto& i : iota((size_t)1, cpu.temp.size())) {
						temp_graphs.emplace_back(5, 1, "", cpu.temp.at(i), graph_symbol, false, false, cpu.temp_max, -23);
					}
				}
			}

		}
		try {
		//? Cpu graphs
		out += Fx::ub + Mv::to(y + 1, x + 1) + graph_upper(cpu.cpu_percent.at(graph_up_field), (data_same or redraw));
		if (not single_graph)
			out += Mv::to( y + graph_up_height + 1 + (mid_line ? 1 : 0), x + 1) + graph_lower(cpu.cpu_percent.at(graph_lo_field), (data_same or redraw));

		//? Uptime
		if (Config::getB("show_uptime")) {
			string upstr = sec_to_dhms(system_uptime());
			if (upstr.size() > 8) {
				upstr.resize(upstr.size() - 3);
				upstr = trans(upstr);
			}
			out += Mv::to(y + (single_graph or not Config::getB("cpu_invert_lower") ? 1 : height - 2), x + 2)
				+ Theme::c("graph_text") + "up" + Mv::r(1) + upstr;
		}

		//? Cpu clock and cpu meter
		if (Config::getB("show_cpu_freq") and not cpuHz.empty())
			out += Mv::to(b_y, b_x + b_width - 10) + Fx::ub + Theme::c("div_line") + Symbols::h_line * (7 - cpuHz.size())
				+ Symbols::title_left + Fx::b + Theme::c("title") + cpuHz + Fx::ub + Theme::c("div_line") + Symbols::title_right;

		out += Mv::to(b_y + 1, b_x + 1) + Theme::c("main_fg") + Fx::b + "CPU " + cpu_meter(cpu.cpu_percent.at("total").back())
			+ Theme::g("cpu").at(cpu.cpu_percent.at("total").back()) + rjust(to_string(cpu.cpu_percent.at("total").back()), 4) + Theme::c("main_fg") + '%';
		if (show_temps) {
			const auto [temp, unit] = celsius_to(cpu.temp.at(0).back(), temp_scale);
			const auto& temp_color = Theme::g("temp").at(clamp(cpu.temp.at(0).back() * 100 / cpu.temp_max, 0ll, 100ll));
			if (b_column_size > 1 or b_columns > 1)
				out += ' ' + Theme::c("inactive_fg") + graph_bg * 5 + Mv::l(5) + temp_color
					+ temp_graphs.at(0)(cpu.temp.at(0), data_same or redraw);
			out += rjust(to_string(temp), 4) + Theme::c("main_fg") + unit;
		}
		out += Theme::c("div_line") + Symbols::v_line;

		} catch (const std::exception& e) { throw std::runtime_error("graphs, clock, meter : " + (string)e.what()); }

		//? Core text and graphs
		int cx = 0, cy = 1, cc = 0;
		for (const auto& n : iota(0, Shared::coreCount)) {
			out += Mv::to(b_y + cy + 1, b_x + cx + 1) + Theme::c("main_fg") + (Shared::coreCount < 100 ? Fx::b + 'C' + Fx::ub : "")
				+ ljust(to_string(n), (b_column_size == 0 ? 2 : 3));
			if (b_column_size > 0 or extra_width > 0)
				out += Theme::c("inactive_fg") + graph_bg * (5 * b_column_size + extra_width) + Mv::l(5 * b_column_size + extra_width)
					+ Theme::g("cpu").at(cpu.core_percent.at(n).back()) + core_graphs.at(n)(cpu.core_percent.at(n), data_same or redraw);
			else
				out += Theme::g("cpu").at(cpu.core_percent.at(n).back());
			out += rjust(to_string(cpu.core_percent.at(n).back()), (b_column_size < 2 ? 3 : 4)) + Theme::c("main_fg") + '%';

			if (show_temps and not hide_cores) {
				const auto [temp, unit] = celsius_to(cpu.temp.at(n+1).back(), temp_scale);
				const auto& temp_color = Theme::g("temp").at(clamp(cpu.temp.at(n+1).back() * 100 / cpu.temp_max, 0ll, 100ll));
				if (b_column_size > 1)
					out += ' ' + Theme::c("inactive_fg") + graph_bg * 5 + Mv::l(5) + temp_color
						+ temp_graphs.at(n+1)(cpu.temp.at(n+1), data_same or redraw);
				out += temp_color + rjust(to_string(temp), 4) + Theme::c("main_fg") + unit;
			}

			out += Theme::c("div_line") + Symbols::v_line;

			if (++cy > ceil((double)Shared::coreCount / b_columns) and n != Shared::coreCount - 1) {
				if (++cc >= b_columns) break;
				cy = 1; cx = (b_width / b_columns) * cc;
			}
		}

		//? Load average
		if (cy < b_height - 1 and cc <= b_columns) {
			string lavg_pre;
			int sep = 1;
			if (b_column_size == 2 and got_sensors) { lavg_pre = "Load AVG:"; sep = 3; }
			else if (b_column_size == 2 or (b_column_size == 1 and got_sensors)) { lavg_pre = "LAV:"; }
			else if (b_column_size == 1 or (b_column_size == 0 and got_sensors)) { lavg_pre = "L"; }
			string lavg;
			for (const auto& val : cpu.load_avg) {
				lavg += string(sep, ' ') + (lavg_pre.size() < 3 ? to_string((int)round(val)) : to_string(val).substr(0, 4));
			}
			out += Mv::to(b_y + b_height - 2, b_x + cx + 1) + Theme::c("main_fg") + lavg_pre + lavg;
		}



		redraw = false;
		return out + Fx::reset;
	}

}

namespace Mem {
	int width_p = 45, height_p = 38;
	int min_width = 36, min_height = 10;
	int x = 1, y, width = 20, height;
	int mem_width, disks_width, divider, item_height, mem_size, mem_meter, graph_height, disk_meter;
	int disks_io_h = 0;
	bool shown = true, redraw = true;
	string box;
	unordered_flat_map<string, Draw::Meter> mem_meters;
	unordered_flat_map<string, Draw::Graph> mem_graphs;
	unordered_flat_map<string, Draw::Meter> disk_meters_used;
	unordered_flat_map<string, Draw::Meter> disk_meters_free;
	unordered_flat_map<string, Draw::Graph> io_graphs;

	string draw(const mem_info& mem, const bool force_redraw, const bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		auto& show_swap = Config::getB("show_swap");
		auto& swap_disk = Config::getB("swap_disk");
		auto& show_disks = Config::getB("show_disks");
		auto& show_io_stat = Config::getB("show_io_stat");
		auto& io_mode = Config::getB("io_mode");
		auto& io_graph_combined = Config::getB("io_graph_combined");
		auto& use_graphs = Config::getB("mem_graphs");
		auto& tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_mem"));
		// auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(1);
		string out;
		out.reserve(height * width);

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			out += box;
			mem_meters.clear();
			mem_graphs.clear();
			disk_meters_free.clear();
			disk_meters_used.clear();
			io_graphs.clear();

			//? Mem graphs and meters
			for (const auto& name : mem_names) {

				if (use_graphs)
					mem_graphs[name] = Draw::Graph{mem_meter, graph_height, name, mem.percent.at(name), graph_symbol};
				else
					mem_meters[name] = Draw::Meter{mem_meter, name};
			}
			if (show_swap and has_swap) {
				for (const auto& name : swap_names) {
					if (use_graphs)
						mem_graphs[name] = Draw::Graph{mem_meter, graph_height, name.substr(5), mem.percent.at(name), graph_symbol};
					else
						mem_meters[name] = Draw::Meter{mem_meter, name.substr(5)};
				}
			}

			//? Disk meters and io graphs
			if (show_disks) {
				if (show_io_stat or io_mode) {
					if (io_mode)
						disks_io_h = max((int)floor((double)(height - 2 - disk_ios) / max(1, disk_ios)), (io_graph_combined ? 1 : 2));
					else
						disks_io_h = 1;
					int half_height = ceil((double)disks_io_h / 2);

					unordered_flat_map<string, int> custom_speeds;
					if (not Config::getS("io_graph_speeds").empty()) {
						auto split = ssplit(Config::getS("io_graph_speeds"));
						for (const auto& entry : split) {
							auto vals = ssplit(entry);
							if (vals.size() == 2 and mem.disks.contains(vals.at(0)) and isint(vals.at(1)))
								custom_speeds[vals.at(0)] = std::stoi(vals.at(1));
						}
					}
					for (const auto& [name, disk] : mem.disks) {
						if (disk.io_read.empty()) continue;
						long long speed = (custom_speeds.contains(name) ? custom_speeds.at(name) : 10) << 20;

						//? Create one combined graph for IO read/write if enabled
						if (not io_mode or (io_mode and io_graph_combined)) {
							deque<long long> combined(disk.io_read.size(), 0);
							rng::transform(disk.io_read, disk.io_write, combined.begin(), std::plus<long long>());
							io_graphs[name] = Draw::Graph{disks_width - (io_mode ? 0 : 6), disks_io_h, "available", combined, graph_symbol, false, true, speed};
						}
						else {
							io_graphs[name + "_read"] = Draw::Graph{disks_width, half_height, "free", disk.io_read, graph_symbol, false, true, speed};
							io_graphs[name + "_write"] = Draw::Graph{disks_width, disks_io_h - half_height, "used", disk.io_write, graph_symbol, true, true, speed};
						}
					}
				}
				if (disk_meter > 0) {
					for (int i = 0; const auto& name : mem.disks_order) {
						if (i * 2 > height - 2) break;
						disk_meters_used[name] = Draw::Meter{disk_meter, "used"};
						if (cmp_less_equal(mem.disks_order.size() * 3, height - 1))
							disk_meters_free[name] = Draw::Meter{disk_meter, "free"};
					}
				}
			}
			out += Mv::to(y, x + width - 6) + Fx::ub + Theme::c("mem_box") + Symbols::title_left + (io_mode ? Fx::b : "") + Theme::c("hi_fg")
				+ 'i' + Theme::c("title") + 'o' + Fx::ub + Theme::c("mem_box") + Symbols::title_right;
			Input::mouse_mappings["i"] = {y, x + width - 5, 1, 2};
		}

		//? Mem and swap
		int cx = 1, cy = 1;
		string divider = (graph_height > 0 ? Mv::l(2) + Theme::c("mem_box") + Symbols::div_left + Theme::c("div_line") + Symbols::h_line * (mem_width - 1)
						+ (show_disks ? "" : Theme::c("mem_box")) + Symbols::div_right + Mv::l(mem_width - 1) + Theme::c("main_fg") : "");
		string up = (graph_height >= 2 ? Mv::l(mem_width - 2) + Mv::u(graph_height - 1) : "");
		bool big_mem = mem_width > 21;

		out += Mv::to(y + 1, x + 2) + Theme::c("title") + Fx::b + "Total:" + rjust(floating_humanizer(Shared::totalMem), mem_width - 9) + Fx::ub + Theme::c("main_fg");
		vector<string> comb_names (mem_names.begin(), mem_names.end());
		if (show_swap and has_swap and not swap_disk) comb_names.insert(comb_names.end(), swap_names.begin(), swap_names.end());
		for (auto name : comb_names) {
			if (cy > height - 4) break;
			string title;
			if (name == "swap_used") {
				if (cy > height - 5) break;
				if (height - cy > 6) {
					if (graph_height > 0) out += Mv::to(y+1+cy, x+1+cx) + divider;
					cy += 1;
				}
				out += Mv::to(y+1+cy, x+1+cx) + Theme::c("title") + Fx::b + "Swap:" + rjust(floating_humanizer(mem.stats.at("swap_total")), mem_width - 8)
					+ Theme::c("main_fg") + Fx::ub;
				cy += 1;
				title = "Used";
			}
			else if (name == "swap_free")
				title = "Free";

			if (title.empty()) title = capitalize(name);
			const string humanized = floating_humanizer(mem.stats.at(name));
			const string graphics = (use_graphs ? mem_graphs.at(name)(mem.percent.at(name), redraw or data_same) : mem_meters.at(name)(mem.percent.at(name).back()));
			if (mem_size > 2) {
				out += Mv::to(y+1+cy, x+1+cx) + divider + ljust(title, 4, false, false, not big_mem) + ljust(":", (big_mem ? 1 : 6))
					+ Mv::to(y+1+cy, x+cx + mem_width - 2 - humanized.size()) + trans(humanized)
					+ Mv::to(y+2+cy, x+cx + (graph_height >= 2 ? 0 : 1)) + graphics + up + rjust(to_string(mem.percent.at(name).back()) + "%", 4);
				cy += (graph_height == 0 ? 2 : graph_height + 1);
			}
			else {
				out += Mv::to(y+1+cy, x+1+cx) + ljust(title, (mem_size > 1 ? 5 : 1)) + (graph_height >= 2 ? "" : " ")
					+ graphics + Theme::c("title") + rjust(humanized, (mem_size > 1 ? 9 : 7));
				cy += (graph_height == 0 ? 1 : graph_height);
			}
		}
		if (graph_height > 0 and cy < height - 2)
			out += Mv::to(y+1+cy, x+1+cx) + divider;

		//? Disks
		if (show_disks) {
			const auto& disks = mem.disks;
			cx = x + mem_width - 1; cy = 0;
			const bool big_disk = disks_width >= 25;
			divider = Mv::l(1) + Theme::c("div_line") + Symbols::div_left + Symbols::h_line * disks_width + Theme::c("mem_box") + Fx::ub + Symbols::div_right + Mv::l(disks_width - 1);
			if (io_mode) {
				for (const auto& mount : mem.disks_order) {
					if (not disks.contains(mount)) continue;
					if (cy > height - 3) break;
					const auto& disk = disks.at(mount);
					if (disk.io_read.empty()) continue;
					const string total = floating_humanizer(disk.total, not big_disk);
					out += Mv::to(y+1+cy, x+1+cx) + divider + Theme::c("title") + Fx::b + uresize(disk.name, disks_width - 2) + Mv::to(y+1+cy, x+cx + disks_width - total.size() - 1)
						+ trans(total) + Fx::ub;
					if (big_disk) {
						const string used_percent = to_string(disk.used_percent);
						out += Mv::to(y+1+cy, x+1+cx + round((double)disks_width / 2) - round((double)used_percent.size() / 2)) + Theme::c("main_fg") + used_percent + '%';
					}
					if (++cy > height - 3) break;
					if (io_graph_combined) {
						auto comb_val = disk.io_read.back() + disk.io_write.back();
						const string humanized = (disk.io_write.back() > 0 ? "▼"s : ""s) + (disk.io_read.back() > 0 ? "▲"s : ""s)
												+ (comb_val > 0 ? Mv::r(1) + floating_humanizer(comb_val, true) : "RW");
						if (disks_io_h == 1) out += Mv::to(y+1+cy, x+1+cx) + string(5, ' ');
						out += Mv::to(y+1+cy, x+1+cx) + io_graphs.at(mount)({comb_val}, redraw or data_same)
							+ Mv::to(y+1+cy, x+1+cx) + Theme::c("main_fg") + humanized;
						cy += disks_io_h;
					}
					else {
						const string human_read = (disk.io_read.back() > 0 ? "▲" + floating_humanizer(disk.io_read.back(), true) : "R");
						const string human_write = (disk.io_write.back() > 0 ? "▼" + floating_humanizer(disk.io_write.back(), true) : "W");
						if (disks_io_h <= 3) out += Mv::to(y+1+cy, x+1+cx) + string(5, ' ') + Mv::to(y+cy + disks_io_h, x+1+cx) + string(5, ' ');
						out += Mv::to(y+1+cy, x+1+cx) + io_graphs.at(mount + "_read")(disk.io_read, redraw or data_same) + Mv::l(disks_width)
							+ Mv::d(1) + io_graphs.at(mount + "_write")(disk.io_write, redraw or data_same)
							+ Mv::to(y+1+cy, x+1+cx) + human_read + Mv::to(y+cy + disks_io_h, x+1+cx) + human_write;
						cy += disks_io_h;
					}
				}
			}
			else {
				for (const auto& mount : mem.disks_order) {
					if (not disks.contains(mount)) continue;
					if (cy > height - 3) break;
					const auto& disk = disks.at(mount);
					auto comb_val = (not disk.io_read.empty() ? disk.io_read.back() + disk.io_write.back() : 0ll);
					const string human_io = (comb_val > 0 and big_disk ? (disk.io_write.back() > 0 ? "▼"s : ""s) + (disk.io_read.back() > 0 ? "▲"s : ""s)
											+ floating_humanizer(comb_val, true) : "");
					const string human_total = floating_humanizer(disk.total, not big_disk);
					const string human_used = floating_humanizer(disk.used, not big_disk);
					const string human_free = floating_humanizer(disk.free, not big_disk);

					out += Mv::to(y+1+cy, x+1+cx) + divider + Theme::c("title") + Fx::b + uresize(disk.name, disks_width - 2) + Mv::to(y+1+cy, x+cx + disks_width - human_total.size() - 1)
						+ trans(human_total) + Fx::ub + Theme::c("main_fg");
					if (big_disk and not human_io.empty())
						out += Mv::to(y+1+cy, x+1+cx + round((double)disks_width / 2) - round((double)human_io.size() / 2)) + Theme::c("main_fg") + human_io;
					if (++cy > height - 3) break;
					if (show_io_stat and io_graphs.contains(mount)) {
						out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " IO: " : " IO   " + Mv::l(2)) + io_graphs.at(mount)({comb_val}, redraw or data_same);
						if (not big_disk) out += Mv::to(y+1+cy, x+cx) + Theme::c("main_fg") + human_io;
						if (++cy > height - 3) break;
					}

					out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " Used:" + rjust(to_string(disk.used_percent) + '%', 4) : "U") + ' '
						+ disk_meters_used.at(mount)(disk.used_percent) + rjust(human_used, (big_disk ? 9 : 5));
					if (++cy > height - 3) break;

					if (cmp_less_equal(disks.size() * 3 + (show_io_stat ? disk_ios : 0), height - 1)) {
						out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " Free:" + rjust(to_string(disk.free_percent) + '%', 4) : "F") + ' '
						+ disk_meters_free.at(mount)(disk.free_percent) + rjust(human_free, (big_disk ? 9 : 5));
						cy++;
						if (cmp_less_equal(disks.size() * 4 + (show_io_stat ? disk_ios : 0), height - 1)) cy++;
					}

				}
			}
			if (cy < height - 2) out += Mv::to(y+1+cy, x+1+cx) + divider;
		}



		redraw = false;
		return out + Fx::reset;
	}

}

namespace Net {
	int width_p = 45, height_p = 30;
	int min_width = 36, min_height = 6;
	int x = 1, y, width = 20, height;
	int b_x, b_y, b_width, b_height, d_graph_height, u_graph_height;
	bool shown = true, redraw = true;
	string old_ip;
	unordered_flat_map<string, Draw::Graph> graphs;
	string box;

	string draw(const net_info& net, const bool force_redraw, const bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		auto& net_sync = Config::getB("net_sync");
		auto& net_auto = Config::getB("net_auto");
		auto& tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_net"));
		string ip_addr = (net.ipv4.empty() ? net.ipv6 : net.ipv4);
		if (old_ip != ip_addr) {
			old_ip = ip_addr;
			redraw = true;
		}
		string out;
		out.reserve(width * height);
		const string title_left = Theme::c("net_box") + Fx::ub + Symbols::title_left;
		const string title_right = Theme::c("net_box") + Fx::ub + Symbols::title_right;
		const int i_size = min((int)selected_iface.size(), 10);
		const long long down_max = (net_auto ? graph_max.at("download") : (long long)(Config::getI("net_download") << 20) / 8);
		const long long up_max = (net_auto ? graph_max.at("upload") : (long long)(Config::getI("net_upload") << 20) / 8);

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			out = box;
			//? Graphs
			graphs.clear();
			graphs["download"] = Draw::Graph{width - b_width - 2, u_graph_height, "download", net.bandwidth.at("download"), graph_symbol, false, true, down_max};
			graphs["upload"] = Draw::Graph{width - b_width - 2, d_graph_height, "upload", net.bandwidth.at("upload"), graph_symbol, true, true, up_max};

			//? Interface selector and buttons

			out += Mv::to(y, x+width - i_size - 10) + title_left + Fx::b + Theme::c("hi_fg") + "<b " + Theme::c("title")
				+ uresize(selected_iface, 10) + Theme::c("hi_fg") + " n>" + title_right
				+ Mv::to(y, x+width - i_size - 16) + title_left + Theme::c("hi_fg") + (net.stat.at("download").offset + net.stat.at("upload").offset > 0 ? Fx::b : "") + 'z'
				+ Theme::c("title") + "ero" + title_right;
			Input::mouse_mappings["b"] = {y, x+width - i_size - 9, 1, 3};
			Input::mouse_mappings["n"] = {y, x+width - 6, 1, 3};
			Input::mouse_mappings["z"] = {y, x+width - i_size - 15, 1, 4};
			if (width - i_size - 20 > 6) {
				out += Mv::to(y, x+width - i_size - 21) + title_left + Theme::c("hi_fg") + (net_auto ? Fx::b : "") + 'a' + Theme::c("title") + "uto" + title_right;
				Input::mouse_mappings["a"] = {y, x+width - i_size - 20, 1, 4};
			}
			if (width - i_size - 20 > 13) {
				out += Mv::to(y, x+width - i_size - 27) + title_left + Theme::c("title") + (net_sync ? Fx::b : "") + 's' + Theme::c("hi_fg")
					+ 'y' + Theme::c("title") + "nc" + title_right;
				Input::mouse_mappings["y"] = {y, x+width - i_size - 26, 1, 4};
			}
		}

		//? IP or device address
		if (not ip_addr.empty() and width - i_size - 35 - ip_addr.size() > 0) {
			out += Mv::to(y, x + 8) + title_left + Theme::c("title") + Fx::b + ip_addr + title_right;
		}

		//? Graphs and stats
		int cy = 0;
		for (const string dir : {"download", "upload"}) {
			out += Mv::to(y+1 + (dir == "upload" ? u_graph_height : 0), x + 1) + graphs.at(dir)(net.bandwidth.at(dir), redraw or data_same or not net.connected)
				+ Mv::to(y+1 + (dir == "upload" ? height - 3: 0), x + 1) + Fx::ub + Theme::c("graph_text")
				+ floating_humanizer((dir == "upload" ? up_max : down_max), true);
			const string speed = floating_humanizer(net.stat.at(dir).speed, false, 0, false, true);
			const string speed_bits = (b_width >= 20 ? floating_humanizer(net.stat.at(dir).speed, false, 0, true, true) : "");
			const string top = floating_humanizer(net.stat.at(dir).top, false, 0, true, true);
			const string total = floating_humanizer(net.stat.at(dir).total);
			const string symbol = (dir == "upload" ? "▲" : "▼");
			out += Mv::to(b_y+1+cy, b_x+1) + Fx::ub + Theme::c("main_fg") + symbol + ' ' + ljust(speed, 10) + (b_width >= 20 ? rjust('(' + speed_bits + ')', 13) : "");
			cy += (b_height == 3 ? 2 : 1);
			if (b_height >= 6) {
				out += Mv::to(b_y+1+cy, b_x+1) + symbol + ' ' + "Top: " + rjust('(' + top, (b_width >= 20 ? 17 : 9)) + ')';
				cy++;
			}
			if (b_height >= 4) {
				out += Mv::to(b_y+1+cy, b_x+1) + symbol + ' ' + "Total: " + rjust(total, (b_width >= 20 ? 16 : 8));
				cy += (b_height > 2 and b_height % 2 ? 2 : 1);
			}
		}


		redraw = false;
		return out + Fx::reset;
	}

}

namespace Proc {
	int width_p = 55, height_p = 68;
	int min_width = 44, min_height = 16;
	int x, y, width = 20, height;
	int start, selected, select_max;
	bool shown = true, redraw = true;
	int selected_pid = 0;
	unordered_flat_map<size_t, Draw::Graph> p_graphs;
	unordered_flat_map<size_t, int> p_counters;
	int counter = 0;
	Draw::TextEdit filter;
	Draw::Graph detailed_cpu_graph;
	Draw::Graph detailed_mem_graph;
	int user_size, thread_size, prog_size, cmd_size, tree_size;
	int dgraph_x, dgraph_width, d_width, d_x, d_y;

	string box;

	int selection(const string& cmd_key) {
		auto start = Config::getI("proc_start");
		auto selected = Config::getI("proc_selected");
		auto last_selected = Config::getI("proc_last_selected");
		const int select_max = (Config::getB("show_detailed") ? Proc::select_max - 8 : Proc::select_max);

		int numpids = Proc::numpids;
		if (cmd_key == "up" and selected > 0) {
			if (start > 0 and selected == 1) start--;
			else selected--;
			if (Config::getI("proc_last_selected") > 0) Config::set("proc_last_selected", 0);
		}
		else if (cmd_key == "mouse_scroll_up" and start > 0) {
			start = max(0, start - 3);
		}
		else if (cmd_key == "mouse_scroll_down" and start < numpids - select_max) {
			start = min(numpids - select_max, start + 3);
		}
		else if (cmd_key == "down") {
			if (start < numpids - select_max and selected == select_max) start++;
			else if (selected == 0 and last_selected > 0) {
				selected = last_selected;
				Config::set("proc_last_selected", 0);
			}
			else selected++;
		}
		else if (cmd_key == "page_up") {
			if (selected > 0 and start == 0) selected = 0;
			else start = max(0, start - select_max);
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
		else if (cmd_key.starts_with("mousey")) {
			int mouse_y = std::stoi(cmd_key.substr(6));
			start = clamp((int)round((double)mouse_y * (numpids - select_max - 2) / (select_max - 2)), 0, max(0, numpids - select_max));
		}

		bool changed = false;
		if (start != Config::getI("proc_start")) {
			Config::set("proc_start", start);
			changed = true;
		}
		if (selected != Config::getI("proc_selected")) {
			Config::set("proc_selected", selected);
			changed = true;
		}
		return (not changed ? -1 : selected);
	}

	string draw(const vector<proc_info>& plist, const bool force_redraw, const bool data_same) {
		if (Runner::stopping) return "";
		auto& proc_tree = Config::getB("proc_tree");
		const bool show_detailed = (Config::getB("show_detailed") and cmp_equal(Proc::detailed.last_pid, Config::getI("detailed_pid")));
		const bool proc_gradient = (Config::getB("proc_gradient") and not Config::getB("lowcolor") and Theme::gradients.contains("proc"));
		auto& proc_colors = Config::getB("proc_colors");
		auto& tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_proc"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(1);
		auto& mem_bytes = Config::getB("proc_mem_bytes");
		start = Config::getI("proc_start");
		selected = Config::getI("proc_selected");
		const int y = show_detailed ? Proc::y + 8 : Proc::y;
		const int height = show_detailed ? Proc::height - 8 : Proc::height;
		const int select_max = show_detailed ? Proc::select_max - 8 : Proc::select_max;
		int numpids = Proc::numpids;
		if (force_redraw) redraw = true;
		string out;
		out.reserve(width * height);

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			out = box;
			const string title_left = Theme::c("proc_box") + Symbols::title_left;
			const string title_right = Theme::c("proc_box") + Symbols::title_right;
			const string title_left_down = Theme::c("proc_box") + Symbols::title_left_down;
			const string title_right_down = Theme::c("proc_box") + Symbols::title_right_down;
			for (const auto& key : {"T", "K", "S", "enter"})
				if (Input::mouse_mappings.contains(key)) Input::mouse_mappings.erase(key);

			//? Adapt sizes of text fields
			user_size = (width < 75 ? 5 : 10);
			thread_size = (width < 75 ? - 1 : 4);
			prog_size = (width > 70 ? 16 : ( width > 55 ? 8 : width - user_size - thread_size - 33));
			cmd_size = (width > 55 ? width - prog_size - user_size - thread_size - 33 : -1);
			tree_size = width - user_size - thread_size - 23;

			//? Detailed box
			if (show_detailed) {
				const bool alive = detailed.status != "Dead";
				dgraph_x = x;
				dgraph_width = max(width / 3, width - 121);
				d_width = width - dgraph_width - 1;
				d_x = x + dgraph_width + 1;
				d_y = Proc::y;

				//? Create cpu and mem graphs if process is alive
				if (alive) {
					detailed_cpu_graph = Draw::Graph{dgraph_width - 1, 7, "cpu", detailed.cpu_percent, graph_symbol, false, true};
					detailed_mem_graph = Draw::Graph{d_width / 3, 1, "", detailed.mem_bytes, graph_symbol, false, false, detailed.first_mem};
				}

				//? Draw structure of details box
				const string pid_str = to_string(detailed.entry.pid);
				out += Mv::to(y, x) + Theme::c("proc_box") + Symbols::div_left + Symbols::h_line + title_left + Theme::c("hi_fg") + Fx::b
				+ (tty_mode ? "4" : Symbols::superscript.at(4)) + Theme::c("title") + "proc"
					+ Fx::ub + title_right + Symbols::h_line * (width - 10) + Symbols::div_right
					+ Mv::to(d_y, dgraph_x + 2) + title_left + Fx::b + Theme::c("title") + pid_str + Fx::ub + title_right
					+ title_left + Fx::b + Theme::c("title") + uresize(detailed.entry.name, dgraph_width - pid_str.size() - 7, true) + Fx::ub + title_right;

				out += Mv::to(d_y, d_x - 1) + Theme::c("proc_box") + Symbols::div_up + Mv::to(y, d_x - 1) + Symbols::div_down + Theme::c("div_line");
				for (const int& i : iota(1, 8)) out += Mv::to(d_y + i, d_x - 1) + Symbols::v_line;

				const string& t_color = (not alive or selected > 0 ? Theme::c("inactive_fg") : Theme::c("title"));
				const string& hi_color = (not alive or selected > 0 ? t_color : Theme::c("hi_fg"));
				const string hide = (selected > 0 ? t_color + "hide " : Theme::c("title") + "hide " + Theme::c("hi_fg"));
				int mouse_x = d_x + 2;
				out += Mv::to(d_y, d_x + 1);
				if (width > 55) {
					out += title_left + hi_color + Fx::b + 't' + t_color + "erminate" + Fx::ub + title_right;
					if (alive and selected == 0) Input::mouse_mappings["t"] = {d_y, mouse_x, 1, 9};
					mouse_x += 11;
				}
				out += title_left + hi_color + Fx::b + 'k' + t_color + "ill" + Fx::ub + title_right
					+ title_left + hi_color + Fx::b + 's' + t_color + "ignals" + Fx::ub + title_right
					+ Mv::to(d_y, d_x + d_width - 10) + title_left + t_color + Fx::b + hide + Symbols::enter + Fx::ub + title_right;
				if (alive and selected == 0) {
					Input::mouse_mappings["k"] = {d_y, mouse_x, 1, 4};
					mouse_x += 6;
					Input::mouse_mappings["s"] = {d_y, mouse_x, 1, 7};
				}
				if (selected == 0) Input::mouse_mappings["enter"] = {d_y, d_x + d_width - 9, 1, 6};

				//? Labels
				const int item_fit = floor((double)(d_width - 2) / 10);
				const int item_width = floor((double)(d_width - 2) / min(item_fit, 8));
				out += Mv::to(d_y + 1, d_x + 1) + Fx::b + Theme::c("title")
										+ cjust("Status:", item_width)
										+ cjust("Elapsed:", item_width);
				if (item_fit >= 3) out += cjust("IO/R:", item_width);
				if (item_fit >= 4) out += cjust("IO/W:", item_width);
				if (item_fit >= 5) out += cjust("Parent:", item_width);
				if (item_fit >= 6) out += cjust("User:", item_width);
				if (item_fit >= 7) out += cjust("Threads:", item_width);
				if (item_fit >= 8) out += cjust("Nice:", item_width);


				//? Command line
				for (int i = 0; const auto& l : {'C', 'M', 'D'})
				out += Mv::to(d_y + 5 + i++, d_x + 1) + l;

				out += Theme::c("main_fg") + Fx::ub;
				const int cmd_size = ulen(detailed.entry.cmd, true);
				for (int num_lines = min(3, (int)ceil((double)cmd_size / (d_width - 5))), i = 0; i < num_lines; i++) {
					out += Mv::to(d_y + 5 + (num_lines == 1 ? 1 : i), d_x + 3)
						+ cjust(luresize(detailed.entry.cmd, cmd_size - (d_width - 5) * i, true), d_width - 5, true, true);
				}

			}

			//? Filter
			auto& filtering = Config::getB("proc_filtering"); // ? filter(20) : Config::getS("proc_filter"))
			const auto filter_text = (filtering) ? filter(max(6, width - 58)) : uresize(Config::getS("proc_filter"), max(6, width - 58));
			out += Mv::to(y, x+9) + title_left + (not filter_text.empty() ? Fx::b : "") + Theme::c("hi_fg") + 'f'
				+ Theme::c("title") + (not filter_text.empty() ? ' ' + filter_text : "ilter")
				+ (not filtering and not filter_text.empty() ? Theme::c("hi_fg") + " del" : "")
				+ (filtering ? Theme::c("hi_fg") + ' ' + Symbols::enter : "") + Fx::ub + title_right;
			if (not filtering) {
				int f_len = (filter_text.empty() ? 6 : ulen(filter_text) + 2);
				Input::mouse_mappings["f"] = {y, x + 10, 1, f_len};
				if (filter_text.empty() and Input::mouse_mappings.contains("delete"))
					Input::mouse_mappings.erase("delete");
				else if (not filter_text.empty())
					Input::mouse_mappings["delete"] = {y, x + 11 + f_len, 1, 3};
			}

			//? per-core, reverse, tree and sorting
			const auto& sorting = Config::getS("proc_sorting");
			const int sort_len = sorting.size();
			const int sort_pos = x + width - sort_len - 8;

			if (width > 55 + sort_len) {
				out += Mv::to(y, sort_pos - 25) + title_left + (Config::getB("proc_per_core") ? Fx::b : "") + Theme::c("title")
					+ "per-" + Theme::c("hi_fg") + 'c' + Theme::c("title") + "ore" + Fx::ub + title_right;
				Input::mouse_mappings["c"] = {y, sort_pos - 24, 1, 8};
			}
			if (width > 45 + sort_len) {
				out += Mv::to(y, sort_pos - 15) + title_left + (Config::getB("proc_reversed") ? Fx::b : "") + Theme::c("hi_fg")
					+ 'r' + Theme::c("title") + "everse" + Fx::ub + title_right;
				Input::mouse_mappings["r"] = {y, sort_pos - 14, 1, 7};
			}
			if (width > 35 + sort_len) {
				out += Mv::to(y, sort_pos - 6) + title_left + (Config::getB("proc_tree") ? Fx::b : "") + Theme::c("title") + "tre"
					+ Theme::c("hi_fg") + 'e' + Fx::ub + title_right;
				Input::mouse_mappings["e"] = {y, sort_pos - 5, 1, 4};
			}
			out += Mv::to(y, sort_pos) + title_left + Fx::b + Theme::c("hi_fg") + "< " + Theme::c("title") + sorting + Theme::c("hi_fg")
				+ " >" + Fx::ub + title_right;
				Input::mouse_mappings["left"] = {y, sort_pos + 1, 1, 2};
				Input::mouse_mappings["right"] = {y, sort_pos + sort_len + 3, 1, 2};

			//? select, info and signal buttons
			const string down_button = (selected == select_max and start == numpids - select_max ? Theme::c("inactive_fg") : Theme::c("hi_fg")) + Symbols::down;
			const string t_color = (selected == 0 ? Theme::c("inactive_fg") : Theme::c("title"));
			const string hi_color = (selected == 0 ? Theme::c("inactive_fg") : Theme::c("hi_fg"));
			int mouse_x = x + 14;
			out += Mv::to(y + height - 1, x + 1) + title_left_down + Fx::b + hi_color + Symbols::up + Theme::c("title") + " select " + down_button + Fx::ub + title_right_down
				+ title_left_down + Fx::b + t_color + "info " + hi_color + Symbols::enter + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings["enter"] = {y + height - 1, mouse_x, 1, 6};
				mouse_x += 8;
			if (width > 60) {
				out += title_left_down + Fx::b + hi_color + 't' + t_color + "erminate" + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings["t"] = {y + height - 1, mouse_x, 1, 9};
				mouse_x += 11;
			}
			if (width > 55) {
				out += title_left_down + Fx::b + hi_color + 'k' + t_color + "ill" + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings["k"] = {y + height - 1, mouse_x, 1, 4};
				mouse_x += 6;
			}
			out += title_left_down + Fx::b + hi_color + 's' + t_color + "ignals" + Fx::ub + title_right_down;
			if (selected > 0) Input::mouse_mappings["s"] = {y + height - 1, mouse_x, 1, 7};

			//? Labels for fields in list
			if (not proc_tree)
				out += Mv::to(y+1, x+1) + Theme::c("title") + Fx::b
					+ rjust("Pid:", 8) + ' '
					+ ljust("Program:", prog_size) + ' '
					+ (cmd_size > 0 ? ljust("Command:", cmd_size) : "") + ' ';
			else
				out += Mv::to(y+1, x+1) + Theme::c("title") + Fx::b
					+ ljust("Tree:", tree_size) + ' ';

			out += (thread_size > 0 ? Mv::l(4) + "Threads: " : "")
					+ ljust("User:", user_size) + ' '
					+ rjust((mem_bytes ? "MemB" : "Mem%"), 5) + ' '
					+ rjust("Cpu%", 10) + Fx::ub;
		}
		//* End of redraw block

		//? Draw details box if shown
		if (show_detailed) {
			const bool alive = detailed.status != "Dead";
			const int item_fit = floor((double)(d_width - 2) / 10);
			const int item_width = floor((double)(d_width - 2) / min(item_fit, 8));

			//? Graph part of box
			string cpu_str = (alive ? to_string(detailed.entry.cpu_p) : "");
			if (alive) {
				cpu_str.resize((detailed.entry.cpu_p < 10 or detailed.entry.cpu_p >= 100 ? 3 : 4));
				cpu_str += '%' + Mv::r(1) + (dgraph_width < 20 ? "C" : "Core") + to_string(detailed.entry.cpu_n);
			}
			out += Mv::to(d_y + 1, dgraph_x + 1) + Fx::ub + detailed_cpu_graph(detailed.cpu_percent, (redraw or data_same or not alive))
				+ Mv::to(d_y + 1, dgraph_x + 1) + Theme::c("title") + Fx::b + cpu_str;
			for (int i = 0; const auto& l : {'C', 'P', 'U'})
					out += Mv::to(d_y + 3 + i++, dgraph_x + 1) + l;

			//? Info part of box
			const string stat_color = (not alive ? Theme::c("inactive_fg") : (detailed.status == "Running" ? Theme::c("proc_misc") : Theme::c("main_fg")));
			out += Mv::to(d_y + 2, d_x + 1) + stat_color + Fx::ub
									+ cjust(detailed.status, item_width) + Theme::c("main_fg")
									+ cjust(detailed.elapsed, item_width);
			if (item_fit >= 3) out += cjust(detailed.io_read, item_width);
			if (item_fit >= 4) out += cjust(detailed.io_write, item_width);
			if (item_fit >= 5) out += cjust(detailed.parent, item_width, true);
			if (item_fit >= 6) out += cjust(detailed.entry.user, item_width, true);
			if (item_fit >= 7) out += cjust(to_string(detailed.entry.threads), item_width);
			if (item_fit >= 8) out += cjust(to_string(detailed.entry.p_nice), item_width);


			const double mem_p = (double)detailed.mem_bytes.back() * 100 / Shared::totalMem;
			string mem_str = to_string(mem_p);
			mem_str.resize((mem_p < 10 or mem_p >= 100 ? 3 : 4));
			out += Mv::to(d_y + 4, d_x + 1) + Theme::c("title") + Fx::b + rjust((item_fit > 4 ? "Memory: " : "M:") + mem_str + "% ", (d_width / 3) - 2)
				+ Theme::c("inactive_fg") + Fx::ub + graph_bg * (d_width / 3) + Mv::l(d_width / 3)
				+ Theme::c("proc_misc") + detailed_mem_graph(detailed.mem_bytes, (redraw or data_same or not alive)) + ' '
				+ Theme::c("title") + Fx::b + detailed.memory;


		}


		//? Check bounds of current selection and view
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
			if (n++ < start or p.filtered) continue;
			bool is_selected = (lc + 1 == selected);
			if (is_selected) selected_pid = (int)p.pid;

			//? Update graphs for processes with above 0.0% cpu usage, delete if below 0.1% 10x times
			const bool has_graph = p_counters.contains(p.pid);
			if ((p.cpu_p > 0 and not has_graph) or (not data_same and has_graph)) {
				if (not has_graph) {
					p_graphs[p.pid] = Draw::Graph{5, 1, "", {}, graph_symbol};
					p_counters[p.pid] = 0;
				}
				else if (p.cpu_p < 0.1 and ++p_counters[p.pid] >= 10) {
					p_graphs.erase(p.pid);
					p_counters.erase(p.pid);
				}
				else
					p_counters.at(p.pid) = 0;
			}

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
					for (int i = 0; int v : {(int)round(p.cpu_p), (int)round(p.mem * 100 / Shared::totalMem), (int)p.threads / 3}) {
						if (proc_gradient) {
							int val = (min(v, 100) + 100) - calc * 100 / select_max;
							if (val < 100) colors[i++] = Theme::g("proc_color").at(val);
							else colors[i++] = Theme::g("process").at(val - 100);
						}
						else
							colors[i++] = Theme::g("process").at(min(v, 100));
					}
					c_color = colors.at(0); m_color = colors.at(1); t_color = colors.at(2);
				}
				else {
					c_color = m_color = t_color = Fx::b;
					end = Fx::ub;
				}
				if (proc_gradient) {
					g_color = Theme::g("proc").at(calc * 100 / select_max);
				}
			}

			//? Normal view line
			if (not proc_tree) {
				out += Mv::to(y+2+lc, x+1)
					+ g_color + rjust(to_string(p.pid), 8) + ' '
					+ c_color + ljust(p.name, prog_size, true) + ' ' + end
					+ (cmd_size > 0 ? g_color + ljust(p.cmd, cmd_size, true, true) + ' ' : "");
			}
			//? Tree view line
			else {
				const string prefix_pid = p.prefix + to_string(p.pid);
				int width_left = tree_size;
				out += Mv::to(y+2+lc, x+1) + g_color + uresize(prefix_pid, width_left) + ' ';
				width_left -= ulen(prefix_pid);
				if (width_left > 0) {
					out += c_color + uresize(p.name, width_left - 1) + end + ' ';
					width_left -= (ulen(p.name) + 1);
				}
				if (width_left > 7 and p.short_cmd != p.name) {
					out += g_color + '(' + uresize(p.short_cmd, width_left - 3, true) + ") ";
					width_left -= (ulen(p.short_cmd, true) + 3);
				}
				out += string(max(0, width_left), ' ');
			}
			//? Common end of line
			string cpu_str = to_string(p.cpu_p);
			if (p.cpu_p < 10 or p.cpu_p >= 100) cpu_str.resize(3);
			string mem_str = (mem_bytes ? floating_humanizer(p.mem, true) : "");
			if (not mem_bytes) {
				double mem_p = (double)p.mem * 100 / Shared::totalMem;
				mem_str = to_string(mem_p);
				mem_str.resize((mem_p < 10 or mem_p >= 100 ? 3 : 4));
				mem_str += '%';
			}
			out += (thread_size > 0 ? t_color + rjust(to_string(min(p.threads, 9999ul)), thread_size) + ' ' + end : "" )
				+ g_color + ljust((cmp_greater(p.user.size(), user_size) ? p.user.substr(0, user_size - 1) + '+' : p.user), user_size) + ' '
				+ m_color + rjust(mem_str, 5) + end + ' '
				+ (is_selected ? "" : Theme::c("inactive_fg")) + graph_bg * 5
				+ (p_graphs.contains(p.pid) ? Mv::l(5) + c_color + p_graphs.at(p.pid)({(p.cpu_p >= 0.1 and p.cpu_p < 5 ? 5ll : (long long)round(p.cpu_p))}, data_same) : "") + end + ' '
				+ c_color + rjust(cpu_str, 4) + "  " + end;
			if (lc++ > height - 5) break;
		}

		out += Fx::reset;
		while (lc++ < height - 5) out += Mv::to(y+lc+1, x+1) + string(width - 2, ' ');

		//? Draw scrollbar if needed
		if (numpids > select_max) {
			const int scroll_pos = clamp((int)round((double)start * (select_max - 2) / (numpids - (select_max - 2))), 0, height - 5);
			out += Mv::to(y + 1, x + width - 2) + Fx::b + Theme::c("main_fg") + Symbols::up
				+ Mv::to(y + height - 2, x + width - 2) + Symbols::down
				+ Mv::to(y + 2 + scroll_pos, x + width - 2) + "█";
		}

		//? Current selection and number of processes
		string location = to_string(start + selected) + '/' + to_string(numpids);
		string loc_clear = Symbols::h_line * max(0ul, 9 - location.size());
		out += Mv::to(y + height - 1, x+width - 3 - max(9, (int)location.size())) + Theme::c("proc_box") + loc_clear
			+ Symbols::title_left_down + Theme::c("title") + Fx::b + location + Fx::ub + Theme::c("proc_box") + Symbols::title_right_down;

		//? Clear out left over graphs from dead processes at a regular interval
		if (not data_same and ++counter >= 100) {
			counter = 0;
			for (auto element = p_graphs.begin(); element != p_graphs.end();) {
				if (rng::find(plist, element->first, &proc_info::pid) == plist.end()) {
					element = p_graphs.erase(element);
					p_counters.erase(element->first);
				}
				else
					++element;
			}
			p_graphs.compact();
			p_counters.compact();
		}

		if (selected == 0 and selected_pid != 0) selected_pid = 0;
		redraw = false;
		return out + Fx::reset;
	}

}

namespace Draw {
	void calcSizes() {
		atomic_wait(Runner::active);
		auto& boxes = Config::getS("shown_boxes");
		auto& cpu_bottom = Config::getB("cpu_bottom");
		auto& mem_below_net = Config::getB("mem_below_net");
		auto& proc_left = Config::getB("proc_left");

		Cpu::box.clear();
		Mem::box.clear();
		Net::box.clear();
		Proc::box.clear();
		Global::clock.clear();
		Global::overlay.clear();

		Input::mouse_mappings.clear();

		Cpu::x = Mem::x = Net::x = Proc::x = 1;
		Cpu::y = Mem::y = Net::y = Proc::y = 1;
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
			const bool show_temp = (Config::getB("check_temp") and got_sensors);
			width = round((double)Term::width * width_p / 100);
			height = max(8, (int)round((double)Term::height * (trim(boxes) == "cpu" ? 100 : height_p) / 100));
			x = 1;
			y = cpu_bottom ? Term::height - height + 1 : 1;

			b_columns = max(1, (int)ceil((double)(Shared::coreCount + 1) / (height - 5)));
			if (b_columns * (21 + 12 * show_temp) < width - (width / 3)) {
				b_column_size = 2;
				b_width = (21 + 12 * show_temp) * b_columns - (b_columns - 1);
			}
			else if (b_columns * (15 + 6 * show_temp) < width - (width / 3)) {
				b_column_size = 1;
				b_width = (15 + 6 * show_temp) * b_columns - (b_columns - 1);
			}
			else if (b_columns * (8 + 6 * show_temp) < width - (width / 3)) {
				b_column_size = 0;
			}
			else {
				b_columns = (width - width / 3) / (8 + 6 * show_temp);
				b_column_size = 0;
			}

			if (b_column_size == 0) b_width = (8 + 6 * show_temp) * b_columns + 1;
			b_height = min(height - 2, (int)ceil((double)Shared::coreCount / b_columns) + 4);

			b_x = x + width - b_width - 1;
			b_y = y + ceil((double)(height - 2) / 2) - ceil((double)b_height / 2) + 1;

			box = createBox(x, y, width, height, Theme::c("cpu_box"), true, (cpu_bottom ? "" : "cpu"), (cpu_bottom ? "cpu" : ""), 1);

			auto& custom = Config::getS("custom_cpu_name");
			const string cpu_title = uresize((custom.empty() ? Cpu::cpuName : custom) , b_width - 14);
			box += createBox(b_x, b_y, b_width, b_height, "", false, cpu_title);
		}

		//* Calculate and draw mem box outlines
		if (Mem::shown) {
			using namespace Mem;
			auto& show_disks = Config::getB("show_disks");
			auto& swap_disk = Config::getB("swap_disk");
			auto& mem_graphs = Config::getB("mem_graphs");

			width = round((double)Term::width * (Proc::shown ? width_p : 100) / 100);
			height = round((double)Term::height * (100 - Cpu::height_p * Cpu::shown - Net::height_p * Net::shown) / 100) + 1;
			if (height + Cpu::height > Term::height) height = Term::height - Cpu::height;
			x = (proc_left and Proc::shown) ? Term::width - width + 1: 1;
			if (mem_below_net and Net::shown)
				y = Term::height - height + 1 - (cpu_bottom ? Cpu::height : 0);
			else
				y = cpu_bottom ? 1 : Cpu::height + 1;

			if (show_disks) {
				mem_width = ceil((double)(width - 3) / 2);
				mem_width += mem_width % 2;
				disks_width = width - mem_width - 2;
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

			mem_meter = max(0, mem_width - (mem_size > 2 ? 7 : 17));
			if (mem_size == 1) mem_meter += 6;

			if (mem_graphs) {
				graph_height = max(1, (int)round((double)((height - (has_swap and not swap_disk ? 2 : 1)) - (mem_size == 3 ? 2 : 1) * item_height) / item_height));
				if (graph_height > 1) mem_meter += 6;
			}
			else
				graph_height = 0;

			if (show_disks) {
				disk_meter = max(-14, width - mem_width - 23);
				if (disks_width < 25) disk_meter += 14;
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
			width = round((double)Term::width * (Proc::shown ? width_p : 100) / 100);
			height = Term::height - Cpu::height - Mem::height;
			x = (proc_left and Proc::shown) ? Term::width - width + 1 : 1;
			if (mem_below_net and Mem::shown)
				y = cpu_bottom ? 1 : Cpu::height + 1;
			else
				y = Term::height - height + 1 - (cpu_bottom ? Cpu::height : 0);

			b_width = (width > 45) ? 27 : 19;
			b_height = (height > 10) ? 9 : height - 2;
			b_x = x + width - b_width - 1;
			b_y = y + ((height - 2) / 2) - b_height / 2 + 1;
			d_graph_height = round((double)(height - 2) / 2);
			u_graph_height = height - 2 - d_graph_height;

			box = createBox(x, y, width, height, Theme::c("net_box"), true, "net", "", 3);
			box += createBox(b_x, b_y, b_width, b_height, "", false, "download", "upload");
		}

		//* Calculate and draw proc box outlines
		if (Proc::shown) {
			using namespace Proc;
			width = Term::width - (Mem::shown ? Mem::width : (Net::shown ? Net::width : 0));
			height = Term::height - Cpu::height;
			x = proc_left ? 1 : Term::width - width + 1;
			y = (cpu_bottom and Cpu::shown) ? 1 : Cpu::height + 1;
			select_max = height - 3;
			box = createBox(x, y, width, height, Theme::c("proc_box"), true, "proc", "", 4);
		}
	}
}