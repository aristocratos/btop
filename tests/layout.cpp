// SPDX-License-Identifier: Apache-2.0

#include <functional>
#include <regex>

#include <gtest/gtest.h>

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_input.hpp"
#include "btop_layout.hpp"
#include "btop_menu.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

namespace {
	auto target() -> Layout::HorizontalInput {
		Layout::HorizontalInput in;
		in.width = 283;
		in.height = 33;
		in.cores = 32;
		in.enabled = in.cpu = in.mem = in.net = in.proc = in.core_temps = true;
		in.gpu_ids = {0};
		in.gpu_offsets = {8};
		in.remaining_min_width = 80;
		return in;
	}
}

TEST(layout, shared_row_and_full_core_capacity) {
	const auto in = target();
	const auto row = Layout::horizontal_row(in);
	ASSERT_TRUE(row);
	EXPECT_EQ(row->cpu_width, 141);
	EXPECT_EQ(row->gpu_width, 142);
	EXPECT_EQ(row->height, 12);
	EXPECT_EQ(row->y, 1);
	EXPECT_GE(row->columns * (row->detail_height - 4), in.cores);
	EXPECT_GE(row->cpu_width - row->detail_width - 2, (row->cpu_width - 2 + 2) / 3);
	EXPECT_EQ(row->remaining_y, 13);
	EXPECT_EQ(row->remaining_height, 21);
	EXPECT_EQ(row->mem_height + row->net_height, 21);
}

TEST(layout, eligibility_and_device_ids) {
	auto in = target();
	in.enabled = false;
	EXPECT_FALSE(Layout::horizontal_row(in));
	in.enabled = true;
	in.cpu = false;
	EXPECT_FALSE(Layout::horizontal_row(in));
	in.cpu = true;
	in.gpu_ids.clear();
	EXPECT_FALSE(Layout::horizontal_row(in));
	in.gpu_ids = {0, 1};
	in.gpu_offsets = {4, 12};
	EXPECT_FALSE(Layout::horizontal_row(in));
	in.gpu_ids = {1};
	const auto row = Layout::horizontal_row(in);
	ASSERT_TRUE(row);
	EXPECT_EQ(row->height, 16);
	EXPECT_EQ(Layout::visible_gpus("cpu gpu1 gpu1 gpu0 gpu10 gpu5", 2), (std::vector<int>{1, 0}));
}

TEST(layout, placement_and_remaining_panel_combinations) {
	for (int mask = 0; mask < 32; ++mask) {
		auto in = target();
		in.mem = mask & 1;
		in.net = mask & 2;
		in.proc = mask & 4;
		in.bottom = mask & 8;
		in.mem_below_net = mask & 16;
		const auto row = Layout::horizontal_row(in);
		ASSERT_TRUE(row);
		EXPECT_EQ(row->remaining_height + row->height, in.height);
		EXPECT_EQ(row->y, in.bottom ? row->remaining_height + 1 : 1);
		EXPECT_EQ(row->remaining_y, in.bottom ? 1 : row->height + 1);
		if (in.mem) {
			EXPECT_GE(row->mem_height, in.mem_min_height);
			EXPECT_GE(row->mem_y, row->remaining_y);
			EXPECT_LE(row->mem_y + row->mem_height, row->remaining_y + row->remaining_height);
		}
		if (in.net) {
			EXPECT_GE(row->net_height, in.net_min_height);
			EXPECT_GE(row->net_y, row->remaining_y);
			EXPECT_LE(row->net_y + row->net_height, row->remaining_y + row->remaining_height);
		}
		if (not (in.mem or in.net or in.proc)) {
			EXPECT_EQ(row->height, in.height);
			EXPECT_EQ(row->column_size, 2);
		}
	}
}

