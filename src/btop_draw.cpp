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

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_input.hpp"
#include "btop_log.hpp"
#include "btop_menu.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

using std::array;
using std::clamp;
using std::cmp_equal;
using std::cmp_greater;
using std::cmp_less;
using std::cmp_less_equal;
using std::floor;
using std::max;
using std::min;
using std::round;
using std::to_string;
using std::views::iota;

using namespace Tools;
using namespace std::literals; // for operator""s
namespace rng = std::ranges;

namespace Symbols {
	const string meter = "■";

	const array<string, 10> superscript = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹" };

	const std::unordered_map<string, vector<string>> graph_symbols = {
		{ "braille_up", {
			" ", "⢀", "⢠", "⢰", "⢸",
			"⡀", "⣀", "⣠", "⣰", "⣸",
			"⡄", "⣄", "⣤", "⣴", "⣼",
			"⡆", "⣆", "⣦", "⣶", "⣾",
			"⡇", "⣇", "⣧", "⣷", "⣿"
		}},
		{"braille_down", {
			" ", "⠈", "⠘", "⠸", "⢸",
			"⠁", "⠉", "⠙", "⠹", "⢹",
			"⠃", "⠋", "⠛", "⠻", "⢻",
			"⠇", "⠏", "⠟", "⠿", "⢿",
			"⡇", "⡏", "⡟", "⡿", "⣿"
		}},
		{"block_up", {
			" ", "▗", "▗", "▐", "▐",
			"▖", "▄", "▄", "▟", "▟",
			"▖", "▄", "▄", "▟", "▟",
			"▌", "▙", "▙", "█", "█",
			"▌", "▙", "▙", "█", "█"
		}},
		{"block_down", {
			" ", "▝", "▝", "▐", "▐",
			"▘", "▀", "▀", "▜", "▜",
			"▘", "▀", "▀", "▜", "▜",
			"▌", "▛", "▛", "█", "█",
			"▌", "▛", "▛", "█", "█"
		}},
		{"tty_up", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}},
		{"tty_down", {
			" ", "░", "░", "▒", "▒",
			"░", "░", "▒", "▒", "█",
			"░", "▒", "▒", "▒", "█",
			"▒", "▒", "▒", "█", "█",
			"▒", "█", "█", "█", "█"
		}}
	};

}

namespace Draw {

	string banner_gen(int y, int x, bool centered, bool redraw) {
		static string banner;
		static size_t width = 0;
		if (redraw) banner.clear();
		if (banner.empty()) {
			string b_color, bg, fg, oc, letter;
			auto lowcolor = Config::getB("lowcolor");
			auto tty_mode = Config::getB("tty_mode");
			for (size_t z = 0; const auto& line : Global::Banner_src) {
				if (const auto w = ulen(line[1]); w > width) width = w;
				if (tty_mode) {
					fg = (z > 2) ? "\x1b[31m" : "\x1b[91m";
					bg = (z > 2) ? "\x1b[90m" : "\x1b[37m";
				}
				else {
					fg = Theme::hex_to_color(line[0], lowcolor);
					int bg_i = 120 - z * 12;
					bg = Theme::dec_to_color(bg_i, bg_i, bg_i, lowcolor);
				}
				for (size_t i = 0; i < line[1].size(); i += 3) {
					if (line[1][i] == ' ') {
						letter = Mv::r(1);
						i -= 2;
					}
					else
						letter = line[1].substr(i, 3);

					b_color = (letter == "█") ? fg : bg;
					if (b_color != oc) banner += b_color;
					banner += letter;
					oc = b_color;
				}
				if (++z < Global::Banner_src.size()) banner += Mv::l(ulen(line[1])) + Mv::d(1);
			}
			banner += Mv::r(18 - Global::Version.size())
					+ Theme::c("main_fg") + Fx::b + Fx::i + "v" + Global::Version + Fx::reset;
		}
		if (redraw) return "";
		return (centered ? Mv::to(y, Term::width / 2 - width / 2) : Mv::to(y, x)) + banner;
	}

	TextEdit::TextEdit() {}
	TextEdit::TextEdit(string text, bool numeric) : numeric(numeric), text(std::move(text)) {
		pos = this->text.size();
		upos = ulen(this->text);
	}

	bool TextEdit::command(const std::string_view key) {
		if (key == "left" and upos > 0) {
			upos--;
			pos = uresize(text, upos).size();
		}
		else if (key == "right" and pos < text.size()) {
			upos++;
			pos = uresize(text, upos).size();
		}
		else if (key == "home" and not text.empty() and pos > 0) {
			pos = upos = 0;
		}
		else if (key == "end" and not text.empty() and pos < text.size()) {
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
		else if (key == "space" and not numeric) {
			text.insert(pos++, 1, ' ');
			upos++;
		}
		else if (ulen(key) == 1 and text.size() < text.max_size() - 20) {
			if (numeric and not isint(key)) return false;
			if (key.size() == 1) {
				text.insert(pos++, 1, key.at(0));
				upos++;
			}
			else {
				const auto first = fmt::format("{}{}", uresize(text, upos), key);
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
		string out;
		size_t c_upos = upos;
		if (text.empty())
			return Fx::ul + " " + Fx::uul;
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

				out = first + uresize(text.substr(pos), limit - ulen(first));
				c_upos = ulen(first);
			}
			catch (const std::exception& e) {
				Logger::error("In TextEdit::operator() : {}", e.what());
				return "";
			}
		}
		else
			out = text;

		if (c_upos == 0)
			return Fx::ul + uresize(out, 1) + Fx::uul + luresize(out, ulen(out) - 1);
		else if (c_upos == ulen(out))
			return out + Fx::ul + " " + Fx::uul;
		else
			return uresize(out, c_upos) + Fx::ul + luresize(uresize(out, c_upos + 1), 1) + Fx::uul + luresize(out, ulen(out) - c_upos - 1);
	}

	void TextEdit::clear() {
		this->text.clear();
	}

	string createBox(
			const int x, const int y, const int width, const int height, string line_color, bool fill, const std::string_view title,
			const std::string_view title2, const int num
	) {
		string out;

		if (line_color.empty())
			line_color = Theme::c("div_line");

		auto tty_mode = Config::getB("tty_mode");
		auto rounded = Config::getB("rounded_corners");
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
			out += fmt::format(
				"{}{}{}{}{}{}{}{}{}", Mv::to(y, x + 2), Symbols::title_left, Fx::b, numbering, Theme::c("title"), title, Fx::ub,
				line_color, Symbols::title_right
			);
		}
		if (not title2.empty()) {
			out += fmt::format(
				"{}{}{}{}{}{}{}{}{}", Mv::to(y + height - 1, x + 2), Symbols::title_left_down, Fx::b, numbering, Theme::c("title"), title2, Fx::ub,
				line_color, Symbols::title_right_down
			);
		}

		return out + Fx::reset + Mv::to(y + 1, x + 1);
	}

	bool update_clock(bool force) {
		const auto& clock_format = Config::getS("clock_format");
		if (not Cpu::shown or clock_format.empty()) {
			if (clock_format.empty() and not Global::clock.empty()) Global::clock.clear();
			return false;
		}

		static const std::unordered_map<string, string> clock_custom_format = {
			{"/user", Tools::username()},
			{"/host", Tools::hostname()},
			{"/uptime", ""}
		};

		static time_t c_time{};
		static size_t clock_len{};
		static string clock_str;

		if (auto n_time = time(nullptr); not force and n_time == c_time)
			return false;
		else {
			c_time = n_time;
			const auto new_clock = Tools::strf_time(clock_format);
			if (not force and new_clock == clock_str) return false;
			clock_str = new_clock;
		}

		auto& out = Global::clock;
		auto cpu_bottom = Config::getB("cpu_bottom");
		const auto& x = Cpu::x;
		const auto y = (cpu_bottom ? Cpu::y + Cpu::height - 1 : Cpu::y);
		const auto& width = Cpu::width;
		const auto& title_left = (cpu_bottom ? Symbols::title_left_down : Symbols::title_left);
		const auto& title_right = (cpu_bottom ? Symbols::title_right_down : Symbols::title_right);


		for (const auto& [c_format, replacement] : clock_custom_format) {
			if (clock_str.contains(c_format)) {
				if (c_format == "/uptime") {
					string upstr = sec_to_dhms(system_uptime());
					if (upstr.size() > 8) upstr.resize(upstr.size() - 3);
					clock_str = s_replace(clock_str, c_format, upstr);
				}
				else {
					clock_str = s_replace(clock_str, c_format, replacement);
				}
			}

		}

		clock_str = uresize(clock_str, std::max(10, width - 66 - (Term::width >= 100 and Config::getB("show_battery") and Cpu::has_battery ? 22 : 0)));
		out.clear();

		if (clock_str.size() != clock_len) {
			if (not Global::resized and clock_len > 0)
				out = Mv::to(y, x+(width / 2)-(clock_len / 2)) + Fx::ub + Theme::c("cpu_box") + Symbols::h_line * clock_len;
			clock_len = clock_str.size();
		}

		out += Mv::to(y, x+(width / 2)-(clock_len / 2)) + Fx::ub + Theme::c("cpu_box") + title_left
			+ Theme::c("title") + Fx::b + clock_str + Theme::c("cpu_box") + Fx::ub + title_right;

		return true;
	}

	//* Meter class ------------------------------------------------------------------------------------------------------------>
	Meter::Meter() {}

	Meter::Meter(const int width, string color_gradient, bool invert)
		: width(width), color_gradient(std::move(color_gradient)), invert(invert) {}

