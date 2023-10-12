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

#include <array>
#include <atomic>
#include <exception>
#include <fstream>
#include <ranges>
#include <string_view>

#include <fmt/core.h>

#include "btop_config.hpp"
#include "btop_shared.hpp"
#include "btop_tools.hpp"

using std::array;
using std::atomic;
using std::string_view;

namespace fs = std::filesystem;
namespace rng = std::ranges;

using namespace std::literals;
using namespace Tools;

//* Functions and variables for reading and writing the btop config file
namespace Config {
	atomic<bool> locked (false);
	atomic<bool> writelock (false);
	bool write_new;
	robin_hood::unordered_flat_set<std::string_view> cached{};
	ConfigSet current = ConfigSet::with_defaults();
	ConfigSet cache{};

	const ConfigSet& get() {
		return current;
	}

	ConfigSet& get_mut(bool locked, bool write) {
		if(write)
			write_new = true;
		return locked ? cache : current;
	}

	fs::path conf_dir;
	fs::path conf_file;

	vector<string> available_batteries = {"Auto"};
	vector<string> current_boxes;
	vector<string> preset_list = {"cpu:0:default,mem:0:default,net:0:default,proc:0:default"};
	int current_preset = -1;

	bool presetsValid(const string& presets) {
		vector<string> new_presets = {preset_list.at(0)};

		for (int x = 0; const auto& preset : ssplit(presets)) {
			if (++x > 9) {
				validError = "Too many presets entered!";
				return false;
			}
			for (int y = 0; const auto& box : ssplit(preset, ',')) {
				if (++y > 4) {
					validError = "Too many boxes entered for preset!";
					return false;
				}
				const auto& vals = ssplit(box, ':');
				if (vals.size() != 3) {
					validError = "Malformatted preset in config value presets!";
					return false;
				}
				if (not is_in(vals.at(0), "cpu", "mem", "net", "proc")) {
					validError = "Invalid box name in config value presets!";
					return false;
				}
				if (not is_in(vals.at(1), "0", "1")) {
					validError = "Invalid position value in config value presets!";
					return false;
				}
				if (not v_contains(valid_graph_symbols_def, vals.at(2))) {
					validError = "Invalid graph name in config value presets!";
					return false;
				}
			}
			new_presets.push_back(preset);
		}

		preset_list = std::move(new_presets);
		return true;
	}

	//* Apply selected preset
	void apply_preset(const string& preset) {
		string boxes;

		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			boxes += vals.at(0) + ' ';
		}
		if (not boxes.empty()) boxes.pop_back();

