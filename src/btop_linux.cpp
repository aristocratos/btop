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

#include <fstream>
#include <ranges>
#include <cmath>
#include <unistd.h>
#include <numeric>
#include <regex>

#include <btop_shared.hpp>
#include <btop_config.hpp>
#include <btop_tools.hpp>

using 	std::string, std::vector, std::ifstream, std::atomic, std::numeric_limits, std::streamsize,
		std::round, std::max, std::min, std::clamp, std::string_literals::operator""s;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Tools {
	double system_uptime() {
		string upstr;
		ifstream pread("/proc/uptime");
		getline(pread, upstr, ' ');
		pread.close();
		return stod(upstr);
	}
}

namespace Cpu {
	vector<uint64_t> core_old_totals;
	vector<uint64_t> core_old_idles;
	vector<string> available_fields;
	cpu_info current_cpu;
}

namespace Shared {

	fs::path procPath;
	fs::path passwd_path;
	fs::file_time_type passwd_time;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount;
	string cpuName;

	void init() {

		//? Shared global variables init
		procPath = (fs::is_directory(fs::path("/proc")) and access("/proc", R_OK) != -1) ? "/proc" : "";
		if (procPath.empty())
			throw std::runtime_error("Proc filesystem not found or no permission to read from it!");

		passwd_path = (fs::is_regular_file(fs::path("/etc/passwd")) and access("/etc/passwd", R_OK) != -1) ? "/etc/passwd" : "";
		if (passwd_path.empty())
			Logger::warning("Could not read /etc/passwd, will show UID instead of username.");

		coreCount = sysconf(_SC_NPROCESSORS_ONLN);
		if (coreCount < 1) {
			coreCount = 1;
			Logger::warning("Could not determine number of cores, defaulting to 1.");
		}

		pageSize = sysconf(_SC_PAGE_SIZE);
		if (pageSize <= 0) {
			pageSize = 4096;
			Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
		}

		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0) {
			clkTck = 100;
			Logger::warning("Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect.");
		}

		ifstream meminfo(Shared::procPath / "meminfo");
		if (meminfo.good()) {
			meminfo.ignore(SSmax, ':');
			meminfo >> totalMem;
			totalMem <<= 10;
		}
		if (not meminfo.good() or totalMem == 0)
			throw std::runtime_error("Could not get total memory size from /proc/meminfo");

		//? Init for namespace Cpu
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Cpu::collect();
		for (auto& [field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty()) Cpu::available_fields.push_back(field);
		}
		Cpu::cpuName = Cpu::get_cpuName();
	}

}

namespace Cpu {
	bool got_sensors = false;
	string cpuName;
	string cpuHz;

	const array<string, 10> time_names = {"user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"};

	unordered_flat_map<string, uint64_t> cpu_old = {
			{"totals", 0},
			{"idles", 0},
			{"user", 0},
			{"nice", 0},
			{"system", 0},
			{"idle", 0},
			{"iowait", 0},
			{"irq", 0},
			{"softirq", 0},
			{"steal", 0},
			{"guest", 0},
			{"guest_nice", 0}
	};

	string get_cpuName() {
		string name;
		ifstream cpuinfo(Shared::procPath / "cpuinfo");
		if (cpuinfo.good()) {
			for (string instr; getline(cpuinfo, instr, ':') and not instr.starts_with("model name");)
				cpuinfo.ignore(SSmax, '\n');
			if (cpuinfo.bad()) return name;
			cpuinfo.ignore(1);
			getline(cpuinfo, name);
			auto name_vec = ssplit(name);

			if ((s_contains(name, "Xeon") or v_contains(name_vec, "Duo")) and v_contains(name_vec, "CPU")) {
				auto cpu_pos = v_index(name_vec, "CPU"s);
				if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
					name = name_vec.at(cpu_pos + 1);
				else
					name.clear();
			}
			else if (v_contains(name_vec, "Ryzen")) {
				auto ryz_pos = v_index(name_vec, "Ryzen"s);
				name = "Ryzen"	+ (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "")
								+ (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
			}
			else if (s_contains(name, "Intel") and v_contains(name_vec, "CPU")) {
				auto cpu_pos = v_index(name_vec, "CPU"s);
				if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1) != "@")
					name = name_vec.at(cpu_pos + 1);
				else
					name.clear();
			}
			else
				name.clear();

			if (name.empty() and not name_vec.empty()) {
				for (const auto& n : name_vec) {
					if (n == "@") break;
					name += n + ' ';
				}
				name.pop_back();
				for (const auto& reg : {regex("Processor"), regex("CPU"), regex("\\(R\\)"), regex("\\(TM\\)"), regex("Intel"),
										regex("AMD"), regex("Core"), regex("\\d?\\.?\\d+[mMgG][hH][zZ]")}) {
					name = std::regex_replace(name, reg, "");
				}
				name = trim(name);
			}
		}

