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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <algorithm>
#include <unordered_map>

using std::string;
using std::vector;

using CfgString = std::string;
using CfgBool = bool;
using CfgInt = int;

class CfgError : public std::runtime_error {
public:
   explicit CfgError(const std::string& err) : std::runtime_error(err) {}
};

template <typename T>
class CfgResult {
private:
   std::variant<T, std::string> result;
public:
   explicit CfgResult(const T& val) : result(val) {}
   explicit CfgResult(const std::string& err) : result(err) {}

   bool is_error() const { 
      return std::holds_alternative<std::string>(result);
   }
   const T& value() const {
      return std::get<T>(result);
   }
   const std::string& error() const {
      return std::get<std::string>(result);
   }
};

template <>
class CfgResult<std::string> {
private:
   enum class Tag { Value, Error };
   std::variant<std::string, Tag> result;

public:
   explicit CfgResult(const std::string& val) : result(val) {}
   explicit CfgResult(const char* /*err*/) : result(Tag::Error) {}

   bool is_error() const {
      return std::holds_alternative<Tag>(result);
   }

   const std::string& value() const {
      if (is_error()) {
         throw std::runtime_error("No value available in error state.");
      }
      return std::get<std::string>(result);
   }

   const std::string error() const {
      if (!is_error()) {
         throw std::runtime_error("No error available in value state.");
      }
      return std::string("Error occurred");  // or return the actual error message if applicable
   }
};

template<typename T>
class CfgStore {
private:
   std::unordered_map<std::string_view, T> active;
   std::unordered_map<std::string_view, T> temp;

   using CfgValidator = std::function<bool(const T&)>;
   std::unordered_map<std::string_view, CfgValidator> validators;
public:
   CfgStore() = default;
   CfgStore(CfgStore&&) = default;
   CfgStore& operator=(CfgStore&&) = default;

   CfgStore(const CfgStore&) = delete;
   CfgStore& operator=(const CfgStore&) = delete;

   [[nodiscard]] CfgResult<T> get(const std::string_view key) const {
      try {
         return CfgResult<T>(active.at(key));
      } catch (const std::out_of_range&) {
         return CfgResult<T>("Config key not found: " + std::string(key));
      }
   }

   [[nodiscard]] CfgResult<bool> set(const std::string_view key, const T& val) {
      auto validator_it = validators.find(key);
      if (validator_it != validators.end()) {
         if (!validator_it->second(val)) {
            return CfgResult<bool>("Validation failed for key: " + std::string(key));
         }
      }
      active[key] = val;
      return CfgResult<bool>(true);
   }

   [[nodiscard]] CfgResult<bool> stage(const std::string_view key, const T& val) {
      auto validator_it = validators.find(key);
      if (validator_it != validators.end()) {
         if (!validator_it->second(val)) {
            return CfgResult<bool>("Validation failed for key: " + std::string(key));
         }
      }
      temp[key] = val;
      return CfgResult<bool>(true);
   }

   void add_validator(const std::string_view key, CfgValidator validator) {
      validators[key] = std::move(validator);
   }

   void commit() {
      active = temp;
   }

   void rollback() {
      temp.clear();
   }
};

enum CfgType {
   CString,
   CInt,
   CBool,
};
using CfgType::CString;
using CfgType::CInt;
using CfgType::CBool;

using CfgVal = std::variant<CfgString, CfgBool, CfgInt>;

struct CfgItem {
   CfgType type;
   CfgVal def;
};

class CfgManager {
private:
   CfgStore<CfgString> string_store;
   CfgStore<CfgBool> bool_store;
   CfgStore<CfgInt> int_store;

