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
#include <regex>
#include <atomic>
#include <filesystem>
#include <ranges>
#include <chrono>
#include <thread>


using std::string, std::vector, std::atomic;


//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//* Collection of escape codes for text style and formatting
namespace Fx {
	extern const string e;		//* Escape sequence start
	extern const string b;		//* Bold on/off
	extern const string ub;		//* Bold off
	extern const string d;		//* Dark on
	extern const string ud;		//* Dark off
	extern const string i;		//* Italic on
	extern const string ui;		//* Italic off
	extern const string ul;		//* Underline on
	extern const string uul;	//* Underline off
	extern const string bl;		//* Blink on
	extern const string ubl;	//* Blink off
	extern const string s;		//* Strike/crossed-out on
	extern const string us;		//* Strike/crossed-out on/off

	//* Reset foreground/background color and text effects
	extern const string reset_base;

	//* Reset text effects and restore theme foregrund and background color
	extern string reset;

	//* Regex for matching color, style and curse move escape sequences
	extern const std::regex escape_regex;

	//* Regex for matching only color and style escape sequences
	extern const std::regex color_regex;

	//* Return a string with all colors and text styling removed
	inline string uncolor(const string& s){
		return regex_replace(s, color_regex, "");
	};
}

//* Collection of escape codes and functions for cursor manipulation
namespace Mv {
	//* Move cursor to <line>, <column>
	const string to(int line, int col);

	//* Move cursor right <x> columns
	const string r(int x);

	//* Move cursor left <x> columns
	const string l(int x);

	//* Move cursor up x lines
	const string u(int x);

	//* Move cursor down x lines
	const string d(int x);

	//* Save cursor position
	extern const string save;

	//* Restore saved cursor postion
	extern const string restore;
}

//* Collection of escape codes and functions for terminal manipulation
namespace Term {
	extern atomic<bool> initialized;
	extern atomic<int> width;
	extern atomic<int> height;
	extern string fg, bg, current_tty;

	//* Hide terminal cursor
	extern const string hide_cursor;

	//* Show terminal cursor
	extern const string show_cursor;

	//* Switch to alternate screen
	extern const string alt_screen;

	//* Switch to normal screen
	extern const string normal_screen;

	//* Clear screen and set cursor to position 0,0
	extern const string clear;

	//* Clear from cursor to end of screen
	extern const string clear_end;

	//* Clear from cursor to beginning of screen
	extern const string clear_begin;

	//* Enable reporting of mouse position on click and release
	extern const string mouse_on;

	//* Disable mouse reporting
	extern const string mouse_off;

	//* Enable reporting of mouse position at any movement
	extern const string mouse_direct_on;

	//* Disable direct mouse reporting
	extern const string mouse_direct_off;

	//* Escape sequence for start of synchronized output
	extern const string sync_start;

	//* Escape sequence for end of synchronized output
	extern const string sync_end;

	//* Returns true if terminal has been resized, updates width and height if update=true
	bool refresh(bool update=true);

	//* Check for a valid tty, save terminal options and set new options
	bool init();

	//* Restore terminal options
	void restore();
}

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {
	constexpr auto SSmax = std::numeric_limits<std::streamsize>::max();

	//* Return number of UTF8 characters in a string
	inline size_t ulen(const string& str){
		return std::ranges::count_if(str, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; } );
	};

	//* Resize a string consisting of UTF8 characters (only reduces size)
	string uresize(const string str, const size_t len);

	//* Return <str> with only uppercase characters
	inline string str_to_upper(string str){
		std::ranges::for_each(str, [](auto& c){ c = ::toupper(c); } );
		return str;
	}

	//* Return <str> with only lowercase characters
	inline string str_to_lower(string str){
		std::ranges::for_each(str, [](char& c){ c = ::tolower(c); } );
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
	bool is_in(First &&first, T && ... t) {
		return ((first == t) || ...);
	}

	//* Return current time since epoch in seconds
	inline uint64_t time_s(){
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in milliseconds
	inline uint64_t time_ms(){
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Return current time since epoch in microseconds
	inline uint64_t time_micros(){
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	//* Check if a string is a valid bool value
	inline bool isbool(const string& str){
		return (str == "true") or (str == "false") or (str == "True") or (str == "False");
	}

	//* Convert string to bool, returning any value not equal to "true" or "True" as false
	inline bool stobool(const string& str){
		return (str == "true" or str == "True");
	}

	//* Check if a string is a valid integer value
	inline bool isint(const string& str){
		return all_of(str.begin() + (str[0] == '-' ? 1 : 0), str.end(), ::isdigit);
	}

	//* Left-trim <t_str> from <str> and return new string
	string ltrim(const string& str, const string& t_str = " ");

	//* Right-trim <t_str> from <str> and return new string
	string rtrim(const string& str, const string& t_str = " ");

	//* Left/right-trim <t_str> from <str> and return new string
	inline string trim(const string& str, const string& t_str = " "){
		return ltrim(rtrim(str, t_str), t_str);
	};

	//* Split <string> at all occurrences of <delim> and return as vector of strings
	vector<string> ssplit(const string& str, const char& delim = ' ');

	//* Put current thread to sleep for <ms> milliseconds
	inline void sleep_ms(const size_t& ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };

	//* Left justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string ljust(string str, const size_t x, const bool utf=false, const bool limit=true);

	//* Right justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string rjust(string str, const size_t x, const bool utf=false, const bool limit=true);

	//* Replace whitespaces " " with escape code for move right
	string trans(const string& str);

	//* Convert seconds to format "Xd HH:MM:SS" and return string
	string sec_to_dhms(size_t sec);

	//* Scales up in steps of 1024 to highest possible unit and returns string with unit suffixed
	//* bit=True or defaults to bytes
	//* start=int to set 1024 multiplier starting unit
	//* short=True always returns 0 decimals and shortens unit to 1 character
	string floating_humanizer(uint64_t value, bool shorten=false, size_t start=0, bool bit=false, bool per_second=false);

	//* Add std::string operator "*" : Repeat string <str> <n> number of times
	std::string operator*(string str, size_t n);

	//* Return current time in <strf> format
	string strf_time(const string& strf);

	//* Waits for <atom> to not be <val>
	#if (__GNUC__ > 10)
		//* Redirects to atomic wait
		inline void atomic_wait(const atomic<bool>& atom, bool val=true){
			atom.wait(val);
		}
	#else
		//* Crude implementation of atomic wait for GCC 10
		inline void atomic_wait(const atomic<bool>& atom, bool val=true){
			while (atom == val) std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	#endif

	//* Waits for <atom> to not be <val> and then sets it to <val> again
	inline void atomic_wait_set(std::atomic<bool>& atom, bool val=true){
		atomic_wait(atom, val);
		atom = val;
	};

}

//* Simple logging implementation
namespace Logger {
	extern const vector<string> log_levels;
	extern std::filesystem::path logfile;

	void set(const string level); //* Set log level, valid arguments: "DISABLED", "ERROR", "WARNING", "INFO" and "DEBUG"
	void log_write(const size_t level, const string& msg);
	inline void error(const string msg){ log_write(1, msg); }
	inline void warning(const string msg){ log_write(2, msg); }
	inline void info(const string msg){ log_write(3, msg); }
	inline void debug(const string msg){ log_write(4, msg); }
}

