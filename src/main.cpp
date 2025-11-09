// SPDX-License-Identifier: Apache-2.0

#include "btop.hpp"

#include <iterator>
#include <ranges>
#include <string_view>
#include <vector>

auto main(int argc, const char* argv[]) -> int {
	return btop_main(std::views::counted(std::next(argv), argc - 1) | std::ranges::to<std::vector<std::string_view>>());
}
