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

#include <fstream>
#include <ranges>
#include <cmath>
#include <unistd.h>
#include <numeric>
#include <regex>
#include <sys/statvfs.h>
#include <netdb.h>
#include <string>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <libproc.h>

#include <btop_shared.hpp>
#include <btop_config.hpp>
#include <btop_tools.hpp>

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu
{
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields;
	vector<string> available_sensors = {"Auto"};
	cpu_info current_cpu;
	fs::path freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";
	bool got_sensors = false, cpu_temp_only = false;

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search /proc/cpuinfo for a cpu name
	string get_cpuName();

	struct Sensor
	{
		fs::path path;
		string label;
		int64_t temp = 0;
		int64_t high = 0;
		int64_t crit = 0;
	};

	unordered_flat_map<string, Sensor> found_sensors;
	string cpu_sensor;
	vector<string> core_sensors;
	unordered_flat_map<int, int> core_mapping;
}

namespace Mem
{
	double old_uptime;
}

namespace Shared
{

	fs::path passwd_path;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount;
	int totalMem_len;

	void init()
	{

		//? Shared global variables init

		// passwd_path = (fs::is_regular_file(fs::path("/etc/passwd")) and access("/etc/passwd", R_OK) != -1) ? "/etc/passwd" : "";
		// if (passwd_path.empty())
		// 	Logger::warning("Could not read /etc/passwd, will show UID instead of username.");

		coreCount = sysconf(_SC_NPROCESSORS_ONLN);
		if (coreCount < 1)
		{
			coreCount = 1;
			Logger::warning("Could not determine number of cores, defaulting to 1.");
		}

		pageSize = sysconf(_SC_PAGE_SIZE);
		if (pageSize <= 0)
		{
			pageSize = 4096;
			Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
		}

		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0)
		{
			clkTck = 100;
			Logger::warning("Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect.");
		}

		int64_t memsize = 0;
		size_t size = sizeof(memsize);
		if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) < 0)
		{
			Logger::warning("Could not get memory size");
		}
		totalMem = memsize;
	}

}

namespace Cpu
{
	string cpuName;
	string cpuHz;
	bool has_battery = true;
	tuple<int, long, string> current_bat;

	const array<string, 10> time_names = {"user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"};

	unordered_flat_map<string, long long> cpu_old = {
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
		{"guest_nice", 0}};

	string get_cpuName()
	{
		string name("11th Gen Intel(R) Core(TM) i5-11600 @ 2.80GHz");

		return name;
	}

	bool get_sensors()
	{
		return not found_sensors.empty();
	}

	void update_sensors()
	{
		if (cpu_sensor.empty())
			return;

		const auto &cpu_sensor = (not Config::getS("cpu_sensor").empty() and found_sensors.contains(Config::getS("cpu_sensor")) ? Config::getS("cpu_sensor") : Cpu::cpu_sensor);

		found_sensors.at(cpu_sensor).temp = stol(readfile(found_sensors.at(cpu_sensor).path, "0")) / 1000;
		current_cpu.temp.at(0).push_back(found_sensors.at(cpu_sensor).temp);
		current_cpu.temp_max = found_sensors.at(cpu_sensor).crit;
		if (current_cpu.temp.at(0).size() > 20)
			current_cpu.temp.at(0).pop_front();

		if (Config::getB("show_coretemp") and not cpu_temp_only)
		{
			vector<string> done;
			for (const auto &sensor : core_sensors)
			{
				if (v_contains(done, sensor))
					continue;
				found_sensors.at(sensor).temp = stol(readfile(found_sensors.at(sensor).path, "0")) / 1000;
				done.push_back(sensor);
			}
			for (const auto &[core, temp] : core_mapping)
			{
				if (cmp_less(core + 1, current_cpu.temp.size()) and cmp_less(temp, core_sensors.size()))
				{
					current_cpu.temp.at(core + 1).push_back(found_sensors.at(core_sensors.at(temp)).temp);
					if (current_cpu.temp.at(core + 1).size() > 20)
						current_cpu.temp.at(core + 1).pop_front();
				}
			}
		}
	}

