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
#include <stdexcept>
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
	unordered_flat_map<uint32_t, ConfigType> cached{};
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

	bool to_int(const string& value, int& result) {
		try {
			result = stoi(value);
		} catch (const std::invalid_argument&) {
			validError = "Invalid numerical value!";
			return false;
		} catch (const std::out_of_range&) {
			validError = "Value out of range!";
			return  false;
		} catch (const std::exception& e) {
			validError = string{e.what()};
			return false;
		}

		return true;
	}

	template<typename T>
	inline void copy_field(ConfigSet& dest, const ConfigSet& src, size_t offset) {
		dynamic_set<T>(dest, offset, dynamic_get<T>(src, offset));
	}

	string getAsString(const size_t offset) {
		switch(offset) {
#define X(T, name, v, desc) \
			case CONFIG_OFFSET(name): \
				return details::to_string<T>(current, offset);
			OPTIONS_LIST
#undef X
		}

		return "";
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
			switch(item.second) {
				case ConfigType::STRING:
					copy_field<string>(current, cache, item.first);
					break;
				case ConfigType::INT:
					copy_field<int>(current, cache, item.first);
					break;
				case ConfigType::BOOL:
					copy_field<bool>(current, cache, item.first);
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

				int intValue;
				switch(found->second.type) {
					case ConfigType::BOOL:
						cread >> value;
						try {
							dynamic_set(set, found->second.offset, stobool(value));
						} catch(std::invalid_argument&) {
							load_warnings.push_back("Got an invalid bool value for config name: " + name);
						}
						break;
					case ConfigType::INT:
						cread >> value;
						if(to_int(value, intValue) and validate(found->second.offset, intValue)) {
							dynamic_set(set, found->second.offset, intValue);
						} else {
							load_warnings.push_back(validError);
						}
						break;
					case ConfigType::STRING:
						if (cread.peek() == '"') {
							cread.ignore(1);
							getline(cread, value, '"');
						}
						else cread >> value;

						if (validate(found->second.offset, value)) {
							dynamic_set(set, found->second.offset, value);
						} else {
							load_warnings.push_back(validError);
						}
						break;
				}

				cread.ignore(SSmax, '\n');
			}

			if (not load_warnings.empty()) write_new = true;

			current = set;
		}
	}

	template<typename T>
	inline void write_field(std::ofstream& stream, const std::string_view name, const T& value, const std::string_view description) {
		stream << "\n\n";
		if(not description.empty()) {
			stream << description << '\n';
		}

		stream << name << " = ";

		if constexpr(std::is_same_v<T, bool>) {
			stream << (value ? "True" : "False");
		}

		if constexpr(std::is_same_v<T, int>) {
			stream << value;
		}

		if constexpr(std::is_same_v<T, string>) {
			stream << '"' << value << '"';
		}
	}

	void write() {
		if (conf_file.empty() or not write_new) return;
		Logger::debug("Writing new config file");
		if (geteuid() != Global::real_uid and seteuid(Global::real_uid) != 0) return;
		std::ofstream cwrite(conf_file, std::ios::trunc);

		if (cwrite.good()) {
			cwrite << "#? Config file for btop v. " << Global::Version;
#define X(T, name, v, desc) write_field(cwrite, #name, current.name, desc);
			OPTIONS_WRITABLE_LIST
#undef X
		}
	}
}
