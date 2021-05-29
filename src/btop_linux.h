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
#include <atomic>
#include <future>
#include <thread>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <ranges>
#include <list>
#include <robin_hood.h>

#include <unistd.h>

#include <btop_config.h>
#include <btop_globs.h>
#include <btop_tools.h>



using std::string, std::vector, std::array, std::ifstream, std::atomic, std::numeric_limits, std::streamsize;
using std::cout, std::flush, std::endl;
namespace fs = std::filesystem;
using namespace Tools;
const auto SSmax = std::numeric_limits<streamsize>::max();

namespace Global {

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
		struct p_cache {
			string name, cmd, user;
			uint64_t cpu_t = 0, cpu_s = 0;
		};
		unordered_flat_map<uint, p_cache> cache;
		unordered_flat_map<string, string> uid_user;
		fs::path passwd_path;
		fs::file_time_type passwd_time;
		uint counter = 0;
		long page_size;
		long clk_tck;

	}

	size_t numpids = 500;
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
	unordered_flat_map<string, uint> sort_map;

	//* proc_info: pid, name, cmd, threads, user, mem, cpu_p, cpu_c, state, cpu_n, p_nice, ppid
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
		size_t threads, s_pos, c_pos, s_count;
		ifstream pread;
		string pid_str, name, cmd, attr, user, instr, uid, status, tmpstr;
		auto since_last = time_ms() - tstamp;
		if (since_last < 1) since_last = 1;
		auto uptime = system_uptime();
		auto sortint = (sort_map.contains(sorting)) ? sort_map[sorting] : 7;
		vector<proc_info> procs;
		procs.reserve((numpids + 10));
		numpids = 0;

		//* Update uid_user map if /etc/passwd changed since last run
		if (!passwd_path.empty() && fs::last_write_time(passwd_path) != passwd_time) {
			string r_uid, r_user;
			passwd_time = fs::last_write_time(passwd_path);
			uid_user.clear();
			pread.open(passwd_path);
			if (pread.good()) {
				while (!pread.eof()){
					getline(pread, r_user, ':');
					pread.ignore(SSmax, ':');
					getline(pread, r_uid, ':');
					uid_user[r_uid] = r_user;
					pread.ignore(SSmax, '\n');
				}
			}
			pread.close();
		}

		//* Iterate over all pids in /proc and get relevant values
		for (auto& d: fs::directory_iterator(Global::proc_path)){
			if (pread.is_open()) pread.close();
			if (stop.load()) {
				procs.clear();
				running.store(false);
				stop.store(false);
				return procs;
			}
			pid_str = d.path().filename();
			cpu = 0.0; cpu_s = 0.0; cpu_t = 0; cpu_n = 0;
			rss_mem = 0; threads = 0; state = '0'; ppid = 0; p_nice = 0;
			new_cache = false;
			if (d.is_directory() && isdigit(pid_str[0])) {
				numpids++;
				pid = stoul(pid_str);

				//* Cache program name, command and username
				if (!cache.contains(pid)) {
					name.clear(); cmd.clear(); user.clear();
					new_cache = true;
					pread.open(d.path() / "comm");
					if (pread.good()) {
						getline(pread, name);
						pread.close();
					}
					else continue;

					pread.open(d.path() / "cmdline");
					if (pread.good()) {
						while(getline(pread, tmpstr, '\0')) cmd += tmpstr + " ";
						pread.close();
						if (!cmd.empty()) cmd.pop_back();
					}
					else continue;

					pread.open(d.path() / "status");
					if (pread.good()) {
						uid.clear();
						while (!pread.eof()){
							getline(pread, status, ':');
							if (status == "Uid") {
								pread.ignore();
								getline(pread, uid, '\t');
								break;
							} else {
								pread.ignore(SSmax, '\n');
							}
						}
						pread.close();
						user = (!uid.empty() && uid_user.contains(uid)) ? uid_user.at(uid) : uid;
					}
					else continue;
					cache[pid] = {name, cmd, user};
				}

				//* Match filter if defined
				if (!filter.empty()
					&& pid_str.find(filter) == string::npos
					&& cache[pid].name.find(filter) == string::npos
					&& cache[pid].cmd.find(filter) == string::npos
					&& cache[pid].user.find(filter) == string::npos) {
						if (new_cache) cache.erase(pid);
						continue;
				}

				//* Parse /proc/[pid]/stat
				pread.open(d.path() / "stat");
				if (pread.good()) {
					getline(pread, instr);
					pread.close();
					s_pos = 0; c_pos = 0; s_count = 0;

					//? Skip pid and comm field and find comm fields closing ')'
					s_pos = instr.find_last_of(')') + 2;
					if (s_pos == string::npos) continue;

					do {
						c_pos = instr.find(' ', s_pos);
						if (c_pos == string::npos) break;

						switch (s_count) {
							case 0: { //? Process state
								state = instr[s_pos];
								break;
							}
							case 1: { //? Process parent pid
								ppid = stoul(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
							case 11: { //? Process utime
								cpu_t = stoull(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
							case 12: { //? Process stime
								cpu_t += stoull(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
							case 16: { //? Process nice value
								p_nice = stoi(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
							case 17: { //? Process number of threads
								threads = stoul(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
							case 19: { //? Cache cpu times and cpu seconds
								if (new_cache) {
									cache[pid].cpu_t = cpu_t;
									cache[pid].cpu_s = stoull(instr.substr(s_pos, c_pos - s_pos));
								};
								break;
							}
							case 36: { //? CPU number last executed on
								cpu_n = stoi(instr.substr(s_pos, c_pos - s_pos));
								break;
							}
						}
						s_pos = c_pos + 1;
					} while (s_count++ < 36);

					if (s_count < 19) continue;

					//? Process cpu usage since last update, 100'000 because (100 percent * 1000 milliseconds) for correct conversion
					cpu = static_cast<double>(100000 * (cpu_t - cache[pid].cpu_t) / since_last) / clk_tck;

					//? Process cumulative cpu usage since process start
					cpu_s = static_cast<double>((cpu_t / clk_tck) / (uptime - (cache[pid].cpu_s / clk_tck)));

					//? Update cache with latest cpu times
					cache[pid].cpu_t = cpu_t;
				}
				else continue;

				//* Get RSS memory in bytes from /proc/[pid]/statm
				pread.open(d.path() / "statm");
				if (pread.good()) {
					pread.ignore(SSmax, ' ');
					pread >> rss_mem;
					pread.close();
					rss_mem *= page_size;
				}

				//* Create proc_info
				procs.emplace_back(pid, cache[pid].name, cache[pid].cmd, threads, cache[pid].user, rss_mem, cpu, cpu_s, state, cpu_n, p_nice, ppid);
			}
		}


		//* Sort processes vector
		std::ranges::sort(procs, [&sortint, &reverse](proc_info& a, proc_info& b) {
				switch (sortint) {
					case 0: return (reverse) ? a.pid < b.pid 			: a.pid > b.pid;
					case 1: return (reverse) ? a.name < b.name 			: a.name > b.name;
					case 2: return (reverse) ? a.cmd < b.cmd 			: a.cmd > b.cmd;
					case 3: return (reverse) ? a.threads < b.threads 	: a.threads > b.threads;
					case 4: return (reverse) ? a.user < b.user 			: a.user > b.user;
					case 5: return (reverse) ? a.mem < b.mem 			: a.mem > b.mem;
					case 6: return (reverse) ? a.cpu_p < b.cpu_p 		: a.cpu_p > b.cpu_p;
					case 7: return (reverse) ? a.cpu_c < b.cpu_c 		: a.cpu_c > b.cpu_c;
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
			unordered_flat_map<uint, p_cache> r_cache;
			r_cache.reserve(procs.size());
			counter = 0;
			if (filter.empty()) {
				for (auto& p : procs) r_cache[p.pid] = cache[p.pid];
				cache.swap(r_cache);
			}
			else cache.clear();
		}
		tstamp = time_ms();
		running.store(false);
		return procs;
	}

	//* Initialize needed variables for collect
	void init(){
		tstamp = time_ms();
		passwd_path = (access("/etc/passwd", R_OK) != -1) ? fs::path("/etc/passwd") : passwd_path;
		if (passwd_path.empty()) Logger::warning("Could not read /etc/passwd, will show UID instead of username.");

		page_size = sysconf(_SC_PAGE_SIZE);
		if (page_size <= 0) {
			page_size = 4096;
			Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
		}


		clk_tck = sysconf(_SC_CLK_TCK);
		if (clk_tck <= 0) {
			clk_tck = 100;
			Logger::warning("Could not get system clocks per second. Defaulting to 100, processes cpu usage might be incorrect.");
		}

		uint i = 0;
		for (auto& item : sort_array) sort_map[item] = i++;
	}
}



#endif
