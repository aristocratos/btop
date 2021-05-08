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
#include <map>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <btop_globs.h>

using std::string, std::to_string, std::round, std::vector, std::map, std::cin;

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//* Collection of escape codes for text style and formatting
namespace Fx {
	//* Escape sequence start
	const string e = "\x1b[";
	//* Bold on
	const string b = e + "1m";
	//* Bold off
	const string ub = e + "22m";
	//* Dark on
	const string d = e + "2m";
	//* Dark off
	const string ud = e + "22m";
	//* Italic on
	const string i = e + "3m";
	//* Italic off
	const string ui = e + "23m";
	//* Underline on
	const string ul = e + "4m";
	//* Underline off
	const string uul = e + "24m";
	//* Blink on
	const string bl = e + "5m";
	//* Blink off
	const string ubl = e + "25m";
	//* Strike / crossed-out on
	const string s = e + "9m";
	//* Strike / crossed-out off
	const string us = e + "29m";
	//* Reset foreground/background color and text effects
	const string reset_base = e + "0m";
	//* Reset text effects and restore default foregrund and background color < Changed by C_Theme
	string reset = reset_base;
};

//* Collection of escape codes and functions for cursor manipulation
namespace Mv {
	//* Move cursor to <line>, <column>
	inline string to(int line, int col){ return Fx::e + to_string(line) + ";" + to_string(col) + "f";}
	//* Move cursor right <x> columns
	inline string r(int x){ return Fx::e + to_string(x) + "C";}
	//* Move cursor left <x> columns
	inline string l(int x){ return Fx::e + to_string(x) + "D";}
	//* Move cursor up x lines
	inline string u(int x){ return Fx::e + to_string(x) + "A";}
	//* Move cursor down x lines
	inline string d(int x) { return Fx::e + to_string(x) + "B";}
	//* Save cursor position
	const string save = Fx::e + "s";
	//* Restore saved cursor postion
	const string restore = Fx::e + "u";
};

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

//* Return number of UTF8 characters in a string
inline size_t ulen(const string& str){
		size_t len = 0;
		for (char c : str) if ((c & 0xC0) != 0x80) ++len;
		return len;
}

//* Return current time since epoch in milliseconds
uint64_t time_ms(){
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

//* Check if a string is a valid bool value
bool isbool(string str){
	return (str == "true") || (str == "false") || (str == "True") || (str == "False");
}

//* Check if a string is a valid integer value
bool isint(string str){
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

//* Split <string> at <delim> <times> (0 for unlimited) times and return vector
vector<string> ssplit(string str, string delim = " ", int times = 0){
	vector<string> out;
	if (str != "" && delim != ""){
		size_t pos = 0;
		int x = 0;
		string tmp;
		while ((pos = str.find(delim)) != string::npos){
			tmp = str.substr(0, pos);
			if (tmp != delim && tmp != "") out.push_back(tmp);
			str.erase(0, pos + delim.size());
			if (times > 0 && ++x >= times) break;
		}
	}
	out.push_back(str);
	return out;
}

//? --------------------------------------------------- CLASSES -----------------------------------------------------

//* Collection of escape codes and functions for terminal manipulation
class C_Term {
	struct termios initial_settings;
public:
	bool initialized = false;
	int width = 0;
	int height = 0;
	bool resized = false;

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

	//* Enable reporting of mouse position on click and release
	const string mouse_on = Fx::e + "?1002h" + Fx::e + "?1015h" + Fx::e + "?1006h";

	//* Disable mouse reporting
	const string mouse_off = Fx::e + "?1002l";

	//* Enable reporting of mouse position at any movement
	const string mouse_direct_on = Fx::e + "?1003h";

	//* Disable direct mouse reporting
	const string mouse_direct_off = Fx::e + "?1003l";

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

	//* Toggle terminal input echo
	bool echo(bool on=true){
		struct termios settings;
		if (tcgetattr(STDIN_FILENO, &settings)) return false;
		if (on) settings.c_lflag |= ECHO;
		else settings.c_lflag &= ~(ECHO);
		return 0 == tcsetattr(STDIN_FILENO, TCSANOW, &settings);
	}

	//* Refresh variables holding current terminal width and height and return true if resized
	bool refresh(){
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		resized = (width != w.ws_col || height != w.ws_row) ? true : false;
		width = w.ws_col;
		height = w.ws_row;
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

	C_Term() {
		init();
		refresh();
		resized = false;
	}
};

#endif