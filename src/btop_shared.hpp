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
#include <array>

using std::string, std::vector, std::deque, robin_hood::unordered_flat_map, std::atomic, std::array;

void clean_quit(const int sig=-1);
void banner_gen();

namespace Global {
	extern const string Version;
	extern string exit_error_msg;
	extern atomic<bool> thread_exception;
	extern int coreCount;
	extern string banner;
	extern atomic<bool> resized;
	extern string overlay;
}

namespace Runner {

	extern atomic<bool> active;
	extern atomic<bool> reading;
	extern atomic<bool> stopping;

	void run(const string& box="", const bool no_update=false, const bool force_redraw=false);
	void stop();

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
	extern int x, y, width, height;
	extern bool shown, redraw, got_sensors;

	struct cpu_info {
		vector<deque<long long>> percent;
		vector<deque<long long>> temp;
		array<float, 3> load_avg;
	};

	//* Collect cpu stats and temperatures
	cpu_info& collect(const bool return_last=false);

	//* Draw contents of cpu box using <cpu> as source
	string draw(const cpu_info& cpu, const bool force_redraw=false, const bool data_same=false);
}

namespace Mem {
	extern string box;
	extern int x, y, width, height;
	extern bool has_swap, shown, redraw;

	struct disk_info {
		uint64_t total = 0, used = 0;
	};

	struct mem_info {
		uint64_t total = 0, available = 0, cached = 0, free = 0;
		unordered_flat_map<string, deque<long long>> percent;
		unordered_flat_map<string, deque<long long>> disks_io;
		unordered_flat_map<string, disk_info> disks;
	};

	//* Collect mem & disks stats
	mem_info& collect(const bool return_last=false);

	//* Draw contents of mem box using <mem> as source
	string draw(const mem_info& mem, const bool force_redraw=false, const bool data_same=false);
}

namespace Net {
	extern string box;
	extern int x, y, width, height;
	extern bool shown, redraw;

	struct net_stat {
		uint64_t speed = 0, top = 0, total = 0;
	};

	struct net_info {
		unordered_flat_map<string, deque<long long>> bandwidth;
		unordered_flat_map<string, net_stat> stat;
		string ip_addr;
	};

	//* Collect net upload/download stats
	net_info& collect(const bool return_last=false);

	//* Draw contents of net box using <net> as source
	string draw(const net_info& net, const bool force_redraw=false, const bool data_same=false);
}

namespace Proc {
	extern atomic<int> numpids;

	extern string box;
	extern int x, y, width, height;
	extern bool shown, redraw;
	extern int select_max;
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

	//* Collect and sort process information from /proc
	vector<proc_info>& collect(const bool return_last=false);

	//* Update current selection and view
	bool selection(const string& cmd_key);

	//* Draw contents of proc box using <plist> as data source
	string draw(const vector<proc_info>& plist, const bool force_redraw=false, const bool data_same=false);
}
