cmake_minimum_required(VERSION 3.12)
project(btop-gl VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile flags for the original btop code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++20 -Dftok=_ftok")

# Find required packages
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

# Find GLFW and GLEW
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

# Find fmt library
pkg_check_modules(FMT REQUIRED fmt)

# Include directories
include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}/../include
)

# Add compile definitions for macOS
if(APPLE)
    add_definitions(-D__APPLE__)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    # Link with Homebrew GLFW specifically
    link_directories(/opt/homebrew/lib)
endif()

# Source files for btop-gl
set(BTOP_GL_SOURCES
    src/main.cpp
    src/btop_glue.cpp
    src/btop_renderer.cpp
)

# Original btop source files (platform-specific)
if(APPLE)
    set(BTOP_PLATFORM_SOURCES
        ../src/osx/btop_collect.cpp
        ../src/osx/sensors.cpp
        ../src/osx/smc.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BTOP_PLATFORM_SOURCES
        ../src/linux/btop_collect.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(BTOP_PLATFORM_SOURCES
        ../src/freebsd/btop_collect.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD") 
    set(BTOP_PLATFORM_SOURCES
        ../src/openbsd/btop_collect.cpp
        ../src/openbsd/sysctlbyname.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(BTOP_PLATFORM_SOURCES
        ../src/netbsd/btop_collect.cpp
    )
endif()

# Common btop source files - minimal set that doesn't require config.h
set(BTOP_COMMON_SOURCES
    ../src/btop_shared.cpp
    ../src/btop_tools.cpp
    ../src/btop_config.cpp
    ../src/btop_theme.cpp
    ../src/btop_draw.cpp
    ../src/btop_input.cpp
    ../src/btop_menu.cpp
    # Excluded: ../src/btop_cli.cpp - requires autotools-generated config.h
    # Excluded: ../src/btop.cpp - contains main() function which conflicts
)

# Add executable
add_executable(btop-gl 
    ${BTOP_GL_SOURCES}
    ${BTOP_PLATFORM_SOURCES}
    ${BTOP_COMMON_SOURCES}
)

# Link libraries
target_link_libraries(btop-gl
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${FMT_LIBRARIES}
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(btop-gl
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework CoreServices"
        glfw  # Link directly with glfw library
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(btop-gl pthread)
endif()

# Add shader files to build directory
file(GLOB SHADERS "shaders/*")
file(COPY ${SHADERS} DESTINATION ${CMAKE_BINARY_DIR}/shaders)

# Install
install(TARGETS btop-gl DESTINATION bin)
install(FILES ${SHADERS} DESTINATION share/btop-gl/shaders)

# Temporarily disable screensaver build due to compilation issues
# TODO: Fix screensaver compilation issues with ScreenSaver framework
# if(APPLE)
#     set(MACOSX_BUNDLE_NAME "BtopScreensaver")
#     set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.btop.screensaver")
#     set(MACOSX_BUNDLE_ICON_FILE "btop.icns")
#     set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
#     set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
#     set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024. All rights reserved.")
# 
#     # Create screensaver bundle
#     add_executable(BtopScreensaver MACOSX_BUNDLE 
#         src/screensaver_mac.mm
#         ${BTOP_PLATFORM_SOURCES}
#         ${BTOP_COMMON_SOURCES}
#         src/btop_glue.cpp
#         src/btop_renderer.cpp
#     )
#     
#     target_link_libraries(BtopScreensaver
#         ${OPENGL_LIBRARIES}
#         ${GLFW_LIBRARIES}
#         ${GLEW_LIBRARIES}
#         "-framework Cocoa"
#         "-framework ScreenSaver"
#         "-framework CoreFoundation"
#         "-framework IOKit"
#         "-framework CoreServices"
#     )
#     
#     target_include_directories(BtopScreensaver PRIVATE
#         ${OPENGL_INCLUDE_DIR}
#         ${GLFW_INCLUDE_DIRS}
#         ${GLEW_INCLUDE_DIRS}
#         ${PROJECT_SOURCE_DIR}/include
#         ${PROJECT_SOURCE_DIR}/../src
#         ${PROJECT_SOURCE_DIR}/../include
#     )
#     
#     # Set bundle properties
#     set_target_properties(BtopScreensaver PROPERTIES
#         BUNDLE TRUE
#         MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
#         OUTPUT_NAME "Btop.saver"
#         SUFFIX ""
#     )
# endif() 