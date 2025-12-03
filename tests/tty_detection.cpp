// SPDX-License-Identifier: Apache-2.0

#include <cstdlib>
#include <gtest/gtest.h>

#include "btop.hpp"

class TtyDetectionTest : public ::testing::Test {
protected:
	void SetUp() override {
		//? Clear relevant environment variables before each test
		unsetenv("COLORTERM");
		unsetenv("TERM_PROGRAM");
		unsetenv("TERM");
	}

	void TearDown() override {
		//? Clean up after each test
		unsetenv("COLORTERM");
		unsetenv("TERM_PROGRAM");
		unsetenv("TERM");
	}
};

//* Test COLORTERM environment variable detection
TEST_F(TtyDetectionTest, colorterm_truecolor) {
	setenv("COLORTERM", "truecolor", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, colorterm_24bit) {
	setenv("COLORTERM", "24bit", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, colorterm_truecolor_uppercase) {
	setenv("COLORTERM", "TRUECOLOR", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, colorterm_24bit_mixed_case) {
	setenv("COLORTERM", "24Bit", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, colorterm_invalid) {
	setenv("COLORTERM", "256color", 1);
	EXPECT_FALSE(supports_truecolor());
}

//* Test TERM_PROGRAM environment variable detection
TEST_F(TtyDetectionTest, term_program_iterm) {
	setenv("TERM_PROGRAM", "iTerm.app", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_vscode) {
	setenv("TERM_PROGRAM", "vscode", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_wezterm) {
	setenv("TERM_PROGRAM", "WezTerm", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_hyper) {
	setenv("TERM_PROGRAM", "Hyper", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_unsupported) {
	setenv("TERM_PROGRAM", "xterm", 1);
	EXPECT_FALSE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_uppercase) {
	setenv("TERM_PROGRAM", "VSCODE", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_program_mixed_case_iterm) {
	setenv("TERM_PROGRAM", "ITERM.APP", 1);
	EXPECT_TRUE(supports_truecolor());
}

//* Test TERM environment variable detection
TEST_F(TtyDetectionTest, term_with_truecolor) {
	setenv("TERM", "xterm-truecolor", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_with_24bit) {
	setenv("TERM", "xterm-24bit", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_with_direct) {
	setenv("TERM", "xterm-direct", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_regular_xterm) {
	setenv("TERM", "xterm-256color", 1);
	EXPECT_FALSE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_basic) {
	setenv("TERM", "xterm", 1);
	EXPECT_FALSE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_uppercase_truecolor) {
	setenv("TERM", "XTERM-TRUECOLOR", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, term_mixed_case_direct) {
	setenv("TERM", "xterm-DIRECT", 1);
	EXPECT_TRUE(supports_truecolor());
}

//* Test no environment variables set
TEST_F(TtyDetectionTest, no_env_vars) {
	EXPECT_FALSE(supports_truecolor());
}

//* Test priority: COLORTERM should be checked first
TEST_F(TtyDetectionTest, colorterm_overrides_term) {
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERM", "xterm", 1);
	EXPECT_TRUE(supports_truecolor());
}

//* Test real-world scenarios
TEST_F(TtyDetectionTest, ssh_with_truecolor_terminal) {
	//? Simulating SSH into a system from a truecolor terminal
	setenv("COLORTERM", "truecolor", 1);
	setenv("TERM", "xterm-256color", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, framebuffer_with_virtio) {
	//? Linux framebuffer console with virtio graphics
	setenv("COLORTERM", "truecolor", 1);
	EXPECT_TRUE(supports_truecolor());
}

TEST_F(TtyDetectionTest, legacy_terminal) {
	//? Old terminal without truecolor support
	setenv("TERM", "xterm", 1);
	EXPECT_FALSE(supports_truecolor());
}
