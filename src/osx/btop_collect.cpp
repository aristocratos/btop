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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libproc.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netdb.h>
#include <netinet/tcp_fsm.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <btop_config.hpp>
#include <btop_shared.hpp>
#include <btop_tools.hpp>
#include <cmath>
#include <fstream>
#include <numeric>
#include <ranges>
#include <regex>
#include <string>

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
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

	struct Sensor {
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
}  // namespace Cpu

namespace Mem {
	double old_uptime;
}

namespace Shared {

	fs::path passwd_path;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount;
	int totalMem_len;

	void init() {
		//? Shared global variables init

		// passwd_path = (fs::is_regular_file(fs::path("/etc/passwd")) and access("/etc/passwd", R_OK) != -1) ? "/etc/passwd" : "";
		// if (passwd_path.empty())
		// 	Logger::warning("Could not read /etc/passwd, will show UID instead of username.");

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

		int64_t memsize = 0;
		size_t size = sizeof(memsize);
		if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) < 0) {
			Logger::warning("Could not get memory size");
		}
		totalMem = memsize;

		//? Init for namespace Cpu
		if (not fs::exists(Cpu::freq_path) or access(Cpu::freq_path.c_str(), R_OK) == -1) Cpu::freq_path.clear();
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Cpu::collect();
		for (auto &[field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty()) Cpu::available_fields.push_back(field);
		}
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();
		for (const auto &[sensor, ignored] : Cpu::found_sensors) {
			Cpu::available_sensors.push_back(sensor);
		}
		Cpu::core_mapping = Cpu::get_core_mapping();

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Mem::collect();
	}

}  // namespace Shared

namespace Cpu {
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

