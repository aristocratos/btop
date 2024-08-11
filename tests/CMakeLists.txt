# SPDX-License-Identifier: Apache-2.0

include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(test_parse_xeon_name test_parse_xeon_name.cpp ${PROJECT_SOURCE_DIR}/src/linux/parse_cpu_names.cpp)
add_test(parse_xeon_name test_parse_xeon_name)
