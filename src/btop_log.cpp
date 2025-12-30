// SPDX-License-Identifier: Apache-2.0

#include "btop_log.hpp"

#include "btop_shared.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace Logger {
	namespace {
		constexpr auto ONE_MEGABYTE = 1024 << 10;

		struct State {
			std::mutex lock {};
			std::once_flag print_header;
			Level level = Level::ERROR;
			std::optional<fs::path> path {};
		};

		[[nodiscard]] auto get_state() -> State& {
			static State state;
			return state;
		}

		auto rotate_log_file(fs::path& path) {
			std::error_code errc {};
			if (fs::exists(path, errc) && !errc && fs::file_size(path, errc) > ONE_MEGABYTE && !errc) {
				auto old_path = path;
				old_path += ".1";
				if (fs::exists(old_path, errc) && !errc) {
					fs::remove(old_path, errc);
				}
				if (!errc) {
					fs::rename(path, old_path, errc);
				}
			}
			return !errc;
		}

		class DropPrivilegeGuard {
		private:
			uid_t saved_euid {};

		public:
			DropPrivilegeGuard() {
				saved_euid = geteuid();
				auto real_uid = getuid();
				if (saved_euid != real_uid && seteuid(real_uid) != 0) {
					throw std::runtime_error(
						fmt::format("Failed to drop privileges to write log file: {}", strerror(errno))
					);
				}
			}

			~DropPrivilegeGuard() noexcept {
				if (saved_euid != geteuid()) {
					// Silently drop error status.
					seteuid(saved_euid);
				}
			}

			DropPrivilegeGuard(const DropPrivilegeGuard&) = delete;
			DropPrivilegeGuard& operator=(const DropPrivilegeGuard&) = delete;
		};
	} // namespace

	void init(const fs::path& path) {
		auto& state = get_state();
		std::lock_guard guard { state.lock };
		state.path = std::make_optional(path);
	}

	void set_log_level(Level level) {
		auto& state = get_state();
		std::lock_guard guard { state.lock };
		state.level = level;
	}

	void set_log_level(const std::string_view level) {
		auto& state = get_state();
		auto it = std::ranges::find(log_levels, level);
		if (it != log_levels.end()) {
			std::lock_guard guard { state.lock };
			state.level = static_cast<Level>(std::distance(log_levels.begin(), it));
		}
	}

	namespace detail {
		[[nodiscard]] auto is_enabled(Level level) -> bool {
			auto& state = get_state();
			return state.level >= level && state.path.has_value();
		}

		void log_write(Level level, const std::string_view msg) {
			auto& state = get_state();

			auto guard = std::lock_guard { state.lock };

			if (!is_enabled(level) || !state.path.has_value()) {
				return;
			}

			std::string buffer {};
			std::call_once(state.print_header, [&buffer]() {
				fmt::format_to(std::back_inserter(buffer), "\n===> btop++ v{}\n", Global::Version);
			});
			auto now = std::chrono::system_clock::now();
			auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
			fmt::format_to(
				std::back_inserter(buffer),
				"{:%F (%T)} | {}: {}\n",
				seconds,
				log_levels.at(std::to_underlying(level)),
				msg
			);

			DropPrivilegeGuard privilege_guard {};

			auto& path = state.path.value();
			if (!rotate_log_file(path)) {
				return;
			}

			auto file = std::ofstream { path, std::ios::app };
			if (!file.is_open()) {
				return;
			}
			file << buffer;
		}
	} // namespace detail

} // namespace Logger
