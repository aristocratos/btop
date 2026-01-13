// SPDX-License-Identifier: Apache-2.0

#include <vector>
#include <limits>
#include <cstdint>

#include <gtest/gtest.h>

#include "btop_tools.hpp"

TEST(tools, string_split) {
	EXPECT_EQ(Tools::ssplit(""), std::vector<std::string> {});
	EXPECT_EQ(Tools::ssplit("foo"), std::vector<std::string> { "foo" });
	{
		auto actual = Tools::ssplit("foo       bar         baz    ");
		auto expected = std::vector<std::string> { "foo", "bar", "baz" };
		EXPECT_EQ(actual, expected);
	}

	{
		auto actual = Tools::ssplit("foobo  oho  barbo  bo  bazbo", 'o');
		auto expected = std::vector<std::string> { "f", "b", "  ", "h", "  barb", "  b", "  bazb" };
		EXPECT_EQ(actual, expected);
	}
}

// =============================================================================
// Safe Numeric Conversion Tests
// =============================================================================

TEST(safe_conversions, stoi_safe_valid_input) {
	EXPECT_EQ(Tools::stoi_safe("0"), 0);
	EXPECT_EQ(Tools::stoi_safe("42"), 42);
	EXPECT_EQ(Tools::stoi_safe("-123"), -123);
	EXPECT_EQ(Tools::stoi_safe("2147483647"), std::numeric_limits<int>::max());
	EXPECT_EQ(Tools::stoi_safe("-2147483648"), std::numeric_limits<int>::min());
}

TEST(safe_conversions, stoi_safe_invalid_input) {
	EXPECT_EQ(Tools::stoi_safe(""), 0);
	EXPECT_EQ(Tools::stoi_safe("", -1), -1);
	EXPECT_EQ(Tools::stoi_safe("not_a_number"), 0);
	EXPECT_EQ(Tools::stoi_safe("not_a_number", 999), 999);
	EXPECT_EQ(Tools::stoi_safe("12.34"), 12);  // Parses integer part
	EXPECT_EQ(Tools::stoi_safe("abc123"), 0);
	EXPECT_EQ(Tools::stoi_safe("   42"), 0);   // Leading whitespace not handled
}

TEST(safe_conversions, stoi_safe_overflow) {
	// Values beyond int range should return fallback
	EXPECT_EQ(Tools::stoi_safe("9999999999999999999", -1), -1);
	EXPECT_EQ(Tools::stoi_safe("-9999999999999999999", -1), -1);
}

TEST(safe_conversions, stol_safe_valid_input) {
	EXPECT_EQ(Tools::stol_safe("0"), 0L);
	EXPECT_EQ(Tools::stol_safe("123456789"), 123456789L);
	EXPECT_EQ(Tools::stol_safe("-987654321"), -987654321L);
}

TEST(safe_conversions, stol_safe_invalid_input) {
	EXPECT_EQ(Tools::stol_safe(""), 0L);
	EXPECT_EQ(Tools::stol_safe("", -1L), -1L);
	EXPECT_EQ(Tools::stol_safe("invalid"), 0L);
}

TEST(safe_conversions, stoll_safe_valid_input) {
	EXPECT_EQ(Tools::stoll_safe("0"), 0LL);
	EXPECT_EQ(Tools::stoll_safe("9223372036854775807"), std::numeric_limits<long long>::max());
	EXPECT_EQ(Tools::stoll_safe("-9223372036854775808"), std::numeric_limits<long long>::min());
}

TEST(safe_conversions, stoll_safe_invalid_input) {
	EXPECT_EQ(Tools::stoll_safe(""), 0LL);
	EXPECT_EQ(Tools::stoll_safe("", -1LL), -1LL);
	EXPECT_EQ(Tools::stoll_safe("garbage"), 0LL);
}

TEST(safe_conversions, stoull_safe_valid_input) {
	EXPECT_EQ(Tools::stoull_safe("0"), 0ULL);
	EXPECT_EQ(Tools::stoull_safe("18446744073709551615"), std::numeric_limits<unsigned long long>::max());
	EXPECT_EQ(Tools::stoull_safe("12345678901234567890"), 12345678901234567890ULL);
}

TEST(safe_conversions, stoull_safe_invalid_input) {
	EXPECT_EQ(Tools::stoull_safe(""), 0ULL);
	EXPECT_EQ(Tools::stoull_safe("", 100ULL), 100ULL);
	EXPECT_EQ(Tools::stoull_safe("-1"), 0ULL);  // Negative values invalid for unsigned
	EXPECT_EQ(Tools::stoull_safe("abc"), 0ULL);
}

TEST(safe_conversions, stod_safe_valid_input) {
	EXPECT_DOUBLE_EQ(Tools::stod_safe("0.0"), 0.0);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("3.14159"), 3.14159);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("-2.71828"), -2.71828);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("1e10"), 1e10);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("1.5e-5"), 1.5e-5);
}

TEST(safe_conversions, stod_safe_invalid_input) {
	EXPECT_DOUBLE_EQ(Tools::stod_safe(""), 0.0);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("", -1.0), -1.0);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("not_a_double"), 0.0);
	EXPECT_DOUBLE_EQ(Tools::stod_safe("not_a_double", 99.9), 99.9);
}

// =============================================================================
// Safe Arithmetic Operation Tests
// =============================================================================

TEST(safe_operations, safe_div_normal) {
	EXPECT_EQ(Tools::safe_div(10, 2), 5);
	EXPECT_EQ(Tools::safe_div(100, 3), 33);
	EXPECT_EQ(Tools::safe_div(-15, 3), -5);
	EXPECT_EQ(Tools::safe_div(0, 5), 0);
}