	string Meter::operator()(int value) {
		if (width < 1) return "";
		value = clamp(value, 0, 100);
		if (not cache.at(value).empty()) return cache.at(value);
		auto& out = cache.at(value);
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
		bool mult = (data.size() - data_offset > 1);
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
				if (height == 1) {
					if (result.at(0) + result.at(1) == 0) graphs.at(current).at(horizon) += Mv::r(1);
					else {
						if (not color_gradient.empty()) graphs.at(current).at(horizon) += Theme::g(color_gradient).at(clamp(max(last, data_value), 0ll, 100ll));
						graphs.at(current).at(horizon) += graph_symbol.at((result.at(0) * 5 + result.at(1)));
					}
				}
				else graphs.at(current).at(horizon) += graph_symbol.at((result.at(0) * 5 + result.at(1)));
			}
			if (mult and i >= 0) last = data_value;
		}
		last = data_value;
		out.clear();
		if (height == 1) {
			//if (not color_gradient.empty())
			//	out += (last < 1 ? Theme::c("inactive_fg") : Theme::g(color_gradient).at(clamp(last, 0ll, 100ll)));
			out += graphs.at(current).at(0);
		}
		else {
			for (const int& i : iota(1, height + 1)) {
				if (i > 1) out += Mv::d(1) + Mv::l(width);
				if (not color_gradient.empty())
					out += (invert) ? Theme::g(color_gradient).at(i * 100 / height) : Theme::g(color_gradient).at(100 - ((i - 1) * 100 / height));
				out += (invert) ? graphs.at(current).at(height - i) : graphs.at(current).at(i-1);
			}
		}
		if (not color_gradient.empty()) out += Fx::reset;
	}

	Graph::Graph() {}

	Graph::Graph(int width, int height, const string& color_gradient,
				 const deque<long long>& data, const string& symbol,
				 bool invert, bool no_zero, long long max_value, long long offset)
	: width(width), height(height), color_gradient(color_gradient),
	  invert(invert), no_zero(no_zero), offset(offset) {
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
			graphs[(i % 2 != 0)].push_back((value_width < width) ? ((height == 1) ? Mv::r(1) : " "s) * (width - value_width) : "");
		}
		if (data.size() == 0) return;
		this->_create(data, data_offset);
	}

	string& Graph::operator()(const deque<long long>& data, bool data_same) {
		if (data_same) return out;

		//? Safety check: return empty if Graph wasn't properly initialized
		if (graphs.empty() or height == 0 or width == 0) return out;

		//? Make room for new characters on graph
		if (not tty_mode) current = not current;
		for (const int& i : iota(0, height)) {
			if (height == 1 and graphs.at(current).at(i).at(1) == '[') {
				if (graphs.at(current).at(i).at(3) == 'C') graphs.at(current).at(i).erase(0, 4);
				else graphs.at(current).at(i).erase(0, graphs.at(current).at(i).find_first_of('m') + 4);
			}
			else if (graphs.at(current).at(i).at(0) == ' ') graphs.at(current).at(i).erase(0, 1);
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
	float max_observed_pwr = 1.0f;

	int graph_up_height, graph_low_height;
	int graph_up_width, graph_low_width;
	int gpu_meter_width;
	bool shown = true, redraw = true, mid_line = false;
	string box;
	vector<Draw::Graph> graphs_upper;
	vector<Draw::Graph> graphs_lower;
	Draw::Meter cpu_meter;
	vector<Draw::Meter> gpu_meters;
	Draw::Meter ane_meter;
	vector<Draw::Graph> core_graphs;
	vector<Draw::Graph> temp_graphs;
	vector<Draw::Graph> gpu_temp_graphs;
	vector<Draw::Graph> gpu_mem_graphs;

    string draw(const cpu_info& cpu, const vector<Gpu::gpu_info>& gpus, bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		bool show_temps = (Config::getB("check_temp") and got_sensors);
		bool show_watts = (Config::getB("show_cpu_watts") and supports_watts);
		auto single_graph = Config::getB("cpu_single_graph");
		bool hide_cores = show_temps and (cpu_temp_only or not Config::getB("show_coretemp"));
		const int extra_width = (hide_cores ? max(6, 6 * b_column_size) : (b_columns == 1 && !show_temps) ? 8 : 0);
	#ifdef GPU_SUPPORT
		const auto& show_gpu_info = Config::getS("show_gpu_info");
		const bool gpu_always = show_gpu_info == "On";
		const bool gpu_auto = show_gpu_info == "Auto";
		const bool show_gpu = (gpus.size() > 0 and (gpu_always or (gpu_auto and Gpu::shown < Gpu::count)));
	#else
		(void)gpus;
	#endif
		auto graph_up_field = Config::getS("cpu_graph_upper");
		if (graph_up_field == "Auto" or not v_contains(Cpu::available_fields, graph_up_field))
			graph_up_field = "total";
		auto graph_lo_field = Config::getS("cpu_graph_lower");
		if (graph_lo_field == "Auto" or not v_contains(Cpu::available_fields, graph_lo_field)) {
		#ifdef GPU_SUPPORT
			graph_lo_field = show_gpu ? "gpu-totals" : graph_up_field;
		#else
			graph_lo_field = graph_up_field;
		#endif
		}
		auto tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_cpu"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(6);
		auto& temp_scale = Config::getS("temp_scale");
		auto cpu_bottom = Config::getB("cpu_bottom");

		const string& title_left = Theme::c("cpu_box") + (cpu_bottom ? Symbols::title_left_down : Symbols::title_left);
		const string& title_right = Theme::c("cpu_box") + (cpu_bottom ? Symbols::title_right_down : Symbols::title_right);
		static int bat_pos = 0, bat_len = 0;
		if (safeVal(cpu.cpu_percent, "total"s).empty()
			or safeVal(cpu.core_percent, 0).empty()
			or (show_temps and safeVal(cpu.temp, 0).empty())) return "";
		if (safeVal(cpu.cpu_percent, "total"s).empty()
			or safeVal(cpu.core_percent, 0).empty()
			or (show_temps and safeVal(cpu.temp, 0).empty())) return "";
		string out;
		out.reserve(width * height);

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			mid_line = (not single_graph and graph_up_field != graph_lo_field);
			graph_up_height = (single_graph ? height - 2 : ceil((double)(height - 2) / 2) - (mid_line and height % 2 != 0));
			graph_low_height = height - 2 - graph_up_height - mid_line;
			const int button_y = cpu_bottom ? y + height - 1 : y;
			out += box;

			//? Buttons on title
			out += Mv::to(button_y, x + 10) + title_left + Theme::c("hi_fg") + Fx::b + 'm' + Theme::c("title") + "enu" + Fx::ub + title_right;
			Input::mouse_mappings["m"] = {button_y, x + 11, 1, 4};
			out += Mv::to(button_y, x + 16) + title_left + Theme::c("hi_fg") + Fx::b + 'p' + Theme::c("title") + "reset "
				+ (Config::current_preset < 0 ? "*" : to_string(Config::current_preset)) + Fx::ub + title_right;
			Input::mouse_mappings["p"] = {button_y, x + 17, 1, 8};
			const string update = to_string(Config::getI("update_ms")) + "ms";
			out += Mv::to(button_y, x + width - update.size() - 8) + title_left + Fx::b + Theme::c("hi_fg") + "- " + Theme::c("title") + update
				+ Theme::c("hi_fg") + " +" + Fx::ub + title_right;
			Input::mouse_mappings["-"] = {button_y, x + width - (int)update.size() - 7, 1, 2};
			Input::mouse_mappings["+"] = {button_y, x + width - 5, 1, 2};

			// Draw container engine name
			if (Cpu::container_engine.has_value()) {
				fmt::format_to(std::back_inserter(out), "{}{}{}{}{}", Mv::to(button_y, x + 28), title_left, Theme::c("title"), Cpu::container_engine.value(), title_right);
			}

			//? Graphs & meters
			const int graph_default_width = x + width - b_width - 3;

			auto init_graphs = [&](vector<Draw::Graph>& graphs, const int graph_height, int& graph_width, const string& graph_field, bool invert) {
			#ifdef GPU_SUPPORT
				if (graph_field.starts_with("gpu")) {
					if (graph_field.find("totals") != string::npos) {
						graphs.resize(gpus.size());
						gpu_temp_graphs.resize(gpus.size());
						gpu_mem_graphs.resize(gpus.size());
						gpu_meters.resize(gpus.size());
						const int gpu_draw_count = gpu_always ? Gpu::count : Gpu::count - Gpu::shown;
						graph_width = gpu_draw_count <= 0 ? graph_default_width : graph_default_width/gpu_draw_count - gpu_draw_count + 1 + graph_default_width%gpu_draw_count;
						for (size_t i = 0; i < gpus.size(); i++) {
							if (gpu_auto and v_contains(Gpu::shown_panels, i))
								continue;
							auto& gpu = gpus[i]; auto& graph = graphs[i];

							//? GPU graphs
							if (gpu.supported_functions.gpu_utilization) {
								if (i + 1 < gpus.size()) {
									graph = Draw::Graph{graph_width, graph_height, "cpu", safeVal(gpu.gpu_percent, graph_field), graph_symbol, invert, true};
								}
								else {
									graph = Draw::Graph{
										graph_width + graph_default_width%graph_width - (int)gpus.size() + 1,
										graph_height, "cpu", safeVal(gpu.gpu_percent, graph_field), graph_symbol, invert, true
									};
								}
							}
						}
					} else {
						graphs.resize(1);
						graph_width = graph_default_width;
						graphs[0] = Draw::Graph{ graph_width, graph_height, "cpu", safeVal(Gpu::shared_gpu_percent, graph_field), graph_symbol, invert, true };
					}
				}
				else {
			#endif
					graphs.resize(1);
					graph_width = graph_default_width;
					graphs[0] = Draw::Graph{ graph_width, graph_height, "cpu", safeVal(cpu.cpu_percent, graph_field), graph_symbol, invert, true };
			#ifdef GPU_SUPPORT
				}
			#endif
			};

            init_graphs(graphs_upper, graph_up_height, graph_up_width, graph_up_field, false);
            if (not single_graph)
            	init_graphs(graphs_lower, graph_low_height, graph_low_width, graph_lo_field, Config::getB("cpu_invert_lower"));

				//? Calculate common meter width for CPU/GPU/ANE alignment in brief view
			//? Base: "XXXX " label (5 chars) + meter + value (up to 12 chars for "XXXXX C/s") + power (6 chars)
			int brief_meter_width = b_width - (show_temps ? 26 - (b_column_size <= 1 and b_columns == 1 ? 7 : 0) : 13);
			if (show_watts) {
				brief_meter_width -= 6;
			}

		#ifdef GPU_SUPPORT
			if (show_gpu and b_columns > 1) {
				gpu_temp_graphs.resize(gpus.size());
				gpu_mem_graphs.resize(gpus.size());
				gpu_meters.resize(gpus.size());

				// Shrink gpu graph width in small boxes to prevent line width extending past box border
				auto gpu_graph_width = b_width < 42 ? 4 : 5;

				for (size_t i = 0; i < gpus.size(); i++) {
					if (gpu_auto and v_contains(Gpu::shown_panels, i))
						continue;
					auto& gpu = gpus[i];

					//? GPU graphs/meters - use consistent width for alignment with CPU/ANE
					if (gpu.supported_functions.temp_info and show_temps) {
						gpu_temp_graphs[i] = Draw::Graph{ gpu_graph_width, 1, "temp", gpu.temp, graph_symbol, false, false, gpu.temp_max, -23 };
					}
					if (gpu.supported_functions.mem_used and gpu.supported_functions.mem_total and b_columns > 1) {
						gpu_mem_graphs[i] = Draw::Graph{ gpu_graph_width, 1, "used", safeVal(gpu.gpu_percent, "gpu-vram-totals"s), graph_symbol };
					}
					if (gpu.supported_functions.gpu_utilization) {
						gpu_meters[i] = Draw::Meter{brief_meter_width, "cpu"};
					}
				}
			}

			//? ANE meter for Apple Silicon - use same width as CPU/GPU for bar alignment
			if (Shared::aneCoreCount > 0 and b_columns > 1) {
				ane_meter = Draw::Meter{brief_meter_width, "cpu"};
			}
			#endif

			int cpu_meter_width = brief_meter_width;
			cpu_meter = Draw::Meter{cpu_meter_width, "cpu"};

			if (mid_line) {
				out += Mv::to(y + graph_up_height + 1, x) + Fx::ub + Theme::c("cpu_box") + Symbols::div_left + Theme::c("div_line")
					+ Symbols::h_line * (width - b_width - 2) + Symbols::div_right
					+ Mv::to(y + graph_up_height + 1, x + ((width - b_width) / 2) - ((graph_up_field.size() + graph_lo_field.size()) / 2) - 4)
					+ Theme::c("main_fg") + graph_up_field + Mv::r(1) + "▲▼" + Mv::r(1) + graph_lo_field;
			}

			if (b_column_size > 0 or extra_width > 0) {
				core_graphs.clear();
				for (const auto& core_data : cpu.core_percent) {
					core_graphs.emplace_back(5 * b_column_size + extra_width, 1, "cpu", core_data, graph_symbol);
				}
			}

			if (show_temps) {
				temp_graphs.clear();
				//? Main CPU temp graph: 6 chars to match GPU panel format
				temp_graphs.emplace_back(6, 1, "temp", safeVal(cpu.temp, 0), graph_symbol, false, false, cpu.temp_max, -23);
				if (not hide_cores and b_column_size > 1) {
					for (const auto& i : iota((size_t)1, cpu.temp.size())) {
						temp_graphs.emplace_back(5, 1, "temp", safeVal(cpu.temp, i), graph_symbol, false, false, cpu.temp_max, -23);
					}
				}
			}
		}

		//? Draw battery if enabled and present
		if (Config::getB("show_battery") and has_battery) {
			static int old_percent{};   // defaults to = 0
			static long old_seconds{};  // defaults to = 0
			static float old_watts{};	// defaults to = 0
			static string old_status;
			static Draw::Meter bat_meter {10, "cpu", true};
			static const std::unordered_map<string, string> bat_symbols = {
				{"charging", "▲"},
				{"discharging", "▼"},
				{"full", "■"},
				{"unknown", "○"}
			};

			const auto& [percent, watts, seconds, status] = current_bat;

			if (redraw or percent != old_percent or (watts != old_watts and Config::getB("show_battery_watts")) or seconds != old_seconds or status != old_status) {
				old_percent = percent;
				old_watts = watts;
				old_seconds = seconds;
				old_status = status;
				const string str_time = (seconds > 0 ? sec_to_dhms(seconds, false, true) : "");
				const string str_percent = to_string(percent) + '%';
				const string str_watts = (watts != -1 and Config::getB("show_battery_watts") ? fmt::format("{:.2f}", watts) + 'W' : "");
				const auto& bat_symbol = bat_symbols.at((bat_symbols.contains(status) ? status : "unknown"));
				const int current_len = (Term::width >= 100 ? 11 : 0) + str_time.size() + str_percent.size() + str_watts.size() + to_string(Config::getI("update_ms")).size();
				const int current_pos = Term::width - current_len - 17;

				if ((bat_pos != current_pos or bat_len != current_len) and bat_pos > 0 and not redraw)
					out += Mv::to(y, bat_pos) + Fx::ub + Theme::c("cpu_box") + Symbols::h_line * (bat_len + 4);
				bat_pos = current_pos;
				bat_len = current_len;

				out += Mv::to(y, bat_pos) + title_left + Theme::c("title") + Fx::b + "BAT" + bat_symbol + ' ' + str_percent
					+ (Term::width >= 100 ? Fx::ub + ' ' + bat_meter(percent) + Fx::b : "")
					+ (not str_time.empty() ? ' ' + Theme::c("title") + str_time : "") + (not str_watts.empty() ? " " + Theme::c("title") + Fx::b + str_watts : "") + Fx::ub + title_right;
			}
		}
		else if (bat_pos > 0) {
			out += Mv::to(y, bat_pos) + Fx::ub + Theme::c("cpu_box") + Symbols::h_line * (bat_len + 4);
			bat_pos = bat_len = 0;
		}

		try {
			//? Cpu/Gpu graphs
			out += Fx::ub + Mv::to(y + 1, x + 1);
			auto draw_graphs = [&](vector<Draw::Graph>& graphs, const int graph_height, const int graph_width, const string& graph_field) {
			#ifdef GPU_SUPPORT
				if (graph_field.starts_with("gpu"))
					if (graph_field.ends_with("totals")) {
						int gpu_drawn = 0;
						for (size_t i = 0; i < gpus.size(); i++) {
							if (gpu_auto and v_contains(Gpu::shown_panels, i)) {
								continue;
							}
							try {
								const auto& gpu_percent = gpus[i].gpu_percent;
								out += graphs[i](safeVal(gpu_percent, graph_field), (data_same or redraw));
							} catch (std::out_of_range& /* unused */) {
								continue;
							}
							if (Gpu::count - (gpu_auto ? Gpu::shown : 0) > 1) {
								auto i_str = to_string(i);
								out += Mv::l(graph_width-1) + Mv::u(graph_height/2) + (graph_width > 5 ? "GPU" : "") + i_str
									+ Mv::d(graph_height/2) + Mv::r(graph_width - 1 - (graph_width > 5)*3 - i_str.size());
							}

							if (++gpu_drawn < Gpu::count - (gpu_auto ? Gpu::shown : 0))
								out += Theme::c("div_line") + (Symbols::v_line + Mv::l(1) + Mv::u(1))*graph_height + Mv::r(1) + Mv::d(1);
						}
					}
					else
						out += graphs[0](safeVal(Gpu::shared_gpu_percent, graph_field), (data_same or redraw));
				else
			#else
				(void)graph_height;
				(void)graph_width;
			#endif
					out += graphs[0](safeVal(cpu.cpu_percent, graph_field), (data_same or redraw));
			};

			draw_graphs(graphs_upper, graph_up_height, graph_up_width, graph_up_field);
			if (not single_graph) {
				out += Mv::to(y + graph_up_height + 1 + mid_line, x + 1);
				draw_graphs(graphs_lower, graph_low_height, graph_low_width, graph_lo_field);
			}

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

		#ifdef __linux__
			const bool freq_range = Config::getS("freq_mode") == "range";
		#else
			const bool freq_range = false;
		#endif

			//? Cpu clock and cpu meter
			if (Config::getB("show_cpu_freq") and not cpuHz.empty())
				out += Mv::to(b_y, b_x + b_width - (freq_range ? 20 : 10)) + Fx::ub + Theme::c("div_line")
					+ Symbols::h_line * ((freq_range ? 17 : 7) - cpuHz.size())
					+ Symbols::title_left + Fx::b + Theme::c("title") + cpuHz + Fx::ub + Theme::c("div_line") + Symbols::title_right;

		//? CPU line format matches GPU panel: " CPU " + meter + 5-digit % + 6-char temp graph + temp
		out += Mv::to(b_y + 1, b_x + 1) + Theme::c("main_fg") + Fx::b + " CPU " + cpu_meter(safeVal(cpu.cpu_percent, "total"s).back());
		if (show_temps and Pwr::shown) {
			//? Right-align percentage when temp hidden - position at where temp section would end
			out += Mv::to(b_y + 1, b_x + b_width - 7);  //? Position for "  100%|"
		}
		out += Theme::g("cpu").at(clamp(safeVal(cpu.cpu_percent, "total"s).back(), 0ll, 100ll)) + rjust(to_string(safeVal(cpu.cpu_percent, "total"s).back()), 5) + Theme::c("main_fg") + '%';
		if (show_temps and not Pwr::shown) {
			const auto [temp, unit] = celsius_to(safeVal(cpu.temp, 0).back(), temp_scale);
			const auto temp_color = Theme::g("temp").at(clamp(safeVal(cpu.temp, 0).back(), 0ll, 100ll));  //? 100°C = max red
			//? Always show temp graph (matching GPU panel behavior)
			if (temp_graphs.size() >= 1ll)
				out += ' ' + Theme::c("inactive_fg") + graph_bg * 6 + Mv::l(6) + temp_color
					+ temp_graphs.at(0)(safeVal(cpu.temp, 0), data_same or redraw);
			out += temp_color + rjust(to_string(temp), 4) + Theme::c("main_fg") + unit;  //? Apply temp color to value
		}

		if (show_watts) {
			string cwatts = fmt::format(" {:>4.{}f}", cpu.usage_watts, cpu.usage_watts < 10.0f ? 2 : cpu.usage_watts < 100.0f ? 1 : 0);
			string cwatts_post = "W";

			max_observed_pwr = max(max_observed_pwr, cpu.usage_watts);
			out += Theme::g("cached").at(clamp(cpu.usage_watts / max_observed_pwr * 100.0f, 0.0f, 100.0f)) + cwatts + Theme::c("main_fg") + cwatts_post; 
		}

			out += Theme::c("div_line") + Symbols::v_line;
		} catch (const std::exception& e) {
			throw std::runtime_error("graphs, clock, meter : " + string{e.what()});
		}

		int max_row = b_height - 3; // Subtracting one extra row for the load average (and power if enabled)
		int n_gpus_to_show = 0;
	#ifdef GPU_SUPPORT
		n_gpus_to_show = show_gpu ? (gpus.size() - (gpu_always ? 0 : Gpu::shown)) : 0;
		//? Include ANE row in count when GPU panel is hidden
		if (show_gpu and Shared::aneCoreCount > 0) n_gpus_to_show++;
	#endif
		max_row -= n_gpus_to_show;

		auto is_cpu_enabled = [&cpu](const std::int32_t num) -> bool {
			return !cpu.active_cpus.has_value() || std::ranges::find(cpu.active_cpus.value(), num) != cpu.active_cpus.value().end();
		};

		//? Core text and graphs
		int cx = 0, cy = 1, cc = 0, core_width = (b_column_size == 0 ? 2 : 3);
		if (Shared::coreCount >= 100) core_width++;
		//? Determine if Apple Silicon with E-cores and P-cores
		const bool has_hybrid_cores = (Shared::eCoreCount > 0 or Shared::pCoreCount > 0);
		for (const auto& n : iota(0, Shared::coreCount)) {
			auto enabled = is_cpu_enabled(n);
			//? Use E/P prefix for Apple Silicon cores, C for all other systems
			char core_prefix = 'C';
			int display_num = n;  //? Core number to display
			if (has_hybrid_cores) {
				//? E-cores are first (0 to eCoreCount-1), P-cores follow (eCoreCount to coreCount-1)
				if (n < Shared::eCoreCount) {
					core_prefix = 'E';
					display_num = n;  //? E0, E1, E2, ...
				} else {
					core_prefix = 'P';
					display_num = n - Shared::eCoreCount;  //? P0, P1, P2, ... (relative to P-core start)
				}
			}
			out += Mv::to(b_y + cy + 1, b_x + cx + 1) + Theme::c(enabled ? "main_fg" : "inactive_fg") + (Shared::coreCount < 100 ? Fx::b + core_prefix + Fx::ub : "")
				+ ljust(to_string(display_num), core_width);
			if ((b_column_size > 0 or extra_width > 0) and cmp_less(n, core_graphs.size()))
				out += Theme::c("inactive_fg") + graph_bg * (5 * b_column_size + extra_width) + Mv::l(5 * b_column_size + extra_width)
					+ core_graphs.at(n)(safeVal(cpu.core_percent, n), data_same or redraw);

			out += enabled ? Theme::g("cpu").at(clamp(safeVal(cpu.core_percent, n).back(), 0ll, 100ll)) : Theme::c("inactive_fg");
			out += rjust(to_string(safeVal(cpu.core_percent, n).back()), (b_column_size < 2 ? 3 : 4)) + Theme::c(enabled ? "main_fg" : "inactive_fg") + '%';

			if (show_temps and not hide_cores) {
				const auto [temp, unit] = celsius_to(safeVal(cpu.temp, n+1).back(), temp_scale);
				const auto temp_color = enabled ? Theme::g("temp").at(clamp(safeVal(cpu.temp, n+1).back(), 0ll, 100ll)) : Theme::c("inactive_fg");  //? 100°C = max red
				if (b_column_size > 1 and std::cmp_greater_equal(temp_graphs.size(), n))
					out += ' ' + Theme::c("inactive_fg") + graph_bg * 5 + Mv::l(5)
						+ temp_graphs.at(n+1)(safeVal(cpu.temp, n+1), data_same or redraw);
				out += temp_color + rjust(to_string(temp), 4) + Theme::c(enabled ? "main_fg" : "inactive_fg") + unit;
			}

			out += Theme::c("div_line") + Symbols::v_line;

			if ((++cy > ceil((double)Shared::coreCount / b_columns) or cy == max_row) and n != Shared::coreCount - 1) {
				if (++cc >= b_columns) break;
				cy = 1; cx = (b_width / b_columns) * cc;
			}
		}

		//? Load average
		if (cy < b_height - 1 and cc <= b_columns) {
			cy = b_height - 2 - n_gpus_to_show;

			string load_avg_pre = "Load avg:";
			string load_avg;

			for (const auto& val : cpu.load_avg) {
				load_avg += fmt::format(" {:.2f}", val);
			}

			int len = load_avg_pre.size() + load_avg.size();
			out += Mv::to(b_y + cy, b_x + 1) + string(max(b_width - len - 2, 0), ' ') + Theme::c("main_fg") + Fx::b + load_avg_pre + Fx::ub + load_avg;
		}

	#ifdef GPU_SUPPORT
		//? Gpu brief info
		if (show_gpu) {
			for (unsigned long i = 0; i < gpus.size(); ++i) {
				if (gpu_auto and v_contains(Gpu::shown_panels, i))
					continue;
				out += Mv::to(b_y + ++cy, b_x + 1) + Theme::c("main_fg") + Fx::b + "GPU";
				if (gpus.size() > 1) out += rjust(to_string(i), 1 + (gpus.size() > 9));
				if (gpus[i].supported_functions.gpu_utilization) {
					out += ' ';
					if (b_columns > 1) {
						out += gpu_meters[i](safeVal(gpus[i].gpu_percent, "gpu-totals"s).back());
					}
					//? Percentage right-aligned
					out += Mv::to(b_y + cy, b_x + b_width - 17)
						+ Theme::g("cpu").at(clamp(safeVal(gpus[i].gpu_percent, "gpu-totals"s).back(), 0ll, 100ll))
						+ rjust(to_string(safeVal(gpus[i].gpu_percent, "gpu-totals"s).back()), 3) + Theme::c("main_fg") + '%';
				}
				//? Temperature (only when PWR panel is hidden)
				if (show_temps and not Pwr::shown and gpus[i].supported_functions.temp_info) {
					const auto [temp, unit] = celsius_to(gpus[i].temp.back(), temp_scale);
					out += Mv::to(b_y + cy, b_x + b_width - 12)
						+ Theme::g("temp").at(clamp(gpus[i].temp.back(), 0ll, 100ll))
						+ rjust(to_string(temp), 3) + Theme::c("main_fg") + unit;
				}
				//? Power right-aligned at end
				if (gpus[i].supported_functions.pwr_usage) {
					out += Mv::to(b_y + cy, b_x + b_width - 6)
						+ Theme::g("cached").at(clamp(safeVal(gpus[i].gpu_percent, "gpu-pwr-totals"s).back(), 0ll, 100ll))
						+ fmt::format("{:>4.1f}", gpus[i].pwr_usage / 1000.0) + Theme::c("main_fg") + 'W';
				}

				if (cy > b_height - 1) break;
			}
		}

		//? ANE (Neural Engine) brief info for Apple Silicon
		if (show_gpu and Shared::aneCoreCount > 0 and cy < b_height - 1) {
			out += Mv::to(b_y + ++cy, b_x + 1) + Theme::c("main_fg") + Fx::b + "ANE";

			//? Format ANE activity
			string ane_activity_str;
			double ane_activity = Shared::aneActivity;
			if (ane_activity >= 1000000.0) {
				ane_activity_str = fmt::format("{:.0f}M", ane_activity / 1000000.0);
			} else if (ane_activity >= 1000.0) {
				ane_activity_str = fmt::format("{:.0f}K", ane_activity / 1000.0);
			} else {
				ane_activity_str = fmt::format("{:.0f}", ane_activity);
			}

			long long ane_percent = static_cast<long long>(std::min(100.0, (ane_activity / 650.0) * 100.0));

			out += ' ';
			if (b_columns > 1) {
				out += ane_meter(ane_percent);
			}

			//? Activity right-aligned (digit aligns with GPU %)
			out += Mv::to(b_y + cy, b_x + b_width - 17)
				+ Theme::g("cpu").at(clamp(ane_percent, 0ll, 100ll))
				+ rjust(ane_activity_str, 3) + Theme::c("main_fg") + " C/s";

			//? Power right-aligned at end
			out += Mv::to(b_y + cy, b_x + b_width - 6)
				+ Theme::g("cpu").at(clamp(static_cast<long long>(Shared::anePower * 10), 0ll, 100ll))
				+ fmt::format("{:>4.1f}", Shared::anePower) + Theme::c("main_fg") + 'W';
		}
	#endif

		redraw = false;
		return out + Fx::reset;
	}

}

