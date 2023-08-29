// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Logger {
	const std::vector<std::string> log_levels = {
		"DISABLED",
		"ERROR",
		"WARNING",
		"INFO",
		"DEBUG",
	};
	extern std::filesystem::path logfile;

	//* Set log level, valid arguments: "DISABLED", "ERROR", "WARNING", "INFO" and "DEBUG"
	void set(const std::string& level);

	void log_write(const size_t level, const std::string& msg);
	inline void error(const std::string msg) { log_write(1, msg); }
	inline void warning(const std::string msg) { log_write(2, msg); }
	inline void info(const std::string msg) { log_write(3, msg); }
	inline void debug(const std::string msg) { log_write(4, msg); }
} // namespace Logger
