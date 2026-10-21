/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
 *
 L icensed under the* Apache License, Version 2.0 (the "License");
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
//? Need the structured /proc interface (psinfo_t). <procfs.h> selects it by defining _STRUCTURED_PROC 1 before
//? including <sys/procfs.h>; if <sys/procfs.h> is seen first it gets the old ioctl-based one and psinfo_t is missing.
//? (Previously <libproc.h> happened to pull <procfs.h> in first.) So only use <procfs.h>, and define it up front
//? so no earlier header can pull in the wrong flavour.
#ifndef _STRUCTURED_PROC
#define _STRUCTURED_PROC 1
#endif
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <kstat.h>
#include <sys/loadavg.h>
#include <sys/sysinfo.h>
#include <sys/mnttab.h>
#include <sys/swap.h>
#include <procfs.h>
#include <dirent.h>
//? getifaddrs(3C) does not exist on Solaris 10 (<ifaddrs.h> is missing), so interfaces are
//? enumerated with the SIOCGLIFCONF ioctls instead - see list_interfaces() below.
#include <net/if.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/tcp_fsm.h>
#include <netinet/in.h> // for inet_ntop stuff
#include <pwd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#include <stdexcept>
#include <cmath>
#include <fstream>
#include <numeric>
#include <ranges>
#include <regex>
#include <string>
#include <memory>
#include <utility>
#include <unordered_set>

#include <fmt/format.h>

#include "../btop_config.hpp"
#include "../btop_log.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
//? Aliased to something other than "fs" - on native Solaris (unlike illumos) <sys/buf.h>,
//? pulled in transitively by some of the headers above, declares its own "fs" at file scope,
//? which collides with a plain "namespace fs = ...".
namespace btop_fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields = {"Auto", "total"};
	vector<string> available_sensors = {"Auto"};
	cpu_info current_cpu;
	bool got_sensors = false, cpu_temp_only = false, supports_watts = false;

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search /proc/cpuinfo for a cpu name
	string get_cpuName();

	struct Sensor {
		btop_fs::path path;
		string label;
		int64_t temp = 0;
		int64_t high = 0;
		int64_t crit = 0;
	};

	string cpu_sensor;
	vector<string> core_sensors;
	std::unordered_map<int, int> core_mapping;
}  // namespace Cpu

namespace Mem {
	double old_uptime;
	std::vector<string> zpools;

	void get_zpools();
}

//? Read boot time from illumos kstat (module "unix", name "system_misc", stat "boot_time")
static long get_boot_time_kstat() {
	long boot_time = 0;
	kstat_ctl_t *kc = kstat_open();
	if (!kc) {
		Logger::warning("kstat_open() failed while getting boot time");
		return 0;
	}

	kstat_t *ksp = kstat_lookup(kc, "unix", 0, "system_misc");
	if (ksp && kstat_read(kc, ksp, NULL) != -1) {
		kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ksp, "boot_time");
		if (kn) {
			switch (kn->data_type) {
				case KSTAT_DATA_INT32:  boot_time = kn->value.i32; break;
				case KSTAT_DATA_UINT32: boot_time = kn->value.ui32; break;
				case KSTAT_DATA_INT64:  boot_time = kn->value.i64; break;
				case KSTAT_DATA_UINT64: boot_time = kn->value.ui64; break;
				default: break;
			}
		}
	} else {
		Logger::warning("Could not get boot time via kstat");
	}

	kstat_close(kc);
	return boot_time;
}

namespace Shared {

	btop_fs::path passwd_path;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount, physicalCoreCount, arg_max;
	int totalMem_len;
	long bootTime;

	void init() {
		//? Shared global variables init
		coreCount = sysconf(_SC_NPROCESSORS_ONLN);
		if (coreCount < 1) {
			coreCount = 1;
			Logger::warning("Could not determine number of cores, defaulting to 1.");
		}

		pageSize = sysconf(_SC_PAGESIZE);
		if (pageSize <= 0) {
			pageSize = 4096;
			Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
		}

		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0) {
			clkTck = 100;
			Logger::warning("Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect.");
		}

		long physPages = sysconf(_SC_PHYS_PAGES);
		if (physPages > 0) {
			totalMem = (uint64_t)physPages * (uint64_t)pageSize;
		} else {
			Logger::warning("Could not get memory size");
		}

		bootTime = get_boot_time_kstat();

		//* Get maximum length of process arguments
		arg_max = sysconf(_SC_ARG_MAX);

		//? Init for namespace Cpu
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Logger::debug("Init -> Cpu::collect()");
		Cpu::collect();
		for (auto &[field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty() and not v_contains(Cpu::available_fields, field)) Cpu::available_fields.push_back(field);
		}
		Logger::debug("Init -> Cpu::get_cpuName()");
		Cpu::cpuName = Cpu::get_cpuName();
		Logger::debug("Init -> Cpu::get_sensors()");
		Cpu::got_sensors = Cpu::get_sensors();
		Logger::debug("Init -> Cpu::get_core_mapping()");
		Cpu::core_mapping = Cpu::get_core_mapping();

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Logger::debug("Init -> Mem::collect()");
		Mem::collect();
		Logger::debug("Init -> Mem::get_zpools()");
		Mem::get_zpools();
	}
}  // namespace Shared

namespace Cpu {
	string cpuName;
	string cpuHz;
	bool has_battery = true;
	tuple<int, float, long, string> current_bat;

	const array<string, 10> time_names = {"user", "nice", "system", "idle"};

	std::unordered_map<string, long long> cpu_old = {
		{"totals", 0},
		{"idles", 0},
		{"user", 0},
		{"system", 0},
		{"iowait", 0}
	};