#ifdef GPU_SUPPORT
namespace Gpu {
	int width_p = 100, height_p = 32;
	int min_width = 41, min_height = 8;
	int width = 41, total_height;
	vector<int> x_vec = {}, y_vec = {}, b_height_vec = {};
	int b_width;
	vector<int> b_x_vec = {}, b_y_vec = {};
	vector<bool> redraw = {};
	int shown = 0;
	int count = 0;
	bool ane_split = false;  //? Toggle for split GPU/ANE braille graph (Apple Silicon, key "6")
	vector<int> shown_panels = {};
	int graph_up_height;
	vector<Draw::Graph> graph_upper_vec = {}, graph_lower_vec = {};
	vector<Draw::Graph> ane_graph_vec = {};  //? ANE braille graph for split view (key "6")
	vector<Draw::Graph> temp_graph_vec = {};
	vector<Draw::Graph> mem_used_graph_vec = {}, mem_util_graph_vec = {};
	vector<Draw::Meter> gpu_meter_vec = {};
	vector<Draw::Graph> pwr_graph_vec = {};  //? Power braille graph (auto-scales)
	vector<Draw::Meter> enc_meter_vec = {};
	vector<Draw::Meter> ane_meter_vec = {};
	vector<string> box = {};

    string draw(const gpu_info& gpu, unsigned long index, bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";

		//? Bounds check for vector access
		if (index >= ane_graph_vec.size() or index >= graph_upper_vec.size()) return "";

		auto& b_x = b_x_vec[index];
		auto& b_y = b_y_vec[index];
		auto& x = x_vec[index];
		auto& y = y_vec[index];

		auto& graph_upper = graph_upper_vec[index];
		auto& graph_lower = graph_lower_vec[index];
		auto& ane_graph = ane_graph_vec[index];
		auto& temp_graph = temp_graph_vec[index];
		auto& mem_used_graph = mem_used_graph_vec[index];
		auto& mem_util_graph = mem_util_graph_vec[index];
		auto& gpu_meter = gpu_meter_vec[index];
		auto& pwr_graph = pwr_graph_vec[index];
		auto& enc_meter = enc_meter_vec[index];
		auto& ane_meter = ane_meter_vec[index];

		if (force_redraw) redraw[index] = true;
        bool show_temps = gpu.supported_functions.temp_info and (Config::getB("check_temp"));
        auto tty_mode = Config::getB("tty_mode");
		auto& temp_scale = Config::getS("temp_scale");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_gpu"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(6);
        auto single_graph = !Config::getB("gpu_mirror_graph");
		string out;
		int height = gpu_b_height_offsets[index] + 4;
		out.reserve(width * height);

		//* Redraw elements not needed to be updated every cycle
		if (redraw[index]) {
			out += box[index];

			//? When ane_split is active (Apple Silicon, key "6"), force split mode: GPU top, ANE bottom
			bool use_ane_split = ane_split and Shared::aneCoreCount > 0;
			bool is_split = not single_graph or use_ane_split;

			graph_up_height = is_split ? (b_height_vec[index] + 1) / 2 : b_height_vec[index];
			int graph_low_height = is_split ? b_height_vec[index] - graph_up_height : 0;

			if (gpu.supported_functions.gpu_utilization) {
				graph_upper = Draw::Graph{x + width - b_width - 3, graph_up_height, "cpu", safeVal(gpu.gpu_percent, "gpu-totals"s), graph_symbol, false, true}; // TODO cpu -> gpu

				if (use_ane_split and not Gpu::shared_gpu_percent.at("ane-activity").empty()) {
					//? ANE graph in lower portion (Apple Silicon split mode)
					ane_graph = Draw::Graph{
						x + width - b_width - 3,
						graph_low_height, "cpu",
						Gpu::shared_gpu_percent.at("ane-activity"),
						graph_symbol,
						Config::getB("cpu_invert_lower"), true
					};
				} else if (use_ane_split) {
					//? ANE split requested but no data yet - use empty graph with single 0 value
					deque<long long> empty_data = {0};
					ane_graph = Draw::Graph{
						x + width - b_width - 3,
						graph_low_height, "cpu",
						empty_data,
						graph_symbol,
						Config::getB("cpu_invert_lower"), true
					};
				} else if (not single_graph) {
					//? Mirrored GPU graph in lower portion
                	graph_lower = Draw::Graph{
                    	x + width - b_width - 3,
                    	graph_low_height, "cpu",
                    	safeVal(gpu.gpu_percent, "gpu-totals"s),
                    	graph_symbol,
                    	Config::getB("cpu_invert_lower"), true
                	};
            	}
				//? GPU meter: "  GPU " label is 6 chars (to align with " ⁶ANE ")
				gpu_meter = Draw::Meter{b_width - (show_temps ? 27 : 14), "cpu"};
			}
			if (gpu.supported_functions.temp_info)
				temp_graph = Draw::Graph{6, 1, "temp", gpu.temp, graph_symbol, false, false, gpu.temp_max, -23};
			if (gpu.supported_functions.pwr_usage)
				pwr_graph = Draw::Graph{b_width - 14, 1, "cached", gpu.pwr, graph_symbol, false, false, gpu.pwr_max_usage, -23};
			if (gpu.supported_functions.mem_utilization)
				mem_util_graph = Draw::Graph{b_width/2 - 1, 2, "free", gpu.mem_utilization_percent, graph_symbol, 0, 0, 100, 4}; // offset so the graph isn't empty at 0-5% utilization
			if (gpu.supported_functions.mem_used and gpu.supported_functions.mem_total)
				mem_used_graph = Draw::Graph{b_width/2 - 2, 2 + 2*(gpu.supported_functions.mem_utilization), "used", safeVal(gpu.gpu_percent, "gpu-vram-totals"s), graph_symbol};
			if (gpu.supported_functions.encoder_utilization)
				enc_meter = Draw::Meter{b_width/2 - 10, "cpu"};
			//? ANE meter: " ⁶ANE " label is 6 chars (same as "  GPU " and "  PWR ")
			//? ANE needs 4 chars less for meter to account for " C/s" suffix (vs GPU's "%" suffix)
			if (Shared::aneCoreCount > 0)
				ane_meter = Draw::Meter{b_width - (show_temps ? 31 : 18), "cpu"};
		}


		//* General GPU info
		int rows_used = 1;
		//? Gpu graph, meter & clock speed
		if (gpu.supported_functions.gpu_utilization) {
			out += Fx::ub + Mv::to(y + rows_used, x + 1) + graph_upper(safeVal(gpu.gpu_percent, "gpu-totals"s), (data_same or redraw[index]));

			//? Lower graph: ANE (when ane_split, key "6") or mirrored GPU (when gpu_mirror_graph)
			if (ane_split and Shared::aneCoreCount > 0) {
				auto& ane_data = Gpu::shared_gpu_percent.at("ane-activity");
				if (not ane_data.empty()) {
					out += Mv::to(y + rows_used + graph_up_height, x + 1) + ane_graph(ane_data, (data_same or redraw[index]));
				}
				//? Draw mid-line with "gpu ▲▼ ane" label AFTER graphs (so it's not overwritten)
				out += Mv::to(y + graph_up_height + 1, x) + Fx::ub + Theme::c("cpu_box") + Symbols::div_left + Theme::c("div_line")
					+ Symbols::h_line * (width - b_width - 2) + Symbols::div_right
					+ Mv::to(y + graph_up_height + 1, x + ((width - b_width) / 2) - 5)
					+ Theme::c("main_fg") + "gpu" + Mv::r(1) + "▲▼" + Mv::r(1) + "ane";
			} else if (not single_graph) {
				out += Mv::to(y + rows_used + graph_up_height, x + 1) + graph_lower(safeVal(gpu.gpu_percent, "gpu-totals"s), (data_same or redraw[index]));
			}

			//? "  GPU " = 6 chars to align with " ⁶ANE " (also 6 chars visually)
			out += Mv::to(b_y + rows_used, b_x + 1) + Theme::c("main_fg") + Fx::b + "  GPU " + gpu_meter(safeVal(gpu.gpu_percent, "gpu-totals"s).back());
			if (show_temps and Pwr::shown) {
				//? Right-align percentage when temp hidden - position at where temp section would end
				out += Mv::to(b_y + rows_used, b_x + b_width - 7);  //? Position for "  100%|"
			}
			out += Theme::g("cpu").at(clamp(safeVal(gpu.gpu_percent, "gpu-totals"s).back(), 0ll, 100ll)) + rjust(to_string(safeVal(gpu.gpu_percent, "gpu-totals"s).back()), 5) + Theme::c("main_fg") + '%';

			//? Temperature graph, I assume the device supports utilization if it supports temperature
			if (show_temps and not Pwr::shown) {
				const auto [temp, unit] = celsius_to(gpu.temp.back(), temp_scale);
				const auto temp_color = Theme::g("temp").at(clamp(gpu.temp.back(), 0ll, 100ll));  //? 100°C = max red
				out += ' ' + Theme::c("inactive_fg") + graph_bg * 6 + Mv::l(6) + temp_color
					+ temp_graph(gpu.temp, data_same or redraw[index]);
				out += temp_color + rjust(to_string(temp), 4) + Theme::c("main_fg") + unit;  //? Apply temp color to value
			}
			out += Theme::c("div_line") + Symbols::v_line;
			rows_used++;
		}

		if (gpu.supported_functions.gpu_clock) {
			string clock_speed_string = to_string(gpu.gpu_clock_speed);
			out += Mv::to(b_y, b_x + b_width - 12) + Theme::c("div_line") + Symbols::h_line*(5-clock_speed_string.size())
				+ Symbols::title_left + Fx::b + Theme::c("title") + clock_speed_string + " MHz" + Fx::ub + Theme::c("div_line") + Symbols::title_right;
		}

		//? Power usage with braille graph (auto-scales based on observed max)
		if (gpu.supported_functions.pwr_usage) {
			int pwr_graph_width = b_width - 14;  //? -1 for extra space in "  PWR "
			long long pwr_pct = gpu.pwr_max_usage > 0 ? clamp(gpu.pwr_usage * 100 / gpu.pwr_max_usage, 0ll, 100ll) : 0;
			//? "  PWR " = 6 chars to align with " ⁶ANE "
			out += Mv::to(b_y + rows_used, b_x + 1) + Theme::c("main_fg") + Fx::b + "  PWR "
				+ Theme::c("inactive_fg") + string(pwr_graph_width, ' ') + Mv::l(pwr_graph_width)
				+ Theme::g("cached").at(clamp(pwr_pct, 0ll, 100ll))
				+ pwr_graph(gpu.pwr, data_same or redraw[index])
				+ fmt::format("{:>5.{}f}", gpu.pwr_usage / 1000.0, gpu.pwr_usage < 10'000 ? 2 : gpu.pwr_usage < 100'000 ? 1 : 0) + Theme::c("main_fg") + 'W';
			if (gpu.supported_functions.pwr_state and gpu.pwr_state != 32) // NVML_PSTATE_UNKNOWN; unsupported or non-nvidia card
				out += std::string(" P-state: ") + (gpu.pwr_state > 9 ? "" : " ") + 'P' + Theme::g("cached").at(clamp(gpu.pwr_state, 0ll, 100ll)) + to_string(gpu.pwr_state);
			rows_used++;
		}

		//? Encode and Decode meters
		bool drawnEncDec = gpu.supported_functions.encoder_utilization and gpu.supported_functions.decoder_utilization;
		if (drawnEncDec) {
			out += Mv::to(b_y + rows_used, b_x +1) + Theme::c("main_fg") + Fx::b + "ENC " + enc_meter(gpu.encoder_utilization)
				+ Theme::g("cpu").at(clamp(gpu.encoder_utilization, 0ll, 100ll)) + rjust(to_string(gpu.encoder_utilization), 4) + Theme::c("main_fg") + '%'
				+ Theme::c("div_line") + Symbols::v_line + Theme::c("main_fg") + Fx::b + "DEC " + enc_meter(gpu.decoder_utilization)
				+ Theme::g("cpu").at(clamp(gpu.decoder_utilization, 0ll, 100ll)) + rjust(to_string(gpu.decoder_utilization), 4) + Theme::c("main_fg") + '%';
			rows_used++;
		}

		//? ANE (Neural Engine) activity and power for Apple Silicon
		if (Shared::aneCoreCount > 0) {
			//? Format ANE activity as commands/second with appropriate suffix
			string ane_activity_str;
			double ane_activity = Shared::aneActivity;
			if (ane_activity >= 1000000.0) {
				ane_activity_str = fmt::format("{:>5.1f}M", ane_activity / 1000000.0);
			} else if (ane_activity >= 1000.0) {
				ane_activity_str = fmt::format("{:>5.1f}K", ane_activity / 1000.0);
			} else {
				ane_activity_str = fmt::format("{:>6.0f}", ane_activity);
			}

			//? Calculate ANE activity as percentage (0-100) for meter display (max 650 C/s)
			long long ane_percent = static_cast<long long>(std::min(100.0, (ane_activity / 650.0) * 100.0));

			//? Add tiny "6" hint when ane_split is available (Apple Silicon with single GPU)
			//? Use "  ANE " format to align with "  GPU " (same 6-char width)
			string ane_label = " ";  //? Leading space for alignment
			if (Gpu::count == 1) {
				ane_label += Theme::c("hi_fg") + (tty_mode ? "6" : Symbols::superscript.at(6)) + Theme::c("main_fg");
			} else {
				ane_label += " ";  //? Extra space when no superscript to match "  GPU "
			}
			ane_label += "ANE ";
			out += Mv::to(b_y + rows_used, b_x + 1) + Theme::c("main_fg") + Fx::b + ane_label + ane_meter(ane_percent);

			//? Position activity value to align with GPU percentage (right-justified before power)
			out += Mv::to(b_y + rows_used, b_x + b_width - 18)
				+ Theme::g("cpu").at(clamp(ane_percent, 0ll, 100ll)) + ane_activity_str + Theme::c("main_fg") + " C/s";

			//? ANE power (right-aligned at box edge, 7 chars from right: "X.XXW" + border)
			out += Mv::to(b_y + rows_used, b_x + b_width - 7)
				+ Theme::g("cpu").at(clamp(static_cast<long long>(Shared::anePower * 10), 0ll, 100ll))
				+ fmt::format("{:>5.2f}", Shared::anePower) + Theme::c("main_fg") + 'W';
			rows_used++;
		}

		if (gpu.supported_functions.mem_total or gpu.supported_functions.mem_used) {
			out += Mv::to(b_y + rows_used, b_x);
			if (gpu.supported_functions.mem_total and gpu.supported_functions.mem_used) {
				string used_memory_string = floating_humanizer(gpu.mem_used);

				auto offset = (gpu.supported_functions.mem_total or gpu.supported_functions.mem_used)
					* (1 + 2*(gpu.supported_functions.mem_total and gpu.supported_functions.mem_used) + 2*gpu.supported_functions.mem_utilization);

				//? Used graph, memory section header, total vram
				out += Theme::c("div_line") + Symbols::div_left + Symbols::h_line + Symbols::title_left + Fx::b + Theme::c("title") + "vram" + Theme::c("div_line") + Fx::ub + Symbols::title_right
					+  Symbols::h_line*(b_width/2-8) + Symbols::div_up + Mv::d(offset)+Mv::l(1) + Symbols::div_down + Mv::l(1)+Mv::u(1) + (Symbols::v_line + Mv::l(1)+Mv::u(1))*(offset-1) + Symbols::div_up
					+  Symbols::h_line + Theme::c("title") + "Used:" + Theme::c("div_line")
					+  Symbols::h_line*(b_width/2+b_width%2-9-used_memory_string.size()) + Theme::c("title") + used_memory_string + Theme::c("div_line") + Symbols::h_line + Symbols::div_right
					+  Mv::d(1) + Mv::l(b_width/2-1) + mem_used_graph(safeVal(gpu.gpu_percent, "gpu-vram-totals"s), (data_same or redraw[index]))
					+  Mv::l(b_width-3) + Mv::u(1+2*gpu.supported_functions.mem_utilization) + Theme::c("main_fg") + Fx::b + "Total:" + rjust(floating_humanizer(gpu.mem_total), b_width/2-9) + Fx::ub
					+  Mv::r(3) + rjust(to_string(safeVal(gpu.gpu_percent, "gpu-vram-totals"s).back()), 3) + '%';

				//? Memory utilization
				if (gpu.supported_functions.mem_utilization)
					out += Mv::l(b_width/2+6) + Mv::d(1) + Theme::c("div_line") + Symbols::div_left+Symbols::h_line + Theme::c("title") + "Utilization:" + Theme::c("div_line") + Symbols::h_line*(b_width/2-14) + Symbols::div_right
						+  Mv::l(b_width/2)   + Mv::d(1) + mem_util_graph(gpu.mem_utilization_percent, (data_same or redraw[index]))
						+  Mv::l(b_width/2-1) + Mv::u(1) + rjust(to_string(gpu.mem_utilization_percent.back()), 3) + '%';

				//? Memory clock speed
				if (gpu.supported_functions.mem_clock) {
					string clock_speed_string = to_string(gpu.mem_clock_speed);
					out += Mv::to(b_y + rows_used, b_x + b_width/2 - 11) + Theme::c("div_line") + Symbols::h_line*(5-clock_speed_string.size())
						+ Symbols::title_left + Fx::b + Theme::c("title") + clock_speed_string + " MHz" + Fx::ub + Theme::c("div_line") + Symbols::title_right;
				}
			} else {
				out += Theme::c("main_fg") + Mv::r(1);
				if (gpu.supported_functions.mem_total)
					out += "VRAM total:" + rjust(floating_humanizer(gpu.mem_total), b_width/(1 + gpu.supported_functions.mem_clock)-14);
				else out += "VRAM usage:" + rjust(floating_humanizer(gpu.mem_used), b_width/(1 + gpu.supported_functions.mem_clock)-14);

				if (gpu.supported_functions.mem_clock)
					out += "   VRAM clock:" + rjust(to_string(gpu.mem_clock_speed) + " MHz", b_width/2-13);
			}
		}

		//? Processes section header
		//out += Mv::to(b_y+8, b_x) + Theme::c("div_line") + Symbols::div_left + Symbols::h_line + Symbols::title_left + Theme::c("main_fg") + Fx::b + "gpu-proc" + Fx::ub + Theme::c("div_line")
		//	+ Symbols::title_right + Symbols::h_line*(b_width/2-12) + Symbols::div_down + Symbols::h_line*(b_width/2-2) + Symbols::div_right;

		//? PCIe link throughput
		if (gpu.supported_functions.pcie_txrx and Config::getB("nvml_measure_pcie_speeds")) {
			string tx_string = floating_humanizer(gpu.pcie_tx, 0, 1, 0, 1);
			string rx_string = floating_humanizer(gpu.pcie_rx, 0, 1, 0, 1);
			out += Mv::to(b_y + b_height_vec[index] - 1, b_x+2) + Theme::c("div_line")
				+ Symbols::title_left_down + Theme::c("title") + Fx::b + "TX:" + Fx::ub + Theme::c("div_line") + Symbols::title_right_down + Symbols::h_line*(b_width/2-9-tx_string.size())
				+ Symbols::title_left_down + Theme::c("title") + Fx::b + tx_string + Fx::ub + Theme::c("div_line") + Symbols::title_right_down + (gpu.supported_functions.mem_total and gpu.supported_functions.mem_used ? Symbols::div_down : Symbols::h_line)
				+ Symbols::title_left_down + Theme::c("title") + Fx::b + "RX:" + Fx::ub + Theme::c("div_line") + Symbols::title_right_down + Symbols::h_line*(b_width/2+b_width%2-9-rx_string.size())
				+ Symbols::title_left_down + Theme::c("title") + Fx::b + rx_string + Fx::ub + Theme::c("div_line") + Symbols::title_right_down + Symbols::round_right_down;
		}

		redraw[index] = false;
		return out + Fx::reset;
	}

}

