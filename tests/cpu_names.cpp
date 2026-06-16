// SPDX-License-Identifier: Apache-2.0

#include "btop_shared.hpp"

#include <gtest/gtest.h>

TEST(cpu_names, amd) {
	EXPECT_EQ(Cpu::trim_name("AMD Ryzen AI 7 PRO 360 w/ Radeon 880M"), "Ryzen AI 7 PRO 360");
	EXPECT_EQ(Cpu::trim_name("AMD Ryzen 7 PRO 4750G with Radeon Graphics"), "Ryzen 7 PRO 4750G");
	EXPECT_EQ(Cpu::trim_name("AMD Ryzen Threadripper PRO 3975WX 32-Cores"), "Ryzen Threadripper PRO 3975WX");
	EXPECT_EQ(Cpu::trim_name("AMD Ryzen 7 5700X 8-Core Processor"), "Ryzen 7 5700X");

	EXPECT_EQ(Cpu::trim_name("AMD EPYC 7543 32-Core Processor"), "EPYC 7543 32-");
}

TEST(cpu_names, intel) {
	EXPECT_EQ(Cpu::trim_name("Intel(R) Pentium(R) III CPU family 1400MHz"), "family");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Pentium(R) CPU        P6200  @ 2.13GHz"), "P6200");

	EXPECT_EQ(Cpu::trim_name("Intel(R) Core(TM) i7 CPU       Q 840  @ 1.87GHz"), "Q");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Core(TM) i5-4570 CPU @ 3.20GHz"), "i5-4570");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz"), "i7-8700");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Core(TM) i5-10600 CPU @ 3.30GHz"), "i5-10600");
	EXPECT_EQ(Cpu::trim_name("12th Gen Intel(R) Core(TM) i5-12600"), "12th Gen i5-12600");
	EXPECT_EQ(Cpu::trim_name("13th Gen Intel(R) Core(TM) i5-13500"), "13th Gen i5-13500");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Core(TM) i5-14600"), "i5-14600");

	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) CPU E5-2690 v3 @ 2.60GHz"), "E5-2690");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) CPU E5-2690 v4 @ 2.60GHz"), "E5-2690");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) Silver 4410Y"), "Xeon Silver 4410Y");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) Gold 6138 CPU @ 2.00GHz"), "@");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) Gold 6240 CPU @ 2.60GHz"), "@");
	EXPECT_EQ(Cpu::trim_name("INTEL(R) XEON(R) GOLD 6548Y+"), "INTEL XEON GOLD 6548Y+");
	EXPECT_EQ(Cpu::trim_name("Intel(R) Xeon(R) Platinum 8368Q CPU @ 2.60GHz"), "@");
}