	string get_cpuName() {
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return "";

		string name;
		//? Same rationale as the cpu_stat chain-walk in Cpu::collect(): find whatever "cpu_info"
		//? kstat actually exists rather than assuming a particular instance number is present.
		for (kstat_t *ksp = kc->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
			if (strcmp(ksp->ks_module, "cpu_info") != 0) continue;
			if (kstat_read(kc, ksp, NULL) == -1) continue;

			//? "brand" (x86 only) is the full CPUID brand string, e.g. "Intel(R) Core(TM)
			//? i5-7200U CPU @ 2.50GHz". "implementation" exists on both x86 and SPARC and gives a
			//? shorter description - used as a fallback when "brand" isn't present.
			for (const char *field : {"brand", "implementation"}) {
				kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ksp, field);
				if (kn and kn->data_type == KSTAT_DATA_STRING and kn->value.str.addr.ptr and *kn->value.str.addr.ptr) {
					name = kn->value.str.addr.ptr;
					break;
				}
			}
			break; //? one CPU's info is enough - they're identical on any SMP illumos/Solaris box
		}

		kstat_close(kc);

		//? Same cleanup upstream Linux's get_cpuName() applies for a brand-string shape it doesn't
		//? specifically recognize: keep everything before "@" (drops a "CPU @ x.xxGHz" clock-speed
		//? suffix, if present), strip common boilerplate words, collapse doubled spaces, trim. Used
		//? here unconditionally rather than as a fallback: it turns e.g. "Intel(R) Core(TM)
		//? i7-6700K CPU @ 4.00GHz" into "i7-6700K", the same short form Linux shows, without
		//? needing Linux's Xeon/Ryzen/Intel-specific token-position matching, which is tuned to
		//? /proc/cpuinfo's exact layout and isn't expected to generalize to illumos's kstat
		//? "brand"/"implementation" strings (including whatever SPARC's "implementation" looks
		//? like, untested here).
		if (not name.empty()) {
			auto name_vec = ssplit(name, ' ');
			name.clear();
			for (const auto &n : name_vec) {
				if (n == "@") break;
				name += n + ' ';
			}
			if (not name.empty()) name.pop_back();
			//? illumos's kstat "brand" renders the trademark symbols lowercase ("(r)"/"(tm)"), unlike
			//? Linux's /proc/cpuinfo which is always uppercase - both are stripped to be safe.
			for (const auto *replace : {"Processor", "CPU", "(R)", "(TM)", "(r)", "(tm)", "Intel", "AMD", "Core"}) {
				name = s_replace(name, replace, "");
				name = s_replace(name, "  ", " ");
			}
			name = trim(name);
		}

		return name;
	}

	bool get_sensors() {
		//? TODO: illumos CPU temperature sensors not yet ported.
		got_sensors = false;
		return got_sensors;
	}

	void update_sensors() {
		//? TODO: illumos CPU temperature sensors not yet ported. No-op for now.
	}

	string get_cpuHz() {
		//? TODO: illumos CPU frequency reporting not yet ported.
		return "";
	}

	auto get_core_mapping() -> std::unordered_map<int, int> {
		std::unordered_map<int, int> core_map;
		if (cpu_temp_only) return core_map;

		for (long i = 0; i < Shared::coreCount; i++) {
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

	auto get_battery() -> tuple<int, float, long, string> {
		//? TODO: illumos battery reporting not yet ported. VMs have no battery anyway.
		has_battery = false;
		return {0, 0.0f, 0, ""};
	}

	auto collect(bool no_update) -> cpu_info & {
		if (Runner::stopping or (no_update and not current_cpu.cpu_percent.at("total").empty()))
			return current_cpu;
		auto &cpu = current_cpu;

		if (getloadavg(cpu.load_avg.data(), cpu.load_avg.size()) < 0) {
			Logger::error("failed to get load averages");
		}

		kstat_ctl_t *kc = kstat_open();
		if (!kc) {
			Logger::error("kstat_open() failed in Cpu::collect()");
			return cpu;
		}

		long long global_totals = 0;
		long long global_idles = 0;
		long long global_user = 0, global_kernel = 0, global_wait = 0;

		//? Walk the live kstat chain for "cpu_stat" kstats instead of probing instances
		//? "cpu_stat0".."cpu_stat<coreCount-1>" by number. CPU ids are not guaranteed to be a
		//? contiguous, zero-based range - on SPARC in particular the id comes from system
		//? topology (e.g. a 64-CPU system reported as instances 64-127), so the old probe-by-
		//? number approach matched nothing and silently left every core blank. core_percent and
		//? core_old_totals/idles are still indexed 0..coreCount-1; "i" here is that array slot,
		//? assigned in the order kstats are found, not the kernel's CPU id.
		long i = 0;
		for (kstat_t *ksp = kc->kc_chain; ksp != nullptr and i < Shared::coreCount; ksp = ksp->ks_next) {
			if (strcmp(ksp->ks_module, "cpu_stat") != 0) continue;
			if (kstat_read(kc, ksp, NULL) == -1) {
				Logger::warning("Could not read kstat for cpu instance {}", ksp->ks_instance);
				continue;
			}
			cpu_stat_t *cs = (cpu_stat_t *)ksp->ks_data;

			try {
				long long c_user = cs->cpu_sysinfo.cpu[CPU_USER];
				long long c_kernel = cs->cpu_sysinfo.cpu[CPU_KERNEL];
				long long c_wait = cs->cpu_sysinfo.cpu[CPU_WAIT];
				long long c_idle = cs->cpu_sysinfo.cpu[CPU_IDLE];

				const long long totals = c_user + c_kernel + c_wait + c_idle;
				const long long idles = c_idle;

				global_totals += totals;
				global_idles += idles;
				global_user += c_user;
				global_kernel += c_kernel;
				global_wait += c_wait;

				//? Calculate cpu total for each core
				const long long calc_totals = max(0ll, totals - core_old_totals.at(i));
				const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
				core_old_totals.at(i) = totals;
				core_old_idles.at(i) = idles;

				cpu.core_percent.at(i).push_back(
					calc_totals > 0
					? clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll)
					: 0ll
				);

				//? Reduce size if there are more values than needed for graph
				if (cpu.core_percent.at(i).size() > 40) cpu.core_percent.at(i).pop_front();

			} catch (const std::exception &e) {
				kstat_close(kc);
				Logger::error("Cpu::collect() : {}", e.what());
				throw std::runtime_error(fmt::format("collect() : {}", e.what()));
			}

			i++;
		}
		if (i < Shared::coreCount)
			Logger::warning("Cpu::collect() -> found {} cpu_stat kstat(s), expected {}", i, Shared::coreCount);

		kstat_close(kc);

		const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
		const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));

		//? Populate cpu.cpu_percent with the fields illumos actually provides
		auto push_field = [&](const string &field, long long val) {
			cpu.cpu_percent.at(field).push_back(clamp((long long)round((double)(val - cpu_old.at(field)) * 100 / calc_totals), 0ll, 100ll));
			cpu_old.at(field) = val;
			while (cmp_greater(cpu.cpu_percent.at(field).size(), width * 2)) cpu.cpu_percent.at(field).pop_front();
		};

			push_field("user", global_user);
			push_field("system", global_kernel);
			push_field("iowait", global_wait);

			cpu_old.at("totals") = global_totals;
			cpu_old.at("idles") = global_idles;

			//? Total usage of cpu
			cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

			//? Reduce size if there are more values than needed for graph
			while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

			if (Config::getB("show_cpu_freq")) {
				auto hz = get_cpuHz();
				if (hz != "") {
					cpuHz = hz;
				}
			}

			if (Config::getB("check_temp") and got_sensors)
				update_sensors();

		if (Config::getB("show_battery") and has_battery)
			current_bat = get_battery();

		return cpu;
	}
}  // namespace Cpu

