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

namespace Global {

	const string SYSTEM = "linux";
	filesystem::path proc_path;

}

namespace Proc {
	namespace {
		uint64_t tstamp;
		long int clk_tck;
		map<int, tuple<string, string, string>> cache;
		map<string, string> uid_user;
		fs::path passwd_path;
		fs::file_time_type passwd_time;
		map<int, uint64_t> cpu_times;
		map<int, uint64_t> cpu_second;
		uint counter = 0;
		long page_size = sysconf(_SC_PAGE_SIZE);
	}

	atomic<bool> stop;
	atomic<bool> running;
	vector<string> sort_vector = {
		"pid",
		"name",
		"command",
		"threads",
		"user",
		"memory",
		"cpu direct",
		"cpu lazy",
	};
	map<string, uint> sort_map;



	//* Collects process information from /proc and returns a vector of tuples
	auto collect(string sorting="pid", bool reverse=false, string filter=""){
		running.store(true);
		int pid;
		uint64_t cpu_t, rss_mem;
		double cpu, cpu_s;
		size_t threads;
		ifstream pread;
		string pid_str, name, cmd, attr, user, instr, uid, status, tmpstr, smap;
		auto since_last = time_ms() - tstamp;
		if (since_last < 1) since_last = 1;
		auto uptime = system_uptime();
		auto sortint = (sort_map.contains(sorting)) ? sort_map[sorting] : 7;
		vector<string> pstat;

		//? Return type! Values in tuple: pid, program, command, threads, username, mem KiB, cpu%, cpu cumulative
		vector<tuple<int, string, string, size_t, string, uint64_t, double, double>> procs;

		//* Update uid_user map if /etc/passwd changed since last run
		if (!passwd_path.empty() && fs::last_write_time(passwd_path) != passwd_time) {
			string r_uid, r_user;
			passwd_time = fs::last_write_time(passwd_path);
			uid_user.clear();
			ifstream pread(passwd_path);
			if (pread.good()) {
				while (true){
					getline(pread, r_user, ':');
					pread.ignore(numeric_limits<streamsize>::max(), ':');
					getline(pread, r_uid, ':');
					uid_user[r_uid] = r_user;
					pread.ignore(numeric_limits<streamsize>::max(), '\n');
					if (pread.eof()) break;
				}
			}
			pread.close();
		}


		//* Iterate over all pid directories in /proc and get relevant values
		for (auto& d: fs::directory_iterator(Global::proc_path)){
			if (stop.load()) {
				procs.clear();
				running.store(false);
				stop.store(false);
				return procs;
			}
			pid_str = fs::path(d.path()).filename();
			cpu = 0.0;
			rss_mem = 0;
			if (d.is_directory() && isdigit(pid_str[0])) {
				pid = stoi(pid_str);

				//* Get cpu usage, cpu cumulative and threads from /proc/[pid]/stat
				if (fs::exists((string)d.path() + "/stat")) {
					pread.clear(); pstat.clear();
					ifstream pread((string)d.path() + "/stat");
					if (pread.good()) while (getline(pread, instr, ' ')) pstat.push_back(instr);
					pread.close();

					if (pstat.size() < 37) continue;

					//? Process number of threads
					threads = stoul(pstat[19]);

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

				//* Get RSS memory in bytes from /proc/[pid]/statm
				if (fs::exists((string)d.path() + "/statm")) {
					pread.clear(); tmpstr.clear();
					ifstream pread((string)d.path() + "/statm");
					if (pread.good()) {
						pread.ignore(numeric_limits<streamsize>::max(), ' ');
						pread >> rss_mem;
						rss_mem *= page_size;
					}
					pread.close();
				}

				//* Cache program name, command and username
				if (!cache.contains(pid)) {
					if (fs::exists((string)d.path() + "/comm")) {
						pread.clear(); name.clear();
						ifstream pread((string)d.path() + "/comm");
						if (pread.good()) getline(pread, name);
						pread.close();
					}
					if (fs::exists((string)d.path() + "/cmdline")) {
						pread.clear(); cmd.clear(); tmpstr.clear();
						ifstream pread((string)d.path() + "/cmdline");
						if (pread.good()) while(getline(pread, tmpstr, '\0')) cmd += tmpstr + " ";
						pread.close();
						if (!cmd.empty()) cmd.pop_back();
					}
					if (fs::exists((string)d.path() + "/status")) {
						pread.clear(); status.clear(); uid.clear();
						ifstream pread((string)d.path() + "/status");
						if (pread.good()) {
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
						}
						pread.close();
						user = (!uid.empty() && uid_user.contains(uid)) ? uid_user.at(uid) : uid;
					}
					cache[pid] = make_tuple(name, cmd, user);
				}

				// //* Match filter if applicable
				if (!filter.empty() &&
					pid_str.find(filter) == string::npos &&				//? Pid
					get<0>(cache[pid]).find(filter) == string::npos &&	//? Program
					get<1>(cache[pid]).find(filter) == string::npos &&	//? Command
					get<2>(cache[pid]).find(filter) == string::npos 	//? User
					) continue;

				//* Create tuple
				procs.push_back(make_tuple(pid, get<0>(cache[pid]), get<1>(cache[pid]), threads, get<2>(cache[pid]), rss_mem, cpu, cpu_s));
			}
		}

		// auto st = time_ms();

		//* Sort processes vector
		ranges::sort(procs, [&sortint, &reverse](	tuple<int, string, string, size_t, string, uint64_t, double, double>& a,
													tuple<int, string, string, size_t, string, uint64_t, double, double>& b)
			{
				switch (sortint) {
					case 0: return (reverse) ? get<0>(a) < get<0>(b) : get<0>(a) > get<0>(b);	//? Pid
					case 1: return (reverse) ? get<1>(a) < get<1>(b) : get<1>(a) > get<1>(b);	//? Program
					case 2: return (reverse) ? get<2>(a) < get<2>(b) : get<2>(a) > get<2>(b);	//? Command
					case 3: return (reverse) ? get<3>(a) < get<3>(b) : get<3>(a) > get<3>(b);	//? Threads
					case 4: return (reverse) ? get<4>(a) < get<4>(b) : get<4>(a) > get<4>(b);	//? User
					case 5: return (reverse) ? get<5>(a) < get<5>(b) : get<5>(a) > get<5>(b);	//? Memory
					case 6: return (reverse) ? get<6>(a) < get<6>(b) : get<6>(a) > get<6>(b);	//? Cpu direct
					case 7: return (reverse) ? get<7>(a) < get<7>(b) : get<7>(a) > get<7>(b);	//? Cpu lazy
				}
				return false;
			}
		);

		//* When using "cpu lazy" sorting push processes with high cpu usage to the front regardless of cumulative usage
		if (sortint == 6 && !reverse) {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 && get<6>(procs[i]) > max) max = get<6>(procs[i]);
				else if (i == 6) target = (max > 30.0) ? max : 10.0;
				if (i == offset && get<6>(procs[i]) > 30.0) offset++;
				else if (get<6>(procs[i]) > target) rotate(procs.begin() + offset, procs.begin() + i, procs.begin() + i + 1);
			}
		}

		//* Clear all cached values at a regular interval to get rid of dead processes
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

	void init(){
		clk_tck = sysconf(_SC_CLK_TCK);
		tstamp = time_ms();
		stop.store(false);
		passwd_path = (fs::exists(fs::path("/etc/passwd"))) ? fs::path("/etc/passwd") : passwd_path;
		uint i = 0;
		for (auto& item : sort_vector) sort_map[item] = i++;
		// collect();
	}
};



#endif