	string get_cpuName() {
		string name;
		char buffer[1024];
		size_t size = sizeof(buffer);
		if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, NULL, 0) < 0) {
			Logger::error("Failed to get CPU name");
			return name;
		}
		name = string(buffer);

		auto name_vec = ssplit(name);

		if ((s_contains(name, "Xeon"s) or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else if (v_contains(name_vec, "Ryzen"s)) {
			auto ryz_pos = v_index(name_vec, "Ryzen"s);
			name = "Ryzen" + (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "") + (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
		} else if (s_contains(name, "Intel"s) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1) != "@")
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else
			name.clear();

		if (name.empty() and not name_vec.empty()) {
			for (const auto &n : name_vec) {
				if (n == "@") break;
				name += n + ' ';
			}
			name.pop_back();
			for (const auto &reg : {regex("Processor"), regex("CPU"), regex("\\(R\\)"), regex("\\(TM\\)"), regex("Intel"),
			                        regex("AMD"), regex("Core"), regex("\\d?\\.?\\d+[mMgG][hH][zZ]")}) {
				name = std::regex_replace(name, reg, "");
			}
			name = trim(name);
		}

		return name;
	}

	bool get_sensors() {
		return not found_sensors.empty();
	}

	void update_sensors() {
		if (cpu_sensor.empty())
			return;

		const auto &cpu_sensor = (not Config::getS("cpu_sensor").empty() and found_sensors.contains(Config::getS("cpu_sensor")) ? Config::getS("cpu_sensor") : Cpu::cpu_sensor);

		found_sensors.at(cpu_sensor).temp = stol(readfile(found_sensors.at(cpu_sensor).path, "0")) / 1000;
		current_cpu.temp.at(0).push_back(found_sensors.at(cpu_sensor).temp);
		current_cpu.temp_max = found_sensors.at(cpu_sensor).crit;
		if (current_cpu.temp.at(0).size() > 20)
			current_cpu.temp.at(0).pop_front();

		if (Config::getB("show_coretemp") and not cpu_temp_only) {
			vector<string> done;
			for (const auto &sensor : core_sensors) {
				if (v_contains(done, sensor))
					continue;
				found_sensors.at(sensor).temp = stol(readfile(found_sensors.at(sensor).path, "0")) / 1000;
				done.push_back(sensor);
			}
			for (const auto &[core, temp] : core_mapping) {
				if (cmp_less(core + 1, current_cpu.temp.size()) and cmp_less(temp, core_sensors.size())) {
					current_cpu.temp.at(core + 1).push_back(found_sensors.at(core_sensors.at(temp)).temp);
					if (current_cpu.temp.at(core + 1).size() > 20)
						current_cpu.temp.at(core + 1).pop_front();
				}
			}
		}
	}

	string get_cpuHz() {
		unsigned int freq = 1;
		size_t size = sizeof(freq);

		int mib[2];
		mib[0] = CTL_HW;
		mib[1] = HW_CPU_FREQ;

		if (sysctl(mib, 2, &freq, &size, NULL, 0) < 0) {
			Logger::error("Failed to get CPU frequency: " + std::to_string(errno));
		}
		return std::to_string(freq / 1000.0 / 1000.0 / 1000.0).substr(0, 3);
	}

	auto get_core_mapping() -> unordered_flat_map<int, int> {
		unordered_flat_map<int, int> core_map;
		if (cpu_temp_only) return core_map;

		natural_t cpu_count;
		natural_t i;
		processor_info_array_t info_array;
		mach_msg_type_number_t info_count;
		kern_return_t error;

		mach_port_t host_port = mach_host_self();
		error = host_processor_info(host_port, PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info_array, &info_count);
		if (error != KERN_SUCCESS) {
			Logger::error("Failed getting CPU info");
			return core_map;
		}
		for (i = 0; i < cpu_count; i++) {
			core_map[i] = i;
		}

		//? If core mapping from cpuinfo was incomplete try to guess remainder, if missing completely, map 0-0 1-1 2-2 etc.
		if (cmp_less(core_map.size(), Shared::coreCount)) {
			if (Shared::coreCount % 2 == 0 and (long) core_map.size() == Shared::coreCount / 2) {
				for (int i = 0, n = 0; i < Shared::coreCount / 2; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[Shared::coreCount / 2 + i] = n++;
				}
			} else {
				core_map.clear();
				for (int i = 0, n = 0; i < Shared::coreCount; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[i] = n++;
				}
			}
		}

		//? Apply user set custom mapping if any
		const auto &custom_map = Config::getS("cpu_core_map");
		if (not custom_map.empty()) {
			try {
				for (const auto &split : ssplit(custom_map)) {
					const auto vals = ssplit(split, ':');
					if (vals.size() != 2) continue;
					int change_id = std::stoi(vals.at(0));
					int new_id = std::stoi(vals.at(1));
					if (not core_map.contains(change_id) or cmp_greater(new_id, core_sensors.size())) continue;
					core_map.at(change_id) = new_id;
				}
			} catch (...) {
			}
		}

		return core_map;
	}

	auto get_battery() -> tuple<int, long, string> {
		if (not has_battery) return {0, 0, ""};

		uint32_t percent = -1;
		long seconds = -1;
		string status = "discharging";
		CFTypeRef ps_info = IOPSCopyPowerSourcesInfo();
		if (ps_info) {
			CFArrayRef one_ps_descriptor = IOPSCopyPowerSourcesList(ps_info);
			if (one_ps_descriptor) {
				if (CFArrayGetCount(one_ps_descriptor)) {
					CFDictionaryRef one_ps = IOPSGetPowerSourceDescription(ps_info, CFArrayGetValueAtIndex(one_ps_descriptor, 0));
					has_battery = true;
					CFNumberRef remaining = (CFNumberRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSTimeToEmptyKey));
					int32_t estimatedMinutesRemaining;
					if (remaining) {
						CFNumberGetValue(remaining, kCFNumberSInt32Type, &estimatedMinutesRemaining);
						seconds = estimatedMinutesRemaining * 60;
					}
					CFNumberRef charge = (CFNumberRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSCurrentCapacityKey));
					if (charge) {
						CFNumberGetValue(charge, kCFNumberSInt32Type, &percent);
					}
					CFBooleanRef charging = (CFBooleanRef)CFDictionaryGetValue(one_ps, CFSTR(kIOPSIsChargingKey));
					if (charging) {
						bool isCharging = CFBooleanGetValue(charging);
						if (isCharging) {
							status = "charging";
							if (percent == 100) {
								status = "full";
							}
						}
					}
				} else {
					has_battery = false;
				}
				CFRelease(one_ps_descriptor);
			} else {
				has_battery = false;
			}
			CFRelease(ps_info);
		}
		return {percent, seconds, status};
	}

	auto collect(const bool no_update) -> cpu_info & {
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty()))
			return current_cpu;
		auto &cpu = current_cpu;

		double avg[3];

		if (getloadavg(avg, sizeof(avg)) < 0) {
			Logger::error("failed to get load averages");
		}

		cpu.load_avg[0] = avg[0];
		cpu.load_avg[1] = avg[1];
		cpu.load_avg[2] = avg[2];
		natural_t cpu_count;
		natural_t i;
		processor_info_array_t info_array;
		mach_msg_type_number_t info_count;
		kern_return_t error;
		processor_cpu_load_info_data_t *cpu_load_info = NULL;

		mach_port_t host_port = mach_host_self();
		error = host_processor_info(host_port, PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info_array, &info_count);
		if (error != KERN_SUCCESS) {
			Logger::error("Failed getting CPU load info");
		}
		cpu_load_info = (processor_cpu_load_info_data_t *)info_array;
		long long global_totals = 0;
		long long global_idles = 0;
		for (i = 0; i < cpu_count; i++) {
			vector<long long> times;
			long long total_sum = 0;
			//? 0=user, 1=nice, 2=system, 3=idle, 4=iowait, 5=irq, 6=softirq, 7=steal, 8=guest, 9=guest_nice
			times.push_back(cpu_load_info[i].cpu_ticks[CPU_STATE_USER]);
			times.push_back(cpu_load_info[i].cpu_ticks[CPU_STATE_NICE]);
			times.push_back(cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM]);
			times.push_back(cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE]);
			times.push_back(0);
			times.push_back(0);
			times.push_back(0);
			times.push_back(0);
			times.push_back(0);
			times.push_back(0);
			for (long long t : times) {
				total_sum += t;
			}
			try {
				//? Subtract fields 8-9 and any future unknown fields
				const long long totals = max(0ll, total_sum - (times.size() > 8 ? std::accumulate(times.begin() + 8, times.end(), 0) : 0));

				//? Add iowait field if present
				const long long idles = max(0ll, times.at(3) + (times.size() > 4 ? times.at(4) : 0));

				global_totals += totals;
				global_idles += idles;
				//? Calculate cpu total for each core
				if (i > Shared::coreCount) break;
				const long long calc_totals = max(0ll, totals - core_old_totals.at(i));
				const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
				core_old_totals.at(i) = totals;
				core_old_idles.at(i) = idles;

				cpu.core_percent.at(i).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

				//? Reduce size if there are more values than needed for graph
				if (cpu.core_percent.at(i).size() > 40) cpu.core_percent.at(i).pop_front();

				//? Populate cpu.cpu_percent with all fields from syscall
				for (int ii = 0; const auto &val : times) {
					cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
					cpu_old.at(time_names.at(ii)) = val;
				}
			} catch (const std::exception &e) {
				Logger::error("get_cpuHz() : " + (string)e.what());
				throw std::runtime_error("collect() : " + (string)e.what());
			}
		}
		const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
		const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));
		cpu_old.at("totals") = global_totals;
		cpu_old.at("idles") = global_idles;

		//? Total usage of cpu
		cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

		//? Reduce size if there are more values than needed for graph
		while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

		if (Config::getB("show_cpu_freq"))
			cpuHz = get_cpuHz();

		if (Config::getB("check_temp") and got_sensors)
			update_sensors();

		if (Config::getB("show_battery") and has_battery)
			current_bat = get_battery();

		return cpu;
	}
}  // namespace Cpu

