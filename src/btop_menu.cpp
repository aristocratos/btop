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

#include <deque>
#include <robin_hood.h>
#include <array>
#include <signal.h>
#include <errno.h>
#include <cmath>
#include <filesystem>
#include <string_view>

#include "btop_menu.hpp"
#include "btop_tools.hpp"
#include "btop_config.hpp"
#include "btop_theme.hpp"
#include "btop_draw.hpp"
#include "btop_shared.hpp"

#define ASIZE(a) (sizeof(a)/sizeof(*a))

using robin_hood::unordered_flat_map;
using std::array;
using std::ceil;
using std::max;
using std::min;
using std::ref;
using std::views::iota;

using namespace Tools;

namespace fs = std::filesystem;

enum Predispositions { isBool, isInt, isString, is2D, isBrowseable, isEditable};
struct MenuEntry {
	std::bitset<8> info;
	uint32_t offset;
	const vector<std::string> *browse_values{};
	const vector<std::string_view> content;
};

template<typename T>
constexpr std::bitset<8> make_info(const bool browsable) {
	std::bitset<8> s;
	switch(Config::type_enum<T>()) {
		case Config::ConfigType::BOOL:
			s.set(isBool);
			break;
		case Config::ConfigType::INT:
			s.set(isInt);
			break;
		case Config::ConfigType::STRING:
			s.set(isString);
			break;
	}

	if(not s.test(isString)) {
		s.set(is2D);
	}

	if(browsable) {
		s.set(isBrowseable);
	} else if(s.test(isString) or s.test(isInt)) {
		s.set(isEditable);
	}

	return s;
}

#define MENU_ENTRY(name, ...) { \
	.info = make_info<decltype(std::declval<Config::ConfigSet>().name)>(false), \
	.offset = CONFIG_OFFSET(name), \
	.content = { #name, __VA_ARGS__ }, \
}

#define MENU_BROWSABLE_ENTRY(name, src, ...) { \
	.info = make_info<decltype(std::declval<Config::ConfigSet>().name)>(true), \
	.offset = CONFIG_OFFSET(name), \
	.browse_values = &(src), \
	.content = { #name, __VA_ARGS__ }, \
}


namespace Menu {
   atomic<bool> active (false);
   string bg;
   bool redraw{true};
   int currentMenu = -1;
   msgBox messageBox;
   int signalToSend{};  // defaults to 0
   int signalKillRet{}; // defaults to 0

   const array<string, 32> P_Signals = {
	   "0",
#ifdef __linux__
#if defined(__hppa__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGSTKFLT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGXCPU",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGVTALRM",
		"SIGPROF", "SIGIO", "SIGWINCH", "SIGSTOP",
		"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU",
		"SIGURG", "SIGXFSZ", "SIGSYS"
#elif defined(__mips__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGWINCH",
		"SIGURG", "SIGIO", "SIGSTOP", "SIGTSTP",
		"SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM",
		"SIGPROF", "SIGXCPU", "SIGXFSZ"
#elif defined(__alpha__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGPWR", "SIGUSR1", "SIGUSR2"
#elif defined (__sparc__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGLOST", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT",	"SIGQUIT",	"SIGILL",
		"SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE",
		"SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT",
		"SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP",
		"SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGIO", "SIGPWR", "SIGSYS"
#endif
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGINFO", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "7", "SIGFPE",
		"SIGKILL", "10", "SIGSEGV", "12",
		"SIGPIPE", "SIGALRM", "SIGTERM", "16",
		"17", "18", "19", "20",
		"21", "22", "23", "24",
		"25", "26", "27", "28",
		"29", "30", "31"
#endif
	};

  unordered_flat_map<string, Input::Mouse_loc> mouse_mappings;

   const array<array<string, 3>, 3> menu_normal = {
		array<string, 3>{
			"┌─┐┌─┐┌┬┐┬┌─┐┌┐┌┌─┐",
			"│ │├─┘ │ ││ ││││└─┐",
			"└─┘┴   ┴ ┴└─┘┘└┘└─┘"
		},
		{
			"┬ ┬┌─┐┬  ┌─┐",
			"├─┤├┤ │  ├─┘",
			"┴ ┴└─┘┴─┘┴  "
		},
		{
			"┌─┐ ┬ ┬ ┬┌┬┐",
			"│─┼┐│ │ │ │ ",
			"└─┘└└─┘ ┴ ┴ "
		}
	};

	const array<array<string, 3>, 3> menu_selected = {
		array<string, 3>{
			"╔═╗╔═╗╔╦╗╦╔═╗╔╗╔╔═╗",
			"║ ║╠═╝ ║ ║║ ║║║║╚═╗",
			"╚═╝╩   ╩ ╩╚═╝╝╚╝╚═╝"
		},
		{
			"╦ ╦╔═╗╦  ╔═╗",
			"╠═╣║╣ ║  ╠═╝",
			"╩ ╩╚═╝╩═╝╩  "
		},
		{
			"╔═╗ ╦ ╦ ╦╔╦╗ ",
			"║═╬╗║ ║ ║ ║  ",
			"╚═╝╚╚═╝ ╩ ╩  "
		}
	};

	const array<int, 3> menu_width = {19, 12, 12};

	const string help_text[][2] = {
		{"Mouse 1", "Clicks buttons and selects in process list."},
		{"Mouse scroll", "Scrolls any scrollable list/text under cursor."},
		{"Esc, m", "Toggles main menu."},
		{"p", "Cycle view presets forwards."},
		{"shift + p", "Cycle view presets backwards."},
		{"1", "Toggle CPU box."},
		{"2", "Toggle MEM box."},
		{"3", "Toggle NET box."},
		{"4", "Toggle PROC box."},
		{"d", "Toggle disks view in MEM box."},
		{"F2, o", "Shows options."},
		{"F1, ?, h", "Shows this window."},
		{"ctrl + z", "Sleep program and put in background."},
		{"q, ctrl + c", "Quits program."},
		{"+, -", "Add/Subtract 100ms to/from update timer."},
		{"Up, Down", "Select in process list."},
		{"Enter", "Show detailed information for selected process."},
		{"Spacebar", "Expand/collapse the selected process in tree view."},
		{"Pg Up, Pg Down", "Jump 1 page in process list."},
		{"Home, End", "Jump to first or last page in process list."},
		{"Left, Right", "Select previous/next sorting column."},
		{"b, n", "Select previous/next network device."},
		{"i", "Toggle disks io mode with big graphs."},
		{"z", "Toggle totals reset for current network device"},
		{"a", "Toggle auto scaling for the network graphs."},
		{"y", "Toggle synced scaling mode for network graphs."},
		{"f, /", "To enter a process filter."},
		{"delete", "Clear any entered filter."},
		{"c", "Toggle per-core cpu usage of processes."},
		{"r", "Reverse sorting order in processes box."},
		{"e", "Toggle processes tree view."},
		{"%", "Toggles memory display mode in processes box."},
		{"Selected +, -", "Expand/collapse the selected process in tree view."},
		{"Selected t", "Terminate selected process with SIGTERM - 15."},
		{"Selected k", "Kill selected process with SIGKILL - 9."},
		{"Selected s", "Select or enter signal to send to process."},
		{"", " "},
		{"", "For bug reporting and project updates, visit:"},
		{"", "https://github.com/aristocratos/btop"},
	};

