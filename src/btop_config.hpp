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

#include "btop_tools.hpp"
#include <string>
#include <vector>
#include <filesystem>

#include <robin_hood.h>

using std::string;
using std::vector;
using robin_hood::unordered_flat_map;

#define OPTIONS_WRITABLE_LIST \
	X(string, color_theme, "Default", \
			"#* Name of a btop++/bpytop/bashtop formatted \".theme\" file, \"Default\" and \"TTY\" for builtin themes.\n" \
			"#* Themes should be placed in \"../share/btop/themes\" relative to binary or \"$HOME/.config/btop/themes\"") \
	X(bool, theme_background, true, \
			"#* If the theme set background should be shown, set to False if you want terminal background transparency.") \
	X(bool, truecolor, true, \
			"#* Sets if 24-bit truecolor should be used, will convert 24-bit colors to 256 color (6x6x6 color cube) if false.") \
	X(bool, force_tty, false, \
			"#* Set to true to force tty mode regardless if a real tty has been detected or not.\n" \
			"#* Will force 16-color mode and TTY theme, set all graph symbols to \"tty\" and swap out other non tty friendly symbols.") \
	X(string, presets, "cpu:1:default,proc:0:default cpu:0:default,mem:0:default,net:0:default cpu:0:block,net:0:tty", \
			"#* Define presets for the layout of the boxes. Preset 0 is always all boxes shown with default settings. Max 9 presets.\n" \
			"#* Format: \"box_name:P:G,box_name:P:G\" P=(0 or 1) for alternate positions, G=graph symbol to use for box.\n" \
			"#* Use whitespace \" \" as separator between different presets.\n" \
			"#* Example: \"cpu:0:default,mem:0:tty,proc:1:default cpu:0:braille,proc:0:tty\"") \
	X(bool, vim_keys, false, \
			"#* Set to True to enable \"h,j,k,l,g,G\" keys for directional control in lists.\n" \
			"#* Conflicting keys for h:\"help\" and k:\"kill\" is accessible while holding shift.") \
	X(bool, rounded_corners, true, \
			"#* Rounded corners on boxes, is ignored if TTY mode is ON.") \
	X(string, graph_symbol, "braille", \
			"#* Default symbols to use for graph creation, \"braille\", \"block\" or \"tty\".\n" \
			"#* \"braille\" offers the highest resolution but might not be included in all fonts.\n" \
			"#* \"block\" has half the resolution of braille but uses more common characters.\n" \
			"#* \"tty\" uses only 3 different symbols but will work with most fonts and should work in a real TTY.\n" \
			"#* Note that \"tty\" only has half the horizontal resolution of the other two, so will show a shorter historical view.") \
	X(string, graph_symbol_cpu, "default", \
			"# Graph symbol to use for graphs in cpu box, \"default\", \"braille\", \"block\" or \"tty\".") \
	X(string, graph_symbol_mem, "default", \
			"# Graph symbol to use for graphs in mem box, \"default\", \"braille\", \"block\" or \"tty\".") \
	X(string, graph_symbol_net, "default", \
			"# Graph symbol to use for graphs in net box, \"default\", \"braille\", \"block\" or \"tty\".") \
	X(string, graph_symbol_proc, "default", \
			"# Graph symbol to use for graphs in proc box, \"default\", \"braille\", \"block\" or \"tty\".") \
	X(string, shown_boxes, "cpu mem net proc", \
			"#* Manually set which boxes to show. Available values are \"cpu mem net proc\", separate values with whitespace.") \
	X(int, update_ms, 2000, \
			"#* Update time in milliseconds, recommended 2000 ms or above for better sample times for graphs.") \
	X(string, proc_sorting, "cpu lazy", \
			"#* Processes sorting, \"pid\" \"program\" \"arguments\" \"threads\" \"user\" \"memory\" \"cpu lazy\" \"cpu direct\",\n" \
			"#* \"cpu lazy\" sorts top process over time (easier to follow), \"cpu direct\" updates top process directly.") \
	X(bool, proc_reversed, false, \
			"#* Reverse sorting order, True or False.") \
	X(bool, proc_tree, false, \
			"#* Show processes as a tree.") \
	X(bool, proc_colors, true, \
			"#* Use the cpu graph colors in the process list.") \
	X(bool, proc_gradient, true, \
			"#* Use a darkening gradient in the process list.") \
	X(bool, proc_per_core, false, \
			"#* If process cpu usage should be of the core it's running on or usage of the total available cpu power.") \
	X(bool, proc_mem_bytes, true, \
			"#* Show process memory as bytes instead of percent.") \
	X(bool, proc_cpu_graphs, true, \
			"#* Show cpu graph for each process.") \
	X(bool, proc_cpu_smaps, false, \
			"#* Use /proc/[pid]/smaps for memory information in the process info box (very slow but more accurate)") \
	X(bool, proc_left, false, \
			"#* Show proc box on left side of screen instead of right.") \
	X(bool, proc_filter_kernel, false, \
			"#* (Linux) Filter processes tied to the Linux kernel(similar behavior to htop).") \
	X(bool, proc_aggregate, false, \
			"#* In tree-view, always accumulate child process resources in the parent process.") \
	X(string, cpu_graph_upper, "total", \
			"#* Sets the CPU stat shown in upper half of the CPU graph, \"total\" is always available.\n" \
			"#* Select from a list of detected attributes from the options menu.") \
	X(string, cpu_graph_lower, "total", \
			"#* Sets the CPU stat shown in lower half of the CPU graph, \"total\" is always available.\n" \
			"#* Select from a list of detected attributes from the options menu.") \
	X(bool, cpu_invert_lower, true, \
			"#* Toggles if the lower CPU graph should be inverted.") \
	X(bool, cpu_single_graph, false, \
			"#* Set to True to completely disable the lower CPU graph.") \
	X(bool, cpu_bottom, false, \
			"#* Show cpu box at bottom of screen instead of top.") \
	X(bool, show_uptime, true, \
			"#* Shows the system uptime in the CPU box.") \
	X(bool, check_temp, true, \
			"#* Shows the system uptime in the CPU box.") \
	X(string, cpu_sensor, "Auto", \
			"#* Which sensor to use for cpu temperature, use options menu to select from list of available sensors.") \
	X(bool, show_coretemp, true, \
			"#* Show temperatures for cpu cores also if check_temp is True and sensors has been found.") \
	X(string, cpu_core_map, "", \
			"#* Set a custom mapping between core and coretemp, can be needed on certain cpus to get correct temperature for correct core.\n" \
			"#* Use lm-sensors or similar to see which cores are reporting temperatures on your machine.\n" \
			"#* Format \"x:y\" x=core with wrong temp, y=core with correct temp, use space as separator between multiple entries.\n" \
			"#* Example: \"4:0 5:1 6:3\"") \
	X(string, temp_scale, "celsius", \
			"#* Which temperature scale to use, available values: \"celsius\", \"fahrenheit\", \"kelvin\" and \"rankine\".") \
	X(bool, base_10_sizes, false, \
			"#* Use base 10 for bits/bytes sizes, KB = 1000 instead of KiB = 1024.") \
	X(bool, show_cpu_freq, true, \
			"#* Show CPU frequency.") \
	X(string, clock_format, "%X", \
			"#* Draw a clock at top of screen, formatting according to strftime, empty string to disable.\n" \
			"#* Special formatting: /host = hostname | /user = username | /uptime = system uptime") \
	X(bool, background_update, true, \
			"#* Update main ui in background when menus are showing, set this to false if the menus is flickering too much for comfort.") \
	X(string, custom_cpu_name, "", \
			"#* Custom cpu model name, empty string to disable.") \
	X(string, disks_filter, "", \
			"#* Optional filter for shown disks, should be full path of a mountpoint, separate multiple values with whitespace \" \".\n" \
			"#* Begin line with \"exclude=\" to change to exclude filter, otherwise defaults to \"most include\" filter. Example: disks_filter=\"exclude=/boot /home/user\".") \
	X(bool, mem_graphs, true, \
			"#* Show graphs instead of meters for memory values.") \
	X(bool, mem_below_net, false, \
			"#* Show mem box below net box instead of above.") \
	X(bool, zfs_arc_cached, true, \
			"#* Count ZFS ARC in cached and available memory.") \
	X(bool, show_swap, true, \
			"#* If swap memory should be shown in memory box.") \
	X(bool, swap_disk, true, \
			"#* Show swap as a disk, ignores show_swap value above, inserts itself after first disk.") \
	X(bool, show_disks, true, \
			"#* If mem box should be split to also show disks info.") \
	X(bool, only_physical, true, \
			"#* Filter out non physical disks. Set this to False to include network disks, RAM disks and similar.") \
	X(bool, use_fstab, true, \
			"#* Read disks list from /etc/fstab. This also disables only_physical.") \
	X(bool, zfs_hide_datasets, false, \
			"#* Setting this to True will hide all datasets, and only show ZFS pools. (IO stats will be calculated per-pool)") \
	X(bool, disk_free_priv, false, \
			"#* Set to true to show available disk space for privileged users.") \
	X(bool, show_io_stat, true, \
			"#* Toggles if io activity % (disk busy time) should be shown in regular disk usage view.") \
	X(bool, io_mode, false, \
			"#* Toggles io mode for disks, showing big graphs for disk read/write speeds.") \
	X(bool, io_graph_combined, false, \
			"#* Set to True to show combined read/write io graphs in io mode.") \
	X(string, io_graph_speeds, "", \
			"#* Set the top speed for the io graphs in MiB/s (100 by default), use format \"mountpoint:speed\" separate disks with whitespace \" \".\n" \
			"#* Example: \"/mnt/media:100 /:20 /boot:1\".") \
	X(int, net_download, 100, \
			"#* Set fixed values for network graphs in Mebibits. Is only used if net_auto is also set to False.") \
	X(int, net_upload, 100, "") \
	X(bool, net_auto, true, \
			"#* Use network graphs auto rescaling mode, ignores any values set above and rescales down to 10 Kibibytes at the lowest.") \
	X(bool, net_sync, true, \
			"#* Sync the auto scaling for download and upload to whichever currently has the highest scale.") \
	X(string, net_iface, "", \
			"#* Starts with the Network Interface specified here.") \
	X(bool, show_battery, true, \
			"#* Show battery stats in top right if battery is present.") \
	X(string, selected_battery, "Auto", \
			"#* Which battery to use if multiple are present. \"Auto\" for auto detection.") \
	X(string, log_level, "WARNING", \
			"#* Set loglevel for \"~/.config/btop/btop.log\" levels are: \"ERROR\" \"WARNING\" \"INFO\" \"DEBUG\".\n" \
			"#* The level set includes all lower levels, i.e. \"DEBUG\" will show all logging info.") \

