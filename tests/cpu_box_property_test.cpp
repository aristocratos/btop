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
*/

//? ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//? CPU Box Property Tests
//?
//? These tests call the ACTUAL Cpu::draw() function with controlled inputs and
//? validate that the output is a properly formed box. This catches real bugs
//? in the rendering code - not just a simulator.
//? ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#include <gtest/gtest.h>

#include <cctype>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

namespace {

using std::string;
using std::vector;

//? Helper: Strip ANSI escape codes from a string
string StripAnsi(const string& input) {
	static const std::regex ansi_regex("\x1b\\[[0-9;]*[a-zA-Z]");
	return std::regex_replace(input, ansi_regex, "");
}

//? Virtual terminal buffer that renders cursor-positioned output to a 2D grid
class TerminalBuffer {
	vector<vector<string>> grid;  // 2D grid of UTF-8 characters
	int cursor_row = 0;
	int cursor_col = 0;
	int height_, width_;

public:
	TerminalBuffer(int height, int width)
		: grid(height, vector<string>(width, " ")), height_(height), width_(width) {}

	void render(const string& input) {
		size_t i = 0;
		while (i < input.size()) {
			if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '[') {
				// Parse ANSI escape sequence
				size_t j = i + 2;
				while (j < input.size() && (std::isdigit(input[j]) || input[j] == ';')) j++;
				if (j < input.size()) {
					char cmd = input[j];
					string params = input.substr(i + 2, j - i - 2);
					handleEscapeSequence(cmd, params);
				}
				i = j + 1;
			} else if (input[i] == '\n') {
				cursor_row++;
				cursor_col = 0;
				i++;
			} else {
				// Regular character - determine UTF-8 length
				unsigned char c = input[i];
				size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
				if (i + len <= input.size()) {
					string ch = input.substr(i, len);
					writeChar(ch);
				}
				i += len;
			}
		}
	}

	//? Extract lines from the grid (for use with existing validator)
	vector<string> extractLines() const {
		vector<string> lines;
		for (const auto& row : grid) {
			string line;
			for (const auto& ch : row) {
				line += ch;
			}
			// Trim trailing spaces
			while (!line.empty() && line.back() == ' ') {
				line.pop_back();
			}
			lines.push_back(line);
		}
		return lines;
	}

	//? Debug: dump the grid
	string dump() const {
		string out;
		for (int r = 0; r < height_; r++) {
			for (int c = 0; c < width_; c++) {
				out += grid[r][c];
			}
			out += "\n";
		}
		return out;
	}

private:
	void handleEscapeSequence(char cmd, const string& params) {
		if (cmd == 'f' || cmd == 'H') {
			// Cursor position: \e[row;colf or \e[row;colH
			auto sep = params.find(';');
			if (sep != string::npos) {
				cursor_row = std::stoi(params.substr(0, sep)) - 1;  // 1-indexed to 0-indexed
				cursor_col = std::stoi(params.substr(sep + 1)) - 1;
			} else if (!params.empty()) {
				cursor_row = std::stoi(params) - 1;
				cursor_col = 0;
			}
		} else if (cmd == 'C') {
			// Move cursor right
			int n = params.empty() ? 1 : std::stoi(params);
			cursor_col += n;
		} else if (cmd == 'A') {
			// Move cursor up
			int n = params.empty() ? 1 : std::stoi(params);
			cursor_row -= n;
		} else if (cmd == 'B') {
			// Move cursor down
			int n = params.empty() ? 1 : std::stoi(params);
			cursor_row += n;
		} else if (cmd == 'D') {
			// Move cursor left
			int n = params.empty() ? 1 : std::stoi(params);
			cursor_col -= n;
		}
		// Ignore color codes (m), clear codes (J, K), etc.
	}

	void writeChar(const string& ch) {
		if (cursor_row >= 0 && cursor_row < height_ &&
		    cursor_col >= 0 && cursor_col < width_) {
			grid[cursor_row][cursor_col] = ch;
		}
		cursor_col++;
	}
};

//? Helper: Split UTF-8 string into individual characters
vector<string> Utf8Split(const string& s) {
	vector<string> chars;
	for (size_t i = 0; i < s.size();) {
		unsigned char c = s[i];
		size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
		if (i + len > s.size()) break;
		chars.push_back(s.substr(i, len));
		i += len;
	}
	return chars;
}

//? Box validation result
struct BoxValidationResult {
	bool valid = true;
	string error;
	int row = -1;
	int col = -1;

	explicit operator bool() const { return valid; }
	static BoxValidationResult Ok() { return {}; }
	static BoxValidationResult Fail(const string& msg, int r = -1, int c = -1) {
		return {false, msg, r, c};
	}
};

