// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include <gtest/gtest.h>

#include "btop_shared.hpp"

namespace {
constexpr uint64_t KiB = 1024;
constexpr uint64_t MiB = KiB * KiB;
}

TEST(net, graph_scale_keeps_retained_history_in_range) {
	Net::net_info net;
	net.bandwidth.at("download") = {50 * MiB, 0, 0, 0, 0, 0};

	const auto scale = Net::calculate_graph_max(net, "download", true, false);

	EXPECT_GE(scale, uint64_t(50 * MiB * 1.3));
}

TEST(net, graph_scale_can_fall_after_history_expires) {
	Net::net_info net;
	auto& history = net.bandwidth.at("download");
	history = {50 * MiB, 0, 0, 0, 0, 0};

	EXPECT_GE(Net::calculate_graph_max(net, "download", true, false), uint64_t(50 * MiB * 1.3));
	history.pop_front();
	EXPECT_EQ(Net::calculate_graph_max(net, "download", true, false), 10 * KiB);
}

TEST(net, synced_graph_scale_keeps_peer_retained_history_in_range) {
	Net::net_info net;
	net.bandwidth.at("download") = {0, 0, 0, 0, 0, 0};
	net.bandwidth.at("upload") = {5 * MiB, 0, 0, 0, 0, 0};

	const auto scale = Net::calculate_graph_max(net, "download", true, true);

	EXPECT_GE(scale, uint64_t(5 * MiB * 1.3));
}

TEST(net, unsynced_graph_scale_ignores_peer_history) {
	Net::net_info net;
	net.bandwidth.at("download") = {0, 0, 0, 0, 0, 0};
	net.bandwidth.at("upload") = {5 * MiB, 0, 0, 0, 0, 0};

	EXPECT_EQ(Net::calculate_graph_max(net, "download", true, false), 10 * KiB);
}