TEST(layout, temperatures_inline_information_and_thresholds) {
	for (const int cores : {1, 2, 8, 32, 128, 256}) {
		auto in = target();
		in.cores = cores;
		in.height = 100;
		in.gpu_offsets = {0};
		const auto row = Layout::horizontal_row(in);
		ASSERT_TRUE(row);
		EXPECT_GE(row->height, 8);
		EXPECT_GE(row->columns * (row->detail_height - 4), cores);
	}
	for (const bool temps : {false, true}) {
		for (const int inline_gpus : {0, 1, 3}) {
			auto in = target();
			in.core_temps = temps;
			in.inline_gpus = inline_gpus;
			in.height = 60;
			for (int width = 100; width <= 319; ++width) {
				in.width = width;
				const auto row = Layout::horizontal_row(in);
				if (not row) continue;
				EXPECT_EQ(row->cpu_width + row->gpu_width, width);
				EXPECT_GE(row->columns * (row->detail_height - 4 - inline_gpus), in.cores);
				in.height = row->required_height + 16;
				EXPECT_TRUE(Layout::horizontal_row(in));
				--in.height;
				EXPECT_FALSE(Layout::horizontal_row(in));
				in.height = 60;
			}
		}
	}
	auto in = target();
	while (Layout::horizontal_row(in)) --in.width;
	EXPECT_FALSE(Layout::horizontal_row(in));
	++in.width;
	EXPECT_TRUE(Layout::horizontal_row(in));
	++in.width;
	EXPECT_TRUE(Layout::horizontal_row(in));
}

namespace {
	// Replay output into cells so assertions see overwrites as well as cursor positions.
	class Screen {
		vector<vector<string>> cells;
	public:
		Screen() : cells(Term::height, vector<string>(Term::width, " ")) {}

		void apply(const string& output, int x, int y, int width, int height) {
			int col = 1, line = 1;
			for (size_t i = 0; i < output.size();) {
				if (output[i] == '\033' and i + 1 < output.size() and output[i + 1] == '[') {
					const auto end = output.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", i + 2);
					ASSERT_NE(end, string::npos);
					const auto params = Tools::ssplit(output.substr(i + 2, end - i - 2), ';');
					const int first = params.empty() or params[0].empty() ? 1 : std::stoi(params[0]);
					switch (output[end]) {
						case 'H': case 'f':
							line = first;
							col = params.size() < 2 ? 1 : std::stoi(params[1]);
							ASSERT_GT(line, 0);
							ASSERT_GT(col, 0);
							ASSERT_LE(line, Term::height);
							ASSERT_LE(col, Term::width);
							break;
						case 'A': line -= first; break;
						case 'B': line += first; break;
						case 'C': col += first; break;
						case 'D': col -= first; break;
						default: break;
					}
					i = end + 1;
				}
				else {
					ASSERT_GE(col, x);
					ASSERT_LT(col, x + width);
					ASSERT_GE(line, y);
					ASSERT_LT(line, y + height);
					size_t end = i + 1;
					while (end < output.size() and (static_cast<unsigned char>(output[end]) & 0xC0) == 0x80) ++end;
					cells.at(line - 1).at(col - 1) = output.substr(i, end - i);
					++col;
					i = end;
				}
			}
		}

		auto text(int line, int col, int width) const -> string {
			string result;
			for (int i = col - 1; i < col - 1 + width; ++i) result += cells.at(line - 1).at(i);
			return result;
		}
	};

#ifdef GPU_SUPPORT
	void expect_panel_bounds(const string& output, int x, int y, int width, int height) {
		Screen screen;
		screen.apply(output, x, y, width, height);
	}
#endif

	// Restore shared inputs and observable UI state even when an assertion aborts a test.
	class SavedState {
		vector<std::function<void()>> restore;
		template<class T> void save(T& value) {
			restore.emplace_back([&value, original = value] { value = original; });
		}
		template<class T> void save(std::atomic<T>& value) {
			restore.emplace_back([&value, original = value.load()] { value = original; });
		}
	public:
		template<class... T> void save_all(T&... values) { (save(values), ...); }
		~SavedState() {
			for (auto& action : restore) action();
		}
	};

	class LayoutIntegration : public testing::Test {
		SavedState state;
	protected:
		Cpu::cpu_info cpu{};
#ifdef GPU_SUPPORT
		vector<Gpu::gpu_info> gpus;
#endif
		auto draw_cpu(bool redraw = true, bool data_same = true) -> string {
			return Cpu::draw(cpu,
#ifdef GPU_SUPPORT
				gpus,
#endif
				redraw, data_same);
		}