//? Validates box structure: corners, edges, consistent width
class BoxStructureValidator {
public:
	BoxValidationResult Validate(const vector<string>& lines) const {
		if (lines.empty()) return BoxValidationResult::Fail("Empty box");

		// Find the actual box lines (those that start/end with box characters)
		vector<string> box_lines;
		for (const auto& line : lines) {
			string stripped = StripAnsi(line);
			if (stripped.empty()) continue;

			auto chars = Utf8Split(stripped);
			if (chars.empty()) continue;

			// Check if this looks like a box line (starts with box char)
			if (IsBoxChar(chars.front())) {
				box_lines.push_back(stripped);
			}
		}

		if (box_lines.empty()) return BoxValidationResult::Fail("No box lines found");
		if (box_lines.size() < 3) return BoxValidationResult::Fail("Box too short (< 3 lines)");

		// Validate width consistency
		const size_t width = Tools::ulen(box_lines.front());
		if (width < 3) return BoxValidationResult::Fail("Box too narrow");

		for (size_t i = 0; i < box_lines.size(); ++i) {
			size_t line_width = Tools::ulen(box_lines[i]);
			if (line_width != width) {
				return BoxValidationResult::Fail(
					"Row " + std::to_string(i) + " width mismatch: expected " +
					std::to_string(width) + ", got " + std::to_string(line_width),
					(int)i);
			}
		}

		// Validate corners
		auto top = Utf8Split(box_lines.front());
		auto bot = Utf8Split(box_lines.back());

		if (!IsCorner(top.front())) return BoxValidationResult::Fail("Invalid TL corner: " + top.front(), 0, 0);
		if (!IsCorner(top.back())) return BoxValidationResult::Fail("Invalid TR corner: " + top.back(), 0, (int)width - 1);
		if (!IsCorner(bot.front())) return BoxValidationResult::Fail("Invalid BL corner: " + bot.front(), (int)box_lines.size() - 1, 0);
		if (!IsCorner(bot.back())) return BoxValidationResult::Fail("Invalid BR corner: " + bot.back(), (int)box_lines.size() - 1, (int)width - 1);

		// Validate vertical edges for middle rows
		for (size_t r = 1; r + 1 < box_lines.size(); ++r) {
			auto row = Utf8Split(box_lines[r]);
			if (row.empty()) return BoxValidationResult::Fail("Empty row", (int)r);

			if (!IsVertical(row.front())) {
				return BoxValidationResult::Fail("Invalid left edge: " + row.front(), (int)r, 0);
			}
			if (!IsVertical(row.back())) {
				return BoxValidationResult::Fail("Invalid right edge: " + row.back(), (int)r, (int)width - 1);
			}
		}

		return BoxValidationResult::Ok();
	}

private:
	static bool IsBoxChar(const string& s) {
		static const std::set<string> kBoxChars = {
			"┌", "┐", "└", "┘", "╭", "╮", "╰", "╯",
			"│", "─", "├", "┤", "┬", "┴", "┼"
		};
		return kBoxChars.count(s);
	}

	static bool IsCorner(const string& s) {
		static const std::set<string> kCorners = {"┌", "┐", "└", "┘", "╭", "╮", "╰", "╯"};
		return kCorners.count(s);
	}

	static bool IsVertical(const string& s) {
		return s == "│" || s == "├" || s == "┤" || IsCorner(s);
	}
};

