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

// #define _GNU_SOURCE
#include <fstream>
#include <ranges>
#include <cmath>
#include <unistd.h>
#include <numeric>
#include <tuple>
#include <regex>
#include <sys/statvfs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <linux/rtnetlink.h>

#include <btop_shared.hpp>
#include <btop_config.hpp>
#include <btop_tools.hpp>

using 	std::string, std::vector, std::ifstream, std::atomic, std::numeric_limits, std::streamsize, std::round, std::max, std::min,
		std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater, std::tuple, std::make_tuple;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields;
	cpu_info current_cpu;
	fs::path freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";
	bool got_sensors = false, cpu_temp_only = false;

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search /proc/cpuinfo for a cpu name
	string get_cpuName();

	//* Parse /proc/cpu info for mapping of core ids
	auto get_core_mapping() -> unordered_flat_map<int, int>;

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
}

namespace Shared {

	fs::path procPath, passwd_path;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount;

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
		if (not fs::exists(Cpu::freq_path) or access(Cpu::freq_path.c_str(), R_OK) == -1) Cpu::freq_path.clear();
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Cpu::collect();
		for (auto& [field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty()) Cpu::available_fields.push_back(field);
		}
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();
		Cpu::core_mapping = Cpu::get_core_mapping();

		//? Init for namespace Mem
		Mem::collect();

		//? Init for namespace Net
		Net::collect();

	}

}

namespace Cpu {
	string cpuName;
	string cpuHz;

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

