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

#ifndef _btop_linux_included_
#define _btop_linux_included_

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <ranges>

#include <unistd.h>
#include <pwd.h>
// #include <grp.h>
#include <sys/stat.h>

#include <btop_config.h>
#include <btop_globs.h>
#include <btop_tools.h>

namespace fs = std::filesystem;
using namespace std;

class Processes {
	uint64_t timestamp;
	map<int, tuple<string, string, string>> cache;
	map<string, unsigned> sorts = {
		{"pid", 0},
		{"name", 1},
		{"command", 2},
		{"user", 3}
	};


	auto collect(string sorting="pid", bool reverse=false){
		int pid, count = 0;
		auto timestamp = time_ms();
		string item, name, cmd, attr, user;
		struct stat ug;
		auto sortint = (sorts.contains(sorting)) ? sorts[sorting] : 0;
		vector<tuple<int, string, string, string>> procs;
		for (auto& d: fs::directory_iterator("/proc")){
			item = fs::path(d.path()).filename();
			bool cached = false;
			if (d.is_directory() && isdigit(item[0])) {
				pid = stoi(item);
				if (cache.contains(pid)) {
					cached = true;
				} else {
					if (fs::is_regular_file((string)d.path() + "/comm")) {
						ifstream pread((string)d.path() + "/comm");
						getline(pread, name);
						pread.close();
					}
					if (fs::is_regular_file((string)d.path() + "/cmdline")) {
						ifstream pread((string)d.path() + "/cmdline");
						getline(pread, cmd);
						// if (cmd.size() > 1) cmd.erase(cmd.size(), 1);
						// cmd = to_string(ulen(cmd)) + ":" + to_string(cmd.size()) + cmd;
						pread.close();
					}
					if (fs::is_regular_file((string)d.path() + "/attr")) {
						attr = (string)d.path() + "/attr";
						stat(attr.c_str(), &ug);  // Error check omitted
						struct passwd *pw = getpwuid(ug.st_uid);
						user = pw->pw_name;
						// struct group  *gr = getgrgid(ug.st_gid);
					}
					cache[pid] = make_tuple(name, clean_nullbyte(cmd), user);
				}

				procs.push_back(make_tuple(pid, get<0>(cache[pid]), get<1>(cache[pid]), get<2>(cache[pid])));
			}
		}

		ranges::sort(procs, [sortint, reverse](tuple<int, string, string, string>& a, tuple<int, string, string, string>& b) {
			if (reverse) {
				switch (sortint) {
					case 0: return get<0>(a) > get<0>(b);
					case 1: return get<1>(a) > get<1>(b);
					case 2: return get<2>(a) > get<2>(b);
					case 3: return get<3>(a) > get<3>(b);
				}
			} else {
				switch (sortint) {
					case 0: return get<0>(a) < get<0>(b);
					case 1: return get<1>(a) < get<1>(b);
					case 2: return get<2>(a) < get<2>(b);
					case 3: return get<3>(a) < get<3>(b);
				}
			}
		});
		// if (reverse) views::reverse(procs);

		timestamp = time_ms() - timestamp;

		return procs;
	}
};



#endif