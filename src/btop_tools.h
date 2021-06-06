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

#ifndef _btop_tools_included_
#define _btop_tools_included_

#include <string>
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
#include <utility>
#include <atomic>
#include <filesystem>
#include <robin_hood.h>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

using std::string, std::vector, std::array, std::regex, std::max, std::to_string, std::cin, std::atomic, robin_hood::unordered_flat_map;
namespace fs = std::filesystem;

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//* Collection of escape codes for text style and formatting
namespace Fx {
	//* Escape sequence start
	const string e = "\x1b[";

	//* Bold on/off
	const string b = e + "1m";
	const string ub = e + "22m";

	//* Dark on/off
	const string d = e + "2m";
	const string ud = e + "22m";

	//* Italic on/off
	const string i = e + "3m";
	const string ui = e + "23m";

	//* Underline on/off
	const string ul = e + "4m";
	const string uul = e + "24m";

	//* Blink on/off
	const string bl = e + "5m";
	const string ubl = e + "25m";

	//* Strike / crossed-out on/off
	const string s = e + "9m";
	const string us = e + "29m";

	//* Reset foreground/background color and text effects
	const string reset_base = e + "0m";

	//* Reset text effects and restore default foregrund and background color < Changed by C_Theme
	string reset = reset_base;

	//* Regex for matching color, style and curse move escape sequences
	const regex escape_regex("\033\\[\\d+;?\\d?;?\\d*;?\\d*;?\\d*(m|f|s|u|C|D|A|B){1}");

	//* Regex for matching only color and style escape sequences
	const regex color_regex("\033\\[\\d+;?\\d?;?\\d*;?\\d*;?\\d*(m){1}");

	//* Return a string with all colors and text styling removed
	string uncolor(string& s){
		return regex_replace(s, color_regex, "");
	}
}

//* Collection of escape codes and functions for cursor manipulation
namespace Mv {
	//* Move cursor to <line>, <column>
	string to(int line, int col){ return Fx::e + to_string(line) + ";" + to_string(col) + "f";}

	//* Move cursor right <x> columns
	string r(int x){ return Fx::e + to_string(x) + "C";}

	//* Move cursor left <x> columns
	string l(int x){ return Fx::e + to_string(x) + "D";}

	//* Move cursor up x lines
	string u(int x){ return Fx::e + to_string(x) + "A";}

	//* Move cursor down x lines
	string d(int x) { return Fx::e + to_string(x) + "B";}

	//* Save cursor position
	const string save = Fx::e + "s";

	//* Restore saved cursor postion
	const string restore = Fx::e + "u";
}

//* Collection of escape codes and functions for terminal manipulation
namespace Term {

	bool initialized = false;
	bool resized = false;
	uint width = 0;
	uint height = 0;
	string fg, bg;

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

	//* Hide terminal cursor
	const string hide_cursor = Fx::e + "?25l";

	//* Show terminal cursor
	const string show_cursor = Fx::e + "?25h";

	//* Switch to alternate screen
	const string alt_screen = Fx::e + "?1049h";

	//* Switch to normal screen
	const string normal_screen = Fx::e + "?1049l";

	//* Clear screen and set cursor to position 0,0
	const string clear = Fx::e + "2J" + Fx::e + "0;0f";

	//* Clear from cursor to end of screen
	const string clear_end = Fx::e + "0J";

	//* Clear from cursor to beginning of screen
	const string clear_begin = Fx::e + "1J";

	//* Enable reporting of mouse position on click and release
	const string mouse_on = Fx::e + "?1002h" + Fx::e + "?1015h" + Fx::e + "?1006h";

	//* Disable mouse reporting
	const string mouse_off = Fx::e + "?1002l";

	//* Enable reporting of mouse position at any movement
	const string mouse_direct_on = Fx::e + "?1003h";

	//* Disable direct mouse reporting
	const string mouse_direct_off = Fx::e + "?1003l";

	//* Refresh variables holding current terminal width and height and return true if resized
	bool refresh(){
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if (width != w.ws_col || height != w.ws_row) {
			width = w.ws_col;
			height = w.ws_row;
			resized = true;
		}
		return resized;
	}

