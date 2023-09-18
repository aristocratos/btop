// SPDX-License-Identifier: Apache-2.0

#include "btop_log.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>

#if defined(HAVE_JOURNALD)
	#include <systemd/sd-journal.h>
#elif defined(HAVE_SYSLOG)
	#include <syslog.h>
#endif

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace fs = std::filesystem;

namespace Logger {
	using namespace Tools;
	std::mutex log_mtx{};
	bool first = true;

	size_t loglevel;
	fs::path logfile;
	constexpr const std::string_view tdf = "%Y/%m/%d (%T) | ";

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

	void log(const size_t level, const std::string_view msg, [[maybe_unused]] const std::source_location location) {
		if (loglevel < level) {
			return;
		}

		std::lock_guard lock{log_mtx};
		lose_priv neutered{};

#if defined(HAVE_JOURNALD) || defined(HAVE_SYSLOG)
		int status = LOG_DEBUG;
		switch (level) {
			case 1: status = LOG_ERR; break;
			case 2: status = LOG_WARNING; break;
			case 3: status = LOG_INFO; break;
			case 4: status = LOG_DEBUG; break;
		}
#endif

#if defined(HAVE_JOURNALD)
		using namespace std::literals;
		sd_journal_print_with_location( // NOLINT(cppcoreguidelines-pro-type-vararg)
			status,
			("CODE_FILE="s + location.file_name()).data(),
			("CODE_LINE="s + std::to_string(location.line())).data(),
			location.function_name(),
			"%s",
			msg.data()
		);
#elif defined(HAVE_SYSLOG)
		syslog(status, "%s", msg.data()); // NOLINT(cppcoreguidelines-pro-type-vararg)
#else
		if (logfile.empty()) {
			return;
		}
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
#endif
	}
} // namespace Logger
