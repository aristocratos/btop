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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <random>
#include <string_view>

#include <fmt/format.h>

#include "btop_anim.hpp"
#include "btop_config.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

using std::string;
using std::string_view;
using std::vector;

//* Everything is drawn on a small canvas made of "pixels" (two per terminal cell, using half blocks)
//* with a glyph layer on top (digits, addresses, flames). The canvas is serialised with as few
//* colour escape codes as possible.
namespace Anim {

	namespace {
		struct RGB {
			int r = 0, g = 0, b = 0;
			bool operator==(const RGB&) const = default;
		};

		constexpr RGB orange{0xd1, 0x7b, 0x13};
		constexpr RGB blue{0x1e, 0x7c, 0xc9};
		constexpr RGB red{0xd1, 0x33, 0x13};
		constexpr RGB white{255, 255, 255};

		constexpr string_view hex_digits = "0123456789ABCDEF";

		//? Temperatures (celsius) where the cpu animation is fully cool and fully hot, flames start above 40% heat
		constexpr double cool_temp = 48.0;
		constexpr double hot_temp = 78.0;
		constexpr double flame_start_heat = 0.4;
		constexpr size_t max_flames = 220;

		RGB lerp(const RGB& from, const RGB& to, double t) {
			t = std::clamp(t, 0.0, 1.0);
			return {static_cast<int>(from.r + (to.r - from.r) * t), static_cast<int>(from.g + (to.g - from.g) * t),
					static_cast<int>(from.b + (to.b - from.b) * t)};
		}

		RGB scale(const RGB& color, double factor) {
			return {std::clamp(static_cast<int>(color.r * factor), 0, 255), std::clamp(static_cast<int>(color.g * factor), 0, 255),
					std::clamp(static_cast<int>(color.b * factor), 0, 255)};
		}

		//? Small integer hash, gives the "random but stable" digits and addresses
		uint32_t hash(uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0) {
			uint32_t h = a * 0x9e3779b1u + 0x7f4a7c15u;
			h = (h ^ (h >> 15)) * 0x85ebca6bu ^ (b + 0x165667b1u);
			h = (h ^ (h >> 13)) * 0xc2b2ae35u ^ (c + 0x27d4eb2fu);
			h = (h ^ (h >> 16)) * 0x85ebca6bu ^ (d + 0x9e3779b9u);
			h ^= h >> 15;
			h *= 0x2c1b3c6du;
			h ^= h >> 12;
			return h;
		}

		//? Share of a hash in the range [0, 1)
		double hash_unit(uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0) {
			return (hash(a, b, c, d) % 1000) / 1000.0;
		}

		//? Character cell canvas with two pixels per cell (top and bottom half) and a glyph layer
		class Canvas {
			int width, height;
			vector<RGB> pixels;
			vector<bool> pixel_set;
			vector<string> glyphs;
			vector<RGB> glyph_colors;

		public:
			Canvas(int width, int height)
				: width(width), height(height), pixels(width * height * 2), pixel_set(width * height * 2, false),
				  glyphs(width * height), glyph_colors(width * height) {}

			void pixel(int x, int y, const RGB& color) {
				if (x < 0 or y < 0 or x >= width or y >= height * 2) return;
				pixels[y * width + x] = color;
				pixel_set[y * width + x] = true;
			}

			void clear_pixel(int x, int y) {
				if (x < 0 or y < 0 or x >= width or y >= height * 2) return;
				pixel_set[y * width + x] = false;
			}

			void rect(int x0, int y0, int x1, int y1, const RGB& color) {
				for (int y = y0; y <= y1; y++) {
					for (int x = x0; x <= x1; x++) pixel(x, y, color);
				}
			}

			void glyph(int x, int y, string_view text, const RGB& color) {
				if (x < 0 or y < 0 or x >= width or y >= height) return;
				glyphs[y * width + x] = text;
				glyph_colors[y * width + x] = color;
			}

			//? Draw an ASCII string, one glyph per cell
			void print(int x, int y, string_view text, const RGB& color) {
				for (const auto& ch : text) glyph(x++, y, string_view(&ch, 1), color);
			}

