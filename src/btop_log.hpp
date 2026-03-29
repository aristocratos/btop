// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <fmt/std.h>

namespace Logger {
	enum class Level : std::uint8_t {
		DISABLED = 0,
		ERROR = 1,
		WARNING = 2,
		INFO = 3,
		DEBUG = 4,
	};

	const std::vector<std::string> log_levels { "DISABLED", "ERROR", "WARNING", "INFO", "DEBUG" };

	namespace detail {
		auto is_enabled(Level level) -> bool;

		void log_write(Level level, const std::string_view msg);
	} // namespace detail

	void init(const std::filesystem::path& path);

	void set_log_level(Level level);

	void set_log_level(const std::string_view level);

	template<typename... T>
	inline void error(fmt::format_string<T...> fmt, T&&... args) {
		// Check if log level is enabled before allocating string.
		if (detail::is_enabled(Level::ERROR)) {
			detail::log_write(Level::ERROR, fmt::format(fmt, std::forward<T>(args)...));
		}
	}

	template<typename... T>
	inline void warning(fmt::format_string<T...> fmt, T&&... args) {
		// Check if log level is enabled before allocating string.
		if (detail::is_enabled(Level::WARNING)) {
			detail::log_write(Level::WARNING, fmt::format(fmt, std::forward<T>(args)...));
		}
	}

	template<typename... T>
	inline void info(fmt::format_string<T...> fmt, T&&... args) {
		// Check if log level is enabled before allocating string.
		if (detail::is_enabled(Level::INFO)) {
			detail::log_write(Level::INFO, fmt::format(fmt, std::forward<T>(args)...));
		}
	}

	template<typename... T>
	inline void debug(fmt::format_string<T...> fmt, T&&... args) {
		// Check if log level is enabled before allocating string.
		if (detail::is_enabled(Level::DEBUG)) {
			detail::log_write(Level::DEBUG, fmt::format(fmt, std::forward<T>(args)...));
		}
	}
} // namespace Logger
