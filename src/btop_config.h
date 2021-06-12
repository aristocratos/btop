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
#include <array>
#include <robin_hood.h>
#include <filesystem>

#include <btop_tools.h>

using std::string, std::vector, robin_hood::unordered_flat_map, std::map;
namespace fs = std::filesystem;
using namespace Tools;


//* Functions and variables for reading and writing the btop config file
namespace Config {
	namespace {

		fs::path conf_dir;
		fs::path conf_file;

		atomic<bool> locked (false);
		atomic<bool> writelock (false);
		bool write_new;

		vector<array<string, 2>> descriptions = {
			{"color_theme", "#* Color theme, looks for a .theme file in \"/usr/[local/]share/bpytop/themes\" and \"~/.config/bpytop/themes\", \"Default\" for builtin default theme.\n"
							"#* Prefix name by a plus sign (+) for a theme located in user themes folder, i.e. color_theme=\"+monokai\"." },
			{"theme_background", "#* If the theme set background should be shown, set to False if you want terminal background transparency."},
			{"truecolor", "#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false."},
			{"shown_boxes", "#* Manually set which boxes to show. Available values are \"cpu mem net proc\", separate values with whitespace."},
			{"update_ms", "#* Update time in milliseconds, increases automatically if set below internal loops processing time, recommended 2000 ms or above for better sample times for graphs."},
			{"proc_update_mult", 	"#* Processes update multiplier, sets how often the process list is updated as a multiplier of \"update_ms\".\n"
									"#* Set to 2 or higher to greatly decrease bpytop cpu usage. (Only integers)."},
			{"proc_sorting",	"#* Processes sorting, \"pid\" \"program\" \"arguments\" \"threads\" \"user\" \"memory\" \"cpu lazy\" \"cpu responsive\",\n"
								"#* \"cpu lazy\" updates top process over time, \"cpu responsive\" updates top process directly."},
			{"proc_reversed", "#* Reverse sorting order, True or False."},
			{"proc_tree", "#* Show processes as a tree."},
			{"tree_depth", "#* Which depth the tree view should auto collapse processes at."},
			{"proc_colors", "#* Use the cpu graph colors in the process list."},
			{"proc_gradient", "#* Use a darkening gradient in the process list."},
			{"proc_per_core", "#* If process cpu usage should be of the core it's running on or usage of the total available cpu power."},
			{"proc_mem_bytes", "#* Show process memory as bytes instead of percent."},
			{"cpu_graph_upper", "#* Sets the CPU stat shown in upper half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},
			{"cpu_graph_lower", "#* Sets the CPU stat shown in lower half of the CPU graph, \"total\" is always available.\n"
								"#* Select from a list of detected attributes from the options menu."},
			{"cpu_invert_lower", "#* Toggles if the lower CPU graph should be inverted."},
			{"cpu_single_graph", "#* Set to True to completely disable the lower CPU graph."},
			{"show_uptime", "#* Shows the system uptime in the CPU box."},
			{"check_temp", "#* Show cpu temperature."},
			{"cpu_sensor", "#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors."},
			{"show_coretemp", "#* Show temperatures for cpu cores also if check_temp is True and sensors has been found."},
			{"temp_scale", "#* Which temperature scale to use, available values: \"celsius\", \"fahrenheit\", \"kelvin\" and \"rankine\"."},
			{"show_cpu_freq", "#* Show CPU frequency."},
			{"draw_clock", "#* Draw a clock at top of screen, formatting according to strftime, empty string to disable."},
			{"background_update", "#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort."},
			{"custom_cpu_name", "#* Custom cpu model name, empty string to disable."},
			{"disks_filter", 	"#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with a comma \",\".\n"
								"#* Begin line with \"exclude=\" to change to exclude filter, otherwise defaults to \"most include\" filter. Example: disks_filter=\"exclude=/boot, /home/user\"."},
			{"mem_graphs", "#* Show graphs instead of meters for memory values."},
			{"show_swap", "#* If swap memory should be shown in memory box."},
			{"swap_disk", "#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk."},
			{"show_disks", "#* If mem box should be split to also show disks info."},
			{"only_physical", "#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar."},
			{"use_fstab", "#* Read disks list from /etc/fstab. This also disables only_physical."},
			{"show_io_stat", "#* Toggles if io stats should be shown in regular disk usage view."},
			{"io_mode", "#* Toggles io mode for disks, showing only big graphs for disk read/write speeds."},
			{"io_graph_combined", "#* Set to True to show combined read/write io graphs in io mode."},
			{"io_graph_speeds", "#* Set the top speed for the io graphs in MiB/s (10 by default), use format \"device:speed\" separate disks with a comma \",\".\n"
								"#* Example: \"/dev/sda:100, /dev/sdb:20\"."},
			{"net_download", "#* Set fixed values for network graphs, default \"10M\" = 10 Mibibytes, possible units \"K\", \"M\", \"G\", append with \"bit\" for bits instead of bytes, i.e \"100mbit\"."},
			{"net_upload", ""},
			{"net_auto", "#* Start in network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest."},
			{"net_sync", "#* Sync the scaling for download and upload to whichever currently has the highest scale."},
			{"net_color_fixed", "#* If the network graphs color gradient should scale to bandwidth usage or auto scale, bandwidth usage is based on \"net_download\" and \"net_upload\" values."},
			{"net_iface", "#* Starts with the Network Interface specified here."},
			{"show_battery", "#* Show battery stats in top right if battery is present."},
			{"log_level", 	"#* Set loglevel for \"~/.config/bpytop/error.log\" levels are: \"ERROR\" \"WARNING\" \"INFO\" \"DEBUG\".\n"
							"#* The level set includes all lower levels, i.e. \"DEBUG\" will show all logging info."}
		};

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
			if (!write_new) write_new = true;
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
	}

