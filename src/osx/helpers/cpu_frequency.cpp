// SPDX-License-Identifier: Apache-2.0

#include "../intel_mac.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
	constexpr auto temporary_path = "/var/run/btop-cpu-frequency-mhz.tmp";

	volatile sig_atomic_t stopping = 0;
	volatile sig_atomic_t child_pid = 0;

	void stop(const int) {
		stopping = 1;
		if (child_pid > 0) kill(child_pid, SIGTERM);
	}

	auto publish_frequency(const double mhz) -> bool {
		const auto output = open(
			temporary_path,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
			0644
		);
		if (output < 0) return false;

		const auto written = dprintf(output, "%.2f\n", mhz);
		const auto close_result = close(output);
		if (written <= 0 or close_result != 0) {
			unlink(temporary_path);
			return false;
		}
		if (rename(temporary_path, IntelMac::cpu_frequency_path) != 0) {
			unlink(temporary_path);
			return false;
		}
		return true;
	}

	auto spawn_powermetrics(int& output_fd) -> pid_t {
		int pipe_fds[2]{};
		if (pipe(pipe_fds) != 0) return -1;

		posix_spawn_file_actions_t actions{};
		if (posix_spawn_file_actions_init(&actions) != 0) {
			close(pipe_fds[0]);
			close(pipe_fds[1]);
			return -1;
		}
		posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
		posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
		posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

		char path[] = "/usr/bin/powermetrics";
		char interval[] = "-i";
		char interval_value[16]{};
		std::snprintf(
			interval_value,
			sizeof(interval_value),
			"%u",
			IntelMac::cpu_frequency_sample_interval_ms
		);
		char buffer_size[] = "-b";
		char line_buffered[] = "1";
		char sampler[] = "-s";
		char sampler_value[] = "cpu_power";
		char pstates[] = "--show-pstates";
		char* arguments[] = {
			path,
			interval,
			interval_value,
			buffer_size,
			line_buffered,
			sampler,
			sampler_value,
			pstates,
			nullptr,
		};
		char* environment[] = {nullptr};

		pid_t pid = -1;
		const auto result = posix_spawn(&pid, path, &actions, nullptr, arguments, environment);
		posix_spawn_file_actions_destroy(&actions);
		close(pipe_fds[1]);
		if (result != 0) {
			close(pipe_fds[0]);
			errno = result;
			return -1;
		}

		output_fd = pipe_fds[0];
		return pid;
	}
}

int main() {
	if (geteuid() != 0) {
		std::fprintf(stderr, "btop CPU frequency helper must run as root\n");
		return 1;
	}

	umask(022);
	unlink(IntelMac::cpu_frequency_path);
	unlink(temporary_path);
	std::signal(SIGINT, stop);
	std::signal(SIGTERM, stop);

	int output_fd = -1;
	const auto pid = spawn_powermetrics(output_fd);
	if (pid < 0) {
		std::fprintf(stderr, "failed to start powermetrics: %s\n", std::strerror(errno));
		return 1;
	}
	child_pid = pid;

	auto* output = fdopen(output_fd, "r");
	if (output == nullptr) {
		close(output_fd);
		kill(pid, SIGTERM);
		waitpid(pid, nullptr, 0);
		return 1;
	}

	char* line = nullptr;
	std::size_t capacity = 0;
	while (not stopping and getline(&line, &capacity, output) >= 0) {
		if (const auto mhz = IntelMac::parse_powermetrics_cpu_mhz(line); mhz.has_value()
		and not publish_frequency(mhz.value())) {
			std::fprintf(stderr, "failed to publish CPU frequency: %s\n", std::strerror(errno));
			break;
		}
	}

	free(line);
	fclose(output);
	if (not stopping) kill(pid, SIGTERM);
	waitpid(pid, nullptr, 0);
	child_pid = 0;
	unlink(IntelMac::cpu_frequency_path);
	unlink(temporary_path);
	return stopping ? 0 : 1;
}
