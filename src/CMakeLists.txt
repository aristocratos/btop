# SPDX-License-Identifier: Apache-2.0

target_sources(btop PRIVATE
  btop.cpp
  btop_cli.cpp
  btop_config.cpp
  btop_draw.cpp
  btop_input.cpp
  btop_menu.cpp
  btop_shared.cpp
  btop_theme.cpp
  btop_tools.cpp
)

if(APPLE)
  add_subdirectory(osx)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Free|Midnight)BSD")
  add_subdirectory(freebsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  add_subdirectory(netbsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  add_subdirectory(openbsd)
elseif(LINUX)
  add_subdirectory(linux)
else()
  message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

include(BuildInfo)
configure_file(config.h.in config.h @ONLY)
target_include_directories(btop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
