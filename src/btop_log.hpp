// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <source_location>
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
	void set(const std::string& level) noexcept;

	void log(const size_t level, const std::string_view msg, const std::source_location location);

	template<typename... Args>
	struct error { // NOLINT(readability-identifier-naming)
		constexpr error(
			const fmt::format_string<Args...> fmt, Args&&... args,
			const std::source_location location = std::source_location::current()
		) {
			log(1, fmt::format(fmt, std::forward<Args>(args)...), location);
		}
	};

	template<typename... Args>
	struct warning { // NOLINT(readability-identifier-naming)
		constexpr warning(
			const fmt::format_string<Args...> fmt, Args&&... args,
			const std::source_location location = std::source_location::current()
		) {
			log(2, fmt::format(fmt, std::forward<Args>(args)...), location);
		}
	};

	template<typename... Args>
	struct info { // NOLINT(readability-identifier-naming)
		constexpr info(
			const fmt::format_string<Args...> fmt, Args&&... args,
			const std::source_location location = std::source_location::current()
		) {
			log(3, fmt::format(fmt, std::forward<Args>(args)...), location);
		}
	};

	template<typename... Args>
	struct debug { // NOLINT(readability-identifier-naming)
		constexpr debug(
			const fmt::format_string<Args...> fmt, Args&&... args,
			const std::source_location location = std::source_location::current()
		) {
			log(4, fmt::format(fmt, std::forward<Args>(args)...), location);
		}
	};

	// Deduction guide to allow default arguement after variadic template
	template<typename... Args>
	error(const std::string_view, Args&&...) -> error<Args...>;

	template<typename... Args>
	warning(const std::string_view, Args&&...) -> warning<Args...>;

	template<typename... Args>
	info(const std::string_view, Args&&...) -> info<Args...>;

	template<typename... Args>
	debug(const std::string_view, Args&&...) -> debug<Args...>;
} // namespace Logger