//? Read ZFS ARC size from kstat (module "zfs", name "arcstats", stat "size").
//? Returns 0 if ZFS/ARC kstats aren't present (e.g. no zpools imported).
static uint64_t get_arc_size() {
	uint64_t arc_size = 0;
	kstat_ctl_t *kc = kstat_open();
	if (!kc) return 0;
	kstat_t *ksp = kstat_lookup(kc, "zfs", 0, "arcstats");
	if (ksp && kstat_read(kc, ksp, NULL) != -1) {
		kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ksp, "size");
		if (kn) arc_size = kn->value.ui64;
	}
	kstat_close(kc);
	return arc_size;
}

namespace Mem {
	bool has_swap = false;
	vector<string> fstab;
	btop_fs::file_time_type fstab_time;
	int disk_ios = 0;
	vector<string> last_found;

	mem_info current_mem{};

	uint64_t get_totalMem() {
		return Shared::totalMem;
	}

	void assign_values(struct disk_info& disk, int64_t readBytes, int64_t writeBytes) {
		disk_ios++;
		if (disk.io_read.empty()) {
			disk.io_read.push_back(0);
		} else {
			disk.io_read.push_back(max((int64_t)0, (readBytes - disk.old_io.at(0))));
		}
		disk.old_io.at(0) = readBytes;
		while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

		if (disk.io_write.empty()) {
			disk.io_write.push_back(0);
		} else {
			disk.io_write.push_back(max((int64_t)0, (writeBytes - disk.old_io.at(1))));
		}
		disk.old_io.at(1) = writeBytes;
		while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

		// no io times - need to push something anyway or we'll get an ABORT
		if (disk.io_activity.empty())
			disk.io_activity.push_back(0);
		else
			disk.io_activity.push_back(clamp((long)round((double)(disk.io_write.back() + disk.io_read.back()) / (1 << 20)), 0l, 100l));
		while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
	}

	class PipeWrapper {
	public:
		PipeWrapper(const char *file, const char *mode) {fd = popen(file, mode);}
		virtual ~PipeWrapper() {if (fd) pclose(fd);}
		auto operator()() -> FILE* { return fd;};
	private:
		FILE *fd;
	};

	// find all zpools in the system. Do this only at startup.
	void get_zpools() {
		std::regex toReplace("\\.");
		PipeWrapper poolPipe = PipeWrapper("zpool list -H -o name", "r");

		while (not std::feof(poolPipe())) {
			char poolName[512];
			size_t len = 512;
			if (fgets(poolName, len, poolPipe())) {
				poolName[strcspn(poolName, "\n")] = 0;
				Logger::debug("zpool found: {}", poolName);
				Mem::zpools.push_back(std::regex_replace(poolName, toReplace, "%25"));
			}
		}
	}

	//? illumos: sum I/O across all physical "disk"-class kstats (sd*, cmdk*, nvme*, etc.).
	//?
	//? LIMITATION: on illumos, a ZFS mount's "device" in /etc/mnttab is the dataset
	//? name (e.g. "rpool/export/home/alex"), not a physical device path - unlike
	//? FreeBSD, where every mount has a real block-device string to match against
	//? devstat. Since a single pool typically spans one or two physical disks shared
	//? by every dataset/mountpoint on it, we can't cleanly attribute I/O to each
	//? individual mountpoint the way FreeBSD's per-dataset zfs kstat sysctls do.
	//? As a pragmatic first pass, aggregate physical-disk I/O is attributed to the
	//? root ("/") mountpoint only, and other mountpoints on the same pool show no
	//? separate I/O activity.
	//? TODO: investigate illumos's per-dataset zfs kstats (module "zfs_objset" on
	//? newer OpenZFS-based illumos) for true per-dataset attribution.
	void collect_disk(std::unordered_map<string, disk_info> &disks, std::unordered_map<string, string> &mapping) {
		(void)mapping;  // unused on illumos - kept for interface symmetry with other platforms

		kstat_ctl_t *kc = kstat_open();
		if (!kc) {
			Logger::warning("kstat_open() failed in Mem::collect_disk()");
			return;
		}

		uint64_t total_read = 0, total_write = 0;
		for (kstat_t *ksp = kc->kc_chain; ksp != nullptr; ksp = ksp->ks_next) {
			if (string(ksp->ks_class) != "disk") continue;
			if (ksp->ks_type != KSTAT_TYPE_IO) continue;
			//? Skip "zfs" module pool-level entries - they duplicate the totals
			//? already reported by the underlying sd*/cmdk*/nvme* device entries.
			if (string(ksp->ks_module) == "zfs") continue;
			if (kstat_read(kc, ksp, NULL) == -1) continue;

			kstat_io_t *io = (kstat_io_t *)ksp->ks_data;
			total_read += io->nread;
			total_write += io->nwritten;
		}
		kstat_close(kc);

		if (disks.contains("/")) {
			assign_values(disks.at("/"), (int64_t)total_read, (int64_t)total_write);
		}
	}