//? Test fixture that sets up the environment for Cpu::draw()
class CpuDrawIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Initialize Config with default values needed for drawing
		InitConfig();

		// Initialize Theme with default colors
		InitTheme();

		// Initialize global state
		InitGlobals();

		// Calculate box sizes (this sets internal b_width, b_columns, etc.)
		Draw::calcSizes();
	}

	void TearDown() override {
		// Reset state if needed
	}

	void InitConfig() {
		// Set required config values
		Config::bools["tty_mode"] = false;
		Config::bools["truecolor"] = true;
		Config::bools["lowcolor"] = false;
		Config::bools["rounded_corners"] = true;
		Config::bools["cpu_single_graph"] = false;
		Config::bools["check_temp"] = false;
		Config::bools["show_cpu_watts"] = false;
		Config::bools["show_coretemp"] = true;
		Config::bools["cpu_bottom"] = false;
		Config::bools["cpu_invert_lower"] = false;
		Config::bools["mem_below_net"] = false;
		Config::bools["proc_left"] = false;
		Config::bools["background_update"] = true;
		Config::bools["show_detailed"] = false;
		Config::bools["proc_gradient"] = false;
		Config::bools["show_battery"] = false;
		Config::bools["show_battery_watts"] = false;
		Config::bools["show_uptime"] = false;
		Config::bools["show_cpu_freq"] = false;

		Config::strings["graph_symbol"] = "braille";
		Config::strings["graph_symbol_cpu"] = "default";
		Config::strings["cpu_graph_upper"] = "total";
		Config::strings["cpu_graph_lower"] = "total";
		Config::strings["temp_scale"] = "celsius";
		Config::strings["shown_boxes"] = "cpu";
		Config::strings["proc_sorting"] = "cpu direct";
		Config::strings["clock_format"] = "";
		Config::strings["custom_cpu_name"] = "";
#ifdef __linux__
		Config::strings["freq_mode"] = "current";
#endif
#ifdef GPU_SUPPORT
		Config::strings["show_gpu_info"] = "Off";
#endif

		Config::ints["update_ms"] = 1000;
		Config::ints["proc_per_core"] = 0;
		Config::ints["selected_pid"] = 0;

		Config::current_preset = 0;
		Config::current_boxes = {"cpu"};
	}

	void InitTheme() {
		// Set up minimal theme colors (empty strings = no color codes)
		Theme::colors["main_fg"] = "";
		Theme::colors["main_bg"] = "";
		Theme::colors["title"] = "";
		Theme::colors["hi_fg"] = "";
		Theme::colors["cpu_box"] = "";
		Theme::colors["mem_box"] = "";
		Theme::colors["net_box"] = "";
		Theme::colors["proc_box"] = "";
		Theme::colors["div_line"] = "";
		Theme::colors["meter_bg"] = "";
		Theme::colors["inactive_fg"] = "";
		Theme::colors["graph_text"] = "";
		Theme::colors["process_start"] = "";
		Theme::colors["process_mid"] = "";
		Theme::colors["process_end"] = "";

		// Set up gradients (101 entries each)
		std::array<string, 101> empty_gradient;
		for (auto& s : empty_gradient) s = "";
		Theme::gradients["cpu"] = empty_gradient;
		Theme::gradients["temp"] = empty_gradient;
		Theme::gradients["free"] = empty_gradient;
		Theme::gradients["cached"] = empty_gradient;
		Theme::gradients["used"] = empty_gradient;
		Theme::gradients["download"] = empty_gradient;
		Theme::gradients["upload"] = empty_gradient;
		Theme::gradients["process"] = empty_gradient;
	}

	void InitGlobals() {
		// Terminal size
		Term::width = 120;
		Term::height = 40;

		// Core count
		Shared::coreCount = 8;

		// CPU info
		Cpu::cpuName = "Test CPU";
		Cpu::cpuHz = "";
		Cpu::available_fields = {"total"};
		Cpu::got_sensors = false;
		Cpu::cpu_temp_only = false;
		Cpu::has_battery = false;
		Cpu::supports_watts = false;
		Cpu::shown = true;
		Cpu::redraw = true;

#ifdef __APPLE__
		// Apple Silicon core info
		Cpu::cpu_core_info.p_cores = 4;
		Cpu::cpu_core_info.e_cores = 4;
		Cpu::cpu_core_info.p_freq_mhz = 3200;
		Cpu::cpu_core_info.e_freq_mhz = 2100;
#endif
	}

#ifdef __APPLE__
	void ApplyAppleSiliconConfig(int p_cores, int e_cores) {
		Cpu::cpu_core_info.p_cores = p_cores;
		Cpu::cpu_core_info.e_cores = e_cores;
		Cpu::cpu_core_info.p_freq_mhz = 3200;
		Cpu::cpu_core_info.e_freq_mhz = 2100;
		Shared::coreCount = p_cores + e_cores;
		Cpu::redraw = true;
		Draw::calcSizes();
	}
#endif

	// Create minimal cpu_info data for testing
	Cpu::cpu_info CreateTestCpuInfo() {
		Cpu::cpu_info cpu;

		// Total CPU usage
		cpu.cpu_percent["total"] = {50};

		// Per-core usage (8 cores)
		for (int i = 0; i < Shared::coreCount; ++i) {
			cpu.core_percent.push_back({20 + i * 5});
		}

		// Load average
		cpu.load_avg = {1.5, 1.2, 1.0};

		return cpu;
	}

	BoxStructureValidator validator;
};

// Test that the real Cpu::draw() produces output
TEST_F(CpuDrawIntegrationTest, DrawProducesOutput) {
	auto cpu = CreateTestCpuInfo();
	vector<Gpu::gpu_info> gpus;  // empty

	// Call the actual draw function
	string output = Cpu::draw(cpu, gpus, true, false);

	// The draw function should produce output
	EXPECT_FALSE(output.empty()) << "Cpu::draw should produce output";

	// The box frame should also be set
	EXPECT_FALSE(Cpu::box.empty()) << "Cpu::box should be set after draw";
}