	const std::array<vector<MenuEntry>, 5> categories{{
		{
			MENU_BROWSABLE_ENTRY(color_theme, Theme::themes,
					"Set color theme.",
					"",
					"Choose from all theme files in (usually)",
					"\"/usr/[local/]share/btop/themes\" and",
					"\"~/.config/btop/themes\".",
					"",
					"\"Default\" for builtin default theme.",
					"\"TTY\" for builtin 16-color theme.",
					"",
					"For theme updates see:",
					"https://github.com/aristocratos/btop"),
			MENU_ENTRY(theme_background,
					"If the theme set background should be shown.",
					"",
					"Set to False if you want terminal background",
					"transparency."),
			MENU_ENTRY(truecolor,
					"Sets if 24-bit truecolor should be used.",
					"",
					"Will convert 24-bit colors to 256 color",
					"(6x6x6 color cube) if False.",
					"",
					"Set to False if your terminal doesn't have",
					"truecolor support and can't convert to",
					"256-color."),
			MENU_ENTRY(force_tty,
					"TTY mode.",
					"",
					"Set to true to force tty mode regardless",
					"if a real tty has been detected or not.",
					"",
					"Will force 16-color mode and TTY theme,",
					"set all graph symbols to \"tty\" and swap",
					"out other non tty friendly symbols."),
			MENU_ENTRY(vim_keys,
					"Enable vim keys.",
					"Set to True to enable \"h,j,k,l\" keys for",
					"directional control in lists.",
					"",
					"Conflicting keys for",
					"h (help) and k (kill)",
					"is accessible while holding shift."),
			MENU_ENTRY(presets,
					"Define presets for the layout of the boxes.",
					"",
					"Preset 0 is always all boxes shown with",
					"default settings.",
					"Max 9 presets.",
					"",
					"Format: \"box_name:P:G,box_name:P:G\"",
					"P=(0 or 1) for alternate positions.",
					"G=graph symbol to use for box.",
					"",
					"Use whitespace \" \" as separator between",
					"different presets.",
					"",
					"Example:",
					"\"mem:0:tty,proc:1:default cpu:0:braille\""),
			MENU_ENTRY(shown_boxes,
					"Manually set which boxes to show.",
					"",
					"Available values are \"cpu mem net proc\".",
					"Separate values with whitespace.",
					"",
					"Toggle between presets with key \"p\"."),
			MENU_ENTRY(update_ms,
					"Update time in milliseconds.",
					"",
					"Recommended 2000 ms or above for better",
					"sample times for graphs.",
					"",
					"Min value: 100 ms",
					"Max value: 86400000 ms = 24 hours."),
			MENU_ENTRY(rounded_corners,
					"Rounded corners on boxes.",
					"",
					"True or False",
					"",
					"Is always False if TTY mode is ON."),
			MENU_BROWSABLE_ENTRY(graph_symbol, Config::valid_graph_symbols,
					"Default symbols to use for graph creation.",
					"",
					"\"braille\", \"block\" or \"tty\".",
					"",
					"\"braille\" offers the highest resolution but",
					"might not be included in all fonts.",
					"",
					"\"block\" has half the resolution of braille",
					"but uses more common characters.",
					"",
					"\"tty\" uses only 3 different symbols but will",
					"work with most fonts.",
					"",
					"Note that \"tty\" only has half the horizontal",
					"resolution of the other two,",
					"so will show a shorter historical view."),
			MENU_ENTRY(clock_format,
					"Draw a clock at top of screen.",
					"(Only visible if cpu box is enabled!)",
					"",
					"Formatting according to strftime, empty",
					"string to disable.",
					"",
					"Custom formatting options:",
					"\"/host\" = hostname",
					"\"/user\" = username",
					"\"/uptime\" = system uptime",
					"",
					"Examples of strftime formats:",
					"\"%X\" = locale HH:MM:SS",
					"\"%H\" = 24h hour, \"%I\" = 12h hour",
					"\"%M\" = minute, \"%S\" = second",
					"\"%d\" = day, \"%m\" = month, \"%y\" = year"),
			MENU_ENTRY(base_10_sizes,
					"Use base 10 for bits and bytes sizes.",
					"",
					"Uses KB = 1000 instead of KiB = 1024,",
					"MB = 1000KB instead of MiB = 1024KiB,",
					"and so on.",
					"",
					"True or False."),
			MENU_ENTRY(background_update,
					"Update main ui when menus are showing.",
					"",
					"True or False.",
					"",
					"Set this to false if the menus is flickering",
					"too much for a comfortable experience."),
			MENU_ENTRY(show_battery,
					"Show battery stats.",
					"(Only visible if cpu box is enabled!)",
					"",
					"Show battery stats in the top right corner",
					"if a battery is present."),
			MENU_BROWSABLE_ENTRY(selected_battery, Config::available_batteries,
					"Select battery.",
					"",
					"Which battery to use if multiple are present.",
					"Can be both batteries and UPS.",
					"",
					"\"Auto\" for auto detection."),
			MENU_BROWSABLE_ENTRY(log_level, Logger::log_levels,
					"Set loglevel for error.log",
					"",
					"\"ERROR\", \"WARNING\", \"INFO\" and \"DEBUG\".",
					"",
					"The level set includes all lower levels,",
					"i.e. \"DEBUG\" will show all logging info."),
		},
		{
			MENU_ENTRY(cpu_bottom,
					"Cpu box location.",
					"",
					"Show cpu box at bottom of screen instead",
					"of top."),
			MENU_BROWSABLE_ENTRY(graph_symbol_cpu, Config::valid_graph_symbols_def,
					"Graph symbol to use for graphs in cpu box.",
					"",
					"\"default\", \"braille\", \"block\" or \"tty\".",
					"",
					"\"default\" for the general default symbol.",),
			MENU_BROWSABLE_ENTRY(cpu_graph_upper, Cpu::available_fields,
					"Cpu upper graph.",
					"",
					"Sets the CPU stat shown in upper half of",
					"the CPU graph.",
					"",
					"\"total\" = Total cpu usage.",
					"\"user\" = User mode cpu usage.",
					"\"system\" = Kernel mode cpu usage.",
					"+ more depending on kernel."),
			MENU_BROWSABLE_ENTRY(cpu_graph_lower, Cpu::available_fields,
					"Cpu lower graph.",
					"",
					"Sets the CPU stat shown in lower half of",
					"the CPU graph.",
					"",
					"\"total\" = Total cpu usage.",
					"\"user\" = User mode cpu usage.",
					"\"system\" = Kernel mode cpu usage.",
					"+ more depending on kernel."),
			MENU_ENTRY(cpu_invert_lower,
					"Toggles orientation of the lower CPU graph.",
					"",
					"True or False."),
			MENU_ENTRY(cpu_single_graph,
					"Completely disable the lower CPU graph.",
					"",
					"Shows only upper CPU graph and resizes it",
					"to fit to box height.",
					"",
					"True or False."),
			MENU_ENTRY(check_temp,
					"Enable cpu temperature reporting.",
					"",
					"True or False."),
			MENU_BROWSABLE_ENTRY(cpu_sensor, Cpu::available_sensors,
					"Cpu temperature sensor",
					"",
					"Select the sensor that corresponds to",
					"your cpu temperature.",
					"",
					"Set to \"Auto\" for auto detection."),
			MENU_ENTRY(show_coretemp,
					"Show temperatures for cpu cores.",
					"",
					"Only works if check_temp is True and",
					"the system is reporting core temps."),
			MENU_ENTRY(cpu_core_map,
					"Custom mapping between core and coretemp.",
					"",
					"Can be needed on certain cpus to get correct",
					"temperature for correct core.",
					"",
					"Use lm-sensors or similar to see which cores",
					"are reporting temperatures on your machine.",
					"",
					"Format: \"X:Y\"",
					"X=core with wrong temp.",
					"Y=core with correct temp.",
					"Use space as separator between multiple",
					"entries.",
					"",
					"Example: \"4:0 5:1 6:3\""),
			MENU_BROWSABLE_ENTRY(temp_scale, Config::temp_scales,
					"Which temperature scale to use.",
					"",
					"Celsius, default scale.",
					"",
					"Fahrenheit, the american one.",
					"",
					"Kelvin, 0 = absolute zero, 1 degree change",
					"equals 1 degree change in Celsius.",
					"",
					"Rankine, 0 = abosulte zero, 1 degree change",
					"equals 1 degree change in Fahrenheit."),
			MENU_ENTRY(show_cpu_freq,
					"Show CPU frequency",
					"",
					"Can cause slowdowns on systems with many",
					"cores and certain kernel versions."),
			MENU_ENTRY(custom_cpu_name,
					"Custom cpu model name in cpu percentage box.",
					"",
					"Empty string to disable."),
			MENU_ENTRY(show_uptime,
					"Shows the system uptime in the CPU box.",
					"",
					"Can also be shown in the clock by using",
					"\"/uptime\" in the formatting.",
					"",
					"True or False."),
		},
		{
			MENU_ENTRY(mem_below_net,
					"Mem box location.",
					"",
					"Show mem box below net box instead of above."),
			MENU_BROWSABLE_ENTRY(graph_symbol_mem, Config::valid_graph_symbols_def,
					"Graph symbol to use for graphs in mem box.",
					"",
					"\"default\", \"braille\", \"block\" or \"tty\".",
					"",
					"\"default\" for the general default symbol.",),
			MENU_ENTRY(mem_graphs,
					"Show graphs for memory values.",
					"",
					"True or False."),
			MENU_ENTRY(show_disks,
					"Split memory box to also show disks.",
					"",
					"True or False."),
			MENU_ENTRY(show_io_stat,
					"Toggle IO activity graphs.",
					"",
					"Show small IO graphs that for disk activity",
					"(disk busy time) when not in IO mode.",
					"",
					"True or False."),
			MENU_ENTRY(io_mode,
					"Toggles io mode for disks.",
					"",
					"Shows big graphs for disk read/write speeds",
					"instead of used/free percentage meters.",
					"",
					"True or False."),
			MENU_ENTRY(io_graph_combined,
					"Toggle combined read and write graphs.",
					"",
					"Only has effect if \"io mode\" is True.",
					"",
					"True or False."),
			MENU_ENTRY(io_graph_speeds,
					"Set top speeds for the io graphs.",
					"",
					"Manually set which speed in MiB/s that",
					"equals 100 percent in the io graphs.",
					"(100 MiB/s by default).",
					"",
					"Format: \"device:speed\" separate disks with",
					"whitespace \" \".",
					"",
					"Example: \"/dev/sda:100, /dev/sdb:20\"."),
			MENU_ENTRY(show_swap,
					"If swap memory should be shown in memory box.",
					"",
					"True or False."),
			MENU_ENTRY(swap_disk,
					"Show swap as a disk.",
					"",
					"Ignores show_swap value above.",
					"Inserts itself after first disk."),
			MENU_ENTRY(only_physical,
					"Filter out non physical disks.",
					"",
					"Set this to False to include network disks,",
					"RAM disks and similar.",
					"",
					"True or False."),
			MENU_ENTRY(use_fstab,
					"(Linux) Read disks list from /etc/fstab.",
					"",
					"This also disables only_physical.",
					"",
					"True or False."),
			MENU_ENTRY(zfs_hide_datasets,
					"(Linux) Hide ZFS datasets in disks list.",
					"",
					"Setting this to True will hide all datasets,",
					"and only show ZFS pools.",
					"",
					"(IO stats will be calculated per-pool)",
					"",
					"True or False."),
			MENU_ENTRY(disk_free_priv,
					"(Linux) Type of available disk space.",
					"",
					"Set to true to show how much disk space is",
					"available for privileged users.",
					"",
					"Set to false to show available for normal",
					"users."),
			MENU_ENTRY(disks_filter,
					"Optional filter for shown disks.",
					"",
					"Should be full path of a mountpoint.",
					"Separate multiple values with",
					"whitespace \" \".",
					"",
					"Begin line with \"exclude=\" to change to",
					"exclude filter.",
					"Oterwise defaults to \"most include\" filter.",
					"",
					"Example:",
					"\"exclude=/boot /home/user\""),
			MENU_ENTRY(zfs_arc_cached,
					"(Linux) Count ZFS ARC as cached memory.",
					"",
					"Add ZFS ARC used to cached memory and",
					"ZFS ARC available to available memory.",
					"These are otherwise reported by the Linux",
					"kernel as used memory.",
					"",
					"True or False."),
		},
		{
			MENU_BROWSABLE_ENTRY(graph_symbol_net, Config::valid_graph_symbols_def,
					"Graph symbol to use for graphs in net box.",
					"",
					"\"default\", \"braille\", \"block\" or \"tty\".",
					"",
					"\"default\" for the general default symbol.",),
			MENU_ENTRY(net_download,
					"Fixed network graph download value.",
					"",
					"Value in Mebibits, default \"100\".",
					"",
					"Can be toggled with auto button."),
			MENU_ENTRY(net_upload,
					"Fixed network graph upload value.",
					"",
					"Value in Mebibits, default \"100\".",
					"",
					"Can be toggled with auto button."),
			MENU_ENTRY(net_auto,
					"Start in network graphs auto rescaling mode.",
					"",
					"Ignores any values set above at start and",
					"rescales down to 10Kibibytes at the lowest.",
					"",
					"True or False."),
			MENU_ENTRY(net_sync,
					"Network scale sync.",
					"",
					"Syncs the scaling for download and upload to",
					"whichever currently has the highest scale.",
					"",
					"True or False."),
			MENU_ENTRY(net_iface,
					"Network Interface.",
					"",
					"Manually set the starting Network Interface.",
					"",
					"Will otherwise automatically choose the NIC",
					"with the highest total download since boot."),
		},
		{
			MENU_ENTRY(proc_left,
					"Proc box location.",
					"",
					"Show proc box on left side of screen",
					"instead of right."),
			MENU_BROWSABLE_ENTRY(graph_symbol_proc, Config::valid_graph_symbols_def,
					"Graph symbol to use for graphs in proc box.",
					"",
					"\"default\", \"braille\", \"block\" or \"tty\".",
					"",
					"\"default\" for the general default symbol.",),
			MENU_BROWSABLE_ENTRY(proc_sorting, Proc::sort_vector,
					"Processes sorting option.",
					"",
					"Possible values:",
					"\"pid\", \"program\", \"arguments\", \"threads\",",
					"\"user\", \"memory\", \"cpu lazy\" and",
					"\"cpu direct\".",
					"",
					"\"cpu lazy\" updates top process over time.",
					"\"cpu direct\" updates top process",
					"directly."),
			MENU_ENTRY(proc_reversed,
					"Reverse processes sorting order.",
					"",
					"True or False."),
			MENU_ENTRY(proc_tree,
					"Processes tree view.",
					"",
					"Set true to show processes grouped by",
					"parents with lines drawn between parent",
					"and child process."),
			MENU_ENTRY(proc_aggregate,
					"Aggregate child's resources in parent.",
					"",
					"In tree-view, include all child resources",
					"with the parent even while expanded."),
			MENU_ENTRY(proc_colors,
					"Enable colors in process view.",
					"",
					"True or False."),
			MENU_ENTRY(proc_gradient,
					"Enable process view gradient fade.",
					"",
					"Fades from top or current selection.",
					"Max fade value is equal to current themes",
					"\"inactive_fg\" color value."),
			MENU_ENTRY(proc_per_core,
					"Process usage per core.",
					"",
					"If process cpu usage should be of the core",
					"it's running on or usage of the total",
					"available cpu power.",
					"",
					"If true and process is multithreaded",
					"cpu usage can reach over 100%."),
			MENU_ENTRY(proc_mem_bytes,
					"Show memory as bytes in process list.",
					" ",
					"Will show percentage of total memory",
					"if False."),
			MENU_ENTRY(proc_cpu_graphs,
					"Show cpu graph for each process.",
					"",
					"True or False"),
			MENU_ENTRY(proc_filter_kernel,
					"(Linux) Filter kernel processes from output.",
					"",
					"Set to 'True' to filter out internal",
					"processes started by the Linux kernel."),
		},
	}};

