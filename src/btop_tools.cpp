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

#include <cmath>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <utility>
#include <robin_hood.h>
#include <thread>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <btop_shared.hpp>
#include <btop_tools.hpp>

using 	std::string_view, std::array, std::regex, std::max, std::to_string, std::cin,
		std::atomic, robin_hood::unordered_flat_map;
namespace fs = std::filesystem;
namespace rng = std::ranges;

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//* Collection of escape codes for text style and formatting
namespace Fx {
	const string e = "\x1b[";
	const string b = e + "1m";
	const string ub = e + "22m";
	const string d = e + "2m";
	const string ud = e + "22m";
	const string i = e + "3m";
	const string ui = e + "23m";
	const string ul = e + "4m";
	const string uul = e + "24m";
	const string bl = e + "5m";
	const string ubl = e + "25m";
	const string s = e + "9m";
	const string us = e + "29m";
	const string reset_base = e + "0m";
	string reset = reset_base;

	const regex escape_regex("\033\\[\\d+;?\\d?;?\\d*;?\\d*;?\\d*(m|f|s|u|C|D|A|B){1}");

	const regex color_regex("\033\\[\\d+;?\\d?;?\\d*;?\\d*;?\\d*(m){1}");

	string uncolor(const string& s){
		return regex_replace(s, color_regex, "");
	}
}

//* Collection of escape codes and functions for cursor manipulation
namespace Mv {
	const string to(int line, int col){ return Fx::e + to_string(line) + ";" + to_string(col) + "f";}
	const string r(int x){ return Fx::e + to_string(x) + "C";}
	const string l(int x){ return Fx::e + to_string(x) + "D";}
	const string u(int x){ return Fx::e + to_string(x) + "A";}
	const string d(int x) { return Fx::e + to_string(x) + "B";}
	const string save = Fx::e + "s";
	const string restore = Fx::e + "u";
}


//* Collection of escape codes and functions for terminal manipulation
namespace Term {

	bool initialized = false;
	bool resized = false;
	uint width = 0;
	uint height = 0;
	string fg, bg, current_tty;

	const string hide_cursor = Fx::e + "?25l";
	const string show_cursor = Fx::e + "?25h";
	const string alt_screen = Fx::e + "?1049h";
	const string normal_screen = Fx::e + "?1049l";
	const string clear = Fx::e + "2J" + Fx::e + "0;0f";
	const string clear_end = Fx::e + "0J";
	const string clear_begin = Fx::e + "1J";
	const string mouse_on = Fx::e + "?1002h" + Fx::e + "?1015h" + Fx::e + "?1006h";
	const string mouse_off = Fx::e + "?1002l";
	const string mouse_direct_on = Fx::e + "?1003h";
	const string mouse_direct_off = Fx::e + "?1003l";

	namespace {
		struct termios initial_settings;

		//* Toggle terminal input echo
		bool echo(bool on=true){
			struct termios settings;
			if (tcgetattr(STDIN_FILENO, &settings)) return false;
			if (on) settings.c_lflag |= ECHO;
			else settings.c_lflag &= ~(ECHO);
			return 0 == tcsetattr(STDIN_FILENO, TCSANOW, &settings);
		}

		//* Toggle need for return key when reading input
		bool linebuffered(bool on=true){
			struct termios settings;
			if (tcgetattr(STDIN_FILENO, &settings)) return false;
			if (on) settings.c_lflag |= ICANON;
			else settings.c_lflag &= ~(ICANON);
			if (tcsetattr(STDIN_FILENO, TCSANOW, &settings)) return false;
			if (on) setlinebuf(stdin);
			else setbuf(stdin, NULL);
			return true;
		}
	}

	bool refresh(){
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if (width != w.ws_col or height != w.ws_row) {
			width = w.ws_col;
			height = w.ws_row;
			resized = true;
		}
		return resized;
	}

