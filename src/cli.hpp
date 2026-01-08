// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace Cli {
	namespace stdfs = std::filesystem;

	// Configuration options set via the command line.
	struct Cli {
		// Alternate path to a configuration file
		std::optional<stdfs::path> config_file;
		// Enable debug mode with additional logs and metrics
		bool debug {};
		// Set an initial process filter.
		std::optional<std::string> filter;
		// Only use ANSI supported graph symbols and colors
		std::optional<bool> force_tty;
		// Use UTF-8 locale even if not detected
		bool force_utf {};
		// Disable true color and only use 256 color mode
		bool low_color {};
		// Start with one of the provided presets
		std::optional<std::uint32_t> preset;
		// Path to a custom themes directory
		std::optional<stdfs::path> themes_dir;
		// The initial refresh rate
		std::optional<std::uint32_t> updates;
	};

	using Result = std::expected<Cli, std::int32_t>;

	// Parse the command line arguments
	[[nodiscard]] auto parse(std::span<const std::string_view> args) noexcept -> Result;

	// Print default config to standard output
	[[nodiscard]] auto default_config() noexcept -> Result;

	// Print a usage header
	void usage() noexcept;

	// Print a help message
	void help() noexcept;

	// Print a hint on how to show more help
	void help_hint() noexcept;
} // namespace Cli
