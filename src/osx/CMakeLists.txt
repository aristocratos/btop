# SPDX-License-Identifier: Apache-2.0

target_sources(
    core
    PRIVATE btop_collect.cpp sensors.cpp smc.cpp
    PUBLIC FILE_SET headers TYPE HEADERS FILES sensors.hpp smc.hpp
)