	bool init(){
		if (not initialized){
			initialized = (bool)isatty(STDIN_FILENO);
			if (initialized) {
				tcgetattr(STDIN_FILENO, &initial_settings);
				current_tty = (string)ttyname(STDIN_FILENO);
				cin.sync_with_stdio(false);
				cin.tie(NULL);
				echo(false);
				linebuffered(false);
				refresh();
				resized = false;
			}
		}
		return initialized;
	}

	void restore(){
		if (initialized) {
			echo(true);
			linebuffered(true);
			tcsetattr(STDIN_FILENO, TCSANOW, &initial_settings);
			initialized = false;
		}
	}
}

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {

	namespace {
		//? Units for floating_humanizer function
		const array<string, 11> Units_bit = {"bit", "Kib", "Mib", "Gib", "Tib", "Pib", "Eib", "Zib", "Yib", "Bib", "GEb"};
		const array<string, 11> Units_byte = {"Byte", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "BiB", "GEB"};
	}

	size_t ulen(string str, const bool escape){
		if (escape) str = std::regex_replace(str, Fx::escape_regex, "");
		return rng::count_if(str, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; } );
	}

	string uresize(string str, const size_t len){
		if (str.size() < 1) return str;
		if (len < 1) return "";
		for (size_t x = 0, i = 0; i < str.size(); i++) {
			if ((static_cast<unsigned char>(str.at(i)) & 0xC0) != 0x80) x++;
			if (x == len + 1) {
				str.resize(i);
				str.shrink_to_fit();
				break;
			}
		}
		return str;
	}

	uint64_t time_s(){
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	uint64_t time_ms(){
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	uint64_t time_micros(){
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	bool isbool(string& str){
		return (str == "true") or (str == "false") or (str == "True") or (str == "False");
	}

	bool stobool(string& str){
		return (str == "true" or str == "True");
	}

	bool isint(string& str){
		if (str.empty()) return false;
		size_t offset = (str[0] == '-' ? 1 : 0);
		return all_of(str.begin() + offset, str.end(), ::isdigit);
	}

	string ltrim(const string& str, const string t_str){
		string_view str_v = str;
		while (str_v.starts_with(t_str)) str_v.remove_prefix(t_str.size());
		return (string)str_v;
	}

	string rtrim(const string& str, const string t_str){
		string_view str_v = str;
		while (str_v.ends_with(t_str)) str_v.remove_suffix(t_str.size());
		return (string)str_v;
	}

	string trim(const string& str, const string t_str){
		return ltrim(rtrim(str, t_str), t_str);
	}

	vector<string> ssplit(const string& str, const char delim){
		vector<string> out;
		for (const auto& s : str 	| rng::views::split(delim)
									| rng::views::transform([](auto &&rng) {
										return string_view(&*rng.begin(), rng::distance(rng));
		})) {
			if (not s.empty()) out.emplace_back(s);
		}
		return out;
	}

	void sleep_ms(const uint& ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	string ljust(string str, const size_t x, bool utf, bool escape, bool lim){
		if (utf or escape) {
			if (not escape and lim and ulen(str) > x) str = uresize(str, x);
			return str + string(max((int)(x - ulen(str, escape)), 0), ' ');
		}
		else {
			if (lim and str.size() > x) str.resize(x);
			return str + string(max((int)(x - str.size()), 0), ' ');
		}
	}

	string rjust(string str, const size_t x, bool utf, bool escape, bool lim){
		if (utf or escape) {
			if (not escape and lim and ulen(str) > x) str = uresize(str, x);
			return string(max((int)(x - ulen(str, escape)), 0), ' ') + str;
		}
		else {
			if (lim and str.size() > x) str.resize(x);
			return string(max((int)(x - str.size()), 0), ' ') + str;
		}
	}

	string trans(const string& str){
		size_t pos;
		string_view oldstr = str;
		string newstr;
		newstr.reserve(str.size());
		while ((pos = oldstr.find(' ')) != string::npos){
			newstr.append(oldstr.substr(0, pos));
			size_t x = 0;
			while (pos + x < oldstr.size() and oldstr.at(pos + x) == ' ') x++;
			newstr.append(Mv::r(x));
			oldstr.remove_prefix(pos + x);
		}
		return (newstr.empty()) ? str : newstr + (string)oldstr;
	}

	string sec_to_dhms(uint sec){
		string out;
		uint d, h, m;
		d = sec / (3600 * 24);
		sec %= 3600 * 24;
		h = sec / 3600;
		sec %= 3600;
		m = sec / 60;
		sec %= 60;
		if (d>0) out = to_string(d) + "d ";
		out += ((h<10) ? "0" : "") + to_string(h) + ":";
		out += ((m<10) ? "0" : "") + to_string(m) + ":";
		out += ((sec<10) ? "0" : "") + to_string(sec);
		return out;
	}

	string floating_humanizer(uint64_t value, bool shorten, uint start, bool bit, bool per_second){
		string out;
		uint mult = (bit) ? 8 : 1;
		auto& units = (bit) ? Units_bit : Units_byte;

		value *= 100 * mult;

		while (value >= 102400){
			value >>= 10;
			if (value < 100){
				out = to_string(value);
				break;
			}
			start++;
		}
		if (out.empty()) {
			out = to_string(value);
			if (out.size() == 4 and start > 0) { out.pop_back(); out.insert(2, ".");}
			else if (out.size() == 3 and start > 0) out.insert(1, ".");
			else if (out.size() >= 2) out.resize(out.size() - 2);
		}
		if (shorten){
				if (out.find('.') != string::npos) out = to_string((int)round(stof(out)));
				if (out.size() > 3) { out = to_string((int)(out[0] - '0') + 1); start++;}
				out.push_back(units[start][0]);
		}
		else out += " " + units[start];

		if (per_second) out += (bit) ? "ps" : "/s";
		return out;
	}

	std::string operator*(string str, size_t n){
		string out;
		out.reserve(str.size() * n);
		while (n-- > 0) out += str;
		return out;
	}

	string strf_time(string strf){
		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm bt {};
		std::stringstream ss;
		ss << std::put_time(localtime_r(&in_time_t, &bt), strf.c_str());
		return ss.str();
	}


	#if (__GNUC__ > 10)
		//* Redirects to atomic wait
		void atomic_wait(atomic<bool>& atom, bool val){
			atom.wait(val);
		}
	#else
		//* Crude implementation of atomic wait for GCC 10
		void atomic_wait(atomic<bool>& atom, bool val){
			while (atom.load() == val) sleep_ms(1);
		}
	#endif

	void atomic_wait_set(atomic<bool>& atom, bool val){
		atomic_wait(atom, val);
		atom.store(val);
	}

}

namespace Logger {
	using namespace Tools;
	namespace {
		std::atomic<bool> busy (false);
		bool first = true;
		string tdf = "%Y/%m/%d (%T) | ";
	}

	vector<string> log_levels = {
			"DISABLED",
			"ERROR",
			"WARNING",
			"INFO",
			"DEBUG",
	};

	size_t loglevel;
	fs::path logfile;

	void set(string level){
		loglevel = v_index(log_levels, level);
	}

	void log_write(uint level, string& msg){
		if (loglevel < level or logfile.empty()) return;
		atomic_wait_set(busy, true);
		std::error_code ec;
		if (fs::exists(logfile) and fs::file_size(logfile, ec) > 1024 << 10 and not ec) {
			auto old_log = logfile;
			old_log += ".1";
			if (fs::exists(old_log)) fs::remove(old_log, ec);
			if (not ec) fs::rename(logfile, old_log, ec);
		}
		if (not ec) {
			std::ofstream lwrite(logfile, std::ios::app);
			if (first) { first = false; lwrite << "\n" << strf_time(tdf) << "===> btop++ v." << Global::Version << "\n";}
			lwrite << strf_time(tdf) << log_levels.at(level) << ": " << msg << "\n";
			lwrite.close();
		}
		else logfile.clear();
		busy.store(false);
	}

	void error(string msg){
		log_write(1, msg);
	}

	void warning(string msg){
		log_write(2, msg);
	}

	void info(string msg){
		log_write(3, msg);
	}

	void debug(string msg){
		log_write(4, msg);
	}
}
