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
#include <atomic>
#include <deque>
#include <robin_hood.h>

using std::string, std::vector, std::deque, robin_hood::unordered_flat_map, std::atomic;

namespace Global {
	extern const string Version;
	extern int coreCount;
	extern string banner;
}

namespace Runner {

	extern atomic<bool> active;
	extern atomic<bool> stop;

}

namespace Tools {
	//* Platform specific function for system_uptime (seconds since last restart)
	double system_uptime();
}

namespace Shared {
	//* Initialize platform specific needed variables and check for errors
	void init();
}

namespace Cpu {
	extern string box, cpuName;
	extern bool shown, redraw, got_sensors;
}

namespace Mem {
	extern string box;
	extern bool has_swap, shown, redraw;
}

namespace Net {
	extern string box;
	extern bool shown, redraw;
}

namespace Proc {
	extern int numpids;
	extern atomic<bool> stop;
	extern atomic<bool> collecting;

	extern string box;
	extern bool shown, redraw;
	extern int current_y, current_h, select_max;
	extern atomic<int> detailed_pid;
	extern int selected_pid, start, selected;

	//? Contains the valid sorting options for processes
	extern vector<string> sort_vector;

	//? Translation from process state char to explanative string
	extern unordered_flat_map<char, string> proc_states;

	//* Container for process information
	struct proc_info {
		size_t pid;
		string name = "", cmd = "";
		size_t threads = 0;
		string user = "";
		uint64_t mem = 0;
		double cpu_p = 0.0, cpu_c = 0.0;
		char state = '0';
		uint64_t cpu_n = 0, p_nice = 0, ppid = 0;
		string prefix = "";
	};

	//* Container for process info box
	struct detail_container {
		proc_info entry;
		string elapsed, parent, status, io_read, io_write, memory;
		size_t last_pid = 0;
		bool skip_smaps = false;
		deque<long long> cpu_percent;
	};

	extern detail_container detailed;

	//* Collect and sort process information from /proc, saves and returns reference to Proc::current_procs;
	vector<proc_info>& collect(bool return_last=false);

	//* Update current selection and view
	void selection(string cmd_key);

	//* Draw contents of proc box using <plist> as data source
	string draw(vector<proc_info> plist);
}
