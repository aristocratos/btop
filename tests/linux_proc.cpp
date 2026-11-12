// SPDX-License-Identifier: Apache-2.0

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utility>

#include <gtest/gtest.h>

#include "btop_shared.hpp"

namespace Shared {
	extern std::filesystem::path procPath, passwd_path;
	extern long pageSize, clkTck;
}

namespace Proc {
	extern vector<proc_info> current_procs;
}

namespace {

class LinuxProc : public testing::Test {
	std::filesystem::path old_proc_path = Shared::procPath;
	std::filesystem::path old_passwd_path = Shared::passwd_path;
	long old_page_size = Shared::pageSize;
	long old_clock_ticks = Shared::clkTck;
	long old_core_count = Shared::coreCount;
	vector<Proc::proc_info> old_procs = std::move(Proc::current_procs);

protected:
	std::filesystem::path root;

	void SetUp() override {
		auto pattern = (std::filesystem::temp_directory_path() / "btop-proc-test-XXXXXX").string();
		const auto directory = mkdtemp(pattern.data());
		ASSERT_NE(directory, nullptr);
		root = directory;
		Shared::procPath = root;
		Shared::passwd_path.clear();
		Shared::pageSize = 4096;
		Shared::clkTck = 100;
		Shared::coreCount = 1;
		Proc::current_procs.clear();
		std::filesystem::create_directory(root / "123");
		std::ofstream(root / "meminfo") << "MemTotal: 134217728 kB\n";
		std::ofstream(root / "uptime") << "100000.00 0.00\n";
		std::ofstream(root / "stat") << "cpu 1000 0 2000 3000 0 0 0 0\n";
		std::ofstream(root / "123/status") << "Uid:\t0\t0\t0\t0\n";
		std::ofstream(root / "123/cmdline") << "worker";
		std::ofstream(root / "123/statm") << "65536 4096 0 0 0 0 0\n";
	}

	void TearDown() override {
		Shared::procPath = old_proc_path;
		Shared::passwd_path = old_passwd_path;
		Shared::pageSize = old_page_size;
		Shared::clkTck = old_clock_ticks;
		Shared::coreCount = old_core_count;
		Proc::current_procs = std::move(old_procs);
		if (not root.empty()) std::filesystem::remove_all(root);
	}

	void write_process(const string& name, const string& comm) {
		std::ofstream(root / "123/comm") << comm << '\n';
		// Fields 3..24: state, parent, CPU ticks, nice, threads, start time, size, RSS.
		std::ofstream(root / "123/stat") << "123 (" << name
			<< ") S 1 0 0 0 0 0 0 0 0 0 120 30 0 0 20 -3 7 0 9000000 268435456 4096\n";
	}

	void expect_stats() {
		const auto& processes = Proc::collect();
		ASSERT_EQ(processes.size(), 1);
		const auto& process = processes.front();
		EXPECT_EQ(process.state, 'S');
		EXPECT_EQ(process.ppid, 1);
		EXPECT_EQ(process.cpu_t, 150);
		EXPECT_EQ(process.p_nice, -3);
		EXPECT_EQ(process.threads, 7);
		EXPECT_EQ(process.cpu_s, 9000000);
		EXPECT_EQ(process.mem, 16 * 1024 * 1024);
	}
};

TEST_F(LinuxProc, PlainName) {
	write_process("worker", "worker");
	expect_stats();
}

TEST_F(LinuxProc, NameGainsSpacesAfterFirstSample) {
	write_process("worker", "worker");
	expect_stats();
	write_process("worker exec job", "worker exec job");
	expect_stats();
	write_process("worker", "worker");
	expect_stats();
}

TEST_F(LinuxProc, NameLosesSpacesAfterFirstSample) {
	write_process("worker exec job", "worker exec job");
	expect_stats();
	write_process("worker", "worker");
	expect_stats();
}

TEST_F(LinuxProc, StatNameDiffersFromCommOnFirstSample) {
	write_process("worker exec job", "worker");
	expect_stats();
}

TEST_F(LinuxProc, ParenthesesInName) {
	write_process("worker", "worker");
	expect_stats();
	write_process("worker ) (job)", "worker ) (job)");
	expect_stats();
}

TEST_F(LinuxProc, NewlineInName) {
	write_process("worker\n job", "worker\n job");
	expect_stats();
}

} // namespace