			if ((s_contains(name, "Xeon"s) or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
				auto cpu_pos = v_index(name_vec, "CPU"s);
				if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
					name = name_vec.at(cpu_pos + 1);
				else
					name.clear();
			}
			else if (v_contains(name_vec, "Ryzen"s)) {
				auto ryz_pos = v_index(name_vec, "Ryzen"s);
				name = "Ryzen"	+ (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "")
								+ (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
			}
			else if (s_contains(name, "Intel"s) and v_contains(name_vec, "CPU"s)) {
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

	bool get_sensors() {
		bool got_cpu = false, got_coretemp = false;
		vector<fs::path> search_paths;
		try {
			//? Setup up paths to search for sensors
			if (fs::exists(fs::path("/sys/class/hwmon")) and access("/sys/class/hwmon", R_OK) != -1) {
				for (const auto& dir : fs::directory_iterator(fs::path("/sys/class/hwmon"))) {
					fs::path add_path = fs::canonical(dir.path());
					if (v_contains(search_paths, add_path) or v_contains(search_paths, add_path / "device")) continue;

					if (s_contains(add_path, "coretemp"))
						got_coretemp = true;

					if (fs::exists(add_path / "temp1_input")) {
						search_paths.push_back(add_path);
					}
					else if (fs::exists(add_path / "device/temp1_input"))
						search_paths.push_back(add_path / "device");
				}
			}
			if (not got_coretemp and fs::exists(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
				for (auto& d : fs::directory_iterator(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
					fs::path add_path = fs::canonical(d.path());

					if (fs::exists(d.path() / "temp1_input") and not v_contains(search_paths, add_path)) {
						search_paths.push_back(add_path);
						got_coretemp = true;
					}
				}
			}
			//? Scan any found directories for temperature sensors
			if (not search_paths.empty()) {
				for (const auto& path : search_paths) {
					const string pname = readfile(path / "name", path.filename());
					for (int i = 1; fs::exists(path / string("temp" + to_string(i) + "_input")); i++) {
						const string basepath = path / string("temp" + to_string(i) + "_");
						const string label = readfile(fs::path(basepath + "label"), "temp" + to_string(i));
						const string sensor_name = pname + "/" + label;
						const int64_t temp = stol(readfile(fs::path(basepath + "input"), "0")) / 1000;
						const int64_t high = stol(readfile(fs::path(basepath + "max"), "80000")) / 1000;
						const int64_t crit = stol(readfile(fs::path(basepath + "crit"), "95000")) / 1000;

						found_sensors[sensor_name] = {fs::path(basepath + "input"), label, temp, high, crit};

						if (not got_cpu and (label.starts_with("Package id") or label.starts_with("Tdie"))) {
							got_cpu = true;
							cpu_sensor = sensor_name;
						}
						else if (label.starts_with("Core") or label.starts_with("Tccd")) {
							got_coretemp = true;
							if (not v_contains(core_sensors, sensor_name)) core_sensors.push_back(sensor_name);
						}
					}
				}
			}
			//? If no good candidate for cpu temp has been found scan /sys/class/thermal
			if (not got_cpu and fs::exists(fs::path("/sys/class/thermal"))) {
				const string rootpath = fs::path("/sys/class/thermal/thermal_zone");
				for (int i = 0; fs::exists(fs::path(rootpath + to_string(i))); i++) {
					const fs::path basepath = rootpath + to_string(i);
					if (not fs::exists(basepath / "temp")) continue;
					const string label = readfile(basepath / "type", "temp" + to_string(i));
					const string sensor_name = "thermal" + to_string(i) + "/" + label;
					const int64_t temp = stol(readfile(basepath / "temp", "0")) / 1000;

					int64_t high, crit;
					for (int ii = 0; fs::exists(basepath / string("trip_point_" + to_string(ii) + "_temp")); ii++) {
						const string trip_type = readfile(basepath / string("trip_point_" + to_string(ii) + "_type"));
						if (not is_in(trip_type, "high", "critical")) continue;
						auto& val = (trip_type == "high" ? high : crit);
						val = stol(readfile(basepath / string("trip_point_" + to_string(ii) + "_temp"), "0")) / 1000;
					}
					if (high < 1) high = 80;
					if (crit < 1) crit = 95;

					found_sensors[sensor_name] = {basepath / "temp", label, temp, high, crit};
				}
			}

		}
		catch (...) {}

		if (not got_coretemp) cpu_temp_only = true;
		if (cpu_sensor.empty() and not found_sensors.empty()) {
			for (const auto& [name, sensor] : found_sensors) {
				if (s_contains(str_to_lower(name), "cpu")) {
					cpu_sensor = name;
					break;
				}
			}
			if (cpu_sensor.empty()) {
				cpu_sensor = found_sensors.begin()->first;
				Logger::warning("No good candidate for cpu sensor found, using random from all found sensors.");
			}
		}

		return not found_sensors.empty();
	}

	void update_sensors() {
		if (cpu_sensor.empty()) return;

		const auto& cpu_sensor = (not Config::getS("cpu_sensor").empty() and found_sensors.contains(Config::getS("cpu_sensor")) ? Config::getS("cpu_sensor") : Cpu::cpu_sensor);

		found_sensors.at(cpu_sensor).temp = stol(readfile(found_sensors.at(cpu_sensor).path, "0")) / 1000;
		current_cpu.temp.at(0).push_back(found_sensors.at(cpu_sensor).temp);
		current_cpu.temp_max = found_sensors.at(cpu_sensor).crit;
		if (current_cpu.temp.at(0).size() > 20) current_cpu.temp.at(0).pop_front();

		if (Config::getB("show_coretemp") and not cpu_temp_only) {
			vector<string> done;
			for (const auto& sensor : core_sensors) {
				if (v_contains(done, sensor)) continue;
				found_sensors.at(sensor).temp = stol(readfile(found_sensors.at(sensor).path, "0")) / 1000;
				done.push_back(sensor);
			}
			for (const auto& [core, temp] : core_mapping) {
				if (cmp_less(core + 1, current_cpu.temp.size()) and cmp_less(temp, core_sensors.size())) {
					current_cpu.temp.at(core + 1).push_back(found_sensors.at(core_sensors.at(temp)).temp);
					if (current_cpu.temp.at(core + 1).size() > 20) current_cpu.temp.at(core + 1).pop_front();
				}
			}
		}
	}

	string get_cpuHz() {
		static int failed = 0;
		if (failed > 4) return ""s;
		string cpuhz;
		try {
			double hz = 0.0;
			//? Try to get freq from /sys/devices/system/cpu/cpufreq/policy first (faster)
			if (not freq_path.empty()) {
				hz = stod(readfile(freq_path, "0.0")) / 1000;
				if (hz <= 0.0 and ++failed >= 2)
					freq_path.clear();
			}
			//? If freq from /sys failed or is missing try to use /proc/cpuinfo
			if (hz <= 0.0) {
				ifstream cpufreq(Shared::procPath / "cpuinfo");
				if (cpufreq.good()) {
					while (cpufreq.ignore(SSmax, '\n')) {
						if (cpufreq.peek() == 'c') {
							cpufreq.ignore(SSmax, ' ');
							if (cpufreq.peek() == 'M') {
								cpufreq.ignore(SSmax, ':');
								cpufreq.ignore(1);
								cpufreq >> hz;
								break;
							}
						}
					}
				}
			}

			if (hz <= 1 or hz >= 1000000) throw std::runtime_error("Failed to read /sys/devices/system/cpu/cpufreq/policy and /proc/cpuinfo.");

			if (hz >= 1000) {
				if (hz >= 10000) cpuhz = to_string((int)round(hz / 1000)); // Future proof until we reach THz speeds :)
				else cpuhz = to_string(round(hz / 100) / 10.0).substr(0, 3);
				cpuhz += " GHz";
			}
			else if (hz > 0)
				cpuhz = to_string((int)round(hz)) + " MHz";

		}
		catch (const std::exception& e) {
			if (++failed < 5) return ""s;
			else {
				Logger::warning("get_cpuHZ() : " + (string)e.what());
				return ""s;
			}
		}

		return cpuhz;
	}

	auto get_core_mapping() -> unordered_flat_map<int, int> {
		unordered_flat_map<int, int> core_map;

		//? Try to get core mapping from /proc/cpuinfo
		ifstream cpuinfo(Shared::procPath / "cpuinfo");
		if (cpuinfo.good()) {
			int cpu, core;
			for (string instr; cpuinfo >> instr;) {
				if (instr == "processor") {
					cpuinfo.ignore(SSmax, ':');
					cpuinfo >> cpu;
				}
				else if (instr.starts_with("core")) {
					cpuinfo.ignore(SSmax, ':');
					cpuinfo >> core;
					core_map[cpu] = core;
				}
				cpuinfo.ignore(SSmax, '\n');
			}
		}

		//? If core mapping from cpuinfo was incomplete try to guess remainder, if missing completely map 0-0 1-1 2-2 etc.
		if (cmp_less(core_map.size(), Shared::coreCount)) {
			if (Shared::coreCount % 2 == 0 and (long)core_map.size() == Shared::coreCount / 2) {
				for (int i = 0; i < Shared::coreCount / 2; i++)
					core_map[Shared::coreCount / 2 + i] = i;
			}
			else {
				core_map.clear();
				for (int i = 0; i < Shared::coreCount; i++)
					core_map[i] = i;
			}
		}

		//? Apply user set custom mapping if any
		const auto& custom_map = Config::getS("cpu_core_map");
		if (not custom_map.empty()) {
			try {
				for (const auto& split : ssplit(custom_map)) {
					const auto vals = ssplit(split, ':');
					if (vals.size() != 2) continue;
					int change_id = std::stoi(vals.at(0));
					int new_id = std::stoi(vals.at(1));
					if (not core_map.contains(change_id) or new_id >= Shared::coreCount) continue;
					core_map.at(change_id) = new_id;
				}
			}
			catch (...) {}
		}

		return core_map;
	}

	auto collect(const bool no_update) -> cpu_info& {
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty())) return current_cpu;
		auto& cpu = current_cpu;

		ifstream cread;

		try {
			//? Get cpu load averages from /proc/loadavg
			cread.open(Shared::procPath / "loadavg");
			if (cread.good()) {
				cread >> cpu.load_avg[0] >> cpu.load_avg[1] >> cpu.load_avg[2];
			}
			cread.close();

			//? Get cpu total times for all cores from /proc/stat
			cread.open(Shared::procPath / "stat");
			for (int i = 0; cread.good() and cread.peek() == 'c'; i++) {
				cread.ignore(SSmax, ' ');

				//? Expected on kernel 2.6.3> : 0=user, 1=nice, 2=system, 3=idle, 4=iowait, 5=irq, 6=softirq, 7=steal, 8=guest, 9=guest_nice
				vector<long long> times;
				long long total_sum = 0;

				for (uint64_t val; cread >> val; total_sum += val) {
					times.push_back(val);
				}
				cread.clear();
				if (times.size() < 4) throw std::runtime_error("Malformatted /proc/stat");

				//? Subtract fields 8-9 and any future unknown fields
				const long long totals = max(0ll, total_sum - (times.size() > 8 ? std::accumulate(times.begin() + 8, times.end(), 0) : 0));

				//? Add iowait field if present
				const long long idles = max(0ll, times.at(3) + (times.size() > 4 ? times.at(4) : 0));

				//? Calculate values for totals from first line of stat
				if (i == 0) {
					const long long calc_totals = max(1ll, totals - cpu_old.at("totals"));
					const long long calc_idles = max(1ll, idles - cpu_old.at("idles"));
					cpu_old.at("totals") = totals;
					cpu_old.at("idles") = idles;

					//? Total usage of cpu
					cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

					//? Reduce size if there are more values than needed for graph
					if (cmp_greater(cpu.cpu_percent.at("total").size(), Term::width * 2)) cpu.cpu_percent.at("total").pop_front();

					//? Populate cpu.cpu_percent with all fields from stat
					for (int ii = 0; const auto& val : times) {
						cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
						cpu_old.at(time_names.at(ii)) = val;

						//? Reduce size if there are more values than needed for graph
						if (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), Term::width * 2)) cpu.cpu_percent.at(time_names.at(ii)).pop_front();

						if (++ii == 10) break;
					}
				}
				//? Calculate cpu total for each core
				else {
					if (i > Shared::coreCount) break;
					const long long calc_totals = max(0ll, totals - core_old_totals.at(i-1));
					const long long calc_idles = max(0ll, idles - core_old_idles.at(i-1));
					core_old_totals.at(i-1) = totals;
					core_old_idles.at(i-1) = idles;

					cpu.core_percent.at(i-1).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

					//? Reduce size if there are more values than needed for graph
					if (cpu.core_percent.at(i-1).size() > 40) cpu.core_percent.at(i-1).pop_front();

				}
			}
		}
		catch (const std::exception& e) {
			Logger::debug("get_cpuHz() : " + (string)e.what());
			if (cread.bad()) throw std::runtime_error("Failed to read /proc/stat");
			else throw std::runtime_error("collect() : " + (string)e.what());
		}

		if (Config::getB("show_cpu_freq"))
			cpuHz = get_cpuHz();

		if (Config::getB("check_temp") and got_sensors)
			update_sensors();

		return cpu;
	}
}

namespace Mem {
	bool has_swap = false;
	bool disks_fail = false;
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios = 0;


