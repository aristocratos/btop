// SPDX-License-Identifier: Apache-2.0

#include "btop_log.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace fs = std::filesystem;

namespace Logger {
	using namespace Tools;
	std::mutex log_mtx {};
	bool first = true;
	const string tdf = "%Y/%m/%d (%T) | ";

	size_t loglevel;
	fs::path logfile;

	//* Wrapper for lowering priviliges if using SUID bit and currently isn't using real userid
	class lose_priv {
		int status = -1;

	public:
		lose_priv() {
			if (geteuid() != Global::real_uid) {
				this->status = seteuid(Global::real_uid);
			}
		}
		~lose_priv() {
			if (status == 0) {
				status = seteuid(Global::set_uid);
			}
		}
	};

	void set(const string& level) { loglevel = v_index(log_levels, level); }

	void log_write(const size_t level, const std::string_view msg) {
		if (loglevel < level or logfile.empty()) {
			return;
		}
		std::lock_guard lock {log_mtx};
		lose_priv neutered{};
		std::error_code ec;
		try {
			if (fs::exists(logfile) and fs::file_size(logfile, ec) > 1024 << 10 and not ec) {
				auto old_log = logfile;
				old_log += ".1";

				if (fs::exists(old_log)) {
					fs::remove(old_log, ec);
				}

				if (not ec) {
					fs::rename(logfile, old_log, ec);
				}
			}
			if (not ec) {
				std::ofstream lwrite(logfile, std::ios::app);
				if (first) {
					first = false;
					fmt::print(lwrite, "\n{}===> btop++ v.{}\n", strf_time(tdf), Global::Version);
				}
				fmt::print(lwrite, "{}{}: {}\n", strf_time(tdf), log_levels.at(level), msg);
			}
			else {
				logfile.clear();
			}
		}
		catch (const std::exception& e) {
			logfile.clear();
			throw std::runtime_error(fmt::format("Exception in Logger::log_write() : {}", e.what()));
		}
	}
} // namespace Logger