		void SetUp() override {
			state.save_all(Config::bools, Config::strings, Config::ints,
				Config::boolsTmp, Config::stringsTmp, Config::intsTmp,
				Config::current_boxes, Config::current_preset, Config::write_new,
				Term::width, Term::height, Term::fg, Term::bg, Fx::reset,
				Theme::colors, Theme::rgbs, Theme::gradients,
				Shared::coreCount, Cpu::got_sensors, Cpu::cpu_temp_only, Cpu::supports_watts,
				Cpu::has_battery, Cpu::current_bat, Cpu::container_engine, Cpu::available_fields,
				Cpu::cpuName, Cpu::cpuHz,
				Global::clock, Global::overlay, Runner::pause_output, Runner::redraw, Menu::redraw,
				Input::mouse_mappings, Proc::p_counters, Proc::p_graphs, Proc::select_max,
				Cpu::x, Cpu::y, Cpu::width, Cpu::height, Cpu::box, Cpu::shown, Cpu::redraw,
				Mem::x, Mem::y, Mem::width, Mem::height, Mem::box, Mem::shown, Mem::redraw,
				Net::x, Net::y, Net::width, Net::height, Net::box, Net::shown, Net::redraw,
				Proc::x, Proc::y, Proc::width, Proc::height, Proc::box, Proc::shown, Proc::redraw);
#ifdef GPU_SUPPORT
			state.save_all(Gpu::count, Gpu::gpu_b_height_offsets, Gpu::gpu_names, Gpu::shared_gpu_percent,
				Gpu::x_vec, Gpu::y_vec, Gpu::width, Gpu::total_height, Gpu::box,
				Gpu::shown, Gpu::shown_panels, Gpu::redraw);
			Gpu::count = 2;
			Gpu::gpu_b_height_offsets = {4, 8};
			Gpu::gpu_names = {"Small GPU", "Full GPU"};
			gpus.resize(2);
			for (auto& gpu : gpus) {
				for (auto& [name, values] : gpu.gpu_percent) values = {42};
				gpu.gpu_clock_speed = 1234;
				gpu.pwr_usage = 100000;
				gpu.pwr_state = 0;
				gpu.mem_total = 8LL << 30;
				gpu.mem_used = 4LL << 30;
			}
			Config::set("shown_boxes", string("cpu mem net proc gpu1"));
			Config::set("show_gpu_info", string("Off"));
#else
			Config::set("shown_boxes", string("cpu mem net proc"));
#endif
			Term::width = 283;
			Term::height = 33;
			Shared::coreCount = 32;
			Cpu::got_sensors = true;
			Cpu::cpu_temp_only = Cpu::supports_watts = Cpu::has_battery = false;
			Cpu::container_engine.reset();
			Cpu::cpuName = "Test CPU";
			Cpu::cpuHz.clear();
			cpu.cpu_percent["total"] = {42};
			cpu.core_percent.resize(32, {42});
			cpu.temp.resize(33, {50});
			cpu.temp_max = 100;
			Config::current_preset.reset();
			Config::set("cpu_gpu_horizontal", true);
			Config::set("show_uptime", false);
			Config::set("check_temp", true);
			Config::set("show_coretemp", true);
			Config::set("cpu_single_graph", false);
			Config::set("cpu_graph_upper", string("total"));
			Config::set("cpu_graph_lower", string("total"));
			Theme::setTheme();
		}
	};
}

