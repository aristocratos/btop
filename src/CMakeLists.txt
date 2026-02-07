# SPDX-License-Identifier: Apache-2.0

add_library(core OBJECT)

target_compile_features(core PUBLIC cxx_std_23)

target_sources(
    core
    PRIVATE
        btop.cpp
        btop_cli.cpp
        btop_config.cpp
        btop_draw.cpp
        btop_input.cpp
        btop_log.cpp
        btop_menu.cpp
        btop_shared.cpp
        btop_theme.cpp
        btop_tools.cpp
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
            FILES
                btop.hpp
                btop_cli.hpp
                btop_config.hpp
                btop_draw.hpp
                btop_input.hpp
                btop_log.hpp
                btop_menu.hpp
                btop_shared.hpp
                btop_theme.hpp
                btop_tools.hpp
)

if(APPLE)
    add_subdirectory(osx)
elseif(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    add_subdirectory(freebsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
    add_subdirectory(netbsd)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Midnight|Open)BSD")
    add_subdirectory(openbsd)
elseif(LINUX)
    add_subdirectory(linux)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

set_target_properties(core PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${BTOP_LTO})

include(configure)
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

target_include_directories(core PUBLIC ${PROJECT_SOURCE_DIR}/include/widecharwidth)

target_compile_definitions(core PUBLIC _FILE_OFFSET_BITS=64 $<$<AND:$<BOOL:${BTOP_GPU}>,$<BOOL:${LINUX}>>:GPU_SUPPORT>)

if(CMAKE_CXX_STANDARD_LIBRARY STREQUAL "libstdc++")
    set(BTOP_STANDARD_LIBRARY "libstdc++" CACHE STRING "")
elseif(CMAKE_CXX_STANDARD_LIBRARY STREQUAL "libc++")
    set(BTOP_STANDARD_LIBRARY "libc++" CACHE STRING "")
endif()
if(BTOP_STANDARD_LIBRARY STREQUAL "libstdc++")
    target_compile_definitions(core PUBLIC _GLIBCXX_ASSERTIONS)
elseif(BTOP_STANDARD_LIBRARY STREQUAL "libc++")
    target_compile_definitions(core PUBLIC _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
else()
    target_compile_definitions(core PUBLIC _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
endif()

include(flags)

add_cxx_flags_if_supported(
    core
    PUBLIC
    REQUIRED
    -Wall
    -Wextra
    -Wpedantic
    # -Wshadow
    -Wnon-virtual-dtor
)
add_cxx_flags_if_supported(
    core
    PUBLIC
    # -Wold-style-cast
    -Wcast-align
    -Wmisleading-indentation
    -Wnull-dereference
    -Wuseless-cast
    # -Weffc++
    -Wheader-hygiene
    -fcf-protection
    -fstack-clash-protection
    -fstack-protector-strong
)

target_link_libraries(
    core
    PUBLIC
        Threads::Threads
        ${CMAKE_DL_LIBS}
        fmt::fmt-header-only
        tomlplusplus::tomlplusplus
        reflectcpp::reflectcpp
        $<$<BOOL:${APPLE}>:$<LINK_LIBRARY:FRAMEWORK,CoreFoundation>>
        $<$<BOOL:${APPLE}>:$<LINK_LIBRARY:FRAMEWORK,IOKit>>
        $<$<BOOL:${BSD}>:kvm::kvm>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},FreeBSD>:devstat::devstat>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},NetBSD>:proplib::proplib>
        $<$<AND:$<BOOL:${BTOP_GPU}>,$<BOOL:${LINUX}>>:$<TARGET_OBJECTS:gpu>>
)

add_executable(btop)
target_sources(btop PRIVATE main.cpp)

set_target_properties(
    btop
    PROPERTIES
        # gersemi: hint
        # command_line: pairs
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        INTERPROCEDURAL_OPTIMIZATION ${BTOP_LTO}
)

if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    string(REGEX MATCH "^[0-9]+" GCC_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
    set(GCC_RUNTIME_DIR "/usr/local/lib/gcc${GCC_MAJOR_VERSION}")
    set_target_properties(btop PROPERTIES BUILD_RPATH ${GCC_RUNTIME_DIR})
endif()

target_link_libraries(btop PUBLIC core)

install(TARGETS btop)
