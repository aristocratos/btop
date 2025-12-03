#include <span>
#include <string_view>

[[nodiscard]] auto btop_main(std::span<const std::string_view> args) -> int;

//* Check if terminal supports truecolor based on environment variables
auto supports_truecolor() -> bool;
