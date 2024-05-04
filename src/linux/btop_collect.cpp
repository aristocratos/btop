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

#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <ranges>
#include <cmath>
#include <unistd.h>
#include <numeric>
#include <sys/statvfs.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h> // for inet_ntop()
#include <filesystem>
#include <future>
#include <dlfcn.h>
#include <unordered_map>
#include <utility>

#if defined(RSMI_STATIC)
	#include <rocm_smi/rocm_smi.h>
#endif

#if !(defined(STATIC_BUILD) && defined(__GLIBC__))
	#include <pwd.h>
#endif

#include "../btop_shared.hpp"
#include "../btop_config.hpp"
#include "../btop_tools.hpp"

using std::clamp;
using std::cmp_greater;
using std::cmp_less;
using std::ifstream;
using std::max;
using std::min;
using std::numeric_limits;
using std::round;
using std::streamsize;
using std::vector;
using std::future;
using std::async;
using std::pair;


namespace fs = std::filesystem;
namespace rng = std::ranges;

using namespace Tools;
using namespace std::literals; // for operator""s
using namespace std::chrono_literals;
//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields = {"Auto", "total"};
	vector<string> available_sensors = {"Auto"};
	cpu_info current_cpu;
	fs::path freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";
	bool got_sensors{};
	bool cpu_temp_only{};

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search /proc/cpuinfo for a cpu name
	string get_cpuName();

	struct Sensor {
		fs::path path;
		string label;
		int64_t temp{};
		int64_t high{};
		int64_t crit{};
	};

	std::unordered_map<string, Sensor> found_sensors;
	string cpu_sensor;
	vector<string> core_sensors;
	std::unordered_map<int, int> core_mapping;
}

namespace Gpu {
	vector<gpu_info> gpus;
#ifdef GPU_SUPPORT
	//? NVIDIA data collection
	namespace Nvml {
		//? NVML defines, structs & typedefs
		#define NVML_DEVICE_NAME_BUFFER_SIZE        64
		#define NVML_SUCCESS                         0
		#define NVML_TEMPERATURE_THRESHOLD_SHUTDOWN  0
		#define NVML_CLOCK_GRAPHICS                  0
		#define NVML_CLOCK_MEM                       2
		#define NVML_TEMPERATURE_GPU                 0
		#define NVML_PCIE_UTIL_TX_BYTES              0
		#define NVML_PCIE_UTIL_RX_BYTES              1

		typedef void* nvmlDevice_t; // we won't be accessing any of the underlying struct's properties, so this is fine
		typedef int nvmlReturn_t, // enums are basically ints
					nvmlTemperatureThresholds_t,
					nvmlClockType_t,
					nvmlPstates_t,
					nvmlTemperatureSensors_t,
					nvmlPcieUtilCounter_t;

		struct nvmlUtilization_t {unsigned int gpu, memory;};
		struct nvmlMemory_t {unsigned long long total, free, used;};

		//? Function pointers
		const char* (*nvmlErrorString)(nvmlReturn_t);
		nvmlReturn_t (*nvmlInit)();
		nvmlReturn_t (*nvmlShutdown)();
		nvmlReturn_t (*nvmlDeviceGetCount)(unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
		nvmlReturn_t (*nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);
		nvmlReturn_t (*nvmlDeviceGetPowerManagementLimit)(nvmlDevice_t, unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetTemperatureThreshold)(nvmlDevice_t, nvmlTemperatureThresholds_t, unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
		nvmlReturn_t (*nvmlDeviceGetClockInfo)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetPowerState)(nvmlDevice_t, nvmlPstates_t*);
		nvmlReturn_t (*nvmlDeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
		nvmlReturn_t (*nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
		nvmlReturn_t (*nvmlDeviceGetPcieThroughput)(nvmlDevice_t, nvmlPcieUtilCounter_t, unsigned int*);

		//? Data
		void* nvml_dl_handle;
		bool initialized = false;
		bool init();
		bool shutdown();
		template <bool is_init> bool collect(gpu_info* gpus_slice);
		vector<nvmlDevice_t> devices;
		unsigned int device_count = 0;
	}

	//? AMD data collection
	namespace Rsmi {

	//? RSMI defines, structs & typedefs
	#define RSMI_DEVICE_NAME_BUFFER_SIZE 128

	#if !defined(RSMI_STATIC)
		#define RSMI_MAX_NUM_FREQUENCIES_V5  32
		#define RSMI_MAX_NUM_FREQUENCIES_V6  33
		#define RSMI_STATUS_SUCCESS           0
		#define RSMI_MEM_TYPE_VRAM            0
		#define RSMI_TEMP_CURRENT             0
		#define RSMI_TEMP_TYPE_EDGE           0
		#define RSMI_CLK_TYPE_MEM             4
		#define RSMI_CLK_TYPE_SYS             0
		#define RSMI_TEMP_MAX                 1

		typedef int rsmi_status_t,
					rsmi_temperature_metric_t,
					rsmi_clk_type_t,
					rsmi_memory_type_t;

		struct rsmi_version_t {uint32_t major,  minor,  patch; const char* build;};
		struct rsmi_frequencies_t_v5 {uint32_t num_supported, current; uint64_t frequency[RSMI_MAX_NUM_FREQUENCIES_V5];};
		struct rsmi_frequencies_t_v6 {bool has_deep_sleep; uint32_t num_supported, current; uint64_t frequency[RSMI_MAX_NUM_FREQUENCIES_V6];};

		//? Function pointers
		rsmi_status_t (*rsmi_init)(uint64_t);
		rsmi_status_t (*rsmi_shut_down)();
		rsmi_status_t (*rsmi_version_get)(rsmi_version_t*);
		rsmi_status_t (*rsmi_num_monitor_devices)(uint32_t*);
		rsmi_status_t (*rsmi_dev_name_get)(uint32_t, char*, size_t);
		rsmi_status_t (*rsmi_dev_power_cap_get)(uint32_t, uint32_t, uint64_t*);
		rsmi_status_t (*rsmi_dev_temp_metric_get)(uint32_t, uint32_t, rsmi_temperature_metric_t, int64_t*);
		rsmi_status_t (*rsmi_dev_busy_percent_get)(uint32_t, uint32_t*);
		rsmi_status_t (*rsmi_dev_memory_busy_percent_get)(uint32_t, uint32_t*);
		rsmi_status_t (*rsmi_dev_gpu_clk_freq_get_v5)(uint32_t, rsmi_clk_type_t, rsmi_frequencies_t_v5*);
		rsmi_status_t (*rsmi_dev_gpu_clk_freq_get_v6)(uint32_t, rsmi_clk_type_t, rsmi_frequencies_t_v6*);
		rsmi_status_t (*rsmi_dev_power_ave_get)(uint32_t, uint32_t, uint64_t*);
		rsmi_status_t (*rsmi_dev_memory_total_get)(uint32_t, rsmi_memory_type_t, uint64_t*);
		rsmi_status_t (*rsmi_dev_memory_usage_get)(uint32_t, rsmi_memory_type_t, uint64_t*);
		rsmi_status_t (*rsmi_dev_pci_throughput_get)(uint32_t, uint64_t*, uint64_t*, uint64_t*);

		uint32_t version_major = 0;

		//? Data
		void* rsmi_dl_handle;
	#endif
		bool initialized = false;
		bool init();
		bool shutdown();
		template <bool is_init> bool collect(gpu_info* gpus_slice);
		uint32_t device_count = 0;
	}
#endif
}

namespace Mem {
	double old_uptime;
}

namespace Shared {

	fs::path procPath, passwd_path;
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
			coreCount = sysconf(_SC_NPROCESSORS_CONF);
			if (coreCount < 1) {
				coreCount = 1;
				Logger::warning("Could not determine number of cores, defaulting to 1.");
			}
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

		//? Init for namespace Cpu
		if (not fs::exists(Cpu::freq_path) or access(Cpu::freq_path.c_str(), R_OK) == -1) Cpu::freq_path.clear();
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Cpu::collect();
		if (Runner::coreNum_reset) Runner::coreNum_reset = false;
		for (auto& [field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty() and not v_contains(Cpu::available_fields, field)) Cpu::available_fields.push_back(field);
		}
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();
		for (const auto& [sensor, ignored] : Cpu::found_sensors) {
			Cpu::available_sensors.push_back(sensor);
		}
		Cpu::core_mapping = Cpu::get_core_mapping();

		//? Init for namespace Gpu
	#ifdef GPU_SUPPORT
		Gpu::Nvml::init();
		Gpu::Rsmi::init();
		if (not Gpu::gpu_names.empty()) {
			for (auto const& [key, _] : Gpu::gpus[0].gpu_percent)
				Cpu::available_fields.push_back(key);
			for (auto const& [key, _] : Gpu::shared_gpu_percent)
				Cpu::available_fields.push_back(key);

			using namespace Gpu;
			gpu_b_height_offsets.resize(gpus.size());
			for (size_t i = 0; i < gpu_b_height_offsets.size(); ++i)
				gpu_b_height_offsets[i] = gpus[i].supported_functions.gpu_utilization
					   + gpus[i].supported_functions.pwr_usage
					   + (gpus[i].supported_functions.mem_total or gpus[i].supported_functions.mem_used)
						* (1 + 2*(gpus[i].supported_functions.mem_total and gpus[i].supported_functions.mem_used) + 2*gpus[i].supported_functions.mem_utilization);
		}
	#endif

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Mem::collect();

		Logger::debug("Shared::init() : Initialized.");
	}
}

namespace Cpu {
	string cpuName;
	string cpuHz;
	bool has_battery = true;
	tuple<int, float, long, string> current_bat;

	const array time_names {
		"user"s, "nice"s, "system"s, "idle"s, "iowait"s,
		"irq"s, "softirq"s, "steal"s, "guest"s, "guest_nice"s
	};

