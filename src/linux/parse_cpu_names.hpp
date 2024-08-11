// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>

auto parse_xeon_name(const std::string& name) -> std::optional<std::string>;
