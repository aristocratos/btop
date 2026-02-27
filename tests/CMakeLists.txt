# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
# TODO: Remove dependency here in case `toml11` gets added as a non-test dependency.
FetchContent_Declare(
  toml11
  GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
  GIT_TAG b32a2fff0d27e1f7522f26a125101500ddb47156 # v4.4.0
  FIND_PACKAGE_ARGS NAMES toml11
)
FetchContent_MakeAvailable(googletest toml11)

add_library(libbtop_test)
target_include_directories(libbtop_test PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libbtop_test libbtop GTest::gtest_main toml11::toml11)

add_executable(btop_test config.cpp tools.cpp)
target_link_libraries(btop_test libbtop_test)

include(GoogleTest)
gtest_discover_tests(btop_test)
