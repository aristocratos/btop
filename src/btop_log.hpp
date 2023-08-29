// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <fmt/core.h>

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

	void log_write(const size_t level, const std::string_view msg);

	template <typename... T>
	inline void error(const fmt::format_string<T...> fmt, T&&... args) {
		log_write(1, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename... T>
	inline void warning(const fmt::format_string<T...> fmt, T&&... args) {
		log_write(2, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename... T>
	inline void info(const fmt::format_string<T...> fmt, T&&... args) {
		log_write(3, fmt::format(fmt, std::forward<T>(args)...));
	}

	template <typename... T>
	inline void debug(const fmt::format_string<T...> fmt, T&&... args) {
		log_write(4, fmt::format(fmt, std::forward<T>(args)...));
	}
} // namespace Logger
