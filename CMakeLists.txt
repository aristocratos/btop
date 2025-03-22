# SPDX-License-Identifier: Apache-2.0
#
# CMake configuration for btop
#

cmake_minimum_required(VERSION 3.25)

# Disable in-source builds since they would override the Makefile
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

project("btop"
  DESCRIPTION "A monitor of resources"
  HOMEPAGE_URL "https://github.com/aristocratos/btop"
  LANGUAGES CXX C
)

include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(CheckIPOSupported)
include(CMakeDependentOption)

# Make our Find<Package>.cmake files available
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

option(BTOP_STATIC "Link btop statically" OFF)
option(BTOP_USE_MOLD "Use mold to link btop" OFF)
option(BTOP_PEDANTIC "Enable a bunch of additional warnings" OFF)
option(BTOP_WERROR "Compile with warnings as errors" OFF)
option(BTOP_GPU "Enable GPU support" ON)
cmake_dependent_option(BTOP_RSMI_STATIC "Link statically to ROCm SMI" OFF "BTOP_GPU" OFF)

# Enable LTO in release builds by default
if(CMAKE_BUILD_TYPE MATCHES "Rel(ease|WithDebInfo)")
  option(BTOP_LTO "Enable LTO" ON)
else()
  option(BTOP_LTO "Enable LTO" OFF)
endif()

if(BTOP_STATIC AND NOT APPLE)
  # Set this before calling find_package
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

add_executable(btop
  src/btop.cpp
  src/btop_cli.cpp
  src/btop_config.cpp
  src/btop_draw.cpp
  src/btop_input.cpp
  src/btop_menu.cpp
  src/btop_shared.cpp
  src/btop_theme.cpp
  src/btop_tools.cpp
)

