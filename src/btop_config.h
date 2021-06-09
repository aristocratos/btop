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

#ifndef _btop_config_included_
#define _btop_config_included_

#include <string>
#include <vector>
#include <robin_hood.h>
#include <filesystem>

#include <btop_tools.h>

using std::string, std::vector, robin_hood::unordered_flat_map;
namespace fs = std::filesystem;
using namespace Tools;


//* Functions and variables for reading and writing the btop config file
namespace Config {
	namespace {

		fs::path conf_dir;
		fs::path conf_file;

		atomic<bool> locked (false);
		atomic<bool> writelock (false);
		unordered_flat_map<string, bool> changed;

		unordered_flat_map<string, string> strings = {
			{"color_theme", "Default"},
			{"shown_boxes", "cpu mem net proc"},
			{"proc_sorting", "cpu lazy"},
			{"cpu_graph_upper", "total"},
			{"cpu_graph_lower", "total"},
			{"cpu_sensor", "Auto"},
			{"temp_scale", "celsius"},
			{"draw_clock", "%X"},
			{"custom_cpu_name", ""},
			{"disks_filter", ""},
			{"io_graph_speeds", ""},
			{"net_download", "10M"},
			{"net_upload", "10M"},
			{"net_iface", ""},
			{"log_level", "WARNING"}
		};
		unordered_flat_map<string, string> stringsTmp;

		unordered_flat_map<string, bool> bools = {
			{"theme_background", true},
			{"truecolor", true},
			{"proc_reversed", false},
			{"proc_tree", false},
			{"proc_colors", true},
			{"proc_gradient", true},
			{"proc_per_core", false},
			{"proc_mem_bytes", true},
			{"cpu_invert_lower", true},
			{"cpu_single_graph", false},
			{"show_uptime", true},
			{"check_temp", true},
			{"show_coretemp", true},
			{"show_cpu_freq", true},
			{"background_update", true},
			{"update_check", true},
			{"mem_graphs", true},
			{"show_swap", true},
			{"swap_disk", true},
			{"show_disks", true},
			{"only_physical", true},
			{"use_fstab", false},
			{"show_io_stat", true},
			{"io_mode", false},
			{"io_graph_combined", false},
			{"net_color_fixed", false},
			{"net_auto", true},
			{"net_sync", false},
			{"show_battery", true},
			{"show_init", false}
		};
		unordered_flat_map<string, bool> boolsTmp;

		unordered_flat_map<string, int> ints = {
			{"update_ms", 2000},
			{"proc_update_mult", 2},
			{"tree_depth", 3}
		};
		unordered_flat_map<string, int> intsTmp;

		bool _locked(){
			atomic_wait(writelock);
			return locked.load();
		}
	}

	//* Return bool config value <name>
	const bool& getB(string name){
		return bools.at(name);
	}

	//* Return integer config value <name>
	const int& getI(string name){
		return ints.at(name);
	}

	//* Return string config value <name>
	const string& getS(string name){
		return strings.at(name);
	}

	//* Set config value <name> to bool <value>
	void set(string name, bool value){
		if (_locked()) boolsTmp.insert_or_assign(name, value);
		else bools.at(name) = value;
		changed.insert_or_assign(name, true);
	}

	//* Set config value <name> to int <value>
	void set(string name, int value){
		if (_locked()) intsTmp.insert_or_assign(name, value);
		ints.at(name) = value;
		changed.insert_or_assign(name, true);
	}

	//* Set config value <name> to string <value>
	void set(string name, string value){
		if (_locked()) stringsTmp.insert_or_assign(name, value);
		else strings.at(name) = value;
		changed.insert_or_assign(name, true);
	}

	//* Flip config bool <name>
	void flip(string name){
		if (_locked()) {
			if (boolsTmp.contains(name)) boolsTmp.at(name) = !boolsTmp.at(name);
			else boolsTmp.insert_or_assign(name, (!bools.at(name)));
		}
		else bools.at(name) = !bools.at(name);
		changed.insert_or_assign(name, true);
	}

	//* Wait if locked then lock config and cache changes until unlock
	void lock(){
		atomic_wait_set(locked);
	}

	//* Unlock config and write any cached values to config
	void unlock(){
		atomic_wait_set(writelock);
		if (stringsTmp.size() > 0) {
			for (auto& item : stringsTmp){
				strings.at(item.first) = item.second;
			}
			stringsTmp.clear();
		}
		if (intsTmp.size() > 0) {
			for (auto& item : intsTmp){
				ints.at(item.first) = item.second;
			}
			intsTmp.clear();
		}
		if (boolsTmp.size() > 0) {
			for (auto& item : boolsTmp){
				bools.at(item.first) = item.second;
			}
			boolsTmp.clear();
		}
		writelock.store(false);
		locked.store(false);
	}

	void load(){
		if (conf_file.empty()) return;
	}

	void write(){
		if (conf_file.empty() || changed.empty()) return;

		if (Logger::loglevel > 3) {
			string items;
			for (auto item : Config::changed) {
				items += item.first + ", ";
			}
			items.pop_back(); items.pop_back();
			Logger::debug("Writing out new config values for: " + items);
		}
	}
}

#endif