#ifdef GPU_SUPPORT
TEST_F(LayoutIntegration, graph_labels_fit_the_graph_interior) {
	Config::set("shown_boxes", string("cpu gpu1"));
	Term::height = 60;
	Cpu::available_fields = {"total", "gpu-vram-total", "gpu-pwr-total"};
	Gpu::shared_gpu_percent["gpu-vram-total"] = {42};
	Gpu::shared_gpu_percent["gpu-pwr-total"] = {42};
	for (const int width : {130, 140, 150, 160, 283, 130}) {
		Term::width = width;
		for (const auto& fields : {std::pair{"gpu-vram-total", "gpu-pwr-total"},
			std::pair{"total", "gpu-vram-total"}, std::pair{"gpu-pwr-total", "total"}}) {
			SCOPED_TRACE(testing::Message() << width << " " << fields.first << " / " << fields.second);
			Config::set("cpu_graph_upper", string(fields.first));
			Config::set("cpu_graph_lower", string(fields.second));
			const auto row = Layout::horizontal_row(Config::getS("shown_boxes"));
			ASSERT_TRUE(row);
			Draw::calcSizes();
			Screen screen;
			for (const bool redraw : {true, false}) {
				screen.apply(draw_cpu(redraw, false), Cpu::x, Cpu::y, Cpu::width, Cpu::height);
				const int divider_y = Cpu::y + (Cpu::height - 2 + 1) / 2 - (Cpu::height % 2 != 0) + 1;
				const int graph_width = row->cpu_width - row->detail_width - 2;
				const auto interior = screen.text(divider_y, Cpu::x + 1, graph_width);
				const auto separator = interior.find("─▲▼─");
				ASSERT_NE(separator, string::npos);
				const string upper{Tools::ltrim(interior.substr(0, separator), "─")};
				const string lower{Tools::rtrim(interior.substr(separator + string("─▲▼─").size()), "─")};
				EXPECT_FALSE(upper.empty());
				EXPECT_FALSE(lower.empty());
				EXPECT_TRUE(string(fields.first).starts_with(upper));
				EXPECT_TRUE(string(fields.second).starts_with(lower));
				const int full_width = string(fields.first).size() + string(fields.second).size() + 4;
				EXPECT_EQ(Tools::ulen(upper) + Tools::ulen(lower) + 4, std::min(full_width, graph_width));
				if (full_width <= graph_width) {
					EXPECT_EQ(upper, fields.first);
					EXPECT_EQ(lower, fields.second);
				}
				const int left_padding = Tools::ulen(interior.substr(0, separator)) - Tools::ulen(upper);
				EXPECT_EQ(left_padding, (graph_width - Tools::ulen(upper) - Tools::ulen(lower) - 4) / 2);
				EXPECT_EQ(screen.text(divider_y, Cpu::x, 1), Symbols::div_left);
				EXPECT_EQ(screen.text(divider_y, Cpu::x + graph_width + 1, 1), Symbols::div_right);
			}
		}
	}
}

TEST_F(LayoutIntegration, validation_allocation_and_visibility_agree) {
	for (const int width : {180, 282, 283, 318}) {
		for (const int height : {27, 28, 33, 36}) {
			Term::width = width;
			Term::height = height;
			for (const bool bottom : {false, true}) {
				Config::set("cpu_bottom", bottom);
				for (const bool left : {false, true}) {
					Config::set("proc_left", left);
					const auto& boxes = Config::getS("shown_boxes");
					const auto row = Layout::horizontal_row(boxes);
					const auto minimum = Term::get_min_size(boxes);
					if (not row) {
						EXPECT_EQ(minimum[1], 36);
						continue;
					}
					EXPECT_LE(minimum[0], width);
					EXPECT_LE(minimum[1], height);
					Draw::calcSizes();
					EXPECT_EQ(Cpu::width, row->cpu_width);
					EXPECT_EQ(Cpu::height, row->height);
					EXPECT_EQ(Gpu::width, row->gpu_width);
					EXPECT_EQ(Gpu::x_vec[0], Cpu::width + 1);
					EXPECT_EQ(Gpu::y_vec[0], Cpu::y);
					EXPECT_EQ(Proc::height, row->remaining_height);
					EXPECT_EQ(Proc::y, row->remaining_y);
					EXPECT_EQ(Mem::height + Net::height, Proc::height);
					EXPECT_EQ(Mem::width + Proc::width, width);
					EXPECT_EQ(Proc::x, left ? 1 : Mem::width + 1);
				}
			}
		}
	}
	Term::width = 283;
	Term::height = 33;
	EXPECT_TRUE(Config::apply_preset("cpu:1:default,mem:0:default,net:0:default,proc:1:default,gpu1:0:default"));
	Draw::calcSizes();
	EXPECT_EQ(Cpu::y, 22);
	EXPECT_EQ(Gpu::shown_panels, (std::vector<int>{1}));
	EXPECT_TRUE(Config::toggle_box("gpu1"));
	EXPECT_FALSE(Layout::horizontal_row(Config::getS("shown_boxes")));
	EXPECT_TRUE(Config::toggle_box("gpu1"));
	EXPECT_TRUE(Layout::horizontal_row(Config::getS("shown_boxes")));
	Config::set("cpu_gpu_horizontal", false);
	EXPECT_EQ(Term::get_min_size(Config::getS("shown_boxes"))[1], 36);
	Term::height = 36;
	Draw::calcSizes();
	EXPECT_EQ(Cpu::width, 283);
	EXPECT_EQ(Gpu::width, 283);
	Config::set("cpu_gpu_horizontal", true);
	Draw::calcSizes();
	EXPECT_EQ(Cpu::width, 141);
}

