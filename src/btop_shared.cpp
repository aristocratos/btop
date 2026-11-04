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

#include <sys/resource.h>
#ifdef __linux__
	#include <sys/socket.h>
	#include <sys/un.h>
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace fs = std::filesystem;
namespace rng = std::ranges;
using namespace Tools;

namespace Cpu {
    std::optional<std::string> container_engine;

	string trim_name(string name) {
		auto name_vec = ssplit(name);

		if ((name.contains("Xeon") or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else if (v_contains(name_vec, "Ryzen"s)) {
			auto ryz_pos = v_index(name_vec, "Ryzen"s);
			name = "Ryzen";
			int tokens = 0;
			for (auto i = ryz_pos + 1; i < name_vec.size() && tokens < 2; i++) {
				const std::string& p = name_vec.at(i);
				if (p != "AI" && p != "PRO" && p != "H" && p != "HX")
					tokens++;
				name += " " + p;
			}
		} else if (name.contains("Intel") and v_contains(name_vec, "CPU"s)) {
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
			for (const auto& replace : {"Processor", "CPU", "(R)", "(TM)", "Intel", "AMD", "Apple", "Core"}) {
				name = s_replace(name, replace, "");
				name = s_replace(name, "  ", " ");
			}
			name = trim(name);
		}

		return name;
	}
}

#ifdef GPU_SUPPORT
namespace Gpu {
	vector<string> gpu_names;
	vector<int> gpu_b_height_offsets;
	std::unordered_map<string, deque<long long>> shared_gpu_percent = {
		{"gpu-average", {}},
		{"gpu-vram-total", {}},
		{"gpu-pwr-total", {}},
	};
	long long gpu_pwr_total_max = 0;
}
#endif

namespace Proc {
bool set_priority(pid_t pid, int priority) {
  if (setpriority(PRIO_PROCESS, pid, priority) == 0) {
    return true;
  }
  return false;
}

	void proc_sorter(vector<proc_info>& proc_vec, const string& sorting, bool reverse, bool tree) {
		if (reverse) {
			switch (v_index(sort_vector, sorting)) {
			case 0: rng::stable_sort(proc_vec, rng::less{}, &proc_info::pid); 		break;
			case 1: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::name);		break;
			case 2: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cmd); 		break;
			case 3: rng::stable_sort(proc_vec, rng::less{}, &proc_info::threads);	break;
			case 4: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::user); 		break;
			case 5: rng::stable_sort(proc_vec, rng::less{}, &proc_info::mem); 		break;
			case 6: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_p);		break;
			case 7: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_c);		break;
			}
		}
		else {
			switch (v_index(sort_vector, sorting)) {
			case 0: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::pid); 		break;
			case 1: rng::stable_sort(proc_vec, rng::less{}, &proc_info::name);		break;
			case 2: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cmd); 		break;
			case 3: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::threads);	break;
			case 4: rng::stable_sort(proc_vec, rng::less{}, &proc_info::user);		break;
			case 5: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::mem); 		break;
			case 6: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cpu_p);   	break;
			case 7: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cpu_c);   	break;
			}
		}

		//* When sorting with "cpu lazy" push processes over threshold cpu usage to the front regardless of cumulative usage
		if (not tree and not reverse and sorting == "cpu lazy") {
			double max = 10.0, target = 30.0;
			for (size_t i = 0, x = 0, offset = 0; i < proc_vec.size(); i++) {
				if (i <= 5 and proc_vec.at(i).cpu_p > max)
					max = proc_vec.at(i).cpu_p;
				else if (i == 6)
					target = (max > 30.0) ? max : 10.0;
				if (i == offset and proc_vec.at(i).cpu_p > 30.0)
					offset++;
				else if (proc_vec.at(i).cpu_p > target) {
					rotate(proc_vec.begin() + offset, proc_vec.begin() + i, proc_vec.begin() + i + 1);
					if (++x > 10) break;
				}
			}
		}
	}

	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting, bool reverse, bool paused, int& c_index, const int index_max, bool collapsed) {
		if (proc_vec.size() > 1 and not paused) {
			if (reverse) {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().threads < b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().mem < b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_p < b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_c < b.entry.get().cpu_c; });	break;
				}
			}
			else {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().threads > b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().mem > b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_p > b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [](const auto& a, const auto& b) { return a.entry.get().cpu_c > b.entry.get().cpu_c; });	break;
				}
			}
		}

		for (auto& r : proc_vec) {
			r.entry.get().tree_index = (collapsed or r.entry.get().filtered ? index_max : c_index++);
			if (not r.children.empty()) {
				tree_sort(r.children, sorting, reverse, paused, c_index, (collapsed or r.entry.get().collapsed or r.entry.get().tree_index == (size_t)index_max));
			}
		}
	}

	auto matches_filter(const proc_info& proc, const std::string& filter) -> bool {
		if (filter.starts_with("!")) {
			if (filter.size() == 1) {
				return true;
			}

			// An incomplete regex throws, see issue https://github.com/aristocratos/btop/issues/1133
			try {
				std::regex regex { filter.substr(1), std::regex::extended };
				return std::regex_search(std::to_string(proc.pid), regex) || std::regex_search(proc.name, regex) ||
							 std::regex_match(proc.cmd, regex) || std::regex_search(proc.user, regex);
			} catch (std::regex_error& /* unused */) {
				return false;
			}
		}

		return std::to_string(proc.pid).contains(filter) || s_contains_ic(proc.name, filter) ||
					 s_contains_ic(proc.cmd, filter) || s_contains_ic(proc.user, filter);
	}

	auto ctr_hidden(const proc_info& proc) -> bool {
		//? Only show processes of the container selected in the container box
		if (not Ctr::selected.empty()) return proc.container != Ctr::selected;
		return not proc.container.empty() and Config::getB("proc_filter_containers");
	}

	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
		int cur_depth, bool collapsed, const string& filter, bool found, bool no_update, bool should_filter) {
		bool filtering = false;

		//? Processes in containers are hidden even if a parent matches the filter
		if (ctr_hidden(cur_proc)) {
			filtering = true;
			cur_proc.filtered = true;
			//? filter_found is only reset by the caller under this condition
			if (should_filter or not filter.empty()) filter_found++;
		}
		//? If filtering, include children of matching processes
		else if (not found and (should_filter or not filter.empty())) {
			if (!matches_filter(cur_proc, filter)) {
				filtering = true;
				cur_proc.filtered = true;
				filter_found++;
			}
			else {
				found = true;
				cur_depth = 0;
			}
		}
		else if (cur_proc.filtered) cur_proc.filtered = false;

		cur_proc.depth = cur_depth;

		//? Set tree index position for process if not filtered out or currently in a collapsed sub-tree
		out_procs.push_back({ cur_proc, {} });
		if (not collapsed and not filtering) {
			cur_proc.tree_index = out_procs.size() - 1;

			//? Try to find name of the binary file and append to program name if not the same
			if (cur_proc.short_cmd.empty() and not cur_proc.cmd.empty()) {
				std::string_view cmd_view = cur_proc.cmd;
				cmd_view = cmd_view.substr((size_t)0, std::min(cmd_view.find(' '), cmd_view.size()));
				cmd_view = cmd_view.substr(std::min(cmd_view.find_last_of('/') + 1, cmd_view.size()));
				cur_proc.short_cmd = string{cmd_view};
			}
		}
		else {
			cur_proc.tree_index = in_procs.size();
		}

		//? Recursive iteration over all children
		for (auto& p : rng::equal_range(in_procs, cur_proc.pid, rng::less{}, &proc_info::ppid)) {
			if (collapsed and not filtering) {
				cur_proc.filtered = true;
			}

			_tree_gen(p, in_procs, out_procs.back().children, cur_depth + 1, (collapsed or cur_proc.collapsed), filter, found, no_update, should_filter);

			if (not no_update and not filtering and (collapsed or cur_proc.collapsed)) {
				//auto& parent = cur_proc;
				if (p.state != 'X') {
					cur_proc.cpu_p += p.cpu_p;
					cur_proc.cpu_c += p.cpu_c;
					cur_proc.mem += p.mem;
					cur_proc.threads += p.threads;
				}
				//? Processes hidden for being in a container are already counted
				if (not ctr_hidden(p)) filter_found++;
				p.filtered = true;
			}
			else if (not no_update and Config::getB("proc_aggregate") and p.state != 'X') {
				cur_proc.cpu_p += p.cpu_p;
				cur_proc.cpu_c += p.cpu_c;
				cur_proc.mem += p.mem;
				cur_proc.threads += p.threads;
			}
		}
	}

	void _collect_prefixes(tree_proc &t, const bool is_last, const string &header) {
		const bool is_filtered = t.entry.get().filtered;
		if (is_filtered) t.entry.get().depth = 0;

		if (!t.children.empty()) t.entry.get().prefix = header + (t.entry.get().collapsed ? "[+]─": "[-]─");
		else t.entry.get().prefix = header + (is_last ? " └─": " ├─");

		for (auto child = t.children.begin(); child != t.children.end(); ++child) {
			_collect_prefixes(*child, child == (t.children.end() - 1),
				is_filtered ? "": header + (is_last ? "   ": " │ "));
		}
	}

	void toggle_tree_collapse(std::vector<proc_info>& current_procs) {
		//? Build sets of all pids and parent pids to identify root processes
		std::unordered_set<size_t> pid_set, parent_pids;
		for (const auto& p : current_procs) {
			pid_set.insert(p.pid);
			parent_pids.insert(static_cast<size_t>(p.ppid));
		}
		//? If any non-root parent is expanded, collapse; otherwise expand
		const bool do_collapse = rng::any_of(current_procs, [&parent_pids, &pid_set](const proc_info& p) {
			return parent_pids.contains(p.pid)
				and pid_set.contains(static_cast<size_t>(p.ppid))
				and not p.collapsed;
		});
		//? Root processes (parent not in tracked list) are never touched
		for (auto& p : current_procs) {
			if (not pid_set.contains(static_cast<size_t>(p.ppid))) continue;
			p.collapsed = do_collapse;
		}
	}

	void _auto_collapse_oversized(std::vector<proc_info>& current_procs, const bool tree_mode_change) {
		//? Only act when the user just switched into tree view
		const int threshold = Config::getI("proc_tree_auto_collapse");
		if (threshold <= 0 or not tree_mode_change) return;
		//? Never collapse the root process or its direct children, only deeper busy parents
		const size_t root_ppid = static_cast<size_t>(current_procs.at(0).ppid);
		std::unordered_set<size_t> root_pids;
		for (const auto& p : current_procs) {
			if (static_cast<size_t>(p.ppid) == root_ppid) root_pids.insert(p.pid);
		}
		for (auto& p : current_procs) {
			if (static_cast<size_t>(p.ppid) == root_ppid or root_pids.contains(static_cast<size_t>(p.ppid))) continue;
			if (rng::count(current_procs, p.pid, &proc_info::ppid) >= threshold) {
				p.collapsed = true;
			}
		}
	}
}