	std::unordered_map<string, long long> cpu_old = {
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
			else if (not cpuinfo.eof()) {
				cpuinfo.ignore(1);
				getline(cpuinfo, name);
			}
			else if (fs::exists("/sys/devices")) {
				for (const auto& d : fs::directory_iterator("/sys/devices")) {
					if (string(d.path().filename()).starts_with("arm")) {
						name = d.path().filename();
						break;
					}
				}
				if (not name.empty()) {
					auto name_vec = ssplit(name, '_');
					if (name_vec.size() < 2) return capitalize(name);
					else return capitalize(name_vec.at(1)) + (name_vec.size() > 2 ? ' ' + capitalize(name_vec.at(2)) : "");
				}

			}

			auto name_vec = ssplit(name, ' ');

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
				if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1).size() != 1)
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
				for (const auto& replace : {"Processor", "CPU", "(R)", "(TM)", "Intel", "AMD", "Core"}) {
					name = s_replace(name, replace, "");
					name = s_replace(name, "  ", " ");
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

					for (const auto & file : fs::directory_iterator(add_path)) {
						if (string(file.path().filename()) == "device") {
							for (const auto & dev_file : fs::directory_iterator(file.path())) {
								string dev_filename = dev_file.path().filename();
								if (dev_filename.starts_with("temp") and dev_filename.ends_with("_input")) {
									search_paths.push_back(file.path());
									break;
								}
							}
						}

						string filename = file.path().filename();
						if (filename.starts_with("temp") and filename.ends_with("_input")) {
							search_paths.push_back(add_path);
							break;
						}
					}
				}
			}
			if (not got_coretemp and fs::exists(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
				for (auto& d : fs::directory_iterator(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
					fs::path add_path = fs::canonical(d.path());

					for (const auto & file : fs::directory_iterator(add_path)) {
						string filename = file.path().filename();
						if (filename.starts_with("temp") and filename.ends_with("_input") and not v_contains(search_paths, add_path)) {
								search_paths.push_back(add_path);
								got_coretemp = true;
								break;
						}
					}
				}
			}
			//? Scan any found directories for temperature sensors
			if (not search_paths.empty()) {
				for (const auto& path : search_paths) {
					const string pname = readfile(path / "name", path.filename());
					for (const auto & file : fs::directory_iterator(path)) {
						const string file_suffix = "input";
						const int file_id = atoi(file.path().filename().c_str() + 4); // skip "temp" prefix
						string file_path = file.path();

						if (!s_contains(file_path, file_suffix) or s_contains(file_path, "nvme")) {
							continue;
						}

						const string basepath = file_path.erase(file_path.find(file_suffix), file_suffix.length());
						const string label = readfile(fs::path(basepath + "label"), "temp" + to_string(file_id));
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

		if (not got_coretemp or core_sensors.empty()) {
			cpu_temp_only = true;
		}
		else {
			rng::sort(core_sensors, rng::less{});
			rng::stable_sort(core_sensors, [](const auto& a, const auto& b){
				return a.size() < b.size();
			});
		}

		if (cpu_sensor.empty() and not found_sensors.empty()) {
			for (const auto& [name, sensor] : found_sensors) {
				if (s_contains(str_to_lower(name), "cpu") or s_contains(str_to_lower(name), "k10temp")) {
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
		static int failed{};

		if (failed > 4)
			return ""s;

		string cpuhz;
		try {
			double hz{};
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

			if (hz <= 1 or hz >= 1000000)
				throw std::runtime_error("Failed to read /sys/devices/system/cpu/cpufreq/policy and /proc/cpuinfo.");

			if (hz >= 1000) {
				if (hz >= 10000) cpuhz = to_string((int)round(hz / 1000)); // Future proof until we reach THz speeds :)
				else cpuhz = to_string(round(hz / 100) / 10.0).substr(0, 3);
				cpuhz += " GHz";
			}
			else if (hz > 0)
				cpuhz = to_string((int)hz) + " MHz";

		}
		catch (const std::exception& e) {
			if (++failed < 5)
				return ""s;
			else {
				Logger::warning("get_cpuHZ() : " + string{e.what()});
				return ""s;
			}
		}

		return cpuhz;
	}

	auto get_core_mapping() -> std::unordered_map<int, int> {
		std::unordered_map<int, int> core_map;
		if (cpu_temp_only) return core_map;

		//? Try to get core mapping from /proc/cpuinfo
		ifstream cpuinfo(Shared::procPath / "cpuinfo");
		if (cpuinfo.good()) {
			int cpu{};
			int core{};
			int n{};
			for (string instr; cpuinfo >> instr;) {
				if (instr == "processor") {
					cpuinfo.ignore(SSmax, ':');
					cpuinfo >> cpu;
				}
				else if (instr.starts_with("core")) {
					cpuinfo.ignore(SSmax, ':');
					cpuinfo >> core;
					if (std::cmp_greater_equal(core, core_sensors.size())) {
						if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
						core_map[cpu] = n++;
					}
					else
						core_map[cpu] = core;
				}
				cpuinfo.ignore(SSmax, '\n');
			}
		}

		//? If core mapping from cpuinfo was incomplete try to guess remainder, if missing completely, map 0-0 1-1 2-2 etc.
		if (cmp_less(core_map.size(), Shared::coreCount)) {
			if (Shared::coreCount % 2 == 0 and (long)core_map.size() == Shared::coreCount / 2) {
				for (int i = 0, n = 0; i < Shared::coreCount / 2; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[Shared::coreCount / 2 + i] = n++;
				}
			}
			else {
				core_map.clear();
				for (int i = 0, n = 0; i < Shared::coreCount; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[i] = n++;
				}
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
					if (not core_map.contains(change_id) or cmp_greater(new_id, core_sensors.size())) continue;
					core_map.at(change_id) = new_id;
				}
			}
			catch (...) {}
		}

		return core_map;
	}

	struct battery {
		fs::path base_dir, energy_now, charge_now, energy_full, charge_full, power_now, current_now, voltage_now, status, online;
		string device_type;
		bool use_energy_or_charge = true;
		bool use_power = true;
	};

	auto get_battery() -> tuple<int, float, long, string> {
		if (not has_battery) return {0, 0, 0, ""};
		static string auto_sel;
		static std::unordered_map<string, battery> batteries;

		//? Get paths to needed files and check for valid values on first run
		if (batteries.empty() and has_battery) {
			try {
				if (fs::exists("/sys/class/power_supply")) {
					for (const auto& d : fs::directory_iterator("/sys/class/power_supply")) {
						//? Only consider online power supplies of type Battery or UPS
						//? see kernel docs for details on the file structure and contents
						//? https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-power
						battery new_bat;
						fs::path bat_dir;
						try {
							if (not d.is_directory()
								or not fs::exists(d.path() / "type")
								or not fs::exists(d.path() / "present")
								or stoi(readfile(d.path() / "present")) != 1)
								continue;
							string dev_type = readfile(d.path() / "type");
							if (is_in(dev_type, "Battery", "UPS")) {
								bat_dir = d.path();
								new_bat.base_dir = d.path();
								new_bat.device_type = dev_type;
							}
						} catch (...) {
							//? skip power supplies not conforming to the kernel standard
							continue;
						}

						if (fs::exists(bat_dir / "energy_now")) new_bat.energy_now = bat_dir / "energy_now";
						else if (fs::exists(bat_dir / "charge_now")) new_bat.charge_now = bat_dir / "charge_now";
						else new_bat.use_energy_or_charge = false;

						if (fs::exists(bat_dir / "energy_full")) new_bat.energy_full = bat_dir / "energy_full";
						else if (fs::exists(bat_dir / "charge_full")) new_bat.charge_full = bat_dir / "charge_full";
						else new_bat.use_energy_or_charge = false;

						if (not new_bat.use_energy_or_charge and not fs::exists(bat_dir / "capacity")) {
							continue;
						}

						if (fs::exists(bat_dir / "power_now")) {
							new_bat.power_now = bat_dir / "power_now";
						}
						else if ((fs::exists(bat_dir / "current_now")) and (fs::exists(bat_dir / "voltage_now"))) {
							 new_bat.current_now = bat_dir / "current_now";
							 new_bat.voltage_now = bat_dir / "voltage_now";
						}
						else {
							new_bat.use_power = false;
						}

						if (fs::exists(bat_dir / "AC0/online")) new_bat.online = bat_dir / "AC0/online";
						else if (fs::exists(bat_dir / "AC/online")) new_bat.online = bat_dir / "AC/online";

						batteries[bat_dir.filename()] = new_bat;
						Config::available_batteries.push_back(bat_dir.filename());
					}
				}
			}
			catch (...) {
				batteries.clear();
			}
			if (batteries.empty()) {
				has_battery = false;
				return {0, 0, 0, ""};
			}
		}

		auto& battery_sel = Config::getS("selected_battery");

		if (auto_sel.empty()) {
			for (auto& [name, bat] : batteries) {
				if (bat.device_type == "Battery") {
					auto_sel = name;
					break;
				}
			}
			if (auto_sel.empty()) auto_sel = batteries.begin()->first;
		}

		auto& b = (battery_sel != "Auto" and batteries.contains(battery_sel) ? batteries.at(battery_sel) : batteries.at(auto_sel));

		int percent = -1;
		long seconds = -1;
		float watts = -1;

		//? Try to get battery percentage
		if (percent < 0) {
			try {
				percent = stoll(readfile(b.base_dir / "capacity", "-1"));
			}
			catch (const std::invalid_argument&) { }
			catch (const std::out_of_range&) { }
		}
		if (b.use_energy_or_charge and percent < 0) {
			try {
				percent = round(100.0 * stoll(readfile(b.energy_now, "-1")) / stoll(readfile(b.energy_full, "1")));
			}
			catch (const std::invalid_argument&) { }
			catch (const std::out_of_range&) { }
		}
		if (b.use_energy_or_charge and percent < 0) {
			try {
				percent = round(100.0 * stoll(readfile(b.charge_now, "-1")) / stoll(readfile(b.charge_full, "1")));
			}
			catch (const std::invalid_argument&) { }
			catch (const std::out_of_range&) { }
		}
		if (percent < 0) {
			has_battery = false;
			return {0, 0, 0, ""};
		}

		//? Get charging/discharging status
		string status = str_to_lower(readfile(b.base_dir / "status", "unknown"));
		if (status == "unknown" and not b.online.empty()) {
			const auto online = readfile(b.online, "0");
			if (online == "1" and percent < 100) status = "charging";
			else if (online == "1") status = "full";
			else status = "discharging";
		}

		//? Get seconds to empty
		if (not is_in(status, "charging", "full")) {
			if (b.use_energy_or_charge ) {
				if (not b.power_now.empty()) {
					try {
						seconds = round((double)stoll(readfile(b.energy_now, "0")) / stoll(readfile(b.power_now, "1")) * 3600);
					}
					catch (const std::invalid_argument&) { }
					catch (const std::out_of_range&) { }
				}
				else if (not b.current_now.empty()) {
					try {
						seconds = round((double)stoll(readfile(b.charge_now, "0")) / (double)stoll(readfile(b.current_now, "1")) * 3600);
					}
					catch (const std::invalid_argument&) { }
					catch (const std::out_of_range&) { }
				}
			}

			if (seconds < 0 and fs::exists(b.base_dir / "time_to_empty")) {
				try {
					seconds = stoll(readfile(b.base_dir / "time_to_empty", "0")) * 60;
				}
				catch (const std::invalid_argument&) { }
				catch (const std::out_of_range&) { }
			}
		}

		//? Get power draw
		if (b.use_power) {
			if (not b.power_now.empty()) {
				try {
					watts = (float)stoll(readfile(b.power_now, "-1")) / 1000000.0;
				}
				catch (const std::invalid_argument&) { }
				catch (const std::out_of_range&) { }
			}
			else if (not b.voltage_now.empty() and not b.current_now.empty()) {
				try {
					watts = (float)stoll(readfile(b.current_now, "-1")) / 1000000.0 * stoll(readfile(b.voltage_now, "1")) / 1000000.0;
				}
				catch (const std::invalid_argument&) { }
				catch (const std::out_of_range&) { }
			}

		}

		return {percent, watts, seconds, status};
	}

	auto collect(bool no_update) -> cpu_info& {
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty())) return current_cpu;
		auto& cpu = current_cpu;

		if (Config::getB("show_cpu_freq"))
			cpuHz = get_cpuHz();

		if (getloadavg(cpu.load_avg.data(), cpu.load_avg.size()) < 0) {
			Logger::error("failed to get load averages");
		}

		ifstream cread;

		try {
			//? Get cpu total times for all cores from /proc/stat
			string cpu_name;
			cread.open(Shared::procPath / "stat");
			int i = 0;
			int target = Shared::coreCount;
			for (; i <= target or (cread.good() and cread.peek() == 'c'); i++) {
				//? Make sure to add zero value for missing core values if at end of file
				if ((not cread.good() or cread.peek() != 'c') and i <= target) {
					if (i == 0) throw std::runtime_error("Failed to parse /proc/stat");
					else {
						//? Fix container sizes if new cores are detected
						while (cmp_less(cpu.core_percent.size(), i)) {
							core_old_totals.push_back(0);
							core_old_idles.push_back(0);
							cpu.core_percent.emplace_back();
						}
						cpu.core_percent.at(i-1).push_back(0);
					}
				}
				else {
					if (i == 0) cread.ignore(SSmax, ' ');
					else {
						cread >> cpu_name;
						int cpuNum = std::stoi(cpu_name.substr(3));
						if (cpuNum >= target - 1) target = cpuNum + (cread.peek() == 'c' ? 2 : 1);

						//? Add zero value for core if core number is missing from /proc/stat
						while (i - 1 < cpuNum) {
							//? Fix container sizes if new cores are detected
							while (cmp_less(cpu.core_percent.size(), i)) {
								core_old_totals.push_back(0);
								core_old_idles.push_back(0);
								cpu.core_percent.emplace_back();
							}
							cpu.core_percent[i-1].push_back(0);
							if (cpu.core_percent.at(i-1).size() > 40) cpu.core_percent.at(i-1).pop_front();
							i++;
						}
					}

					//? Expected on kernel 2.6.3> : 0=user, 1=nice, 2=system, 3=idle, 4=iowait, 5=irq, 6=softirq, 7=steal, 8=guest, 9=guest_nice
					vector<long long> times;
					long long total_sum = 0;

					for (uint64_t val; cread >> val; total_sum += val) {
						times.push_back(val);
					}
					cread.clear();
					if (times.size() < 4) throw std::runtime_error("Malformed /proc/stat");

					//? Subtract fields 8-9 and any future unknown fields
					const long long totals = max(0ll, total_sum - (times.size() > 8 ? std::accumulate(times.begin() + 8, times.end(), 0ll) : 0));

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
						while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

						//? Populate cpu.cpu_percent with all fields from stat
						for (int ii = 0; const auto& val : times) {
							cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
							cpu_old.at(time_names.at(ii)) = val;

							//? Reduce size if there are more values than needed for graph
							while (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), width * 2)) cpu.cpu_percent.at(time_names.at(ii)).pop_front();

							if (++ii == 10) break;
						}
						continue;
					}
					//? Calculate cpu total for each core
					else {
						//? Fix container sizes if new cores are detected
						while (cmp_less(cpu.core_percent.size(), i)) {
							core_old_totals.push_back(0);
							core_old_idles.push_back(0);
							cpu.core_percent.emplace_back();
						}
						const long long calc_totals = max(0ll, totals - core_old_totals.at(i-1));
						const long long calc_idles = max(0ll, idles - core_old_idles.at(i-1));
						core_old_totals.at(i-1) = totals;
						core_old_idles.at(i-1) = idles;

						cpu.core_percent.at(i-1).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));
					}
				}

				//? Reduce size if there are more values than needed for graph
				if (cpu.core_percent.at(i-1).size() > 40) cpu.core_percent.at(i-1).pop_front();
			}

			//? Notify main thread to redraw screen if we found more cores than previously detected
			if (cmp_greater(cpu.core_percent.size(), Shared::coreCount)) {
				Logger::debug("Changing CPU max corecount from " + to_string(Shared::coreCount) + " to " + to_string(cpu.core_percent.size()) + ".");
				Runner::coreNum_reset = true;
				Shared::coreCount = cpu.core_percent.size();
				while (cmp_less(current_cpu.temp.size(), cpu.core_percent.size() + 1)) current_cpu.temp.push_back({0});
			}

		}
		catch (const std::exception& e) {
			Logger::debug("Cpu::collect() : " + string{e.what()});
			if (cread.bad()) throw std::runtime_error("Failed to read /proc/stat");
			else throw std::runtime_error("Cpu::collect() : " + string{e.what()});
		}