#define OPTIONS_INTERNAL_LIST \
	X(int, selected_pid, 0, "") \
	X(string, selected_name, "", "") \
	X(int, proc_start, 0, "") \
	X(int, proc_selected, 0, "") \
	X(int, proc_last_selected, 0, "") \
	X(int, selected_depth, 0, "") \
	X(bool, lowcolor, false, "") \
	X(bool, tty_mode, false, "") \
	X(bool, show_detailed, false, "") \
	X(int, detailed_pid, 0, "") \
	X(bool, proc_filtering, false, "") \
	X(string, proc_filter, "", "") \


#define OPTIONS_LIST OPTIONS_WRITABLE_LIST OPTIONS_INTERNAL_LIST

#define CONFIG_OFFSET(name) offsetof(Config::ConfigSet, name)
#define CONFIG_SET(name, v) Config::set< \
	decltype(std::declval<Config::ConfigSet>().name) \
>(CONFIG_OFFSET(name), v, Config::is_writable(#name))

//* Functions and variables for reading and writing the btop config file
namespace Config {
	enum class ConfigType {
		BOOL,
		INT,
		STRING
	};

	namespace details {
		template<typename T> struct TypeToEnum;
		template<>
		struct TypeToEnum<bool> {
			static constexpr ConfigType value = ConfigType::BOOL;
		};

		template<>
		struct TypeToEnum<int> {
			static constexpr ConfigType value = ConfigType::INT;
		};

		template<>
		struct TypeToEnum<std::string> {
			static constexpr ConfigType value = ConfigType::STRING;
		};
	}

	struct ConfigSet {
#define X(T, name, v, desc) T name;
		OPTIONS_LIST
#undef X

		static ConfigSet with_defaults() {
			return {
#define X(T, name, v, desc) .name = v,
				OPTIONS_LIST
#undef X
			};
		}
	};

	struct ParseData {
		// Offset will be 32bit to save space/make this fit in a register
		uint32_t offset;
		ConfigType type;
	};

	template<typename T>
	constexpr inline ConfigType type_enum() {
		return details::TypeToEnum<T>::value;
	}

	const unordered_flat_map<std::string_view, ParseData> parse_table = {
#define X(T, name, v, desc) { #name, { \
		.offset = CONFIG_OFFSET(name), \
		.type = Config::type_enum<decltype(std::declval<ConfigSet>().name)>(), \
	} },
		OPTIONS_LIST
#undef X
	};

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	const vector<string> valid_graph_symbols = { "braille", "block", "tty" };
	const vector<string> valid_graph_symbols_def = { "default", "braille", "block", "tty" };
	const vector<string> valid_boxes = { "cpu", "mem", "net", "proc" };
	const vector<string> temp_scales = { "celsius", "fahrenheit", "kelvin", "rankine" };

	extern vector<string> current_boxes;
	extern vector<string> preset_list;
	extern vector<string> available_batteries;
	extern int current_preset;
	extern unordered_flat_map<uint32_t, ConfigType> cached;

	constexpr static bool is_writable(const std::string_view option) {
#define X(T, name, v, desc) if(option == #name) return false;
		OPTIONS_INTERNAL_LIST
#undef X
		return true;
	}


	//* Get the current config object
	const ConfigSet& get();

	//* Get the current config object for writing
	ConfigSet& get_mut(bool locked, bool write);

	//* Check if string only contains space separated valid names for boxes
	bool check_boxes(const string& boxes);

	//* Toggle box and update config string shown_boxes
	void toggle_box(const string& box);

	//* Parse and setup config value presets
	bool presetsValid(const string& presets);

	//* Apply selected preset
	void apply_preset(const string& preset);

	bool _locked();

	string getAsString(const size_t offset);

	template<typename T>
	inline const T& dynamic_get(const ConfigSet& src, size_t offset) {
		return *(const T*)((char*)&src + offset);
	}

	template<typename T>
	inline void dynamic_set(ConfigSet& dest, size_t offset, const T& value) {
		*(T*)((char*)&dest + offset) = value;
	}

	template<typename T>
	void set(const size_t offset, const T& value, const bool writable) {
		bool locked = _locked();
		auto& set = get_mut(locked, writable);
		if(locked)
			cached[offset] =  type_enum<T>();
		dynamic_set(set, offset, value);
	}

	namespace details {
		template<typename T>
		inline string to_string(const ConfigSet& set, const size_t offset);

		template<>
		inline string to_string<bool>(const ConfigSet& set, const size_t offset) {
			return dynamic_get<bool>(set, offset) ? "True" : "False";
		}

		template<>
		inline string to_string<int>(const ConfigSet& set, const size_t offset) {
			return std::to_string(dynamic_get<int>(set, offset));
		}

		template<>
		inline string to_string<string>(const ConfigSet& set, const size_t offset) {
			return dynamic_get<string>(set, offset);
		}
	}

	extern string validError;

	//* Converts string to int. True on success, false on error.
	bool to_int(const string& value, int& result);

	template<typename T>
	bool validate(const size_t offset, const T& value, const bool setError = true) {
		using namespace Tools;
		bool isValid = true;

#define VALIDATE(name, cond, err) case CONFIG_OFFSET(name): \
		isValid = (cond); \
		if(setError and not isValid) { \
			validError = (err); \
		} \
		break

		if constexpr(std::is_same_v<T, int>) {
			switch(offset) {
				VALIDATE(update_ms, value >= 100 && value <= 86400000,
						"Config value update_ms out of range (100 <= x <= 86400000)");
			}
		}

		if constexpr(std::is_same_v<T, string>) {
			switch(offset) {
				VALIDATE(log_level, Tools::v_contains(Logger::log_levels, value),
						"Invalid log level: " + value);
				VALIDATE(graph_symbol, Tools::v_contains(valid_graph_symbols, value),
						"Invalid graph symbol identifier: " + value);
				VALIDATE(graph_symbol_proc, value == "default" || v_contains(valid_graph_symbols, value),
						"Invalid graph symbol identifier for graph_symbol_proc: " + value);
				VALIDATE(graph_symbol_cpu, value == "default" || v_contains(valid_graph_symbols, value),
						"Invalid graph symbol identifier for graph_symbol_cpu: " + value);
				VALIDATE(graph_symbol_mem, value == "default" || v_contains(valid_graph_symbols, value),
						"Invalid graph symbol identifier for graph_symbol_mem: " + value);
				VALIDATE(graph_symbol_net, value == "default" || v_contains(valid_graph_symbols, value),
						"Invalid graph symbol identifier for graph_symbol_net: " + value);
				VALIDATE(shown_boxes, value.empty() || check_boxes(value),
						"Invalid box_name(s) in shown_boxes");
				VALIDATE(cpu_core_map, [&]{
					const auto maps = ssplit(value);
					for (const auto& map : maps) {
						const auto map_split = ssplit(map, ':');
						if (map_split.size() != 2 || not isint(map_split.at(0)) or not isint(map_split.at(1)))
							return false;
					}
					return true;
				}(), "Invalid formatting of cpu_core_map!");
				VALIDATE(io_graph_speeds, [&]{
					const auto maps = ssplit(value);
					for (const auto& map : maps) {
						const auto map_split = ssplit(map, ':');
						if (map_split.size() != 2 || map_split.at(0).empty() or not isint(map_split.at(1)))
							return false;
					}
					return true;
				}(), "Invalid formatting of io_graph_speeds!");

				case CONFIG_OFFSET(presets):
					return presetsValid(value);
			}
		}

#undef VALIDATE

		return isValid;

	}

	//* Lock config and cache changes until unlocked
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(const std::filesystem::path& conf_file, vector<string>& load_warnings);

	//* Write the config file to disk
	void write();
}