namespace Ctr {
	auto parse_cgroup(const std::string_view cgroup) -> std::optional<ctr_info> {
		const auto is_id = [](const std::string_view str) {
			return str.size() == 64 and rng::all_of(str, [](unsigned char c) { return std::isxdigit(c); });
		};

		//? Iterate from the root so nested containers are attributed to the outermost one
		std::string_view prev;
		for (size_t start = 0; start < cgroup.size();) {
			const size_t end = std::min(cgroup.find('/', start), cgroup.size());
			const auto part = cgroup.substr(start, end - start);
			const auto path = cgroup.substr(0, end);
			start = end + 1;

			const bool scope = part.ends_with(".scope");
			const auto stem = part.substr(0, part.size() - (scope ? 6 : 0));

			//? LXC and Incus: lxc.payload.<name> or lxc/<name> (Proxmox)
			if (stem.starts_with("lxc.payload.") and stem.size() > 12)
				return ctr_info{"lxc", string{stem.substr(12)}, string{path}};
			if (prev == "lxc" and not part.empty())
				return ctr_info{"lxc", string{part}, string{path}};

			//? systemd-nspawn and other systemd-machined containers: machine-<name>.scope, qemu machines are not containers
			if (scope and stem.starts_with("machine-") and stem.size() > 8 and not stem.starts_with("machine-qemu"))
				return ctr_info{"nspawn", s_replace(string{stem.substr(8)}, "\\x2d", "-"), string{path}};

			//? OCI runtimes (docker, podman, containerd, cri-o...): [<engine>-]<64 hex id>[.scope]
			if (stem.size() >= 64 and is_id(stem.substr(stem.size() - 64)) and (stem.size() == 64 or stem.at(stem.size() - 65) == '-')) {
				const auto engine = stem.substr(0, stem.size() - std::min<size_t>(stem.size(), 65));
				//? conmon monitors the container from the outside
				if (not engine.ends_with("conmon")) {
					return ctr_info{
						string{cgroup.contains("kubepods") ? "k8s" : engine == "libpod" ? "podman"
							: not engine.empty() ? engine : prev == "docker" ? "docker" : "container"},
						string{stem.substr(stem.size() - 64, 12)},
						string{path}
					};
				}
			}

			prev = part;
		}
		return std::nullopt;
	}

