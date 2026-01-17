# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

include(CMakeDependentOption)

cmake_dependent_option(
    BTOP_LTO
    "Enable LTO (default in release builds)"
    ON
    [[CMAKE_BUILD_TYPE MATCHES "Rel(ease|WithDebInfo)"]]
    OFF
)
if(LINUX)
    option(BTOP_GPU "Enable GPU support" ON)
endif()
option(BTOP_USE_SYSTEM_FMT "Disable automatic download of fmt" OFF)

option(BTOP_TESTS "Build tests" ON)
if(BTOP_TESTS)
    option(BTOP_USE_SYSTEM_GTEST "Disable automatic download of googletest" OFF)
endif()
