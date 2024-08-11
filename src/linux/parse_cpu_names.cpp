// SPDX-License-Identifier: Apache-2.0

#include "parse_cpu_names.hpp"

#include <cstddef>
#include <optional>
#include <regex>
#include <string>

auto parse_xeon_name(const std::string& name) -> std::optional<std::string> {
  std::regex regex{R"((?:\S+\(R\) ?)+ ([a-zA-Z0-9\- ]+[^ ])? ?CPU ([a-zA-Z0-9\- ]+[^ ])? ?(?:@ \d\.\d\dGHz))"};
  std::smatch match{};
  if (std::regex_match(name, match, regex)) {
    for (std::size_t i = 1; i < match.size(); ++i) {
      if (!match[i].str().empty()) {
        return match[i].str();
      }
    }
  }
  return std::nullopt;
}