	auto docker_name(const std::string_view response, const std::string_view id) -> string {
		//? Containers are listed as {"Id":"<64 hex id>","Names":["/<name>",...
		static constexpr std::string_view id_key = "\"Id\":\"", names_key = "\",\"Names\":[\"/";
		auto pos = response.find(string{id_key} + string{id});
		if (pos == std::string_view::npos) return "";
		pos += id_key.size() + 64;
		if (pos + names_key.size() > response.size() or response.substr(pos, names_key.size()) != names_key) return "";
		pos += names_key.size();
		const auto name = response.substr(pos, response.find('"', pos) - pos);
		//? Only accept the characters docker allows in a name, the name is printed to the terminal
		const auto valid = [](unsigned char c) { return std::isalnum(c) or c == '_' or c == '.' or c == '-'; };
		return (name.empty() or not rng::all_of(name, valid)) ? "" : string{name};
	}

#ifdef __linux__
	//* Get list of running containers from the docker engine api, empty if the docker socket isn't accessible
	static auto docker_containers() -> string {
		string path = "/var/run/docker.sock";
		if (const char* host = getenv("DOCKER_HOST"); host != nullptr and std::string_view{host}.starts_with("unix://")) path = host + 7;

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		if (path.size() >= sizeof(addr.sun_path)) return "";
		path.copy(addr.sun_path, path.size());

		const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (fd < 0) return "";

		//? Don't hang the runner thread if the docker daemon doesn't answer
		const timeval timeout{1, 0};
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

		//? HTTP/1.0 to get the response unchunked and the connection closed when done
		static constexpr std::string_view request = "GET /containers/json HTTP/1.0\r\nHost: docker\r\n\r\n";
		string response;
		if (connect(fd, (const sockaddr*)&addr, sizeof(addr)) == 0
		and send(fd, request.data(), request.size(), MSG_NOSIGNAL) == (ssize_t)request.size()) {
			char buf[4096];
			for (ssize_t count; (count = recv(fd, buf, sizeof(buf), 0)) > 0;) response.append(buf, count);
		}
		close(fd);
		return response;
	}
#endif