TEST(safe_operations, safe_div_by_zero) {
	EXPECT_EQ(Tools::safe_div(10, 0), 0);
	EXPECT_EQ(Tools::safe_div(10, 0, -1), -1);
	EXPECT_EQ(Tools::safe_div(0, 0), 0);
	EXPECT_EQ(Tools::safe_div(100, 0, 999), 999);
}

TEST(safe_operations, safe_div_types) {
	// Test with different numeric types
	EXPECT_EQ(Tools::safe_div(10ULL, 3ULL), 3ULL);
	EXPECT_EQ(Tools::safe_div(10L, 0L, -1L), -1L);
	EXPECT_DOUBLE_EQ(Tools::safe_div(10.0, 4.0), 2.5);
	EXPECT_DOUBLE_EQ(Tools::safe_div(10.0, 0.0, -1.0), -1.0);
}

TEST(safe_operations, safe_mod_normal) {
	EXPECT_EQ(Tools::safe_mod(10, 3), 1);
	EXPECT_EQ(Tools::safe_mod(15, 5), 0);
	EXPECT_EQ(Tools::safe_mod(7, 10), 7);
}

TEST(safe_operations, safe_mod_by_zero) {
	EXPECT_EQ(Tools::safe_mod(10, 0), 0);
	EXPECT_EQ(Tools::safe_mod(10, 0, -1), -1);
	EXPECT_EQ(Tools::safe_mod(0, 0), 0);
}

TEST(safe_operations, safe_at_valid_index) {
	std::vector<int> vec = {10, 20, 30, 40, 50};
	EXPECT_EQ(Tools::safe_at(vec, 0, -1), 10);
	EXPECT_EQ(Tools::safe_at(vec, 2, -1), 30);
	EXPECT_EQ(Tools::safe_at(vec, 4, -1), 50);
}

TEST(safe_operations, safe_at_invalid_index) {
	std::vector<int> vec = {10, 20, 30};
	EXPECT_EQ(Tools::safe_at(vec, 3, -1), -1);
	EXPECT_EQ(Tools::safe_at(vec, 100, 999), 999);

	std::vector<int> empty_vec;
	EXPECT_EQ(Tools::safe_at(empty_vec, 0, -1), -1);
}

TEST(safe_operations, safe_at_string_vector) {
	std::vector<std::string> vec = {"hello", "world"};
	static const std::string fallback = "default";
	EXPECT_EQ(Tools::safe_at(vec, 0, fallback), "hello");
	EXPECT_EQ(Tools::safe_at(vec, 1, fallback), "world");
	EXPECT_EQ(Tools::safe_at(vec, 5, fallback), "default");
}

TEST(safe_operations, safe_sub_normal) {
	EXPECT_EQ(Tools::safe_sub(10U, 3U), 7U);
	EXPECT_EQ(Tools::safe_sub(100ULL, 50ULL), 50ULL);
	EXPECT_EQ(Tools::safe_sub(5U, 5U), 0U);
}

TEST(safe_operations, safe_sub_underflow_prevention) {
	EXPECT_EQ(Tools::safe_sub(3U, 10U), 0U);
	EXPECT_EQ(Tools::safe_sub(0U, 1U), 0U);
	EXPECT_EQ(Tools::safe_sub(0ULL, 100ULL), 0ULL);
}

TEST(safe_operations, safe_sub_uint64) {
	uint64_t a = 1000000000000ULL;
	uint64_t b = 999999999999ULL;
	EXPECT_EQ(Tools::safe_sub(a, b), 1ULL);
	EXPECT_EQ(Tools::safe_sub(b, a), 0ULL);
}

// =============================================================================
// Edge Cases and Boundary Tests
// =============================================================================

TEST(edge_cases, numeric_boundaries) {
	// Test parsing at numeric boundaries
	EXPECT_EQ(Tools::stoi_safe("2147483647"), 2147483647);   // INT_MAX
	EXPECT_EQ(Tools::stoi_safe("-2147483648"), -2147483648); // INT_MIN

	// Test with strings containing valid prefix
	EXPECT_EQ(Tools::stoi_safe("42abc"), 42);
	EXPECT_EQ(Tools::stoi_safe("100.5"), 100);
}

TEST(edge_cases, whitespace_handling) {
	// Leading/trailing whitespace - from_chars doesn't skip whitespace
	EXPECT_EQ(Tools::stoi_safe("  42"), 0);  // Leading space = invalid
	EXPECT_EQ(Tools::stoi_safe("42  "), 42); // Trailing space = valid (stops at non-digit)
	EXPECT_EQ(Tools::stoi_safe("\t123"), 0); // Tab = invalid
}

TEST(edge_cases, special_strings) {
	// Various edge case strings
	EXPECT_EQ(Tools::stoi_safe("+42"), 42);   // Explicit positive sign
	EXPECT_EQ(Tools::stoi_safe("--42"), 0);   // Double negative = invalid
	EXPECT_EQ(Tools::stoi_safe("0x10"), 0);   // Hex prefix = invalid for decimal parsing
	EXPECT_EQ(Tools::stoi_safe("0"), 0);
	EXPECT_EQ(Tools::stoi_safe("-0"), 0);
}

TEST(edge_cases, real_world_parsing) {
	// Simulate parsing from /proc files
	EXPECT_EQ(Tools::stoi_safe("cpu3".substr(3)), 3);
	EXPECT_EQ(Tools::stoll_safe("123456 kB".substr(0, 6)), 123456LL);

	// GPU P-state parsing simulation
	std::string pstate = "P1";
	int idx = Tools::stoi_safe(pstate.substr(1), -1) - 1;
	EXPECT_EQ(idx, 0);  // P1 -> index 0

	pstate = "P3";
	idx = Tools::stoi_safe(pstate.substr(1), -1) - 1;
	EXPECT_EQ(idx, 2);  // P3 -> index 2
}
