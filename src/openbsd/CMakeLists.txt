# SPDX-License-Identifier: Apache-2.0

target_sources(
    core
    PRIVATE btop_collect.cpp sysctlbyname.cpp
    PUBLIC FILE_SET headers TYPE HEADERS FILES internal.h sysctlbyname.h
)