			//? Serialise the canvas, `x0` and `y0` is the terminal position of the top left cell
			string render(int x0, int y0) const {
				const bool low_color = Config::getB("lowcolor");
				string out;
				out.reserve(width * height * 12);
				RGB current_fg, current_bg;
				bool fg_default = true, bg_default = true;

				auto set_fg = [&](const RGB& color) {
					if (not fg_default and color == current_fg) return;
					out += Theme::dec_to_color(color.r, color.g, color.b, low_color, "fg");
					current_fg = color;
					fg_default = false;
				};
				auto set_bg = [&](const RGB& color) {
					if (not bg_default and color == current_bg) return;
					out += Theme::dec_to_color(color.r, color.g, color.b, low_color, "bg");
					current_bg = color;
					bg_default = false;
				};
				auto default_bg = [&]() {
					if (bg_default) return;
					out += "\033[49m";
					bg_default = true;
				};

				for (int row = 0; row < height; row++) {
					out += Mv::to(y0 + row, x0);
					for (int col = 0; col < width; col++) {
						const int top = (row * 2) * width + col, bottom = (row * 2 + 1) * width + col;
						const bool has_top = pixel_set[top], has_bottom = pixel_set[bottom];
						const auto& text = glyphs[row * width + col];

						if (not text.empty()) {
							if (has_top and has_bottom) set_bg(lerp(pixels[top], pixels[bottom], 0.5));
							else if (has_top) set_bg(pixels[top]);
							else if (has_bottom) set_bg(pixels[bottom]);
							else default_bg();
							set_fg(glyph_colors[row * width + col]);
							out += text;
						}
						else if (has_top and has_bottom) {
							if (pixels[top] == pixels[bottom]) {
								set_bg(pixels[top]);
								out += ' ';
							}
							else {
								set_fg(pixels[top]);
								set_bg(pixels[bottom]);
								out += "▀";
							}
						}
						else if (has_top) {
							default_bg();
							set_fg(pixels[top]);
							out += "▀";
						}
						else if (has_bottom) {
							default_bg();
							set_fg(pixels[bottom]);
							out += "▄";
						}
						else {
							default_bg();
							out += ' ';
						}
					}
				}
				return out + Fx::reset;
			}
		};

		uint64_t tick = 0;
		std::mt19937 rng{std::random_device{}()};

		double random_unit() {
			return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
		}

		//* ------------------------------------------------ CPU ------------------------------------------------
		//? Set by the runner thread when drawing, read by the main loop to decide if frames are needed
		std::atomic<bool> cpu_valid = false;
		std::atomic<bool> mem_valid = false;

		struct CpuState {
			int x = 0, y = 0, width = 0, height = 0;
			vector<int> core_percent;
			double temp_c = -1;
		} cpu;

		struct Flame {
			float x, y, speed_x, speed_y;
			int age, life;
		};
		vector<Flame> flames;

		double average_usage(const CpuState& state) {
			if (state.core_percent.empty()) return 0;
			return std::accumulate(state.core_percent.begin(), state.core_percent.end(), 0.0) / state.core_percent.size();
		}

		//? 0 = cool, 1 = hot. Without temp sensors the heat is estimated from the average usage.
		double heat_of(const CpuState& state) {
			const double temp = state.temp_c >= 0 ? state.temp_c : 40.0 + 0.4 * average_usage(state);
			return std::clamp((temp - cool_temp) / (hot_temp - cool_temp), 0.0, 1.0);
		}

		//? Cell layout of the cpu region: flame margins around a package (pin ring) holding the die
		struct CpuLayout {
			int flame_rows, package_x0, package_y0, package_x1, package_y1;
			int die_x0, die_y0, die_x1, die_y1, die_width, die_height;
		};

		CpuLayout cpu_layout(int width, int height) {
			CpuLayout layout{};
			layout.flame_rows = height >= 9 ? 2 : (height >= 7 ? 1 : 0);
			layout.package_x0 = 1;
			layout.package_x1 = width - 2;
			layout.package_y0 = layout.flame_rows;
			layout.package_y1 = height - 1;
			layout.die_x0 = layout.package_x0 + 1;
			layout.die_x1 = layout.package_x1 - 1;
			layout.die_y0 = layout.package_y0 + 1;
			layout.die_y1 = layout.package_y1 - 1;
			layout.die_width = layout.die_x1 - layout.die_x0 + 1;
			layout.die_height = layout.die_y1 - layout.die_y0 + 1;
			return layout;
		}

