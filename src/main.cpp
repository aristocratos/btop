#include "btop.hpp"

#include <ranges>
#include <string_view>
#include <vector>

auto main(int argc, const char* argv[]) -> int {
	return btop_main(std::views::counted(argv, argc) | std::ranges::to<std::vector<std::string_view>>());
}
