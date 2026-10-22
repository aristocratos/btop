// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace Layout {

	struct HorizontalInput {
		int width = 0, height = 0, cores = 1;
		bool enabled = false, cpu = false, mem = false, net = false, proc = false;
		bool bottom = false, core_temps = false, mem_below_net = false;
		int inline_gpus = 0;
		std::vector<int> gpu_ids, gpu_offsets;
		int cpu_min_width = 60, gpu_min_width = 41;
		int cpu_min_height = 8, gpu_min_height = 8;
		int remaining_min_width = 0, mem_min_height = 10, net_min_height = 6, proc_min_height = 16;
	};

	struct HorizontalRow {
		int cpu_width, gpu_width, height, y, remaining_y, remaining_height;
		int columns, column_size, detail_width, detail_height;
		int mem_height, net_height, mem_y, net_y;
		int required_height;
	};

	// No cached visibility: callers can evaluate a proposed preset or box toggle.
	inline auto horizontal_row(const HorizontalInput& in) -> std::optional<HorizontalRow> {
		if (not in.enabled or not in.cpu or in.gpu_ids.size() != 1 or in.cores < 1)
			return {};
		const int gpu = in.gpu_ids.front();
		if (gpu < 0 or gpu >= static_cast<int>(in.gpu_offsets.size()) or in.width < in.remaining_min_width)
			return {};
		const int remaining_min = std::max(in.proc ? in.proc_min_height : 0,
			(in.mem ? in.mem_min_height : 0) + (in.net ? in.net_min_height : 0));
		std::optional<HorizontalRow> best;
		for (int cpu_width = in.cpu_min_width; cpu_width <= in.width - in.gpu_min_width; ++cpu_width) {
			const int gpu_width = in.width - cpu_width;
			const int gpu_detail_width = std::clamp(gpu_width / 2, in.gpu_min_width, 65);
			// Borders count toward panel widths. At least a third of the interior is history.
			const int gpu_interior = gpu_width - 2, cpu_interior = cpu_width - 2;
			if (gpu_interior - gpu_detail_width < (gpu_interior + 2) / 3) continue;
			const int detail_limit = cpu_interior - (cpu_interior + 2) / 3;
			for (int size = 0; size <= 2; ++size) {
				const int cell_width = size == 0 ? 8 + 6 * in.core_temps
					: size == 1 ? 15 + 6 * in.core_temps : 21 + 12 * in.core_temps;
				const int columns = std::min(std::max(2, in.cores), (detail_limit - 1) / (cell_width - (size > 0)));
				if (columns < 2) continue;
				// Inline GPU rows need room for memory, temperature and power as well as utilization.
				const int detail_width = std::max(in.inline_gpus > 0 ? 45 : 29,
					size == 0 ? cell_width * columns + 1 : (cell_width - 1) * columns + 1);
				if (detail_width > detail_limit) continue;
				const int detail_height = (in.cores + columns - 1) / columns + 4 + in.inline_gpus;
				const int required = std::max({in.cpu_min_height, in.gpu_min_height, detail_height + 2, in.gpu_offsets[gpu] + 4});
				if (required + remaining_min > in.height) continue;
				const int height = remaining_min == 0 ? in.height : required;
				// Minimize height, then distance from equal widths, then prefer richer core graphs.
				if (best and std::tuple{height, std::abs(2 * cpu_width - in.width), -size}
					>= std::tuple{best->height, std::abs(2 * best->cpu_width - in.width), -best->column_size}) continue;
				const int remaining = in.height - height;
				const int remaining_y = in.bottom ? 1 : height + 1;
				const int mem_height = not in.mem ? 0 : not in.net ? remaining
					: std::clamp(remaining * 40 / 68, in.mem_min_height, remaining - in.net_min_height);
				const int net_height = in.net ? remaining - mem_height : 0;
				best = HorizontalRow{
					.cpu_width = cpu_width, .gpu_width = gpu_width, .height = height,
					.y = in.bottom ? remaining + 1 : 1, .remaining_y = remaining_y, .remaining_height = remaining,
					.columns = columns, .column_size = size, .detail_width = detail_width, .detail_height = detail_height,
					.mem_height = mem_height, .net_height = net_height,
					.mem_y = remaining_y + (in.mem_below_net ? net_height : 0),
					.net_y = remaining_y + (in.mem_below_net ? 0 : mem_height), .required_height = required
				};
			}
		}
		return best;
	}

	// Adapter for the current configuration and detected hardware.
	auto horizontal_row(const std::string& boxes) -> std::optional<HorizontalRow>;
	auto visible_gpus(const std::string& boxes, int count) -> std::vector<int>;
}
