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
#include <atomic>
#include <filesystem>

using std::string, std::vector, std::atomic;

namespace Global {
	extern int coreCount;
}

namespace Tools {
	double system_uptime();
}

namespace Proc {

	extern std::filesystem::path proc_path;
	extern size_t numpids;
	extern atomic<bool> stop;
	extern atomic<bool> collecting;
	extern vector<string> sort_vector;

	//* Container for process information
	struct proc_info {
		uint64_t pid;
		string name = "", cmd = "";
		size_t threads = 0;
		string user = "";
		uint64_t mem = 0;
		double cpu_p = 0.0, cpu_c = 0.0;
		char state = '0';
		uint64_t cpu_n = 0, p_nice = 0, ppid = 0;
		string prefix = "";
	};

	extern vector<proc_info> current_procs;

	//* Collects and sorts process information from /proc, saves to and returns reference to Proc::current_procs;
	vector<proc_info>& collect();

	//* Initialize needed variables for collect
	void init();
}