TEST_F(LayoutIntegration, renders_all_cores_gpu_footer_and_header_controls) {
	for (const bool bottom : {false, true}) {
		Config::set("cpu_bottom", bottom);
		Draw::calcSizes();
		const auto output = Cpu::draw(cpu, gpus, true, true);
		expect_panel_bounds(output, Cpu::x, Cpu::y, Cpu::width, Cpu::height);
		const auto plain = std::regex_replace(output, std::regex("\033\\[[0-9;]*[A-Za-z]"), "");
		for (int core = 0; core < 32; ++core) EXPECT_NE(plain.find("C" + std::to_string(core) + " "), string::npos);
		const auto gpu_output = Gpu::draw(gpus[1], 0, true, true);
		expect_panel_bounds(gpu_output, Gpu::x_vec[0], Gpu::y_vec[0], Gpu::width, Cpu::height);
		EXPECT_NE(gpu_output.find("TX:"), string::npos);
		EXPECT_NE(gpu_output.find("RX:"), string::npos);
		EXPECT_NE(gpu_output.find(Mv::to(Cpu::y + 10, 220)), string::npos);
		for (const auto& key : {"m", "p", "+", "-"}) {
			const auto& hit = Input::mouse_mappings.at(key);
			EXPECT_EQ(hit.line, bottom ? 33 : 1);
			EXPECT_GT(hit.col, Cpu::x);
			EXPECT_LT(hit.col + hit.width, Cpu::x + Cpu::width);
		}
		EXPECT_TRUE(Draw::update_clock(true));
	}
	Cpu::has_battery = true;
	Cpu::current_bat = {100, 42.5f, 100000L, "charging"};
	Cpu::container_engine = string(100, 'x');
	Config::set("clock_format", string(100, 'X'));
	for (const int width : {160, 283, 318}) {
		Term::width = width;
		Term::height = 60;
		for (const bool temps : {false, true}) {
			Config::set("check_temp", temps);
			for (const string info : {"Off", "Auto", "On"}) {
				Config::set("show_gpu_info", info);
				Draw::calcSizes();
				Screen screen;
				screen.apply(draw_cpu(), Cpu::x, Cpu::y, Cpu::width, Cpu::height);
				EXPECT_TRUE(Draw::update_clock(true));
				screen.apply(Global::clock, Cpu::x, Cpu::y, Cpu::width, Cpu::height);
				const auto header = screen.text(Cpu::y + Cpu::height - 1, Cpu::x, Cpu::width);
				EXPECT_NE(header.find("menu"), string::npos);
				EXPECT_NE(header.find("preset *"), string::npos);
				EXPECT_NE(header.find("BAT▲ 100%"), string::npos);
				EXPECT_NE(header.find("- " + std::to_string(Config::getI("update_ms")) + "ms +"), string::npos);
			}
		}
	}
	gpus[1].supported_functions.encoder_utilization = false;
	Draw::calcSizes();
	const auto decoder_only = Gpu::draw(gpus[1], 0, true, true);
	EXPECT_EQ(decoder_only.find("ENC "), string::npos);
	EXPECT_NE(decoder_only.find("DEC "), string::npos);
	Config::set("show_coretemp", false);
	Draw::calcSizes();
	expect_panel_bounds(Cpu::draw(cpu, gpus, true, true), Cpu::x, Cpu::y, Cpu::width, Cpu::height);

	// Stacked fallback with unequal device capabilities must retain per-panel graph and footer heights.
	Term::height = 40;
	Config::set("shown_boxes", string("cpu gpu0 gpu1"));
	Config::set("cpu_bottom", false);
	Config::set("show_gpu_info", string("Off"));
	Config::set("gpu_mirror_graph", true);
	gpus[0].supported_functions.mem_used = false;
	gpus[0].supported_functions.mem_utilization = false;
	Draw::calcSizes();
	for (const bool redraw : {true, false}) {
		for (int slot = 0; slot < 2; ++slot) {
			const int height = slot == 0 ? Gpu::y_vec[1] - Gpu::y_vec[0] : Term::height - Gpu::y_vec[1] + 1;
			expect_panel_bounds(Gpu::draw(gpus[slot], slot, redraw, true), 1, Gpu::y_vec[slot], Gpu::width, height);
		}
	}
}
#else
TEST_F(LayoutIntegration, gpu_disabled_ignores_option) {
	Config::set("cpu_gpu_horizontal", true);
	Term::width = 283;
	Term::height = 33;
	EXPECT_FALSE(Layout::horizontal_row("cpu mem net proc gpu0"));
	EXPECT_EQ(Term::get_min_size("cpu mem net proc")[1], 24);
	Config::set("cpu_gpu_horizontal", false);
}
#endif

