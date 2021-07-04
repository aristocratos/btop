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

#if defined(__linux__)

#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <list>
#include <cmath>
#include <iostream>
#include <cmath>

#include <unistd.h>

#include <btop_shared.hpp>
#include <btop_config.hpp>
#include <btop_tools.hpp>



using 	std::string, std::vector, std::ifstream, std::atomic, std::numeric_limits, std::streamsize,
		std::round, std::string_literals::operator""s;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {
	double system_uptime(){
		string upstr;
		ifstream pread("/proc/uptime");
		getline(pread, upstr, ' ');
		pread.close();
		return stod(upstr);
	}
}

namespace Shared {

	fs::path proc_path;
	fs::path passwd_path;
	fs::file_time_type passwd_time;
	long page_size;
	long clk_tck;

	void init(){
		proc_path = (fs::is_directory(fs::path("/proc")) and access("/proc", R_OK) != -1) ? "/proc" : "";
		if (proc_path.empty()) {
			string errmsg = "Proc filesystem not found or no permission to read from it!";
			Logger::error(errmsg);
			std::cout << "ERROR: " << errmsg << std::endl;
			exit(1);
		}

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
	}

}

namespace Cpu {
	bool got_sensors = false;
	string cpuName = "";
}

namespace Mem {
	bool has_swap = false;
}

namespace Proc {
	namespace {
		struct p_cache {
			string name, cmd, user;
			size_t name_offset;
			uint64_t cpu_t = 0, cpu_s = 0;
			string prefix = "";
			size_t depth = 0;
			bool collapsed = false;
		};

		vector<proc_info> current_procs;
		unordered_flat_map<size_t, p_cache> cache;
		unordered_flat_map<string, string> uid_user;

		int counter = 0;
	}
	uint64_t old_cputimes = 0;
	size_t numpids = 500;
	atomic<bool> stop (false);
	atomic<bool> collecting (false);
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
	unordered_flat_map<char, string> proc_states = {
		{'R', "Running"},
		{'S', "Sleeping"},
		{'D', "Waiting"},
		{'Z', "Zombie"},
		{'T', "Stopped"},
		{'t', "Tracing"},
		{'X', "Dead"},
		{'x', "Dead"},
		{'K', "Wakekill"},
		{'W', "Unknown"},
		{'P', "Parked"}
	};

	detail_container detailed;

	//* Generate process tree list
	void _tree_gen(const proc_info& cur_proc, const vector<proc_info>& in_procs, vector<proc_info>& out_procs, int cur_depth, const bool collapsed, const string& filter, bool found=false){
		auto cur_pos = out_procs.size();
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not filter.empty() and not found) {
			if (not s_contains(std::to_string(cur_proc.pid), filter)
			and not s_contains(cur_proc.name, filter)
			and not s_contains(cur_proc.cmd, filter)
			and not s_contains(cur_proc.user, filter)) {
				filtering = true;
			}
			else {
				found = true;
				cur_depth = 0;
			}
		}

		if (not collapsed and not filtering)
			out_procs.push_back(cur_proc);

		int children = 0;
		for (auto& p : rng::equal_range(in_procs, cur_proc.pid, rng::less{}, &proc_info::ppid)) {
			if (collapsed and not filtering) {
				out_procs.back().cpu_p += p.cpu_p;
				out_procs.back().mem += p.mem;
				out_procs.back().threads += p.threads;
			}
			else children++;
			_tree_gen(p, in_procs, out_procs, cur_depth + 1, (collapsed ? true : cache.at(cur_proc.pid).collapsed), filter, found);
		}
		if (collapsed or filtering) return;

		if (out_procs.size() > cur_pos + 1 and not out_procs.back().prefix.ends_with("] "))
			out_procs.back().prefix.replace(out_procs.back().prefix.size() - 8, 8, " └─ ");

