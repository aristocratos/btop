// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include "btop_tools.hpp"
#include "btop_config.hpp"

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

struct HumanizerCase {
	bool base10_sizes;
	std::string base10_bitrate;
	uint64_t value;
	bool shorten;
	size_t start;
	bool bit;
	bool per_second;
	std::string expected;
};

class FloatingHumanizerTest : public testing::TestWithParam<HumanizerCase> {
protected:
	bool old_base10_sizes;
	std::string old_base10_bitrate;

	void SetUp() override {
		old_base10_sizes = Config::getB("base_10_sizes");
		old_base10_bitrate = Config::getS("base_10_bitrate");
	}

	void TearDown() override {
		Config::set("base_10_sizes", old_base10_sizes);
		Config::set("base_10_bitrate", old_base10_bitrate);
	}
};

TEST_P(FloatingHumanizerTest, FormatsValue) {
	const auto& test = GetParam();
	Config::set("base_10_sizes", test.base10_sizes);
	Config::set("base_10_bitrate", test.base10_bitrate);

	const auto actual = Tools::floating_humanizer(
		test.value, test.shorten, test.start, test.bit, test.per_second);
	std::cout << "floating_humanizer(value=" << test.value
				<< ", shorten=" << std::boolalpha << test.shorten
				<< ", start=" << test.start
				<< ", bit=" << test.bit
				<< ", per_second=" << test.per_second
				<< ") -> \"" << actual << '"' << std::endl;

	EXPECT_EQ(actual, test.expected);
}

INSTANTIATE_TEST_SUITE_P(
	Branches,
	FloatingHumanizerTest,
	testing::Values(
		HumanizerCase{false,  "Auto",     0, false, 0, false, false, "0.00 Byte"},
		HumanizerCase{false,  "Auto",  1024, false, 0, false, false, "1.00 KiB"},
		HumanizerCase{true ,  "Auto",  1000, false, 0, false, false, "1.00 kB"},
		HumanizerCase{false,  "Auto",     1,  true, 0, false, false, "1.0B"},
		HumanizerCase{false,  "Auto", 10240,  true, 0, false, false, "10K"},
		HumanizerCase{false,  "Auto",     0, false, 2, false, false, "0.00 MiB"},
		HumanizerCase{false,  "True",   125, false, 0,  true,  true, "1.00 kbps"},
		HumanizerCase{true , "False",   128, false, 0,  true,  true, "1.00 Kibps"},
		HumanizerCase{false,  "Auto",  1024, false, 0, false,  true, "1.00 KiB/s"})
);

struct HumanizerRoundingPoint {
	uint64_t hundredths;
	const string full;
	const string shortened;
};

const std::array humanizer_rounding_points{
	HumanizerRoundingPoint{  104,   "1.04",   "1.0"},
	HumanizerRoundingPoint{  106,   "1.06",   "1.1"},
	HumanizerRoundingPoint{  114,   "1.14",   "1.1"},
	HumanizerRoundingPoint{  116,   "1.16",   "1.2"},
	HumanizerRoundingPoint{  149,   "1.49",   "1.5"},
	HumanizerRoundingPoint{  150,   "1.50",   "1.5"},
	HumanizerRoundingPoint{  994,   "9.94",   "9.9"},
	HumanizerRoundingPoint{  996,   "9.96",  "10"  },
	HumanizerRoundingPoint{  999,   "9.99",  "10"  },
	HumanizerRoundingPoint{ 1000,  "10.0" ,  "10"  },
	HumanizerRoundingPoint{ 1004,  "10.0" ,  "10"  },
	HumanizerRoundingPoint{ 1005,  "10.1" ,  "10"  },
	HumanizerRoundingPoint{ 1010,  "10.1" ,  "10"  },
	HumanizerRoundingPoint{ 1049,  "10.5" ,  "10"  },
	HumanizerRoundingPoint{ 1051,  "10.5" ,  "11"  },
	HumanizerRoundingPoint{ 1494,  "14.9" ,  "15"  },
	HumanizerRoundingPoint{ 1496,  "15.0" ,  "15"  },
	HumanizerRoundingPoint{ 1994,  "19.9" ,  "20"  },
	HumanizerRoundingPoint{ 1996,  "20.0" ,  "20"  },
	HumanizerRoundingPoint{ 9984,  "99.8" , "100"  },
	HumanizerRoundingPoint{ 9986,  "99.9" , "100"  },
	HumanizerRoundingPoint{ 9994,  "99.9" , "100"  },
	HumanizerRoundingPoint{ 9996, "100"   , "100"  },
	HumanizerRoundingPoint{49949, "499"   , "499"  },
	HumanizerRoundingPoint{49951, "500"   , "500"  },
	HumanizerRoundingPoint{99949, "999"   , "999"  },
	HumanizerRoundingPoint{99951,   "1.00",   "1.0"},
};

std::vector<HumanizerCase> make_humanizer_rounding_cases() {
	constexpr std::array<uint64_t, 5> unit_scales{
		1, 1000, 1000000, 1000000000, 1000000000000
	};
	constexpr std::array<const char*, 6> byte_units{
		"Byte", "kB", "MB", "GB", "TB", "PB"
	};
	constexpr std::array<const char*, 6> short_byte_units{
		"B", "k", "M", "G", "T", "P"
	};

	std::vector<HumanizerCase> cases;
	for (size_t magnitude = 0; magnitude < unit_scales.size(); ++magnitude) {
		for (const auto& point : humanizer_rounding_points) {
			const uint64_t nominator = point.hundredths * unit_scales[magnitude];
			const uint64_t denominator = 100;

			if (nominator % denominator != 0) continue;

			const uint64_t value = nominator / denominator;

			const bool is_lower_unit = (magnitude > 0) and (point.hundredths < 100);
			if ((magnitude >= unit_scales.size() - 1) and (point.hundredths >= 99950)) continue;
			const bool is_higher_unit = (magnitude < unit_scales.size() - 1) and (point.hundredths >= 99950);
			const size_t actual_magnitude = is_lower_unit ? magnitude - 1 : (is_higher_unit ? magnitude + 1 : magnitude);

			for (const bool shorten : {false, true}) {
				const string expected = shorten
					? point.shortened + short_byte_units[actual_magnitude]
					: point.full + " " + byte_units[actual_magnitude];

				cases.push_back(HumanizerCase{
					true,
					"Auto",
					value,
					shorten,
					0,
					false,
					false,
					expected
				});
			}
		}
	}
	return cases;
}

const std::vector<HumanizerCase> humanizer_rounding_cases = make_humanizer_rounding_cases();

INSTANTIATE_TEST_SUITE_P(
		Rounding,
		FloatingHumanizerTest,
		testing::ValuesIn(humanizer_rounding_cases)
);