namespace Mem {
	bool has_swap = false;
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios = 0;
	vector<string> last_found;

	mem_info current_mem{};

	uint64_t get_totalMem() {
		return Shared::totalMem;
	}

	auto collect(const bool no_update) -> mem_info & {
		if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
			return current_mem;

		auto &show_swap = Config::getB("show_swap");
		auto &show_disks = Config::getB("show_disks");
		auto &swap_disk = Config::getB("swap_disk");
		auto &mem = current_mem;
		static const bool snapped = (getenv("BTOP_SNAPPED") != NULL);

		vm_statistics64 p;
		mach_msg_type_number_t info_size = HOST_VM_INFO64_COUNT;
		if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&p, &info_size) == 0) {
			mem.stats.at("available") = p.free_count * Shared::pageSize;
			mem.stats.at("free") = p.free_count * Shared::pageSize;
			mem.stats.at("cached") = p.external_page_count * Shared::pageSize;
			mem.stats.at("used") = (p.active_count + p.inactive_count + p.wire_count) * Shared::pageSize;
		}

		int mib[2] = {CTL_VM, VM_SWAPUSAGE};

		struct xsw_usage swap;
		size_t len = sizeof(struct xsw_usage);
		if (sysctl(mib, 2, &swap, &len, NULL, 0) == 0) {
			mem.stats.at("swap_total") = swap.xsu_total;
			mem.stats.at("swap_free") = swap.xsu_avail;
			mem.stats.at("swap_used") = swap.xsu_used;
		}

		if (show_swap and mem.stats.at("swap_total") > 0) {
			for (const auto &name : swap_names) {
				mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
				while (cmp_greater(mem.percent.at(name).size(), width * 2))
					mem.percent.at(name).pop_front();
			}
			has_swap = true;
		} else
			has_swap = false;
		//? Calculate percentages
		for (const auto &name : mem_names) {
			mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / Shared::totalMem));
			while (cmp_greater(mem.percent.at(name).size(), width * 2))
				mem.percent.at(name).pop_front();
		}

		if (show_disks) {
			double uptime = system_uptime();
			auto &disks_filter = Config::getS("disks_filter");
			bool filter_exclude = false;
			// auto &only_physical = Config::getB("only_physical");
			auto &disks = mem.disks;
			vector<string> filter;
			if (not disks_filter.empty()) {
				filter = ssplit(disks_filter);
				if (filter.at(0).starts_with("exclude=")) {
					filter_exclude = true;
					filter.at(0) = filter.at(0).substr(8);
				}
			}

			struct statfs *stfs;
			int count = getmntinfo(&stfs, MNT_WAIT);
			vector<string> found;
			found.reserve(last_found.size());
			for (int i = 0; i < count; i++) {
				std::error_code ec;
				string mountpoint = stfs[i].f_mntonname;
				string dev = stfs[i].f_mntfromname;
				disks[mountpoint] = disk_info{fs::canonical(dev, ec), fs::path(mountpoint).filename()};

				//? Match filter if not empty
				if (not filter.empty()) {
					bool match = v_contains(filter, mountpoint);
					if ((filter_exclude and match) or (not filter_exclude and not match))
						continue;
				}

				found.push_back(mountpoint);
				if (not v_contains(last_found, mountpoint))
					redraw = true;

				if (disks.at(mountpoint).dev.empty())
					disks.at(mountpoint).dev = dev;
				if (disks.at(mountpoint).name.empty())
					disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
				disks.at(mountpoint).free = stfs[i].f_bfree;
				disks.at(mountpoint).total = stfs[i].f_iosize;
			}

			//? Remove disks no longer mounted or filtered out
			if (swap_disk and has_swap) found.push_back("swap");
			for (auto it = disks.begin(); it != disks.end();) {
				if (not v_contains(found, it->first))
					it = disks.erase(it);
				else
					it++;
			}
			if (found.size() != last_found.size()) redraw = true;
			last_found = std::move(found);

			//? Get disk/partition stats
			for (auto &[mountpoint, disk] : disks) {
				if (std::error_code ec; not fs::exists(mountpoint, ec))
					continue;
				struct statvfs vfs;
				if (statvfs(mountpoint.c_str(), &vfs) < 0) {
					Logger::warning("Failed to get disk/partition stats with statvfs() for: " + mountpoint);
					continue;
				}
				disk.total = vfs.f_blocks * vfs.f_frsize;
				disk.free = vfs.f_bfree * vfs.f_frsize;
				disk.used = disk.total - disk.free;
				disk.used_percent = round((double)disk.used * 100 / disk.total);
				disk.free_percent = 100 - disk.used_percent;
			}

			//? Setup disks order in UI and add swap if enabled
			mem.disks_order.clear();
			if (snapped and disks.contains("/mnt"))
				mem.disks_order.push_back("/mnt");
			else if (disks.contains("/"))
				mem.disks_order.push_back("/");
			if (swap_disk and has_swap) {
				mem.disks_order.push_back("swap");
				if (not disks.contains("swap"))
					disks["swap"] = {"", "swap"};
				disks.at("swap").total = mem.stats.at("swap_total");
				disks.at("swap").used = mem.stats.at("swap_used");
				disks.at("swap").free = mem.stats.at("swap_free");
				disks.at("swap").used_percent = mem.percent.at("swap_used").back();
				disks.at("swap").free_percent = mem.percent.at("swap_free").back();
			}
			for (const auto &name : last_found)
				if (not is_in(name, "/", "swap"))
					mem.disks_order.push_back(name);

			old_uptime = uptime;
		}
		return mem;
	}

}  // namespace Mem

