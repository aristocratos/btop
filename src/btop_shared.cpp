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
#include <filesystem>
#include <fstream>
#include <ranges>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

	namespace {
		void set_tree_totals(tree_proc& proc) {
			auto& entry = proc.entry.get();
			proc.tree_threads = entry.threads;
			proc.tree_mem = entry.mem;
			proc.tree_cpu_p = entry.cpu_p;
			proc.tree_cpu_c = entry.cpu_c;
			// _tree_gen already folds a collapsed branch into its entry for display.
			// Re-adding its children here would make the sort total double-count them.
			if (entry.collapsed) return;

			for (auto& child : proc.children) {
				set_tree_totals(child);
				if (child.entry.get().state == 'X') continue;
				proc.tree_threads += child.tree_threads;
				proc.tree_mem += child.tree_mem;
				proc.tree_cpu_p += child.tree_cpu_p;
				proc.tree_cpu_c += child.tree_cpu_c;
			}
		}
	}

	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting, bool reverse, bool paused, int& c_index, const int index_max, bool collapsed) {
		const bool use_tree_totals = not Config::getB("proc_aggregate");
		if (use_tree_totals) {
			for (auto& proc : proc_vec) set_tree_totals(proc);
		}
		if (proc_vec.size() > 1 and not paused) {
			if (reverse) {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_threads < b.tree_threads : a.entry.get().threads < b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_mem < b.tree_mem : a.entry.get().mem < b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_cpu_p < b.tree_cpu_p : a.entry.get().cpu_p < b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_cpu_c < b.tree_cpu_c : a.entry.get().cpu_c < b.entry.get().cpu_c; });	break;
				}
			}
			else {
				switch (v_index(sort_vector, sorting)) {
				case 3: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_threads > b.tree_threads : a.entry.get().threads > b.entry.get().threads; });	break;
				case 5: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_mem > b.tree_mem : a.entry.get().mem > b.entry.get().mem; });	break;
				case 6: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_cpu_p > b.tree_cpu_p : a.entry.get().cpu_p > b.entry.get().cpu_p; });	break;
				case 7: rng::stable_sort(proc_vec, [use_tree_totals](const auto& a, const auto& b) { return use_tree_totals ? a.tree_cpu_c > b.tree_cpu_c : a.entry.get().cpu_c > b.entry.get().cpu_c; });	break;
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

	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
		int cur_depth, bool collapsed, const string& filter, bool found, bool no_update, bool should_filter) {
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not found and (should_filter or not filter.empty())) {
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
				filter_found++;
				p.filtered = true;
			}
			else if (Config::getB("proc_aggregate") and p.state != 'X') {
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

	namespace {
		std::unordered_map<string, bool> saved_tree_states;
		string loaded_tree_states;

		auto encode_tree_state_key(const string& key) -> string {
			static constexpr char hex[] = "0123456789abcdef";
			string encoded;
			encoded.reserve(key.size() * 2);
			for (const unsigned char c : key) {
				encoded += hex[c >> 4];
				encoded += hex[c & 0x0f];
			}
			return encoded;
		}

		auto decode_tree_state_key(const string& encoded) -> std::optional<string> {
			if (encoded.size() % 2 != 0) return std::nullopt;
			string key;
			key.reserve(encoded.size() / 2);
			for (size_t i = 0; i < encoded.size(); i += 2) {
				auto hex_value = [](const char c) -> int {
					if (c >= '0' and c <= '9') return c - '0';
					if (c >= 'a' and c <= 'f') return c - 'a' + 10;
					return -1;
				};
				const int high = hex_value(encoded.at(i));
				const int low = hex_value(encoded.at(i + 1));
				if (high < 0 or low < 0) return std::nullopt;
				key += static_cast<char>((high << 4) | low);
			}
			return key;
		}

		auto tree_state_key(const proc_info& process, const vector<proc_info>& processes) -> std::optional<string> {
			vector<string> ancestry;
			std::unordered_set<size_t> seen;
			const proc_info* current = &process;
			while (current != nullptr and seen.insert(current->pid).second) {
				ancestry.push_back(current->name);
				auto parent = rng::find(processes, current->ppid, &proc_info::pid);
				current = (parent == processes.end()) ? nullptr : &*parent;
			}
			if (ancestry.empty()) return std::nullopt;
			rng::reverse(ancestry);
			string key;
			for (const auto& name : ancestry) {
				if (not key.empty()) key += '\x1f';
				key += name;
			}
			return key;
		}

		void load_tree_states() {
			const auto& encoded = Config::getS("proc_tree_state");
			if (encoded == loaded_tree_states) return;
			saved_tree_states.clear();
			for (const auto& item : ssplit(encoded, ',')) {
				if (item.size() < 3 or item.at(1) != ':') continue;
				auto key = decode_tree_state_key(item.substr(2));
				if (key.has_value() and is_in(item.at(0), '0', '1'))
					saved_tree_states.insert_or_assign(*key, item.at(0) == '1');
			}
			loaded_tree_states = encoded;
		}

		void save_tree_states() {
			string encoded;
			for (const auto& [key, collapsed] : saved_tree_states) {
				if (not encoded.empty()) encoded += ',';
				encoded += collapsed ? "1:" : "0:";
				encoded += encode_tree_state_key(key);
			}
			loaded_tree_states = encoded;
			Config::set("proc_tree_state", encoded);
		}
	}

	void remember_tree_state(const vector<proc_info>& processes, const size_t pid, const bool collapsed) {
		if (not Config::getB("proc_tree_persist_state")) return;
		load_tree_states();
		const auto process = rng::find(processes, pid, &proc_info::pid);
		if (process == processes.end()) return;
		const auto key = tree_state_key(*process, processes);
		if (not key.has_value()) return;
		saved_tree_states.insert_or_assign(*key, collapsed);
		save_tree_states();
	}

	void remember_tree_children(const vector<proc_info>& processes, const size_t pid) {
		if (not Config::getB("proc_tree_persist_state")) return;
		load_tree_states();
		for (const auto& process : processes) {
			if (process.ppid != pid) continue;
			const auto key = tree_state_key(process, processes);
			if (key.has_value()) saved_tree_states.insert_or_assign(*key, process.collapsed);
		}
		save_tree_states();
	}

	void restore_tree_state(vector<proc_info>& processes) {
		if (not Config::getB("proc_tree_persist_state")) return;
		load_tree_states();
		for (auto& process : processes) {
			const auto key = tree_state_key(process, processes);
			if (key.has_value()) {
				if (const auto saved = saved_tree_states.find(*key); saved != saved_tree_states.end())
					process.collapsed = saved->second;
			}
		}
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