		out_procs.at(cur_pos).prefix = " │ "s * cur_depth + (children > 0 ? (cache.at(cur_proc.pid).collapsed ? "[+] " : "[-] ") : " ├─ ");
	}

	//* Get detailed info for selected process
	void _collect_details(size_t pid, uint64_t uptime, vector<proc_info>& procs, bool is_filtered){
		fs::path pid_path = Shared::proc_path / std::to_string(pid);

		if (pid != detailed.last_pid) {
			detailed.last_pid = pid;
			detailed.cpu_percent.clear();
			detailed.parent.clear();
			detailed.io_read.clear();
			detailed.io_write.clear();
			detailed.skip_smaps = false;
		}

		//? Copy proc_info for process from proc vector
		auto p = rng::find(procs, pid, &proc_info::pid);
		detailed.entry = *p;

		//? Update cpu percent deque for process cpu graph
		detailed.cpu_percent.push_back(round(detailed.entry.cpu_c));
		while (detailed.cpu_percent.size() > Term::width) detailed.cpu_percent.pop_front();

		//? Process runtime
		detailed.elapsed = sec_to_dhms(uptime - (cache.at(pid).cpu_s / Shared::clk_tck));

		//? Get parent process name
		if (detailed.parent.empty() and cache.contains(detailed.entry.ppid)) detailed.parent = cache.at(detailed.entry.ppid).name;

		//? Expand process status from single char to explanative string
		detailed.status = (proc_states.contains(detailed.entry.state)) ? proc_states.at(detailed.entry.state) : "Unknown";

		ifstream d_read;
		string short_str;

		//? Try to get RSS mem from proc/[pid]/smaps
		detailed.memory.clear();
		if (not detailed.skip_smaps and fs::exists(pid_path / "smaps")) {
			d_read.open(pid_path / "smaps");
			if (d_read.good()) {
				uint64_t rss = 0;
				try {
					while (not d_read.eof()) {
						d_read.ignore(SSmax, 'R');
						if (d_read.peek() == 's') {
							d_read.ignore(SSmax, ':');
							getline(d_read, short_str, 'k');
							rss += stoull(short_str);
						}
					}
					if (rss == detailed.entry.mem >> 10)
						detailed.skip_smaps = true;
					else
						detailed.memory = floating_humanizer(rss, false, 1);
				}
				catch (std::invalid_argument const&) {}
				catch (std::out_of_range const&) {}
			}
			d_read.close();
		}
		if (detailed.memory.empty()) detailed.memory = floating_humanizer(detailed.entry.mem, false);

		//? Get bytes read and written from proc/[pid]/io
		if (fs::exists(pid_path / "io")) {
			d_read.open(pid_path / "io");
			if (d_read.good()) {
				try {
					string name;
					while (not d_read.eof()) {
						getline(d_read, name, ':');
						if (name.ends_with("read_bytes")) {
							getline(d_read, short_str);
							detailed.io_read = floating_humanizer(stoull(short_str));
						}
						else if (name.ends_with("write_bytes")) {
							getline(d_read, short_str);
							detailed.io_write = floating_humanizer(stoull(short_str));
							break;
						}
						else
							d_read.ignore(SSmax, '\n');
					}
				}
				catch (std::invalid_argument const&) {}
				catch (std::out_of_range const&) {}
			}
			d_read.close();
		}

		if (is_filtered) procs.erase(p);

	}

	//* Collects and sorts process information from /proc
	vector<proc_info>& collect(bool return_last){
		if (return_last) return current_procs;
		atomic_wait_set(collecting);
		auto& sorting = Config::getS("proc_sorting");
		auto reverse = Config::getB("proc_reversed");
		auto& filter = Config::getS("proc_filter");
		auto per_core = Config::getB("proc_per_core");
		auto tree = Config::getB("proc_tree");
		auto show_detailed = Config::getB("show_detailed");
		size_t detailed_pid = Config::getI("detailed_pid");
		ifstream pread;
		string long_string;
		string short_str;
		double uptime = system_uptime();
		vector<proc_info> procs;
		procs.reserve((numpids + 10));
		int npids = 0;
		int cmult = (per_core) ? Global::coreCount : 1;
		bool got_detailed = false;
		bool detailed_filtered = false;

		//? Update uid_user map if /etc/passwd changed since last run
		if (not Shared::passwd_path.empty() and fs::last_write_time(Shared::passwd_path) != Shared::passwd_time) {
			string r_uid, r_user;
			Shared::passwd_time = fs::last_write_time(Shared::passwd_path);
			uid_user.clear();
			pread.open(Shared::passwd_path);
			if (pread.good()) {
				while (not pread.eof()){
					getline(pread, r_user, ':');
					pread.ignore(SSmax, ':');
					getline(pread, r_uid, ':');
					uid_user[r_uid] = r_user;
					pread.ignore(SSmax, '\n');
				}
			}
			pread.close();
		}

		//* Get cpu total times from /proc/stat
		uint64_t cputimes = 0;
		pread.open(Shared::proc_path / "stat");
		if (pread.good()) {
			pread.ignore(SSmax, ' ');
			for (uint64_t times; pread >> times; cputimes += times);
			pread.close();
		}
		else return current_procs;

		//* Iterate over all pids in /proc
		for (auto& d: fs::directory_iterator(Shared::proc_path)){
			if (pread.is_open()) pread.close();
			if (stop) {
				collecting = false;
				stop = false;
				return current_procs;
			}

			string pid_str = d.path().filename();
			if (not isdigit(pid_str[0])) continue;

			npids++;
			proc_info new_proc (stoul(pid_str));

			//* Cache program name, command and username
			if (not cache.contains(new_proc.pid)) {
				string name, cmd, user;
				pread.open(d.path() / "comm");
				if (not pread.good()) continue;
				getline(pread, name);
				pread.close();
				size_t name_offset = rng::count(name, ' ');


				pread.open(d.path() / "cmdline");
				if (not pread.good()) continue;
				long_string.clear();
				while(getline(pread, long_string, '\0')) cmd += long_string + ' ';
				pread.close();
				if (not cmd.empty()) cmd.pop_back();


				pread.open(d.path() / "status");
				if (not pread.good()) continue;
				string uid;
				string line;
				while (not pread.eof()){
					getline(pread, line, ':');
					if (line == "Uid") {
						pread.ignore();
						getline(pread, uid, '\t');
						break;
					} else {
						pread.ignore(SSmax, '\n');
					}
				}
				pread.close();
				user = (uid_user.contains(uid)) ? uid_user.at(uid) : uid;

				cache[new_proc.pid] = {name, cmd, user, name_offset};
			}

			//* Match filter if defined
			if (not tree and not filter.empty()
				and not s_contains(pid_str, filter)
				and not s_contains(cache[new_proc.pid].name, filter)
				and not s_contains(cache[new_proc.pid].cmd, filter)
				and not s_contains(cache[new_proc.pid].user, filter)) {
					if (show_detailed and new_proc.pid == detailed_pid)
						detailed_filtered = true;
					else
						continue;
			}
			new_proc.name = cache[new_proc.pid].name;
			new_proc.cmd = cache[new_proc.pid].cmd;
			new_proc.user = cache[new_proc.pid].user;

			//* Parse /proc/[pid]/stat
			pread.open(d.path() / "stat");
			if (not pread.good()) continue;

			//? Check cached value for whitespace characters in name and set offset to get correct fields from stat file
			size_t& offset = cache.at(new_proc.pid).name_offset;
			short_str.clear();
			size_t x = 0, next_x = 3;
			uint64_t cpu_t = 0;
			try {
				for (;;) {
					while (++x - offset < next_x) {
						pread.ignore(SSmax, ' ');
					}

					getline(pread, short_str, ' ');

					switch (x-offset) {
						case 3: { //? Process state
							new_proc.state = short_str[0];
							continue;
						}
						case 4: { //? Parent pid
							new_proc.ppid = stoull(short_str);
							next_x = 14;
							continue;
						}
						case 14: { //? Process utime
							cpu_t = stoull(short_str);
							continue;
						}
						case 15: { //? Process stime
							cpu_t += stoull(short_str);
							next_x = 19;
							continue;
						}
						case 19: { //? Nice value
							new_proc.p_nice = stoull(short_str);
							continue;
						}
						case 20: { //? Number of threads
							new_proc.threads = stoull(short_str);
							next_x = (cache[new_proc.pid].cpu_s == 0) ? 22 : 24;
							continue;
						}
						case 22: { //? Save cpu seconds to cache if missing
							cache[new_proc.pid].cpu_s = stoull(short_str);
							next_x = 24;
							continue;
						}
						case 24: { //? RSS memory (can be inaccurate, but parsing smaps increases total cpu usage by ~20x)
							new_proc.mem = stoull(short_str) * Shared::page_size;
							next_x = 40;
							continue;
						}
						case 40: { //? CPU number last executed on
							new_proc.cpu_n = stoull(short_str);
							goto stat_loop_done;
						}
					}
				}

			}
			catch (std::invalid_argument const&) { continue; }
			catch (std::out_of_range const&) { continue; }
			catch (std::ios_base::failure const&) {}

			stat_loop_done:
			pread.close();

			if (x-offset < 24) continue;

			//? Process cpu usage since last update
			new_proc.cpu_p = round(cmult * 1000 * (cpu_t - cache[new_proc.pid].cpu_t) / (cputimes - old_cputimes)) / 10.0;

			//? Process cumulative cpu usage since process start
			new_proc.cpu_c = ((double)cpu_t / Shared::clk_tck) / (uptime - (cache[new_proc.pid].cpu_s / Shared::clk_tck));

			//? Update cache with latest cpu times
			cache[new_proc.pid].cpu_t = cpu_t;

			if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
				got_detailed = true;
			}

			//? Push process to vector
			procs.push_back(new_proc);

		}

		//* Update the details info box for process if active
		if (show_detailed and got_detailed) {
			_collect_details(detailed_pid, round(uptime), procs, detailed_filtered);
		}
		else if (show_detailed and not got_detailed) {
			detailed.status = "Dead";
		}

		//* Sort processes
		auto cmp = [&reverse](const auto &a, const auto &b) { return (reverse ? a < b : a > b); };
		switch (v_index(sort_vector, sorting)) {
				case 0: { rng::sort(procs, cmp, &proc_info::pid); 	  break; }
				case 1: { rng::sort(procs, cmp, &proc_info::name);	  break; }
				case 2: { rng::sort(procs, cmp, &proc_info::cmd); 	  break; }
				case 3: { rng::sort(procs, cmp, &proc_info::threads); break; }
				case 4: { rng::sort(procs, cmp, &proc_info::user); 	  break; }
				case 5: { rng::sort(procs, cmp, &proc_info::mem); 	  break; }
				case 6: { rng::sort(procs, cmp, &proc_info::cpu_p);   break; }
				case 7: { rng::sort(procs, cmp, &proc_info::cpu_c);   break; }
		}

		//* When sorting with "cpu lazy" push processes over threshold cpu usage to the front regardless of cumulative usage
		if (not tree and not reverse and sorting == "cpu lazy") {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 and procs[i].cpu_p > max)
					max = procs[i].cpu_p;
				else if (i == 6)
					target = (max > 30.0) ? max : 10.0;
				if (i == offset and procs[i].cpu_p > 30.0)
					offset++;
				else if (procs[i].cpu_p > target)
					rotate(procs.begin() + offset, procs.begin() + i, procs.begin() + i + 1);
			}
		}

		//* Generate tree view if enabled
		if (tree) {
			vector<proc_info> tree_procs;
			tree_procs.reserve(procs.size());

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(procs, rng::less{}, &proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(procs, procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, procs, tree_procs, 0, cache.at(p.pid).collapsed, filter);
			}
			procs.swap(tree_procs);
		}


		//* Clear dead processes from cache at a regular interval
		if (++counter >= 10000 or ((int)cache.size() > npids + 100)) {
			counter = 0;
			unordered_flat_map<size_t, p_cache> r_cache;
			r_cache.reserve(procs.size());
			rng::for_each(procs, [&r_cache](const auto &p){
				if (cache.contains(p.pid))
					r_cache[p.pid] = cache.at(p.pid);
			});
			cache.swap(r_cache);
		}

		old_cputimes = cputimes;
		current_procs.swap(procs);
		numpids = npids;
		collecting = false;
		return current_procs;
	}
}

#endif