		auto min_size = Term::get_min_size(boxes);
		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			return;
		}

		for (const auto& box : ssplit(preset, ',')) {
			const auto& vals = ssplit(box, ':');
			if (vals.at(0) == "cpu") {
				CONFIG_SET(cpu_bottom, vals.at(1) != "0");
				CONFIG_SET(graph_symbol_cpu, vals.at(2));
			} else if (vals.at(0) == "mem") {
				CONFIG_SET(mem_below_net, vals.at(1) != "0");
				CONFIG_SET(graph_symbol_mem, vals.at(2));
			} else if (vals.at(0) == "proc") {
				CONFIG_SET(proc_left, vals.at(1) != "0");
				CONFIG_SET(graph_symbol_proc, vals.at(2));
			} else if(vals.at(0) == "net") {
				CONFIG_SET(graph_symbol_net, vals.at(2));
			}
		}

		if (check_boxes(boxes)) CONFIG_SET(shown_boxes, boxes);
	}

	bool _locked() {
		atomic_wait(writelock);
		return locked.load();
	}

	void lock() {
		atomic_wait(writelock);
		locked = true;
	}

	string validError;

	bool intValid(const std::string_view name, const string& value) {
		int i_value;
		try {
			i_value = stoi(value);
		}
		catch (const std::invalid_argument&) {
			validError = "Invalid numerical value!";
			return false;
		}
		catch (const std::out_of_range&) {
			validError = "Value out of range!";
			return false;
		}
		catch (const std::exception& e) {
			validError = string{e.what()};
			return false;
		}

		if (name == "update_ms" and i_value < 100)
			validError = "Config value update_ms set too low (<100).";

		else if (name == "update_ms" and i_value > 86400000)
			validError = "Config value update_ms set too high (>86400000).";

		else
			return true;

		return false;
	}

	bool stringValid(const std::string_view name, const string& value) {
		if (name == "log_level" and not v_contains(Logger::log_levels, value))
			validError = "Invalid log_level: " + value;

		else if (name == "graph_symbol" and not v_contains(valid_graph_symbols, value))
			validError = "Invalid graph symbol identifier: " + value;

		else if (name.starts_with("graph_symbol_") and (value != "default" and not v_contains(valid_graph_symbols, value)))
			validError = fmt::format("Invalid graph symbol identifier for {}: {}", name, value);

		else if (name == "shown_boxes" and not value.empty() and not check_boxes(value))
			validError = "Invalid box name(s) in shown_boxes!";

		else if (name == "presets" and not presetsValid(value))
			return false;

		else if (name == "cpu_core_map") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (not isint(map_split.at(0)) or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of cpu_core_map!";
					return false;
				}
			}
			return true;
		}
		else if (name == "io_graph_speeds") {
			const auto maps = ssplit(value);
			bool all_good = true;
			for (const auto& map : maps) {
				const auto map_split = ssplit(map, ':');
				if (map_split.size() != 2)
					all_good = false;
				else if (map_split.at(0).empty() or not isint(map_split.at(1)))
					all_good = false;

				if (not all_good) {
					validError = "Invalid formatting of io_graph_speeds!";
					return false;
				}
			}
			return true;
		}

		else
			return true;

		return false;
	}

	template<typename T>
	inline void copy_field(ConfigSet& dest, const ConfigSet& src, size_t offset) {
		dynamic_set<T>(dest, offset, dynamic_get<T>(src, offset));
	}

	string getAsString(const std::string_view name) {
		auto& entry = parse_table.at(name);
		switch(entry.type) {
			case ConfigType::BOOL:
				return dynamic_get<bool>(current, entry.offset) ? "True" : "False";
			case ConfigType::INT:
				return to_string(dynamic_get<int>(current, entry.offset));
			case ConfigType::STRING:
				return dynamic_get<string>(current, entry.offset);
			default:
				throw std::logic_error("Not implemented");
		}
	}

	void unlock() {
		if (not locked) return;
		atomic_wait(Runner::active);
		atomic_lock lck(writelock, true);

		if(Proc::shown) {
			current.selected_pid = Proc::selected_pid;
			current.selected_name = Proc::selected_name;
			current.proc_start = Proc::start;
			current.proc_selected = Proc::selected;
			current.selected_depth = Proc::selected_depth;
		}

		for(auto& item: cached) {
			auto& data = parse_table.at(item);

			switch(data.type) {
				case ConfigType::STRING:
					copy_field<string>(current, cache, data.offset);
					break;
				case ConfigType::INT:
					copy_field<int>(current, cache, data.offset);
					break;
				case ConfigType::BOOL:
					copy_field<bool>(current, cache, data.offset);
					break;
			}
		}
		cached.clear();

		locked = false;
	}

	bool check_boxes(const string& boxes) {
		auto new_boxes = ssplit(boxes);
		for (auto& box : new_boxes) {
			if (not v_contains(valid_boxes, box)) return false;
		}
		current_boxes = std::move(new_boxes);
		return true;
	}

	void toggle_box(const string& box) {
		auto old_boxes = current_boxes;
		auto box_pos = rng::find(current_boxes, box);
		if (box_pos == current_boxes.end())
			current_boxes.push_back(box);
		else
			current_boxes.erase(box_pos);

		string new_boxes;
		if (not current_boxes.empty()) {
			for (const auto& b : current_boxes) new_boxes += b + ' ';
			new_boxes.pop_back();
		}

		auto min_size = Term::get_min_size(new_boxes);

		if (Term::width < min_size.at(0) or Term::height < min_size.at(1)) {
			current_boxes = old_boxes;
			return;
		}

		CONFIG_SET(shown_boxes, new_boxes);
	}

	void load(const fs::path& conf_file, vector<string>& load_warnings) {
		ConfigSet set = ConfigSet::with_defaults();
		if (conf_file.empty())
			return;
		else if (not fs::exists(conf_file)) {
			write_new = true;
			return;
		}
		std::ifstream cread(conf_file);
		if (cread.good()) {
			if (string v_string; cread.peek() != '#' or (getline(cread, v_string, '\n') and not s_contains(v_string, Global::Version)))
				write_new = true;
			while (not cread.eof()) {
				cread >> std::ws;
				if (cread.peek() == '#') {
					cread.ignore(SSmax, '\n');
					continue;
				}
				string name, value;
				getline(cread, name, '=');
				if (name.ends_with(' ')) name = trim(name);
				auto found = parse_table.find(name);
				if (found == parse_table.end()) {
					cread.ignore(SSmax, '\n');
					continue;
				}
				cread >> std::ws;

				switch(found->second.type) {
					case ConfigType::BOOL:
						cread >> value;
						if (not isbool(value))
							load_warnings.push_back("Got an invalid bool value for config name: " + name);
						else
							dynamic_set(set, found->second.offset, stobool(value));
						break;
					case ConfigType::INT:
						cread >> value;
						if (not isint(value))
							load_warnings.push_back("Got an invalid integer value for config name: " + name);
						else if (not intValid(name, value)) {
							load_warnings.push_back(validError);
						}
						else
							dynamic_set(set, found->second.offset, stoi(value));
						break;
					case ConfigType::STRING:
						if (cread.peek() == '"') {
							cread.ignore(1);
							getline(cread, value, '"');
						}
						else cread >> value;

						if (not stringValid(name, value))
							load_warnings.push_back(validError);
						else
							dynamic_set(set, found->second.offset, value);
						break;
				}

				cread.ignore(SSmax, '\n');
			}

			if (not load_warnings.empty()) write_new = true;

			current = set;
		}
	}

	void write() {
		if (conf_file.empty() or not write_new) return;
		Logger::debug("Writing new config file");
		if (geteuid() != Global::real_uid and seteuid(Global::real_uid) != 0) return;
		std::ofstream cwrite(conf_file, std::ios::trunc);
		auto write_field = [&](const std::string_view name, const std::string_view description) {
			cwrite << "\n\n";
			if(not description.empty()) {
				cwrite << description << "\n";
			}
			cwrite << name << " = ";
			auto& data = parse_table.at(name);
			switch(data.type) {
				case ConfigType::BOOL:
					cwrite << (dynamic_get<bool>(current, data.offset) ? "True" : "False");
					break;
				case ConfigType::INT:
					cwrite << dynamic_get<int>(current, data.offset);
					break;
				case ConfigType::STRING:
					cwrite << '"' << dynamic_get<string>(current, data.offset) << '"';
					break;
			}
		};

		if (cwrite.good()) {
			cwrite << "#? Config file for btop v. " << Global::Version;
#define X(T, name, v, desc) write_field(#name, desc);
			OPTIONS_WRITABLE_LIST
#undef X
		}
	}
}