namespace Net {
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
	class getifaddr_wrapper {
		struct ifaddrs *ifaddr;

	   public:
		int status;
		getifaddr_wrapper() { status = getifaddrs(&ifaddr); }
		~getifaddr_wrapper() { freeifaddrs(ifaddr); }
		auto operator()() -> struct ifaddrs * { return ifaddr; }
	};

	auto collect(const bool no_update) -> net_info & {
		auto &net = current_net;
		auto &config_iface = Config::getS("net_iface");
		auto &net_sync = Config::getB("net_sync");
		auto &net_auto = Config::getB("net_auto");
		auto new_timestamp = time_ms();

		if (not no_update and errors < 3) {
			//? Get interface list using getifaddrs() wrapper
			getifaddr_wrapper if_wrap{};
			if (if_wrap.status != 0) {
				errors++;
				Logger::error("Net::collect() -> getifaddrs() failed with id " + to_string(if_wrap.status));
				redraw = true;
				return empty_net;
			}
			int family = 0;
			char ip[NI_MAXHOST];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all items in getifaddrs() list
			for (auto *ifa = if_wrap(); ifa != NULL; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == NULL) continue;
				family = ifa->ifa_addr->sa_family;
				const auto &iface = ifa->ifa_name;
				//? Get IPv4 address
				if (family == AF_INET) {
					if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
						net[iface].ipv4 = ip;
				}
				//? Get IPv6 address
				else if (family == AF_INET6) {
					if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
						net[iface].ipv6 = ip;
				}

				//? Update available interfaces vector and get status of interface
				if (not v_contains(interfaces, iface)) {
					interfaces.push_back(iface);
					net[iface].connected = (ifa->ifa_flags & IFF_RUNNING);
				}
			}

			unordered_flat_map<string, std::tuple<uint64_t, uint64_t>> ifstats;
			int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
			size_t len;
			if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
				Logger::error("failed getting network interfaces");
			}
			char *buf = (char *)malloc(len);
			if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
				Logger::error("failed getting network interfaces");
			}
			char *lim = buf + len;
			char *next = NULL;
			for (next = buf; next < lim;) {
				struct if_msghdr *ifm = (struct if_msghdr *)next;
				next += ifm->ifm_msglen;
				if (ifm->ifm_type == RTM_IFINFO2) {
					struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
					struct sockaddr_dl *sdl = (struct sockaddr_dl *)(if2m + 1);
					char iface[32];
					strncpy(iface, sdl->sdl_data, sdl->sdl_nlen);
					iface[sdl->sdl_nlen] = 0;
					ifstats[iface] = std::tuple(if2m->ifm_data.ifi_ibytes, if2m->ifm_data.ifi_obytes);
				}
			}

			//? Get total recieved and transmitted bytes + device address if no ip was found
			for (const auto &iface : interfaces) {
				for (const string dir : {"download", "upload"}) {
					auto &saved_stat = net.at(iface).stat.at(dir);
					auto &bandwidth = net.at(iface).bandwidth.at(dir);
					auto dirval = dir == "download" ? std::get<0>(ifstats[iface]) : std::get<1>(ifstats[iface]);
					uint64_t val = saved_stat.last;
					try {
						val = max(dirval, val);
					} catch (const std::invalid_argument &) {
					} catch (const std::out_of_range &) {
					}

					//? Update speed, total and top values
					saved_stat.speed = round((double)(val - saved_stat.last) / ((double)(new_timestamp - timestamp) / 1000));
					if (saved_stat.speed > saved_stat.top) saved_stat.top = saved_stat.speed;
					if (saved_stat.offset > val) saved_stat.offset = 0;
					saved_stat.total = val - saved_stat.offset;
					saved_stat.last = val;

					//? Add values to graph
					bandwidth.push_back(saved_stat.speed);
					while (cmp_greater(bandwidth.size(), width * 2)) bandwidth.pop_front();

					//? Set counters for auto scaling
					if (net_auto and selected_iface == iface) {
						if (saved_stat.speed > graph_max[dir]) {
							++max_count[dir][0];
							if (max_count[dir][1] > 0) --max_count[dir][1];
						} else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
							++max_count[dir][1];
							if (max_count[dir][0] > 0) --max_count[dir][0];
						}
					}
				}
			}

			//? Clean up net map if needed
			if (net.size() > interfaces.size()) {
				for (auto it = net.begin(); it != net.end();) {
					if (not v_contains(interfaces, it->first))
						it = net.erase(it);
					else
						it++;
				}
				net.compact();
			}

			timestamp = new_timestamp;
		}
		//? Return empty net_info struct if no interfaces was found
		if (net.empty())
			return empty_net;

		//? Find an interface to display if selected isn't set or valid
		if (selected_iface.empty() or not v_contains(interfaces, selected_iface)) {
			max_count["download"][0] = max_count["download"][1] = max_count["upload"][0] = max_count["upload"][1] = 0;
			redraw = true;
			if (net_auto) rescale = true;
			if (not config_iface.empty() and v_contains(interfaces, config_iface))
				selected_iface = config_iface;
			else {
				//? Sort interfaces by total upload + download bytes
				auto sorted_interfaces = interfaces;
				rng::sort(sorted_interfaces, [&](const auto &a, const auto &b) {
					return cmp_greater(net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
					                   net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
				});
				selected_iface.clear();
				//? Try to set to a connected interface
				for (const auto &iface : sorted_interfaces) {
					if (net.at(iface).connected) selected_iface = iface;
					break;
				}
				//? If no interface is connected set to first available
				if (selected_iface.empty() and not sorted_interfaces.empty())
					selected_iface = sorted_interfaces.at(0);
				else if (sorted_interfaces.empty())
					return empty_net;
			}
		}

		//? Calculate max scale for graphs if needed
		if (net_auto) {
			bool sync = false;
			for (const auto &dir : {"download", "upload"}) {
				for (const auto &sel : {0, 1}) {
					if (rescale or max_count[dir][sel] >= 5) {
						const uint64_t avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
						                                ? std::accumulate(net.at(selected_iface).bandwidth.at(dir).rbegin(), net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0) / 5
						                                : net[selected_iface].stat[dir].speed);
						graph_max[dir] = max(uint64_t(avg_speed * (sel == 0 ? 1.3 : 3.0)), (uint64_t)10 << 10);
						max_count[dir][0] = max_count[dir][1] = 0;
						redraw = true;
						if (net_sync) sync = true;
						break;
					}
				}
				//? Sync download/upload graphs if enabled
				if (sync) {
					const auto other = (string(dir) == "upload" ? "download" : "upload");
					graph_max[other] = graph_max[dir];
					max_count[other][0] = max_count[other][1] = 0;
					break;
				}
			}
		}

		rescale = false;
		return net.at(selected_iface);
	}
}  // namespace Net

