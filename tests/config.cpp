// SPDX-License-Identifier: Apache-2.0

#include "btop_config.hpp"

#include <cstdlib>
#include <fstream>
#include <unistd.h>

#include <gtest/gtest.h>

TEST(config, gpu_utilization_mode_is_portable) {
	//? Load a macOS option followed by a shared option, without touching user config.
	auto filename = (std::filesystem::temp_directory_path() / "btop-config-XXXXXX").string();
	const int fd = mkstemp(filename.data());
	ASSERT_NE(fd, -1);
	close(fd);
	{
		std::ofstream config(filename);
		config << "# btop test configuration\n"
			"gpu_utilization_mode = \"normalized\"\n"
			"color_theme = \"portable-config-test\"\n";
	}
	const auto old_theme = Config::getS("color_theme");
	const bool old_write_new = Config::write_new;
#if defined(__APPLE__) and defined(GPU_SUPPORT)
	const auto old_mode = Config::getS("gpu_utilization_mode");
#endif
	std::vector<std::string> warnings;
	Config::load(filename, warnings);
	std::filesystem::remove(filename);
	EXPECT_TRUE(warnings.empty());
	EXPECT_EQ(Config::getS("color_theme"), "portable-config-test");
#if defined(__APPLE__) and defined(GPU_SUPPORT)
	EXPECT_EQ(Config::getS("gpu_utilization_mode"), "normalized");
	Config::strings.at("gpu_utilization_mode") = old_mode;
#else
	//? Linux and builds without GPU support must silently skip the unknown key.
	EXPECT_FALSE(Config::strings.contains("gpu_utilization_mode"));
#endif
	Config::strings.at("color_theme") = old_theme;
	Config::write_new = old_write_new;
}