	//* Check for a valid tty, save terminal options and set new options
	bool init(){
		if (!initialized){
			initialized = (bool)isatty(STDIN_FILENO);
			if (initialized) {
				initialized = (0 == tcgetattr(STDIN_FILENO, &initial_settings));
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

	//* Restore terminal options
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

	//* Return number of UTF8 characters in a string with option to disregard escape sequences
	size_t ulen(string str, bool escape=false){
		if (escape) str = std::regex_replace(str, Fx::escape_regex, "");
		return std::count_if(str.begin(), str.end(),
			[](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; } );
	}

	//* Resize a string consisting of UTF8 characters (only reduces size)
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

	//* Return current time since epoch in seconds
	uint64_t time_s(){
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in milliseconds
	uint64_t time_ms(){
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in microseconds
	uint64_t time_micros(){
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Check if a string is a valid bool value
	bool isbool(string& str){
		return (str == "true") || (str == "false") || (str == "True") || (str == "False");
	}

	//* Check if a string is a valid integer value
	bool isint(string& str){
		return all_of(str.begin(), str.end(), ::isdigit);
	}

	//* Left-trim <t_str> from <str> and return string
	string ltrim(string str, string t_str = " "){
		while (str.starts_with(t_str)) str.erase(0, t_str.size());
		return str;
	}

	//* Right-trim <t_str> from <str> and return string
	string rtrim(string str, string t_str = " "){
		while (str.ends_with(t_str)) str.resize(str.size() - t_str.size());
		return str;
	}

	//* Left-right-trim <t_str> from <str> and return string
	string trim(string str, string t_str = " "){
		return ltrim(rtrim(str, t_str), t_str);
	}

	//* Split <string> at <delim> <time> number of times (0 for unlimited) and return vector
	vector<string> ssplit(string str, string delim = " ", int times = 0, bool ignore_remainder=false){
		vector<string> out;
		if (times > 0) out.reserve(times);
		if (!str.empty() && !delim.empty()){
			size_t pos = 0;
			int x = 0;
			string tmp;
			while ((pos = str.find(delim)) != string::npos){
				tmp = str.substr(0, pos);
				if (tmp != delim && !tmp.empty()) out.push_back(tmp);
				str.erase(0, pos + delim.size());
				if (times > 0 && ++x >= times) break;
			}
		}
		if (!ignore_remainder) out.push_back(str);
		return out;
	}

	//* Put current thread to sleep for <ms> milliseconds
	void sleep_ms(uint ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	//* Left justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string ljust(string str, const size_t x, bool utf=false, bool escape=false, bool lim=true){
		if (utf || escape) {
			if (!escape && lim && ulen(str) > x) str = uresize(str, x);
			return str + string(max((int)(x - ulen(str, escape)), 0), ' ');
		}
		else {
			if (lim && str.size() > x) str.resize(x);
			return str + string(max((int)(x - str.size()), 0), ' ');
		}
	}

	//* Right justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string rjust(string str, const size_t x, bool utf=false, bool escape=false, bool lim=true){
		if (utf || escape) {
			if (!escape && lim && ulen(str) > x) str = uresize(str, x);
			return string(max((int)(x - ulen(str, escape)), 0), ' ') + str;
		}
		else {
			if (lim && str.size() > x) str.resize(x);
			return string(max((int)(x - str.size()), 0), ' ') + str;
		}
	}

	//* Replace whitespaces " " with escape code for move right
	string trans(string str){
		size_t pos;
		string newstr;
		while ((pos = str.find(' ')) != string::npos){
			newstr.append(str.substr(0, pos));
			str.erase(0, pos);
			pos = 1;
			while (pos < str.size() && str.at(pos) == ' ') pos++;
			newstr.append(Mv::r(pos));
			str.erase(0, pos);
		}
		return (newstr.empty()) ? str : newstr + str;
	}

	//* Convert seconds to format "Xd HH:MM:SS" and return string
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

	//? Units for floating_humanizer function
	const array<string, 11> Units_bit = {"bit", "Kib", "Mib", "Gib", "Tib", "Pib", "Eib", "Zib", "Yib", "Bib", "GEb"};
	const array<string, 11> Units_byte = {"Byte", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "BiB", "GEB"};

	//* Scales up in steps of 1024 to highest possible unit and returns string with unit suffixed
	//* bit=True or defaults to bytes
	//* start=int to set 1024 multiplier starting unit
	//* short=True always returns 0 decimals and shortens unit to 1 character
	string floating_humanizer(uint64_t value, bool shorten=false, uint start=0, bool bit=false, bool per_second=false){
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
			if (out.size() == 4 && start > 0) { out.pop_back(); out.insert(2, ".");}
			else if (out.size() == 3 && start > 0) out.insert(1, ".");
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

	//* Add std::string operator "*" : Repeat string <str> <n> number of times
	std::string operator*(string str, size_t n){
		string out;
		out.reserve(str.size() * n);
		while (n-- > 0) out += str;
		return out;
	}

	//* Return current time in <strf> format
	string strf_time(string strf){
		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm bt {};
		std::stringstream ss;
		ss << std::put_time(localtime_r(&in_time_t, &bt), strf.c_str());
		return ss.str();
	}


	#if __GNUC__ > 10
		//* Redirects to atomic wait
		void atomic_wait(atomic<bool>& atom, bool val=true){
			atom.wait(val);
		}
	#else
		//* Crude implementation of atomic wait for GCC < 11
		void atomic_wait(atomic<bool>& atom, bool val=true){
			while (atom.load() == val) sleep_ms(1);
		}
	#endif

	//* Waits for <atom> to not be <val> and then sets it to <val> again
	void atomic_wait_set(atomic<bool>& atom, bool val=true){
		atomic_wait(atom, val);
		atom.store(val);
	}

}

//* Simple logging implementation
namespace Logger {
	using namespace Tools;
	namespace {
		std::atomic<bool> busy (false);
		bool first = true;
		string tdf = "%Y/%m/%d (%T) | ";
		unordered_flat_map<uint, string> log_levels = {
			{ 0, "DISABLED" },
			{ 1, "ERROR" },
			{ 2, "WARNING" },
			{ 3, "INFO" },
			{ 4, "DEBUG" }
		};
	}

	fs::path logfile;
	uint loglevel = 2;

	void log_write(uint level, string& msg){
		if (loglevel < level || logfile.empty()) return;
		atomic_wait_set(busy, true);
		std::error_code ec;
		if (fs::file_size(logfile, ec) > 1024 << 10 && !ec) {
			auto old_log = logfile;
			old_log += ".1";
			if (fs::exists(old_log)) fs::remove(old_log, ec);
			if (!ec) fs::rename(logfile, old_log, ec);
		}
		if (!ec) {
			std::ofstream lwrite(logfile, std::ios::app);
			if (first) { first = false; lwrite << "\n" << strf_time(tdf) << "===> btop++ v." << Global::Version << "\n";}
			lwrite << strf_time(tdf) << log_levels[level] << ": " << msg << "\n";
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


#endif