	auto collect(bool no_update) -> mem_info & {
		if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
			return current_mem;

		auto show_swap = Config::getB("show_swap");
		auto show_disks = Config::getB("show_disks");
		auto swap_disk = Config::getB("swap_disk");
		auto &mem = current_mem;
		static bool snapped = (getenv("BTOP_SNAPPED") != nullptr);

		//? Memory stats via illumos kstat("unix", 0, "system_pages") + ZFS ARC size.
		//? illumos has no Linux/BSD-style active/inactive page-aging split; instead
		//? we treat ZFS ARC as reclaimable "cached" memory (same conceptual role as
		//? Linux page cache / FreeBSD's own ARC handling), and derive used/available
		//? from total/free/cached so the three figures stay internally consistent:
		//?   used      = total - free - cached
		//?   available = free + cached
		uint64_t pagesTotal = 0, pagesFree = 0;
		{
			kstat_ctl_t *kc = kstat_open();
			if (kc) {
				kstat_t *ksp = kstat_lookup(kc, "unix", 0, "system_pages");
				if (ksp && kstat_read(kc, ksp, NULL) != -1) {
					kstat_named_t *kt = (kstat_named_t *)kstat_data_lookup(ksp, "pagestotal");
					kstat_named_t *kf = (kstat_named_t *)kstat_data_lookup(ksp, "pagesfree");
					if (kt) pagesTotal = kt->value.ui64;
					if (kf) pagesFree = kf->value.ui64;
				} else {
					Logger::warning("Could not read kstat unix:system_pages in Mem::collect()");
				}
				kstat_close(kc);
			} else {
				Logger::warning("kstat_open() failed in Mem::collect()");
			}
		}
		(void)pagesTotal;  // total comes from Shared::totalMem (sysconf-derived); kept for reference/debugging

		uint64_t freeBytes = pagesFree * (uint64_t)Shared::pageSize;
		uint64_t cachedBytes = get_arc_size();
		uint64_t usedBytes = (Shared::totalMem > freeBytes + cachedBytes)
			? Shared::totalMem - freeBytes - cachedBytes
			: 0;

		mem.stats.at("used") = usedBytes;
		mem.stats.at("cached") = cachedBytes;
		mem.stats.at("free") = freeBytes;
		mem.stats.at("available") = freeBytes + cachedBytes;

		if (show_swap) {
			//? illumos native swapctl(2): no kvm handle needed, direct syscall.
			uint64_t totalSwapPages = 0, freeSwapPages = 0;
			int num = swapctl(SC_GETNSWP, nullptr);
			if (num > 0) {
				size_t sz = sizeof(swaptbl_t) + (size_t)num * sizeof(swapent_t);
				std::unique_ptr<char[]> buf(new char[sz]);
				swaptbl_t *st = reinterpret_cast<swaptbl_t *>(buf.get());
				st->swt_n = num;
				vector<std::unique_ptr<char[]>> pathBufs;
				pathBufs.reserve(num);
				for (int i = 0; i < num; i++) {
					pathBufs.emplace_back(new char[MAXPATHLEN]);
					st->swt_ent[i].ste_path = pathBufs.back().get();
				}
				int n = swapctl(SC_LIST, st);
				if (n > 0) {
					for (int i = 0; i < n; i++) {
						totalSwapPages += st->swt_ent[i].ste_pages;
						freeSwapPages += st->swt_ent[i].ste_free;
					}
				} else if (n < 0) {
					Logger::warning("swapctl(SC_LIST) failed in Mem::collect()");
				}
			} else if (num < 0) {
				Logger::warning("swapctl(SC_GETNSWP) failed in Mem::collect()");
			}
			uint64_t totalSwap = totalSwapPages * (uint64_t)Shared::pageSize;
			uint64_t freeSwap = freeSwapPages * (uint64_t)Shared::pageSize;
			uint64_t usedSwap = (totalSwap > freeSwap) ? totalSwap - freeSwap : 0;
			mem.stats.at("swap_total") = totalSwap;
			mem.stats.at("swap_used") = usedSwap;
			mem.stats.at("swap_free") = freeSwap;
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
			std::unordered_map<string, string> mapping;  // keep mapping from device -> mountpoint, since IOKit doesn't give us the mountpoint
			double uptime = system_uptime();
			auto &disks_filter = Config::getS("disks_filter");
			bool filter_exclude = false;
			// auto only_physical = Config::getB("only_physical");
			auto &disks = mem.disks;
			vector<string> filter;
			if (not disks_filter.empty()) {
				filter = ssplit(disks_filter);
				if (filter.at(0).starts_with("exclude=")) {
					filter_exclude = true;
					filter.at(0) = filter.at(0).substr(8);
				}
			}

			//? illumos: enumerate live mounts from /etc/mnttab via getmntent(3C),
			//? rather than BSD's getmntinfo()/struct statfs. Pseudo-filesystems are
			//? excluded by fstype, matching what a real /etc/mnttab actually reports
			//? on illumos (confirmed against a running system): devfs, dev, ctfs,
			//? proc, mntfs, tmpfs, objfs, bootfs, sharefs, lofs, fd, autofs.
			static const std::unordered_set<string> pseudo_fstypes = {
				"devfs", "dev", "ctfs", "proc", "mntfs", "tmpfs",
				"objfs", "bootfs", "sharefs", "lofs", "fd", "autofs",
				"vmblock", "vmhgfs"  //? VMware Tools pseudo-filesystems (seen on a Solaris 10 guest)
			};

			vector<string> found;
			found.reserve(last_found.size());

			FILE *mfp = fopen(MNTTAB, "r");
			if (mfp) {
				struct mnttab mt;
				while (getmntent(mfp, &mt) == 0) {
					string fstype = mt.mnt_fstype ? mt.mnt_fstype : "";
					if (pseudo_fstypes.contains(fstype))
						continue;

					string mountpoint = mt.mnt_mountp ? mt.mnt_mountp : "";
					string dev = mt.mnt_special ? mt.mnt_special : "";
					if (mountpoint.empty()) continue;

					std::error_code ec;
					mapping[dev] = mountpoint;

					//? Match filter if not empty
					if (not filter.empty()) {
						bool match = v_contains(filter, mountpoint);
						if ((filter_exclude and match) or (not filter_exclude and not match))
							continue;
					}

					found.push_back(mountpoint);
					if (not disks.contains(mountpoint)) {
						disks[mountpoint] = disk_info{btop_fs::canonical(dev, ec), btop_fs::path(mountpoint).filename()};

						if (disks.at(mountpoint).dev.empty())
							disks.at(mountpoint).dev = dev;

						if (disks.at(mountpoint).name.empty())
							disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
					}

					if (not v_contains(last_found, mountpoint))
						redraw = true;

					//? free/total get overwritten properly via statvfs() just below;
					//? no per-mount size info is available directly from mnttab itself.
				}
				fclose(mfp);
			} else {
				Logger::warning("Could not open " MNTTAB " in Mem::collect()");
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
				if (std::error_code ec; not btop_fs::exists(mountpoint, ec))
					continue;
				struct statvfs vfs;
				if (statvfs(mountpoint.c_str(), &vfs) < 0) {
					Logger::warning("Failed to get disk/partition stats with statvfs() for: {}", mountpoint);
					continue;
				}
				disk.total = vfs.f_blocks * vfs.f_frsize;
				disk.free = vfs.f_bfree * vfs.f_frsize;
				disk.used = disk.total - disk.free;
				if (disk.total != 0) {
					disk.used_percent = round((double)disk.used * 100 / disk.total);
					disk.free_percent = 100 - disk.used_percent;
				} else {
					disk.used_percent = 0;
					disk.free_percent = 0;
				}
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
				if (not is_in(name, "/", "swap", "/dev"))
					mem.disks_order.push_back(name);

			disk_ios = 0;
			collect_disk(disks, mapping);

			old_uptime = uptime;
		}
		return mem;
	}

}  // namespace Mem

//? One address of one interface, as reported by SIOCGLIFCONF.
struct IfEntry {
	string name;              //? Physical interface name, e.g. "e1000g0" (":1" alias suffix stripped)
	int family = AF_UNSPEC;   //? AF_INET or AF_INET6
	struct sockaddr_storage addr {};
	bool running = false;     //? IFF_RUNNING
};

//? Portable replacement for getifaddrs(): works on Solaris 10 and on illumos.
//? Returns false and sets err (an errno value) on failure. Like getifaddrs(), an interface with
//? several addresses produces several entries, physical interface first.
static bool list_interfaces(vector<IfEntry> &out, int &err) {
	out.clear();
	err = 0;

	//? SIOCGLIFCONF with AF_UNSPEC lists both IPv4 and IPv6 through an AF_INET socket, but
	//? SIOCGLIFFLAGS is answered by the stack matching the socket family, so IPv6 needs its own socket.
	const int sock4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock4 < 0) { err = errno; return false; }
	const int sock6 = socket(AF_INET6, SOCK_DGRAM, 0);  //? may fail if IPv6 is unavailable - that's fine