	msgBox::msgBox() {}
	msgBox::msgBox(int width, int boxtype, vector<string> content, string title)
	: width(width), boxtype(boxtype) {
		auto& config = Config::get();
		auto tty_mode = config.tty_mode;
		auto rounded = config.rounded_corners;
		const auto& right_up = (tty_mode or not rounded ? Symbols::right_up : Symbols::round_right_up);
		const auto& left_up = (tty_mode or not rounded ? Symbols::left_up : Symbols::round_left_up);
		const auto& right_down = (tty_mode or not rounded ? Symbols::right_down : Symbols::round_right_down);
		const auto& left_down = (tty_mode or not rounded ? Symbols::left_down : Symbols::round_left_down);
		height = content.size() + 7;
		x = Term::width / 2 - width / 2;
		y = Term::height/2 - height/2;
		if (boxtype == 2) selected = 1;


		button_left = left_up + Symbols::h_line * 6 + Mv::l(7) + Mv::d(2) + left_down + Symbols::h_line * 6 + Mv::l(7) + Mv::u(1) + Symbols::v_line;
		button_right = Symbols::v_line + Mv::l(7) + Mv::u(1) + Symbols::h_line * 6 + right_up + Mv::l(7) + Mv::d(2) + Symbols::h_line * 6 + right_down + Mv::u(2);

		box_contents = Draw::createBox(x, y, width, height, Theme::c("hi_fg"), true, title) + Mv::d(1);
		for (const auto& line : content) {
			box_contents += Mv::save + Mv::r(max((size_t)0, (width / 2) - (Fx::uncolor(line).size() / 2) - 1)) + line + Mv::restore + Mv::d(1);
		}
	}

