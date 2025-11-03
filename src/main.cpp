#include "btop.hpp"

#include <ranges>
#include <string_view>
#include <vector>

auto main(int argc, const char* argv[]) -> int {
	return btop_main(std::views::counted(argv + 1, argc - 1) | std::ranges::to<std::vector<std::string_view>>());
}
