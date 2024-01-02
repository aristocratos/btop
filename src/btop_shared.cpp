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

#include <ranges>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace rng = std::ranges;
using namespace Tools;

#ifdef GPU_SUPPORT
namespace Gpu {
	vector<string> gpu_names;
	vector<int> gpu_b_height_offsets;
	std::unordered_map<string, deque<long long>> shared_gpu_percent = {
		{"gpu-average", {}},
		{"gpu-vram-total", {}},
		{"gpu-pwr-total", {}},
	};
	long long gpu_pwr_total_max;
}
#endif

namespace Proc {
	void proc_sorter(vector<proc_info>& proc_vec, const string& sorting, bool reverse, bool tree) {
		if (reverse) {
			switch (v_index(sort_vector, sorting)) {
			case 0: rng::stable_sort(proc_vec, rng::less{}, &proc_info::pid); 		break;
			case 1: rng::stable_sort(proc_vec, rng::less{}, &proc_info::name);		break;
			case 2: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cmd); 		break;
			case 3: rng::stable_sort(proc_vec, rng::less{}, &proc_info::threads);	break;
			case 4: rng::stable_sort(proc_vec, rng::less{}, &proc_info::user);		break;
			case 5: rng::stable_sort(proc_vec, rng::less{}, &proc_info::mem); 		break;
			case 6: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_p);		break;
			case 7: rng::stable_sort(proc_vec, rng::less{}, &proc_info::cpu_c);		break;
			}
		}
		else {
			switch (v_index(sort_vector, sorting)) {
			case 0: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::pid); 		break;
			case 1: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::name);		break;
			case 2: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::cmd); 		break;
			case 3: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::threads);	break;
			case 4: rng::stable_sort(proc_vec, rng::greater{}, &proc_info::user); 		break;
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

	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting, bool reverse, int& c_index, const int index_max, bool collapsed) {
		if (proc_vec.size() > 1) {
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
				tree_sort(r.children, sorting, reverse, c_index, (collapsed or r.entry.get().collapsed or r.entry.get().tree_index == (size_t)index_max));
			}
		}
	}

	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
		int cur_depth, bool collapsed, const string& filter, bool found, bool no_update, bool should_filter) {
		auto cur_pos = out_procs.size();
		bool filtering = false;

		//? If filtering, include children of matching processes
		if (not found and (should_filter or not filter.empty())) {
			if (not s_contains(std::to_string(cur_proc.pid), filter)
			and not s_contains_ic(cur_proc.name, filter)
			and not s_contains_ic(cur_proc.cmd, filter)
			and not s_contains_ic(cur_proc.user, filter)) {
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
				cur_proc.cpu_p += p.cpu_p;
				cur_proc.cpu_c += p.cpu_c;
				cur_proc.mem += p.mem;
				cur_proc.threads += p.threads;
				filter_found++;
				p.filtered = true;
			}
			else if (Config::getB("proc_aggregate")) {
				cur_proc.cpu_p += p.cpu_p;
				cur_proc.cpu_c += p.cpu_c;
				cur_proc.mem += p.mem;
				cur_proc.threads += p.threads;
			}
		}
		if (collapsed or filtering) {
			return;
		}

		//? Add tree terminator symbol if it's the last child in a sub-tree
		if (out_procs.back().children.size() > 0 and out_procs.back().children.back().entry.get().prefix.size() >= 8 and not out_procs.back().children.back().entry.get().prefix.ends_with("]─"))
			out_procs.back().children.back().entry.get().prefix.replace(out_procs.back().children.back().entry.get().prefix.size() - 8, 8, " └─ ");

		//? Add collapse/expand symbols if process have any children
		out_procs.at(cur_pos).entry.get().prefix = " │ "s * cur_depth + (out_procs.at(cur_pos).children.size() > 0 ? (cur_proc.collapsed ? "[+]─" : "[-]─") : " ├─ ");

	}

}
