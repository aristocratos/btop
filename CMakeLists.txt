# Required for platform variables
cmake_minimum_required(VERSION 3.25)

project(btop
        VERSION 1.2.13
        DESCRIPTION "Resource monitor that shows usage and stats for processor, memory, disks, network and processes."
        LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(CheckIPOSupported)

find_package(Threads REQUIRED)

# Colorful compiler output
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Detect host platform
if (BSD)
    set(PLATFORM_DIR freebsd)
elseif (LINUX)
    set(PLATFORM_DIR linux)
elseif (APPLE)
    set(PLATFORM_DIR osx)
endif ()

# Gather source files
set(BTOP_SOURCES
        src/btop.cpp
        src/btop_config.cpp
        src/btop_draw.cpp
        src/btop_input.cpp
        src/btop_menu.cpp
        src/btop_shared.cpp
        src/btop_theme.cpp
        src/btop_tools.cpp
        src/${PLATFORM_DIR}/btop_collect.cpp
        )

add_executable(btop ${BTOP_SOURCES})

# Set c++ standard and enable fPIE
set_target_properties(btop PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE YES
        )

target_compile_definitions(btop PRIVATE
        $<$<CONFIG:Release>:_FORTIFY_SOURCE=2>
        _GLIBCXX_ASSERTIONS
        )

check_cxx_compiler_flag(-fstack-clash-protection HAS_FLAG_STACK_CLASH_PROTECTION)
if (HAS_FLAG_STACK_CLASH_PROTECTION)
    set(COMPILE_FLAGS ${COMPILE_FLAGS} -fstack-clash-protection)
endif ()
check_cxx_compiler_flag(-fcf-protection HAS_FLAG_CF_PROTECTION)
if (HAS_FLAG_CF_PROTECTION)
    set(COMPILE_FLAGS ${COMPILE_FLAGS} -fcf-protection)
endif ()
check_cxx_compiler_flag(-fstack-protector HAS_FLAG_STACK_PROTECTOR)
if (HAS_FLAG_STACK_PROTECTOR)
    set(COMPILE_FLAGS ${COMPILE_FLAGS} -fstack-protector)
endif ()
# Enables -ftree-loop-vectorize with GCC which is also enabled by default with -O2
# and is known to clang as well, unlike -ftree-loop-vectorize
check_cxx_compiler_flag(-ftree-vectorize HAS_FLAG_TREE_VECTORIZE)
if (HAS_FLAG_TREE_VECTORIZE)
    set(COMPILE_FLAGS ${COMPILE_FLAGS} -ftree-vectorize)
endif ()
# Enable more warnings
# Suppress clang specific messages in robin_hood.h and widechar_width.hpp
target_compile_options(btop PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CXX_COMPILER_ID:Clang>:-Wno-deprecated-builtins -Wno-deprecated-declarations>
        ${COMPILE_FLAGS}
        )

# Make external deps known and local files to be included with <>
target_include_directories(btop PRIVATE
        include
        src
        )

target_link_libraries(btop PRIVATE Threads::Threads)

# Enable LTO
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set_target_properties(btop PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

# Make clang work
# Use libcxx with clang, see: https://github.com/llvm/llvm-project/issues/55673#issuecomment-1135973337
# Enable experimental features needed to use std::ranges::views
target_compile_options(btop PRIVATE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)
target_link_options(btop PRIVATE $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)
target_compile_definitions(btop PRIVATE $<$<CXX_COMPILER_ID:Clang>:_LIBCPP_ENABLE_EXPERIMENTAL>)