	string get_cpuHz()
	{
		uint64_t freq = 0;
		size_t size = sizeof(freq);

		return "1.0";
		// if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) < 0)
		// {
		// 	perror("sysctl");
		// }
		// Logger::debug("cpufreq:" + freq);
		// return "" + freq;
	}

	auto get_core_mapping() -> unordered_flat_map<int, int>
	{
		unordered_flat_map<int, int> core_map;
		return core_map;
	}

	auto get_battery() -> tuple<int, long, string>
	{
		// if (not has_battery)
		return {0, 0, ""};
	}

	auto collect(const bool no_update) -> cpu_info &
	{
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty()))
			return current_cpu;
		auto &cpu = current_cpu;

		if (Config::getB("show_cpu_freq"))
			cpuHz = get_cpuHz();

		return cpu;
	}
}

namespace Mem
{
	bool has_swap = false;
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios = 0;
	vector<string> last_found;

	mem_info current_mem{};

	auto collect(const bool no_update) -> mem_info &
	{
		if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
			return current_mem;

		auto &mem = current_mem;
		FILE *fpIn = popen("/usr/bin/vm_stat", "r");
		if (fpIn)
		{
			char buf[512];
			while (fgets(buf, sizeof(buf), fpIn) != NULL)
			{
				char *delim = ":\n.";
				char *tokens = strtok(buf, delim);
				while (tokens)
				{
					char *label = tokens;
					char *val = strtok(nullptr, delim);
					if (strstr(label, "Pages free"))
					{
						uint64_t f = stoull(trim(val));
						mem.stats.at("available") = f * 4096;
						mem.stats.at("free") = f * 4096;
						// } else if (strstr(label, "Pages free")) {
					}
					tokens = strtok(nullptr, delim);
				}
			}
			pclose(fpIn);
		}
		else
		{
			Logger::error("failed to read vm_stat");
		}
		mem.stats.at("used") = Shared::totalMem - mem.stats.at("available");
		return mem;
	}

}

namespace Net
{
	unordered_flat_map<string, net_info> current_net;
	net_info empty_net = {};
	vector<string> interfaces;
	string selected_iface;
	int errors = 0;
	unordered_flat_map<string, uint64_t> graph_max = {{"download", {}}, {"upload", {}}};
	unordered_flat_map<string, array<int, 2>> max_count = {{"download", {}}, {"upload", {}}};
	bool rescale = true;
	uint64_t timestamp = 0;

	//* RAII wrapper for getifaddrs
	class getifaddr_wrapper
	{
		struct ifaddrs *ifaddr;

	public:
		int status;
		getifaddr_wrapper() { status = getifaddrs(&ifaddr); }
		~getifaddr_wrapper() { freeifaddrs(ifaddr); }
		auto operator()() -> struct ifaddrs * { return ifaddr; }
	};

	auto collect(const bool no_update) -> net_info &
	{
		return empty_net;
	}
}

namespace Proc
{

	vector<proc_info> current_procs;
	unordered_flat_map<string, string> uid_user;
	string current_sort;
	string current_filter;
	bool current_rev = false;

	fs::file_time_type passwd_time;

	uint64_t cputimes;
	int collapse = -1, expand = -1;
	uint64_t old_cputimes = 0;
	atomic<int> numpids = 0;
	int filter_found = 0;

	detail_container detailed;

	//* Generate process tree list
	void _tree_gen(proc_info &cur_proc, vector<proc_info> &in_procs, vector<std::reference_wrapper<proc_info>> &out_procs, int cur_depth, const bool collapsed, const string &filter, bool found = false, const bool no_update = false, const bool should_filter = false)
	{
		auto cur_pos = out_procs.size();
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not found and (should_filter or not filter.empty()))
		{
			if (not s_contains(std::to_string(cur_proc.pid), filter) and not s_contains(cur_proc.name, filter) and not s_contains(cur_proc.cmd, filter) and not s_contains(cur_proc.user, filter))
			{
				filtering = true;
				cur_proc.filtered = true;
				filter_found++;
			}
			else
			{
				found = true;
				cur_depth = 0;
			}
		}
		else if (cur_proc.filtered)
			cur_proc.filtered = false;