		//? Move the flames one step and spawn new ones depending on heat
		void update_flames() {
			if (not cpu_valid) {
				flames.clear();
				return;
			}
			const auto layout = cpu_layout(cpu.width, cpu.height);

			for (auto& flame : flames) {
				flame.x += flame.speed_x;
				flame.y += flame.speed_y;
				flame.speed_x += static_cast<float>((random_unit() - 0.5) * 0.25);
				flame.age++;
			}
			std::erase_if(flames, [&](const Flame& flame) {
				return flame.age >= flame.life or flame.y < -0.5f or flame.x < -0.5f or flame.x > cpu.width - 0.5f;
			});

			const double intensity = std::clamp((heat_of(cpu) - flame_start_heat) / (1.0 - flame_start_heat), 0.0, 1.0);
			if (intensity <= 0.01) return;

			const double expected = intensity * (cpu.width / 12.0) + 0.4;
			const int spawn_count = static_cast<int>(expected) + (random_unit() < expected - std::floor(expected) ? 1 : 0);
			for (int i = 0; i < spawn_count and flames.size() < max_flames; i++) {
				Flame flame{};
				if (random_unit() < 0.7 or layout.flame_rows == 0) {
					//? Rise from the top edge of the package
					flame.x = static_cast<float>(layout.package_x0 + random_unit() * (layout.package_x1 - layout.package_x0));
					flame.y = static_cast<float>(layout.package_y0 + 0.4);
					flame.speed_x = static_cast<float>((random_unit() - 0.5) * 0.25);
					flame.speed_y = static_cast<float>(-(0.25 + random_unit() * 0.35));
				}
				else {
					//? Lick out of the sides
					const bool left = random_unit() < 0.5;
					flame.x = static_cast<float>(left ? layout.package_x0 : layout.package_x1);
					flame.y = static_cast<float>(layout.package_y0 + random_unit() * (layout.package_y1 - layout.package_y0));
					flame.speed_x = static_cast<float>((left ? -1 : 1) * (0.1 + random_unit() * 0.2));
					flame.speed_y = static_cast<float>(-(0.1 + random_unit() * 0.25));
				}
				flame.life = 4 + static_cast<int>(random_unit() * (3 + intensity * 3));
				flames.push_back(flame);
			}
		}

		//* ------------------------------------------------ MEM ------------------------------------------------
		struct MemState {
			int x = 0, y = 0, width = 0, height = 0;
			int used_percent = 0;
		} mem;
	} // namespace

	bool cpu_enabled() {
		return Config::getB("cpu_animation") and not Config::getB("tty_mode");
	}

	bool mem_enabled() {
		return Config::getB("mem_animation") and not Config::getB("tty_mode");
	}

	bool active() {
		return (cpu_valid and cpu_enabled()) or (mem_valid and mem_enabled());
	}

	void reset() {
		cpu_clear();
		mem_clear();
	}

	void cpu_clear() {
		cpu_valid = false;
		flames.clear();
	}

	void mem_clear() {
		mem_valid = false;
	}

	bool cpu_set(int x, int y, int width, int height, const vector<int>& core_percent, double temp_c) {
		const auto layout = cpu_layout(width, height);
		if (width < 14 or height < 6 or layout.die_width < 6 or layout.die_height < 2) {
			cpu_clear();
			return false;
		}
		if (cpu.width != width or cpu.height != height) flames.clear();
		cpu = {x, y, width, height, core_percent, temp_c};
		cpu_valid = true;
		return true;
	}

