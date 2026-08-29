# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

add_library(libbtop_test)
target_include_directories(libbtop_test PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libbtop_test libbtop GTest::gtest_main)

set(BTOP_TEST_SOURCES cpu_names.cpp tools.cpp)
if(APPLE)
	# Net::read_ifstats_pair 对齐安全回归测试（对应 osx/btop_collect.cpp 的修复）
	list(APPEND BTOP_TEST_SOURCES btop_collect.cpp)
endif()
add_executable(btop_test ${BTOP_TEST_SOURCES})
target_link_libraries(btop_test libbtop_test)

include(GoogleTest)
gtest_discover_tests(btop_test)