		return name;
	}

	string get_cpuHz() {
		static bool failed = false;
		if (failed) return "";
		string cpuhz;
		try {
			ifstream cpuinfo(Shared::procPath / "cpuinfo");
			if (cpuinfo.good()) {
				string instr;
				while (getline(cpuinfo, instr, ':') and not instr.starts_with("cpu MHz"))
					cpuinfo.ignore(SSmax, '\n');
				cpuinfo.ignore(1);
				getline(cpuinfo, instr);
				if (instr.empty()) throw std::runtime_error("");

				int hz_int = round(std::stod(instr));

				if (hz_int >= 1000) {
					if (hz_int >= 10000) cpuhz = to_string((int)round((double)hz_int / 1000)); // Future proof until we reach THz speeds :)
					else cpuhz = to_string(round((double)hz_int / 100) / 10.0).substr(0, 3);
					cpuhz += " GHz";
				}
				else if (hz_int > 0)
					cpuhz = to_string(hz_int) + " MHz";
			}
		}
		catch (...) {
			failed = true;
			Logger::warning("Failed to get cpu clock speed from /proc/cpuinfo.");
			cpuhz.clear();
		}

		return cpuhz;
	}

	cpu_info collect(const bool no_update) {
		if (no_update and not current_cpu.cpu_percent.at("total").empty()) return current_cpu;
		auto& cpu = current_cpu;
		// const auto& cpu_sensor = Config::getS("cpu_sensor");

		string short_str;
		ifstream cread;

		//? Get cpu total times for all cores from /proc/stat
		cread.open(Shared::procPath / "stat");
		if (cread.good()) {
			for (int i = 0; getline(cread, short_str, ' ') and short_str.starts_with("cpu"); i++) {

				//? Excepted on kernel 2.6.3> : 0=user, 1=nice, 2=system, 3=idle, 4=iowait, 5=irq, 6=softirq, 7=steal, 8=guest, 9=guest_nice
				vector<uint64_t> times;
				uint64_t total_sum = 0;

				for (uint64_t val; cread >> val; total_sum += val) {
					times.push_back(val);
				}
				cread.clear();
				if (times.size() < 4) throw std::runtime_error("Malformatted /proc/stat");

				//? Subtract fields 8-9 and any future unknown fields
				const uint64_t totals = total_sum - (times.size() > 8 ? std::accumulate(times.begin() + 8, times.end(), 0) : 0);

				//? Add iowait field if present
				const uint64_t idles = times[3] + (times.size() > 4 ? times[4] : 0);

				//? Calculate values for totals from first line of stat
				if (i == 0) {
					const uint64_t calc_totals = totals - cpu_old["totals"];
					const uint64_t calc_idles = idles - cpu_old["idles"];
					cpu_old["totals"] = totals;
					cpu_old["idles"] = idles;

					//? Total usage of cpu
					cpu.cpu_percent["total"].push_back(clamp((uint64_t)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ul, 100ul));

					//? Reduce size if there are more values than needed for graph
					while ((int)cpu.cpu_percent["total"].size() > Term::width * 2) cpu.cpu_percent["total"].pop_front();

					//? Populate cpu.cpu_percent with all fields from stat
					for (int ii = 0; const auto& val : times) {
						cpu.cpu_percent[time_names.at(ii)].push_back(clamp((uint64_t)round((double)(val - cpu_old[time_names.at(ii)]) * 100 / calc_totals), 0ul, 100ul));
						cpu_old[time_names.at(ii)] = val;

						//? Reduce size if there are more values than needed for graph
						while ((int)cpu.cpu_percent[time_names.at(ii)].size() > Term::width * 2) cpu.cpu_percent[time_names.at(ii)].pop_front();

						if (++ii == 10) break;
					}
				}
				//? Calculate cpu total for each core
				else {
					if (i > Shared::coreCount) break;
					const uint64_t calc_totals = totals - core_old_totals[i-1];;
					const uint64_t calc_idles = idles - core_old_idles[i-1];;
					core_old_totals[i-1] = totals;
					core_old_idles[i-1] = idles;

					cpu.core_percent[i-1].push_back(clamp((uint64_t)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ul, 100ul));

					//? Reduce size if there are more values than needed for graph
					if ((int)cpu.core_percent[i-1].size() > 20) cpu.core_percent[i-1].pop_front();

				}
			}
			cread.close();
		}
		else {
			throw std::runtime_error("Failed to read /proc/stat");
		}

		if (Config::getB("show_cpu_freq"))
			cpuHz = get_cpuHz();

		return cpu;
	}
}

namespace Mem {
	bool has_swap = false;

	mem_info current_mem;