   std::unordered_map<std::string, CfgItem> cfg_keys = {
      {"color_theme",         {CString, "Default"}},
      {"shown_boxes",         {CString, "cpu mem net proc"}},
      {"graph_symbol",        {CString, "braille"}},
      {"presets",             {CString, "cpu:1:default,proc:0:default cpu:0:default,mem:0:default,net:0:default cpu:0:block,net:0:tty"}},
      {"graph_symbol_cpu",    {CString, "default"}},
      {"graph_symbol_gpu",    {CString, "default"}},
      {"graph_symbol_mem",    {CString, "default"}},
      {"graph_symbol_net",    {CString, "default"}},
      {"graph_symbol_proc",   {CString, "default"}},
      {"proc_sorting",        {CString, "cpu lazy"}},
      {"cpu_graph_upper",     {CString, "Auto"}},
      {"cpu_graph_lower",     {CString, "Auto"}},
      {"cpu_sensor",          {CString, "Auto"}},
      {"selected_battery",    {CString, "Auto"}},
      {"cpu_core_map",        {CString, ""}},
      {"temp_scale",          {CString, "celsius"}},
      {"clock_format",        {CString, "%X"}},
      {"custom_cpu_name",     {CString, ""}},
      {"disks_filter",        {CString, ""}},
      {"io_graph_speeds",     {CString, ""}},
      {"net_iface",           {CString, ""}},
      {"log_level",           {CString, "WARNING"}},
      {"proc_filter",         {CString, ""}},
      {"proc_command",        {CString, ""}},
      {"selected_name",       {CString, ""}},
#ifdef GPU_SUPPORT
      {"custom_gpu_name0",    {CString, ""}},
      {"custom_gpu_name1",    {CString, ""}},
      {"custom_gpu_name2",    {CString, ""}},
      {"custom_gpu_name3",    {CString, ""}},
      {"custom_gpu_name4",    {CString, ""}},
      {"custom_gpu_name5",    {CString, ""}},
      {"show_gpu_info",       {CString, ""}},
#endif
      {"theme_background",    {CBool, true}},
      {"truecolor",           {CBool, true}},
      {"rounded_corners",     {CBool, true}},
      {"proc_reversed",       {CBool, false}},
      {"proc_tree",           {CBool, false}},
      {"proc_colors",         {CBool, true}},
      {"proc_gradient",       {CBool, true}},
      {"proc_per_core",       {CBool, false}},
      {"proc_mem_bytes",      {CBool, true}},
      {"proc_cpu_graphs",     {CBool, true}},
      {"proc_info_smaps",     {CBool, false}},
      {"proc_left",           {CBool, false}},
      {"proc_filter_kernel",  {CBool, false}},
      {"cpu_invert_lower",    {CBool, true}},
      {"cpu_single_graph",    {CBool, false}},
      {"cpu_bottom",          {CBool, false}},
      {"show_uptime",         {CBool, true}},
      {"check_temp",          {CBool, true}},
      {"show_coretemp",       {CBool, true}},
      {"show_cpu_freq",       {CBool, true}},
      {"background_update",   {CBool, true}},
      {"mem_graphs",          {CBool, true}},
      {"mem_below_net",       {CBool, false}},
      {"zfs_arc_cached",      {CBool, true}},
      {"show_swap",           {CBool, true}},
      {"swap_disk",           {CBool, true}},
      {"show_disks",          {CBool, true}},
      {"only_physical",       {CBool, true}},
      {"use_fstab",           {CBool, true}},
      {"zfs_hide_datasets",   {CBool, false}},
      {"show_io_stat",        {CBool, true}},
      {"io_mode",             {CBool, false}},
      {"base_10_sizes",       {CBool, false}},
      {"io_graph_combined",   {CBool, false}},
      {"net_auto",            {CBool, true}},
      {"net_sync",            {CBool, true}},
      {"show_battery",        {CBool, true}},
      {"show_battery_watts",  {CBool, true}},
      {"vim_keys",            {CBool, false}},
      {"tty_mode",            {CBool, false}},
      {"disk_free_priv",      {CBool, false}},
      {"force_tty",           {CBool, false}},
      {"lowcolor",            {CBool, false}},
      {"show_detailed",       {CBool, false}},
      {"proc_filtering",      {CBool, false}},
      {"proc_aggregate",      {CBool, false}},
#ifdef GPU_SUPPORT
      {"nvml_measure_pcie_speeds",  {CBool, true}},
      {"rsmi_measure_pcie_speeds",  {CBool, true}},
      {"gpu_mirror_graph",          {CBool, true}},
#endif
      {"update_ms",           {CInt, 2000}},
      {"net_download",        {CInt, 100}},
      {"net_upload",          {CInt, 100}},
      {"detailed_pid",        {CInt, 0}},
      {"selected_pid",        {CInt, 0}},
      {"selected_depth",      {CInt, 0}},
      {"proc_start",          {CInt, 0}},
      {"proc_selected",       {CInt, 0}},
      {"proc_last_selected",  {CInt, 0}},
   };