if(APPLE)
  target_sources(btop PRIVATE src/osx/btop_collect.cpp src/osx/sensors.cpp src/osx/smc.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
  target_sources(btop PRIVATE src/freebsd/btop_collect.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  target_sources(btop PRIVATE src/openbsd/btop_collect.cpp src/openbsd/sysctlbyname.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  target_sources(btop PRIVATE src/netbsd/btop_collect.cpp)
elseif(LINUX)
  target_sources(btop PRIVATE src/linux/btop_collect.cpp)
  if(BTOP_GPU)
    target_sources(btop PRIVATE
      src/linux/intel_gpu_top/intel_gpu_top.c
      src/linux/intel_gpu_top/igt_perf.c
      src/linux/intel_gpu_top/intel_device_info.c
      src/linux/intel_gpu_top/intel_name_lookup_shim.c
    )
  endif()
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

check_include_file_cxx(ranges CXX_HAVE_RANGES)
if(NOT CXX_HAVE_RANGES)
  message(FATAL_ERROR "The compiler doesn't support <ranges>")
endif()

# Generate build info
execute_process(
  COMMAND "git" "rev-parse" "--short" "HEAD"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
set(CONFIGURE_COMMAND
    "cmake -DBTOP_STATIC=${BTOP_STATIC} -DBTOP_USE_MOLD=${BTOP_USE_MOLD} -DBTOP_GPU=${BTOP_GPU}"
)
get_filename_component(CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
set(COMPILER "${CXX_COMPILER_BASENAME}")
set(COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY IMMEDIATE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Check for and enable LTO
if(BTOP_LTO)
  check_ipo_supported(RESULT ipo_supported)
  if(ipo_supported)
    set_target_properties(btop PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()

target_compile_options(btop PRIVATE -Wall -Wextra -Wpedantic -ftree-vectorize)

if(BTOP_PEDANTIC)
  target_compile_options(btop PRIVATE
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
    -Wconversion -Wsign-conversion -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Weffc++
    $<$<CXX_COMPILER_ID:Clang>:-Wheader-hygiene -Wgnu -Wthread-safety>
    $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op>
    $<$<CXX_COMPILER_ID:GNU>:-Wnull-dereference -Wuseless-cast>
  )
endif()
if(BTOP_WERROR)
  target_compile_options(btop PRIVATE -Werror)
endif()

if(NOT APPLE)
  target_compile_options(btop PRIVATE -fstack-clash-protection)
endif()
check_cxx_compiler_flag(-fstack-protector HAS_FSTACK_PROTECTOR)
if(HAS_FSTACK_PROTECTOR)
  target_compile_options(btop PRIVATE -fstack-protector)
endif()
check_cxx_compiler_flag(-fcf-protection HAS_FCF_PROTECTION)
if(HAS_FCF_PROTECTION)
  target_compile_options(btop PRIVATE -fcf-protection)
endif()

check_cxx_source_compiles(
  "
  #include <algorithm>

  #if !defined(__clang__) || !defined(_LIBCPP_VERSION)
  # error \"This is not clang with libcxx by llvm\"
  #endif

  int main() {
    return 0;
  }
  "
  IS_CLANG_LIBCPP
  FAIL_REGEX "This is not clang with libcxx by llvm"
)

target_compile_definitions(btop PRIVATE
  FMT_HEADER_ONLY
  _FILE_OFFSET_BITS=64
  # LLVM's clang in combination with libc++
  $<$<AND:$<CONFIG:Debug>,$<BOOL:${IS_CLANG_LIBCPP}>>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
  # LLVM's clang or gcc in combination with libstdc++ (GNU)
  $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:${IS_CLANG_LIBCPP}>>>:_GLIBCXX_ASSERTIONS>
)

target_include_directories(btop SYSTEM PRIVATE include)

# Enable pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(btop Threads::Threads)

# Enable GPU support
if(LINUX AND BTOP_GPU)
  target_compile_definitions(btop PRIVATE GPU_SUPPORT)

  if(BTOP_RSMI_STATIC)
    # ROCm doesn't properly add it's folders to the module path if `CMAKE_MODULE_PATH` is already
    # set
    # We could also manually append ROCm's path here
    set(_CMAKE_MODULE_PATH CMAKE_MODULE_PATH)
    unset(CMAKE_MODULE_PATH)

    # NOTE: This might be problematic in the future if other sub projects depend on this or if
    # btop starts producing libraries
    # Build a static ROCm library
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    add_subdirectory(lib/rocm_smi_lib EXCLUDE_FROM_ALL)

    add_library(ROCm INTERFACE)
    # Export ROCm's properties to a target
    target_compile_definitions(ROCm INTERFACE RSMI_STATIC)
    target_include_directories(ROCm INTERFACE lib/rocm_smi_lib/include)
    target_link_libraries(ROCm INTERFACE rocm_smi64)

    set(CMAKE_MODULE_PATH _CMAKE_MODULE_PATH)

    target_link_libraries(btop ROCm)
  endif()
  if(NOT BTOP_STATIC)
    target_link_libraries(btop ${CMAKE_DL_LIBS})
  endif()
endif()

if(BTOP_USE_MOLD)
  target_link_options(btop PRIVATE -fuse-ld=mold)
endif()

if(BTOP_STATIC)
  target_compile_definitions(btop PRIVATE STATIC_BUILD)
  target_link_options(btop PRIVATE -static LINKER:--fatal-warnings)
endif()

# Other platform dependent flags
if(APPLE)
  target_link_libraries(btop
    $<LINK_LIBRARY:FRAMEWORK,CoreFoundation> $<LINK_LIBRARY:FRAMEWORK,IOKit>
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "MidnightBSD")
  # Avoid version mismatch for libstdc++ when a specific version of GCC is installed and not the
  # default one since all use the default ones RPATH
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(REGEX MATCH "^[0-9]+" GCC_VERSION_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
    set_target_properties(btop PROPERTIES
      INSTALL_RPATH "/usr/local/lib/gcc${GCC_VERSION_MAJOR}"
      BUILD_WITH_INSTALL_RPATH TRUE
    )

    # The gcc compiler wrapper doesn't add '--eh-frame-hdr' on FreeBSD with static builds
    # https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=278551
    if(BTOP_STATIC)
      target_link_options(btop PRIVATE LINKER:--eh-frame-hdr)
    endif()
  endif()

  find_package(devstat REQUIRED)
  find_package(kvm REQUIRED)
  target_link_libraries(btop devstat::devstat kvm::kvm)
  if(BTOP_STATIC)
    find_package(elf REQUIRED)
    target_link_libraries(btop elf::elf)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(btop PRIVATE -static-libstdc++)
  endif()
  find_package(kvm REQUIRED)
  target_link_libraries(btop kvm::kvm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(btop PRIVATE -static-libstdc++)
  endif()
  find_package(kvm REQUIRED)
  find_package(proplib REQUIRED)
  target_link_libraries(btop kvm::kvm proplib::proplib)
endif()


# Check if lowdown is installed
find_program(LOWDOWN_EXECUTABLE lowdown)

if(LOWDOWN_EXECUTABLE)
  # Custom target to compile Markdown to man page using lowdown
  add_custom_command(
    OUTPUT btop.1
    COMMAND lowdown -s -T man -o btop.1 ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
    VERBATIM
  )
  add_custom_target(generate_manpage ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/btop.1)
  # Install the man page
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/btop.1" DESTINATION "share/man/man1")
else()
  message(STATUS "Command 'lowdown' not found: skipping generating man page btop.1")
endif()

install(TARGETS btop RUNTIME)
install(FILES "btop.desktop" DESTINATION "share/applications")
install(FILES "Img/icon.png" DESTINATION "share/icons/hicolor/48x48/apps" RENAME "btop.png")
install(FILES "Img/icon.svg" DESTINATION "share/icons/hicolor/scalable/apps" RENAME "btop.svg")
install(DIRECTORY "themes" DESTINATION "share/btop")
