# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop
#

cmake_minimum_required(VERSION 3.20)

# Disable in-source builds since they would override the Makefile
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("btop"
  VERSION 1.2.13
  DESCRIPTION "A monitor of resources"
  HOMEPAGE_URL "https://github.com/aristocratos/btop"
  LANGUAGES CXX
)

# Make custom modules available
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

# When the build type is not set we can't fortify
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
include(CMakeDependentOption)
option(BTOP_STATIC "Link btop statically" OFF)
option(BTOP_LTO "Enable LTO" ON)
option(BTOP_USE_MOLD "Use mold to link btop" OFF)
option(BTOP_PEDANTIC "Enable a bunch of additional warnings" OFF)
option(BTOP_WERROR "Compile with warnings as errors" OFF)
option(BTOP_GPU "Enable GPU support" ON)
cmake_dependent_option(BTOP_RSMI_STATIC "Link statically to ROCm SMI" OFF "BTOP_GPU" OFF)

if(BTOP_STATIC)
  # Set this before calling find_package
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(CheckIPOSupported)

check_include_file_cxx(ranges CXX_HAS_RANGES)
if(NOT CXX_HAS_RANGES)
  message(FATAL_ERROR "The compiler doesn't support <ranges>")
endif()

add_executable(btop
  src/btop.cpp
  src/btop_config.cpp
  src/btop_draw.cpp
  src/btop_input.cpp
  src/btop_menu.cpp
  src/btop_shared.cpp
  src/btop_theme.cpp
  src/btop_tools.cpp
)

# NOTE: Checks can be simplified with CMake 3.25
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_sources(btop PRIVATE
    src/osx/btop_collect.cpp
    src/osx/sensors.cpp
    src/osx/smc.cpp
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  target_sources(btop PRIVATE src/freebsd/btop_collect.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(btop PRIVATE src/linux/btop_collect.cpp)
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

# Check for and enable LTO
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported AND BTOP_LTO)
  set_target_properties(btop PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# TODO: enable more warnings in coordination with upstream
target_compile_options(btop PRIVATE
  -Wall -Wextra -Wpedantic
  -ftree-vectorize -fstack-clash-protection
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(btop PRIVATE
    -Wheader-hygiene -Wgnu -Wthread-safety
  )
endif()

if(BTOP_PEDANTIC)
  target_compile_options(btop PRIVATE
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wformat=2 -Wimplicit-fallthrough -Weffc++
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(btop PRIVATE
      -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference
      -Wuseless-cast
    )
  endif()
endif()

if(BTOP_WERROR)
  target_compile_options(btop PRIVATE -Werror)
endif()

check_cxx_compiler_flag(-fstack-protector CXX_HAS_FSTACK_PROTECTOR)
if(CXX_HAS_FSTACK_PROTECTOR)
  target_compile_options(btop PRIVATE -fstack-protector)
endif()

check_cxx_compiler_flag(-fcf-protection CXX_HAS_FCF_PROTECTION)
if(CXX_HAS_FCF_PROTECTION)
  target_compile_options(btop PRIVATE -fcf-protection)
endif()

target_compile_definitions(btop PRIVATE
  _FILE_OFFSET_BITS=64
  _GLIBCXX_ASSERTIONS _LIBCPP_ENABLE_ASSERTIONS=1
  # Only has an effect with optimizations enabled
  $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
  FMT_HEADER_ONLY
)

# Enable GPU support
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BTOP_GPU)
  target_compile_definitions(btop PRIVATE GPU_SUPPORT)

  if(BTOP_RSMI_STATIC)
    # ROCm doesn't properly add it's folders to the module path
    # if `CMAKE_MODULE_PATH` is already set
    # We could also manully append ROCm's path here
    set(_CMAKE_MODULE_PATH CMAKE_MODULE_PATH)
    unset(CMAKE_MODULE_PATH)

    # NOTE: This might be problematic in the future if other sub projects
    # depend on this or if btop starts producing libraries
    # Build a static ROCm library
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    add_subdirectory(lib/rocm_smi_lib EXCLUDE_FROM_ALL)

    add_library(ROCm INTERFACE)
    # Export ROCm's properties to a CMake target (which should've been done by ROCm :-/)
    target_compile_definitions(ROCm INTERFACE RSMI_STATIC)
    target_include_directories(ROCm INTERFACE lib/rocm_smi_lib/include)
    target_link_libraries(ROCm INTERFACE rocm_smi64)

    set(CMAKE_MODULE_PATH _CMAKE_MODULE_PATH)

    target_link_libraries(btop PRIVATE ROCm)
  endif()
endif()

target_include_directories(btop SYSTEM PRIVATE include)

# mold
if(BTOP_USE_MOLD)
  target_link_options(btop PRIVATE -fuse-ld=mold)
endif()

if(BTOP_STATIC)
  target_compile_definitions(btop PRIVATE STATIC_BUILD)
  target_link_options(btop PRIVATE -static LINKER:--fatal-warnings)
endif()

# Add libraries
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(btop PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_libraries(btop PRIVATE $<LINK_LIBRARY:FRAMEWORK,CoreFoundation)
  target_link_libraries(btop PRIVATE $<LINK_LIBRARY:FRAMEWORK,IOKit)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_package(devstat REQUIRED)
  find_package(kvm REQUIRED)
  target_link_libraries(btop PRIVATE devstat::devstat kvm::kvm)
  if(BTOP_STATIC)
    find_package(elf REQUIRED)
    target_link_libraries(btop PRIVATE elf::elf)
  endif()
endif()

install(TARGETS btop RUNTIME)
install(FILES "btop.desktop" DESTINATION "share/applications")
install(FILES "Img/icon.png" DESTINATION "share/icons/hicolor/48x48/apps" RENAME "btop.png")
install(FILES "Img/icon.svg" DESTINATION "share/icons/hicolor/scalable/apps" RENAME "btop.svg")
install(DIRECTORY "themes" DESTINATION "share/btop")