		if (Config::getB("check_temp") and got_sensors)
			update_sensors();

		if (Config::getB("show_battery") and has_battery)
			current_bat = get_battery();

		return cpu;
	}
}

#ifdef GPU_SUPPORT
namespace Gpu {
    //? NVIDIA
    namespace Nvml {
		bool init() {
			if (initialized) return false;

			//? Dynamic loading & linking
			//? Try possible library names for libnvidia-ml.so
			const array libNvAlts = {
				"libnvidia-ml.so",
				"libnvidia-ml.so.1",
			};

			for (const auto& l : libNvAlts) {
				nvml_dl_handle = dlopen(l, RTLD_LAZY);
				if (nvml_dl_handle != nullptr) {
					break;
				}
			}
 			if (!nvml_dl_handle) {
				Logger::info("Failed to load libnvidia-ml.so, NVIDIA GPUs will not be detected: "s + dlerror());
 				return false;
 			}

			auto load_nvml_sym = [&](const char sym_name[]) {
				auto sym = dlsym(nvml_dl_handle, sym_name);
				auto err = dlerror();
				if (err != nullptr) {
					Logger::error(string("NVML: Couldn't find function ") + sym_name + ": " + err);
					return (void*)nullptr;
				} else return sym;
			};

            #define LOAD_SYM(NAME)  if ((NAME = (decltype(NAME))load_nvml_sym(#NAME)) == nullptr) return false

		    LOAD_SYM(nvmlErrorString);
		    LOAD_SYM(nvmlInit);
		    LOAD_SYM(nvmlShutdown);
		    LOAD_SYM(nvmlDeviceGetCount);
		    LOAD_SYM(nvmlDeviceGetHandleByIndex);
		    LOAD_SYM(nvmlDeviceGetName);
		    LOAD_SYM(nvmlDeviceGetPowerManagementLimit);
		    LOAD_SYM(nvmlDeviceGetTemperatureThreshold);
		    LOAD_SYM(nvmlDeviceGetUtilizationRates);
		    LOAD_SYM(nvmlDeviceGetClockInfo);
		    LOAD_SYM(nvmlDeviceGetPowerUsage);
		    LOAD_SYM(nvmlDeviceGetPowerState);
		    LOAD_SYM(nvmlDeviceGetTemperature);
		    LOAD_SYM(nvmlDeviceGetMemoryInfo);
		    LOAD_SYM(nvmlDeviceGetPcieThroughput);

            #undef LOAD_SYM

			//? Function calls
			nvmlReturn_t result = nvmlInit();
    		if (result != NVML_SUCCESS) {
    			Logger::debug(std::string("Failed to initialize NVML, NVIDIA GPUs will not be detected: ") + nvmlErrorString(result));
    			return false;
    		}

			//? Device count
			result = nvmlDeviceGetCount(&device_count);
    		if (result != NVML_SUCCESS) {
    			Logger::warning(std::string("NVML: Failed to get device count: ") + nvmlErrorString(result));
    			return false;
    		}

			if (device_count > 0) {
				devices.resize(device_count);
				gpus.resize(device_count);
				gpu_names.resize(device_count);

				initialized = true;

				//? Check supported functions & get maximums
				Nvml::collect<1>(gpus.data());

				return true;
			} else {initialized = true; shutdown(); return false;}
		}

		bool shutdown() {
			if (!initialized) return false;
			nvmlReturn_t result = nvmlShutdown();
			if (NVML_SUCCESS == result) {
				initialized = false;
				dlclose(nvml_dl_handle);
			} else Logger::warning(std::string("Failed to shutdown NVML: ") + nvmlErrorString(result));

			return !initialized;
		}

		template <bool is_init> // collect<1> is called in Nvml::init(), and populates gpus.supported_functions
		bool collect(gpu_info* gpus_slice) { // raw pointer to vector data, size == device_count
			if (!initialized) return false;

			nvmlReturn_t result;
			std::thread pcie_tx_thread, pcie_rx_thread;
			// DebugTimer nvTotalTimer("Nvidia Total");
			for (unsigned int i = 0; i < device_count; ++i) {
				if constexpr(is_init) {
					//? Device Handle
    				result = nvmlDeviceGetHandleByIndex(i, devices.data() + i);
        			if (result != NVML_SUCCESS) {
    					Logger::warning(std::string("NVML: Failed to get device handle: ") + nvmlErrorString(result));
    					gpus[i].supported_functions = {false, false, false, false, false, false, false, false};
    					continue;
        			}

					//? Device name
					char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    				result = nvmlDeviceGetName(devices[i], name, NVML_DEVICE_NAME_BUFFER_SIZE);
        			if (result != NVML_SUCCESS)
    					Logger::warning(std::string("NVML: Failed to get device name: ") + nvmlErrorString(result));
        			else {
        				gpu_names[i] = string(name);
        				for (const auto& brand : {"NVIDIA", "Nvidia", "(R)", "(TM)"}) {
							gpu_names[i] = s_replace(gpu_names[i], brand, "");
						}
						gpu_names[i] = trim(gpu_names[i]);
        			}

    				//? Power usage
    				unsigned int max_power;
    				result = nvmlDeviceGetPowerManagementLimit(devices[i], &max_power);
    				if (result != NVML_SUCCESS)
						Logger::warning(std::string("NVML: Failed to get maximum GPU power draw, defaulting to 225W: ") + nvmlErrorString(result));
					else {
						gpus[i].pwr_max_usage = max_power; // RSMI reports power in microWatts
						gpu_pwr_total_max += max_power;
					}

					//? Get temp_max
					unsigned int temp_max;
    				result = nvmlDeviceGetTemperatureThreshold(devices[i], NVML_TEMPERATURE_THRESHOLD_SHUTDOWN, &temp_max);
        			if (result != NVML_SUCCESS)
    					Logger::warning(std::string("NVML: Failed to get maximum GPU temperature, defaulting to 110°C: ") + nvmlErrorString(result));
    				else gpus[i].temp_max = (long long)temp_max;
				}

				//? PCIe link speeds, the data collection takes >=20ms each call so they run on separate threads
				if (gpus_slice[i].supported_functions.pcie_txrx and (Config::getB("nvml_measure_pcie_speeds") or is_init)) {
					pcie_tx_thread = std::thread([gpus_slice, i]() {
						unsigned int tx;
						nvmlReturn_t result = nvmlDeviceGetPcieThroughput(devices[i], NVML_PCIE_UTIL_TX_BYTES, &tx);
    					if (result != NVML_SUCCESS) {
							Logger::warning(std::string("NVML: Failed to get PCIe TX throughput: ") + nvmlErrorString(result));
							if constexpr(is_init) gpus_slice[i].supported_functions.pcie_txrx = false;
						} else gpus_slice[i].pcie_tx = (long long)tx;
					});

					pcie_rx_thread = std::thread([gpus_slice, i]() {
						unsigned int rx;
						nvmlReturn_t result = nvmlDeviceGetPcieThroughput(devices[i], NVML_PCIE_UTIL_RX_BYTES, &rx);
    					if (result != NVML_SUCCESS) {
							Logger::warning(std::string("NVML: Failed to get PCIe RX throughput: ") + nvmlErrorString(result));
						} else gpus_slice[i].pcie_rx = (long long)rx;
					});
				}

				// DebugTimer nvTimer("Nv utilization");
				//? GPU & memory utilization
				if (gpus_slice[i].supported_functions.gpu_utilization) {
					nvmlUtilization_t utilization;
					result = nvmlDeviceGetUtilizationRates(devices[i], &utilization);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get GPU utilization: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.gpu_utilization = false;
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_utilization = false;
    				} else {
						gpus_slice[i].gpu_percent.at("gpu-totals").push_back((long long)utilization.gpu);
						gpus_slice[i].mem_utilization_percent.push_back((long long)utilization.memory);
    				}
				}

				// nvTimer.stop_rename_reset("Nv clock");
				//? Clock speeds
				if (gpus_slice[i].supported_functions.gpu_clock) {
					unsigned int gpu_clock;
					result = nvmlDeviceGetClockInfo(devices[i], NVML_CLOCK_GRAPHICS, &gpu_clock);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get GPU clock speed: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.gpu_clock = false;
					} else gpus_slice[i].gpu_clock_speed = (long long)gpu_clock;
				}

