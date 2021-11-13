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

#pragma once

#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <filesystem>
#include <ranges>
#include <chrono>
#include <thread>
#include <tuple>
#include <pthread.h>
#include <limits.h>
#ifndef HOST_NAME_MAX
	#ifdef __APPLE__
		#define HOST_NAME_MAX 255
	#else
		#define HOST_NAME_MAX 64
	#endif
#endif

using std::string, std::vector, std::atomic, std::to_string, std::tuple, std::array;


//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//* Collection of escape codes for text style and formatting
namespace Fx {
	const string e = "\x1b[";				//* Escape sequence start
	const string b = e + "1m";				//* Bold on/off
	const string ub = e + "22m";			//* Bold off
	const string d = e + "2m";				//* Dark on
	const string ud = e + "22m";			//* Dark off
	const string i = e + "3m";				//* Italic on
	const string ui = e + "23m";			//* Italic off
	const string ul = e + "4m";				//* Underline on
	const string uul = e + "24m";			//* Underline off
	const string bl = e + "5m";				//* Blink on
	const string ubl = e + "25m";			//* Blink off
	const string s = e + "9m";				//* Strike/crossed-out on
	const string us = e + "29m";			//* Strike/crossed-out on/off

	//* Reset foreground/background color and text effects
	const string reset_base = e + "0m";

	//* Reset text effects and restore theme foregrund and background color
	extern string reset;

	//* Return a string with all colors and text styling removed
	string uncolor(const string& s);

}

//* Collection of escape codes and functions for cursor manipulation
namespace Mv {
	//* Move cursor to <line>, <column>
	inline string to(const int& line, const int& col) { return Fx::e + to_string(line) + ';' + to_string(col) + 'f'; }

	//* Move cursor right <x> columns
	inline string r(const int& x) { return Fx::e + to_string(x) + 'C'; }

	//* Move cursor left <x> columns
	inline string l(const int& x) { return Fx::e + to_string(x) + 'D'; }

	//* Move cursor up x lines
	inline string u(const int& x) { return Fx::e + to_string(x) + 'A'; }

	//* Move cursor down x lines
	inline string d(const int& x) { return Fx::e + to_string(x) + 'B'; }

	//* Save cursor position
	const string save = Fx::e + "s";

	//* Restore saved cursor postion
	const string restore = Fx::e + "u";
}

//* Collection of escape codes and functions for terminal manipulation
namespace Term {
	extern atomic<bool> initialized;
	extern atomic<int> width;
	extern atomic<int> height;
	extern string fg, bg, current_tty;

	const string hide_cursor = Fx::e + "?25l";
	const string show_cursor = Fx::e + "?25h";
	const string alt_screen = Fx::e + "?1049h";
	const string normal_screen = Fx::e + "?1049l";
	const string clear = Fx::e + "2J" + Fx::e + "0;0f";
	const string clear_end = Fx::e + "0J";
	const string clear_begin = Fx::e + "1J";
	const string mouse_on = Fx::e + "?1002h" + Fx::e + "?1015h" + Fx::e + "?1006h"; //? Enable reporting of mouse position on click and release
	const string mouse_off = Fx::e + "?1002l" + Fx::e + "?1015l" + Fx::e + "?1006l";
	const string mouse_direct_on = Fx::e + "?1003h"; //? Enable reporting of mouse position at any movement
	const string mouse_direct_off = Fx::e + "?1003l";
	const string sync_start = Fx::e + "?2026h"; //? Start of terminal synchronized output
	const string sync_end = Fx::e + "?2026l"; //? End of terminal synchronized output

	//* Returns true if terminal has been resized and updates width and height
	bool refresh(bool only_check=false);

	//* Returns an array with the lowest possible width, height with current box config
	auto get_min_size(const string& boxes) -> array<int, 2>;

	//* Check for a valid tty, save terminal options and set new options
	bool init();

