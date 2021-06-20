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


using std::string, std::vector;


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
	string uncolor(string& s);
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
	extern bool initialized;
	extern bool resized;
	extern uint width;
	extern uint height;
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

	//* Refresh variables holding current terminal width and height and return true if resized
	bool refresh();

	//* Check for a valid tty, save terminal options and set new options
	bool init();

	//* Restore terminal options
	void restore();
}

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {
	const auto SSmax = std::numeric_limits<std::streamsize>::max();

	//* Return number of UTF8 characters in a string with option to disregard escape sequences
	size_t ulen(string str, const bool escape=false);

	//* Resize a string consisting of UTF8 characters (only reduces size)
	string uresize(string str, const size_t len);

	//* Check if vector <vec> contains value <find_val>
	template <typename T>
	bool v_contains(const vector<T>& vec, const T find_val) {
		return std::ranges::find(vec, find_val) != vec.end();
	}

	//* Return index of <find_val> from vector <vec>, returns size of <vec> if <find_val> is not present
	template <typename T>
	size_t v_index(vector<T>& vec, T find_val) {
		return std::ranges::distance(vec.begin(), std::ranges::find(vec, find_val));
	}

	//* Return current time since epoch in seconds
	uint64_t time_s();

	//* Return current time since epoch in milliseconds
	uint64_t time_ms();

	//* Return current time since epoch in microseconds
	uint64_t time_micros();

	//* Check if a string is a valid bool value
	bool isbool(string& str);

	//* Convert string to bool, returning any value not equal to "true" or "True" as false
	bool stobool(string& str);

	//* Check if a string is a valid integer value
	bool isint(string& str);

	//* Left-trim <t_str> from <str> and return new string
	string ltrim(const string& str, const string t_str = " ");

	//* Right-trim <t_str> from <str> and return new string
	string rtrim(const string& str, const string t_str = " ");

	//* Left-right-trim <t_str> from <str> and return new string
	string trim(const string& str, const string t_str = " ");

	//* Split <string> at all occurrences of <delim> and return as vector of strings
	vector<string> ssplit(const string& str, const char delim = ' ');

	//* Put current thread to sleep for <ms> milliseconds
	void sleep_ms(const uint& ms);

	//* Left justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string ljust(string str, const size_t x, bool utf=false, bool escape=false, bool lim=true);

	//* Right justify string <str> if <x> is greater than <str> length, limit return size to <x> by default
	string rjust(string str, const size_t x, bool utf=false, bool escape=false, bool lim=true);

	//* Replace whitespaces " " with escape code for move right
	string trans(const string& str);

	//* Convert seconds to format "Xd HH:MM:SS" and return string
	string sec_to_dhms(uint sec);

	//* Scales up in steps of 1024 to highest possible unit and returns string with unit suffixed
	//* bit=True or defaults to bytes
	//* start=int to set 1024 multiplier starting unit
	//* short=True always returns 0 decimals and shortens unit to 1 character
	string floating_humanizer(uint64_t value, bool shorten=false, uint start=0, bool bit=false, bool per_second=false);

	//* Add std::string operator "*" : Repeat string <str> <n> number of times
	std::string operator*(string str, size_t n);

	//* Return current time in <strf> format
	string strf_time(string strf);

	//* Waits for <atom> to not be <val>
	void atomic_wait(std::atomic<bool>& atom, bool val=true);

	//* Waits for <atom> to not be <val> and then sets it to <val> again
	void atomic_wait_set(std::atomic<bool>& atom, bool val=true);

}

//* Simple logging implementation
namespace Logger {
	extern vector<string> log_levels;
	extern std::filesystem::path logfile;

	void set(string level); //* Set log level, valid arguments: "DISABLED", "ERROR", "WARNING", "INFO" and "DEBUG"
	void log_write(uint level, string& msg);
	void error(string msg);
	void warning(string msg);
	void info(string msg);
	void debug(string msg);
}

