// SPDX-License-Identifier: Apache-2.0

#include "btop.hpp"

#include <iterator>
#include <ranges>
#include <string_view>
#include <vector>

auto main(int argc, const char* argv[]) -> int {
	auto _args = std::views::counted(std::next(argv), argc - 1); return btop_main(std::vector<std::string_view>(_args.begin(), _args.end()));
}