				if (gpus_slice[i].supported_functions.mem_clock) {
					unsigned int mem_clock;
					result = nvmlDeviceGetClockInfo(devices[i], NVML_CLOCK_MEM, &mem_clock);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get VRAM clock speed: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_clock = false;
					} else gpus_slice[i].mem_clock_speed = (long long)mem_clock;
				}

				// nvTimer.stop_rename_reset("Nv power");
    			//? Power usage & state
				if (gpus_slice[i].supported_functions.pwr_usage) {
    				unsigned int power;
    				result = nvmlDeviceGetPowerUsage(devices[i], &power);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get GPU power usage: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.pwr_usage = false;
    				} else {
    					gpus_slice[i].pwr_usage = (long long)power;
    					gpus_slice[i].gpu_percent.at("gpu-pwr-totals").push_back(clamp((long long)round((double)gpus_slice[i].pwr_usage * 100.0 / (double)gpus_slice[i].pwr_max_usage), 0ll, 100ll));
    				}
    			}

				if (gpus_slice[i].supported_functions.pwr_state) {
					nvmlPstates_t pState;
    				result = nvmlDeviceGetPowerState(devices[i], &pState);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get GPU power state: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.pwr_state = false;
    				} else gpus_slice[i].pwr_state = static_cast<int>(pState);
    			}

				// nvTimer.stop_rename_reset("Nv temp");
    			//? GPU temperature
				if (gpus_slice[i].supported_functions.temp_info) {
    				if (Config::getB("check_temp")) {
						unsigned int temp;
						nvmlReturn_t result = nvmlDeviceGetTemperature(devices[i], NVML_TEMPERATURE_GPU, &temp);
    					if (result != NVML_SUCCESS) {
							Logger::warning(std::string("NVML: Failed to get GPU temperature: ") + nvmlErrorString(result));
							if constexpr(is_init) gpus_slice[i].supported_functions.temp_info = false;
    					} else gpus_slice[i].temp.push_back((long long)temp);
					}
				}

				// nvTimer.stop_rename_reset("Nv mem");
				//? Memory info
				if (gpus_slice[i].supported_functions.mem_total) {
					nvmlMemory_t memory;
					result = nvmlDeviceGetMemoryInfo(devices[i], &memory);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get VRAM info: ") + nvmlErrorString(result));
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_total = false;
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_used = false;
					} else {
						gpus_slice[i].mem_total = memory.total;
						gpus_slice[i].mem_used = memory.used;
						//gpu.mem_free = memory.free;

						auto used_percent = (long long)round((double)memory.used * 100.0 / (double)memory.total);
						gpus_slice[i].gpu_percent.at("gpu-vram-totals").push_back(used_percent);
					}
				}

    			//? TODO: Processes using GPU
    				/*unsigned int proc_info_len;
    				nvmlProcessInfo_t* proc_info = 0;
    				result = nvmlDeviceGetComputeRunningProcesses_v3(device, &proc_info_len, proc_info);
    				if (result != NVML_SUCCESS) {
						Logger::warning(std::string("NVML: Failed to get compute processes: ") + nvmlErrorString(result));
    				} else {
    					for (unsigned int i = 0; i < proc_info_len; ++i)
    						gpus_slice[i].graphics_processes.push_back({proc_info[i].pid, proc_info[i].usedGpuMemory});
    				}*/

				// nvTimer.stop_rename_reset("Nv pcie thread join");
				//? Join PCIE TX/RX threads
				if constexpr(is_init) { // there doesn't seem to be a better way to do this, but this should be fine considering it's just 2 lines
					pcie_tx_thread.join();
					pcie_rx_thread.join();
				} else if (gpus_slice[i].supported_functions.pcie_txrx and Config::getB("nvml_measure_pcie_speeds")) {
					pcie_tx_thread.join();
					pcie_rx_thread.join();
				}
    		}

			return true;
		}
    }

	//? AMD
	namespace Rsmi {
		bool init() {
			if (initialized) return false;

			//? Dynamic loading & linking
		#if !defined(RSMI_STATIC)

			//? Try possible library paths and names for librocm_smi64.so
			const array libRocAlts = {
				"/opt/rocm/lib/librocm_smi64.so",
				"librocm_smi64.so",
				"librocm_smi64.so.5", // fedora
				"librocm_smi64.so.1.0", // debian
				"librocm_smi64.so.6"
			};

			for (const auto& l : libRocAlts) {
				rsmi_dl_handle = dlopen(l, RTLD_LAZY);
				if (rsmi_dl_handle != nullptr) {
					break;
				}
 			}

			if (!rsmi_dl_handle) {
				Logger::info("Failed to load librocm_smi64.so, AMD GPUs will not be detected: "s + dlerror());
				return false;
			}

			auto load_rsmi_sym = [&](const char sym_name[]) {
				auto sym = dlsym(rsmi_dl_handle, sym_name);
				auto err = dlerror();
				if (err != nullptr) {
					Logger::error(string("ROCm SMI: Couldn't find function ") + sym_name + ": " + err);
					return (void*)nullptr;
				} else return sym;
			};

            #define LOAD_SYM(NAME)  if ((NAME = (decltype(NAME))load_rsmi_sym(#NAME)) == nullptr) return false

		    LOAD_SYM(rsmi_init);
		    LOAD_SYM(rsmi_shut_down);
			LOAD_SYM(rsmi_version_get);
		    LOAD_SYM(rsmi_num_monitor_devices);
		    LOAD_SYM(rsmi_dev_name_get);
		    LOAD_SYM(rsmi_dev_power_cap_get);
		    LOAD_SYM(rsmi_dev_temp_metric_get);
		    LOAD_SYM(rsmi_dev_busy_percent_get);
		    LOAD_SYM(rsmi_dev_memory_busy_percent_get);
		    LOAD_SYM(rsmi_dev_power_ave_get);
		    LOAD_SYM(rsmi_dev_memory_total_get);
		    LOAD_SYM(rsmi_dev_memory_usage_get);
		    LOAD_SYM(rsmi_dev_pci_throughput_get);

            #undef LOAD_SYM
        #endif

			//? Function calls
			rsmi_status_t result = rsmi_init(0);
			if (result != RSMI_STATUS_SUCCESS) {
				Logger::debug("Failed to initialize ROCm SMI, AMD GPUs will not be detected");
				return false;
			}

		#if !defined(RSMI_STATIC)
			//? Check version
			rsmi_version_t version;
			result = rsmi_version_get(&version);
			if (result != RSMI_STATUS_SUCCESS) {
				Logger::warning("ROCm SMI: Failed to get version");
				return false;
			} else if (version.major == 5) {
				if ((rsmi_dev_gpu_clk_freq_get_v5 = (decltype(rsmi_dev_gpu_clk_freq_get_v5))load_rsmi_sym("rsmi_dev_gpu_clk_freq_get")) == nullptr)
					return false;
			// In the release tarballs of rocm 6.0.0 and 6.0.2 the version queried with rsmi_version_get is 7.0.0.0
			} else if (version.major == 6 || version.major == 7) {
				if ((rsmi_dev_gpu_clk_freq_get_v6 = (decltype(rsmi_dev_gpu_clk_freq_get_v6))load_rsmi_sym("rsmi_dev_gpu_clk_freq_get")) == nullptr)
					return false;
			} else {
				Logger::warning("ROCm SMI: Dynamic loading only supported for version 5 and 6");
				return false;
			}
			version_major = version.major;
		#endif

			//? Device count
			result = rsmi_num_monitor_devices(&device_count);
			if (result != RSMI_STATUS_SUCCESS) {
				Logger::warning("ROCm SMI: Failed to fetch number of devices");
				return false;
			}

			if (device_count > 0) {
				gpus.resize(gpus.size() + device_count);
				gpu_names.resize(gpus.size() + device_count);

				initialized = true;

				//? Check supported functions & get maximums
				Rsmi::collect<1>(gpus.data() + Nvml::device_count);

				return true;
			} else {initialized = true; shutdown(); return false;}
		}

		bool shutdown() {
			if (!initialized) return false;
    		if (rsmi_shut_down() == RSMI_STATUS_SUCCESS) {
				initialized = false;
			#if !defined(RSMI_STATIC)
				dlclose(rsmi_dl_handle);
			#endif
			} else Logger::warning("Failed to shutdown ROCm SMI");

			return true;
		}

		template <bool is_init>
		bool collect(gpu_info* gpus_slice) { // raw pointer to vector data, size == device_count, offset by Nvml::device_count elements
			if (!initialized) return false;
			rsmi_status_t result;

			for (uint32_t i = 0; i < device_count; ++i) {
				if constexpr(is_init) {
					//? Device name
					char name[RSMI_DEVICE_NAME_BUFFER_SIZE];
    				result = rsmi_dev_name_get(i, name, RSMI_DEVICE_NAME_BUFFER_SIZE);
        			if (result != RSMI_STATUS_SUCCESS)
    					Logger::warning("ROCm SMI: Failed to get device name");
        			else gpu_names[Nvml::device_count + i] = string(name);

    				//? Power usage
    				uint64_t max_power;
    				result = rsmi_dev_power_cap_get(i, 0, &max_power);
    				if (result != RSMI_STATUS_SUCCESS)
						Logger::warning("ROCm SMI: Failed to get maximum GPU power draw, defaulting to 225W");
					else {
						gpus_slice[i].pwr_max_usage = (long long)(max_power/1000); // RSMI reports power in microWatts
						gpu_pwr_total_max += gpus_slice[i].pwr_max_usage;
					}

					//? Get temp_max
					int64_t temp_max;
    				result = rsmi_dev_temp_metric_get(i, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_MAX, &temp_max);
        			if (result != RSMI_STATUS_SUCCESS)
    					Logger::warning("ROCm SMI: Failed to get maximum GPU temperature, defaulting to 110°C");
    				else gpus_slice[i].temp_max = (long long)temp_max;
    			}

				//? GPU utilization
				if (gpus_slice[i].supported_functions.gpu_utilization) {
					uint32_t utilization;
					result = rsmi_dev_busy_percent_get(i, &utilization);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get GPU utilization");
						if constexpr(is_init) gpus_slice[i].supported_functions.gpu_utilization = false;
    				} else gpus_slice[i].gpu_percent.at("gpu-totals").push_back((long long)utilization);
				}

				//? Memory utilization
				if (gpus_slice[i].supported_functions.mem_utilization) {
					uint32_t utilization;
					result = rsmi_dev_memory_busy_percent_get(i, &utilization);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get VRAM utilization");
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_utilization = false;
    				} else gpus_slice[i].mem_utilization_percent.push_back((long long)utilization);
				}
			#if !defined(RSMI_STATIC)
				//? Clock speeds
				if (gpus_slice[i].supported_functions.gpu_clock) {
					if (version_major == 5) {
						rsmi_frequencies_t_v5 frequencies;
						result = rsmi_dev_gpu_clk_freq_get_v5(i, RSMI_CLK_TYPE_SYS, &frequencies);
						if (result != RSMI_STATUS_SUCCESS) {
							Logger::warning("ROCm SMI: Failed to get GPU clock speed: ");
							if constexpr(is_init) gpus_slice[i].supported_functions.gpu_clock = false;
						} else gpus_slice[i].gpu_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
					}
					else if (version_major == 6 || version_major == 7) {
						rsmi_frequencies_t_v6 frequencies;
						result = rsmi_dev_gpu_clk_freq_get_v6(i, RSMI_CLK_TYPE_SYS, &frequencies);
						if (result != RSMI_STATUS_SUCCESS) {
							Logger::warning("ROCm SMI: Failed to get GPU clock speed: ");
							if constexpr(is_init) gpus_slice[i].supported_functions.gpu_clock = false;
						} else gpus_slice[i].gpu_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
					}
				}

				if (gpus_slice[i].supported_functions.mem_clock) {
					if (version_major == 5) {
						rsmi_frequencies_t_v5 frequencies;
						result = rsmi_dev_gpu_clk_freq_get_v5(i, RSMI_CLK_TYPE_MEM, &frequencies);
						if (result != RSMI_STATUS_SUCCESS) {
							Logger::warning("ROCm SMI: Failed to get VRAM clock speed: ");
							if constexpr(is_init) gpus_slice[i].supported_functions.mem_clock = false;
						} else gpus_slice[i].mem_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
					}
					else if (version_major == 6 || version_major == 7) {
						rsmi_frequencies_t_v6 frequencies;
						result = rsmi_dev_gpu_clk_freq_get_v6(i, RSMI_CLK_TYPE_MEM, &frequencies);
						if (result != RSMI_STATUS_SUCCESS) {
							Logger::warning("ROCm SMI: Failed to get VRAM clock speed: ");
							if constexpr(is_init) gpus_slice[i].supported_functions.mem_clock = false;
						} else gpus_slice[i].mem_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
					}
				}
			#else
				//? Clock speeds
				if (gpus_slice[i].supported_functions.gpu_clock) {
					rsmi_frequencies_t frequencies;
					result = rsmi_dev_gpu_clk_freq_get(i, RSMI_CLK_TYPE_SYS, &frequencies);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get GPU clock speed: ");
						if constexpr(is_init) gpus_slice[i].supported_functions.gpu_clock = false;
    				} else gpus_slice[i].gpu_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
				}

				if (gpus_slice[i].supported_functions.mem_clock) {
					rsmi_frequencies_t frequencies;
					result = rsmi_dev_gpu_clk_freq_get(i, RSMI_CLK_TYPE_MEM, &frequencies);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get VRAM clock speed: ");
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_clock = false;
    				} else gpus_slice[i].mem_clock_speed = (long long)frequencies.frequency[frequencies.current]/1000000; // Hz to MHz
				}
			#endif

    			//? Power usage & state
				if (gpus_slice[i].supported_functions.pwr_usage) {
    				uint64_t power;
    				result = rsmi_dev_power_ave_get(i, 0, &power);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get GPU power usage");
						if constexpr(is_init) gpus_slice[i].supported_functions.pwr_usage = false;
    				} else {
							gpus_slice[i].pwr_usage = (long long)power / 1000;
							gpus_slice[i].gpu_percent.at("gpu-pwr-totals").push_back(clamp((long long)round((double)gpus_slice[i].pwr_usage * 100.0 / (double)gpus_slice[i].pwr_max_usage), 0ll, 100ll));
						}

					if constexpr(is_init) gpus_slice[i].supported_functions.pwr_state = false;
				}

    			//? GPU temperature
				if (gpus_slice[i].supported_functions.temp_info) {
    				if (Config::getB("check_temp") or is_init) {
						int64_t temp;
    					result = rsmi_dev_temp_metric_get(i, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp);
        				if (result != RSMI_STATUS_SUCCESS) {
    						Logger::warning("ROCm SMI: Failed to get GPU temperature");
							if constexpr(is_init) gpus_slice[i].supported_functions.temp_info = false;
    					} else gpus_slice[i].temp.push_back((long long)temp/1000);
    				}
				}

				//? Memory info
				if (gpus_slice[i].supported_functions.mem_total) {
					uint64_t total;
					result = rsmi_dev_memory_total_get(i, RSMI_MEM_TYPE_VRAM, &total);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get total VRAM");
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_total = false;
					} else gpus_slice[i].mem_total = total;
				}

				if (gpus_slice[i].supported_functions.mem_used) {
					uint64_t used;
					result = rsmi_dev_memory_usage_get(i, RSMI_MEM_TYPE_VRAM, &used);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get VRAM usage");
						if constexpr(is_init) gpus_slice[i].supported_functions.mem_used = false;
					} else {
						gpus_slice[i].mem_used = used;
						if (gpus_slice[i].supported_functions.mem_total)
							gpus_slice[i].gpu_percent.at("gpu-vram-totals").push_back((long long)round((double)used * 100.0 / (double)gpus_slice[i].mem_total));
					}
				}

				//? PCIe link speeds
				if (gpus_slice[i].supported_functions.pcie_txrx) {
					uint64_t tx, rx;
					result = rsmi_dev_pci_throughput_get(i, &tx, &rx, 0);
    				if (result != RSMI_STATUS_SUCCESS) {
						Logger::warning("ROCm SMI: Failed to get PCIe throughput");
						if constexpr(is_init) gpus_slice[i].supported_functions.pcie_txrx = false;
					} else {
						gpus_slice[i].pcie_tx = (long long)tx;
						gpus_slice[i].pcie_rx = (long long)rx;
					}
				}
    		}

			return true;
		}
	}

	// TODO: Intel

	//? Collect data from GPU-specific libraries
	auto collect(bool no_update) -> vector<gpu_info>& {
		if (Runner::stopping or (no_update and not gpus.empty())) return gpus;

		// DebugTimer gpu_timer("GPU Total");

		//* Collect data
		Nvml::collect<0>(gpus.data()); // raw pointer to vector data, size == Nvml::device_count
		Rsmi::collect<0>(gpus.data() + Nvml::device_count); // size = Rsmi::device_count

		//* Calculate average usage
		long long avg = 0;
		long long mem_usage_total = 0;
		long long mem_total = 0;
		long long pwr_total = 0;
		for (auto& gpu : gpus) {
			if (gpu.supported_functions.gpu_utilization)
				avg += gpu.gpu_percent.at("gpu-totals").back();
			if (gpu.supported_functions.mem_used)
				mem_usage_total += gpu.mem_used;
			if (gpu.supported_functions.mem_total)
				mem_total += gpu.mem_total;
			if (gpu.supported_functions.pwr_usage)
				mem_total += gpu.pwr_usage;

			//* Trim vectors if there are more values than needed for graphs
			if (width != 0) {
				//? GPU & memory utilization
				while (cmp_greater(gpu.gpu_percent.at("gpu-totals").size(), width * 2)) gpu.gpu_percent.at("gpu-totals").pop_front();
				while (cmp_greater(gpu.mem_utilization_percent.size(), width)) gpu.mem_utilization_percent.pop_front();
				//? Power usage
				while (cmp_greater(gpu.gpu_percent.at("gpu-pwr-totals").size(), width)) gpu.gpu_percent.at("gpu-pwr-totals").pop_front();
				//? Temperature
				while (cmp_greater(gpu.temp.size(), 18)) gpu.temp.pop_front();
				//? Memory usage
				while (cmp_greater(gpu.gpu_percent.at("gpu-vram-totals").size(), width/2)) gpu.gpu_percent.at("gpu-vram-totals").pop_front();
			}
		}

		shared_gpu_percent.at("gpu-average").push_back(avg / gpus.size());
		if (mem_total != 0)
			shared_gpu_percent.at("gpu-vram-total").push_back(mem_usage_total / mem_total);
		if (gpu_pwr_total_max != 0)
			shared_gpu_percent.at("gpu-pwr-total").push_back(pwr_total / gpu_pwr_total_max);

		if (width != 0) {
			while (cmp_greater(shared_gpu_percent.at("gpu-average").size(), width * 2)) shared_gpu_percent.at("gpu-average").pop_front();
			while (cmp_greater(shared_gpu_percent.at("gpu-pwr-total").size(), width * 2)) shared_gpu_percent.at("gpu-pwr-total").pop_front();
			while (cmp_greater(shared_gpu_percent.at("gpu-vram-total").size(), width * 2)) shared_gpu_percent.at("gpu-vram-total").pop_front();
		}

		return gpus;
	}
}
#endif

