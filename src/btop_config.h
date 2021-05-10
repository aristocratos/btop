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
#include <map>

#include <btop_tools.h>

using namespace std;


//? Classes, functions and variables for reading and writing the btop config file

#define Bool bool()
#define Int int()
#define String string()


//* Used for classes and functions needing pre-initialised values
namespace State {
	bool truecolor = true;
	string fg, bg;
	unsigned width, height;
}

class C_Config {
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
public:
	C_Config(){
		bools["truecolor"] = "true";
		strings["color_theme"] = "Default";
		ints["tree_depth"] = 3;

		State::truecolor = bools["truecolor"];
	}

	//* Return config value <name> as a bool
	bool operator()(bool b_type, string name){
		return bools.at(name);
	}

	//* Return config value <name> as a int
	int operator()(int i_type, string name){
		return ints.at(name);
	}

	//* Return config value <name> as a string
	string operator()(string s_type, string name){
		return strings.at(name);
	}
};

#endif