	auto cleanup = [&]() { close(sock4); if (sock6 >= 0) close(sock6); };

	struct lifnum lifn {};
	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;
	if (ioctl(sock4, SIOCGLIFNUM, &lifn) < 0) { err = errno; cleanup(); return false; }

	//? A little headroom in case an interface appears between the two ioctls
	vector<struct lifreq> reqs((size_t)lifn.lifn_count + 4);
	struct lifconf lifc {};
	lifc.lifc_family = AF_UNSPEC;
	lifc.lifc_flags = 0;
	lifc.lifc_len = (int)(reqs.size() * sizeof(struct lifreq));
	lifc.lifc_buf = reinterpret_cast<caddr_t>(reqs.data());
	if (ioctl(sock4, SIOCGLIFCONF, &lifc) < 0) { err = errno; cleanup(); return false; }

	const size_t count = (size_t)lifc.lifc_len / sizeof(struct lifreq);
	for (size_t i = 0; i < count and i < reqs.size(); i++) {
		IfEntry e;
		e.name = reqs[i].lifr_name;
		if (auto colon = e.name.find(':'); colon != string::npos)
			e.name.resize(colon);
		e.addr = reqs[i].lifr_addr;
		e.family = e.addr.ss_family;

		struct lifreq fl {};
		strncpy(fl.lifr_name, reqs[i].lifr_name, sizeof(fl.lifr_name) - 1);
		const int fsock = (e.family == AF_INET6 and sock6 >= 0) ? sock6 : sock4;
		if (ioctl(fsock, SIOCGLIFFLAGS, &fl) == 0)
			e.running = (fl.lifr_flags & IFF_RUNNING) != 0;

		out.push_back(std::move(e));
	}

