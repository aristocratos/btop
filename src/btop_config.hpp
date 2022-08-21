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
#include <robin_hood.h>
#include <filesystem>

using std::string, std::vector, robin_hood::unordered_flat_map;

//* Functions and variables for reading and writing the btop config file
namespace Config {

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	extern unordered_flat_map<string, string> strings;
	extern unordered_flat_map<string, string> stringsTmp;
	extern unordered_flat_map<string, bool> bools;
	extern unordered_flat_map<string, bool> boolsTmp;
	extern unordered_flat_map<string, int> ints;
	extern unordered_flat_map<string, int> intsTmp;

	const vector<string> valid_graph_symbols = { "braille", "block", "tty" };
	const vector<string> valid_graph_symbols_def = { "default", "braille", "block", "tty" };
	const vector<string> valid_boxes = { "cpu", "mem", "net", "proc" };
	const vector<string> temp_scales = { "celsius", "fahrenheit", "kelvin", "rankine" };

	extern vector<string> current_boxes;
	extern vector<string> preset_list;
	extern vector<string> available_batteries;
	extern int current_preset;

	//* Check if string only contains space separated valid names for boxes
	bool check_boxes(const string& boxes);

	//* Toggle box and update config string shown_boxes
	void toggle_box(const string& box);

	//* Parse and setup config value presets
	bool presetsValid(const string& presets);

	//* Apply selected preset
	void apply_preset(const string& preset);

	bool _locked(const string& name);

	//* Return bool for config key <name>
	inline const bool& getB(const string& name) { return bools.at(name); }

	//* Return integer for config key <name>
	inline const int& getI(const string& name) { return ints.at(name); }

	//* Return string for config key <name>
	inline const string& getS(const string& name) { return strings.at(name); }

	string getAsString(const string& name);

	extern string validError;

	bool intValid(const string& name, const string& value);
	bool stringValid(const string& name, const string& value);

	//* Set config key <name> to bool <value>
	inline void set(const string& name, const bool& value) {
		if (_locked(name)) boolsTmp.insert_or_assign(name, value);
		else bools.at(name) = value;
	}

	//* Set config key <name> to int <value>
	inline void set(const string& name, const int& value) {
		if (_locked(name)) intsTmp.insert_or_assign(name, value);
		else ints.at(name) = value;
	}

	//* Set config key <name> to string <value>
	inline void set(const string& name, const string& value) {
		if (_locked(name)) stringsTmp.insert_or_assign(name, value);
		else strings.at(name) = value;
	}

	//* Flip config key bool <name>
	void flip(const string& name);

	//* Lock config and cache changes until unlocked
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(const std::filesystem::path& conf_file, vector<string>& load_warnings);

	//* Write the config file to disk
	void write();
}







