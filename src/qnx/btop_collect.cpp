/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
   QNX Neutrino platform backend for btop++

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

// ---- QNX-specific headers ----
#include <sys/syspage.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>
#include <sys/states.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <devctl.h>

// ---- Standard POSIX headers ----
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

// ---- C++ Standard Library ----
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "../btop_config.hpp"
#include "../btop_log.hpp"
#include "../btop_shared.hpp"
#include "../btop_tools.hpp"

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

// ---------------------------------------------------------------------------
// QNX-local helpers (file-scope)
// ---------------------------------------------------------------------------

namespace {

constexpr unsigned int QNX_MAX_CPUS = 256;

// Idle nanoseconds per CPU, sampled at each Cpu::collect() call.
uint64_t prev_idle_ns[QNX_MAX_CPUS] = {};

// time_ms() reading when prev_idle_ns was last updated (set by Cpu::collect).
uint64_t cpu_collect_ms = 0;

// time_ms() reading when the previous Proc::collect cycle completed (set by Proc::collect).
uint64_t proc_collect_ms = 0;

// Unix epoch of system boot (filled in Shared::init).
time_t boot_time_sec = 0;

// Total physical RAM in bytes (from syspage asinfo).
uint64_t total_ram_bytes = 0;

// Load average state (exponential moving average over 1/5/15 min).
double load_avg_vals[3] = {0.0, 0.0, 0.0};
uint64_t load_avg_last_ms = 0;

// Translate a QNX thread state code to the single-char state btop uses.
char qnx_state_char(unsigned int s) {
    switch (s) {
        case STATE_RUNNING:  return 'R';
        case STATE_READY:    return 'R';
        case STATE_STOPPED:  return 'T';
        case STATE_DEAD:     return 'Z';
        default:             return 'S';
    }
}

// Read a small /proc text file into a string (returns "" on error).
std::string read_proc_file(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return {};
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return {};
    buf[n] = '\0';
    return buf;
}

// Sum all "ram" asinfo entries → total physical bytes.
uint64_t read_total_ram() {
    uint64_t total = 0;
    const struct asinfo_entry* as = SYSPAGE_ENTRY(asinfo);
    int n = (int)(SYSPAGE_ENTRY_SIZE(asinfo) / sizeof(*as));
    const char* strings = SYSPAGE_ENTRY(strings)->data;
    for (int i = 0; i < n; i++) {
        if (std::strcmp(strings + as[i].name, "ram") == 0)
            total += as[i].end - as[i].start + 1;
    }
    return total;
}

// Free physical RAM via POSIX typed-memory interface.
uint64_t read_free_ram() {
    int fd = posix_typed_mem_open("ram", O_RDONLY, POSIX_TYPED_MEM_ALLOCATE);
    if (fd < 0) return 0;
    struct posix_typed_mem_info info{};
    posix_typed_mem_get_info(fd, &info);
    close(fd);
    return (uint64_t)info.posix_tmi_length;
}

// Update exponential moving-average load estimates.
// runnable = number of threads in RUNNING or READY state across all processes.
void update_load_avg(double runnable) {
    uint64_t now_ms = time_ms();
    if (load_avg_last_ms == 0) {
        load_avg_vals[0] = load_avg_vals[1] = load_avg_vals[2] = runnable;
        load_avg_last_ms = now_ms;
        return;
    }
    double dt = (double)(now_ms - load_avg_last_ms) / 1000.0;
    load_avg_last_ms = now_ms;

    // Windows: 1, 5, 15 minutes in seconds
    constexpr double T[3] = {60.0, 300.0, 900.0};
    for (int i = 0; i < 3; i++) {
        double alpha = std::exp(-dt / T[i]);
        load_avg_vals[i] = load_avg_vals[i] * alpha + runnable * (1.0 - alpha);
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Forward declarations of per-namespace storage
// (These must precede Shared::init which references them.)
// ---------------------------------------------------------------------------

namespace Cpu {
    vector<long long> core_old_totals;
    vector<long long> core_old_idles;
    vector<string>    available_fields = {"Auto", "total", "user", "system"};
    vector<string>    available_sensors = {"Auto"};
    cpu_info          current_cpu;
    bool              got_sensors    = false;
    bool              cpu_temp_only  = false;
    bool              supports_watts = false;
    bool              has_battery    = false;
    string            cpuName;
    string            cpuHz;
    std::unordered_map<int, int>       core_mapping;
    tuple<int, float, long, string>    current_bat = {0, 0.0f, 0L, "not_found"};

    // Forward-declare collect so Shared::init can call it.
    auto collect(bool no_update) -> cpu_info&;
    string get_cpuName();
    auto get_core_mapping() -> std::unordered_map<int, int>;
} // namespace Cpu

namespace Mem {
    bool            has_swap  = false;
    int             disk_ios  = 0;
    vector<string>  last_found;
    mem_info        current_mem{};

    auto collect(bool no_update) -> mem_info&;
} // namespace Mem

// ---------------------------------------------------------------------------
// Shared::init
// ---------------------------------------------------------------------------

namespace Shared {
    long coreCount, page_size, clk_tck;

    void init() {
        // CPU count
        coreCount = (long)_syspage_ptr->num_cpu;
        if (coreCount < 1) coreCount = 1;

        // Page size
        page_size = sysconf(_SC_PAGE_SIZE);
        if (page_size <= 0) page_size = 4096;

        // Clock ticks
        clk_tck = sysconf(_SC_CLK_TCK);
        if (clk_tck <= 0) clk_tck = 100;

        // Total RAM
        total_ram_bytes = read_total_ram();

        // Boot time
        boot_time_sec = (time_t)SYSPAGE_ENTRY(qtime)->boot_time;

        // Init per-core percent storage
        Cpu::current_cpu.core_percent.assign((size_t)coreCount, {});
        Cpu::current_cpu.temp.assign((size_t)coreCount + 1, {});
        Cpu::core_old_totals.assign((size_t)coreCount, 0LL);
        Cpu::core_old_idles.assign((size_t)coreCount, 0LL);

        Logger::debug("Shared::init -> Cpu::collect()");
        Cpu::collect();

        for (auto& [field, vec] : Cpu::current_cpu.cpu_percent) {
            if (not vec.empty() and not v_contains(Cpu::available_fields, field))
                Cpu::available_fields.push_back(field);
        }

        Logger::debug("Shared::init -> Cpu::get_cpuName()");
        Cpu::cpuName = Cpu::get_cpuName();

        Logger::debug("Shared::init -> Cpu::get_core_mapping()");
        Cpu::core_mapping = Cpu::get_core_mapping();

        Logger::debug("Shared::init -> Mem::collect()");
        Mem::collect();

        Logger::debug("Shared::init complete");
    }
} // namespace Shared

// ---------------------------------------------------------------------------
// Tools::system_uptime
// ---------------------------------------------------------------------------

namespace Tools {
    double system_uptime() {
        time_t bt = boot_time_sec;
        if (bt == 0)
            bt = (time_t)SYSPAGE_ENTRY(qtime)->boot_time;
        return (double)(time(nullptr) - bt);
    }
} // namespace Tools

// ---------------------------------------------------------------------------
// Cpu namespace — full implementation
// ---------------------------------------------------------------------------

namespace Cpu {

    string get_cpuName() {
        struct utsname un{};
        if (uname(&un) == 0)
            return std::string(un.machine) + " (" + un.sysname + " " + un.release + ")";
        return "QNX CPU";
    }

    string get_cpuHz() { return {}; }

    auto get_core_mapping() -> std::unordered_map<int, int> {
        std::unordered_map<int, int> m;
        for (int i = 0; i < (int)Shared::coreCount; i++) m[i] = i;
        return m;
    }

    auto get_battery() -> tuple<int, float, long, string> {
        return {0, 0.0f, 0L, "not_found"};
    }

    auto collect(bool no_update) -> cpu_info& {
        if (Runner::stopping) return current_cpu;
        if (no_update and not current_cpu.cpu_percent.at("total").empty())
            return current_cpu;

        auto& cpu = current_cpu;

        if (cmp_less(cpu.core_percent.size(), (size_t)Shared::coreCount))
            cpu.core_percent.resize((size_t)Shared::coreCount);

        uint64_t now_ms     = time_ms();
        uint64_t elapsed_ms = (cpu_collect_ms == 0) ? 1000 : (now_ms - cpu_collect_ms);
        if (elapsed_ms == 0) elapsed_ms = 1;
        uint64_t elapsed_ns = elapsed_ms * 1'000'000ULL;

        double total_busy = 0.0;
        int    valid      = 0;

        for (int i = 0; i < (int)Shared::coreCount and i < (int)QNX_MAX_CPUS; i++) {
            clockid_t cid = ClockId(1, i + 1); // PID=procnto, TID=cpu+1
            uint64_t  idle_ns = 0;
            if (cid != (clockid_t)-1)
                ClockTime(cid, nullptr, &idle_ns);

            uint64_t delta = (idle_ns >= prev_idle_ns[i]) ? (idle_ns - prev_idle_ns[i]) : 0;
            prev_idle_ns[i] = idle_ns;

            double pct = clamp((1.0 - (double)delta / (double)elapsed_ns) * 100.0, 0.0, 100.0);

            cpu.core_percent.at((size_t)i).push_back((long long)round(pct));
            while (cmp_greater(cpu.core_percent.at((size_t)i).size(),
                               (size_t)Cpu::width * 2 + 2))
                cpu.core_percent.at((size_t)i).pop_front();

            total_busy += pct;
            valid++;
        }
        cpu_collect_ms = now_ms;

        double avg = (valid > 0) ? (total_busy / valid) : 0.0;

        auto push = [&](const string& key, long long v) {
            auto& dq = cpu.cpu_percent.at(key);
            dq.push_back(v);
            while (cmp_greater(dq.size(), (size_t)Cpu::width * 2 + 2))
                dq.pop_front();
        };
        push("total",  (long long)round(avg));
        push("user",   (long long)round(avg));
        push("system", 0LL);

        // Load average: read from /proc/loadavg if available, else use our EMA.
        {
            std::string la_str = read_proc_file("/proc/loadavg");
            double la[3] = {0, 0, 0};
            if (not la_str.empty() and
                std::sscanf(la_str.c_str(), "%lf %lf %lf", &la[0], &la[1], &la[2]) == 3) {
                cpu.load_avg[0] = la[0];
                cpu.load_avg[1] = la[1];
                cpu.load_avg[2] = la[2];
            } else {
                cpu.load_avg[0] = load_avg_vals[0];
                cpu.load_avg[1] = load_avg_vals[1];
                cpu.load_avg[2] = load_avg_vals[2];
            }
        }

        return cpu;
    }
} // namespace Cpu

// ---------------------------------------------------------------------------
// Mem namespace — full implementation
// ---------------------------------------------------------------------------

namespace Mem {

    uint64_t get_totalMem() { return total_ram_bytes; }

    auto collect(bool no_update) -> mem_info& {
        if (Runner::stopping or (no_update and not current_mem.percent.at("used").empty()))
            return current_mem;

        auto& mem = current_mem;

        uint64_t total      = total_ram_bytes;
        uint64_t free_bytes = read_free_ram();
        uint64_t used_bytes = (free_bytes < total) ? (total - free_bytes) : 0;

        mem.stats.at("used")      = used_bytes;
        mem.stats.at("free")      = free_bytes;
        mem.stats.at("available") = free_bytes;
        mem.stats.at("cached")    = 0;
        mem.stats.at("swap_total") = 0;
        mem.stats.at("swap_used")  = 0;
        mem.stats.at("swap_free")  = 0;
        has_swap = false;

        if (total > 0) {
            for (const auto& name : mem_names) {
                mem.percent.at(name).push_back(
                    (long long)round((double)mem.stats.at(name) * 100.0 / (double)total));
                while (cmp_greater(mem.percent.at(name).size(),
                                   (size_t)Mem::width * 2 + 2))
                    mem.percent.at(name).pop_front();
            }
        }

        if (Config::getB("show_disks")) {
            auto& disks = mem.disks;
            const auto& disks_filter = Config::getS("disks_filter");
            vector<string> filter;
            bool filter_exclude = false;
            if (not disks_filter.empty()) {
                filter = ssplit(disks_filter);
                if (filter.at(0).starts_with("exclude=")) {
                    filter_exclude = true;
                    filter.at(0) = filter.at(0).substr(8);
                }
            }

            vector<string> found;
            {
                // QNX has no /proc/mounts; parse the output of the `mount` command instead.
                // Output format: "<device> on <mountpoint> type <fstype> [options]"
                FILE* mf = popen("mount", "r");
                if (mf != nullptr) {
                    char line[1024];
                    while (fgets(line, sizeof(line), mf) != nullptr) {
                        // Tokenise: dev=token[0], "on"=token[1], mp=token[2], "type"=token[3], fstype=token[4]
                        std::istringstream iss(line);
                        std::string dev, on, mp, type_kw, fstype;
                        if (not (iss >> dev >> on >> mp >> type_kw >> fstype)) continue;
                        if (on != "on" or type_kw != "type") continue;

                        // Strip parenthesised options appended to fstype e.g. "dos (fat16)"
                        // fstype itself may already be clean; options follow as next tokens.

                        // Skip virtual / non-storage filesystems
                        if (is_in(fstype, "shmem"s, "proc"s, "tmpfs"s, "devfs"s,
                                  "autofs"s, "procfs"s))
                            continue;

                        if (not filter.empty()) {
                            bool match = v_contains(filter, mp);
                            if ((filter_exclude and match) or (not filter_exclude and not match))
                                continue;
                        }

                        found.push_back(mp);
                        if (not disks.contains(mp)) {
                            std::error_code ec;
                            fs::path dev_path = fs::canonical(dev, ec);
                            string disk_name = (mp == "/") ? "root"s
                                             : fs::path(mp).filename().string();
                            disks[mp] = disk_info{dev_path, disk_name};
                            if (disks.at(mp).dev.empty()) disks.at(mp).dev = dev;
                            disks.at(mp).fstype = fstype;
                        }

                        struct statvfs vfs{};
                        if (statvfs(mp.c_str(), &vfs) == 0 and vfs.f_blocks > 0) {
                            auto& disk = disks.at(mp);
                            disk.total = (int64_t)vfs.f_blocks * (int64_t)vfs.f_frsize;
                            disk.free  = (int64_t)vfs.f_bfree  * (int64_t)vfs.f_frsize;
                            disk.used  = disk.total - disk.free;
                            disk.used_percent = (int)round((double)disk.used  * 100.0 / disk.total);
                            disk.free_percent = 100 - disk.used_percent;
                        }
                    }
                    pclose(mf);
                }
            }

            for (auto it = disks.begin(); it != disks.end();) {
                if (not v_contains(found, it->first)) it = disks.erase(it);
                else ++it;
            }
            if (found.size() != last_found.size()) redraw = true;
            last_found = std::move(found);

            mem.disks_order.clear();
            if (disks.contains("/")) mem.disks_order.push_back("/");
            for (const auto& mp : last_found)
                if (mp != "/") mem.disks_order.push_back(mp);

            disk_ios = 0;
        }

        return mem;
    }
} // namespace Mem

// ---------------------------------------------------------------------------
// Net namespace
// ---------------------------------------------------------------------------

namespace Net {
    std::unordered_map<string, net_info> current_net;
    net_info   empty_net = {};
    vector<string> interfaces;
    string selected_iface;
    int    errors    = 0;
    bool   rescale   = true;
    std::unordered_map<string, uint64_t>      graph_max = {{"download", 0}, {"upload", 0}};
    std::unordered_map<string, array<int, 2>> max_count = {{"download", {0,0}}, {"upload", {0,0}}};
    uint64_t timestamp = 0;

    auto collect(bool no_update) -> net_info& {
        auto& net           = current_net;
        const auto& config_iface = Config::getS("net_iface");
        auto  net_sync      = Config::getB("net_sync");
        auto  net_auto      = Config::getB("net_auto");
        auto  new_timestamp = time_ms();

        if (not no_update and errors < 3) {
            IfAddrsPtr if_addrs{};
            if (if_addrs.get_status() != 0) {
                errors++;
                Logger::error("Net::collect() -> getifaddrs() failed with id {}",
                              if_addrs.get_status());
                redraw = true;
                return empty_net;
            }

            char ip[INET6_ADDRSTRLEN];
            interfaces.clear();

            // First pass: IP addresses & connected state
            for (auto* ifa = if_addrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr) continue;
                int family       = ifa->ifa_addr->sa_family;
                const string iface = ifa->ifa_name;

                if (not v_contains(interfaces, iface)) {
                    interfaces.push_back(iface);
                    net[iface].connected = (ifa->ifa_flags & IFF_RUNNING) != 0;
                    net[iface].ipv4.clear();
                    net[iface].ipv6.clear();
                }
                if (family == AF_INET and net[iface].ipv4.empty()) {
                    if (inet_ntop(AF_INET,
                            &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                            ip, sizeof(ip)))
                        net[iface].ipv4 = ip;
                } else if (family == AF_INET6 and net[iface].ipv6.empty()) {
                    if (inet_ntop(AF_INET6,
                            &reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr)->sin6_addr,
                            ip, sizeof(ip)))
                        net[iface].ipv6 = ip;
                }
            }

            // Second pass: byte counters from AF_LINK ifa_data (struct if_data)
            for (auto* ifa = if_addrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr) continue;
                if (ifa->ifa_addr->sa_family != AF_LINK) continue;
                if (ifa->ifa_data == nullptr) continue;

                const string iface = ifa->ifa_name;
                if (not v_contains(interfaces, iface)) continue;

                auto* ifd = reinterpret_cast<struct if_data*>(ifa->ifa_data);
                uint64_t rx = (uint64_t)ifd->ifi_ibytes;
                uint64_t tx = (uint64_t)ifd->ifi_obytes;

                for (const auto& dir : {"download"s, "upload"s}) {
                    uint64_t val        = (dir == "download") ? rx : tx;
                    auto& saved         = net.at(iface).stat.at(dir);
                    auto& bw            = net.at(iface).bandwidth.at(dir);

                    if (val < saved.last) { saved.rollover += saved.last; saved.last = 0; }
                    if (cmp_greater((unsigned long long)saved.rollover + val,
                                    numeric_limits<uint64_t>::max()))
                        saved.rollover = saved.last = 0;

                    uint64_t dt_ms = (timestamp == 0) ? 1000 : (new_timestamp - timestamp);
                    if (dt_ms == 0) dt_ms = 1;
                    saved.speed = (uint64_t)round((double)(val - saved.last) / ((double)dt_ms / 1000.0));
                    if (saved.speed > saved.top) saved.top = saved.speed;
                    if (saved.offset > val + saved.rollover) saved.offset = 0;
                    saved.total = (val + saved.rollover) - saved.offset;
                    saved.last  = val;

                    bw.push_back((long long)saved.speed);
                    while (cmp_greater(bw.size(), (size_t)Net::width * 2 + 2))
                        bw.pop_front();

                    if (net_auto and selected_iface == iface) {
                        if (saved.speed > graph_max[dir]) {
                            ++max_count[dir][0];
                            if (max_count[dir][1] > 0) --max_count[dir][1];
                        } else if (graph_max[dir] > 10 << 10 and
                                   saved.speed < graph_max[dir] / 10) {
                            ++max_count[dir][1];
                            if (max_count[dir][0] > 0) --max_count[dir][0];
                        }
                    }
                }
            }

            // Clean removed interfaces
            for (auto it = net.begin(); it != net.end();) {
                if (not v_contains(interfaces, it->first)) it = net.erase(it);
                else ++it;
            }
            timestamp = new_timestamp;
        }

        if (net.empty()) return empty_net;

        // Interface selection
        if (selected_iface.empty() or not v_contains(interfaces, selected_iface)) {
            max_count["download"][0] = max_count["download"][1] =
            max_count["upload"][0]   = max_count["upload"][1]   = 0;
            redraw = true;
            if (net_auto) rescale = true;

            if (not config_iface.empty() and v_contains(interfaces, config_iface)) {
                selected_iface = config_iface;
            } else {
                auto sorted = interfaces;
                rng::sort(sorted, [&](const auto& a, const auto& b) {
                    return cmp_greater(
                        net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
                        net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
                });
                selected_iface.clear();
                for (const auto& iface : sorted) {
                    if (net.at(iface).connected) { selected_iface = iface; break; }
                }
                if (selected_iface.empty() and not sorted.empty())
                    selected_iface = sorted.at(0);
                else if (sorted.empty())
                    return empty_net;
            }
        }

        // Auto-scale
        if (net_auto) {
            bool sync = false;
            for (const auto& dir : {"download"s, "upload"s}) {
                for (int sel : {0, 1}) {
                    if (rescale or max_count[dir][sel] >= 5) {
                        const long long avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
                            ? std::accumulate(
                                net.at(selected_iface).bandwidth.at(dir).rbegin(),
                                net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0LL) / 5
                            : (long long)net[selected_iface].stat[dir].speed);
                        graph_max[dir] = (uint64_t)max(
                            (long long)(avg_speed * (sel == 0 ? 1.3 : 3.0)),
                            (long long)(10 << 10));
                        max_count[dir][0] = max_count[dir][1] = 0;
                        redraw = true;
                        if (net_sync) sync = true;
                        break;
                    }
                }
                if (sync) {
                    const auto other = (dir == "upload"s) ? "download"s : "upload"s;
                    graph_max[other] = graph_max[dir];
                    max_count[other][0] = max_count[other][1] = 0;
                    break;
                }
            }
        }

        rescale = false;
        return net.at(selected_iface);
    }
} // namespace Net