TEST_F(LayoutIntegration, composed_header_survives_resizing_and_settings_changes) {
	Config::set("cpu_gpu_horizontal", false);
	Config::set("shown_boxes", string("cpu"));
	Config::set("clock_format", string(100, 'X'));
	Config::set("show_battery_watts", true);
	Cpu::container_engine = string(100, 'z');
	Cpu::has_battery = true;
	Term::height = 60;
	for (const bool bottom : {false, true}) {
		Config::set("cpu_bottom", bottom);
		for (const int interval : {2000, 86400000}) {
			Config::set("update_ms", interval);
			for (const int width : {60, 68, 80, 100, 283, 60}) {
				Term::width = width;
				SCOPED_TRACE(testing::Message() << "bottom=" << bottom << " interval=" << interval << " width=" << width);
				Screen screen;
				for (const bool battery : {true, false, true}) {
					Config::set("show_battery", battery);
					Draw::calcSizes();
					for (const bool redraw : {true, false}) {
						Cpu::current_bat = redraw ? std::tuple{100, 42.5f, 100000L, string("charging")}
							: std::tuple{9, 4.5f, 3600L, string("discharging")};
						screen.apply(draw_cpu(redraw, false), Cpu::x, Cpu::y, Cpu::width, Cpu::height);
						ASSERT_TRUE(Draw::update_clock(true));
						screen.apply(Global::clock, Cpu::x, Cpu::y, Cpu::width, Cpu::height);
						const int line = bottom ? Cpu::y + Cpu::height - 1 : Cpu::y;
						const auto header = screen.text(line, Cpu::x, Cpu::width);
						EXPECT_NE(header.find("menu"), string::npos);
						EXPECT_NE(header.find("preset *"), string::npos);
						EXPECT_NE(header.find("- " + std::to_string(interval) + "ms +"), string::npos);
						if (battery) {
							const string core = redraw ? "BAT▲ 100%" : "BAT▼ 9%";
							const auto start = header.find(core);
							ASSERT_NE(start, string::npos);
							const auto end = header.find(bottom ? Symbols::title_right_down : Symbols::title_right, start);
							ASSERT_NE(end, string::npos);
							string extras = header.substr(start + core.size(), end - start - core.size());
							for (const auto& element : {redraw ? string(" 42.50W") : string(" 4.50W"),
								" " + Tools::sec_to_dhms(redraw ? 100000L : 3600L, false, true),
								string(" ■■■■■■■■■■")}) {
								if (extras.starts_with(element)) extras.erase(0, element.size());
							}
							EXPECT_TRUE(extras.empty()) << "Partial battery element: " << extras;
							if (width == 60) {
								EXPECT_EQ(header.find('W'), string::npos);
								EXPECT_EQ(header.find(':'), string::npos);
							}
							if (width == 283) {
								EXPECT_NE(header.find(redraw ? "42.50W" : "4.50W"), string::npos);
								EXPECT_NE(header.find(Tools::sec_to_dhms(redraw ? 100000L : 3600L, false, true)), string::npos);
							}
						}
						else EXPECT_EQ(header.find("BAT"), string::npos);
						for (const auto& [key, label] : std::vector<std::pair<string, string>>{
							{"m", "menu"}, {"p", "preset *"}, {"-", "- "}, {"+", " +"}}) {
							const auto& hit = Input::mouse_mappings.at(key);
							EXPECT_EQ(hit.line, line);
							EXPECT_EQ(hit.height, 1);
							EXPECT_EQ(screen.text(hit.line, hit.col, hit.width), label);
						}
					}
				}
			}
		}
	}
}