namespace Pwr {
	//? Local variables for layout calculations (not declared in btop_shared.hpp)
	int width_p = 100, height_p = 25;

	//? Subpanel dimensions
	int sub_width = 0;  //? Width of each of the 3 subpanels
	int graph_height = 1;  //? Height of braille graphs (dynamic)

	//? Graphs for power history
	Draw::Graph cpu_pwr_graph, gpu_pwr_graph, ane_pwr_graph;

	string draw(bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";
		if (not shown) return "";

		if (force_redraw) redraw = true;

		auto tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_pwr"));
		string out;
		out.reserve(width * height);

		//? Calculate subpanel width (3 equal columns)
		sub_width = (width - 4) / 3;
		int graph_width = sub_width - 4;
		if (graph_width < 6) graph_width = 6;

		//? Calculate graph height based on panel height
		//? Layout: header(1) + label+temp(1) + graph(dynamic) + values(1) + border(1)
		graph_height = max(1, height - 5);

		//? Redraw structural elements on resize/force
		if (redraw) {
			out += box;

			//? Create/update graphs with dynamic height
			cpu_pwr_graph = Draw::Graph{graph_width, graph_height, "cached", cpu_pwr_history, graph_symbol, false, false, cpu_pwr_max, -23};
			gpu_pwr_graph = Draw::Graph{graph_width, graph_height, "cached", gpu_pwr_history, graph_symbol, false, false, gpu_pwr_max, -23};
			ane_pwr_graph = Draw::Graph{graph_width, graph_height, "cached", ane_pwr_history, graph_symbol, false, false, ane_pwr_max, -23};
		}

		//? Get current power values
		double cpu_pwr = Shared::cpuPower;
		double gpu_pwr = Shared::gpuPower;
		double ane_pwr = Shared::anePower;
		double total_pwr = cpu_pwr + gpu_pwr + ane_pwr;

		//? Get temperatures from Shared namespace
		long long cpu_temp = Shared::cpuTemp;
		long long gpu_temp = Shared::gpuTemp;

		//? Header row: Total power with avg/max
		out += Mv::to(y + 1, x + 2)
			+ Theme::c("title") + Fx::b
			+ fmt::format("Power: {:.2f}W", total_pwr)
			+ Theme::c("main_fg") + Fx::ub
			+ fmt::format(" (avg {:.2f}W, max {:.2f}W)",
				Shared::cpuPowerAvg + Shared::gpuPowerAvg + Shared::anePowerAvg,
				Shared::cpuPowerPeak + Shared::gpuPowerPeak + Shared::anePowerPeak);

		//? Draw vertical dividers between subpanels
		int div1_x = x + sub_width + 1;
		int div2_x = x + sub_width * 2 + 2;
		for (int r = 2; r < height - 1; r++) {
			out += Mv::to(y + r, div1_x) + Theme::c("div_line") + Symbols::v_line;
			out += Mv::to(y + r, div2_x) + Theme::c("div_line") + Symbols::v_line;
		}

		//? CPU Subpanel (left)
		int col1_x = x + 2;
		int row = 2;
		out += Mv::to(y + row, col1_x) + Theme::c("main_fg") + Fx::b + "CPU" + Fx::ub;
		//? CPU temperature with color gradient (100°C = max red)
		{
			long long temp_pct = cpu_temp > 0 ? clamp(cpu_temp, 0ll, 100ll) : 0;
			out += Mv::to(y + row, col1_x + sub_width - 7)
				+ Theme::g("temp").at(temp_pct) + fmt::format("{:>3}°C", cpu_temp);
		}

		row++;
		//? CPU power graph (multi-row) - Graph returns complete multi-line output
		out += Mv::to(y + row, col1_x)
			+ cpu_pwr_graph(cpu_pwr_history, data_same or redraw);

		row += graph_height;
		out += Mv::to(y + row, col1_x) + Theme::c("main_fg")
			+ fmt::format("{:.2f}W avg {:.2f}W", cpu_pwr, Shared::cpuPowerAvg);

		//? GPU Subpanel (middle)
		int col2_x = div1_x + 2;
		row = 2;
		out += Mv::to(y + row, col2_x) + Theme::c("main_fg") + Fx::b + "GPU" + Fx::ub;
		//? GPU temperature with color gradient (100°C = max red)
		if (gpu_temp > 0) {
			long long temp_pct = clamp(gpu_temp, 0ll, 100ll);
			out += Mv::to(y + row, col2_x + sub_width - 7)
				+ Theme::g("temp").at(temp_pct) + fmt::format("{:>3}°C", gpu_temp);
		}

		row++;
		//? GPU power graph (multi-row) - Graph returns complete multi-line output
		out += Mv::to(y + row, col2_x)
			+ gpu_pwr_graph(gpu_pwr_history, data_same or redraw);

		row += graph_height;
		out += Mv::to(y + row, col2_x) + Theme::c("main_fg")
			+ fmt::format("{:.2f}W avg {:.2f}W", gpu_pwr, Shared::gpuPowerAvg);

		//? ANE Subpanel (right)
		int col3_x = div2_x + 2;
		row = 2;
		out += Mv::to(y + row, col3_x) + Theme::c("main_fg") + Fx::b + "ANE" + Fx::ub;

		row++;
		//? ANE power graph (multi-row) - Graph returns complete multi-line output
		out += Mv::to(y + row, col3_x)
			+ ane_pwr_graph(ane_pwr_history, data_same or redraw);

		row += graph_height;
		out += Mv::to(y + row, col3_x) + Theme::c("main_fg")
			+ fmt::format("{:.2f}W avg {:.2f}W", ane_pwr, Shared::anePowerAvg);

		redraw = false;
		return out + Fx::reset;
	}
}
#endif

namespace Mem {
	int width_p = 45, height_p = 36;
	int min_width = 36, min_height = 10;
	int x = 1, y, width = 20, height;
	int mem_width, disks_width, divider, item_height, mem_size, mem_meter, graph_height, graph_height_remainder, disk_meter;
	int disks_io_h = 0;
	int disks_io_half = 0;
	bool shown = true, redraw = true;
	int disk_start = 0, disk_selected = 0, disk_select_max = 0, num_disks = 0;
	string box;
	std::unordered_map<string, Draw::Meter> mem_meters;
	std::unordered_map<string, Draw::Graph> mem_graphs;
	std::unordered_map<string, Draw::Meter> disk_meters_used;
	std::unordered_map<string, Draw::Meter> disk_meters_free;
	std::unordered_map<string, Draw::Graph> io_graphs;

	//? Disk selection/scrolling function - returns new selection or -1 if unchanged
	int disk_selection(const std::string_view cmd_key, int num_disks) {
		auto start = Config::getI("disk_start");
		auto selected = Config::getI("disk_selected");
		auto vim_keys = Config::getB("vim_keys");

		if ((cmd_key == "up" or (vim_keys and cmd_key == "k")) and selected > 0) {
			if (start > 0 and selected == 1) start--;
			else selected--;
		}
		else if (cmd_key == "mouse_scroll_up" and start > 0) {
			start = max(0, start - 1);
		}
		else if (cmd_key == "mouse_scroll_down" and start < num_disks - disk_select_max) {
			start = min(num_disks - disk_select_max, start + 1);
		}
		else if (cmd_key == "down" or (vim_keys and cmd_key == "j")) {
			if (start < num_disks - disk_select_max and selected == disk_select_max) start++;
			else if (selected == 0) selected = 1;
			else selected++;
		}
		else if (cmd_key == "page_up") {
			if (selected > 0 and start == 0) selected = 1;
			else start = max(0, start - disk_select_max);
		}
		else if (cmd_key == "page_down") {
			if (selected > 0 and start >= num_disks - disk_select_max) selected = disk_select_max;
			else start = clamp(start + disk_select_max, 0, max(0, num_disks - disk_select_max));
		}
		else if (cmd_key == "home" or (vim_keys and cmd_key == "g")) {
			start = 0;
			if (selected > 0) selected = 1;
		}
		else if (cmd_key == "end" or (vim_keys and cmd_key == "G")) {
			start = max(0, num_disks - disk_select_max);
			if (selected > 0) selected = min(disk_select_max, num_disks);
		}

		//? Clamp selection to valid range
		if (selected > min(disk_select_max, num_disks - start)) selected = min(disk_select_max, num_disks - start);

		bool changed = false;
		if (start != Config::getI("disk_start")) {
			Config::set("disk_start", start);
			changed = true;
		}
		if (selected != Config::getI("disk_selected")) {
			Config::set("disk_selected", selected);
			changed = true;
		}
		disk_start = start;
		disk_selected = selected;
		return (not changed ? -1 : selected);
	}