// ---------------------------------------------------------------------------
// Proc namespace
// ---------------------------------------------------------------------------

namespace Proc {
    vector<proc_info>                   current_procs;
    std::unordered_map<string, string>  uid_user;
    string  current_sort;
    string  current_filter;
    bool    current_rev      = false;
    bool    is_tree_mode     = false;
    int     collapse         = -1;
    int     expand           = -1;
    int     toggle_children  = -1;
    atomic<int> numpids      = 0;
    int     filter_found     = 0;

    detail_container detailed;
    static std::unordered_set<size_t> dead_procs;

    static string get_status(char s) {
        switch (s) {
            case 'R': return "Running";
            case 'T': return "Stopped";
            case 'Z': return "Zombie";
            case 'S': return "Sleeping";
            default:  return "Unknown";
        }
    }

    static void _collect_details(size_t pid, vector<proc_info>& procs) {
        if (pid != detailed.last_pid) {
            detailed = {};
            detailed.last_pid = pid;
            detailed.skip_smaps = not Config::getB("proc_info_smaps");
        }

        auto p_it = rng::find(procs, pid, &proc_info::pid);
        if (p_it == procs.end()) return;
        detailed.entry = *p_it;

        if (not Config::getB("proc_per_core"))
            detailed.entry.cpu_p *= Shared::coreCount;

        detailed.cpu_percent.push_back(
            clamp((long long)round(detailed.entry.cpu_p), 0LL, 100LL));
        while (cmp_greater(detailed.cpu_percent.size(), (size_t)width))
            detailed.cpu_percent.pop_front();

        if (detailed.entry.state != 'X')
            detailed.elapsed = sec_to_dhms((size_t)(time(nullptr) - (time_t)detailed.entry.cpu_s));
        else
            detailed.elapsed = sec_to_dhms(detailed.entry.death_time);
        if (detailed.elapsed.size() > 8)
            detailed.elapsed.resize(detailed.elapsed.size() - 3);

        if (detailed.parent.empty()) {
            auto pe = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
            if (pe != procs.end()) detailed.parent = pe->name;
        }

        detailed.status = get_status(detailed.entry.state);
        detailed.mem_bytes.push_back((long long)detailed.entry.mem);
        detailed.memory = floating_humanizer(detailed.entry.mem);

        if (detailed.first_mem == -1
                or detailed.first_mem < (long long)detailed.mem_bytes.back() / 2
                or detailed.first_mem > (long long)detailed.mem_bytes.back() * 4) {
            detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2,
                                     Mem::get_totalMem());
            redraw = true;
        }
        while (cmp_greater(detailed.mem_bytes.size(), (size_t)width))
            detailed.mem_bytes.pop_front();
    }

    auto collect(bool no_update) -> vector<proc_info>& {
        const auto& sorting       = Config::getS("proc_sorting");
        auto        rev           = Config::getB("proc_reversed");
        const auto& filter        = Config::getS("proc_filter");
        auto        per_core      = Config::getB("proc_per_core");
        auto        tree          = Config::getB("proc_tree");
        auto        show_detailed = Config::getB("show_detailed");
        auto        pause_list    = Config::getB("pause_proc_list");
        const size_t dpid         = (size_t)Config::getI("detailed_pid");

        bool should_filter  = (current_filter != filter);
        if (should_filter)  current_filter = filter;
        bool sorted_change    = (sorting != current_sort or rev != current_rev or should_filter);
        bool tree_mode_change = (tree != is_tree_mode);
        if (sorted_change)    { current_sort = sorting; current_rev = rev; }
        if (tree_mode_change) is_tree_mode = tree;

        const int cmult = per_core ? (int)Shared::coreCount : 1;
        bool got_detailed = false;

        static vector<size_t> found;
        uint64_t now_ms = time_ms();

        if (no_update and not current_procs.empty()) {
            if (show_detailed and dpid != detailed.last_pid)
                _collect_details(dpid, current_procs);
        } else {
            should_filter = true;
            found.clear();

            // Count runnable threads for load-avg EMA
            double runnable_count = 0.0;

            std::error_code dir_ec;
            for (const auto& dent : fs::directory_iterator("/proc", dir_ec)) {
                if (dir_ec) break;
                const auto& fname = dent.path().filename().string();
                if (fname.empty() or not std::isdigit((unsigned char)fname[0])) continue;

                size_t pid = 0;
                try { pid = std::stoul(fname); } catch (...) { continue; }
                if (pid < 1) continue;

                found.push_back(pid);

                bool no_cache = false;
                auto find_old = rng::find(current_procs, pid, &proc_info::pid);
                if (find_old == current_procs.end()) {
                    if (not pause_list) {
                        current_procs.push_back({pid});
                        find_old = current_procs.end() - 1;
                        no_cache = true;
                    } else continue;
                } else if (dead_procs.contains(pid)) continue;

                auto& np = *find_old;

                std::string as_path = "/proc/" + std::to_string(pid) + "/as";
                int as_fd = open(as_path.c_str(), O_RDONLY);
                if (as_fd < 0) {
                    // /proc/<pid>/as is not accessible (e.g. another user's process
                    // and we are not root).  We can still populate name/cmd from
                    // /proc/<pid>/cmdline which is world-readable on QNX.
                    if (no_cache) {
                        std::string cl = read_proc_file("/proc/" + std::to_string(pid) + "/cmdline");
                        for (auto& c : cl) if (c == '\0') c = ' ';
                        while (not cl.empty() and cl.back() == ' ') cl.pop_back();
                        if (not cl.empty()) {
                            // First NUL-separated token is argv[0]; use its basename as name.
                            std::string argv0 = cl.substr(0, cl.find(' '));
                            np.name = fs::path(argv0).filename().string();
                            np.cmd  = cl;
                            if (np.cmd.size() > 1000) { np.cmd.resize(1000); np.cmd.shrink_to_fit(); }
                        } else {
                            np.name = "[" + std::to_string(pid) + "]";
                            np.cmd  = np.name;
                        }
                    }
                    np.state = 'S';
                    continue;
                }

                procfs_info info{};
                if (devctl(as_fd, DCMD_PROC_INFO, &info, sizeof(info), nullptr) != EOK) {
                    close(as_fd); continue;
                }

                if (no_cache) {
                    // Name: use DCMD_PROC_MAPDEBUG_BASE on the already-open as_fd so
                    // that this works without root (reading /proc/<pid>/exefile fails
                    // for other users' processes when not root).
                    struct {
                        procfs_debuginfo hdr;
                        char             path[PATH_MAX];
                    } dbg{};
                    dbg.hdr.vaddr = info.base_address;
                    std::string exe_path;
                    if (devctl(as_fd, DCMD_PROC_MAPDEBUG_BASE, &dbg, sizeof(dbg), nullptr) == EOK
                            and dbg.hdr.path[0] != '\0') {
                        exe_path = dbg.hdr.path;
                    }
                    np.name = exe_path.empty()
                        ? "[" + std::to_string(pid) + "]"
                        : fs::path(exe_path).filename().string();

                    // Command line: read from /proc (may be empty if not root);
                    // fall back to the exe path obtained above via devctl.
                    {
                        std::string cl = read_proc_file("/proc/" + std::to_string(pid) + "/cmdline");
                        for (auto& c : cl) if (c == '\0') c = ' ';
                        while (not cl.empty() and cl.back() == ' ') cl.pop_back();
                        np.cmd = cl.empty() ? (exe_path.empty() ? np.name : exe_path) : cl;
                        if (np.cmd.size() > 1000) { np.cmd.resize(1000); np.cmd.shrink_to_fit(); }
                    }

                    np.ppid  = (uint64_t)info.parent;
                    np.cpu_s = (uint64_t)(boot_time_sec +
                                          (time_t)(info.start_time / 1'000'000'000ULL));

                    uid_t uid = (uid_t)info.uid;
                    std::string uid_str = std::to_string(uid);
                    if (not uid_user.contains(uid_str)) {
                        struct passwd* pw = getpwuid(uid);
                        uid_user[uid_str] = pw ? pw->pw_name : uid_str;
                    }
                    np.user = uid_user.at(uid_str);
                }

                np.threads = (size_t)info.num_threads;

                // State: read TID 1
                {
                    procfs_status st{};
                    st.tid = 1;
                    if (devctl(as_fd, DCMD_PROC_TIDSTATUS, &st, sizeof(st), nullptr) == EOK) {
                        np.state = qnx_state_char(st.state);
                        if (st.state == STATE_RUNNING or st.state == STATE_READY)
                            runnable_count += 1.0;
                    } else np.state = 'S';
                }

                // RSS memory from /proc/<pid>/vmstat
                {
                    std::string vs = read_proc_file("/proc/" + std::to_string(pid) + "/vmstat");
                    const char* p = std::strstr(vs.c_str(), "as_stats.rss=");
                    uint64_t rss = 0;
                    if (p) rss = std::strtoull(p + std::strlen("as_stats.rss="), nullptr, 0);
                    np.mem = rss * (uint64_t)Shared::page_size;
                }

                // CPU%
                {
                    uint64_t cpu_ns_now = info.utime + info.stime;
                    uint64_t cpu_ns_old = np.cpu_t; // repurposed: stores prev total ns
                    uint64_t delta_ns   = (cpu_ns_now >= cpu_ns_old) ? (cpu_ns_now - cpu_ns_old) : 0;

                    // Use the interval between the two most-recent process collection
                    // cycles.  cpu_collect_ms is set at the end of the *CPU* pass (which
                    // runs just before this), so (now_ms - cpu_collect_ms) would be only
                    // a few milliseconds and would wildly inflate cpu_p.
                    uint64_t int_ns = (proc_collect_ms > 0)
                        ? ((now_ms - proc_collect_ms) * 1'000'000ULL)
                        : 1'000'000'000ULL;
                    if (int_ns == 0) int_ns = 1'000'000'000ULL;

                    np.cpu_p = clamp((double)delta_ns / (double)int_ns * 100.0 * cmult,
                                     0.0, 100.0 * (double)Shared::coreCount);

                    double elapsed_s = max(1.0, (double)(time(nullptr) - (time_t)np.cpu_s));
                    np.cpu_c = ((double)(info.utime + info.stime) / 1e9) / elapsed_s;

                    // Store current cpu ns in cpu_t for next cycle delta
                    np.cpu_t = cpu_ns_now;
                }

                np.p_nice = 0;
                close(as_fd);

                if (show_detailed and not got_detailed and np.pid == dpid)
                    got_detailed = true;
            }

            // Update load average EMA
            update_load_avg(runnable_count);

            // Remove dead entries
            if (not pause_list) {
                auto rm = rng::remove_if(current_procs, [&](const auto& e) {
                    return not v_contains(found, e.pid);
                });
                current_procs.erase(rm.begin(), rm.end());
                dead_procs.clear();
            } else {
                for (auto& r : current_procs) {
                    if (rng::find(found, r.pid) == found.end()) {
                        if (r.state != 'X')
                            r.death_time = (uint64_t)(time(nullptr) - (time_t)r.cpu_s);
                        r.state = 'X';
                        dead_procs.emplace(r.pid);
                        if (not Config::getB("keep_dead_proc_usage")) {
                            r.cpu_p = 0.0; r.mem = 0;
                        }
                    }
                }
            }

            if (show_detailed and got_detailed)
                _collect_details(dpid, current_procs);
            else if (show_detailed and not got_detailed and detailed.status != "Dead") {
                detailed.status = "Dead"; redraw = true;
            }

            // Record when this collection cycle finished so the next cycle
            // can compute the correct wall-clock interval for cpu_p.
            proc_collect_ms = now_ms;
        }

        // Filter
        if (should_filter) {
            filter_found = 0;
            for (auto& p : current_procs) {
                if (not tree and not filter.empty()) {
                    p.filtered = not matches_filter(p, filter);
                    if (p.filtered) filter_found++;
                } else p.filtered = false;
            }
        }

        // Sort
        if (sorted_change or tree_mode_change or (not no_update and not pause_list))
            proc_sorter(current_procs, sorting, rev, tree);

        // Tree view
        if (tree and (not no_update or should_filter or sorted_change)) {
            bool locate_sel = false;

            if (toggle_children != -1) {
                auto col = rng::find(current_procs, (size_t)toggle_children, &proc_info::pid);
                if (col != current_procs.end()) {
                    for (auto& p : current_procs) {
                        if (p.ppid == col->pid) {
                            auto ch = rng::find(current_procs, p.pid, &proc_info::pid);
                            if (ch != current_procs.end())
                                ch->collapsed = not ch->collapsed;
                        }
                    }
                    if (Config::ints.at("proc_selected") > 0) locate_sel = true;
                }
                toggle_children = -1;
            }

            if (int fp = (collapse != -1 ? collapse : expand); fp != -1) {
                auto col = rng::find(current_procs, (size_t)fp, &proc_info::pid);
                if (col != current_procs.end()) {
                    if (collapse == expand)     col->collapsed = not col->collapsed;
                    else if (collapse > -1)     col->collapsed = true;
                    else if (expand > -1)       col->collapsed = false;
                    if (Config::ints.at("proc_selected") > 0) locate_sel = true;
                }
                collapse = expand = -1;
            }

            if (should_filter or not filter.empty()) filter_found = 0;

            vector<tree_proc> tree_procs;
            tree_procs.reserve(current_procs.size());

            if (not pause_list)
                for (auto& p : current_procs)
                    if (not v_contains(found, p.ppid) or p.ppid == p.pid) p.ppid = 0;

            rng::stable_sort(current_procs, rng::less{}, &proc_info::ppid);

            if (not current_procs.empty()) {
                for (auto& p : rng::equal_range(current_procs,
                                                 current_procs.at(0).ppid,
                                                 rng::less{}, &proc_info::ppid))
                    _tree_gen(p, current_procs, tree_procs, 0, false, filter,
                              false, no_update, should_filter);
            }

            int idx = 0;
            tree_sort(tree_procs, sorting, rev,
                      (pause_list and not (sorted_change or tree_mode_change)),
                      idx, (int)current_procs.size());

            for (auto t = tree_procs.begin(); t != tree_procs.end(); ++t)
                _collect_prefixes(*t, t == tree_procs.end() - 1);

            rng::stable_sort(current_procs, rng::less{}, &proc_info::tree_index);

            if (locate_sel) {
                int loc = (int)rng::find(current_procs, (size_t)Proc::selected_pid,
                                          &proc_info::pid)->tree_index;
                if (Config::ints.at("proc_start") >= loc or
                    Config::ints.at("proc_start") <= loc - Proc::select_max)
                    Config::ints.at("proc_start") = max(0, loc - 1);
                Config::ints.at("proc_selected") = loc - Config::ints.at("proc_start") + 1;
            }
        }

        numpids = (int)current_procs.size() - filter_found;
        return current_procs;
    }
} // namespace Proc
