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
	uint32_t hundredths;
	const char* full;
	const char* shortened;
	const char* lower_unit_full;
	const char* lower_unit_shortened;
};

const std::array humanizer_rounding_points{
	HumanizerRoundingPoint{5, "0.05", "0.1", "50.0", "50"},
	HumanizerRoundingPoint{10, "0.10", "0.1", "100", "100"},
	HumanizerRoundingPoint{15, "0.15", "0.1", "150", "150"},
	HumanizerRoundingPoint{45, "0.45", "0.5", "450", "450"},
	HumanizerRoundingPoint{50, "0.50", "0.5", "500", "500"},
	HumanizerRoundingPoint{55, "0.55", "0.6", "550", "550"},
	HumanizerRoundingPoint{90, "0.90", "0.9", "900", "900"},
	HumanizerRoundingPoint{95, "0.95", "0.9", "950", "950"},
	HumanizerRoundingPoint{100, "1.00", "1.0", nullptr, nullptr},
	HumanizerRoundingPoint{105, "1.05", "1.1", nullptr, nullptr},
	HumanizerRoundingPoint{110, "1.10", "1.1", nullptr, nullptr},
	HumanizerRoundingPoint{115, "1.15", "1.1", nullptr, nullptr},
	HumanizerRoundingPoint{149, "1.49", "1.5", nullptr, nullptr},
	HumanizerRoundingPoint{150, "1.50", "1.5", nullptr, nullptr},
	HumanizerRoundingPoint{990, "9.90", "9.9", nullptr, nullptr},
	HumanizerRoundingPoint{995, "9.95", "9.9", nullptr, nullptr},
	HumanizerRoundingPoint{1000, "10.0", "10", nullptr, nullptr},
	HumanizerRoundingPoint{1005, "10.1", "10", nullptr, nullptr},
	HumanizerRoundingPoint{1010, "10.1", "10", nullptr, nullptr},
	HumanizerRoundingPoint{1490, "14.9", "15", nullptr, nullptr},
	HumanizerRoundingPoint{1495, "14.9", "15", nullptr, nullptr},
	HumanizerRoundingPoint{1994, "19.9", "20", nullptr, nullptr},
	HumanizerRoundingPoint{1995, "19.9", "20", nullptr, nullptr},
	HumanizerRoundingPoint{9985, "99.8", "100", nullptr, nullptr},
	HumanizerRoundingPoint{9990, "99.9", "100", nullptr, nullptr},
	HumanizerRoundingPoint{9995, "100", "100", nullptr, nullptr},
	HumanizerRoundingPoint{10000, "100", "100", nullptr, nullptr},
	HumanizerRoundingPoint{49985, "500", "500", nullptr, nullptr},
	HumanizerRoundingPoint{49990, "500", "500", nullptr, nullptr},
	HumanizerRoundingPoint{49995, "500", "500", nullptr, nullptr},
	HumanizerRoundingPoint{50000, "500", "500", nullptr, nullptr},
	HumanizerRoundingPoint{99985, "1000", "1000", nullptr, nullptr},
	HumanizerRoundingPoint{99990, "1000", "1000", nullptr, nullptr},
	HumanizerRoundingPoint{99995, "1000", "1000", nullptr, nullptr},
	HumanizerRoundingPoint{100000, "1000", "1000", nullptr, nullptr},
};

std::vector<HumanizerCase> make_humanizer_rounding_cases() {
	constexpr std::array<uint64_t, 5> unit_scales{
		1, 1000, 1000000, 1000000000, 1000000000000
	};
	constexpr std::array<const char*, 6> byte_units{
		"Byte", "kB", "MB", "GB", "TB", "PB"
	};
	constexpr std::array<const char*, 6> bit_units{
		"bit", "kb", "Mb", "Gb", "Tb", "Pb"
	};
	constexpr std::array<const char*, 6> short_byte_units{
		"B", "k", "M", "G", "T", "P"
	};
	constexpr std::array<const char*, 6> short_bit_units{
		"b", "k", "M", "G", "T", "P"
	};

	std::vector<HumanizerCase> cases;
	for (size_t unit = 0; unit < unit_scales.size(); ++unit) {
		for (const auto& point : humanizer_rounding_points) {
			for (const bool bit : {false, true}) {
				const uint64_t numerator = static_cast<uint64_t>(point.hundredths) * unit_scales[unit];
				const uint64_t divisor = bit ? 800 : 100;
				if (numerator % divisor != 0) continue;

				const uint64_t value = numerator / divisor;
				const bool lower_unit = unit > 0 and point.hundredths < 100;
				const bool promoted = point.hundredths == 100000;
				const size_t displayed_unit = lower_unit ? unit - 1 : unit + (promoted ? 1 : 0);
				const char* full = lower_unit ? point.lower_unit_full : promoted ? "1.00" : point.full;
				const char* shortened = lower_unit ? point.lower_unit_shortened : promoted ? "1.0" : point.shortened;
				for (const bool shorten : {false, true}) {
					const string expected = shorten
						? string(bit ? short_bit_units[displayed_unit] : short_byte_units[displayed_unit])
						: string(" ") + (bit ? bit_units[displayed_unit] : byte_units[displayed_unit]);
					cases.push_back(HumanizerCase{
						true,
						"Auto",
						value,
						shorten,
						0,
						bit,
						false,
						string(shorten ? shortened : full) + expected
					});
				}
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
