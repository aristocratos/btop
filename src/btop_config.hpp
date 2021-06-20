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
#include <filesystem>

using std::string, std::vector;

//* Functions and variables for reading and writing the btop config file
namespace Config {

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	extern const vector<string> valid_graph_symbols;

	//* Return bool for config key <name>
	const bool& getB(string name);

	//* Return integer for config key <name>
	const int& getI(string name);

	//* Return string for config key <name>
	const string& getS(string name);

	//* Set config key <name> to bool <value>
	void set(string name, bool value);

	//* Set config key <name> to int <value>
	void set(string name, int value);

	//* Set config key <name> to string <value>
	void set(string name, string value);

	//* Flip config key bool <name>
	void flip(string name);

	//* Wait if locked then lock config and cache changes until unlock
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(std::filesystem::path conf_file, vector<string>& load_errors);

	//* Write the config file to disk
	void write();
}