	mem_info current_mem {};

	auto collect(const bool no_update) -> mem_info& {
		if (Runner::stopping or no_update) return current_mem;
		auto& show_swap = Config::getB("show_swap");
		auto& swap_disk = Config::getB("swap_disk");
		auto& show_disks = Config::getB("show_disks");
		auto& mem = current_mem;

		mem.stats.at("swap_total") = 0;

		//? Read memory info from /proc/meminfo
		ifstream meminfo(Shared::procPath / "meminfo");
		if (meminfo.good()) {
			bool got_avail = false;
			for (string label; meminfo >> label;) {
				if (label == "MemFree:") {
					meminfo >> mem.stats.at("free");
					mem.stats.at("free") <<= 10;
				}
				else if (label == "MemAvailable:") {
					meminfo >> mem.stats.at("available");
					mem.stats.at("available") <<= 10;
					got_avail = true;
				}
				else if (label == "Cached:") {
					meminfo >> mem.stats.at("cached");
					mem.stats.at("cached") <<= 10;
					if (not show_swap and not swap_disk) break;
				}
				else if (label == "SwapTotal:") {
					meminfo >> mem.stats.at("swap_total");
					mem.stats.at("swap_total") <<= 10;
				}
				else if (label == "SwapFree:") {
					meminfo >> mem.stats.at("swap_free");
					mem.stats.at("swap_free") <<= 10;
					break;
				}
				meminfo.ignore(SSmax, '\n');
			}
			if (not got_avail) mem.stats.at("available") = mem.stats.at("free") + mem.stats.at("cached");
			mem.stats.at("used") = Shared::totalMem - mem.stats.at("available");
			if (mem.stats.at("swap_total") > 0) mem.stats.at("swap_used") = mem.stats.at("swap_total") - mem.stats.at("swap_free");
		}
		else
			throw std::runtime_error("Failed to read /proc/meminfo");

		meminfo.close();

		//? Calculate percentages
		for (const auto& name : mem_names) {
			mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / Shared::totalMem));
		}

