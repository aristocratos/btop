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
#include <deque>
#include <array>
#include <unordered_map>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <list>


#include <unistd.h>

#include <btop_config.h>
#include <btop_globs.h>
#include <btop_tools.h>


using std::string, std::vector, std::array, std::ifstream, std::atomic, std::numeric_limits, std::streamsize, std::unordered_map, std::deque, std::list;
namespace fs = std::filesystem;
using namespace Tools;

namespace Global {

	const string System = "linux";
	fs::path proc_path;

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
		size_t numpids = 500;
		long int clk_tck;
		struct p_cache {
			string name, cmd, user;
			uint64_t cpu_t = 0, cpu_s = 0;
		};
		unordered_map<uint, p_cache> cache;
		unordered_map<string, string> uid_user;
		fs::path passwd_path;
		fs::file_time_type passwd_time;
		uint counter = 0;
		long page_size = sysconf(_SC_PAGE_SIZE);

	}

	atomic<bool> stop (false);
	atomic<bool> running (false);
	array<string, 8> sort_array = {
		"pid",
		"name",
		"command",
		"threads",
		"user",
		"memory",
		"cpu direct",
		"cpu lazy",
	};
	unordered_map<string, uint> sort_map;

	//* proc_info: pid, name, cmd, threads, user, mem, cpu_p, cpu_c, state, tty, cpu_n, p_nice, ppid
	struct proc_info {
		uint pid;
		string name, cmd;
		size_t threads;
		string user;
		uint64_t mem;
		double cpu_p, cpu_c;
		char state;
		int cpu_n, p_nice;
		uint ppid;
	};


	//* Collects process information from /proc and returns a vector of proc_info structs
	auto collect(string sorting="pid", bool reverse=false, string filter=""){
		running.store(true);
		uint pid, ppid;
		uint64_t cpu_t, rss_mem;
		double cpu, cpu_s;
		bool new_cache;
		char state;
		int cpu_n, p_nice;
		size_t threads;
		ifstream pread;
		string pid_str, name, cmd, attr, user, instr, uid, status, tmpstr;
		auto since_last = time_ms() - tstamp;
		if (since_last < 1) since_last = 1;
		auto uptime = system_uptime();
		auto sortint = (sort_map.contains(sorting)) ? sort_map[sorting] : 7;
		vector<string> pstat;
		pstat.reserve(40);
		vector<proc_info> procs;
		procs.reserve((numpids + 10));
		vector<uint> c_pids;
		c_pids.reserve((numpids + 10));
		numpids = 0;
		uint parenthesis = 0;

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
			numpids++;
			pid_str = fs::path(d.path()).filename();
			cpu = 0.0; cpu_s = 0.0; cpu_t = 0; cpu_n = 0;
			rss_mem = 0; threads = 0; state = '0'; ppid = 0; p_nice = 0;
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
					pread.clear(); instr.clear(); pstat.clear(); pstat.reserve(40); pstat.resize(3, "");
					ifstream pread((string)d.path() + "/stat");
					if (pread.good()) {
						parenthesis = 1;
						pread.ignore(numeric_limits<streamsize>::max(), '(');
						while (parenthesis > 0 && !pread.eof()) {
							instr = pread.get();
							if (instr == "(") ++parenthesis;
							else if (instr == ")") --parenthesis;
						}
						pread.ignore(1);
						while (getline(pread, instr, ' ') && pstat.size() < 40) pstat.push_back(instr);
					}
					pread.close();

					if (pstat.size() < 22) continue;

					//? Process state
					state = pstat[3][0];

					//? Process parent pid
					ppid = stoul(pstat[4]);

					//? Process nice value
					p_nice = stoi(pstat[19]);

					//? Process number of threads
					threads = stoul(pstat[20]);

					//? Process utime + stime
					cpu_t = stoull(pstat[14]) + stoull(pstat[15]);

					//? Cache cpu times and cpu seconds
					if (new_cache) {
						cache[pid].cpu_t = cpu_t;
						cache[pid].cpu_s = stoull(pstat[22]);
					}

					//? CPU number last executed on
					if (pstat.size() > 39) cpu_n = stoi(pstat[39]);


					//? Process cpu usage since last update, 100'000 because (100 percent * 1000 milliseconds) for correct conversion
					cpu = static_cast<double>(100000 * (cpu_t - cache[pid].cpu_t) / since_last) / clk_tck;

					//? Process cumulative cpu usage since process start
					cpu_s = static_cast<double>((cpu_t / clk_tck) / (uptime - (cache[pid].cpu_s / clk_tck)));

					//? Add latest cpu times to cache
					cache[pid].cpu_t = cpu_t;
				}

				//* Get RSS memory in bytes from /proc/[pid]/statm
				if (fs::exists((string)d.path() + "/statm")) {
					pread.clear();
					ifstream pread((string)d.path() + "/statm");
					if (pread.good()) {
						pread.ignore(numeric_limits<streamsize>::max(), ' ');
						pread >> rss_mem;
						rss_mem *= page_size;
					}
					pread.close();
				}

				//* Match filter if defined
				if (!filter.empty() &&
					pid_str.find(filter) == string::npos &&
					cache[pid].name.find(filter) == string::npos &&
					cache[pid].cmd.find(filter) == string::npos &&
					cache[pid].user.find(filter) == string::npos
					) continue;

				//* Create proc_info
				procs.push_back(proc_info(pid, cache[pid].name, cache[pid].cmd, threads, cache[pid].user, rss_mem, cpu, cpu_s, state, cpu_n, p_nice, ppid));
			}
		}


		//* Sort processes vector
		std::ranges::sort(procs, [&sortint, &reverse](proc_info& a, proc_info& b)
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

		//* Clear dead processes from cache at a regular interval
		if (++counter >= 10000 || (filter.empty() && cache.size() > procs.size() + 100)) {
			unordered_map<uint, p_cache> r_cache;
			counter = 0;
			for (auto& p : c_pids) r_cache[p] = cache[p];
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
		for (auto& item : sort_array) sort_map[item] = i++;
	}
}



#endif
