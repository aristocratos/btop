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
#define _btop_config_included_ 1

#include <string>
#include <vector>
#include <map>

#include <btop_tools.h>

using std::string, std::vector, std::map;
using namespace Tools;


//* Functions and variables for reading and writing the btop config file
namespace Config {
	namespace {

		bool changed = false;

		map<string, string> strings = {
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
		map<string, bool> bools = {
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
		map<string, int> ints = {
			{"update_ms", 2000},
			{"proc_update_mult", 2},
			{"tree_depth", 3}
		};
	};

	//* Return config value <name> as a bool
	bool& getB(string name){
		return bools.at(name);
	}

	//* Return config value <name> as a int
	int& getI(string name){
		return ints.at(name);
	}

	//* Return config value <name> as a string
	string& getS(string name){
		return strings.at(name);
	}

	//* Set config value <name> to bool <value>
	void setB(string name, bool value){
		bools.at(name) = value;
		changed = true;
	}

	//* Set config value <name> to int <value>
	void setI(string name, int value){
		ints.at(name) = value;
		changed = true;
	}

	//* Set config value <name> to string <value>
	void setS(string name, string value){
		strings.at(name) = value;
		changed = true;
	}

	bool load(string source){
		(void)source;
		return true;
	}
};

#endif