		if (show_swap and mem.stats.at("swap_total") > 0) {
			for (const auto& name : swap_names) {
				mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
			}
			has_swap = true;
		}
		else
			has_swap = false;

		//? Remove values beyond whats needed for graph creation
		for (auto& [ignored, deq] : mem.percent) {
			if (cmp_greater(deq.size(), Term::width * 2)) deq.pop_front();
		}

		//? Get disks stats
		if (show_disks and not disks_fail) {
			try {
				auto& disks_filter = Config::getS("disks_filter");
				bool filter_exclude = false;
				auto& use_fstab = Config::getB("use_fstab");
				auto& only_physical = Config::getB("only_physical");
				auto& disks = mem.disks;
				ifstream diskread;

				vector<string> filter;
				if (not disks_filter.empty()) {
					filter = ssplit(disks_filter);
					if (filter.at(0).starts_with("exclude=")) {
						filter_exclude = true;
						filter.at(0) = filter.at(0).substr(8);
					}
				}

				//? Get list of "real" filesystems from /proc/filesystems
				vector<string> fstypes;
				if (only_physical and not use_fstab) {
					fstypes = {"zfs", "wslfs", "drvfs"};
					diskread.open(Shared::procPath / "filesystems");
					if (diskread.good()) {
						for (string fstype; diskread >> fstype;) {
							if (not is_in(fstype, "nodev", "squashfs", "nullfs"))
								fstypes.push_back(fstype);
							diskread.ignore(SSmax, '\n');
						}
					}
					else
						throw std::runtime_error("Failed to read /proc/filesystems");
					diskread.close();
				}

				//? Get disk list to use from fstab if enabled
				if (use_fstab and fs::last_write_time("/etc/fstab") != fstab_time) {
					fstab.clear();
					fstab_time = fs::last_write_time("/etc/fstab");
					diskread.open("/etc/fstab");
					if (diskread.good()) {
						for (string instr; diskread >> instr;) {
							if (not instr.starts_with('#')) {
								diskread >> instr;
								if (not is_in(instr, "none", "swap")) fstab.push_back(instr);
							}
							diskread.ignore(SSmax, '\n');
						}
					}
					else
						throw std::runtime_error("Failed to read /etc/fstab");
					diskread.close();
				}

				//? Get mounts from /etc/mtab or /proc/self/mounts
				vector<string> found;
				diskread.open((fs::exists("/etc/mtab") ? fs::path("/etc/mtab") : Shared::procPath / "self/mounts"));
				if (diskread.good()) {
					string dev, mountpoint, fstype;
					while (not diskread.eof()) {
						std::error_code ec;
						diskread >> dev >> mountpoint >> fstype;

						//? Match filter if not empty
						if (not filter.empty()) {
							bool match = v_contains(filter, mountpoint);
							if ((filter_exclude and match) or (not filter_exclude and not match))
								continue;
						}

						if ((not use_fstab and not only_physical)
						or (use_fstab and v_contains(fstab, mountpoint))
						or (not use_fstab and only_physical and v_contains(fstypes, fstype))) {
							found.push_back(mountpoint);

							//? Save mountpoint, name, dev path and path to /sys/block stat file
							if (not disks.contains(mountpoint)) {
								disks[mountpoint] = disk_info{fs::canonical(dev, ec), fs::path(mountpoint).filename()};
								if (disks.at(mountpoint).dev.empty()) disks.at(mountpoint).dev = dev;
								if (disks.at(mountpoint).name.empty()) disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
								string devname = disks.at(mountpoint).dev.filename();
								while (devname.size() >= 2) {
									if (fs::exists("/sys/block/" + devname + "/stat")) {
										disks.at(mountpoint).stat = "/sys/block/" + devname + "/stat";
										break;
									}
									devname.resize(devname.size() - 1);
								}
							}

						}
						diskread.ignore(SSmax, '\n');
					}
					//? Remove disks no longer mounted or filtered out
					if (swap_disk and has_swap) found.push_back("swap");
					for (auto it = disks.begin(); it != disks.end();) {
						if (not v_contains(found, it->first))
							it = disks.erase(it);
						else
							it++;
					}
				}
				else
					throw std::runtime_error("Failed to get mounts from /etc/mtab and /proc/self/mounts");
				diskread.close();

				//? Get disk/partition stats
				for (auto& [mountpoint, disk] : disks) {
					if (not fs::exists(mountpoint)) continue;
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
				if (disks.contains("/")) mem.disks_order.push_back("/");
				if (swap_disk and has_swap) {
					mem.disks_order.push_back("swap");
					if (not disks.contains("swap")) disks["swap"] = {"", "swap"};
					disks.at("swap").total = mem.stats.at("swap_total");
					disks.at("swap").used = mem.stats.at("swap_used");
					disks.at("swap").free = mem.stats.at("swap_free");
					disks.at("swap").used_percent = mem.percent.at("swap_used").back();
					disks.at("swap").free_percent = mem.percent.at("swap_free").back();
				}
				for (const auto& name : found)
					if (not is_in(name, "/", "swap")) mem.disks_order.push_back(name);

				//? Get disks IO
				int64_t sectors_read, sectors_write;
				disk_ios = 0;
				for (auto& [ignored, disk] : disks) {
					if (disk.stat.empty()) continue;
					diskread.open(disk.stat);
					if (diskread.good()) {
						disk_ios++;
						for (int i = 0; i < 2; i++) { diskread >> std::ws; diskread.ignore(SSmax, ' '); }
						diskread >> sectors_read;
						if (disk.io_read.empty())
							disk.io_read.push_back(0);
						else
							disk.io_read.push_back(max(0l, (sectors_read - disk.old_io.at(0)) * 512));
						disk.old_io.at(0) = sectors_read;
						if (cmp_greater(disk.io_read.size(), Term::width * 2)) disk.io_read.pop_front();

						for (int i = 0; i < 3; i++) { diskread >> std::ws; diskread.ignore(SSmax, ' '); }
						diskread >> sectors_write;
						if (disk.io_write.empty())
							disk.io_write.push_back(0);
						else
							disk.io_write.push_back(max(0l, (sectors_write - disk.old_io.at(1)) * 512));
						disk.old_io.at(1) = sectors_write;
						if (cmp_greater(disk.io_write.size(), Term::width * 2)) disk.io_write.pop_front();
					}
					diskread.close();
				}
			}
			catch (const std::exception& e) {
				Logger::warning("Error in Mem::collect() : " + (string)e.what());
				disks_fail = true;
			}
		}

		return mem;
	}

}

