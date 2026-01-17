# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

add_executable(btop_test tools.cpp)
target_link_libraries(btop_test PRIVATE core GTest::gtest_main)

gtest_discover_tests(btop_test)