namespace Mem {
	bool has_swap{};
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios{};
	vector<string> last_found;

	//?* Find the filepath to the specified ZFS object's stat file
	fs::path get_zfs_stat_file(const string& device_name, size_t dataset_name_start, bool zfs_hide_datasets);

	//?* Collect total ZFS pool io stats
	bool zfs_collect_pool_total_stats(struct disk_info &disk);

	mem_info current_mem {};

	uint64_t get_totalMem() {
		ifstream meminfo(Shared::procPath / "meminfo");
		int64_t totalMem;
		if (meminfo.good()) {
			meminfo.ignore(SSmax, ':');
			meminfo >> totalMem;
			totalMem <<= 10;
		}
		if (not meminfo.good() or totalMem == 0)
			throw std::runtime_error("Could not get total memory size from /proc/meminfo");

		return totalMem;
	}

	auto collect(bool no_update) -> mem_info& {
		if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty())) return current_mem;
		auto show_swap = Config::getB("show_swap");
		auto swap_disk = Config::getB("swap_disk");
		auto show_disks = Config::getB("show_disks");
		auto zfs_arc_cached = Config::getB("zfs_arc_cached");
		auto totalMem = get_totalMem();
		auto& mem = current_mem;

		mem.stats.at("swap_total") = 0;

		//? Read ZFS ARC info from /proc/spl/kstat/zfs/arcstats
		uint64_t arc_size = 0, arc_min_size = 0;
		if (zfs_arc_cached) {
			ifstream arcstats(Shared::procPath / "spl/kstat/zfs/arcstats");
			if (arcstats.good()) {
				for (string label; arcstats >> label;) {
					if (label == "c_min") {
						arcstats >> arc_min_size >> arc_min_size; // double read skips type column
					}
					else if (label == "size") {
						arcstats >> arc_size >> arc_size;
						break;
					}
				}
			}
			arcstats.close();
		}

		//? Read memory info from /proc/meminfo
		ifstream meminfo(Shared::procPath / "meminfo");
		if (meminfo.good()) {
			bool got_avail = false;
			for (string label; meminfo.peek() != 'D' and meminfo >> label;) {
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
			if (zfs_arc_cached) {
				mem.stats.at("cached") += arc_size;
				// The ARC will not shrink below arc_min_size, so that memory is not available
				if (arc_size > arc_min_size)
					mem.stats.at("available") += arc_size - arc_min_size;
			}
			mem.stats.at("used") = totalMem - (mem.stats.at("available") <= totalMem ? mem.stats.at("available") : mem.stats.at("free"));

			if (mem.stats.at("swap_total") > 0) mem.stats.at("swap_used") = mem.stats.at("swap_total") - mem.stats.at("swap_free");
		}
		else
			throw std::runtime_error("Failed to read /proc/meminfo");

		meminfo.close();

		//? Calculate percentages
		for (const auto& name : mem_names) {
			mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / totalMem));
			while (cmp_greater(mem.percent.at(name).size(), width * 2)) mem.percent.at(name).pop_front();
		}

		if (show_swap and mem.stats.at("swap_total") > 0) {
			for (const auto& name : swap_names) {
				mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
				while (cmp_greater(mem.percent.at(name).size(), width * 2)) mem.percent.at(name).pop_front();
			}
			has_swap = true;
		}
		else
			has_swap = false;

		//? Get disks stats
		if (show_disks) {
			static vector<string> ignore_list;
			double uptime = system_uptime();
			auto free_priv = Config::getB("disk_free_priv");
			try {
				auto& disks_filter = Config::getS("disks_filter");
				bool filter_exclude = false;
				auto use_fstab = Config::getB("use_fstab");
				auto only_physical = Config::getB("only_physical");
				auto zfs_hide_datasets = Config::getB("zfs_hide_datasets");
				auto& disks = mem.disks;
				static std::unordered_map<string, future<pair<disk_info, int>>> disks_stats_promises;
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
								#ifdef SNAPPED
									if (instr == "/") fstab.push_back("/mnt");
									else if (not is_in(instr, "none", "swap")) fstab.push_back(instr);
								#else
									if (not is_in(instr, "none", "swap")) fstab.push_back(instr);
								#endif
							}
							diskread.ignore(SSmax, '\n');
						}
					}
					else
						throw std::runtime_error("Failed to read /etc/fstab");
					diskread.close();
				}

				//? Get mounts from /etc/mtab or /proc/self/mounts
				diskread.open((fs::exists("/etc/mtab") ? fs::path("/etc/mtab") : Shared::procPath / "self/mounts"));
				if (diskread.good()) {
					vector<string> found;
					found.reserve(last_found.size());
					string dev, mountpoint, fstype;
					while (not diskread.eof()) {
						std::error_code ec;
						diskread >> dev >> mountpoint >> fstype;
						diskread.ignore(SSmax, '\n');

						if (v_contains(ignore_list, mountpoint) or v_contains(found, mountpoint)) continue;

						//? Match filter if not empty
						if (not filter.empty()) {
							bool match = v_contains(filter, mountpoint);
							if ((filter_exclude and match) or (not filter_exclude and not match))
								continue;
						}

						//? Skip ZFS datasets if zfs_hide_datasets option is enabled
						size_t zfs_dataset_name_start = 0;
						if (fstype == "zfs" && (zfs_dataset_name_start = dev.find('/')) != std::string::npos && zfs_hide_datasets) continue;

						if ((not use_fstab and not only_physical)
						or (use_fstab and v_contains(fstab, mountpoint))
						or (not use_fstab and only_physical and v_contains(fstypes, fstype))) {
							found.push_back(mountpoint);
							if (not v_contains(last_found, mountpoint)) redraw = true;

							//? Save mountpoint, name, fstype, dev path and path to /sys/block stat file
							if (not disks.contains(mountpoint)) {
								disks[mountpoint] = disk_info{fs::canonical(dev, ec), fs::path(mountpoint).filename(), fstype};
								if (disks.at(mountpoint).dev.empty()) disks.at(mountpoint).dev = dev;
								#ifdef SNAPPED
									if (mountpoint == "/mnt") disks.at(mountpoint).name = "root";
								#endif
								if (disks.at(mountpoint).name.empty()) disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
								string devname = disks.at(mountpoint).dev.filename();
								int c = 0;
								while (devname.size() >= 2) {
									if (fs::exists("/sys/block/" + devname + "/stat", ec) and access(string("/sys/block/" + devname + "/stat").c_str(), R_OK) == 0) {
										if (c > 0 and fs::exists("/sys/block/" + devname + '/' + disks.at(mountpoint).dev.filename().string() + "/stat", ec))
											disks.at(mountpoint).stat = "/sys/block/" + devname + '/' + disks.at(mountpoint).dev.filename().string() + "/stat";
										else
											disks.at(mountpoint).stat = "/sys/block/" + devname + "/stat";
										break;
									//? Set ZFS stat filepath
									} else if (fstype == "zfs") {
										disks.at(mountpoint).stat = get_zfs_stat_file(dev, zfs_dataset_name_start, zfs_hide_datasets);
										if (disks.at(mountpoint).stat.empty()) {
											Logger::debug("Failed to get ZFS stat file for device " + dev);
										}
										break;
									}
									devname.resize(devname.size() - 1);
									c++;
								}
							}

							//? If zfs_hide_datasets option was switched, refresh stat filepath
							if (fstype == "zfs" && ((zfs_hide_datasets && !is_directory(disks.at(mountpoint).stat))
								|| (!zfs_hide_datasets && is_directory(disks.at(mountpoint).stat)))) {
								disks.at(mountpoint).stat = get_zfs_stat_file(dev, zfs_dataset_name_start, zfs_hide_datasets);
								if (disks.at(mountpoint).stat.empty()) {
									Logger::debug("Failed to get ZFS stat file for device " + dev);
								}
							}
						}
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
				}
				else
					throw std::runtime_error("Failed to get mounts from /etc/mtab and /proc/self/mounts");
				diskread.close();

				//? Get disk/partition stats
				for (auto it = disks.begin(); it != disks.end(); ) {
					auto &[mountpoint, disk] = *it;
					if (v_contains(ignore_list, mountpoint) or disk.name == "swap") {
						it = disks.erase(it);
						continue;
					}
					if(auto promises_it = disks_stats_promises.find(mountpoint); promises_it != disks_stats_promises.end()){
						auto& promise = promises_it->second;
						if(promise.valid() &&
						   promise.wait_for(0s) == std::future_status::timeout) {
							++it;
							continue;
						}
						auto promise_res = promises_it->second.get();
						if(promise_res.second != -1){
							ignore_list.push_back(mountpoint);
							Logger::warning("Failed to get disk/partition stats for mount \""+ mountpoint + "\" with statvfs error code: " + to_string(promise_res.second) + ". Ignoring...");
							it = disks.erase(it);
							continue;
						}
						auto &updated_stats = promise_res.first;
						disk.total = updated_stats.total;
						disk.free = updated_stats.free;
						disk.used = updated_stats.used;
						disk.used_percent = updated_stats.used_percent;
						disk.free_percent = updated_stats.free_percent;
					}
					disks_stats_promises[mountpoint] = async(std::launch::async, [mountpoint, &free_priv]() -> pair<disk_info, int> {
						struct statvfs vfs;
						disk_info disk;
						if (statvfs(mountpoint.c_str(), &vfs) < 0) {
							return pair{disk, errno};
						}
						disk.total = vfs.f_blocks * vfs.f_frsize;
						disk.free = (free_priv ? vfs.f_bfree : vfs.f_bavail) * vfs.f_frsize;
						disk.used = disk.total - disk.free;
						disk.used_percent = round((double)disk.used * 100 / disk.total);
						disk.free_percent = 100 - disk.used_percent;
						return pair{disk, -1};
					});
					++it;
				}

				//? Setup disks order in UI and add swap if enabled
				mem.disks_order.clear();
				#ifdef SNAPPED
					if (disks.contains("/mnt")) mem.disks_order.push_back("/mnt");
				#else
					if (disks.contains("/")) mem.disks_order.push_back("/");
				#endif
				if (swap_disk and has_swap) {
					mem.disks_order.push_back("swap");
					if (not disks.contains("swap")) disks["swap"] = {"", "swap", "swap"};
					disks.at("swap").total = mem.stats.at("swap_total");
					disks.at("swap").used = mem.stats.at("swap_used");
					disks.at("swap").free = mem.stats.at("swap_free");
					disks.at("swap").used_percent = mem.percent.at("swap_used").back();
					disks.at("swap").free_percent = mem.percent.at("swap_free").back();
				}
				for (const auto& name : last_found)
					#ifdef SNAPPED
						if (not is_in(name, "/mnt", "swap")) mem.disks_order.push_back(name);
					#else
						if (not is_in(name, "/", "swap")) mem.disks_order.push_back(name);
					#endif

				//? Get disks IO
				int64_t sectors_read, sectors_write, io_ticks, io_ticks_temp;
				disk_ios = 0;
				for (auto& [ignored, disk] : disks) {
					if (disk.stat.empty() or access(disk.stat.c_str(), R_OK) != 0) continue;
					if (disk.fstype == "zfs" && zfs_hide_datasets && zfs_collect_pool_total_stats(disk)) {
						disk_ios++;
						continue;
					}
					diskread.open(disk.stat);
					if (diskread.good()) {
						disk_ios++;
						//? ZFS Pool Support
						if (disk.fstype == "zfs") {
							// skip first three lines
							for (int i = 0; i < 3; i++) diskread.ignore(numeric_limits<streamsize>::max(), '\n');
							// skip characters until '4' is reached, indicating data type 4, next value will be out target
							diskread.ignore(numeric_limits<streamsize>::max(), '4');
							diskread >> io_ticks;

							// skip characters until '4' is reached, indicating data type 4, next value will be out target
							diskread.ignore(numeric_limits<streamsize>::max(), '4');
							diskread >> sectors_write; // nbytes written
							if (disk.io_write.empty())
								disk.io_write.push_back(0);
							else
								disk.io_write.push_back(max((int64_t)0, (sectors_write - disk.old_io.at(1))));
							disk.old_io.at(1) = sectors_write;
							while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

							// skip characters until '4' is reached, indicating data type 4, next value will be out target
							diskread.ignore(numeric_limits<streamsize>::max(), '4');
							diskread >> io_ticks_temp;
							io_ticks += io_ticks_temp;

							// skip characters until '4' is reached, indicating data type 4, next value will be out target
							diskread.ignore(numeric_limits<streamsize>::max(), '4');
							diskread >> sectors_read; // nbytes read
							if (disk.io_read.empty())
								disk.io_read.push_back(0);
							else
								disk.io_read.push_back(max((int64_t)0, (sectors_read - disk.old_io.at(0))));
							disk.old_io.at(0) = sectors_read;
							while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

							if (disk.io_activity.empty())
								disk.io_activity.push_back(0);
							else
								disk.io_activity.push_back(max((int64_t)0, (io_ticks - disk.old_io.at(2))));
							disk.old_io.at(2) = io_ticks;
							while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
						} else {
							for (int i = 0; i < 2; i++) { diskread >> std::ws; diskread.ignore(SSmax, ' '); }
							diskread >> sectors_read;
							if (disk.io_read.empty())
								disk.io_read.push_back(0);
							else
								disk.io_read.push_back(max((int64_t)0, (sectors_read - disk.old_io.at(0)) * 512));
							disk.old_io.at(0) = sectors_read;
							while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

							for (int i = 0; i < 3; i++) { diskread >> std::ws; diskread.ignore(SSmax, ' '); }
							diskread >> sectors_write;
							if (disk.io_write.empty())
								disk.io_write.push_back(0);
							else
								disk.io_write.push_back(max((int64_t)0, (sectors_write - disk.old_io.at(1)) * 512));
							disk.old_io.at(1) = sectors_write;
							while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

							for (int i = 0; i < 2; i++) { diskread >> std::ws; diskread.ignore(SSmax, ' '); }
							diskread >> io_ticks;
							if (disk.io_activity.empty())
								disk.io_activity.push_back(0);
							else
								disk.io_activity.push_back(clamp((long)round((double)(io_ticks - disk.old_io.at(2)) / (uptime - old_uptime) / 10), 0l, 100l));
							disk.old_io.at(2) = io_ticks;
							while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
						}
					} else {
						Logger::debug("Error in Mem::collect() : when opening " + string{disk.stat});
					}
					diskread.close();
				}
				old_uptime = uptime;
			}
			catch (const std::exception& e) {
				Logger::warning("Error in Mem::collect() : " + string{e.what()});
			}
		}

		return mem;
	}

	fs::path get_zfs_stat_file(const string& device_name, size_t dataset_name_start, bool zfs_hide_datasets) {
		fs::path zfs_pool_stat_path;
		if (zfs_hide_datasets) {
			zfs_pool_stat_path = Shared::procPath / "spl/kstat/zfs" / device_name;
			if (access(zfs_pool_stat_path.c_str(), R_OK) == 0) {
				return zfs_pool_stat_path;
			} else {
				Logger::debug("Can't access folder: " + zfs_pool_stat_path.string());
				return "";
			}
		}

		ifstream filestream;
		string filename;
		string name_compare;

		if (dataset_name_start != std::string::npos) { // device is a dataset
			zfs_pool_stat_path = Shared::procPath / "spl/kstat/zfs" / device_name.substr(0, dataset_name_start);
		} else { // device is a pool
			zfs_pool_stat_path = Shared::procPath / "spl/kstat/zfs" / device_name;
		}

		// looking through all files that start with 'objset' to find the one containing `device_name` object stats
		try {
			for (const auto& file: fs::directory_iterator(zfs_pool_stat_path)) {
				filename = file.path().filename();
				if (filename.starts_with("objset")) {
					filestream.open(file.path());
					if (filestream.good()) {
						// skip first two lines
						for (int i = 0; i < 2; i++) filestream.ignore(numeric_limits<streamsize>::max(), '\n');
						// skip characters until '7' is reached, indicating data type 7, next value will be object name
						filestream.ignore(numeric_limits<streamsize>::max(), '7');
						filestream >> name_compare;
						if (name_compare == device_name) {
							filestream.close();
							if (access(file.path().c_str(), R_OK) == 0) {
								return file.path();
							} else {
								Logger::debug("Can't access file: " + file.path().string());
								return "";
							}
						}
					}
					filestream.close();
				}
			}
		}
		catch (fs::filesystem_error& e) {}

		Logger::debug("Could not read directory: " + zfs_pool_stat_path.string());
		return "";
	}

	bool zfs_collect_pool_total_stats(struct disk_info &disk) {
		ifstream diskread;

		int64_t bytes_read;
		int64_t bytes_write;
		int64_t io_ticks;
		int64_t bytes_read_total{};
		int64_t bytes_write_total{};
		int64_t io_ticks_total{};
		int64_t objects_read{};

		// looking through all files that start with 'objset'
		for (const auto& file: fs::directory_iterator(disk.stat)) {
			if ((file.path().filename()).string().starts_with("objset")) {
				diskread.open(file.path());
				if (diskread.good()) {
					try {
						// skip first three lines
						for (int i = 0; i < 3; i++) diskread.ignore(numeric_limits<streamsize>::max(), '\n');
						// skip characters until '4' is reached, indicating data type 4, next value will be out target
						diskread.ignore(numeric_limits<streamsize>::max(), '4');
						diskread >> io_ticks;
						io_ticks_total += io_ticks;

						// skip characters until '4' is reached, indicating data type 4, next value will be out target
						diskread.ignore(numeric_limits<streamsize>::max(), '4');
						diskread >> bytes_write;
						bytes_write_total += bytes_write;

						// skip characters until '4' is reached, indicating data type 4, next value will be out target
						diskread.ignore(numeric_limits<streamsize>::max(), '4');
						diskread >> io_ticks;
						io_ticks_total += io_ticks;

						// skip characters until '4' is reached, indicating data type 4, next value will be out target
						diskread.ignore(numeric_limits<streamsize>::max(), '4');
						diskread >> bytes_read;
						bytes_read_total += bytes_read;
					} catch (const std::exception& e) {
						continue;
					}

					// increment read objects counter if no errors were encountered
					objects_read++;
				} else {
					Logger::debug("Could not read file: " + file.path().string());
				}
				diskread.close();
			}
		}

		// if for some reason no objects were read
		if (objects_read == 0) return false;

		if (disk.io_write.empty())
			disk.io_write.push_back(0);
		else
			disk.io_write.push_back(max((int64_t)0, (bytes_write_total - disk.old_io.at(1))));
		disk.old_io.at(1) = bytes_write_total;
		while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

		if (disk.io_read.empty())
			disk.io_read.push_back(0);
		else
			disk.io_read.push_back(max((int64_t)0, (bytes_read_total - disk.old_io.at(0))));
		disk.old_io.at(0) = bytes_read_total;
		while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

		if (disk.io_activity.empty())
			disk.io_activity.push_back(0);
		else
			disk.io_activity.push_back(max((int64_t)0, (io_ticks_total - disk.old_io.at(2))));
		disk.old_io.at(2) = io_ticks_total;
		while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();

		return true;
	}

}