   void setup_validators() {
      /// Graph symbol validation
      string_store.add_validator("graph_symbol", [this](const std::string& val) {
         return std::ranges::find(valid_graph_symbols, val) != valid_graph_symbols.end();
      });
   }

   CfgResult<bool> try_parse_bool(const std::string& val) {
      std::string lower = val;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

      if (lower == "true" || lower == "1" || lower == "yes") {
         return CfgResult<bool>(true);
      } else if (lower == "false" || lower == "0" || lower == "no") {
         return CfgResult<bool>(false);
      }
      return CfgResult<bool>("Invalid boolean value: " + val);
   }

   CfgResult<int> try_parse_int(const std::string& val) {
      try {
         size_t pos;
         int res = std::stoi(val, &pos);
         if (pos != val.length()) {
            return CfgResult<int>("Invalid integer value: " + val);
         }
         return CfgResult<int>(res);
      } catch (const std::exception&) {
         return CfgResult<int>("Invalid integer value: " + val);
      }
   }

   template<typename T>
   CfgResult<bool> try_set(const std::string& key, const std::string& val) {
      if constexpr (std::is_same_v<T, CfgString>) {
         return set<CfgString>(key, val);
      } else if constexpr (std::is_same_v<T, CfgBool>) {
         auto im = try_parse_bool(val);
         if (im.is_error()) {
            return CfgResult<bool>(im.error());
         }
         return set<CfgBool>(key, im.value());
      } else if constexpr (std::is_same_v<T, CfgInt>) {
         auto im = try_parse_int(val);
         if (im.is_error()) {
            return CfgResult<bool>(im.error());
         }
         return set<CfgInt>(key, im.value());
      }
      return CfgResult<bool>("Unsupported type");
   }

   std::filesystem::path cfg_dir;
   std::filesystem::path cfg_file;

   /// CONSTANTS
   static constexpr int ONE_DAY_MILLIS = 1000 * 60 * 60 * 24;

   const std::vector<std::string> valid_graph_symbols{
      "braille", "block", "tty", "default"
   };
   const std::vector<std::string> valid_boxes{
      "cpu", "mem", "net", "proc"
#ifdef GPU_SUPPORT
      , "gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5"
#endif
   };
   const std::vector<std::string> temp_scales{
      "celsius", "fahrenheit", "kelvin", "rankine"
   };
#ifdef GPU_SUPPORT
   const std::vector<std::string> show_gpu_values{
      "Auto", "On", "Off"
   };
#endif

   std::vector<std::string> current_boxes;
   std::vector<std::string> preset_list;
   std::vector<std::string> available_batteries;
public:
   explicit CfgManager(const std::string& cfg_path);

   CfgManager(CfgManager&&) noexcept = default;
   CfgManager& operator=(CfgManager&&) noexcept = default;
    
   CfgManager(const CfgManager&) = delete;
   CfgManager& operator=(const CfgManager&) = delete;

   bool load();
   bool init();

   template<typename T>
   [[nodiscard]] CfgResult<T> get(const std::string_view key) const {
      if constexpr (std::is_same_v<T, CfgString>) {
         return string_store.get(key);
      } else if constexpr (std::is_same_v<T, CfgBool>) {
         return bool_store.get(key);
      } else if constexpr (std::is_same_v<T, CfgInt>) {
         return int_store.get(key);
      } else {
         static_assert(always_false<T>, "Invalid configuration type");
      }
   }

   template<typename T>
   [[nodiscard]] CfgResult<bool> set(const std::string_view key, const T& val) {
      if constexpr (std::is_same_v<T, CfgString>) {
         return string_store.set(key, val);
      } else if constexpr (std::is_same_v<T, CfgBool>) {
         return bool_store.set(key, val);
      } else if constexpr (std::is_same_v<T, CfgInt>) {
         return int_store.set(key, val);
      } else {
         static_assert(always_false<T>, "Invalid configuration type");
      }
   }