	cleanup();
	return true;
}

namespace Net {
	std::unordered_map<string, net_info> current_net;
	net_info empty_net = {};
	vector<string> interfaces;
	string selected_iface;
	int errors = 0;
	std::unordered_map<string, uint64_t> graph_max = {{"download", {}}, {"upload", {}}};
	std::unordered_map<string, array<int, 2>> max_count = {{"download", {}}, {"upload", {}}};
	bool rescale = true;
	uint64_t timestamp = 0;

	auto collect(bool no_update) -> net_info & {
		auto &net = current_net;
		auto &config_iface = Config::getS("net_iface");
		auto net_sync = Config::getB("net_sync");
		auto net_auto = Config::getB("net_auto");
		auto new_timestamp = time_ms();

		if (not no_update and errors < 3) {
			//? Get interface list (SIOCGLIFCONF; getifaddrs() is not available on Solaris 10)
			vector<IfEntry> if_list;
			int if_err = 0;
			if (not list_interfaces(if_list, if_err)) {
				errors++;
				Logger::error("Net::collect() -> failed to list interfaces, errno: {}", strerror(if_err));
				redraw = true;
				return empty_net;
			}
			static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN); // 46 >= 16, compile-time assurance.
			enum { IPBUFFER_MAXSIZE = INET6_ADDRSTRLEN }; // manually using the known biggest value, guarded by the above static_assert
			char ip[IPBUFFER_MAXSIZE];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all addresses of all interfaces
			for (const auto &ent : if_list) {
				const int family = ent.family;
				const auto &iface = ent.name;
				//? Update available interfaces vector and get status of interface
				if (not v_contains(interfaces, iface)) {
					interfaces.push_back(iface);
					net[iface].connected = ent.running;

					// An interface can have more than one IP of the same family associated with it,
					// but we pick only the first one to show in the NET box.
					// Note: Interfaces without any IPv4 and IPv6 set are still valid and monitorable!
					net[iface].ipv4.clear();
					net[iface].ipv6.clear();
				}
				//? Get IPv4 address
				if (family == AF_INET) {
					if (net[iface].ipv4.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<const struct sockaddr_in*>(&ent.addr)->sin_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv4 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv4 to string for iface {}, errno: {}", iface, strerror(errsv));
						}
					}
				}
				//? Get IPv6 address
				else if (family == AF_INET6) {
					if (net[iface].ipv6.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<const struct sockaddr_in6*>(&ent.addr)->sin6_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv6 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv6 to string for iface {}, errno: {}", iface, strerror(errsv));
						}
					}
				}
			}

			//? illumos: per-interface byte counters via kstat, rather than BSD's
			//? PF_ROUTE/NET_RT_IFLIST routing-socket dump. Each link exposes its
			//? own named kstat (module "link", class "net") with 64-bit rbytes64/
			//? obytes64 counters, looked up directly by interface name.
			std::unordered_map<string, std::tuple<uint64_t, uint64_t>> ifstats;
			{
				kstat_ctl_t *kc = kstat_open();
				if (!kc) {
					Logger::error("kstat_open() failed in Net::collect()");
				} else {
					for (const auto &iface : interfaces) {
						kstat_t *ksp = kstat_lookup(kc, nullptr, -1, iface.c_str());
						if (!ksp or kstat_read(kc, ksp, NULL) == -1) {
							ifstats[iface] = std::tuple(0ull, 0ull);
							continue;
						}
						kstat_named_t *rb = (kstat_named_t *)kstat_data_lookup(ksp, "rbytes64");
						kstat_named_t *ob = (kstat_named_t *)kstat_data_lookup(ksp, "obytes64");
						ifstats[iface] = std::tuple(
							rb ? rb->value.ui64 : 0ull,
							ob ? ob->value.ui64 : 0ull
						);
					}
					kstat_close(kc);
				}
			}

			//? Get total received and transmitted bytes + device address if no ip was found
			for (const auto &iface : interfaces) {
				for (const string dir : {"download", "upload"}) {
					auto &saved_stat = net.at(iface).stat.at(dir);
					auto &bandwidth = net.at(iface).bandwidth.at(dir);
					uint64_t val = dir == "download" ? std::get<0>(ifstats[iface]) : std::get<1>(ifstats[iface]);

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
}  // namespace Net

namespace Proc {

	vector<proc_info> current_procs;
	std::unordered_map<string, string> uid_user;
	string current_sort;
	string current_filter;
	bool current_rev = false;
	bool is_tree_mode;

	btop_fs::file_time_type passwd_time;

	uint64_t cputimes;
	int collapse = -1, expand = -1, toggle_children = -1, collapse_all = -1;
	uint64_t old_cputimes = 0;
	atomic<int> numpids = 0;
	int filter_found = 0;

	detail_container detailed;
	static std::unordered_set<size_t> dead_procs;

	//? illumos: process state comes from psinfo_t's pr_lwp.pr_sname, already a
	//? single printable character (no BSD-style bitmask/lookup needed).
	string get_status(char s) {
		switch (s) {
			case 'S': return "Sleeping";
			case 'R': return "Running";
			case 'O': return "Running";   // on-processor
			case 'W': return "Sleeping";  // waiting (closest analog)
			case 'Z': return "Zombie";
			case 'T': return "Stopped";
			default:  return "Unknown";
		}
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, vector<proc_info> &procs) {
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

		//? Process runtime : current time - start time (both in unix time - seconds since epoch)
		struct timeval currentTime;
		gettimeofday(&currentTime, nullptr);
		// only interested in second granularity, so ignoring tc_usec
		if (detailed.entry.state != 'X') detailed.elapsed = sec_to_dhms(currentTime.tv_sec - detailed.entry.cpu_s);
		else detailed.elapsed = sec_to_dhms(detailed.entry.death_time);
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

		//? Get parent process name
		if (detailed.parent.empty()) {
			auto p_entry = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
			if (p_entry != procs.end()) detailed.parent = p_entry->name;
		}

		//? Expand process status from single char to explanative string
		detailed.status = get_status(detailed.entry.state);

		detailed.mem_bytes.push_back(detailed.entry.mem);
		detailed.memory = floating_humanizer(detailed.entry.mem);

		if (detailed.first_mem == -1 or detailed.first_mem < detailed.mem_bytes.back() / 2 or detailed.first_mem > detailed.mem_bytes.back() * 4) {
			detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Mem::get_totalMem());
			redraw = true;
		}

		while (cmp_greater(detailed.mem_bytes.size(), width)) detailed.mem_bytes.pop_front();

		//? Per-process I/O, from /proc/<pid>/usage (prusage_t, see proc(5)). This block was originally
		//? copied from the macOS backend (proc_pid_rusage()/RUSAGE_INFO_CURRENT are Darwin APIs) and
		//? left commented out with no illumos equivalent filled in, which is why these fields were
		//? always empty here.
		//?
		//? pr_inblk/pr_oublk (block-device I/O counts) were tried first, but testing against real,
		//? sustained raw-device I/O showed them staying at 0 throughout - this turns out to be a
		//? long-documented Solaris/illumos quirk, not a bug here: Brendan Gregg's 2005 analysis
		//? (brendangregg.com/Solaris/paper_diskubyp1.pdf) found they "hardly increase" regardless of
		//? actual I/O volume, and psutil's own Solaris backend deliberately avoids them for the same
		//? reason. pr_ioch ("chars read and written") is what's actually populated - the trade-off is
		//? that it is a single counter covering both directions, with no read/write split available.
		//? The combined total is shown as io_read; io_write is left empty (not "0 Byte", which would
		//? wrongly claim a confirmed-zero write count we don't actually have) to signal that a
		//? separate write figure isn't available on this platform, rather than implying it is zero.
		char usage_path[32];
		snprintf(usage_path, sizeof(usage_path), "/proc/%zu/usage", pid);
		detailed.io_write.clear();
		if (int usage_fd = open(usage_path, O_RDONLY); usage_fd >= 0) {
			prusage_t pu;
			ssize_t rd = read(usage_fd, &pu, sizeof(pu));
			close(usage_fd);
			//? Reading /proc/<pid>/usage can fail for processes we don't own - same caveat as on
			//? every other platform btop supports - in which case io_read just stays empty too.
			if (rd == (ssize_t)sizeof(pu)) {
				detailed.io_read = floating_humanizer((uint64_t)pu.pr_ioch);
			}
		}
	}

	//* Collects and sorts process information from /proc
	auto collect(bool no_update) -> vector<proc_info> & {
		const auto &sorting = Config::getS("proc_sorting");
		auto reverse = Config::getB("proc_reversed");
		const auto &filter = Config::getS("proc_filter");
		auto per_core = Config::getB("proc_per_core");
		auto tree = Config::getB("proc_tree");
		auto show_detailed = Config::getB("show_detailed");
		const auto pause_proc_list = Config::getB("pause_proc_list");
		const size_t detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		bool tree_mode_change = tree != is_tree_mode;
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}
		if (tree_mode_change) is_tree_mode = tree;

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		static vector<size_t> found;

		//? illumos: reuse the global CPU-tick total Cpu::collect() already computed
		//? via kstat, rather than duplicating a second sysctl-based tally here.
		cputimes = (uint64_t)Cpu::cpu_old.at("totals");

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, current_procs);
		} else {
			//* ---------------------------------------------Collection start----------------------------------------------

			should_filter = true;
			found.clear();
			struct timeval currentTime;
			gettimeofday(&currentTime, nullptr);
			const double timeNow = currentTime.tv_sec + (currentTime.tv_usec / 1'000'000);

			//? illumos: enumerate /proc directly - each numeric entry is a pid,
			//? and /proc/<pid>/psinfo is a single fixed-layout struct (psinfo_t)
			//? readable in one read(2) call. No kvm handle or separate argv call
			//? needed (pr_psargs already gives a single, if truncated, command
			//? string), which makes this simpler overall than FreeBSD's kvm walk.
			DIR *procdir = opendir("/proc");
			if (!procdir) {
				Logger::error("Proc::collect() -> failed to open /proc");
			} else {
				struct dirent *entry;
				while ((entry = readdir(procdir)) != nullptr) {
					//? Only interested in purely-numeric entries (pids)
					bool numeric = entry->d_name[0] != '\0';
					for (const char *c = entry->d_name; *c; c++) {
						if (*c < '0' or *c > '9') { numeric = false; break; }
					}
					if (not numeric) continue;

					char path[64];
					snprintf(path, sizeof(path), "/proc/%s/psinfo", entry->d_name);
					int fd = open(path, O_RDONLY);
					if (fd < 0) continue;  //? process exited between readdir() and open() - normal race, skip

					psinfo_t ps;
					ssize_t rd = read(fd, &ps, sizeof(ps));
					close(fd);
					if (rd != (ssize_t)sizeof(ps)) continue;

					const size_t pid = (size_t)ps.pr_pid;
					if (pid < 1) continue;
					found.push_back(pid);

					//? Check if pid already exists in current_procs
					bool no_cache = false;
					auto find_old = rng::find(current_procs, pid, &proc_info::pid);
					//? Only add new processes if not paused
					if (find_old == current_procs.end()) {
						if (not pause_proc_list) {
							current_procs.push_back({pid});
							find_old = current_procs.end() - 1;
							no_cache = true;
						}
						else continue;
					}
					else if (dead_procs.contains(pid)) continue;

					auto &new_proc = *find_old;

					//? Get program name, command, username, parent pid, nice and status
					if (no_cache) {
						new_proc.name = ps.pr_fname;
						new_proc.cmd = ps.pr_psargs;
						if (new_proc.cmd.empty()) new_proc.cmd = new_proc.name;
						if (new_proc.cmd.size() > 1000) {
							new_proc.cmd.resize(1000);
							new_proc.cmd.shrink_to_fit();
						}
						new_proc.ppid = ps.pr_ppid;
						new_proc.cpu_s = round(ps.pr_start.tv_sec);
						struct passwd *pwd = getpwuid(ps.pr_uid);
						if (pwd)
							new_proc.user = pwd->pw_name;
					}
					new_proc.p_nice = ps.pr_lwp.pr_nice;
					new_proc.state = ps.pr_lwp.pr_sname;

					//? illumos pr_time is already combined usr+sys cpu time for
					//? the process (no manual ru_utime+ru_stime summing needed).
					int64_t cpu_t = (int64_t)ps.pr_time.tv_sec * 1'000'000 + ps.pr_time.tv_nsec / 1'000;

					//? illumos pr_rssize is already in KB, unlike FreeBSD's
					//? page-count ki_rssize - convert directly to bytes.
					new_proc.mem = (uint64_t)ps.pr_rssize * 1024;
					new_proc.threads = ps.pr_nlwp;

					//? Process cpu usage since last update.
					//? illumos pr_pctcpu is fixed-point, scaled so 0x8000 == 100%.
					new_proc.cpu_p = clamp((100.0 * ps.pr_pctcpu / 32768.0) * cmult, 0.0, 100.0 * Shared::coreCount);

					//? Process cumulative cpu usage since process start
					new_proc.cpu_c = (double)(cpu_t * Shared::clkTck / 1'000'000) / max(1.0, timeNow - new_proc.cpu_s);

					//? Update cached value with latest cpu times
					new_proc.cpu_t = cpu_t;

					if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
						got_detailed = true;
					}
				}
				closedir(procdir);
			}

			//? Clear dead processes from current_procs if not paused
			if (not pause_proc_list) {
				auto eraser = rng::remove_if(current_procs, [&](const auto& element) { return not v_contains(found, element.pid); });
				current_procs.erase(eraser.begin(), eraser.end());
				if (!dead_procs.empty()) dead_procs.clear();
			}
			//? Set correct state of dead processes if paused
			else {
				const bool keep_dead_proc_usage = Config::getB("keep_dead_proc_usage");
				for (auto& r : current_procs) {
					if (rng::find(found, r.pid) == found.end()) {
						if (r.state != 'X') {
							struct timeval currentTime;
							gettimeofday(&currentTime, nullptr);
							r.death_time = currentTime.tv_sec - r.cpu_s;
						}
						r.state = 'X';
						dead_procs.emplace(r.pid);
						//? Reset cpu usage for dead processes if paused and option is set
						if (!keep_dead_proc_usage) {
							r.cpu_p = 0.0;
							r.mem = 0;
						}
					}
				}
			}

			//? Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, current_procs);
			} else if (show_detailed and not got_detailed and detailed.status != "Dead") {
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
		if ((sorted_change or tree_mode_change) or (not no_update and not pause_proc_list)) {
			proc_sorter(current_procs, sorting, reverse, tree);
		}

		//* Generate tree view if enabled
		if (tree and (not no_update or should_filter or sorted_change)) {
			bool locate_selection = false;

			if (toggle_children != -1) {
				auto collapser = rng::find(current_procs, toggle_children, &proc_info::pid);
				if (collapser != current_procs.end()){
					for (auto& p : current_procs) {
						if (p.ppid == collapser->pid) {
							auto child = rng::find(current_procs, p.pid, &proc_info::pid);
							if (child != current_procs.end()){
								child->collapsed = not child->collapsed;
							}
						}
					}
					if (Config::ints.at("proc_selected") > 0) locate_selection = true;
				}
				toggle_children = -1;
			}

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

			if (collapse_all != -1) {
				toggle_tree_collapse(current_procs);
				collapse_all = -1;
				if (Config::ints.at("proc_selected") > 0) locate_selection = true;
			}

			if (should_filter or not filter.empty()) filter_found = 0;

			vector<tree_proc> tree_procs;
			tree_procs.reserve(current_procs.size());

			if (!pause_proc_list) {
				for (auto& p : current_procs) {
					if (not v_contains(found, p.ppid)) p.ppid = 0;
				}
			}

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, & proc_info::ppid);

			//? Auto-collapse processes with many children when entering tree mode
			_auto_collapse_oversized(current_procs, tree_mode_change);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			//? Recursive sort over tree structure to account for collapsed processes in the tree
			int index = 0;
			tree_sort(tree_procs, sorting, reverse, (pause_proc_list and not (sorted_change or tree_mode_change)), index, current_procs.size());

			//? Recursive construction of ASCII tree prefixes.
			for (auto t = tree_procs.begin(); t != tree_procs.end(); ++t) {
				_collect_prefixes(*t, t == tree_procs.end() - 1);
			}

			//? Final sort based on tree index
			rng::stable_sort(current_procs, rng::less {}, &proc_info::tree_index);

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
}  // namespace Proc

namespace Tools {
	//? illumos: reuse Shared::bootTime (already read once via kstat in
	//? Shared::init()) rather than issuing a second, BSD-only sysctl here.
	double system_uptime() {
		struct timeval currTime;
		gettimeofday(&currTime, nullptr);
		return (double)(currTime.tv_sec - Shared::bootTime);
	}
}  // namespace Tools