	//* Restore terminal options
	void restore();
}

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {
	constexpr auto SSmax = std::numeric_limits<std::streamsize>::max();
	extern atomic<int> active_locks;

	//* Return number of UTF8 characters in a string (wide=true counts UTF-8 characters with a width > 1 as 2 characters)
	inline size_t ulen(const string& str, const bool wide=false) {
		return 	std::ranges::count_if(str, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; })
				+ (wide ? std::ranges::count_if(str, [](char c) { return (static_cast<unsigned char>(c) > 0xef); }) : 0);
	}

	//* Resize a string consisting of UTF8 characters (only reduces size)
	string uresize(const string str, const size_t len, const bool wide=false);

	//* Resize a string consisting of UTF8 characters from left (only reduces size)
	string luresize(const string str, const size_t len, const bool wide=false);

	//* Replace <from> in <str> with <to> and return new string
	string s_replace(const string& str, const string& from, const string& to);

	//* Capatilize <str>
	inline string capitalize(string str) {
		str.at(0) = toupper(str.at(0));
		return str;
	}

	//* Return <str> with only uppercase characters
	inline string str_to_upper(string str) {
		std::ranges::for_each(str, [](auto& c) { c = ::toupper(c); } );
		return str;
	}

	//* Return <str> with only lowercase characters
	inline string str_to_lower(string str) {
		std::ranges::for_each(str, [](char& c) { c = ::tolower(c); } );
		return str;
	}

	//* Check if vector <vec> contains value <find_val>
	template <typename T, typename T2>
	inline bool v_contains(const vector<T>& vec, const T2& find_val) {
		return std::ranges::find(vec, find_val) != vec.end();
	}

	//* Check if string <str> contains value <find_val>
	template <typename T>
	inline bool s_contains(const string& str, const T& find_val) {
		return str.find(find_val) != string::npos;
	}

	//* Return index of <find_val> from vector <vec>, returns size of <vec> if <find_val> is not present
	template <typename T>
	inline size_t v_index(const vector<T>& vec, const T& find_val) {
		return std::ranges::distance(vec.begin(), std::ranges::find(vec, find_val));
	}

	//* Compare <first> with all following values
	template<typename First, typename ... T>
	inline bool is_in(const First& first, const T& ... t) {
		return ((first == t) or ...);
	}

	//* Return current time since epoch in seconds
	inline uint64_t time_s() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in milliseconds
	inline uint64_t time_ms() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in microseconds
	inline uint64_t time_micros() {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Check if a string is a valid bool value
	inline bool isbool(const string& str) {
		return is_in(str, "true", "false", "True", "False");
	}

	//* Convert string to bool, returning any value not equal to "true" or "True" as false
	inline bool stobool(const string& str) {
		return is_in(str, "true", "True");
	}

	//* Check if a string is a valid integer value (only postive)
	inline bool isint(const string& str) {
		return all_of(str.begin(), str.end(), ::isdigit);
	}

	//* Left-trim <t_str> from <str> and return new string
	string ltrim(const string& str, const string& t_str = " ");

	//* Right-trim <t_str> from <str> and return new string
	string rtrim(const string& str, const string& t_str = " ");

	//* Left/right-trim <t_str> from <str> and return new string
	inline string trim(const string& str, const string& t_str = " ") {
		return ltrim(rtrim(str, t_str), t_str);
	}

	//* Split <string> at all occurrences of <delim> and return as vector of strings
	auto ssplit(const string& str, const char& delim = ' ') -> vector<string>;

	//* Put current thread to sleep for <ms> milliseconds
	inline void sleep_ms(const size_t& ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

	//* Put current thread to sleep for <micros> microseconds
	inline void sleep_micros(const size_t& micros) { std::this_thread::sleep_for(std::chrono::microseconds(micros)); }

	//* Left justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string ljust(string str, const size_t x, const bool utf=false, const bool wide=false, const bool limit=true);

	//* Right justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string rjust(string str, const size_t x, const bool utf=false, const bool wide=false, const bool limit=true);

	//* Center justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string cjust(string str, const size_t x, const bool utf=false, const bool wide=false, const bool limit=true);

	//* Replace whitespaces " " with escape code for move right
	string trans(const string& str);

	//* Convert seconds to format "<days>d <hours>:<minutes>:<seconds>" and return string
	string sec_to_dhms(size_t seconds, bool no_days=false, bool no_seconds=false);

	//* Scales up in steps of 1024 to highest positive value unit and returns string with unit suffixed
	//* bit=True or defaults to bytes
	//* start=int to set 1024 multiplier starting unit
	//* short=True always returns 0 decimals and shortens unit to 1 character
	string floating_humanizer(uint64_t value, const bool shorten=false, size_t start=0, const bool bit=false, const bool per_second=false);

	//* Add std::string operator * : Repeat string <str> <n> number of times
	std::string operator*(const string& str, int64_t n);

	//* Return current time in <strf> format
	string strf_time(const string& strf);

	string hostname();
	string username();

	static inline void busy_wait (void) {
	#if defined __i386__ || defined __x86_64__
		__builtin_ia32_pause();
	#elif defined __ia64__
		__asm volatile("hint @pause" : : : "memory");
	#elif defined __sparc__ && (defined __arch64__ || defined __sparc_v9__)
		__asm volatile("membar #LoadLoad" : : : "memory");
	#else
		__asm volatile("" : : : "memory");
	#endif
	}

	void atomic_wait(const atomic<bool>& atom, const bool old=true) noexcept;

	void atomic_wait_for(const atomic<bool>& atom, const bool old=true, const uint64_t wait_ms=0) noexcept;

	//* Waits for atomic<bool> to be false and sets it to true on construct, sets to false on destruct
	class atomic_lock {
		atomic<bool>& atom;
		bool not_true = false;
	public:
		atomic_lock(atomic<bool>& atom, bool wait=false);
		~atomic_lock();
	};

	//* Read a complete file and return as a string
	string readfile(const std::filesystem::path& path, const string& fallback="");

	//* Convert a celsius value to celsius, fahrenheit, kelvin or rankin and return tuple with new value and unit.
	auto celsius_to(const long long& celsius, const string& scale) -> tuple<long long, string>;

}

//* Simple logging implementation
namespace Logger {
	const vector<string> log_levels = {
		"DISABLED",
		"ERROR",
		"WARNING",
		"INFO",
		"DEBUG",
	};
	extern std::filesystem::path logfile;

	//* Set log level, valid arguments: "DISABLED", "ERROR", "WARNING", "INFO" and "DEBUG"
	void set(const string& level);

	void log_write(const size_t level, const string& msg);
	inline void error(const string msg) { log_write(1, msg); }
	inline void warning(const string msg) { log_write(2, msg); }
	inline void info(const string msg) { log_write(3, msg); }
	inline void debug(const string msg) { log_write(4, msg); }
}

