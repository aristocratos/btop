# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

add_library(libbtop_test INTERFACE)
target_include_directories(libbtop_test INTERFACE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(libbtop_test INTERFACE FMT_HEADER_ONLY _FILE_OFFSET_BITS=64)
target_link_libraries(libbtop_test INTERFACE GTest::gtest_main Threads::Threads)
if(LINUX AND BTOP_GPU)
  target_compile_definitions(libbtop_test INTERFACE GPU_SUPPORT)
  target_link_libraries(libbtop_test INTERFACE igt)
endif()

if(LINUX AND BTOP_GPU)
  add_executable(btop_test tools.cpp theme.cpp $<TARGET_OBJECTS:libbtop> $<TARGET_OBJECTS:igt>)
else()
  add_executable(btop_test tools.cpp theme.cpp $<TARGET_OBJECTS:libbtop>)
endif()
target_link_libraries(btop_test libbtop_test)

# Link platform-specific libraries to test executable (same as main btop executable)
if(APPLE)
  target_link_libraries(btop_test
    $<LINK_LIBRARY:FRAMEWORK,CoreFoundation> $<LINK_LIBRARY:FRAMEWORK,IOKit>
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
  target_link_libraries(btop_test devstat::devstat kvm::kvm)
  if(BTOP_STATIC)
    target_link_libraries(btop_test elf::elf)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  target_link_libraries(btop_test kvm::kvm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  target_link_libraries(btop_test kvm::kvm proplib::proplib)
elseif(LINUX)
  target_link_libraries(btop_test dl)
endif()

include(GoogleTest)
gtest_discover_tests(btop_test)
