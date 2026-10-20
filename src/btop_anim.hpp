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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

//* Pixelated animations: a CPU die with per core number crunching and a DDR5 RAM stick with a stream of addresses.
//* The box draw functions register their region and data with `*_set()` and print `*_render()`,
//* the main loop calls `frame()` at `frame_ms` intervals to animate in between data updates.
namespace Anim {
	//? Milliseconds between animation frames
	constexpr uint64_t frame_ms = 90;

	//? True if a frame tick is needed, i.e. an animation is enabled and on screen
	bool active();

	//? Forget all registered regions, called when the box layout is recalculated
	void reset();

	//? Config toggles (animations are never used in tty mode)
	bool cpu_enabled();
	bool mem_enabled();

	//* Registers the cpu region (absolute terminal coordinates) and its data.
	//* `temp_c` < 0 means no sensor available, heat is then estimated from usage.
	//* Returns false (and stays unregistered) if the region is too small.
	bool cpu_set(int x, int y, int width, int height, const std::vector<int>& core_percent, double temp_c);
	void cpu_clear();
	std::string cpu_render();

	//* Registers the memory stick region and used memory percent. Returns false if the region is too small.
	bool mem_set(int x, int y, int width, int height, int used_percent);
	void mem_clear();
	std::string mem_render();

	//* Advances the animation one frame and renders everything registered
	std::string frame();
}