	string cpu_render() {
		if (not cpu_valid) return "";
		const int width = cpu.width, height = cpu.height;
		Canvas canvas(width, height);
		const auto layout = cpu_layout(width, height);
		const double usage = average_usage(cpu) / 100.0;
		const double heat = heat_of(cpu);
		const RGB base = lerp(orange, red, heat);
		const double blue_chance = (1.0 - heat) * 0.16;

		//? Package with gold pins, the pins glow with usage
		canvas.rect(layout.package_x0, layout.package_y0 * 2, layout.package_x1, layout.package_y1 * 2 + 1, {38, 33, 30});
		const RGB pin = lerp({105, 88, 42}, base, usage * 0.6);
		for (int x = layout.package_x0 + 1; x < layout.package_x1; x += 2) {
			canvas.pixel(x, layout.package_y0 * 2, pin);
			canvas.pixel(x, layout.package_y1 * 2 + 1, pin);
		}
		for (int y = layout.package_y0 * 2 + 2; y < layout.package_y1 * 2; y += 2) {
			canvas.pixel(layout.package_x0, y, pin);
			canvas.pixel(layout.package_x1, y, pin);
		}
		canvas.rect(layout.die_x0, layout.die_y0 * 2, layout.die_x1, layout.die_y1 * 2 + 1, lerp({16, 14, 13}, blue, 0.05 * (1.0 - heat)));

		//? Tile layout, cores are grouped when the die is too small for one tile each
		const int core_count = std::max<int>(1, cpu.core_percent.size());
		int tile_count = core_count, columns = 1, rows = 1, tile_width = 0, tile_height = 0;
		constexpr int gap_x = 1;
		int gap_y = 0;
		while (true) {
			columns = std::clamp(static_cast<int>(std::lround(std::sqrt(tile_count * layout.die_width / (2.4 * layout.die_height)))), 1, tile_count);
			rows = (tile_count + columns - 1) / columns;
			gap_y = (layout.die_height >= rows * 4 + rows - 1) ? 1 : 0;
			tile_width = (layout.die_width - gap_x * (columns - 1)) / columns;
			tile_height = (layout.die_height - gap_y * (rows - 1)) / rows;
			if ((tile_width >= 3 and tile_height >= 1) or tile_count == 1) break;
			tile_count = (tile_count + 1) / 2;
		}
		tile_width = std::max(tile_width, 1);
		tile_height = std::max(tile_height, 1);
		const int origin_x = layout.die_x0 + (layout.die_width - (tile_width * columns + gap_x * (columns - 1))) / 2;
		const int origin_y = layout.die_y0 + (layout.die_height - (tile_height * rows + gap_y * (rows - 1))) / 2;

		for (int tile = 0; tile < tile_count; tile++) {
			const int tile_x = origin_x + (tile % columns) * (tile_width + gap_x);
			const int tile_y = origin_y + (tile / columns) * (tile_height + gap_y);

			//? Cores covered by this tile and their average usage
			const int first_core = tile * core_count / tile_count;
			const int last_core = std::max(first_core + 1, (tile + 1) * core_count / tile_count);
			const int covered = std::min<int>(last_core, cpu.core_percent.size()) - first_core;
			const double sum = covered > 0 ? std::accumulate(cpu.core_percent.begin() + first_core, cpu.core_percent.begin() + first_core + covered, 0.0) : 0.0;
			const int percent = std::clamp(static_cast<int>(sum / (last_core - first_core)), 0, 100);
			const double load = percent / 100.0;

			RGB background = lerp({16, 14, 13}, base, 0.10 + 0.30 * load);
			background = lerp(background, blue, 0.10 * (1.0 - heat));
			canvas.rect(tile_x, tile_y * 2, tile_x + tile_width - 1, (tile_y + tile_height) * 2 - 1, background);

			const bool has_label = tile_height >= 3 and tile_width >= 6;
			if (has_label) {
				const RGB label_color = lerp(base, white, 0.25);
				const string value = fmt::format("{}%", percent);
				canvas.print(tile_x, tile_y, fmt::format("{}{}", last_core - first_core == 1 ? 'C' : 'G', first_core).substr(0, tile_width), label_color);
				if (tile_width > static_cast<int>(value.size()) + 3) canvas.print(tile_x + tile_width - value.size(), tile_y, value, label_color);
			}

			//? Scrolling digits, the busier the core the denser and faster they flow
			for (int row = has_label ? 1 : 0; row < tile_height; row++) {
				for (int col = 0; col < tile_width; col++) {
					const int step = static_cast<int>(tick) * (2 + static_cast<int>(load * 6) + static_cast<int>(hash(tile, col, 3) % 2)) / 4;
					const uint32_t cell = hash(tile, col, row + step, 77);
					if (cell % 100 < 6.0 + load * 88.0) {
						RGB color = hash_unit(tile, col, row + step, 13) < blue_chance ? lerp(base, blue, 0.75) : base;
						const double brightness = 0.45 + 0.55 * hash_unit(tile, col, row + step, 9);
						color = lerp(scale(color, brightness), white, 0.22 * brightness * load);
						canvas.glyph(tile_x + col, tile_y + row, hex_digits.substr(hash(tile, col, row + step, 5) % 16, 1), color);
					}
					else if (cell % 3 == 0 and load > 0.02) {
						canvas.glyph(tile_x + col, tile_y + row, "·", scale(base, 0.28));
					}
				}
			}
		}

		//? Flames in classic ASCII art, never drawn over the die
		for (const auto& flame : flames) {
			const int flame_x = static_cast<int>(std::lround(flame.x)), flame_y = static_cast<int>(std::lround(flame.y));
			if (flame_x < 0 or flame_y < 0 or flame_x >= width or flame_y >= height) continue;
			if (flame_x >= layout.die_x0 and flame_x <= layout.die_x1 and flame_y >= layout.die_y0 and flame_y <= layout.die_y1) continue;
			const double age = static_cast<double>(flame.age) / flame.life;
			if (age < 0.25) canvas.glyph(flame_x, flame_y, (flame_x & 1) ? "(" : ")", lerp(red, orange, 0.55));
			else if (age < 0.5) canvas.glyph(flame_x, flame_y, "*", red);
			else if (age < 0.75) canvas.glyph(flame_x, flame_y, "^", scale(red, 0.8));
			else canvas.glyph(flame_x, flame_y, (flame_x & 1) ? "'" : "`", scale(red, 0.5));
		}

		//? Average temperature readout in the flame margin
		if (layout.flame_rows >= 1 and width >= 18) {
			const string readout = cpu.temp_c >= 0 ? fmt::format("avg {:.0f}", cpu.temp_c) : "temp n/a";
			const RGB readout_color = lerp(base, white, 0.2);
			canvas.print(1, 0, readout, readout_color);
			if (cpu.temp_c >= 0) {
				canvas.glyph(1 + readout.size(), 0, "°", readout_color);
				canvas.glyph(2 + readout.size(), 0, "C", readout_color);
			}
		}

		return canvas.render(cpu.x, cpu.y);
	}