		//? Set tree index position for process if not filtered out or currently in a collapsed sub-tree
		if (not collapsed and not filtering)
		{
			out_procs.push_back(std::ref(cur_proc));
			cur_proc.tree_index = out_procs.size() - 1;
			//? Try to find name of the binary file and append to program name if not the same
			if (cur_proc.short_cmd.empty() and not cur_proc.cmd.empty())
			{
				std::string_view cmd_view = cur_proc.cmd;
				cmd_view = cmd_view.substr((size_t)0, min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				cur_proc.short_cmd = (string)cmd_view;
			}
		}
		else
		{
			cur_proc.tree_index = in_procs.size();
		}

		//? Recursive iteration over all children
		int children = 0;
		for (auto &p : rng::equal_range(in_procs, cur_proc.pid, rng::less{}, &proc_info::ppid))
		{
			if (not no_update and not filtering and (collapsed or cur_proc.collapsed))
			{
				out_procs.back().get().cpu_p += p.cpu_p;
				out_procs.back().get().mem += p.mem;
				out_procs.back().get().threads += p.threads;
				filter_found++;
			}
			if (collapsed and not filtering)
			{
				cur_proc.filtered = true;
			}
			else
				children++;
			_tree_gen(p, in_procs, out_procs, cur_depth + 1, (collapsed ? true : cur_proc.collapsed), filter, found, no_update, should_filter);
		}
		if (collapsed or filtering)
			return;

		//? Add tree terminator symbol if it's the last child in a sub-tree
		if (out_procs.size() > cur_pos + 1 and not out_procs.back().get().prefix.ends_with("]─"))
			out_procs.back().get().prefix.replace(out_procs.back().get().prefix.size() - 8, 8, " └─ ");

		//? Add collapse/expand symbols if process have any children
		out_procs.at(cur_pos).get().prefix = " │ "s * cur_depth + (children > 0 ? (cur_proc.collapsed ? "[+]─" : "[-]─") : " ├─ ");
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, const uint64_t uptime, vector<proc_info> &procs)
	{
	}

	//* Collects and sorts process information from /proc
	auto collect(const bool no_update) -> vector<proc_info> &
	{
		int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
		struct kinfo_proc *processes = NULL;
		const double uptime = system_uptime();
		auto procs = &current_procs;

		for (int retry = 3; retry > 0; retry--)
		{
			size_t size = 0;
			if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0 || size == 0)
			{
				Logger::error("Unable to get size of kproc_infos");
			}

			processes = (struct kinfo_proc *)malloc(size);

			if (sysctl(mib, 4, processes, &size, NULL, 0) == 0)
			{
				size_t count = size / sizeof(struct kinfo_proc);
				for (size_t i = 0; i < count; i++)
				{
					struct kinfo_proc kproc = processes[i];
					Proc::proc_info p{kproc.kp_proc.p_pid};
					char fullname[PROC_PIDPATHINFO_MAXSIZE];
					proc_pidpath(p.pid, fullname, sizeof(fullname));
					p.cmd = std::string(fullname);
					size_t lastSlash = p.cmd.find_last_of('/');
					p.name = p.cmd.substr(lastSlash + 1);
					p.ppid = kproc.kp_eproc.e_ppid;
					p.p_nice = kproc.kp_proc.p_nice;
					struct proc_taskinfo pti;
					if (sizeof(pti) == proc_pidinfo(p.pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti)))
					{
						p.threads = pti.pti_threadnum;
						p.cpu_t = pti.pti_total_user + pti.pti_total_system;
						p.cpu_c = (double)p.cpu_t / max(1.0, (uptime * Shared::clkTck) - p.cpu_s);
						p.cpu_p = 0;
						p.cpu_s = pti.pti_total_system;
					}
					procs->push_back(p);
				}
			}
		}
		return current_procs;
	}
}

namespace Tools
{
	double system_uptime()
	{
		struct timeval ts;
		std::size_t len = sizeof(ts);
		int mib[2] = {CTL_KERN, KERN_BOOTTIME};
		if (sysctl(mib, 2, &ts, &len, NULL, 0) != -1)
		{
			return ts.tv_sec;
		}
		return 0.0;
	}
}