namespace Net {
	unordered_flat_map<string, net_info> current_net;
	net_info empty_net = {};
	vector<string> interfaces;
	string selected_iface;
	int errors = 0;
	unordered_flat_map<string, uint64_t> graph_max = { {"download", {}}, {"upload", {}} };
	unordered_flat_map<string, array<int, 2>> max_count = { {"download", {}}, {"upload", {}} };
	bool rescale = true;

	//* RAII wrapper for getifaddrs
	class getifaddr_wrapper {
		struct ifaddrs* ifaddr;
	public:
		int status;
		getifaddr_wrapper() { status = getifaddrs(&ifaddr); }
		~getifaddr_wrapper() { freeifaddrs(ifaddr); }
		auto operator()() -> struct ifaddrs* { return ifaddr; }
	};

	auto collect(const bool no_update) -> net_info& {
		auto& net = current_net;
		auto& config_iface = Config::getS("net_iface");
		auto& net_sync = Config::getB("net_sync");
		auto& net_auto = Config::getB("net_auto");

		if (not no_update and errors < 3) {
			//? Get interface list using getifaddrs() wrapper
			getifaddr_wrapper if_wrap = {};
			if (if_wrap.status != 0) {
				errors++;
				Logger::error("Net::collect() -> getifaddrs() failed with id " + to_string(if_wrap.status));
				return empty_net;
			}
			int family = 0;
			char ip[NI_MAXHOST];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all items in getifaddrs() list
			for (auto* ifa = if_wrap(); ifa != NULL; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == NULL) continue;
				family = ifa->ifa_addr->sa_family;
				const auto& iface = ifa->ifa_name;

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

			//? Get total recieved and transmitted bytes + device address if no ip was found
			for (const auto& iface : interfaces) {
				if (net.at(iface).ipv4.empty() and net.at(iface).ipv6.empty())
					net.at(iface).ipv4 = readfile("/sys/class/net/" + iface + "/address");

				for (const string dir : {"download", "upload"}) {
					const fs::path sys_file = "/sys/class/net/" + iface + "/statistics/" + (dir == "download" ? "rx_bytes" : "tx_bytes");
					auto& saved_stat = net.at(iface).stat.at(dir);
					auto& bandwidth = net.at(iface).bandwidth.at(dir);

					const uint64_t val = max(stoul(readfile(sys_file, "0")), saved_stat.last);

					//? Update speed, total and top values
					saved_stat.speed = val - (saved_stat.last == 0 ? val : saved_stat.last);
					if (saved_stat.speed > saved_stat.top) saved_stat.top = saved_stat.speed;
					if (saved_stat.offset > val) saved_stat.offset = 0;
					saved_stat.total = val - saved_stat.offset;
					saved_stat.last = val;

					//? Add values to graph
					bandwidth.push_back(saved_stat.speed);
					if (cmp_greater(bandwidth.size(), Term::width.load() * 2)) bandwidth.pop_front();

					//? Set counters for auto scaling
					if (net_auto and selected_iface == iface) {
						if (saved_stat.speed > graph_max[dir]) {
							++max_count[dir][0];
							if (max_count[dir][1] > 0) --max_count[dir][1];
						}
						else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
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
			}
		}

		//? Return empty net_info struct if no interfaces was found
		if (net.empty())
			return empty_net;

		//? Find an interface to display if selected isn't set or valid
		if (selected_iface.empty() or not v_contains(interfaces, selected_iface)) {
			max_count["download"][0] = max_count["download"][1] = max_count["upload"][0] = max_count["upload"][1] = 0;
			redraw = true;
			if (net_auto) rescale = true;
			if (not config_iface.empty() and v_contains(interfaces, config_iface)) selected_iface = config_iface;
			else {
				//? Sort interfaces by total upload + download bytes
				auto sorted_interfaces = interfaces;
				rng::sort(sorted_interfaces, [&](const auto& a, const auto& b){
					return 	cmp_greater(net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
										net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
				});
				selected_iface.clear();
				//? Try to set to a connected interface
				for (const auto& iface : sorted_interfaces) {
					if (net.at(iface).connected) selected_iface = iface;
					break;
				}
				//? If no interface is connected set to first available
				if (selected_iface.empty()) selected_iface = sorted_interfaces.at(0);
			}
		}

		//? Calculate max scale for graphs if needed
		if (net_auto) {
			bool sync = false;
			for (const auto& dir: {"download", "upload"}) {
				for (const auto& sel : {0, 1}) {
					if (rescale or max_count[dir][sel] >= 5) {
						const uint64_t avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
							? std::accumulate(net.at(selected_iface).bandwidth.at(dir).rbegin(), net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0) / 5
							: net[selected_iface].stat[dir].speed);
						graph_max[dir] = max(uint64_t(avg_speed * (sel == 0 ? 1.3 : 3.0)), 10ul << 10);
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
		fs::file_time_type passwd_time;
		uint64_t cputimes;

		int counter = 0;
	}
	int collapse = -1, expand = -1;
	uint64_t old_cputimes = 0;
	atomic<int> numpids = 0;

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

		//? Add process to vector if not filtered out or currently in a collapsed sub-tree
		if (not collapsed and not filtering) {
			out_procs.push_back(cur_proc);
			//? Try to find name of the binary file and append to program name if not the same (replacing cmd string)
			if (std::string_view cmd_view = cur_proc.cmd; not cmd_view.empty() and not cmd_view.starts_with('(')) {
				cmd_view = cmd_view.substr(0, min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				if (cmd_view == cur_proc.name)
					out_procs.back().cmd.clear();
				else
					out_procs.back().cmd = '(' + (string)cmd_view + ')';
			}
		}

		//? Recursive iteration over all children
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

		//? Add tree terminator symbol if it's the last child in a sub-tree
		if (out_procs.size() > cur_pos + 1 and not out_procs.back().prefix.ends_with("]─"))
			out_procs.back().prefix.replace(out_procs.back().prefix.size() - 8, 8, " └─ ");

		//? Add collapse/expand symbols if process have any children
		out_procs.at(cur_pos).prefix = " │ "s * cur_depth + (children > 0 ? (cache.at(cur_proc.pid).collapsed ? "[+]─" : "[-]─") : " ├─ ");
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, const uint64_t uptime, vector<proc_info>& procs) {
		fs::path pid_path = Shared::procPath / std::to_string(pid);

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
		detailed.cpu_percent.push_back(round(detailed.entry.cpu_p));
		if (cmp_greater(detailed.cpu_percent.size(), Term::width.load())) detailed.cpu_percent.pop_front();

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
			uint64_t rss = 0;
			try {
				while (d_read.good()) {
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

		if (cmp_greater(detailed.mem_bytes.size(), Term::width.load())) detailed.mem_bytes.pop_front();

		//? Get bytes read and written from proc/[pid]/io
		if (fs::exists(pid_path / "io")) {
			d_read.open(pid_path / "io");
			try {
				string name;
				while (d_read.good()) {
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
			d_read.close();
		}
	}

	//* Collects and sorts process information from /proc
	auto collect(const bool no_update) -> vector<proc_info> {
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

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not cache.empty()) {
			procs = current_procs;
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, round(uptime), procs);
		}
		//* ---------------------------------------------Collection start----------------------------------------------
		else {
			//? Update uid_user map if /etc/passwd changed since last run
			if (not Shared::passwd_path.empty() and fs::last_write_time(Shared::passwd_path) != passwd_time) {
				string r_uid, r_user;
				passwd_time = fs::last_write_time(Shared::passwd_path);
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
				else {
					Shared::passwd_path.clear();
				}
				pread.close();
			}

			//? Get cpu total times from /proc/stat
			cputimes = 0;
			pread.open(Shared::procPath / "stat");
			if (pread.good()) {
				pread.ignore(SSmax, ' ');
				for (uint64_t times; pread >> times; cputimes += times);
			}
			else throw std::runtime_error("Failure to read /proc/stat");
			pread.close();

			//? Iterate over all pids in /proc
			for (const auto& d: fs::directory_iterator(Shared::procPath)) {
				if (Runner::stopping)
					return procs;
				if (pread.is_open()) pread.close();

				const string pid_str = d.path().filename();
				if (not isdigit(pid_str[0])) continue;

				proc_info new_proc (stoul(pid_str));

				//? Cache program name, command and username
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

				new_proc.name = cache.at(new_proc.pid).name;
				new_proc.cmd = cache.at(new_proc.pid).cmd;
				new_proc.user = cache.at(new_proc.pid).user;

				//? Parse /proc/[pid]/stat
				pread.open(d.path() / "stat");
				if (not pread.good()) continue;

				//? Check cached value for whitespace characters in name and set offset to get correct fields from stat file
				const auto& offset = cache.at(new_proc.pid).name_offset;
				short_str.clear();
				size_t x = 0, next_x = 3;
				uint64_t cpu_t = 0;
				try {
					for (;;) {
						while (pread.good() and ++x < next_x + offset) {
							pread.ignore(SSmax, ' ');
						}
						if (pread.bad()) break;

						getline(pread, short_str, ' ');

						switch (x-offset) {
							case 3: //? Process state
								new_proc.state = short_str.at(0);
								continue;
							case 4: //? Parent pid
								new_proc.ppid = stoull(short_str);
								next_x = 14;
								continue;
							case 14: //? Process utime
								cpu_t = stoull(short_str);
								continue;
							case 15: //? Process stime
								cpu_t += stoull(short_str);
								next_x = 19;
								continue;
							case 19: //? Nice value
								new_proc.p_nice = stoull(short_str);
								continue;
							case 20: //? Number of threads
								new_proc.threads = stoull(short_str);
								if (cache.at(new_proc.pid).cpu_s == 0) {
									next_x = 22;
									cache.at(new_proc.pid).cpu_t = cpu_t;
								}
								else
									next_x = 24;
								continue;
							case 22: //? Save cpu seconds to cache if missing
								cache.at(new_proc.pid).cpu_s = stoull(short_str);
								next_x = 24;
								continue;
							case 24: //? RSS memory (can be inaccurate, but parsing smaps increases total cpu usage by ~20x)
								new_proc.mem = stoull(short_str) * Shared::pageSize;
								next_x = 39;
								continue;
							case 39: //? CPU number last executed on
								new_proc.cpu_n = stoull(short_str);
								goto stat_loop_done;
						}
					}

				}
				catch (const std::invalid_argument&) { continue; }
				catch (const std::out_of_range&) { continue; }

				stat_loop_done:
				pread.close();

				if (x-offset < 24) continue;

				//? Process cpu usage since last update
				new_proc.cpu_p = round(cmult * 1000 * (cpu_t - cache.at(new_proc.pid).cpu_t) / max(1ul, cputimes - old_cputimes)) / 10.0;

				//? Process cumulative cpu usage since process start
				new_proc.cpu_c = (double)cpu_t / max(1.0, (uptime * Shared::clkTck) - cache.at(new_proc.pid).cpu_s);

				//? Update cache with latest cpu times
				cache.at(new_proc.pid).cpu_t = cpu_t;

				if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
					got_detailed = true;
				}

				//? Push process to vector
				procs.push_back(new_proc);

			}

			//? Clear dead processes from cache at a regular interval
			if (++counter >= 1000 or (cache.size() > procs.size() + 100)) {
				counter = 0;
				for (auto it = cache.begin(); it != cache.end();) {
					if (rng::find(procs, it->first, &proc_info::pid) == procs.end())
						it = cache.erase(it);
					else
						it++;
				}
			}

			//? Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, round(uptime), procs);
			}
			else if (show_detailed and not got_detailed and detailed.status != "Dead") {
				detailed.status = "Dead";
				redraw = true;
			}

			old_cputimes = cputimes;
			current_procs = procs;
		}
		//* ---------------------------------------------Collection done-----------------------------------------------

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
				case 0: rng::sort(procs, cmp, &proc_info::pid); 	break;
				case 1: rng::sort(procs, cmp, &proc_info::name);	break;
				case 2: rng::sort(procs, cmp, &proc_info::cmd); 	break;
				case 3: rng::sort(procs, cmp, &proc_info::threads); break;
				case 4: rng::sort(procs, cmp, &proc_info::user); 	break;
				case 5: rng::sort(procs, cmp, &proc_info::mem); 	break;
				case 6: rng::sort(procs, cmp, &proc_info::cpu_p);   break;
				case 7: rng::sort(procs, cmp, &proc_info::cpu_c);   break;
		}

		//* When sorting with "cpu lazy" push processes over threshold cpu usage to the front regardless of cumulative usage
		if (not tree and not reverse and sorting == "cpu lazy") {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, x = 0, offset = 0; i < procs.size(); i++) {
				if (i <= 5 and procs.at(i).cpu_p > max)
					max = procs.at(i).cpu_p;
				else if (i == 6)
					target = (max > 30.0) ? max : 10.0;
				if (i == offset and procs.at(i).cpu_p > 30.0)
					offset++;
				else if (procs.at(i).cpu_p > target) {
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

namespace Tools {
	double system_uptime() {
		string upstr;
		ifstream pread(Shared::procPath / "uptime");
		getline(pread, upstr, ' ');
		pread.close();
		return stod(upstr);
	}
}

#endif