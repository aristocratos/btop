// SPDX-License-Identifier: Apache-2.0

#include "btop_log.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

#include "btop_shared.hpp"
#include "btop_tools.hpp"

namespace fs = std::filesystem;

namespace Logger {
	using namespace Tools;
	std::atomic<bool> busy(false);
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

	void log_write(const size_t level, const string& msg) {
		if (loglevel < level or logfile.empty()) {
			return;
		}
		atomic_lock lck(busy, true);
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
					lwrite << "\n" << strf_time(tdf) << "===> btop++ v." << Global::Version << "\n";
				}
				lwrite << strf_time(tdf) << log_levels.at(level) << ": " << msg << "\n";
			}
			else {
				logfile.clear();
			}
		}
		catch (const std::exception& e) {
			logfile.clear();
			throw std::runtime_error("Exception in Logger::log_write() : " + string{e.what()});
		}
	}
} // namespace Logger