	bool mem_set(int x, int y, int width, int height, int used_percent) {
		if (width < 22 or height < 5) {
			mem_clear();
			return false;
		}
		mem = {x, y, width, height, std::clamp(used_percent, 0, 100)};
		mem_valid = true;
		return true;
	}

	string mem_render() {
		if (not mem_valid) return "";
		const int width = mem.width, height = mem.height;
		const double usage = mem.used_percent / 100.0;
		Canvas canvas(width, height);

		constexpr RGB pcb{14, 66, 44}, pcb_edge{26, 104, 70}, chip{15, 16, 21}, chip_edge{48, 53, 64}, gold{214, 170, 52};
		constexpr RGB pmic{58, 48, 42}, pmic_edge{96, 80, 66};
		constexpr RGB read_color{120, 255, 215}, write_color{240, 165, 45};

		//? PCB with gold fingers and the key notch
		canvas.rect(0, 1, width - 1, height * 2 - 1, pcb);
		for (int x = 0; x < width; x++) {
			canvas.pixel(x, 1, pcb_edge);
			if (x % 2 == 0) canvas.pixel(x, height * 2 - 1, gold);
		}
		const int notch_x = static_cast<int>(width * 0.40);
		for (int x = notch_x; x < notch_x + 2; x++) {
			canvas.clear_pixel(x, height * 2 - 1);
			canvas.clear_pixel(x, height * 2 - 2);
		}
		canvas.clear_pixel(0, 1);
		canvas.clear_pixel(width - 1, 1);

		//? DRAM chips with a gap in the middle and a small PMIC at the right end
		const int band_top = 3, band_bottom = height * 2 - 5;
		const int gap = width >= 56 ? 2 : 1;
		auto chip_width = [&](int count) { return (width - 2.0 - gap * (count + 1) - 2 * gap) / (count + 0.55); };
		const int chip_count = chip_width(8) < 4.5 ? 4 : 8;
		const double chip_w = chip_width(chip_count);
		double chip_x = 1 + gap;
		for (int i = 0; i <= chip_count; i++) {
			const bool is_pmic = i == chip_count;
			const double w = is_pmic ? chip_w * 0.55 : chip_w;
			const int x0 = static_cast<int>(std::lround(chip_x)), x1 = static_cast<int>(std::lround(chip_x + w)) - 1;
			const int y0 = is_pmic ? band_top + 1 : band_top;
			const int y1 = is_pmic ? band_top + std::max(1, (band_bottom - band_top) / 2) : band_bottom;
			if (x1 > x0 and y1 > y0) {
				const RGB& edge = is_pmic ? pmic_edge : chip_edge;
				canvas.rect(x0, y0, x1, y1, is_pmic ? pmic : chip);
				for (int x = x0; x <= x1; x++) {
					canvas.pixel(x, y0, edge);
					canvas.pixel(x, y1, edge);
				}
				for (int y = y0; y <= y1; y++) {
					canvas.pixel(x0, y, edge);
					canvas.pixel(x1, y, edge);
				}
			}
			chip_x += w + gap + (i == chip_count / 2 - 1 ? 2 * gap : 0);
		}

		//? Decoupling caps and silkscreen below the chips
		for (int x = 3; x < width - 2; x += 4) {
			if (x < notch_x - 1 or x > notch_x + 2) canvas.pixel(x, height * 2 - 4, {150, 130, 85});
		}
		if (width >= 24 and height >= 6) canvas.print(2, height - 2, "DDR5", {214, 204, 160});

		//? Address stream: random hex addresses flowing across the stick, denser and faster with usage
		const int lanes = height - 3;
		const int address_length = width >= 44 ? 10 : (width >= 30 ? 8 : 6);
		const int lane_length = width + address_length;
		const int slots = std::max(1, lane_length / (address_length + 3));
		for (int lane = 0; lane < lanes; lane++) {
			const int direction = (lane % 2 == 0) ? 1 : -1;
			const double speed = (0.45 + hash_unit(lane, 1)) * (0.35 + 0.95 * usage);
			const double phase = hash_unit(lane, 2) * lane_length;
			for (int slot = 0; slot < slots; slot++) {
				const double travelled = slot * (static_cast<double>(lane_length) / slots) + phase + tick * speed;
				const auto laps = static_cast<uint32_t>(std::floor(travelled / lane_length));
				if (hash(lane, slot, laps, 3) % 100 >= 22.0 + usage * 70.0) continue;

				const double position = travelled - laps * static_cast<double>(lane_length);
				const int left = static_cast<int>(direction > 0 ? position : lane_length - position) - address_length;
				const RGB& color = hash(lane, slot, laps, 4) % 6 == 0 ? write_color : read_color;
				for (int i = 0; i < address_length; i++) {
					const int x = left + i;
					if (x < 0 or x >= width) continue;

					//? Leading char is brightest, the tail fades out
					const double lead = direction > 0 ? static_cast<double>(i) / (address_length - 1) : 1.0 - static_cast<double>(i) / (address_length - 1);
					const RGB fade = scale(color, 0.25 + 0.75 * lead);
					const string_view digit = i == 0 ? "0" : i == 1 ? "x" : hex_digits.substr(hash(lane, slot, laps, 10 + i) % 16, 1);
					canvas.glyph(x, 1 + lane, digit, lead > 0.99 ? lerp(fade, white, 0.45) : fade);
				}
			}
		}

		return canvas.render(mem.x, mem.y);
	}

	string frame() {
		if (not active()) return "";
		tick++;
		string out;
		if (cpu_valid and cpu_enabled()) {
			update_flames();
			out += cpu_render();
		}
		if (mem_valid and mem_enabled()) out += mem_render();
		return out;
	}
}
