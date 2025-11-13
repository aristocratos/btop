// SPDX-License-Identifier: Apache-2.0

#include "btop_cli.hpp"

#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/base.h>
#include <fmt/format.h>

#include "btop_shared.hpp"
#include "config.h"

using namespace std::string_view_literals;

static constexpr auto BOLD = "\033[1m"sv;
static constexpr auto BOLD_UNDERLINE = "\033[1;4m"sv;
static constexpr auto BOLD_RED = "\033[1;31m"sv;
static constexpr auto YELLOW = "\033[33m"sv;
static constexpr auto RESET = "\033[0m"sv;

static void version() noexcept {
	if constexpr (GIT_COMMIT.empty()) {
		fmt::println("btop version: {}{}{}", BOLD, Global::Version, RESET);
	} else {
		fmt::println("btop version: {}{}+{}{}", BOLD, Global::Version, GIT_COMMIT, RESET);
	}
}

static void build_info() noexcept {
	fmt::println("Compiled with: {} ({})", COMPILER, COMPILER_VERSION);
	fmt::println("Configured with: {}", CONFIGURE_COMMAND);
}

static void error(std::string_view msg) noexcept {
	fmt::println("{}error:{} {}\n", BOLD_RED, RESET, msg);
}

namespace Cli {
	[[nodiscard]] auto parse(const std::span<const std::string_view> args) noexcept -> Result {
		Cli cli {};

		for (auto it = args.begin(); it != args.end(); ++it) {
			auto arg = *it;

			if (arg == "-h" || arg == "--help") {
				usage();
				help();
				return std::unexpected { 0 };
			}
			if (arg == "-v" || arg == "-V") {
				version();
				return std::unexpected { 0 };
			}
			if (arg == "--version") {
				version();
				build_info();
				return std::unexpected { 0 };
			}

			if (arg == "-d" || arg == "--debug") {
				cli.debug = true;
				continue;
			}
			if (arg == "--force-utf") {
				cli.force_utf = true;
				continue;
			}
			if (arg == "-l" || arg == "--low-color") {
				cli.low_color = true;
				continue;
			}
			if (arg == "-t" || arg == "--tty") {
				if (cli.force_tty.has_value()) {
					error("tty mode can't be set twice");
					return std::unexpected { 1 };
				}
				cli.force_tty = std::make_optional(true);
				continue;
			}
			if (arg == "--no-tty") {
				if (cli.force_tty.has_value()) {
					error("tty mode can't be set twice");
					return std::unexpected { 1 };
				}
				cli.force_tty = std::make_optional(false);
				continue;
			}

			if (arg == "-c" || arg == "--config") {
				// This flag requires an argument.
				if (++it == args.end()) {
					error("Config requires an argument");
					return std::unexpected { 1 };
				}

				auto arg = *it;
				auto config_file = stdfs::path { arg };

				if (stdfs::is_directory(config_file)) {
					error("Config file can't be a directory");
					return std::unexpected { 1 };
				}

				cli.config_file = std::make_optional(config_file);
				continue;
			}
			if (arg == "-f" || arg == "--filter") {
				// This flag requires an argument.
				if (++it == args.end()) {
					error("Filter requires an argument");
					return std::unexpected { 1 };
				}

				auto arg = *it;
				cli.filter = std::make_optional(arg);
				continue;
			}
			if (arg == "-p" || arg == "--preset") {
				// This flag requires an argument.
				if (++it == args.end()) {
					error("Preset requires an argument");
					return std::unexpected { 1 };
				}

				auto arg = *it;
				try {
					auto preset_id = std::clamp(std::stoi(arg.data()), 0, 9);
					cli.preset = std::make_optional(preset_id);
				} catch (std::invalid_argument& e) {
					error("Preset must be a positive number");
					return std::unexpected { 1 };
				} catch (std::out_of_range& e) {
					error(fmt::format("Preset argument is out of range: {}", arg.data()));
					return std::unexpected { 1 };
				}
				continue;
			}
			if (arg == "--themes-dir") {
				// This flag requires an argument.
				if (++it == args.end()) {
					error("Themes directory requires an argument");
					return std::unexpected { 1 };
				}

				auto arg = *it;
				auto themes_dir = stdfs::path { arg };

				if (not stdfs::is_directory(themes_dir)) {
					error("Themes directory does not exist or is not a directory");
					return std::unexpected { 1 };
				}

				cli.themes_dir = std::make_optional(themes_dir);
				continue;
			}
			if (arg == "-u" || arg == "--update") {
				// This flag requires an argument.
				if (++it == args.end()) {
					error("Update requires an argument");
					return std::unexpected { 1 };
				}

				auto arg = *it;
				try {
					auto refresh_rate = std::max(std::stoi(arg.data()), 100);
					cli.updates = refresh_rate;
				} catch (std::invalid_argument& e) {
					error("Update must be a positive number");
					return std::unexpected { 1 };
				} catch (std::out_of_range& e) {
					error(fmt::format("Update argument is out of range: {}", arg.data()));
					return std::unexpected { 1 };
				}
				continue;
			}

			error(fmt::format("Unknown argument '{}{}{}'", YELLOW, arg, RESET));
			return std::unexpected { 1 };
		}
		return cli;
	}

	void usage() noexcept {
		fmt::println("{0}Usage:{1} {2}btop{1} [OPTIONS]\n", BOLD_UNDERLINE, RESET, BOLD);
	}

	void help() noexcept {
		fmt::print(
			"{0}Options:{1}\n"
			"  {2}-c, --config{1} <file>     Path to a config file\n"
			"  {2}-d, --debug{1}             Start in debug mode with additional logs and metrics\n"
			"  {2}-f, --filter{1} <filter>   Set an initial process filter\n"
			"  {2}    --force-utf{1}         Override automatic UTF locale detection\n"
			"  {2}-l, --low-color{1}         Disable true color, 256 colors only\n"
			"  {2}-p, --preset{1} <id>       Start with a preset (0-9)\n"
			"  {2}-t, --tty{1}               Force tty mode with ANSI graph symbols and 16 colors only\n"
			"  {2}    --themes-dir{1} <dir>  Path to a custom themes directory\n"
			"  {2}    --no-tty{1}            Force disable tty mode\n"
			"  {2}-u, --update{1} <ms>       Set an initial update rate in milliseconds\n"
			"  {2}-h, --help{1}              Show this help message and exit\n"
			"  {2}-V, --version{1}           Show a version message and exit (more with --version)\n",
			BOLD_UNDERLINE, RESET, BOLD
		);
	}

	void help_hint() noexcept {
		fmt::println("For more information, try '{}--help{}'", BOLD, RESET);
	}
} // namespace Cli