// Test that the box contains expected structural elements
TEST_F(CpuDrawIntegrationTest, BoxHasStructuralElements) {
	auto cpu = CreateTestCpuInfo();
	vector<Gpu::gpu_info> gpus;

	Cpu::draw(cpu, gpus, true, false);
	string stripped = StripAnsi(Cpu::box);

	// Box should have corner characters
	bool has_corners = stripped.find("╭") != string::npos ||
					   stripped.find("┌") != string::npos;
	EXPECT_TRUE(has_corners) << "Box should have corner characters";

	// Box should have vertical lines
	EXPECT_TRUE(stripped.find("│") != string::npos)
		<< "Box should have vertical line characters";

	// Box should have horizontal lines
	EXPECT_TRUE(stripped.find("─") != string::npos)
		<< "Box should have horizontal line characters";
}

#ifdef __APPLE__
struct SiliconConfig {
	int p_cores;
	int e_cores;
	string name;
};

vector<SiliconConfig> BuildAppleSiliconConfigs() {
	return {
		{4, 4, "M1"},
		{8, 2, "M1_Pro"},
		{8, 2, "M1_Max"},
		{16, 4, "M1_Ultra"},
		{4, 4, "M2"},
		{6, 4, "M2_Pro_10C"},
		{8, 4, "M2_Pro_12C"},
		{8, 4, "M2_Max"},
		{16, 8, "M2_Ultra"},
		{4, 4, "M3"},
		{6, 6, "M3_Pro"},
		{5, 6, "M3_Pro_11C"},
		{12, 4, "M3_Max"},
		{10, 4, "M3_Max_14C"},
		{12, 4, "M3_Max_16C"},
		{4, 6, "M4"},
		{10, 4, "M4_Pro_12C"},
		{10, 4, "M4_Pro_14C"},
		{10, 4, "M4_Max_14C"},
		{12, 4, "M4_Max_16C"},
	};
}

vector<SiliconConfig> BuildAppleSiliconSweep() {
	vector<SiliconConfig> configs;
	for (int p = 1; p <= 8; ++p) {
		for (int e = 1; e <= 8; ++e) {
			configs.push_back({p, e, "P" + std::to_string(p) + "_E" + std::to_string(e)});
		}
	}
	return configs;
}

vector<SiliconConfig> BuildAppleSiliconTestMatrix() {
	auto configs = BuildAppleSiliconConfigs();
	auto sweep = BuildAppleSiliconSweep();
	configs.insert(configs.end(), sweep.begin(), sweep.end());
	return configs;
}

string SanitizeTestName(const string& name) {
	string sanitized;
	sanitized.reserve(name.size() + 2);
	for (char ch : name) {
		if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
			sanitized.push_back(ch);
		} else {
			sanitized.push_back('_');
		}
	}
	if (sanitized.empty() || std::isdigit(static_cast<unsigned char>(sanitized.front())) != 0) {
		sanitized.insert(sanitized.begin(), 'C');
		sanitized.insert(sanitized.begin() + 1, '_');
	}
	return sanitized;
}

class CpuDrawAppleSiliconConfigTest
	: public CpuDrawIntegrationTest
	, public ::testing::WithParamInterface<SiliconConfig> {};

TEST_P(CpuDrawAppleSiliconConfigTest, AppleSiliconLayoutHasExpectedContent) {
	const auto& cfg = GetParam();
	ApplyAppleSiliconConfig(cfg.p_cores, cfg.e_cores);

	auto cpu = CreateTestCpuInfo();
	vector<Gpu::gpu_info> gpus;

	string output = Cpu::draw(cpu, gpus, true, false);
	string stripped = StripAnsi(output);

	EXPECT_FALSE(output.empty()) << cfg.name << ": Cpu::draw should produce output";

	EXPECT_TRUE(stripped.find("P-CPU") != string::npos)
		<< cfg.name << ": Apple Silicon output should contain P-CPU header";
	EXPECT_TRUE(stripped.find("E-CPU") != string::npos)
		<< cfg.name << ": Apple Silicon output should contain E-CPU header";

	EXPECT_TRUE(stripped.find("P0") != string::npos)
		<< cfg.name << ": Apple Silicon output should contain P-core labels (P0, P1, ...)";
	EXPECT_TRUE(stripped.find("E0") != string::npos)
		<< cfg.name << ": Apple Silicon output should contain E-core labels (E0, E1, ...)";

	// Render box to virtual terminal buffer, then validate structure
	TerminalBuffer term_buf(Term::height, Term::width);
	term_buf.render(Cpu::box);
	auto lines = term_buf.extractLines();
	auto res = validator.Validate(lines);
	EXPECT_TRUE(res) << cfg.name << ": " << res.error;
}

INSTANTIATE_TEST_SUITE_P(
	AppleSiliconConfigs,
	CpuDrawAppleSiliconConfigTest,
	::testing::ValuesIn(BuildAppleSiliconTestMatrix()),
	[](const testing::TestParamInfo<SiliconConfig>& info) {
		return SanitizeTestName(info.param.name);
	});

#endif

}  // namespace
