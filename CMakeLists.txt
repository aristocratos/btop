# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_EXTENSIONS OFF)

project(btop CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(options)

if(BTOP_GPU)
    enable_language(C)
endif()

if(BTOP_LTO)
    include(CheckIPOSupported)
    check_ipo_supported()
endif()

include(features)
include(dependencies)

add_subdirectory(src)

if(BTOP_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

find_program(lowdown_FOUND lowdown)
if(lowdown_FOUND)
    add_custom_command(
        OUTPUT btop.1
        COMMAND lowdown -T man -s -o btop.1 ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manpage.md
    )
    add_custom_target(manpage ALL DEPENDS btop.1)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/btop.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

install(FILES btop.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES Img/logo.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps RENAME btop.png)
install(FILES Img/logo.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME btop.png)
install(DIRECTORY themes DESTINATION ${CMAKE_INSTALL_DATADIR}/btop)