	string draw(const mem_info& mem, bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		auto show_swap = Config::getB("show_swap");
		auto swap_disk = Config::getB("swap_disk");
		auto show_disks = Config::getB("show_disks");
		auto show_io_stat = Config::getB("show_io_stat");
		auto io_mode = Config::getB("io_mode");
		auto io_graph_combined = Config::getB("io_graph_combined");
		auto use_graphs = Config::getB("mem_graphs");
		auto tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_mem"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(6);
		auto totalMem = Mem::get_totalMem();
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

			//? Mem graphs and meters - create with per-item heights for remainder distribution
			{
				vector<string> all_mem_names(mem_names.begin(), mem_names.end());
				if (show_swap and has_swap) {
					all_mem_names.insert(all_mem_names.end(), swap_names.begin(), swap_names.end());
				}
				const int num_items = (int)all_mem_names.size();
				const int extra_threshold = num_items - graph_height_remainder;
				int idx = 0;
				for (const auto& name : all_mem_names) {
					//? Last N items get +1 height (where N = remainder)
					const int item_graph_height = graph_height + ((idx >= extra_threshold and graph_height_remainder > 0) ? 1 : 0);
					idx++;
					if (use_graphs) {
						const string graph_name = (name.starts_with("swap_") ? name.substr(5) : name);
						mem_graphs[name] = Draw::Graph{mem_meter, item_graph_height, graph_name, safeVal(mem.percent, name), graph_symbol};
					}
					else {
						const string meter_name = (name.starts_with("swap_") ? name.substr(5) : name);
						mem_meters[name] = Draw::Meter{mem_meter, meter_name};
					}
				}
			}

			//? Disk meters and io graphs
			if (show_disks) {
				if (show_io_stat or io_mode) {
					std::unordered_map<string, int> custom_speeds;
					int half_height = 0;
					if (io_mode) {
						disks_io_h = max((int)floor((double)(height - 2 - (disk_ios * 2)) / max(1, disk_ios)), (io_graph_combined ? 1 : 2));
						half_height = ceil((double)disks_io_h / 2);

						if (not Config::getS("io_graph_speeds").empty()) {
							auto split = ssplit(Config::getS("io_graph_speeds"));
							for (const auto& entry : split) {
								auto vals = ssplit(entry, ':');
								if (vals.size() == 2 and mem.disks.contains(vals.at(0)) and isint(vals.at(1)))
									try {
										custom_speeds[vals.at(0)] = std::stoi(vals.at(1));
									}
									catch (const std::out_of_range&) { continue; }
							}
						}
					}

					for (const auto& [name, disk] : mem.disks) {
						if (disk.io_read.empty()) continue;

						io_graphs[name + "_activity"] = Draw::Graph{disks_width - 6, 1, "available", disk.io_activity, graph_symbol};

						if (io_mode) {
							//? Create one combined graph for IO read/write if enabled
							long long speed = (custom_speeds.contains(name) ? custom_speeds.at(name) : 100) << 20;
							if (io_graph_combined) {
								deque<long long> combined(disk.io_read.size(), 0);
								rng::transform(disk.io_read, disk.io_write, combined.begin(), std::plus<long long>());
								io_graphs[name] = Draw::Graph{
									disks_width, disks_io_h, "available", combined,
									graph_symbol, false, true, speed};
							}
							else {
								io_graphs[name + "_read"] = Draw::Graph{
									disks_width, half_height, "free",
									disk.io_read, graph_symbol, false,
									true, speed};
								io_graphs[name + "_write"] = Draw::Graph{
									disks_width, disks_io_h - half_height,
									"used", disk.io_write, graph_symbol,
									true, true, speed};
							}
						}
					}
				}

				for (int i = 0; const auto& [name, ignored] : mem.disks) {
					if (i * 2 > height - 2) break;
					disk_meters_used[name] = Draw::Meter{disk_meter, "used"};
					//? Always create Free meters - scrolling handles overflow
					disk_meters_free[name] = Draw::Meter{disk_meter, "free"};
				}

				out += Mv::to(y, x + width - 6) + Fx::ub + Theme::c("mem_box") + Symbols::title_left + (io_mode ? Fx::b : "") + Theme::c("hi_fg")
				+ 'i' + Theme::c("title") + 'o' + Fx::ub + Theme::c("mem_box") + Symbols::title_right;
				Input::mouse_mappings["i"] = {y, x + width - 5, 1, 2};
			}

		}

		//? Mem and swap
		int cx = 1, cy = 1;
		string divider = (graph_height > 0 ? Mv::l(2) + Theme::c("mem_box") + Symbols::div_left + Theme::c("div_line") + Symbols::h_line * (mem_width - 1)
						+ (show_disks ? "" : Theme::c("mem_box")) + Symbols::div_right + Mv::l(mem_width - 1) + Theme::c("main_fg") : "");
		bool big_mem = mem_width > 21;

		out += Mv::to(y + 1, x + 2) + Theme::c("title") + Fx::b + "Total:" + rjust(floating_humanizer(totalMem), mem_width - 9) + Fx::ub + Theme::c("main_fg");
		vector<string> comb_names (mem_names.begin(), mem_names.end());
		if (show_swap and has_swap and not swap_disk) comb_names.insert(comb_names.end(), swap_names.begin(), swap_names.end());
		//? Distribute remainder lines to LAST items to fill bottom properly
		const int num_mem_items = (int)comb_names.size();
		const int extra_threshold = num_mem_items - graph_height_remainder;  //? Items at or after this index get +1
		int item_index = 0;
		for (const auto& name : comb_names) {
			if (cy > height - 2) break;  //? Use full height (height - 2 accounts for border)
			string title;
			if (name == "swap_used") {
				if (cy > height - 3) break;
				if (height - cy > 4) {
					if (graph_height > 0) out += Mv::to(y+1+cy, x+1+cx) + divider;
					cy += 1;
				}
				out += Mv::to(y+1+cy, x+1+cx) + Theme::c("title") + Fx::b + "Swap:" + rjust(floating_humanizer(safeVal(mem.stats, "swap_total"s)), mem_width - 8)
					+ Theme::c("main_fg") + Fx::ub;
				cy += 1;
				title = "Used";
			}
			else if (name == "swap_free")
				title = "Free";

			if (title.empty()) title = capitalize(name);
			const string humanized = floating_humanizer(safeVal(mem.stats, name));
			const int offset = max(0, divider.empty() ? 9 - (int)humanized.size() : 0);
			const string graphics = (
				use_graphs and mem_graphs.contains(name) ? mem_graphs.at(name)(safeVal(mem.percent, name), redraw or data_same)
				: mem_meters.contains(name) ? mem_meters.at(name)(safeVal(mem.percent, name).back())
				: "");
			//? Calculate per-item graph height (last N items get +1 where N = remainder)
			const int this_graph_extra = (item_index >= extra_threshold and graph_height_remainder > 0) ? 1 : 0;
			const int this_graph_height = graph_height + this_graph_extra;
			//? Calculate up movement based on actual graph height for this item
			const string up = (this_graph_height >= 2 ? Mv::l(mem_width - 2) + Mv::u(this_graph_height - 1) : "");
			item_index++;
			if (mem_size > 2) {
				out += Mv::to(y+1+cy, x+1+cx) + divider + title.substr(0, big_mem ? 10 : 5) + ":"
					+ Mv::to(y+1+cy, x+cx + mem_width - 2 - humanized.size()) + (divider.empty() ? Mv::l(offset) + string(" ") * offset + humanized : trans(humanized))
					+ Mv::to(y+2+cy, x+cx + (this_graph_height >= 2 ? 0 : 1)) + graphics + up + rjust(to_string(safeVal(mem.percent, name).back()) + "%", 4);
				cy += (graph_height == 0 ? 2 : this_graph_height + 1);
			}
			else {
				out += Mv::to(y+1+cy, x+1+cx) + ljust(title, (mem_size > 1 ? 5 : 1)) + (this_graph_height >= 2 ? "" : " ")
					+ graphics + Theme::c("title") + rjust(humanized, (mem_size > 1 ? 9 : 7));
				cy += (graph_height == 0 ? 1 : this_graph_height);
			}
		}
		//? Add final divider if there's remaining space (closes off the last section)
		if (graph_height > 0 and cy < height - 2)
			out += Mv::to(y+1+cy, x+1+cx) + divider;

		//? Disks
		if (show_disks) {
			const auto& disks = mem.disks;
			cx = mem_width; cy = 0;
			bool big_disk = disks_width >= 25;
			divider = Mv::l(1) + Theme::c("div_line") + Symbols::div_left + Symbols::h_line * disks_width + Theme::c("mem_box") + Fx::ub + Symbols::div_right + Mv::l(disks_width);
			const string hu_div = Theme::c("div_line") + Symbols::h_line + Theme::c("main_fg");

			//? Calculate how many disks can fit and set up scrolling
			//? Use max lines_per_disk for conservative scroll bounds (prevents scrolling past valid positions)
			//? Actual fitting is determined by cy checks in render loop (disk images use 3 lines, regular use 4 with IO)
			num_disks = (int)mem.disks_order.size();
			const int max_lines_per_disk = show_io_stat ? 4 : 3;  //? Max lines any disk could use
			disk_select_max = max(1, (height - 2) / max_lines_per_disk);
			disk_start = Config::getI("disk_start");
			disk_selected = Config::getI("disk_selected");

			//? Clamp scroll position to valid range
			if (disk_start > max(0, num_disks - disk_select_max)) {
				disk_start = max(0, num_disks - disk_select_max);
				Config::set("disk_start", disk_start);
			}
			if (disk_selected > min(disk_select_max, num_disks)) {
				disk_selected = min(disk_select_max, num_disks);
				Config::set("disk_selected", disk_selected);
			}

			//? Track actual visible count for scroll indicators
			int actual_visible_count = 0;

			//? Show scroll indicator at top if scrolled down
			bool has_more_above = disk_start > 0;
			bool has_more_below = false;  //? Updated after rendering

			if (io_mode) {
				int disk_index = 0;
				int visible_index = 0;
				//? In io_mode, each disk needs: name(1) + activity(1) + io_graph(disks_io_h)
				const int io_mode_lines_per_disk = 2 + disks_io_h;
				for (const auto& mount : mem.disks_order) {
					if (not disks.contains(mount)) continue;
					disk_index++;
					if (disk_index <= disk_start) continue;  //? Skip disks before scroll position
					const auto disk = safeVal(disks, mount);
					if (disk.io_read.empty()) continue;  //? Skip disks without IO data in io_mode
					//? Check if there's enough space for this complete io_mode disk entry
					if (cy + io_mode_lines_per_disk > height - 2) break;
					visible_index++;
					const string total = floating_humanizer(disk.total, not big_disk);
					//? Highlight selected disk
					const bool is_selected = (disk_selected > 0 and visible_index == disk_selected);
					const string title_color = is_selected ? Theme::c("hi_fg") : Theme::c("title");
					out += Mv::to(y+1+cy, x+1+cx) + divider + title_color + Fx::b + uresize(disk.name, disks_width - 8) + Mv::to(y+1+cy, x+cx + disks_width - total.size())
						+ trans(total) + Fx::ub;
					if (big_disk) {
						const string used_percent = to_string(disk.used_percent);
						out += Mv::to(y+1+cy, x+1+cx + round((double)disks_width / 2) - round((double)used_percent.size() / 2) - 1) + hu_div + used_percent + '%' + hu_div;
					}
					if (io_graphs.contains(mount + "_activity")) {
					out += Mv::to(y+2+cy++, x+1+cx) + (big_disk ? " IO% " : " IO   " + Mv::l(2)) + Theme::c("inactive_fg") + graph_bg * (disks_width - 6)
						+ Mv::l(disks_width - 6) + io_graphs.at(mount + "_activity")(disk.io_activity, redraw or data_same) + Theme::c("main_fg");
					}
					cy++;  //? Advance to IO graph row (space already verified at loop start)
					if (io_graph_combined) {
						if (not io_graphs.contains(mount)) continue;
						auto comb_val = disk.io_read.back() + disk.io_write.back();
						const string humanized = (disk.io_write.back() > 0 ? "▼"s : ""s) + (disk.io_read.back() > 0 ? "▲"s : ""s)
												+ (comb_val > 0 ? Mv::r(1) + floating_humanizer(comb_val, true) : "RW");
						if (disks_io_h == 1) out += Mv::to(y+1+cy, x+1+cx) + string(5, ' ');
						out += Mv::to(y+1+cy, x+1+cx) + io_graphs.at(mount)({comb_val}, redraw or data_same)
							+ Mv::to(y+1+cy, x+1+cx) + Theme::c("main_fg") + humanized;
						cy += disks_io_h;
					}
					else {
						if (not io_graphs.contains(mount + "_read") or not io_graphs.contains(mount + "_write")) continue;
						const string human_read = (disk.io_read.back() > 0 ? "▲" + floating_humanizer(disk.io_read.back(), true) : "R");
						const string human_write = (disk.io_write.back() > 0 ? "▼" + floating_humanizer(disk.io_write.back(), true) : "W");
						if (disks_io_h <= 3) out += Mv::to(y+1+cy, x+1+cx) + string(5, ' ') + Mv::to(y+cy + disks_io_h, x+1+cx) + string(5, ' ');
						out += Mv::to(y+1+cy, x+1+cx) + io_graphs.at(mount + "_read")(disk.io_read, redraw or data_same) + Mv::l(disks_width)
							+ Mv::d(1) + io_graphs.at(mount + "_write")(disk.io_write, redraw or data_same)
							+ Mv::to(y+1+cy, x+1+cx) + human_read + Mv::to(y+cy + disks_io_h, x+1+cx) + human_write;
						cy += disks_io_h;
					}
				}
				actual_visible_count = visible_index;
			}
			else {
				int disk_index = 0;
				int visible_index = 0;
				for (const auto& mount : mem.disks_order) {
					if (not disks.contains(mount)) continue;
					disk_index++;
					if (disk_index <= disk_start) continue;  //? Skip disks before scroll position

					const auto disk = safeVal(disks, mount);
					if (disk.name.empty() or not disk_meters_used.contains(mount)) continue;

					//? Calculate lines needed for THIS disk (disk images don't have IO stats)
					const bool disk_has_io = show_io_stat and not disk.io_read.empty() and io_graphs.contains(mount + "_activity");
					const int lines_needed = disk_has_io ? 4 : 3;  //? name + [IO] + used + free

					//? Check if there's enough space for this complete disk
					if (cy + lines_needed > height - 2) break;
					visible_index++;

					auto comb_val = (not disk.io_read.empty() ? disk.io_read.back() + disk.io_write.back() : 0ll);
					const string human_io = (comb_val > 0 ? (disk.io_write.back() > 0 and big_disk ? "▼"s : ""s) + (disk.io_read.back() > 0 and big_disk ? "▲"s : ""s)
											+ floating_humanizer(comb_val, true) : "");
					const string human_total = floating_humanizer(disk.total, not big_disk);
					const string human_used = floating_humanizer(disk.used, not big_disk);
					const string human_free = floating_humanizer(disk.free, not big_disk);

					//? Highlight selected disk
					const bool is_selected = (disk_selected > 0 and visible_index == disk_selected);
					const string title_color = is_selected ? Theme::c("hi_fg") : Theme::c("title");
					out += Mv::to(y+1+cy, x+1+cx) + divider + title_color + Fx::b + uresize(disk.name, disks_width - 8) + Mv::to(y+1+cy, x+cx + disks_width - human_total.size())
						+ trans(human_total) + Fx::ub + Theme::c("main_fg");
					if (big_disk and not human_io.empty())
						out += Mv::to(y+1+cy, x+1+cx + round((double)disks_width * 2 / 3) - round((double)human_io.size() / 2) - 1) + hu_div + human_io + hu_div;
					cy++;

					if (disk_has_io) {
						out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " IO% " : " IO   " + Mv::l(2)) + Theme::c("inactive_fg") + graph_bg * (disks_width - 6) + Theme::g("available").at(clamp(disk.io_activity.back(), 50ll, 100ll))
							+ Mv::l(disks_width - 6) + io_graphs.at(mount + "_activity")(disk.io_activity, redraw or data_same) + Theme::c("main_fg");
						if (not big_disk) out += Mv::to(y+1+cy, x+cx+1) + Theme::c("main_fg") + human_io;
						cy++;
					}

					out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " Used:" + rjust(to_string(disk.used_percent) + '%', 4) : "U") + ' '
						+ disk_meters_used.at(mount)(disk.used_percent) + rjust(human_used, (big_disk ? 9 : 5));
					cy++;

					//? Always show Free row for visible disks
					if (disk_meters_free.contains(mount)) {
						out += Mv::to(y+1+cy, x+1+cx) + (big_disk ? " Free:" + rjust(to_string(disk.free_percent) + '%', 4) : "F") + ' '
						+ disk_meters_free.at(mount)(disk.free_percent) + rjust(human_free, (big_disk ? 9 : 5));
						cy++;
					}

				}
				actual_visible_count = visible_index;
			}
			//? No trailing divider - use full panel height for disks

			//? Clear remaining rows to prevent ghost content from previous renders
			const string clear_line = string(disks_width, ' ');
			while (cy < height - 2) {
				out += Mv::to(y+1+cy, x+1+cx) + clear_line;
				cy++;
			}

			//? Determine if there are more disks below based on actual visible count
			has_more_below = disk_start + actual_visible_count < num_disks;

			//? Show scroll indicators if there are hidden disks
			//? Both arrows on bottom border, side by side (↑↓) at right edge
			if (has_more_above or has_more_below) {
				const string scroll_ind = Theme::c("hi_fg") + Fx::b;
				const int scroll_x = x + cx + disks_width - 2;  //? Right side of disk panel
				if (has_more_above)
					out += Mv::to(y + height - 1, scroll_x - 1) + scroll_ind + Symbols::up + Fx::ub;  //? ↑ next to ↓
				if (has_more_below)
					out += Mv::to(y + height - 1, scroll_x) + scroll_ind + Symbols::down + Fx::ub;
			}
		}

		redraw = false;
		return out + Fx::reset;
	}

}