	//* Set config value <name> to int <value>
	void set(string name, int value){
		if (_locked()) intsTmp.insert_or_assign(name, value);
		ints.at(name) = value;
	}

	//* Set config value <name> to string <value>
	void set(string name, string value){
		if (_locked()) stringsTmp.insert_or_assign(name, value);
		else strings.at(name) = value;
	}

	//* Flip config bool <name>
	void flip(string name){
		if (_locked()) {
			if (boolsTmp.contains(name)) boolsTmp.at(name) = !boolsTmp.at(name);
			else boolsTmp.insert_or_assign(name, (!bools.at(name)));
		}
		else bools.at(name) = !bools.at(name);
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
		writelock = false;
		locked = false;
	}

	//* Load the config file from disk
	void load(fs::path conf_file, vector<string>& load_errors){
		if (conf_file.empty())
			return;
		else if (!fs::exists(conf_file)) {
			write_new = true;
			return;
		}
		std::ifstream cread(conf_file);
		if (cread.good()) {
			unordered_flat_map<string, int> valid_names;
			for (auto &n : descriptions)
				valid_names[n[0]] = 0;
			string v_string;
			getline(cread, v_string, '\n');
			if (!v_string.ends_with(Global::Version))
				write_new = true;
			while (!cread.eof()) {
				cread >> std::ws;
				if (cread.peek() == '#') {
					cread.ignore(SSmax, '\n');
					continue;
				}
				string name, value;
				getline(cread, name, '=');
				if (!valid_names.contains(name)) {
					cread.ignore(SSmax, '\n');
					continue;
				}

				if (bools.contains(name)) {
					cread >> value;
					if (!isbool(value))
						load_errors.push_back("Got an invalid bool value for config name: " + name);
					else
						bools.at(name) = stobool(value);
				}
				else if (ints.contains(name)) {
					cread >> value;
					if (!isint(value))
						load_errors.push_back("Got an invalid integer value for config name: " + name);
					else
						ints.at(name) = stoi(value);
				}
				else if (strings.contains(name)) {
					cread >> std::ws;
					if (cread.peek() == '"') {
						cread.ignore(1);
						getline(cread, value, '"');
					}
					else cread >> value;

					if (name == "log_level" && !v_contains(Logger::log_levels, value)) load_errors.push_back("Invalid log_level: " + value);
					else strings.at(name) = value;
				}

				cread.ignore(SSmax, '\n');
			}
			cread.close();
			if (!load_errors.empty()) write_new = true;
		}
	}

	//* Write the config file to disk
	void write(){
		if (conf_file.empty() || !write_new) return;
		Logger::debug("Writing new config file");
		std::ofstream cwrite(conf_file, std::ios::trunc);
		if (cwrite.good()) {
			cwrite << "#? Config file for btop v. " << Global::Version;
			for (auto [name, description] : descriptions) {
				cwrite << "\n\n" << (description.empty() ? "" : description + "\n") << name << "=";
				if (strings.contains(name)) cwrite << "\"" << strings.at(name) << "\"";
				else if (ints.contains(name)) cwrite << ints.at(name);
				else if (bools.contains(name)) cwrite << (bools.at(name) ? "True" : "False");
			}
			cwrite.close();
		}
	}
}

#endif