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
#define _btop_linux_included_ 1

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

	const string System = "linux";
	filesystem::path proc_path;

}

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

double system_uptime(){
	string upstr;
	ifstream pread("/proc/uptime");
	getline(pread, upstr, ' ');
	pread.close();
	return stod(upstr);
}

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

namespace Proc {
	namespace {
		uint64_t tstamp;
		long int clk_tck;
		struct p_cache {
			string name, cmd, user;
			uint64_t cpu_t = 0, cpu_s = 0;
		};
		map<uint, p_cache> cache;
		map<string, string> uid_user;
		fs::path passwd_path;
		fs::file_time_type passwd_time;
		uint counter = 0;
		long page_size = sysconf(_SC_PAGE_SIZE);

	}

	atomic<bool> stop (false);
	atomic<bool> running (false);
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

	//* proc_info: pid, name, cmd, threads, user, mem, cpu_p, cpu_c
	struct proc_info {
		uint pid;
		string name, cmd;
		size_t threads;
		string user;
		uint64_t mem;
		double cpu_p, cpu_c;
	};


	//* Collects process information from /proc and returns a vector of proc_info structs
	auto collect(string sorting="pid", bool reverse=false, string filter=""){
		running.store(true);
		uint pid;
		uint64_t cpu_t, rss_mem;
		double cpu, cpu_s;
		bool new_cache;
		size_t threads;
		ifstream pread;
		string pid_str, name, cmd, attr, user, instr, uid, status, tmpstr;
		auto since_last = time_ms() - tstamp;
		if (since_last < 1) since_last = 1;
		auto uptime = system_uptime();
		auto sortint = (sort_map.contains(sorting)) ? sort_map[sorting] : 7;
		vector<string> pstat;
		vector<proc_info> procs;
		vector<uint> c_pids;

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


		//* Iterate over all pids in /proc and get relevant values
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
			new_cache = false;
			if (d.is_directory() && isdigit(pid_str[0])) {
				pid = stoul(pid_str);
				c_pids.push_back(pid);

				//* Cache program name, command and username
				if (!cache.contains(pid)) {
					name.clear(); cmd.clear(); user.clear();
					new_cache = true;
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
					cache[pid] = p_cache(name, cmd, user);
				}

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

					//? Cache cpu times and cpu seconds
					if (new_cache) {
						cache[pid].cpu_t = cpu_t;
						cache[pid].cpu_s = stoull(pstat[21]);
					}

					//? Cache process start time
					// if (!cpu_second.contains(pid)) cpu_second[pid] = stoull(pstat[21]);

					//? Process cpu usage since last update, 100'000 because (100 percent * 1000 milliseconds) for correct conversion
					cpu = static_cast<double>(100000 * (cpu_t - cache[pid].cpu_t) / since_last) / clk_tck;

					//? Process cumulative cpu usage since process start
					cpu_s = static_cast<double>((cpu_t / clk_tck) / (uptime - (cache[pid].cpu_s / clk_tck)));

					//? Add latest cpu times to cache
					cache[pid].cpu_t = cpu_t;
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



				// //* Match filter if applicable
				if (!filter.empty() &&
					pid_str.find(filter) == string::npos &&				//? Pid
					cache[pid].name.find(filter) == string::npos &&		//? Program
					cache[pid].cmd.find(filter) == string::npos &&		//? Command
					cache[pid].user.find(filter) == string::npos 		//? User
					) continue;

				//* Create proc_info
				procs.push_back(proc_info(pid, cache[pid].name, cache[pid].cmd, threads, cache[pid].user, rss_mem, cpu, cpu_s));
			}
		}

		// auto st = time_ms();

		//* Sort processes vector
		ranges::sort(procs, [&sortint, &reverse](proc_info& a, proc_info& b)
			{
				switch (sortint) {
					case 0: return (reverse) ? a.pid < b.pid : a.pid > b.pid;
					case 1: return (reverse) ? a.name < b.name : a.name > b.name;
					case 2: return (reverse) ? a.cmd < b.cmd : a.cmd > b.cmd;
					case 3: return (reverse) ? a.threads < b.threads : a.threads > b.threads;
					case 4: return (reverse) ? a.user < b.user : a.user > b.user;
					case 5: return (reverse) ? a.mem < b.mem : a.mem > b.mem;
					case 6: return (reverse) ? a.cpu_p < b.cpu_p : a.cpu_p > b.cpu_p;
					case 7: return (reverse) ? a.cpu_c < b.cpu_c : a.cpu_c > b.cpu_c;
				}
				return false;
			}
		);

		//* When using "cpu lazy" sorting push processes with high cpu usage to the front regardless of cumulative usage
		if (sortint == 7 && !reverse) {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 && procs[i].cpu_p > max) max = procs[i].cpu_p;
				else if (i == 6) target = (max > 30.0) ? max : 10.0;
				if (i == offset && procs[i].cpu_p > 30.0) offset++;
				else if (procs[i].cpu_p > target) rotate(procs.begin() + offset, procs.begin() + i, procs.begin() + i + 1);
			}
		}

		//* Clear all cached values at a regular interval to get rid of dead processes
		if (++counter >= 5 || (filter.empty() && cache.size() > procs.size() + 100)) {
			map<uint, p_cache> r_cache;
			counter = 0;
			for (auto& p : c_pids){
				r_cache[p] = cache[p];
			}
			cache = move(r_cache);

		}

		tstamp = time_ms();
		running.store(false);
		return procs;
	}

	//* Initialize needed variables for collect
	void init(){
		clk_tck = sysconf(_SC_CLK_TCK);
		tstamp = time_ms();
		passwd_path = (fs::exists(fs::path("/etc/passwd"))) ? fs::path("/etc/passwd") : passwd_path;
		uint i = 0;
		for (auto& item : sort_vector) sort_map[item] = i++;
	}
};



#endif