   template<typename T>
   static constexpr bool always_false = false;
   
   void flip(std::string_view key) {
      (void)bool_store.set(key, !bool_store.get(key).value());
   }

   [[nodiscard]] bool set_boxes(const std::string& boxes);
   [[nodiscard]] bool toggle_box(const std::string& box);
   [[nodiscard]] bool validate_box_sizes(const std::string& boxes) const;

   [[nodiscard]] bool validate_presets(const std::string& presets) const;
   [[nodiscard]] bool apply_preset(const std::string& preset);

   [[nodiscard]] std::optional<std::filesystem::path>
      get_config_dir() const noexcept;
};

/// GLOBAL CONFIGURATION MANAGER INSTANCE, WILL BE REFACTORED EVENTUALLY
extern CfgManager g_CfgMgr;

//* Functions and variables for reading and writing the btop config 5 file
namespace Config {

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	extern std::unordered_map<std::string_view, string> strings;
	extern std::unordered_map<std::string_view, string> stringsTmp;
	extern std::unordered_map<std::string_view, bool> bools;
	extern std::unordered_map<std::string_view, bool> boolsTmp;
	extern std::unordered_map<std::string_view, int> ints;
	extern std::unordered_map<std::string_view, int> intsTmp;

	const vector<string> valid_graph_symbols = { "braille", "block", "tty" };
	const vector<string> valid_graph_symbols_def = { "default", "braille", "block", "tty" };
	const vector<string> valid_boxes = {
		"cpu", "mem", "net", "proc"
#ifdef GPU_SUPPORT
		,"gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5"
#endif
		};
	const vector<string> temp_scales = { "celsius", "fahrenheit", "kelvin", "rankine" };
#ifdef GPU_SUPPORT
	const vector<string> show_gpu_values = { "Auto", "On", "Off" };
#endif
	extern vector<string> current_boxes;
	extern vector<string> preset_list;
	extern vector<string> available_batteries;
	extern int current_preset;

	constexpr int ONE_DAY_MILLIS = 1000 * 60 * 60 * 24;

	[[nodiscard]] std::optional<std::filesystem::path> get_config_dir() noexcept;

	//* Check if string only contains space separated valid names for boxes and set current_boxes
	bool set_boxes(const string& boxes);

	bool validBoxSizes(const string& boxes);

	//* Toggle box and update config string shown_boxes
	bool toggle_box(const string& box);

	//* Parse and setup config value presets
	bool presetsValid(const string& presets);

	//* Apply selected preset
	bool apply_preset(const string& preset);

	bool _locked(const std::string_view name);

	//* Return bool for config key <name>
	inline const bool& getB(const std::string_view name) { return bools.at(name); }

	//* Return integer for config key <name>
	inline const int& getI(const std::string_view name) { return ints.at(name); }

	//* Return string for config key <name>
	inline const string& getS(const std::string_view name) { return strings.at(name); }

	string getAsString(const std::string_view name);

	extern string validError;

	bool intValid(const std::string_view name, const string& value);
	bool stringValid(const std::string_view name, const string& value);

	//* Set config key <name> to bool <value>
	inline void set(const std::string_view name, bool value) {
		if (_locked(name)) boolsTmp.insert_or_assign(name, value);
		else bools.at(name) = value;
	}

	//* Set config key <name> to int <value>
	inline void set(const std::string_view name, const int value) {
		if (_locked(name)) intsTmp.insert_or_assign(name, value);
		else ints.at(name) = value;
	}

	//* Set config key <name> to string <value>
	inline void set(const std::string_view name, const string& value) {
		if (_locked(name)) stringsTmp.insert_or_assign(name, value);
		else strings.at(name) = value;
	}

	//* Flip config key bool <name>
	void flip(const std::string_view name);

	//* Lock config and cache changes until unlocked
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(const std::filesystem::path& conf_file, vector<string>& load_warnings);

	//* Write the config file to disk
	void write();
}