namespace Proc {

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
	void _tree_gen(proc_info &cur_proc, vector<proc_info> &in_procs, vector<std::reference_wrapper<proc_info>> &out_procs, int cur_depth, const bool collapsed, const string &filter, bool found = false, const bool no_update = false, const bool should_filter = false) {
		auto cur_pos = out_procs.size();
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not found and (should_filter or not filter.empty())) {
			if (not s_contains(std::to_string(cur_proc.pid), filter) and not s_contains(cur_proc.name, filter) and not s_contains(cur_proc.cmd, filter) and not s_contains(cur_proc.user, filter)) {
				filtering = true;
				cur_proc.filtered = true;
				filter_found++;
			} else {
				found = true;
				cur_depth = 0;
			}
		} else if (cur_proc.filtered)
			cur_proc.filtered = false;

		//? Set tree index position for process if not filtered out or currently in a collapsed sub-tree
		if (not collapsed and not filtering) {
			out_procs.push_back(std::ref(cur_proc));
			cur_proc.tree_index = out_procs.size() - 1;
			//? Try to find name of the binary file and append to program name if not the same
			if (cur_proc.short_cmd.empty() and not cur_proc.cmd.empty()) {
				std::string_view cmd_view = cur_proc.cmd;
				cmd_view = cmd_view.substr((size_t)0, min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				cur_proc.short_cmd = (string)cmd_view;
			}
		} else {
			cur_proc.tree_index = in_procs.size();
		}

		//? Recursive iteration over all children
		int children = 0;
		for (auto &p : rng::equal_range(in_procs, cur_proc.pid, rng::less{}, &proc_info::ppid)) {
			if (not no_update and not filtering and (collapsed or cur_proc.collapsed)) {
				out_procs.back().get().cpu_p += p.cpu_p;
				out_procs.back().get().mem += p.mem;
				out_procs.back().get().threads += p.threads;
				filter_found++;
			}
			if (collapsed and not filtering) {
				cur_proc.filtered = true;
			} else
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
	void _collect_details(const size_t pid, const uint64_t uptime, vector<proc_info> &procs) {
		if (pid != detailed.last_pid) {
			detailed = {};
			detailed.last_pid = pid;
			detailed.skip_smaps = not Config::getB("proc_info_smaps");
		}

		//? Copy proc_info for process from proc vector
		auto p_info = rng::find(procs, pid, &proc_info::pid);
		detailed.entry = *p_info;

		//? Update cpu percent deque for process cpu graph
		if (not Config::getB("proc_per_core")) detailed.entry.cpu_p *= Shared::coreCount;
		detailed.cpu_percent.push_back(clamp((long long)round(detailed.entry.cpu_p), 0ll, 100ll));
		while (cmp_greater(detailed.cpu_percent.size(), width)) detailed.cpu_percent.pop_front();

		//? Process runtime
		detailed.elapsed = sec_to_dhms(uptime - (detailed.entry.cpu_s / Shared::clkTck));
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

		//? Get parent process name
		if (detailed.parent.empty()) {
			auto p_entry = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
			if (p_entry != procs.end()) detailed.parent = p_entry->name;
		}

		//? Expand process status from single char to explanative string
		detailed.status = (proc_states.contains(detailed.entry.state)) ? proc_states.at(detailed.entry.state) : "Unknown";
	}

	//* Collects and sorts process information from /proc
	auto collect(const bool no_update) -> vector<proc_info> & {
		const auto &sorting = Config::getS("proc_sorting");
		const auto &reverse = Config::getB("proc_reversed");
		const auto &filter = Config::getS("proc_filter");
		const auto &per_core = Config::getB("proc_per_core");
		const auto &tree = Config::getB("proc_tree");
		const auto &show_detailed = Config::getB("show_detailed");
		const size_t detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		const bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}

		const double uptime = system_uptime();

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, round(uptime), current_procs);
		} else {
			int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
			struct kinfo_proc *processes = NULL;
			vector<size_t> found;
			for (int retry = 3; retry > 0; retry--) {
				size_t size = 0;
				if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0 || size == 0) {
					Logger::error("Unable to get size of kproc_infos");
				}
				uint64_t cpu_t = 0;
				processes = (struct kinfo_proc *)malloc(size);
				if (sysctl(mib, 4, processes, &size, NULL, 0) == 0) {
					size_t count = size / sizeof(struct kinfo_proc);
					for (size_t i = 0; i < count; i++) {
						struct kinfo_proc kproc = processes[i];
						Proc::proc_info p{(size_t)kproc.kp_proc.p_pid};
						char fullname[PROC_PIDPATHINFO_MAXSIZE];
						const size_t pid = p.pid;
						proc_pidpath(pid, fullname, sizeof(fullname));
						p.cmd = std::string(fullname);
						size_t lastSlash = p.cmd.find_last_of('/');
						p.name = p.cmd.substr(lastSlash + 1);
						p.ppid = kproc.kp_eproc.e_ppid;
						p.p_nice = kproc.kp_proc.p_nice;
						struct proc_taskinfo pti;
						if (sizeof(pti) == proc_pidinfo(p.pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti))) {
							p.threads = pti.pti_threadnum;
							cpu_t = pti.pti_total_user + pti.pti_total_system;
							p.cpu_s = pti.pti_total_system;
							p.cpu_c = (double)p.cpu_t / max(1.0, (uptime * Shared::clkTck) - p.cpu_s);
							p.mem = pti.pti_resident_size;
						}
						struct passwd *pwd = getpwuid(kproc.kp_eproc.e_ucred.cr_uid);
						p.user = pwd->pw_name;
						found.push_back(pid);
						//? Check if pid already exists in current_procs
						auto find_old = rng::find(current_procs, pid, &proc_info::pid);
						if (find_old == current_procs.end()) {
							current_procs.push_back(p);
							find_old = current_procs.end() - 1;
						}
						//? Process cpu usage since last update
						p.cpu_p = clamp(round(cmult * 1000 * (cpu_t - p.cpu_t) / max((uint64_t)1, cputimes - old_cputimes)) / 10.0, 0.0, 100.0 * Shared::coreCount);

						//? Process cumulative cpu usage since process start
						p.cpu_c = (double)cpu_t / max(1.0, (uptime * Shared::clkTck) - p.cpu_s);

						//? Update cached value with latest cpu times
						p.cpu_t = cpu_t;
					}
				}
			}

			// //? Clear dead processes from current_procs
			// auto eraser = rng::remove_if(current_procs, [&](const auto& element){ return not v_contains(found, element.pid); });
			// current_procs.erase(eraser.begin(), eraser.end());

			//? Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, round(uptime), current_procs);
			} else if (show_detailed and not got_detailed and detailed.status != "Dead") {
				detailed.status = "Dead";
				redraw = true;
			}

			old_cputimes = cputimes;
		}

		//* ---------------------------------------------Collection done-----------------------------------------------

		//* Sort processes
		if (sorted_change or not no_update) {
			switch (v_index(sort_vector, sorting)) {
				case 0:
					rng::sort(current_procs, rng::greater{}, &proc_info::pid);
					break;
				case 1:
					rng::sort(current_procs, rng::greater{}, &proc_info::name);
					break;
				case 2:
					rng::sort(current_procs, rng::greater{}, &proc_info::cmd);
					break;
				case 3:
					rng::sort(current_procs, rng::greater{}, &proc_info::threads);
					break;
				case 4:
					rng::sort(current_procs, rng::greater{}, &proc_info::user);
					break;
				case 5:
					rng::sort(current_procs, rng::greater{}, &proc_info::mem);
					break;
				case 6:
					rng::sort(current_procs, rng::greater{}, &proc_info::cpu_p);
					break;
				case 7:
					rng::sort(current_procs, rng::greater{}, &proc_info::cpu_c);
					break;
			}
			if (reverse) rng::reverse(current_procs);

			//* When sorting with "cpu lazy" push processes over threshold cpu usage to the front regardless of cumulative usage
			if (not tree and not reverse and sorting == "cpu lazy") {
				double max = 10.0, target = 30.0;
				for (size_t i = 0, x = 0, offset = 0; i < current_procs.size(); i++) {
					if (i <= 5 and current_procs.at(i).cpu_p > max)
						max = current_procs.at(i).cpu_p;
					else if (i == 6)
						target = (max > 30.0) ? max : 10.0;
					if (i == offset and current_procs.at(i).cpu_p > 30.0)
						offset++;
					else if (current_procs.at(i).cpu_p > target) {
						rotate(current_procs.begin() + offset, current_procs.begin() + i, current_procs.begin() + i + 1);
						if (++x > 10) break;
					}
				}
			}
		}

		//* Match filter if defined
		if (should_filter) {
			filter_found = 0;
			for (auto &p : current_procs) {
				if (not tree and not filter.empty()) {
					if (not s_contains(to_string(p.pid), filter) and not s_contains(p.name, filter) and not s_contains(p.cmd, filter) and not s_contains(p.user, filter)) {
						p.filtered = true;
						filter_found++;
					} else {
						p.filtered = false;
					}
				} else {
					p.filtered = false;
				}
			}
		}

		//* Generate tree view if enabled
		if (tree and (not no_update or should_filter or sorted_change)) {
			if (auto find_pid = (collapse != -1 ? collapse : expand); find_pid != -1) {
				auto collapser = rng::find(current_procs, find_pid, &proc_info::pid);
				if (collapser != current_procs.end()) {
					if (collapse == expand) {
						collapser->collapsed = not collapser->collapsed;
					} else if (collapse > -1) {
						collapser->collapsed = true;
					} else if (expand > -1) {
						collapser->collapsed = false;
					}
				}
				collapse = expand = -1;
			}
			if (should_filter or not filter.empty()) filter_found = 0;

			vector<std::reference_wrapper<proc_info>> tree_procs;
			tree_procs.reserve(current_procs.size());

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, &proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto &p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			//? Final sort based on tree index
			rng::sort(current_procs, rng::less{}, &proc_info::tree_index);
			if (reverse) rng::reverse(current_procs);
		}

		numpids = (int)current_procs.size() - filter_found;
		return current_procs;
	}
}  // namespace Proc

namespace Tools {
	double system_uptime() {
		struct timeval ts, currTime;
		std::size_t len = sizeof(ts);
		int mib[2] = {CTL_KERN, KERN_BOOTTIME};
		if (sysctl(mib, 2, &ts, &len, NULL, 0) != -1) {
			gettimeofday(&currTime, NULL);
			return currTime.tv_sec - ts.tv_sec;
		}
		return 0.0;
	}
}  // namespace Tools