	string msgBox::operator()() {
		string out;
		int pos = width / 2 - (boxtype == 0 ? 6 : 14);
		auto& first_color = (selected == 0 ? Theme::c("hi_fg") : Theme::c("div_line"));
		out = Mv::d(1) + Mv::r(pos) + Fx::b + first_color + button_left + (selected == 0 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
			+ (boxtype == 0 ? "    Ok    " : "    Yes    ") + first_color + button_right;
		mouse_mappings["button1"] = Input::Mouse_loc{y + height - 4, x + pos + 1, 3, 12 + (boxtype > 0 ? 1 : 0)};
		if (boxtype > 0) {
			auto& second_color = (selected == 1 ? Theme::c("hi_fg") : Theme::c("div_line"));
			out += Mv::r(2) + second_color + button_left + (selected == 1 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
				+ "    No    " + second_color + button_right;
			mouse_mappings["button2"] = Input::Mouse_loc{y + height - 4, x + pos + 15 + (boxtype > 0 ? 1 : 0), 3, 12};
		}
		return box_contents + out + Fx::reset;
	}

	//? Process input
	int msgBox::input(string key) {
		if (key.empty()) return Invalid;

		if (is_in(key, "escape", "backspace", "q") or key == "button2") {
			return No_Esc;
		}
		else if (key == "button1" or (boxtype == 0 and str_to_upper(key) == "O")) {
			return Ok_Yes;
		}
		else if (is_in(key, "enter", "space")) {
			return selected + 1;
		}
		else if (boxtype == 0) {
			return Invalid;
		}
		else if (str_to_upper(key) == "Y") {
			return Ok_Yes;
		}
		else if (str_to_upper(key) == "N") {
			return No_Esc;
		}
		else if (is_in(key, "right", "tab")) {
			if (++selected > 1) selected = 0;
			return Select;
		}
		else if (is_in(key, "left", "shift_tab")) {
			if (--selected < 0) selected = 1;
			return Select;
		}

		return Invalid;
	}

	void msgBox::clear() {
		box_contents.clear();
		box_contents.shrink_to_fit();
		button_left.clear();
		button_left.shrink_to_fit();
		button_right.clear();
		button_right.shrink_to_fit();
		if (mouse_mappings.contains("button1")) mouse_mappings.erase("button1");
		if (mouse_mappings.contains("button2")) mouse_mappings.erase("button2");
	}

	enum menuReturnCodes {
		NoChange,
		Changed,
		Closed,
		Switch
	};

	int signalChoose(const string& key) {
		auto& config = Config::get();
		auto s_pid = (config.show_detailed and config.selected_pid == 0 ? config.detailed_pid : config.selected_pid);
		static int x{}; // defaults to 0
		static int y{}; // defaults to 0
		static int selected_signal = -1;

		if (bg.empty()) selected_signal = -1;
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 40;
			y = Term::height/2 - 9;
			bg = Draw::createBox(x + 2, y, 78, 19, Theme::c("hi_fg"), true, "signals");
			bg += Mv::to(y+2, x+3) + Theme::c("title") + Fx::b + cjust("Send signal to PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == config.detailed_pid ? Proc::detailed.entry.name : config.selected_name), 30) + ")", 76);
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			if (int new_select = stoi(key.substr(7)); new_select == selected_signal)
				goto ChooseEntering;
			else
				selected_signal = new_select;
		}
		else if (is_in(key, "enter", "space") and selected_signal >= 0) {
			ChooseEntering:
			signalKillRet = 0;
			if (s_pid < 1) {
				signalKillRet = ESRCH;
				menuMask.set(SignalReturn);
			}
			else if (kill(s_pid, selected_signal) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			return Closed;
		}
		else if (key.size() == 1 and isdigit(key.at(0)) and selected_signal < 10) {
			selected_signal = std::min(std::stoi((selected_signal < 1 ? key : to_string(selected_signal) + key)), 64);
		}
		else if (key == "backspace" and selected_signal != -1) {
			selected_signal = (selected_signal < 10 ? -1 : selected_signal / 10);
		}
		else if (is_in(key, "up", "k") and selected_signal != 16) {
			if (selected_signal == 1) selected_signal = 31;
			else if (selected_signal < 6) selected_signal += 25;
			else {
				bool offset = (selected_signal > 16);
				selected_signal -= 5;
				if (selected_signal <= 16 and offset) selected_signal--;
			}
		}
		else if (is_in(key, "down", "j")) {
			if (selected_signal == 31) selected_signal = 1;
			else if (selected_signal < 1 or selected_signal == 16) selected_signal = 1;
			else if (selected_signal > 26) selected_signal -= 25;
			else {
				bool offset = (selected_signal < 16);
				selected_signal += 5;
				if (selected_signal >= 16 and offset) selected_signal++;
				if (selected_signal > 31) selected_signal = 31;
			}
		}
		else if (is_in(key, "left", "h") and selected_signal > 0 and selected_signal != 16) {
			if (--selected_signal < 1) selected_signal = 31;
			else if (selected_signal == 16) selected_signal--;
		}
		else if (is_in(key, "right", "l") and selected_signal <= 31 and selected_signal != 16) {
			if (++selected_signal > 31) selected_signal = 1;
			else if (selected_signal == 16) selected_signal++;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y+4, cx = x+4;
			out = bg + Mv::to(cy++, x+3) + Theme::c("main_fg") + Fx::ub
				+ rjust("Enter signal number: ", 48) + Theme::c("hi_fg") + (selected_signal >= 0 ? to_string(selected_signal) : "") + Theme::c("main_fg") + Fx::bl + "█" + Fx::ubl;

			auto sig_str = to_string(selected_signal);
			for (int count = 0, i = 0; const auto& sig : P_Signals) {
				if (count == 0 or count == 16) { count++; continue; }
				if (i++ % 5 == 0) { ++cy; cx = x+4; }
				out += Mv::to(cy, cx);
				if (count == selected_signal) out += Theme::c("selected_bg") + Theme::c("selected_fg") + Fx::b + ljust(to_string(count), 3) + ljust('(' + sig + ')', 12) + Fx::reset;
				else out += Theme::c("hi_fg") + ljust(to_string(count), 3) + Theme::c("main_fg") + ljust('(' + sig + ')', 12);
				if (redraw) mouse_mappings["button_" + to_string(count)] = {cy, cx, 1, 15};
				count++;
				cx += 15;
			}

			cy++;
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "↑ ↓ ← →", 33, true) + Theme::c("main_fg") + Fx::ub + " | To choose signal.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("0-9", 33) + Theme::c("main_fg") + Fx::ub + " | Enter manually.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ENTER", 33) + Theme::c("main_fg") + Fx::ub + " | To send signal.";
			mouse_mappings["enter"] = {cy, x, 1, 73};
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ESC or \"q\"", 33) + Theme::c("main_fg") + Fx::ub + " | To abort.";
			mouse_mappings["escape"] = {cy, x, 1, 73};

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

	int sizeError(const string& key) {
		if (redraw) {
			vector<string> cont_vec;
			cont_vec.push_back(Fx::b + Theme::g("used")[100] + "Error:" + Theme::c("main_fg") + Fx::ub);
			cont_vec.push_back("Terminal size to small to" + Fx::reset);
			cont_vec.push_back("display menu or box!" + Fx::reset);

			messageBox = Menu::msgBox{45, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	int signalSend(const string& key) {
		auto& config = Config::get();
		auto s_pid = (config.show_detailed and config.selected_pid == 0 ? config.detailed_pid : config.selected_pid);
		if (s_pid == 0) return Closed;
		if (redraw) {
			atomic_wait(Runner::active);
			auto& p_name = (s_pid == config.detailed_pid ? Proc::detailed.entry.name : config.selected_name);
			vector<string> cont_vec = {
				Fx::b + Theme::c("main_fg") + "Send signal: " + Fx::ub + Theme::c("hi_fg") + to_string(signalToSend)
				+ (signalToSend > 0 and signalToSend <= 32 ? Theme::c("main_fg") + " (" + P_Signals.at(signalToSend) + ')' : ""),

				Fx::b + Theme::c("main_fg") + "To PID: " + Fx::ub + Theme::c("hi_fg") + to_string(s_pid) + Theme::c("main_fg") + " ("
				+ uresize(p_name, 16) + ')' + Fx::reset,
			};
			messageBox = Menu::msgBox{50, 1, cont_vec, (signalToSend > 1 and signalToSend <= 32 and signalToSend != 17 ? P_Signals.at(signalToSend) : "signal")};
			Global::overlay = messageBox();
		}
		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes) {
			signalKillRet = 0;
			if (kill(s_pid, signalToSend) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::Select) {
			Global::overlay = messageBox();
			return Changed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	int signalReturn(const string& key) {
		if (redraw) {
			vector<string> cont_vec;
			cont_vec.push_back(Fx::b + Theme::g("used")[100] + "Failure:" + Theme::c("main_fg") + Fx::ub);
			if (signalKillRet == EINVAL) {
				cont_vec.push_back("Unsupported signal!" + Fx::reset);
			}
			else if (signalKillRet == EPERM) {
				cont_vec.push_back("Insufficient permissions to send signal!" + Fx::reset);
			}
			else if (signalKillRet == ESRCH) {
				cont_vec.push_back("Process not found!" + Fx::reset);
			}
			else {
				cont_vec.push_back("Unknown error! (errno: " + to_string(signalKillRet) + ')' + Fx::reset);
			}

			messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	int mainMenu(const string& key) {
		auto& config = Config::get();
		enum MenuItems { Options, Help, Quit };
		static int y{};         // defaults to 0
		static int selected{};  // defaults to 0
		static vector<string> colors_selected;
		static vector<string> colors_normal;
		auto tty_mode = config.tty_mode;
		if (bg.empty()) selected = 0;
		int retval = Changed;

		if (redraw) {
			y = Term::height/2 - 10;
			bg = Draw::banner_gen(y, 0, true);
			if (not tty_mode) {
				colors_selected = {
					Theme::hex_to_color(Global::Banner_src.at(0).at(0)),
					Theme::hex_to_color(Global::Banner_src.at(2).at(0)),
					Theme::hex_to_color(Global::Banner_src.at(4).at(0))
				};
				colors_normal = {
					Theme::hex_to_color("#CC"),
					Theme::hex_to_color("#AA"),
					Theme::hex_to_color("#80")
				};
			}
		}
		else if (is_in(key, "escape", "q", "m", "mouse_click")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			if (int new_select = key.back() - '0'; new_select == selected)
				goto MainEntering;
			else
				selected = new_select;
		}
		else if (is_in(key, "enter", "space")) {
			MainEntering:
			switch (selected) {
				case Options:
					menuMask.set(Menus::Options);
					currentMenu = Menus::Options;
					return Switch;
				case Help:
					menuMask.set(Menus::Help);
					currentMenu = Menus::Help;
					return Switch;
				case Quit:
					clean_quit(0);
			}
		}
		else if (is_in(key, "down", "tab", "mouse_scroll_down", "j")) {
			if (++selected > 2) selected = 0;
		}
		else if (is_in(key, "up", "shift_tab", "mouse_scroll_up", "k")) {
			if (--selected < 0) selected = 2;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg + Fx::reset + Fx::b;
			auto cy = y + 7;
			for (const auto& i : iota(0, 3)) {
				if (tty_mode) out += (i == selected ? Theme::c("hi_fg") : Theme::c("main_fg"));
				const auto& menu = (not tty_mode and i == selected ? menu_selected[i] : menu_normal[i]);
				const auto& colors = (i == selected ? colors_selected : colors_normal);
				if (redraw) mouse_mappings["button_" + to_string(i)] = {cy, Term::width/2 - menu_width[i]/2, 3, menu_width[i]};
				for (int ic = 0; const auto& line : menu) {
					out += Mv::to(cy++, Term::width/2 - menu_width[i]/2) + (tty_mode ? "" : colors[ic++]) + line;
				}
			}
			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

	int optionsMenu(const string& key) {
		static int y{};                 // defaults to 0
		static int x{};                 // defaults to 0
		static int height{};            // defaults to 0
		static int page{};              // defaults to 0
		static int pages{};             // defaults to 0
		static int selected{};          // defaults to 0
		static int select_max{};        // defaults to 0
		static int item_height{};       // defaults to 0
		static int selected_cat{};      // defaults to 0
		static int max_items{};         // defaults to 0
		static int last_sel{};          // defaults to 0
		static bool editing{};          // defaults to false
		static Draw::TextEdit editor;
		static string warnings;
		static bitset<8> selPred;
		auto& config = Config::get();
		auto tty_mode = config.tty_mode;
		auto vim_keys = config.vim_keys;
		if (max_items == 0) {
			for (const auto& cat : categories) {
				if ((int)cat.size() > max_items) max_items = cat.size();
			}
		}
		if (bg.empty()) {
			page = selected = selected_cat = last_sel = 0;
			redraw = true;
			Theme::updateThemes();
		}
		int retval = Changed;
		bool recollect{};       // defaults to false
		bool screen_redraw{};   // defaults to false
		bool theme_refresh{};   // defaults to false

		//? Draw background if needed else process input
		if (redraw) {
			mouse_mappings.clear();
			selPred.reset();
			y = max(1, Term::height/2 - 3 - max_items);
			x = Term::width/2 - 39;
			height = min(Term::height - 7, max_items * 2 + 4);
			if (height % 2 != 0) height--;
			bg 	= Draw::banner_gen(y, 0, true)
				+ Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, "tab" + Symbols::right)
				+ Mv::to(y+8, x) + Theme::c("hi_fg") + Symbols::div_left + Theme::c("div_line") + Symbols::h_line * 29
				+ Symbols::div_up + Symbols::h_line * (78 - 32) + Theme::c("hi_fg") + Symbols::div_right
				+ Mv::to(y+6+height - 1, x+30) + Symbols::div_down + Theme::c("div_line");
			for (const auto& i : iota(0, height - 4)) {
				bg += Mv::to(y+9 + i, x + 30) + Symbols::v_line;
			}
		}
		else if (not warnings.empty() and not key.empty()) {
			auto ret = messageBox.input(key);
			if (ret == msgBox::msgReturn::Ok_Yes or ret == msgBox::msgReturn::No_Esc) {
				warnings.clear();
				messageBox.clear();
			}
		}
		else if (editing and not key.empty()) {
			if (is_in(key, "escape", "mouse_click")) {
				editor.clear();
				editing = false;
			}
			else if (key == "enter") {
				const auto& option = categories[selected_cat][item_height * page + selected];
				int intValue;
				if (selPred.test(isString) and Config::validate(option.offset, editor.text)) {
					Config::set<string>(option.offset, editor.text, true);
					switch(option.offset) {
						case CONFIG_OFFSET(custom_cpu_name):
							screen_redraw = true;
							break;
						case CONFIG_OFFSET(shown_boxes):
						case CONFIG_OFFSET(presets):
							screen_redraw = true;
							Config::current_preset = -1;
							break;
						case CONFIG_OFFSET(clock_format):
							Draw::update_clock(true);
							screen_redraw = true;
							break;
						case CONFIG_OFFSET(cpu_core_map):
							atomic_wait(Runner::active);
							Cpu::core_mapping = Cpu::get_core_mapping();
							break;
					}
				}
				else if (selPred.test(isInt) and Config::to_int(editor.text, intValue) and Config::validate(option.offset, intValue)) {
					Config::set<int>(option.offset, intValue, true);
				}
				else
					warnings = Config::validError;

				editor.clear();
				editing = false;
			}
			else if (not editor.command(key))
				retval = NoChange;
		}
		else if (key == "mouse_click") {
			const auto [mouse_x, mouse_y] = Input::mouse_pos;
			if (mouse_x < x or mouse_x > x + 80 or mouse_y < y + 6 or mouse_y > y + 6 + height) {
				return Closed;
			}
			else if (mouse_x < x + 30 and mouse_y > y + 8) {
				auto m_select = ceil((double)(mouse_y - y - 8) / 2) - 1;
				if (selected != m_select)
					selected = m_select;
				else if (selPred.test(isEditable))
					goto mouseEnter;
				else retval = NoChange;
			}
		}
		else if (is_in(key, "enter", "e", "E") and selPred.test(isEditable)) {
			mouseEnter:
			const auto& option = categories[selected_cat][item_height * page + selected];
			editor = Draw::TextEdit{Config::getAsString(option.offset), selPred.test(isInt)};
			editing = true;
			mouse_mappings.clear();
		}
		else if (is_in(key, "escape", "q", "o", "backspace")) {
			return Closed;
		}
		else if (is_in(key, "down", "mouse_scroll_down") or (vim_keys and key == "j")) {
			if (++selected > select_max or selected >= item_height) {
				if (page < pages - 1) page++;
				else if (pages > 1) page = 0;
				selected = 0;
			}
		}
		else if (is_in(key, "up", "mouse_scroll_up") or (vim_keys and key == "k")) {
			if (--selected < 0) {
				if (page > 0) page--;
				else if (pages > 1) page = pages - 1;

				selected = item_height - 1;
			}
		}
		else if (pages > 1 and key == "page_down") {
			if (++page >= pages) page = 0;
			selected = 0;
		}
		else if (pages > 1 and key == "page_up") {
			if (--page < 0) page = pages - 1;
			selected = 0;
		}
		else if (key == "tab") {
			if (++selected_cat >= (int)categories.size()) selected_cat = 0;
			page = selected = 0;
		}
		else if (key == "shift_tab") {
			if (--selected_cat < 0) selected_cat = (int)categories.size() - 1;
			page = selected = 0;
		}
		else if (is_in(key, "1", "2", "3", "4", "5") or key.starts_with("select_cat_")) {
			selected_cat = key.back() - '0' - 1;
			page = selected = 0;
		}
		else if (is_in(key, "left", "right") or (vim_keys and is_in(key, "h", "l"))) {
			const auto& option = categories[selected_cat][item_height * page + selected];
			if (selPred.test(isInt)) {
				const int mod = (option.offset == CONFIG_OFFSET(update_ms) ? 100 : 1);
				long value = Config::dynamic_get<int>(config, option.offset);
				if (key == "right" or (vim_keys and key == "l")) value += mod;
				else value -= mod;

				if (Config::validate<int>(option.offset, value))
					Config::set(option.offset, static_cast<int>(value), true);
				else {
					warnings = Config::validError;
				}
			}
			else if (selPred.test(isBool)) {
				Config::set(option.offset, not Config::dynamic_get<bool>(config, option.offset), true);
				screen_redraw = true;
				switch(option.offset) {
					case CONFIG_OFFSET(truecolor):
						theme_refresh = true;
						CONFIG_SET(lowcolor, not config.lowcolor);
						break;
					case CONFIG_OFFSET(force_tty):
						theme_refresh = true;
						CONFIG_SET(tty_mode, not config.tty_mode);
						break;
					case CONFIG_OFFSET(rounded_corners):
					case CONFIG_OFFSET(theme_background):
						theme_refresh = true;
						break;
					case CONFIG_OFFSET(background_update):
						Runner::pause_output = false;
						break;
					case CONFIG_OFFSET(base_10_sizes):
						recollect = true;
						break;

				}
			}
			else if (selPred.test(isBrowseable)) {
				auto& optList = *option.browse_values;
				int i = v_index(optList, Config::dynamic_get<string>(config, option.offset));

				if ((key == "right" or (vim_keys and key == "l")) and ++i >= (int)optList.size()) i = 0;
				else if ((key == "left" or (vim_keys and key == "h")) and --i < 0) i = optList.size() - 1;
				Config::set(option.offset, optList.at(i), true);

				switch(option.offset) {
					case CONFIG_OFFSET(color_theme):
						theme_refresh = true;
						break;
					case CONFIG_OFFSET(log_level):
						Logger::set(optList.at(i));
						Logger::info("Logger set to " + optList.at(i));
						break;
					case CONFIG_OFFSET(proc_sorting):
					case CONFIG_OFFSET(cpu_sensor):
					case CONFIG_OFFSET(graph_symbol):
					case CONFIG_OFFSET(graph_symbol_proc):
					case CONFIG_OFFSET(graph_symbol_cpu):
					case CONFIG_OFFSET(graph_symbol_mem):
					case CONFIG_OFFSET(graph_symbol_net):
					case CONFIG_OFFSET(cpu_graph_lower):
					case CONFIG_OFFSET(cpu_graph_upper):
						screen_redraw = true;
						break;
				}
			}
			else
				retval = NoChange;
		}
		else {
			retval = NoChange;
		}

		//? Draw the menu
		if (retval == Changed) {
			Config::unlock();
			auto& out = Global::overlay;
			out = bg;
			item_height = min((int)categories[selected_cat].size(), (int)floor((double)(height - 4) / 2));
			pages = ceil((double)categories[selected_cat].size() / item_height);
			if (page > pages - 1) page = pages - 1;
			select_max = min(item_height - 1, (int)categories[selected_cat].size() - 1 - item_height * page);
			if (selected > select_max) {
				selected = select_max;
			}

			//? Get variable properties for currently selected option
			if (selPred.none() or last_sel != (selected_cat << 8) + selected) {
				selPred = categories[selected_cat][item_height * page + selected].info;
				last_sel = (selected_cat << 8) + selected;
			}

			//? Category buttons
			out += Mv::to(y+7, x+4);
			for (int i = 0; const auto& m : {"general", "cpu", "mem", "net", "proc"}) {
				out += Fx::b + (i == selected_cat
						? Theme::c("hi_fg") + '[' + Theme::c("title") + m + Theme::c("hi_fg") + ']'
						: Theme::c("hi_fg") + to_string(i + 1) + Theme::c("title") + m + ' ')
					+ Mv::r(10);
				if (string button_name = "select_cat_" + to_string(i + 1); not editing and not mouse_mappings.contains(button_name))
					mouse_mappings[button_name] = {y+6, x+2 + 15*i, 3, 15};
				i++;
			}
			if (pages > 1) {
				out += Mv::to(y+6 + height - 1, x+2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			//? Option name and value
			auto cy = y+9;
			for (int c = 0, i = max(0, item_height * page); c++ < item_height and i < (int)categories[selected_cat].size(); i++) {
				const auto& option = categories[selected_cat][i];
				const auto& value = (option.offset == CONFIG_OFFSET(color_theme) ? (string) fs::path(config.color_theme).stem() : Config::getAsString(option.offset));

				out += Mv::to(cy++, x + 1) + (c-1 == selected ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("title"))
					+ Fx::b + cjust(capitalize(s_replace(option.content[0], "_", " "))
						+ (c-1 == selected and selPred.test(isBrowseable)
							? ' ' + to_string(v_index(*option.browse_values, (option.offset == CONFIG_OFFSET(color_theme) ? config.color_theme : value)) + 1) + '/' + to_string(option.browse_values->size())
							: ""), 29);
				out	+= Mv::to(cy++, x + 1) + (c-1 == selected ? "" : Theme::c("main_fg")) + Fx::ub + "  "
					+ (c-1 == selected and editing ? cjust(editor(24), 34, true) : cjust(value, 25, true)) + "  ";

				if (c-1 == selected) {
					if (not editing and (selPred.test(is2D) or selPred.test(isBrowseable))) {
						out += Fx::b + Mv::to(cy-1, x+2) + Symbols::left + Mv::to(cy-1, x+28) + Symbols::right;
						mouse_mappings["left"] = {cy-2, x, 2, 5};
						mouse_mappings["right"] = {cy-2, x+25, 2, 5};
					}
					if (selPred.test(isEditable)) {
						out += Fx::b + Mv::to(cy-1, x+28 - (not editing and selPred.test(isInt) ? 2 : 0)) + (tty_mode ? "E" : Symbols::enter);
					}
					//? Description of selected option
					out += Fx::reset + Theme::c("title") + Fx::b;
					for (int cyy = y+7; const auto& desc: categories[selected_cat][i].content) {
						if (cyy++ == y+7) continue;
						else if (cyy == y+10) out += Theme::c("main_fg") + Fx::ub;
						else if (cyy > y + height + 4) break;
						out += Mv::to(cyy, x+32);
						out += desc;
					}
				}
			}

			if (not warnings.empty()) {
				messageBox = msgBox{min(78, (int)ulen(warnings) + 10), msgBox::BoxTypes::OK, {uresize(warnings, 74)}, "warning"};
				out += messageBox();
			}

			out += Fx::reset;
		}

		if (theme_refresh) {
			Theme::setTheme();
			Draw::banner_gen(0, 0, false, true);
			screen_redraw = true;
			redraw = true;
			optionsMenu("");
		}
		if (screen_redraw) {
			auto overlay_bkp = std::move(Global::overlay);
			auto clock_bkp = std::move(Global::clock);
			Draw::calcSizes();
			Global::overlay = std::move(overlay_bkp);
			Global::clock = std::move(clock_bkp);
			recollect = true;
		}
		if (recollect) {
			Runner::run("all", false, true);
			retval = NoChange;
		}

		return (redraw ? Changed : retval);
	}

	int helpMenu(const string& key) {
		static int y{};         // defaults to 0
		static int x{};         // defaults to 0
		static int height{};    // defaults to 0
		static int page{};      // defaults to 0
		static int pages{};     // defaults to 0

		if (bg.empty()) page = 0;
		int retval = Changed;

		if (redraw) {

			y = max(1, Term::height/2 - 4 - (int)(ASIZE(help_text) / 2));
			x = Term::width/2 - 39;
			height = min(Term::height - 6, (int)ASIZE(help_text) + 3);
			pages = ceil((double)ASIZE(help_text) / (height - 3));
			page = 0;
			bg = Draw::banner_gen(y, 0, true);
			bg += Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, "help");
		}
		else if (is_in(key, "escape", "q", "h", "backspace", "space", "enter", "mouse_click")) {
			return Closed;
		}
		else if (pages > 1 and is_in(key, "down", "page_down", "tab", "mouse_scroll_down")) {
			if (++page >= pages) page = 0;
		}
		else if (pages > 1 and is_in(key, "up", "page_up", "shift_tab", "mouse_scroll_up")) {
			if (--page < 0) page = pages - 1;
		}
		else {
			retval = NoChange;
		}


		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg;
			if (pages > 1) {
				out += Mv::to(y+height+6, x + 2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			auto cy = y+7;
			out += Mv::to(cy++, x + 1) + Theme::c("title") + Fx::b + cjust("Key:", 20) + "Description:";
			for (int c = 0, i = max(0, (height - 3) * page); c++ < height - 3 and i < (int)ASIZE(help_text); i++) {
				out += Mv::to(cy++, x + 1) + Theme::c("hi_fg") + Fx::b + cjust(help_text[i][0], 20)
					+ Theme::c("main_fg") + Fx::ub + help_text[i][1];
			}
			out += Fx::reset;
		}


		return (redraw ? Changed : retval);
	}

	//* Add menus here and update enum Menus in header
	const auto menuFunc = vector{
		ref(sizeError),
		ref(signalChoose),
		ref(signalSend),
		ref(signalReturn),
		ref(optionsMenu),
		ref(helpMenu),
		ref(mainMenu),
	};
	bitset<8> menuMask;

	void process(string key) {
		if (menuMask.none()) {
			Menu::active = false;
			Global::overlay.clear();
			Global::overlay.shrink_to_fit();
			Runner::pause_output = false;
			bg.clear();
			bg.shrink_to_fit();
			currentMenu = -1;
			Runner::run("all", true, true);
			mouse_mappings.clear();
			return;
		}

		if (currentMenu < 0 or not menuMask.test(currentMenu)) {
			Menu::active = true;
			redraw = true;
			if (((menuMask.test(Main) or menuMask.test(Options) or menuMask.test(Help) or menuMask.test(SignalChoose))
			and (Term::width < 80 or Term::height < 24))
			or (Term::width < 50 or Term::height < 20)) {
				menuMask.reset();
				menuMask.set(SizeError);
			}

			for (const auto& i : iota(0, (int)menuMask.size())) {
				if (menuMask.test(i)) currentMenu = i;
			}

		}

		auto retCode = menuFunc.at(currentMenu)(key);
		if (retCode == Closed) {
			menuMask.reset(currentMenu);
			mouse_mappings.clear();
			bg.clear();
			Runner::pause_output = false;
			process();
		}
		else if (redraw) {
			redraw = false;
			Runner::run("all", true, true);
		}
		else if (retCode == Changed)
			Runner::run("overlay");
		else if (retCode == Switch) {
			Runner::pause_output = false;
			bg.clear();
			redraw = true;
			mouse_mappings.clear();
			process();
		}
	}

	void show(int menu, int signal) {
		menuMask.set(menu);
		signalToSend = signal;
		process();
	}
}
