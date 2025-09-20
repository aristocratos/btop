cmake_minimum_required(VERSION 3.15)
project(btop-gl VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(glfw3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Vulkan REQUIRED)

# Platform-specific dependencies
if(APPLE)
    find_library(CORESERVICES_FRAMEWORK CoreServices)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../src
    ${Vulkan_INCLUDE_DIRS}
)

# Common btop sources we need
set(BTOP_COMMON_SOURCES
    ../src/btop_shared.cpp
    ../src/btop_tools.cpp
    ../src/btop_config.cpp
    ../src/btop_theme.cpp
    ../src/btop_draw.cpp
    ../src/btop_input.cpp
    ../src/btop_menu.cpp
    src/btop_glue.cpp
)

# Platform-specific btop sources
if(APPLE)
    list(APPEND BTOP_COMMON_SOURCES
        ../src/osx/btop_collect.cpp
        ../src/osx/sensors.cpp
        ../src/osx/smc.cpp
    )
endif()

# Vulkan version (only version!)
set(VULKAN_SOURCES
    src/main_vulkan.cpp
    src/vulkan_renderer.cpp
    nanovg.c
    ${BTOP_COMMON_SOURCES}
)

# Create the Vulkan executable 🚀
add_executable(btop-gl-vulkan ${VULKAN_SOURCES})

# Compile definitions
target_compile_definitions(btop-gl-vulkan PRIVATE
    OSX
    FMT_HEADER_ONLY
    NANOVG_VULKAN_IMPLEMENTATION
)

# Link libraries for Vulkan version
target_link_libraries(btop-gl-vulkan
    glfw
    fmt::fmt
    Vulkan::Vulkan
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(btop-gl-vulkan
        ${CORESERVICES_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
    )
endif()

# Compiler options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(btop-gl-vulkan PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-unused-variable
    )
endif()

# Add shader files to build directory
file(GLOB SHADERS shaders/*.vert shaders/*.frag)
file(COPY ${SHADERS} DESTINATION ${CMAKE_BINARY_DIR}/shaders)

# Install Vulkan executable
install(TARGETS btop-gl-vulkan DESTINATION bin)
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