namespace Net {
	int width_p = 45, height_p = 32;
	int min_width = 36, min_height = 6;
	int x = 1, y, width = 20, height;
	int b_x, b_y, b_width, b_height, d_graph_height, u_graph_height;
	bool shown = true, redraw = true;
	const int MAX_IFNAMSIZ = 15;
	string old_ip;
	std::unordered_map<string, Draw::Graph> graphs;
	string box;

	string draw(const net_info& net, bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";
		if (force_redraw) redraw = true;
		auto net_sync = Config::getB("net_sync");
		auto net_auto = Config::getB("net_auto");
		auto tty_mode = Config::getB("tty_mode");
		auto swap_upload_download = Config::getB("swap_upload_download");
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
		const int i_size = min((int)selected_iface.size(), MAX_IFNAMSIZ);
		const long long down_max = (net_auto ? safeVal(graph_max, "download"s) : ((long long)(Config::getI("net_download")) << 20) / 8);
		const long long up_max = (net_auto ? safeVal(graph_max, "upload"s) : ((long long)(Config::getI("net_upload")) << 20) / 8);

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			out = box;
			//? Graphs
			graphs.clear();
			if (safeVal(net.bandwidth, "download"s).empty() or safeVal(net.bandwidth, "upload"s).empty())
				return out + Fx::reset;

			graphs["download"] = Draw::Graph{
				width - b_width - 2, u_graph_height, "download",
				net.bandwidth.at("download"), graph_symbol,
				swap_upload_download, true, down_max};
			graphs["upload"] = Draw::Graph{
				width - b_width - 2, d_graph_height, "upload",
				net.bandwidth.at("upload"), graph_symbol, !swap_upload_download, true, up_max};

			//? Interface selector and buttons

			out += Mv::to(y, x+width - i_size - 9) + title_left + Fx::b + Theme::c("hi_fg") + Symbols::left + "b " + Theme::c("title")
				+ uresize(selected_iface, MAX_IFNAMSIZ) + Theme::c("hi_fg") + " n" + Symbols::right + title_right
				+ Mv::to(y, x+width - i_size - 15) + title_left + Theme::c("hi_fg") + (safeVal(net.stat, "download"s).offset + safeVal(net.stat, "upload"s).offset > 0 ? Fx::b : "") + 'z'
				+ Theme::c("title") + "ero" + title_right;
			Input::mouse_mappings["b"] = {y, x+width - i_size - 8, 1, 3};
			Input::mouse_mappings["n"] = {y, x+width - 6, 1, 3};
			Input::mouse_mappings["z"] = {y, x+width - i_size - 14, 1, 4};
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
		if (not ip_addr.empty() and cmp_greater(width - i_size - 36, ip_addr.size())) {
			out += Mv::to(y, x + 8) + title_left + Theme::c("title") + Fx::b + ip_addr + title_right;
		}

		//? Graphs and stats
		for (const string dir : {"download", "upload"}) {
			//         |  upload  |  download  |
			// no swap |  bottom  |     top    |
			//  swap   |    top   |   bottom   |
			// XNOR operation (==)
			if ((not swap_upload_download and dir == "download") or (swap_upload_download and dir == "upload")) {
				out += Mv::to(y+1, x + 1);
			} else {
				out += Mv::to(y + u_graph_height + 1 + ((height * swap_upload_download) % 2), x + 1);
			}
			out += graphs.at(dir)(safeVal(net.bandwidth, dir), redraw or data_same or not net.connected)
				+ Mv::to(y+1 + (((dir == "upload") == (!swap_upload_download)) * (height - 3)), x + 1) + Fx::ub + Theme::c("graph_text")
				+ floating_humanizer((dir == "upload" ? up_max : down_max), true);
			const string speed = floating_humanizer(safeVal(net.stat, dir).speed, false, 0, false, true);
			const string speed_bits = (b_width >= 20 ? floating_humanizer(safeVal(net.stat, dir).speed, false, 0, true, true) : "");
			const string top = floating_humanizer(safeVal(net.stat, dir).top, false, 0, true, true);
			const string total = floating_humanizer(safeVal(net.stat, dir).total);
			const string symbol = (dir == "upload" ? "▲" : "▼");
			if ((swap_upload_download and dir == "upload") or (not swap_upload_download and dir == "download")) {
				// Top graph
				out += Mv::to(b_y+1, b_x+1) + Fx::ub + Theme::c("main_fg") + symbol + ' ' + ljust(speed, 10) + (b_width >= 20 ? rjust('(' + speed_bits + ')', 13) : "");
				if (b_height >= 8)
					out += Mv::to(b_y+2, b_x+1) + symbol + ' ' + "Top: " + rjust('(' + top, (b_width >= 20 ? 17 : 9)) + ')';
				if (b_height >= 6)
					out += Mv::to(b_y+2 + (b_height >= 8), b_x+1) + symbol + ' ' + "Total: " + rjust(total, (b_width >= 20 ? 16 : 8));
			} else {
				// Bottom graph
				out += Mv::to(b_y + b_height - (b_height / 2), b_x + 1) + Fx::ub + Theme::c("main_fg") + symbol + ' ' + ljust(speed, 10) + (b_width >= 20 ? rjust('(' + speed_bits + ')', 13) : "");
				if (b_height >= 8)
					out += Mv::to(b_y + b_height - (b_height / 2) + 1, b_x + 1) + symbol + ' ' + "Top: " + rjust('(' + top, (b_width >= 20 ? 17 : 9)) + ')';
				if (b_height >= 6)
					out += Mv::to(b_y + b_height - (b_height / 2) + 1 + (b_height >= 8), b_x + 1) + symbol + ' ' + "Total: " + rjust(total, (b_width >= 20 ? 16 : 8));
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
	bool is_last_process_in_list = false;
	int selected_pid = 0, selected_depth = 0;
	int scroll_pos;
	string selected_name;
	std::unordered_map<size_t, Draw::Graph> p_graphs;
	std::unordered_map<size_t, bool> p_wide_cmd;
	std::unordered_map<size_t, int> p_counters;
	int counter = 0;
	Draw::TextEdit filter;
	Draw::Graph detailed_cpu_graph;
	Draw::Graph detailed_mem_graph;
	int user_size, thread_size, prog_size, cmd_size, tree_size;
	int dgraph_x, dgraph_width, d_width, d_x, d_y;

	string box;

	int selection(const std::string_view cmd_key) {
		auto start = Config::getI("proc_start");
		auto selected = Config::getI("proc_selected");
		auto last_selected = Config::getI("proc_last_selected");
		int select_max = (Config::getB("show_detailed") ? (Config::getB("proc_banner_shown") ? Proc::select_max - 9 : Proc::select_max - 8) :
																(Config::getB("proc_banner_shown") ? Proc::select_max - 1 : Proc::select_max));

		if (Config::getB("follow_process")) {
			if (selected == 0) selected = Config::getI("proc_followed");;
			if (not Config::getB("pause_proc_list")) {
				Config::flip("follow_process");
				Config::set("followed_pid", 0);
				Config::set("proc_followed", 0);
				select_max++;
			}
			redraw = true;
		}

		auto vim_keys = Config::getB("vim_keys");

		int numpids = Proc::numpids;
		if ((cmd_key == "up" or (vim_keys and cmd_key == "k")) and selected > 0) {
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
		else if (cmd_key == "down" or (vim_keys and cmd_key == "j")) {
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
		else if (cmd_key == "home" or (vim_keys and cmd_key == "g")) {
			start = 0;
			if (selected > 0) selected = 1;
		}
		else if (cmd_key == "end" or (vim_keys and cmd_key == "G")) {
			start = max(0, numpids - select_max);
			if (selected > 0) selected = select_max;
		}
		else if (cmd_key.starts_with("mousey")) {
			int mouse_y = std::atoi(cmd_key.substr(6).data());
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

	string draw(const vector<proc_info>& plist, bool force_redraw, bool data_same) {
		if (Runner::stopping) return "";
		auto proc_tree = Config::getB("proc_tree");
		bool show_detailed = (Config::getB("show_detailed") and cmp_equal(Proc::detailed.last_pid, Config::getI("detailed_pid")));
		bool proc_gradient = (Config::getB("proc_gradient") and not Config::getB("lowcolor") and Theme::gradients.contains("proc"));
		auto proc_colors = Config::getB("proc_colors");
		auto tty_mode = Config::getB("tty_mode");
		auto& graph_symbol = (tty_mode ? "tty" : Config::getS("graph_symbol_proc"));
		auto& graph_bg = Symbols::graph_symbols.at((graph_symbol == "default" ? Config::getS("graph_symbol") + "_up" : graph_symbol + "_up")).at(6);
		auto mem_bytes = Config::getB("proc_mem_bytes");
		auto vim_keys = Config::getB("vim_keys");
		auto show_graphs = Config::getB("proc_cpu_graphs");
		const auto pause_proc_list = Config::getB("pause_proc_list");
		auto follow_process = Config::getB("follow_process"); 
		int followed_pid = Config::getI("followed_pid");
		int followed = Config::getI("proc_followed");
		auto proc_banner_shown = pause_proc_list or follow_process;
		Config::set("proc_banner_shown", proc_banner_shown);
		start = Config::getI("proc_start");
		selected = Config::getI("proc_selected");
		const int y = show_detailed ? Proc::y + 8 : Proc::y;
		const int height = show_detailed ? Proc::height - 8 : Proc::height;
		const int select_max = show_detailed ? (proc_banner_shown ? Proc::select_max - 9 : Proc::select_max - 8) : 
												(proc_banner_shown ? Proc::select_max - 1 : Proc::select_max);
		auto totalMem = Mem::get_totalMem();
		int numpids = Proc::numpids;
		if (force_redraw) redraw = true;
		string out;
		out.reserve(width * height);

		//? Move current selection/view to the selected process when a process should be followed
		if (follow_process and (not pause_proc_list or Config::getB("update_following"))) {
			Config::set("update_following", false);
			int loc = 1;
			bool can_follow = false;
			for (auto& p : plist) {
				if (p.filtered or (proc_tree and p.tree_index == plist.size())) continue;
				if (p.pid == (size_t)followed_pid) {
					can_follow = true;
					break;
				}
				loc++;
			}

			if (can_follow) {
				start = max(0, loc - (select_max / 2));
				followed = loc < (select_max / 2) ? loc : start > numpids - select_max ? select_max - numpids + loc : select_max / 2;
				Config::set("proc_followed", followed);
				selected = followed_pid != Config::getI("detailed_pid") ? followed : 0;
			}
			else {
				Config::set("followed_pid", followed_pid = 0);
				Config::set("follow_process", follow_process = false);
				Config::set("proc_banner_shown", proc_banner_shown = pause_proc_list);
				Config::set("proc_followed", 0);
			}
		}

		//? redraw if selection reaches or leaves the end of the list
		if (selected != Config::getI("proc_last_selected")) {
			if (selected >= select_max and start >= numpids - select_max) {
				redraw = true;
				is_last_process_in_list = true;
			}
			else if (is_last_process_in_list) {
				redraw = true;
				is_last_process_in_list = false;
			}
		}

		//* Redraw elements not needed to be updated every cycle
		if (redraw) {
			out = box;
			const string title_left = Theme::c("proc_box") + Symbols::title_left;
			const string title_right = Theme::c("proc_box") + Symbols::title_right;
			const string title_left_down = Theme::c("proc_box") + Symbols::title_left_down;
			const string title_right_down = Theme::c("proc_box") + Symbols::title_right_down;
			for (const auto& key : {"t", "K", "k", "s", "N", "F", "enter", "info_enter"})
				if (Input::mouse_mappings.contains(key)) Input::mouse_mappings.erase(key);

			//? Adapt sizes of text fields
			user_size = (width < 75 ? 5 : 10);
			thread_size = (width < 75 ? - 1 : 4);
			prog_size = (width > 70 ? 16 : ( width > 55 ? 8 : width - user_size - thread_size - 33));
			cmd_size = (width > 55 ? width - prog_size - user_size - thread_size - 33 : -1);
			tree_size = width - user_size - thread_size - 23;
			if (not show_graphs) {
				cmd_size += 5;
				tree_size += 5;
			}

			//? Detailed box
			if (show_detailed) {
				bool alive = detailed.status != "Dead";
				dgraph_x = x;
				dgraph_width = max(width / 3, width - 121);
				d_width = width - dgraph_width - 1;
				d_x = x + dgraph_width + 1;
				d_y = Proc::y;

				//? Create cpu and mem graphs if process is alive
				if (alive or pause_proc_list) {
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

				const string t_color = (not alive or selected > 0 ? Theme::c("inactive_fg") : Theme::c("title"));
				const string hi_color = (not alive or selected > 0 ? t_color : Theme::c("hi_fg"));
				int mouse_x = d_x + 2;
				out += Mv::to(d_y, d_x + 1);
				if (width > 55) {
					out += Fx::ub + title_left + hi_color + Fx::b + 't' + t_color + "erminate" + Fx::ub + title_right;
					if (alive and selected == 0) Input::mouse_mappings["t"] = {d_y, mouse_x, 1, 9};
					mouse_x += 11;
				}
				out += title_left + hi_color + Fx::b + (vim_keys ? 'K' : 'k') + t_color + "ill" + Fx::ub + title_right
					+ title_left + hi_color + Fx::b + 's' + t_color + "ignals" + Fx::ub + title_right
					+ title_left + hi_color + Fx::b + 'N' + t_color + "ice" + Fx::ub + title_right;
				if (alive and selected == 0) {
					Input::mouse_mappings[vim_keys ? "K" : "k"] = {d_y, mouse_x, 1, 4};
					mouse_x += 6;
					Input::mouse_mappings["s"] = {d_y, mouse_x, 1, 7};
				    mouse_x += 9;
					Input::mouse_mappings["N"] = {d_y, mouse_x, 1, 5};
				    mouse_x += 7;
				}
				if (width > 77) {
				    fmt::format_to(std::back_inserter(out), "{}{}{}{}{}{}{}{}",
				    	title_left, follow_process ? Fx::b : "",
				    	hi_color, 'F', t_color, "ollow",
				    	Fx::ub, title_right);
				    if (selected == 0) Input::mouse_mappings["F"] = {d_y, mouse_x, 1, 6};
				}

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
				const auto san_cmd = replace_ascii_control(detailed.entry.cmd);
				const int cmd_size = ulen(san_cmd, true);
				for (int num_lines = min(3, (int)ceil((double)cmd_size / (d_width - 5))), i = 0; i < num_lines; i++) {
					out += Mv::to(d_y + 5 + (num_lines == 1 ? 1 : i), d_x + 3)
						+ cjust(luresize(san_cmd, cmd_size - (d_width - 5) * i, true), d_width - 5, true, true);
				}

			}

			//? Filter
			auto filtering = Config::getB("proc_filtering"); // ? filter(20) : Config::getS("proc_filter"))
			const auto filter_text = (filtering) ? filter(max(6, width - 66)) : uresize(Config::getS("proc_filter"), max(6, width - 66));
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

			//? pause, per-core, reverse, tree and sorting
			const auto& sorting = Config::getS("proc_sorting");
			const int sort_len = sorting.size();
			const int sort_pos = x + width - sort_len - 8;

			if (width > 60 + sort_len) {
			    fmt::format_to(std::back_inserter(out), "{}{}{}{}{}{}{}{}{}{}{}",
					Mv::to(y, sort_pos - 32), title_left, pause_proc_list ? Fx::b : "",
					Theme::c("title"), "pa", Theme::c("hi_fg"), 'u', Theme::c("title"), "se",
			    	Fx::ub, title_right);
			    Input::mouse_mappings["u"] = {y, sort_pos - 31, 1, 5};
			}
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
			out += Mv::to(y, sort_pos) + title_left + Fx::b + Theme::c("hi_fg") + Symbols::left + " " + Theme::c("title") + sorting + " " + Theme::c("hi_fg")
				+ Symbols::right + Fx::ub + title_right;
				Input::mouse_mappings["left"] = {y, sort_pos + 1, 1, 2};
				Input::mouse_mappings["right"] = {y, sort_pos + sort_len + 3, 1, 2};

			//? select, info, signal, and follow buttons
			const string down_button = (is_last_process_in_list ? Theme::c("inactive_fg") : Theme::c("hi_fg")) + Symbols::down;
			const string t_color = (selected == 0 ? Theme::c("inactive_fg") : Theme::c("title"));
			const string hi_color = (selected == 0 ? Theme::c("inactive_fg") : Theme::c("hi_fg"));
			int mouse_x = x + 14;
			out += Mv::to(y + height - 1, x + 1) + title_left_down + Fx::b + hi_color + Symbols::up + Theme::c("title") + " select " + down_button + Fx::ub + title_right_down
				+ title_left_down + Fx::b + t_color + "info " + hi_color + Symbols::enter + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings["info_enter"] = {y + height - 1, mouse_x, 1, 6};
				mouse_x += 8;
			if (width > 60) {
				out += title_left_down + Fx::b + hi_color + 't' + t_color + "erminate" + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings["t"] = {y + height - 1, mouse_x, 1, 9};
				mouse_x += 11;
			}
			if (width > 55) {
				out += title_left_down + Fx::b + hi_color + (vim_keys ? 'K' : 'k') + t_color + "ill" + Fx::ub + title_right_down;
				if (selected > 0) Input::mouse_mappings[vim_keys ? "K" : "k"] = {y + height - 1, mouse_x, 1, 4};
				mouse_x += 6;
			}
			out += title_left_down + Fx::b + hi_color + 's' + t_color + "ignals" + Fx::ub + title_right_down;
			if (selected > 0) Input::mouse_mappings["s"] = {y + height - 1, mouse_x, 1, 7};
		    mouse_x += 9;
		    out += title_left_down + Fx::b + hi_color + 'N' + t_color + "ice" + Fx::ub + title_right_down;
		    if (selected > 0) Input::mouse_mappings["N"] = {y + height -1, mouse_x, 1, 5};
			mouse_x += 6;
			if (width > 72) {
			    fmt::format_to(std::back_inserter(out), "{}{}{}{}{}{}{}{}",
			    	title_left_down, follow_process ? Fx::b : "",
			    	hi_color, 'F', t_color, "ollow",
			    	Fx::ub, title_right_down);
			    if (selected > 0) Input::mouse_mappings["F"] = {y + height - 1, mouse_x, 1, 6};
			}

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
					+ rjust("Cpu%", (show_graphs ? 10 : 5)) + Fx::ub;
		}
		//* End of redraw block

		//? Draw details box if shown
		if (show_detailed) {
			bool alive = detailed.status != "Dead";
			const int item_fit = floor((double)(d_width - 2) / 10);
			const int item_width = floor((double)(d_width - 2) / min(item_fit, 8));

			//? Graph part of box
			string cpu_str = (alive or pause_proc_list ? fmt::format("{:.2f}", detailed.entry.cpu_p) : "");
			if (alive or pause_proc_list) {
				cpu_str.resize(4);
				if (cpu_str.ends_with('.')) { cpu_str.pop_back(); cpu_str.pop_back(); }
			}
			out += Mv::to(d_y + 1, dgraph_x + 1) + Fx::ub + detailed_cpu_graph(detailed.cpu_percent, (redraw or data_same or not alive))
				+ Mv::to(d_y + 1, dgraph_x + 1) + Theme::c("title") + Fx::b + rjust(cpu_str, 4) + "%";
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


			const double mem_p = detailed.mem_bytes.back() * 100.0 / totalMem;
			string mem_str = fmt::format("{:.2f}", mem_p);
			mem_str.resize(4);
			if (mem_str.ends_with('.')) mem_str.pop_back();
			out += Mv::to(d_y + 4, d_x + 1) + Theme::c("title") + Fx::b + rjust((item_fit > 4 ? "Memory: " : "M:") + rjust(mem_str, 4) + "% ", (d_width / 3) - 2)
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
			if (p.filtered or (proc_tree and p.tree_index == plist.size()) or n++ < start) continue;
			bool is_selected = (lc + 1 == selected);
			bool is_followed = followed_pid == (int)p.pid;
			if (is_selected) {
				selected_pid = (int)p.pid;
				selected_name = p.name;
				selected_depth = p.depth;
			}

			//? Update graphs for processes with above 0.0% cpu usage, delete if below 0.1% 10x times
			bool has_graph = show_graphs ? p_counters.contains(p.pid) : false;
			if (show_graphs and ((p.cpu_p > 0 and not has_graph) or (not data_same and has_graph))) {
				if (not has_graph) {
					p_graphs[p.pid] = Draw::Graph{5, 1, "", {}, graph_symbol};
					p_counters[p.pid] = 0;
				}
				else if (p.cpu_p < 0.1 and ++p_counters[p.pid] >= 10) {
					if (p_graphs.contains(p.pid)) p_graphs.erase(p.pid);
					p_counters.erase(p.pid);
				}
				else
					p_counters[p.pid] = 0;
			}

			out += Fx::reset;

			//? Set correct gradient colors if enabled
			string c_color, m_color, t_color, g_color, end;
			if (is_selected or is_followed) {
				c_color = m_color = t_color = g_color = Fx::b;
				end = Fx::ub;
				const string highlight = is_followed ? Theme::c("followed_bg") + Theme::c("followed_fg") : Theme::c("selected_bg") + Theme::c("selected_fg");
				fmt::format_to(std::back_inserter(out), "{}{}", highlight, Fx::b);
			}
			else {
				int calc = (selected > lc) ? selected - lc : lc - selected;
				if (proc_colors) {
					end = Theme::c("main_fg") + Fx::ub;
					array<string, 3> colors;
					for (int i = 0; int v : {(int)round(p.cpu_p), (int)round(p.mem * 100 / totalMem), (int)p.threads / 3}) {
						if (proc_gradient) {
							int val = (min(v, 100) + 100) - calc * 100 / select_max;
							if (val < 100) colors[i++] = Theme::g("proc_color").at(max(0, val));
							else colors[i++] = Theme::g("process").at(clamp(val - 100, 0, 100));
						}
						else
							colors[i++] = Theme::g("process").at(clamp(v, 0, 100));
					}
					c_color = colors.at(0); m_color = colors.at(1); t_color = colors.at(2);
				}
				else {
					c_color = m_color = t_color = Fx::b;
					end = Fx::ub;
				}
				if (proc_gradient) {
					g_color = Theme::g("proc").at(clamp(calc * 100 / select_max, 0, 100));
				}
			}

			const auto san_cmd = replace_ascii_control(p.cmd);

			if (not p_wide_cmd.contains(p.pid)) p_wide_cmd[p.pid] = ulen(san_cmd) != ulen(san_cmd, true);

			//? Normal view line
			if (not proc_tree) {
				out += Mv::to(y+2+lc, x+1)
					+ g_color + rjust(to_string(p.pid), 8) + ' '
					+ c_color + ljust(p.name, prog_size, true) + ' ' + end
					+ (cmd_size > 0 ? g_color + ljust(san_cmd, cmd_size, true, p_wide_cmd[p.pid]) + Mv::to(y+2+lc, x+11+prog_size+cmd_size) + ' ' : "");
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
				if (width_left > 7) {
					const string_view cmd = width_left > 40 ? rtrim(san_cmd) : p.short_cmd;
					if (not cmd.empty() and cmd != p.name) {
						out += g_color + '(' + uresize(string{cmd}, width_left - 3, p_wide_cmd[p.pid]) + ") ";
						width_left -= (ulen(string{cmd}, true) + 3);
					}
				}
				out += string(max(0, width_left), ' ') + Mv::to(y+2+lc, x+2+tree_size);
			}
			//? Common end of line
			string cpu_str = fmt::format("{:.2f}", p.cpu_p);
			if (p.cpu_p < 10 or (p.cpu_p >= 100 and p.cpu_p < 1000)) cpu_str.resize(3);
			else if (p.cpu_p >= 10'000) {
				cpu_str = fmt::format("{:.2f}", p.cpu_p / 1000);
				cpu_str.resize(3);
				if (cpu_str.ends_with('.')) cpu_str.pop_back();
				cpu_str += "k";
			}
			string mem_str = (mem_bytes ? floating_humanizer(p.mem, true) : "");
			if (not mem_bytes) {
				double mem_p = clamp((double)p.mem * 100 / totalMem, 0.0, 100.0);
				mem_str = mem_p < 0.01 ? "0" : fmt::format("{:.1f}", mem_p);
				if (mem_str.size() > 3) mem_str.resize(3);
				if (mem_str.ends_with('.')) mem_str.pop_back();
				mem_str += '%';
			}

			// Shorten process thread representation when larger than 5 digits: 10000 -> 10K ...
			const std::string proc_threads_string = [&] {
				if (p.threads > 9999) {
					return std::to_string(p.threads / 1000) + 'K';
				} else {
					return std::to_string(p.threads);
				}
			}();

			out += (thread_size > 0 ? t_color + rjust(proc_threads_string, thread_size) + ' ' + end : "" )
				+ g_color + ljust((cmp_greater(p.user.size(), user_size) ? p.user.substr(0, user_size - 1) + '+' : p.user), user_size) + ' '
				+ m_color + rjust(mem_str, 5) + end + ' '
				+ (is_selected or is_followed ? "" : Theme::c("inactive_fg")) + (show_graphs ? graph_bg * 5: "")
				+ (p_graphs.contains(p.pid) ? Mv::l(5) + c_color + p_graphs.at(p.pid)({(p.cpu_p >= 0.1 and p.cpu_p < 5 ? 5ll : (long long)round(p.cpu_p))}, data_same) : "") + end + ' '
				+ c_color + rjust(cpu_str, 4) + "  " + end;
			if (lc++ > height - 5) break;
			else if (lc > height - 5 and proc_banner_shown) break;
		}

		out += Fx::reset;
		while (lc++ < height - 3) out += Mv::to(y+lc+1, x+1) + string(width - 2, ' ');
		if (proc_banner_shown) {
			fmt::format_to(std::back_inserter(out), "{}{}{}{}{:^{}}{}",
				Mv::to(y + height - 2, x + 1),
				(pause_proc_list and follow_process) ? Theme::c("proc_banner_bg")
					: pause_proc_list ? Theme::c("proc_pause_bg")
					: Theme::c("proc_follow_bg"), 
				Theme::c("proc_banner_fg"), Fx::b,
				(pause_proc_list and follow_process) ? "Paused list and Following process"
					: pause_proc_list ? "Process list paused"
					: "Following process", width - 2,
				Fx::reset);
		}

		//? Draw scrollbar if needed
		if (numpids > select_max) {
			scroll_pos = clamp((int)round((double)start * select_max / (numpids - select_max)), 0, height - 5);
			out += Mv::to(y + 1, x + width - 2) + Fx::b + Theme::c("main_fg") + Symbols::up
				+ Mv::to(y + height - 2, x + width - 2) + Symbols::down;

			for (int i = y + 2; i < y + height - 2; i++) {
				out += Mv::to(i, x + width - 2) + ((i == y + 2 + scroll_pos) ? "█" : " ");
			}
		}

		//? Current selection and number of processes
		string location = to_string(start + (follow_process ? followed : selected)) + '/' + to_string(numpids);
		string loc_clear = Symbols::h_line * max((size_t)0, 9 - location.size());
		out += Mv::to(y + height - 1, x+width - 3 - max(9, (int)location.size())) + Fx::ub + Theme::c("proc_box") + loc_clear
			+ Symbols::title_left_down + Theme::c("title") + Fx::b + location + Fx::ub + Theme::c("proc_box") + Symbols::title_right_down;

		//? Clear out left over graphs from dead processes at a regular interval
		if (not data_same and ++counter >= 100) {
			counter = 0;

			std::erase_if(p_graphs, [&](const auto& pair) {
				return rng::find(plist, pair.first, &proc_info::pid) == plist.end();
			});

			std::erase_if(p_counters, [&](const auto& pair) {
				return rng::find(plist, pair.first, &proc_info::pid) == plist.end();
			});

			std::erase_if(p_wide_cmd, [&](const auto& pair) {
				return rng::find(plist, pair.first, &proc_info::pid) == plist.end();
			});
		}

		//? Draw hide button if detailed view is shown
		if (show_detailed) {
			const bool greyed_out = selected_pid != Config::getI("detailed_pid") && selected > 0; 
			fmt::format_to(std::back_inserter(out), "{}{}{}{}{}{}{}{}{}{}{}",
				Mv::to(d_y, d_x + d_width - 10), 
				Theme::c("proc_box"), Symbols::title_left, Fx::b,
				greyed_out ? Theme::c("inactive_fg") : Theme::c("title"), "hide ",
				greyed_out ? "" : Theme::c("hi_fg"), Symbols::enter,
				Fx::ub, Theme::c("proc_box"), Symbols::title_right);
			if (not greyed_out) Input::mouse_mappings["enter"] = {d_y, d_x + d_width - 9, 1, 6};
			else Input::mouse_mappings.erase("enter");
		}

		if (selected == 0 and selected_pid != 0) {
			selected_pid = 0;
			selected_name.clear();
		}
		redraw = false;
		return out + Fx::reset;
	}

}

namespace Draw {
	void calcSizes() {
		atomic_wait(Runner::active);
		Config::unlock();
		auto boxes = Config::getS("shown_boxes");
		auto cpu_bottom = Config::getB("cpu_bottom");
		auto mem_below_net = Config::getB("mem_below_net");
		auto net_beside_mem = Config::getB("net_beside_mem");
		auto proc_full_width = Config::getB("proc_full_width");
		auto proc_left = Config::getB("proc_left");

		Cpu::box.clear();

		Mem::box.clear();
		Net::box.clear();
		Proc::box.clear();
		Global::clock.clear();
		Global::overlay.clear();
		Runner::pause_output = false;
		Runner::redraw = true;
		Proc::p_counters.clear();
		Proc::p_graphs.clear();
		if (Menu::active) Menu::redraw = true;

		Input::mouse_mappings.clear();

		Cpu::x = Mem::x = Net::x = Proc::x = 1;
		Cpu::y = Mem::y = Net::y = Proc::y = 1;
		Cpu::width = Mem::width = Net::width = Proc::width = 0;
		Cpu::height = Mem::height = Net::height = Proc::height = 0;
		Cpu::redraw = Mem::redraw = Net::redraw = Proc::redraw = true;

		Cpu::shown = boxes.contains("cpu");

		//? Check if only "top panels" are shown (no mem/net/proc) - defined before GPU_SUPPORT for cross-platform use
		bool only_top_panels = not (boxes.contains("mem") or boxes.contains("net") or boxes.contains("proc"));

	#ifdef GPU_SUPPORT
		Gpu::box.clear();
		Gpu::width = 0;
		Gpu::shown_panels.clear();
		if (Gpu::count > 0) {
			std::istringstream iss(boxes, std::istringstream::in);
			string current;
			while (iss >> current) {
				if (current.starts_with("gpu"))
					Gpu::shown_panels.push_back(current.back()-'0');
			}
		}
		Gpu::shown = Gpu::shown_panels.size();

		// Calculate layout for top panels (CPU, GPU, Pwr)
		Pwr::shown = boxes.contains("pwr");
		Pwr::box.clear();
		Pwr::redraw = true;

		//? Count how many top panels are active
		int top_panel_count = (Cpu::shown ? 1 : 0) + (Gpu::shown > 0 ? 1 : 0) + (Pwr::shown ? 1 : 0);

		//? Pre-calculate heights for dynamic splitting
		Gpu::total_height = 0;
		Pwr::height = 0;

		if (only_top_panels and top_panel_count > 0) {
			//? Split space evenly among active top panels
			int space_per_panel = (Term::height - 1) / top_panel_count;

			if (Gpu::shown > 0) {
				Gpu::total_height = space_per_panel;
			}
			if (Pwr::shown) {
				Pwr::height = space_per_panel;
			}
			//? CPU will get remaining space in its section
		} else {
			//? Default: GPU and Pwr use minimal heights when bottom panels shown
			for (int i = 0; i < Gpu::shown; i++) {
				using namespace Gpu;
				total_height += 4 + gpu_b_height_offsets[shown_panels[i]];
			}
			if (Pwr::shown) {
				Pwr::height = Pwr::min_height;
			}
		}
	#endif
		Mem::shown = boxes.contains("mem");
		Net::shown = boxes.contains("net");
		Proc::shown = boxes.contains("proc");

		//* Calculate and draw cpu box outlines
		if (Cpu::shown) {
			using namespace Cpu;
		#ifdef GPU_SUPPORT
			// inline GPU information
			int gpus_extra_height =
				Config::getS("show_gpu_info") == "On" ? Gpu::count
				: Config::getS("show_gpu_info") == "Auto" ? Gpu::count - Gpu::shown
				: 0;
			//? Extra row for ANE when GPU panel ("5") is NOT shown (ANE shown inline with GPU)
			int ane_extra_height = (Shared::aneCoreCount > 0 and Gpu::shown == 0) ? 1 : 0;
		#endif
            const bool show_temp = (Config::getB("check_temp") and got_sensors);
			width = round((double)Term::width * width_p / 100);
		#ifdef GPU_SUPPORT
			if (only_top_panels and (Gpu::shown > 0 or Pwr::shown)) {
				//? When only top panels shown, CPU gets remaining space after GPU and Pwr
				height = Term::height - Gpu::total_height - Pwr::height - gpus_extra_height - ane_extra_height;
			} else {
				height = max(8, (int)ceil((double)Term::height * (trim(boxes) == "cpu" ? 100 : height_p/(Gpu::shown+1) + (Gpu::shown != 0)*5) / 100));
				//? Always add extra height for GPU/ANE info when GPU panel is hidden
				height += gpus_extra_height;
				if (Shared::aneCoreCount > 0 and Gpu::shown == 0) height += 1;
			}
		#else
			height = max(8, (int)ceil((double)Term::height * (trim(boxes) == "cpu" ? 100 : height_p) / 100));
		#endif
			//? Limit cpu height to 13 when bottom panels (mem/net/proc) are shown
			if (not only_top_panels and height > 13) height = 13;

			x = 1;
			y = cpu_bottom ? Term::height - height + 1 : 1;

		#ifdef GPU_SUPPORT
			//? Use height - 6 because drawing loop's cy == max_row condition means effective rows = max_row - 1
			b_columns = max(2, (int)ceil((double)(Shared::coreCount + 1) / (height - gpus_extra_height - ane_extra_height - 6)));
		#else
			b_columns = max(1, (int)ceil((double)(Shared::coreCount + 1) / (height - 6)));
		#endif
		#ifdef GPU_SUPPORT
			//? When GPU panel is visible, use most compact format to maximize main CPU graph area
			//? Important for high core count systems (e.g., M3 Ultra with 32 cores)
			if (Gpu::shown > 0) {
				//? Force b_column_size = 0: no per-core braille graphs, just "E0 XX%" format
				b_column_size = 0;
				b_width = (8 + 6 * show_temp) * b_columns + 1;
				//? Cap at available space (2/3 of box width) to preserve main graph area
				int max_info_width = width - (width / 3) - 1;
				b_width = min(b_width, max_info_width);
			} else {
		#endif
			if (b_columns * (21 + 12 * show_temp) < width - (width / 3)) {
				b_column_size = 2;
				b_width =  max(29, (21 + 12 * show_temp) * b_columns - (b_columns - 1));
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
			b_width = min(b_width, 46);  //? Cap to match GPU info box width
		#ifdef GPU_SUPPORT
			}
		#endif
			//? Recalculate b_column_size based on capped width
			if (b_column_size == 2 and b_width < (21 + 12 * show_temp) * b_columns - (b_columns - 1)) {
				b_column_size = 1;
			}
			if (b_column_size == 1 and b_width < (15 + 6 * show_temp) * b_columns - (b_columns - 1)) {
				b_column_size = 0;
			}
		#ifdef GPU_SUPPORT
			//gpus_extra_height = max(0, gpus_extra_height - 1);
			int ane_row = (Shared::aneCoreCount > 0 and Gpu::shown == 0) ? 1 : 0;
			b_height = min(height - 2, (int)ceil((double)Shared::coreCount / b_columns) + 4 + gpus_extra_height + ane_row);
		#else
			b_height = min(height - 2, (int)ceil((double)Shared::coreCount / b_columns) + 4);
		#endif

			b_x = x + width - b_width - 1;
			b_y = y + ceil((double)(height - 2) / 2) - ceil((double)b_height / 2) + 1;

			box = createBox(x, y, width, height, Theme::c("cpu_box"), true, (cpu_bottom ? "" : "cpu"), (cpu_bottom ? "cpu" : ""), 1);

			auto& custom = Config::getS("custom_cpu_name");
			static const bool hasCpuHz = not Cpu::get_cpuHz().empty();
		#ifdef __linux__
			static const bool freq_range = Config::getS("freq_mode") == "range";
		#else
			static const bool freq_range = false;
		#endif
			//? Build CPU title, optionally including GPU info when GPU panel is hidden on Apple Silicon
			string base_title = (custom.empty() ? Cpu::cpuName : custom);
		#ifdef GPU_SUPPORT
			//? On Apple Silicon, append GPU and ANE core count when GPU panel is hidden
			if (Shared::gpuCoreCount > 0 and Gpu::shown == 0) {
				base_title += " " + to_string(Shared::gpuCoreCount) + " GPUs";
				if (Shared::aneCoreCount > 0) {
					base_title += " " + to_string(Shared::aneCoreCount) + " ANEs";
				}
			}
		#endif
			const string cpu_title = uresize(
					base_title,
					b_width - (Config::getB("show_cpu_freq") and hasCpuHz ? (freq_range ? 24 : 14) : 5)
			);
			box += createBox(b_x, b_y, b_width, b_height, "", false, cpu_title);
		}

	#ifdef GPU_SUPPORT
		//* Calculate and draw gpu box outlines
		if (Gpu::shown != 0) {
			using namespace Gpu;
			x_vec.resize(shown); y_vec.resize(shown);
			b_x_vec.resize(shown); b_y_vec.resize(shown);
			b_height_vec.resize(shown);
			box.resize(shown);
			graph_upper_vec.resize(shown); graph_lower_vec.resize(shown);
			ane_graph_vec.resize(shown);
			temp_graph_vec.resize(shown);
			mem_used_graph_vec.resize(shown); mem_util_graph_vec.resize(shown);
			gpu_meter_vec.resize(shown);
			pwr_graph_vec.resize(shown);
			enc_meter_vec.resize(shown);
			ane_meter_vec.resize(shown);
			redraw.resize(shown);
			total_height = 0;
			for (auto i = 0; i < shown; ++i) {
				redraw[i] = true;
				int height = 0;
				width = Term::width;
				if (Cpu::shown)
					if (only_top_panels)
						height = (Term::height - Cpu::height - Pwr::height) / Gpu::shown;  //? Fill remaining space after CPU and Pwr
					else height = Cpu::height;
				else
					if (only_top_panels)
						height = (Term::height - total_height - Pwr::height) / (Gpu::shown - i) + (i == 0) * ((Term::height - total_height - Pwr::height) % (Gpu::shown - i));
					else
						height = max(min_height, (int)ceil((double)Term::height * height_p/Gpu::shown / 100));

				b_height_vec[i] = gpu_b_height_offsets[shown_panels[i]] + 2;
				height += (height+Cpu::height == Term::height-1);
				height = max(height, b_height_vec[i] + 1);  //? Reduced padding for tighter GPU panel fit
				//? Limit gpu height to 13 when bottom panels (mem/net/proc) are shown
				if (not only_top_panels and height > 13) height = 13;
				x_vec[i] = 1; y_vec[i] = 1 + total_height + (not Config::getB("cpu_bottom"))*Cpu::shown*Cpu::height;
				//? For Apple Silicon with single GPU, display "gpu" instead of "gpu0"
				string box_name = (Shared::gpuCoreCount > 0 and Gpu::count == 1)
					? "gpu"
					: std::string("gpu") + (char)(shown_panels[i]+'0');
				box[i] = createBox(x_vec[i], y_vec[i], width, height, Theme::c("cpu_box"), true, box_name, "", (shown_panels[i]+5)%10); // TODO gpu_box
				b_width = clamp(width/2, min_width, 46);
				total_height += height;

				//? Main statistics box
				b_x_vec[i] = x_vec[i] + width - b_width - 1;
				b_y_vec[i] = y_vec[i] + ceil((double)(height - 2 - b_height_vec[i]) / 2) + 1;

				string name = Config::getS(std::string("custom_gpu_name") + (char)(shown_panels[i]+'0'));
				if (name.empty()) name = gpu_names[shown_panels[i]];
				//? Add ANE count to GPU panel header for Apple Silicon
				if (Shared::aneCoreCount > 0) {
					name += " " + to_string(Shared::aneCoreCount) + " ANEs";
				}

				box[i] += createBox(b_x_vec[i], b_y_vec[i], b_width, b_height_vec[i], "", false, name.substr(0, b_width-3));
				b_height_vec[i] = height - 2;
			}
		}

		//* Calculate and draw pwr (power) box outlines
		if (Pwr::shown) {
			using namespace Pwr;

			width = Term::width;

			//? Height was pre-calculated above, but handle edge cases
			if (only_top_panels) {
				//? When only top panels shown, Pwr height was pre-calculated
				//? If Pwr is the only panel, it gets full height
				if (not Cpu::shown and Gpu::shown == 0) {
					height = Term::height - 1;
				}
				//? Otherwise height was already set in pre-calculation
			} else {
				//? When bottom panels shown, use min_height
				height = min_height;
			}
			height = max(height, min_height);

			x = 1;
			y = (Config::getB("cpu_bottom") ? 1 : Cpu::height + 1) + Gpu::total_height;

			box = createBox(x, y, width, height, Theme::c("cpu_box"), true, "pwr", "", 7);
		}

		//? Calculate Pwr offset for panels below it
		int pwr_offset = 0;
		if (Pwr::shown) pwr_offset = Pwr::height;
	#endif

		//* Calculate and draw mem box outlines
		if (Mem::shown) {
			using namespace Mem;
			auto show_disks = Config::getB("show_disks");
			auto swap_disk = Config::getB("swap_disk");
			auto mem_graphs = Config::getB("mem_graphs");

			//? Side-by-side mode: mem and net share the horizontal space
			if (net_beside_mem and Net::shown) {
				//? Width: always half the terminal (proc goes below, not beside)
				width = Term::width / 2;

				//? Height depends on proc position
				if (Proc::shown and proc_full_width) {
					//? Proc full width mode: mem+net get fixed height, proc gets the rest
				#ifdef GPU_SUPPORT
					int available = Term::height - Cpu::height - Gpu::total_height - pwr_offset;
				#else
					int available = Term::height - Cpu::height;
				#endif
					//? Fixed height for mem (enough for memory stats + ~6 disks)
					//? Cap at 20 lines so proc gets maximum space
					int max_height = 20;
					height = min(max_height, available - 6);  //? Ensure proc gets at least 6 lines
					if (height < 10) height = 10;  //? Absolute minimum
				}
				else {
					//? No proc or proc under net only: mem takes full height
				#ifdef GPU_SUPPORT
					height = Term::height - Cpu::height - Gpu::total_height - pwr_offset;
				#else
					height = Term::height - Cpu::height;
				#endif
				}

				x = 1;  //? Mem always on left in side-by-side
			#ifdef GPU_SUPPORT
				y = (cpu_bottom ? 1 : Cpu::height + 1) + Gpu::total_height + pwr_offset;
			#else
				y = cpu_bottom ? 1 : Cpu::height + 1;
			#endif
			}
			else {
				//? Original stacked layout
				width = round((double)Term::width * (Proc::shown ? width_p : 100) / 100);
			#ifdef GPU_SUPPORT
				height = ceil((double)Term::height * (100 - Net::height_p * Net::shown*4 / ((Gpu::shown != 0 and Cpu::shown) + 4)) / 100) - Cpu::height - Gpu::total_height - pwr_offset;
			#else
				height = ceil((double)Term::height * (100 - Cpu::height_p * Cpu::shown - Net::height_p * Net::shown) / 100) + 1;
			#endif
				x = (proc_left and Proc::shown) ? Term::width - width + 1: 1;
				if (mem_below_net and Net::shown)
			#ifdef GPU_SUPPORT
					y = Term::height - height + 1 - (cpu_bottom ? Cpu::height : 0);
				else
					y = (cpu_bottom ? 1 : Cpu::height + 1) + Gpu::total_height + pwr_offset;
			#else
					y = Term::height - height + 1 - (cpu_bottom ? Cpu::height : 0);
				else
					y = cpu_bottom ? 1 : Cpu::height + 1;
			#endif
			}

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
				//? Calculate graph height with remainder distribution for perfect fit
				//? Available = content area (height-2) - Total row (1) - one label line per item
				//? With swap: also subtract swap header row
				int swap_overhead = (has_swap and not swap_disk) ? 2 : 0;  //? Swap header + divider
				int available_graph_lines = (height - 2) - 1 - item_height - swap_overhead;
				graph_height = max(1, available_graph_lines / item_height);
				graph_height_remainder = available_graph_lines > 0 ? available_graph_lines % item_height : 0;
				if (graph_height > 1) mem_meter += 6;
			}
			else {
				graph_height = 0;
				graph_height_remainder = 0;
			}

			if (show_disks) {
				disk_meter = max(-14, width - mem_width - 23);
				if (disks_width < 25) disk_meter += 14;
			}

			box = createBox(x, y, width, height, Theme::c("mem_box"), true, "mem", "", 2);
			box += Mv::to(y, (show_disks ? divider + 2 : x + width - 9)) + Theme::c("mem_box") + Symbols::title_left + (show_disks ? Fx::b : "")
				+ Theme::c("hi_fg") + 'd' + Theme::c("title") + "isks" + Fx::ub + Theme::c("mem_box") + Symbols::title_right;
			Input::mouse_mappings["d"] = {y, (show_disks ? divider + 3 : x + width - 8), 1, 5};
			if (show_disks) {
				box += Mv::to(y, divider) + Symbols::div_up + Mv::to(y + height - 1, divider) + Symbols::div_down + Theme::c("div_line");
				for (auto i : iota(1, height - 1))
					box += Mv::to(y + i, divider) + Symbols::v_line;
			}
		}

		//* Calculate and draw net box outlines
		if (Net::shown) {
			using namespace Net;

			//? Side-by-side mode: net is beside mem (to the right)
			if (net_beside_mem and Mem::shown) {
				//? Width: remaining space after mem (proc goes below, not beside)
				width = Term::width - Mem::width;

				//? Height depends on proc position
				if (Proc::shown and not proc_full_width) {
					//? Proc under net only: net is fixed at 12 lines, proc gets the rest
					height = 12;
				}
				else if (Proc::shown and proc_full_width) {
					//? Proc full width: net matches mem height (both are capped)
					height = Mem::height;
				}
				else {
					//? No proc: net takes full height
				#ifdef GPU_SUPPORT
					height = Term::height - Cpu::height - Gpu::total_height - pwr_offset;
				#else
					height = Term::height - Cpu::height;
				#endif
				}

				//? Position right after mem box
				x = Mem::x + Mem::width;
			#ifdef GPU_SUPPORT
				y = (cpu_bottom ? 1 : Cpu::height + 1) + Gpu::total_height + pwr_offset;
			#else
				y = cpu_bottom ? 1 : Cpu::height + 1;
			#endif
			}
			else {
				//? Original stacked layout
				width = round((double)Term::width * (Proc::shown ? width_p : 100) / 100);
			#ifdef GPU_SUPPORT
				height = Term::height - Cpu::height - Gpu::total_height - Mem::height - pwr_offset;
			#else
				height = Term::height - Cpu::height - Mem::height;
			#endif
				x = (proc_left and Proc::shown) ? Term::width - width + 1 : 1;
				if (mem_below_net and Mem::shown)
				#ifdef GPU_SUPPORT
					y = (cpu_bottom ? 1 : Cpu::height + 1) + Gpu::total_height + pwr_offset;
				#else
					y = cpu_bottom ? 1 : Cpu::height + 1;
				#endif
				else
					y = Term::height - height + 1 - (cpu_bottom ? Cpu::height : 0);
			}

			b_width = (width > 45) ? 27 : 19;
			b_height = (height > 10) ? 9 : height - 2;
			b_x = x + width - b_width - 1;
			b_y = y + ((height - 2) / 2) - b_height / 2 + 1;
			d_graph_height = round((double)(height - 2) / 2);
			u_graph_height = height - 2 - d_graph_height;

			box = createBox(x, y, width, height, Theme::c("net_box"), true, "net", "", 3);
			auto swap_up_down = Config::getB("swap_upload_download");
			if (swap_up_down)
				box += createBox(b_x, b_y, b_width, b_height, "", false, "upload", "download");
			else
				box += createBox(b_x, b_y, b_width, b_height, "", false, "download", "upload");
		}

		//* Calculate and draw proc box outlines
		if (Proc::shown) {
			using namespace Proc;

			//? Side-by-side mode with proc positioning options
			if (net_beside_mem and Mem::shown and Net::shown) {
				if (proc_full_width) {
					//? Proc full width: spans under both mem and net
					width = Term::width;
					x = 1;
				#ifdef GPU_SUPPORT
					height = Term::height - Cpu::height - Gpu::total_height - pwr_offset - Mem::height;
				#else
					height = Term::height - Cpu::height - Mem::height;
				#endif
					y = Mem::y + Mem::height;
				}
				else {
					//? Proc under net only: right side, same width as net
					width = Net::width;
					x = Net::x;
				#ifdef GPU_SUPPORT
					height = Term::height - Cpu::height - Gpu::total_height - pwr_offset - Net::height;
				#else
					height = Term::height - Cpu::height - Net::height;
				#endif
					y = Net::y + Net::height;
				}
			}
			else {
				//? Original stacked layout
				width = Term::width - (Mem::shown ? Mem::width : (Net::shown ? Net::width : 0));
			#ifdef GPU_SUPPORT
				height = Term::height - Cpu::height - Gpu::total_height - pwr_offset;
			#else
				height = Term::height - Cpu::height;
			#endif
				x = proc_left ? 1 : Term::width - width + 1;
			#ifdef GPU_SUPPORT
				y = ((cpu_bottom and Cpu::shown) ? 1 : Cpu::height + 1) + Gpu::total_height + pwr_offset;
			#else
				y = (cpu_bottom and Cpu::shown) ? 1 : Cpu::height + 1;
			#endif
			}

			select_max = height - 3;
			box = createBox(x, y, width, height, Theme::c("proc_box"), true, "proc", "", 4);
		}
	}
}
