// SPDX-License-Identifier: Apache-2.0

// TODO: Use GTest or Catch or something
#undef NDEBUG
#include <cassert>
#include <optional>
#include <string>

#include "cpu_names.hpp"
#include "linux/parse_cpu_names.hpp"

auto main() -> int {
  auto result = parse_xeon_name(xeon_E5_2623_v3);
  assert(result.value() == "E5-2623 v3");

  result = parse_xeon_name(xeon_gold_6240);
  assert(result.value() == "Gold 6240");

  result = parse_xeon_name(xeon_gold_6338N);
  assert(result.value() == "Gold 6338N");

  result = parse_xeon_name(core_i9_13900H);
  assert(result == std::nullopt);

  result = parse_xeon_name(pentium_III);
  assert(result == std::nullopt);
}