namespace Net {
	std::unordered_map<string, net_info> current_net;
	net_info empty_net = {};
	vector<string> interfaces;
	string selected_iface;
	int errors{};
	std::unordered_map<string, uint64_t> graph_max = { {"download", {}}, {"upload", {}} };
	std::unordered_map<string, array<int, 2>> max_count = { {"download", {}}, {"upload", {}} };
	bool rescale{true};
	uint64_t timestamp{};

	auto collect(bool no_update) -> net_info& {
		if (Runner::stopping) return empty_net;
		auto& net = current_net;
		auto& config_iface = Config::getS("net_iface");
		auto net_sync = Config::getB("net_sync");
		auto net_auto = Config::getB("net_auto");
		auto new_timestamp = time_ms();

		if (not no_update and errors < 3) {
			//? Get interface list using getifaddrs() wrapper
			IfAddrsPtr if_addrs {};
			if (if_addrs.get_status() != 0) {
				errors++;
				Logger::error("Net::collect() -> getifaddrs() failed with id " + to_string(if_addrs.get_status()));
				redraw = true;
				return empty_net;
			}
			int family = 0;
			static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN); // 46 >= 16, compile-time assurance.
			enum { IPBUFFER_MAXSIZE = INET6_ADDRSTRLEN }; // manually using the known biggest value, guarded by the above static_assert
			char ip[IPBUFFER_MAXSIZE];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all items in getifaddrs() list
			for (auto* ifa = if_addrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == nullptr) continue;
				family = ifa->ifa_addr->sa_family;
				const auto& iface = ifa->ifa_name;

				//? Update available interfaces vector and get status of interface
				if (not v_contains(interfaces, iface)) {
					interfaces.push_back(iface);
					net[iface].connected = (ifa->ifa_flags & IFF_RUNNING);

					// An interface can have more than one IP of the same family associated with it,
					// but we pick only the first one to show in the NET box.
					// Note: Interfaces without any IPv4 and IPv6 set are still valid and monitorable!
					net[iface].ipv4.clear();
					net[iface].ipv6.clear();
				}


				//? Get IPv4 address
				if (family == AF_INET) {
					if (net[iface].ipv4.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv4 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv4 to string for iface " + string(iface) + ", errno: " + strerror(errsv));
						}
					}
				}
				//? Get IPv6 address
				else if (family == AF_INET6) {
					if (net[iface].ipv6.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv6 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv6 to string for iface " + string(iface) + ", errno: " + strerror(errsv));
						}
					}
				} //else, ignoring family==AF_PACKET (see man 3 getifaddrs) which is the first one in the `for` loop.
			}

			//? Get total received and transmitted bytes + device address if no ip was found
			for (const auto& iface : interfaces) {
				if (net.at(iface).ipv4.empty() and net.at(iface).ipv6.empty())
					net.at(iface).ipv4 = readfile("/sys/class/net/" + iface + "/address");

				for (const string dir : {"download", "upload"}) {
					const fs::path sys_file = "/sys/class/net/" + iface + "/statistics/" + (dir == "download" ? "rx_bytes" : "tx_bytes");
					auto& saved_stat = net.at(iface).stat.at(dir);
					auto& bandwidth = net.at(iface).bandwidth.at(dir);

					uint64_t val{};
					try { val = (uint64_t)stoull(readfile(sys_file, "0")); }
					catch (const std::invalid_argument&) {}
					catch (const std::out_of_range&) {}

					//? Update speed, total and top values
					if (val < saved_stat.last) {
						saved_stat.rollover += saved_stat.last;
						saved_stat.last = 0;
					}
					if (cmp_greater((unsigned long long)saved_stat.rollover + (unsigned long long)val, numeric_limits<uint64_t>::max())) {
						saved_stat.rollover = 0;
						saved_stat.last = 0;
					}
					saved_stat.speed = round((double)(val - saved_stat.last) / ((double)(new_timestamp - timestamp) / 1000));
					if (saved_stat.speed > saved_stat.top) saved_stat.top = saved_stat.speed;
					if (saved_stat.offset > val + saved_stat.rollover) saved_stat.offset = 0;
					saved_stat.total = (val + saved_stat.rollover) - saved_stat.offset;
					saved_stat.last = val;

					//? Add values to graph
					bandwidth.push_back(saved_stat.speed);
					while (cmp_greater(bandwidth.size(), width * 2)) bandwidth.pop_front();

					//? Set counters for auto scaling
					if (net_auto and selected_iface == iface) {
						if (net_sync and saved_stat.speed < net.at(iface).stat.at(dir == "download" ? "upload" : "download").speed) continue;
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
				if (selected_iface.empty() and not sorted_interfaces.empty()) selected_iface = sorted_interfaces.at(0);
				else if (sorted_interfaces.empty()) return empty_net;

			}
		}

		//? Calculate max scale for graphs if needed
		if (net_auto) {
			bool sync = false;
			for (const auto& dir: {"download", "upload"}) {
				for (const auto& sel : {0, 1}) {
					if (rescale or max_count[dir][sel] >= 5) {
						const long long avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
							? std::accumulate(net.at(selected_iface).bandwidth.at(dir).rbegin(), net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0ll) / 5
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
}

namespace Proc {

	vector<proc_info> current_procs;
	std::unordered_map<string, string> uid_user;
	string current_sort;
	string current_filter;
	bool current_rev{};

	fs::file_time_type passwd_time;

	uint64_t cputimes;
	int collapse = -1, expand = -1;
	uint64_t old_cputimes{};
	atomic<int> numpids{};
	int filter_found{};

	detail_container detailed;
	constexpr size_t KTHREADD = 2;
	static std::unordered_set<size_t> kernels_procs = {KTHREADD};

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
			detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Mem::get_totalMem());
			redraw = true;
		}

		while (cmp_greater(detailed.mem_bytes.size(), width)) detailed.mem_bytes.pop_front();

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
	auto collect(bool no_update) -> vector<proc_info>& {
		if (Runner::stopping) return current_procs;
		const auto& sorting = Config::getS("proc_sorting");
		auto reverse = Config::getB("proc_reversed");
		const auto& filter = Config::getS("proc_filter");
		auto per_core = Config::getB("proc_per_core");
		auto should_filter_kernel = Config::getB("proc_filter_kernel");
		auto tree = Config::getB("proc_tree");
		auto show_detailed = Config::getB("show_detailed");
		const size_t detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}
		ifstream pread;
		string long_string;
		string short_str;

		static vector<size_t> found;

		const double uptime = system_uptime();

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		static size_t proc_clear_count{};

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, round(uptime), current_procs);
		}
		//* ---------------------------------------------Collection start----------------------------------------------
		else {
			should_filter = true;
			found.clear();

			//? First make sure kernel proc cache is cleared.
			if (should_filter_kernel and ++proc_clear_count >= 256) {
				//? Clearing the cache is used in the event of a pid wrap around.
				//? In that event processes that acquire old kernel pids would also be filtered out so we need to manually clean the cache every now and then.
				kernels_procs.clear();
				kernels_procs.emplace(KTHREADD);
				proc_clear_count = 0;
			}

			auto totalMem = Mem::get_totalMem();
			int totalMem_len = to_string(totalMem >> 10).size();

			//? Update uid_user map if /etc/passwd changed since last run
			if (not Shared::passwd_path.empty() and fs::last_write_time(Shared::passwd_path) != passwd_time) {
				string r_uid, r_user;
				passwd_time = fs::last_write_time(Shared::passwd_path);
				uid_user.clear();
				pread.open(Shared::passwd_path);
				if (pread.good()) {
					while (pread.good()) {
						getline(pread, r_user, ':');
						pread.ignore(SSmax, ':');
						getline(pread, r_uid, ':');
						if (uid_user.contains(r_uid)) break;
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
					return current_procs;

				if (pread.is_open()) pread.close();

				const string pid_str = d.path().filename();
				if (not isdigit(pid_str[0])) continue;

				const size_t pid = stoul(pid_str);

				if (should_filter_kernel and kernels_procs.contains(pid)) {
					continue;
				}

				found.push_back(pid);

				//? Check if pid already exists in current_procs
				auto find_old = rng::find(current_procs, pid, &proc_info::pid);
				bool no_cache{};
				if (find_old == current_procs.end()) {
					current_procs.push_back({pid});
					find_old = current_procs.end() - 1;
					no_cache = true;
				}

				auto& new_proc = *find_old;

				//? Get program name, command and username
				if (no_cache) {
					pread.open(d.path() / "comm");
					if (not pread.good()) continue;
					getline(pread, new_proc.name);
					pread.close();
					//? Check for whitespace characters in name and set offset to get correct fields from stat file
					new_proc.name_offset = rng::count(new_proc.name, ' ');

					pread.open(d.path() / "cmdline");
					if (not pread.good()) continue;
					long_string.clear();
					while(getline(pread, long_string, '\0')) {
						new_proc.cmd += long_string + ' ';
						if (new_proc.cmd.size() > 1000) {
							new_proc.cmd.resize(1000);
							break;
						}
					}
					pread.close();
					if (not new_proc.cmd.empty()) new_proc.cmd.pop_back();

					pread.open(d.path() / "status");
					if (not pread.good()) continue;
					string uid;
					string line;
					while (pread.good()) {
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
					if (uid_user.contains(uid)) {
						new_proc.user = uid_user.at(uid);
					}
					else {
					#if !(defined(STATIC_BUILD) && defined(__GLIBC__))
						try {
							struct passwd* udet;
							udet = getpwuid(stoi(uid));
							if (udet != nullptr and udet->pw_name != nullptr) {
								new_proc.user = string(udet->pw_name);
							}
							else {
								new_proc.user = uid;
							}
						}
						catch (...) { new_proc.user = uid; }
					#else
						new_proc.user = uid;
					#endif
					}
				}

				//? Parse /proc/[pid]/stat
				pread.open(d.path() / "stat");
				if (not pread.good()) continue;

				const auto& offset = new_proc.name_offset;
				short_str.clear();
				int x = 0, next_x = 3;
				uint64_t cpu_t = 0;
				try {
					for (;;) {
						while (pread.good() and ++x < next_x + offset) pread.ignore(SSmax, ' ');
						if (not pread.good()) break;
						else getline(pread, short_str, ' ');

						switch (x-offset) {
							case 3: //? Process state
								new_proc.state = short_str.at(0);
								if (new_proc.ppid != 0) next_x = 14;
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
								new_proc.p_nice = stoll(short_str);
								continue;
							case 20: //? Number of threads
								new_proc.threads = stoull(short_str);
								if (new_proc.cpu_s == 0) {
									next_x = 22;
									new_proc.cpu_t = cpu_t;
								}
								else
									next_x = 24;
								continue;
							case 22: //? Get cpu seconds if missing
								new_proc.cpu_s = stoull(short_str);
								next_x = 24;
								continue;
							case 24: //? RSS memory (can be inaccurate, but parsing smaps increases total cpu usage by ~20x)
								if (cmp_greater(short_str.size(), totalMem_len))
									new_proc.mem = totalMem;
								else
									new_proc.mem = stoull(short_str) * Shared::pageSize;
						}
						break;
					}

				}
				catch (const std::invalid_argument&) { continue; }
				catch (const std::out_of_range&) { continue; }

				pread.close();

				if (should_filter_kernel and new_proc.ppid == KTHREADD) {
					kernels_procs.emplace(new_proc.pid);
					found.pop_back();
				}

				if (x-offset < 24) continue;

				//? Get RSS memory from /proc/[pid]/statm if value from /proc/[pid]/stat looks wrong
				if (new_proc.mem >= totalMem) {
					pread.open(d.path() / "statm");
					if (not pread.good()) continue;
					pread.ignore(SSmax, ' ');
					pread >> new_proc.mem;
					new_proc.mem *= Shared::pageSize;
					pread.close();
				}

				//? Process cpu usage since last update
				new_proc.cpu_p = clamp(round(cmult * 1000 * (cpu_t - new_proc.cpu_t) / max((uint64_t)1, cputimes - old_cputimes)) / 10.0, 0.0, 100.0 * Shared::coreCount);

				//? Process cumulative cpu usage since process start
				new_proc.cpu_c = (double)cpu_t / max(1.0, (uptime * Shared::clkTck) - new_proc.cpu_s);

				//? Update cached value with latest cpu times
				new_proc.cpu_t = cpu_t;

				if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
					got_detailed = true;
				}
			}

			//? Clear dead processes from current_procs and remove kernel processes if enabled
			auto eraser = rng::remove_if(current_procs, [&](const auto& element){ return not v_contains(found, element.pid); });
			current_procs.erase(eraser.begin(), eraser.end());

			//? Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, round(uptime), current_procs);
			}
			else if (show_detailed and not got_detailed and detailed.status != "Dead") {
				detailed.status = "Dead";
				redraw = true;
			}

			old_cputimes = cputimes;
		}
		//* ---------------------------------------------Collection done-----------------------------------------------

		//* Match filter if defined
		if (should_filter) {
			filter_found = 0;
			for (auto& p : current_procs) {
				if (not tree and not filter.empty()) {
					if (!matches_filter(p, filter)) {
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

		//* Sort processes
		if (sorted_change or not no_update) {
			proc_sorter(current_procs, sorting, reverse, tree);
		}

		//* Generate tree view if enabled
		if (tree and (not no_update or should_filter or sorted_change)) {
			bool locate_selection = false;
			if (auto find_pid = (collapse != -1 ? collapse : expand); find_pid != -1) {
				auto collapser = rng::find(current_procs, find_pid, &proc_info::pid);
				if (collapser != current_procs.end()) {
					if (collapse == expand) {
						collapser->collapsed = not collapser->collapsed;
					}
					else if (collapse > -1) {
						collapser->collapsed = true;
					}
					else if (expand > -1) {
						collapser->collapsed = false;
					}
					if (Config::ints.at("proc_selected") > 0) locate_selection = true;
				}
				collapse = expand = -1;
			}
			if (should_filter or not filter.empty()) filter_found = 0;

			vector<tree_proc> tree_procs;
			tree_procs.reserve(current_procs.size());

			for (auto& p : current_procs) {
				if (not v_contains(found, p.ppid)) p.ppid = 0;
			}

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, & proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			//? Recursive sort over tree structure to account for collapsed processes in the tree
			int index = 0;
			tree_sort(tree_procs, sorting, reverse, index, current_procs.size());

			//? Add tree begin symbol to first item if childless
			if (tree_procs.size() > 0 and tree_procs.front().children.empty() and tree_procs.front().entry.get().prefix.size() >= 8)
				tree_procs.front().entry.get().prefix.replace(tree_procs.front().entry.get().prefix.size() - 8, 8, " ┌─ ");

			//? Add tree terminator symbol to last item if childless
			if (tree_procs.size() > 0 and tree_procs.back().children.empty() and tree_procs.back().entry.get().prefix.size() >= 8)
				tree_procs.back().entry.get().prefix.replace(tree_procs.back().entry.get().prefix.size() - 8, 8, " └─ ");

			//? Final sort based on tree index
			rng::sort(current_procs, rng::less{}, & proc_info::tree_index);

			//? Move current selection/view to the selected process when collapsing/expanding in the tree
			if (locate_selection) {
				int loc = rng::find(current_procs, Proc::selected_pid, &proc_info::pid)->tree_index;
				if (Config::ints.at("proc_start") >= loc or Config::ints.at("proc_start") <= loc - Proc::select_max)
					Config::ints.at("proc_start") = max(0, loc - 1);
				Config::ints.at("proc_selected") = loc - Config::ints.at("proc_start") + 1;
			}
		}

		numpids = (int)current_procs.size() - filter_found;

		return current_procs;
	}
}

namespace Tools {
	double system_uptime() {
		string upstr;
		ifstream pread(Shared::procPath / "uptime");
		if (pread.good()) {
			try {
				getline(pread, upstr, ' ');
				pread.close();
				return stod(upstr);
			}
			catch (const std::invalid_argument&) {}
			catch (const std::out_of_range&) {}
		}
        throw std::runtime_error("Failed to get uptime from " + string{Shared::procPath} + "/uptime");
	}
}