	vector<ctr_info> current_ctrs;
	string selected;

	void collect(const vector<Proc::proc_info>& procs) {
		static uint64_t old_time{};
		const uint64_t now = time_micros();
		const auto per_core = Config::getB("proc_per_core");

		//? Group processes by container, sum of process values is used for containers without readable cgroup stats
		for (auto& c : current_ctrs) {
			c.procs = 0;
			c.cpu_p = 0;
			c.mem = c.mem_limit = 0;
		}
		[[maybe_unused]] bool new_docker = false;
		for (const auto& p : procs) {
			if (p.container.empty() or p.state == 'X') continue;
			auto c = rng::find(current_ctrs, p.container, &ctr_info::path);
			if (c == current_ctrs.end()) {
				auto new_ctr = parse_cgroup(p.container);
				if (not new_ctr) continue;
				if (new_ctr->engine == "docker") new_docker = true;
				current_ctrs.push_back(std::move(*new_ctr));
				c = current_ctrs.end() - 1;
			}
			c->procs++;
			c->cpu_p += p.cpu_p;
			c->mem += p.mem;
		}
		std::erase_if(current_ctrs, [](const auto& c) { return c.procs == 0; });

	#ifdef __linux__
		//? The cgroup only has the id of docker containers, ask docker for the names when a new container shows up
		if (new_docker) {
			const auto response = docker_containers();
			for (auto& c : current_ctrs) {
				//? Name is still the short id from the cgroup path if not found earlier
				if (c.engine != "docker" or not c.path.contains(c.name)) continue;
				if (auto name = docker_name(response, c.name); not name.empty()) c.name = std::move(name);
			}
		}
	#endif

		rng::sort(current_ctrs, rng::less{}, &ctr_info::name);
		if (rng::find(current_ctrs, selected, &ctr_info::path) == current_ctrs.end()) selected.clear();

		for (auto& c : current_ctrs) {
			//? Cgroup v2 stats, same values as reported by the container engines
			const fs::path cgroup = "/sys/fs/cgroup" + c.path;
			string key;
			uint64_t val{};

			if (std::ifstream cpu_stat{cgroup / "cpu.stat"}; cpu_stat >> key >> val and key == "usage_usec") {
				if (c.cpu_t > 0 and val >= c.cpu_t and now > old_time)
					c.cpu_p = 100.0 * (val - c.cpu_t) / (now - old_time) / (per_core ? 1 : Shared::coreCount);
				c.cpu_t = val;
			}

			if (std::ifstream mem_current{cgroup / "memory.current"}; mem_current >> val) {
				c.mem = val;
				//? Reclaimable file cache is not counted as used
				for (std::ifstream mem_stat{cgroup / "memory.stat"}; mem_stat >> key >> val;) {
					if (key == "inactive_file") {
						c.mem -= std::min(c.mem, val);
						break;
					}
				}
			}

			//? Contains "max" if unlimited
			if (std::ifstream mem_max{cgroup / "memory.max"}; mem_max >> val) c.mem_limit = val;

			//? Cpu graph is in percent of total available cpu power
			c.cpu_percent.push_back(std::clamp(std::llround(c.cpu_p / (per_core ? Shared::coreCount : 1)), 0ll, 100ll));
			while (std::cmp_greater(c.cpu_percent.size(), Term::width.load())) c.cpu_percent.pop_front();
		}

		old_time = now;
	}
}

auto detect_container() -> std::optional<std::string> {
    std::error_code err;

    if (fs::exists(fs::path("/run/.containerenv"), err)) {
        return std::make_optional(std::string { "podman" });
    }
    if (fs::exists(fs::path("/.dockerenv"), err)) {
        return std::make_optional(std::string { "docker" });
    }
    auto systemd_container = fs::path("/run/systemd/container");
    if (fs::exists(systemd_container, err)) {
        auto stream = std::ifstream { systemd_container };
        auto buf = std::string {};
        stream >> buf;
        return std::make_optional(buf);
    }

    return std::nullopt;
}

#if defined(GPU_SUPPORT)
const array<string, 2> Gpu::mem_names { "used", "free" };
#endif

const vector<string> Proc::sort_vector = {
	"pid",
	"name",
	"command",
	"threads",
	"user",
	"memory",
	"cpu direct",
	"cpu lazy",
};

const std::unordered_map<char, string> Proc::proc_states = {
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
