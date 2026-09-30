// SPDX-License-Identifier: Apache-2.0

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "btop_shared.hpp"

using Ctr::ctr_info;
using Ctr::parse_cgroup;

static const std::string id = "3f2a9c1b04de5a7788990011223344556677889900aabbccddeeff0011223344";

TEST(container, oci_runtimes) {
	{
		const auto path = "/system.slice/docker-" + id + ".scope";
		EXPECT_EQ(parse_cgroup(path), (ctr_info {"docker", "3f2a9c1b04de", path}));
		//? Cgroups created inside the container belong to the container
		EXPECT_EQ(parse_cgroup(path + "/system.slice/nginx.service"), (ctr_info {"docker", "3f2a9c1b04de", path}));
	}
	EXPECT_EQ(parse_cgroup("/docker/" + id), (ctr_info {"docker", "3f2a9c1b04de", "/docker/" + id}));
	{
		const auto path = "/user.slice/user-1000.slice/user@1000.service/user.slice/libpod-" + id + ".scope";
		EXPECT_EQ(parse_cgroup(path + "/container"), (ctr_info {"podman", "3f2a9c1b04de", path}));
	}
	{
		const auto path = "/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod1234.slice/cri-containerd-" + id + ".scope";
		EXPECT_EQ(parse_cgroup(path), (ctr_info {"k8s", "3f2a9c1b04de", path}));
	}
	EXPECT_EQ(parse_cgroup("/kubepods/burstable/pod1234/" + id), (ctr_info {"k8s", "3f2a9c1b04de", "/kubepods/burstable/pod1234/" + id}));
	EXPECT_EQ(parse_cgroup("/system.slice/nerdctl-" + id + ".scope"), (ctr_info {"nerdctl", "3f2a9c1b04de", "/system.slice/nerdctl-" + id + ".scope"}));
	EXPECT_EQ(parse_cgroup("/default/" + id), (ctr_info {"container", "3f2a9c1b04de", "/default/" + id}));
}

TEST(container, named) {
	EXPECT_EQ(parse_cgroup("/lxc.payload.web/system.slice/nginx.service"), (ctr_info {"lxc", "web", "/lxc.payload.web"}));
	EXPECT_EQ(parse_cgroup("/lxc/101/ns/init.scope"), (ctr_info {"lxc", "101", "/lxc/101"}));
	EXPECT_EQ(parse_cgroup("/machine.slice/machine-my\\x2dbox.scope/payload"), (ctr_info {"nspawn", "my-box", "/machine.slice/machine-my\\x2dbox.scope"}));
}

TEST(container, nested_is_outermost) {
	const auto path = "/lxc.payload.host/system.slice/docker-" + id + ".scope";
	EXPECT_EQ(parse_cgroup(path), (ctr_info {"lxc", "host", "/lxc.payload.host"}));
}

TEST(container, not_a_container) {
	for (const auto& path : {
		std::string {},
		std::string {"/"},
		std::string {"/init.scope"},
		std::string {"/user.slice/user-1000.slice/session-3.scope"},
		std::string {"/system.slice/docker.service"},
		std::string {"/lxc.monitor.web"},
		std::string {"/lxc.monitor/101"},
		std::string {"/machine.slice/machine-qemu\\x2d1\\x2dvm.scope"},
		"/machine.slice/libpod-conmon-" + id + ".scope",
		"/system.slice/docker-" + id.substr(1) + ".scope",
	}) {
		EXPECT_EQ(parse_cgroup(path), std::nullopt) << path;
	}
}

TEST(container, docker_name) {
	const auto response = "HTTP/1.0 200 OK\r\n\r\n[{\"Id\":\"" + id + "\",\"Names\":[\"/web-1\"],\"Image\":\"nginx\","
		"\"Labels\":{\"Id\":\"3f2a\"}},{\"Id\":\"aa" + id.substr(2) + "\",\"Names\":[\"/bad\\u001b[31m\"]}]";
	EXPECT_EQ(Ctr::docker_name(response, "3f2a9c1b04de"), "web-1");
	//? Names that can't be docker names are rejected
	EXPECT_EQ(Ctr::docker_name(response, "aa2a9c1b04de"), "");
	EXPECT_EQ(Ctr::docker_name(response, "ffffffffffff"), "");
	EXPECT_EQ(Ctr::docker_name("", "3f2a9c1b04de"), "");
	EXPECT_EQ(Ctr::docker_name("{\"Id\":\"3f2a9c1b04de", "3f2a9c1b04de"), "");
}