	mem_info collect(const bool no_update) {
		(void)no_update;
		return current_mem;
	}

}

namespace Net {
	net_info current_net;

	net_info collect(const bool no_update) {
		(void)no_update;
		return current_net;
	}
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
		uint64_t cputimes;

		int counter = 0;
	}
	int collapse = -1, expand = -1;
	uint64_t old_cputimes = 0;
	atomic<int> numpids = 0;
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
	void _tree_gen(const proc_info& cur_proc, const vector<proc_info>& in_procs, vector<proc_info>& out_procs, int cur_depth, const bool collapsed, const string& filter, bool found=false) {
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

		if (not collapsed and not filtering) {
			out_procs.push_back(cur_proc);
			if (std::string_view cmd_view = cur_proc.cmd; not cmd_view.empty()) {
				cmd_view = cmd_view.substr(0, std::min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(std::min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				if (cmd_view == cur_proc.name)
					out_procs.back().cmd.clear();
				else
					out_procs.back().cmd = '(' + (string)cmd_view + ')';
			}
		}

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

		if (out_procs.size() > cur_pos + 1 and not out_procs.back().prefix.ends_with("]─"))
			out_procs.back().prefix.replace(out_procs.back().prefix.size() - 8, 8, " └─ ");

		out_procs.at(cur_pos).prefix = " │ "s * cur_depth + (children > 0 ? (cache.at(cur_proc.pid).collapsed ? "[+]─" : "[-]─") : " ├─ ");
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, const uint64_t uptime, vector<proc_info>& procs) {
		fs::path pid_path = Shared::procPath / std::to_string(pid);

		if (pid != detailed.last_pid) {
			detailed = {};
			detailed.last_pid = pid;
			detailed.skip_smaps = (not Config::getB("proc_info_smaps"));
		}

		//? Copy proc_info for process from proc vector
		auto p = rng::find(procs, pid, &proc_info::pid);
		detailed.entry = *p;

		//? Update cpu percent deque for process cpu graph
		if (not Config::getB("proc_per_core")) detailed.entry.cpu_p *= Shared::coreCount;
		detailed.cpu_percent.push_back(round(detailed.entry.cpu_p));
		while (detailed.cpu_percent.size() > (size_t)Term::width) detailed.cpu_percent.pop_front();

		//? Process runtime
		detailed.elapsed = sec_to_dhms(uptime - (cache.at(pid).cpu_s / Shared::clkTck));
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

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
					else {
						detailed.mem_bytes.push_back(rss << 10);
						detailed.memory = floating_humanizer(rss, false, 1);
					}
				}
				catch (const std::invalid_argument&) {}
				catch (const std::out_of_range&) {}
			}
			d_read.close();
		}
		if (detailed.memory.empty()) {
			detailed.mem_bytes.push_back(detailed.entry.mem);
			detailed.memory = floating_humanizer(detailed.entry.mem);
		}
		if (detailed.first_mem == -1 or detailed.first_mem < detailed.mem_bytes.back() / 2 or detailed.first_mem > detailed.mem_bytes.back() * 4) {
			detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Shared::totalMem);
			redraw = true;
		}

		while (detailed.mem_bytes.size() > (size_t)Term::width) detailed.mem_bytes.pop_front();

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
				catch (const std::invalid_argument&) {}
				catch (const std::out_of_range&) {}
			}
			d_read.close();
		}
	}

	//* Collects and sorts process information from /proc
	vector<proc_info> collect(const bool no_update) {
		const auto& sorting = Config::getS("proc_sorting");
		const auto& reverse = Config::getB("proc_reversed");
		const auto& filter = Config::getS("proc_filter");
		const auto& per_core = Config::getB("proc_per_core");
		const auto& tree = Config::getB("proc_tree");
		const auto& show_detailed = Config::getB("show_detailed");
		const size_t detailed_pid = Config::getI("detailed_pid");
		ifstream pread;
		string long_string;
		string short_str;
		const double uptime = system_uptime();
		vector<proc_info> procs;
		procs.reserve(current_procs.size() + 10);
		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;
		if (no_update and not cache.empty()) {
			procs = current_procs;
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, round(uptime), procs);
			goto proc_no_update;
		}

		//? Update uid_user map if /etc/passwd changed since last run
		if (not Shared::passwd_path.empty() and fs::last_write_time(Shared::passwd_path) != Shared::passwd_time) {
			string r_uid, r_user;
			Shared::passwd_time = fs::last_write_time(Shared::passwd_path);
			uid_user.clear();
			pread.open(Shared::passwd_path);
			if (pread.good()) {
				while (not pread.eof()) {
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
		cputimes = 0;
		pread.open(Shared::procPath / "stat");
		if (pread.good()) {
			pread.ignore(SSmax, ' ');
			for (uint64_t times; pread >> times; cputimes += times);
			pread.close();
		}
		else throw std::runtime_error("Failure to read /proc/stat");

		//* Iterate over all pids in /proc
		for (const auto& d: fs::directory_iterator(Shared::procPath)) {
			if (Runner::stopping)
				return current_procs;
			if (pread.is_open()) pread.close();

			const string pid_str = d.path().filename();
			if (not isdigit(pid_str[0])) continue;

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
				while (not pread.eof()) {
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
							if (cache[new_proc.pid].cpu_s == 0) {
								next_x = 22;
								cache[new_proc.pid].cpu_t = cpu_t;
							}
							else
								next_x = 24;
							continue;
						}
						case 22: { //? Save cpu seconds to cache if missing
							cache[new_proc.pid].cpu_s = stoull(short_str);
							next_x = 24;
							continue;
						}
						case 24: { //? RSS memory (can be inaccurate, but parsing smaps increases total cpu usage by ~20x)
							new_proc.mem = stoull(short_str) * Shared::pageSize;
							next_x = 39;
							continue;
						}
						case 39: { //? CPU number last executed on
							new_proc.cpu_n = stoull(short_str);
							goto stat_loop_done;
						}
					}
				}

			}
			catch (const std::invalid_argument&) { continue; }
			catch (const std::out_of_range&) { continue; }

			stat_loop_done:
			pread.close();

			if (x-offset < 24) continue;

			//? Process cpu usage since last update
			new_proc.cpu_p = round(cmult * 1000 * (cpu_t - cache[new_proc.pid].cpu_t) / (cputimes - old_cputimes)) / 10.0;

			//? Process cumulative cpu usage since process start
			new_proc.cpu_c = (double)cpu_t / ((uptime * Shared::clkTck) - cache[new_proc.pid].cpu_s);

			//? Update cache with latest cpu times
			cache[new_proc.pid].cpu_t = cpu_t;

			if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
				got_detailed = true;
			}

			//? Push process to vector
			procs.push_back(new_proc);

		}

		//* Clear dead processes from cache at a regular interval
		if (++counter >= 10000 or (cache.size() > procs.size() + 100)) {
			counter = 0;
			unordered_flat_map<size_t, p_cache> r_cache;
			r_cache.reserve(procs.size());
			rng::for_each(procs, [&r_cache](const auto &p) {
				if (cache.contains(p.pid))
					r_cache[p.pid] = cache.at(p.pid);
			});
			cache = std::move(r_cache);
		}

		//* Update the details info box for process if active
		if (show_detailed and got_detailed) {
			_collect_details(detailed_pid, round(uptime), procs);
		}
		else if (show_detailed and not got_detailed and detailed.status != "Dead") {
			detailed.status = "Dead";
			redraw = true;
		}

		old_cputimes = cputimes;
		current_procs = procs;

		proc_no_update:

		//* Match filter if defined
		if (not tree and not filter.empty()) {
			const auto filtered = rng::remove_if(procs, [&filter](const auto& p) {
					return (not s_contains(to_string(p.pid), filter)
						and not s_contains(p.name, filter)
						and not s_contains(p.cmd, filter)
						and not s_contains(p.user, filter));
			});
			procs.erase(filtered.begin(), filtered.end());
		}

		//* Sort processes
		const auto cmp = [&reverse](const auto &a, const auto &b) { return (reverse ? a < b : a > b); };
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
			for (size_t i = 0, x = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 and procs[i].cpu_p > max)
					max = procs[i].cpu_p;
				else if (i == 6)
					target = (max > 30.0) ? max : 10.0;
				if (i == offset and procs[i].cpu_p > 30.0)
					offset++;
				else if (procs[i].cpu_p > target) {
					rotate(procs.begin() + offset, procs.begin() + i, procs.begin() + i + 1);
					if (++x > 10) break;
				}
			}
		}

		//* Generate tree view if enabled
		if (tree) {
			if (collapse > -1 and collapse == expand) {
				if (cache.contains(collapse)) cache.at(collapse).collapsed = not cache.at(collapse).collapsed;
				collapse = expand = -1;
			}
			else if (collapse > -1) {
				if (cache.contains(collapse)) cache.at(collapse).collapsed = true;
				collapse = -1;
			}
			else if (expand > -1) {
				if (cache.contains(expand)) cache.at(expand).collapsed = false;
				expand = -1;
			}

			vector<proc_info> tree_procs;
			tree_procs.reserve(procs.size());

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(procs, rng::less{}, &proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (const auto& p : rng::equal_range(procs, procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, procs, tree_procs, 0, false, filter);
			}

			procs = std::move(tree_procs);
		}

		numpids = (int)procs.size();

		return procs;
	}
}

#endif