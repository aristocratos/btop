// SPDX-License-Identifier: Apache-2.0

#include "btop_config.hpp"

#include <gtest/gtest.h>
#include <toml.hpp>

TEST(config, current_config_serializes_to_toml) {
    ASSERT_TRUE(toml::try_parse_str(Config::current_config()).is_ok())
        << "config serializer doesn't produce valid toml";
}
