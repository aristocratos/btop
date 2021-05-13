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

#include <btop_config.h>
#include <btop_globs.h>
#include <btop_tools.h>

namespace fs = std::filesystem;
using namespace std;

class Processes {
	uint64_t tstamp;
	long int clk_tck;
	map<int, tuple<string, string, string>> cache;
	map<string, unsigned> sorts = {
		{"pid", 0},
		{"name", 1},
		{"command", 2},
		{"user", 3},
		{"cpu direct", 4},
		{"cpu lazy", 5}
	};
	map<string, string> uid_user;
	fs::file_time_type passwd_time;
	map<int, uint64_t> cpu_times;
	map<int, uint64_t> cpu_second;
	unsigned counter = 0;
public:
	atomic<bool> stop;
	atomic<bool> running;

	//* Collects process information from /proc and returns a vector of tuples
	auto collect(string sorting="pid", bool reverse=false, string filter=""){
		running.store(true);
		int pid;
		uint64_t cpu_t;
		double cpu, cpu_s;
		ifstream pread;
		string pid_str, name, cmd, attr, user, instr, uid, status, tmpstr;
		auto since_last = time_ms() - tstamp;
		if (since_last < 1) since_last = 1;
		auto uptime = system_uptime();
		auto sortint = (sorts.contains(sorting)) ? sorts[sorting] : 5;
		vector<string> pstat;

		//? Return type! Values in tuple: pid, program, command, username, cpu%, cpu cumulative
		vector<tuple<int, string, string, string, double, double>> procs;

		//* Update uid_user map if /etc/passwd changed since last run
		if (fs::last_write_time(fs::path("/etc/passwd")) != passwd_time) {
			string r_uid, r_user;
			passwd_time = fs::last_write_time(fs::path("/etc/passwd"));
			uid_user.clear();
			pread.clear();
			ifstream pread("/etc/passwd");
			while (true){
				getline(pread, r_user, ':');
				pread.ignore(numeric_limits<streamsize>::max(), ':');
				getline(pread, r_uid, ':');
				uid_user[r_uid] = r_user;
				pread.ignore(numeric_limits<streamsize>::max(), '\n');
				if (pread.eof()) break;
			}
			pread.close();
		}


		//* Iterate over all pid directories in /proc and get relevant values
		for (auto& d: fs::directory_iterator("/proc")){
			if (stop.load()) {
				procs.clear();
				running.store(false);
				stop.store(false);
				return procs;
			}
			pid_str = fs::path(d.path()).filename();
			cpu = 0.0;
			if (d.is_directory() && isdigit(pid_str[0])) {
				pid = stoi(pid_str);

				//* Get cpu usage
				if (fs::is_regular_file((string)d.path() + "/stat")) {
					pstat.clear();
					ifstream pread((string)d.path() + "/stat");
					while (getline(pread, instr, ' ')) pstat.push_back(instr);
					pread.close();

					//? Process utime + stime
					cpu_t = stoull(pstat[13]) + stoull(pstat[14]);
					if (!cpu_times.contains(pid)) cpu_times[pid] = cpu_t;

					//? Cache process start time
					if (!cpu_second.contains(pid)) cpu_second[pid] = stoull(pstat[21]);

					//? Process cpu usage since last update, 100'000 because (100 percent * 1000 milliseconds) for correct conversion
					cpu = static_cast<double>(100000 * (cpu_t - cpu_times[pid]) / since_last) / clk_tck;

					//? Process cumulative cpu usage since process start
					cpu_s = static_cast<double>((cpu_t / clk_tck) / (uptime - (cpu_second[pid] / clk_tck)));
					cpu_times[pid] = cpu_t;
				}

				//* Cache program name, command and username
				if (!cache.contains(pid)) {
					if (fs::is_regular_file((string)d.path() + "/comm")) {
						ifstream pread((string)d.path() + "/comm");
						getline(pread, name);
						pread.close();
					}
					if (fs::is_regular_file((string)d.path() + "/cmdline")) {
						cmd.clear();
						ifstream pread((string)d.path() + "/cmdline");
						while(getline(pread, tmpstr, '\0')) cmd += tmpstr + " ";
						pread.close();
						if (!cmd.empty()) cmd.pop_back();
					}
					if (fs::exists((string)d.path() + "/status")) {
						ifstream pread((string)d.path() + "/status");
						status.clear();
						while (!pread.eof()){
							getline(pread, status, ':');
							if (status == "Uid") {
								pread.ignore();
								getline(pread, uid, '\t');
								break;
							} else {
								pread.ignore(numeric_limits<streamsize>::max(), '\n');
							}
						}
						pread.close();
						user = (uid_user.contains(uid)) ? uid_user.at(uid) : "";
					}
					cache[pid] = make_tuple(name, cmd, user);
				}

				// //* Match filter if applicable
				if (!filter.empty() &&
					pid_str.find(filter) == string::npos &&
					get<0>(cache[pid]).find(filter) == string::npos &&
					get<1>(cache[pid]).find(filter) == string::npos &&
					get<2>(cache[pid]).find(filter) == string::npos
					) continue;

				//* Create tuple
				procs.push_back(make_tuple(pid, get<0>(cache[pid]), get<1>(cache[pid]), get<2>(cache[pid]), cpu, cpu_s));
			}
		}

		// auto st = time_ms();

		//* Sort processes vector
		ranges::sort(procs, [&sortint, &reverse](tuple<int, string, string, string, double, double>& a, tuple<int, string, string, string, double, double>& b) {
				switch (sortint) {
					case 0: return (reverse) ? get<0>(a) < get<0>(b) : get<0>(a) > get<0>(b);	//? Pid
					case 1: return (reverse) ? get<1>(a) < get<1>(b) : get<1>(a) > get<1>(b);	//? Program
					case 2: return (reverse) ? get<2>(a) < get<2>(b) : get<2>(a) > get<2>(b);	//? Command
					case 3: return (reverse) ? get<3>(a) < get<3>(b) : get<3>(a) > get<3>(b);	//? User
					case 4: return (reverse) ? get<4>(a) < get<4>(b) : get<4>(a) > get<4>(b);	//? Cpu direct
					case 5: return (reverse) ? get<5>(a) < get<5>(b) : get<5>(a) > get<5>(b);	//? Cpu lazy
				}
				return false;
			}
		);

		//* When using "cpu lazy" sorting, push processes with high cpu usage to the front regardless of cumulative usage
		if (sortint == 5 && !reverse) {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 && get<4>(procs[i]) > max) max = get<4>(procs[i]);
				else if (i == 6) target = (max > 30.0) ? max : 10.0;
				if (i == offset && get<4>(procs[i]) > 30.0) offset++;
				else if (get<4>(procs[i]) > target) rotate(procs.begin() + offset, procs.begin() + i, procs.begin() + i + 1);
			}
		}



		// cout << "Sort took: " << time_ms() - st << "ms " << flush;



		//* Clear all cached values at a regular interval
		if (++counter >= 10000 || (filter.empty() && cache.size() > procs.size() + 100)) {
			counter = 0;
			cache.clear();
			cpu_times.clear();
			cpu_second.clear();
		}

		tstamp = time_ms();
		running.store(false);
		return procs;
	}

	Processes() {
		clk_tck = sysconf(_SC_CLK_TCK);
		tstamp = time_ms();
		stop.store(false);
		collect();
	}
};



#endif
