# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

include(FetchContent)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(BSD)
    find_package(kvm REQUIRED)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    find_package(devstat REQUIRED)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
    find_package(proplib REQUIRED)
endif()

# TODO: We might want to have a macro for this.

if(BTOP_USE_SYSTEM_FMT)
    find_package(fmt REQUIRED)
else()
    set(FMT_OS OFF)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        # gersemi: off
        # Includes a fix for running clang-tidy.
        GIT_TAG a9ea225cadc2ad0d0620ddd458310b24d2a1b4b8 # master
        GIT_SHALLOW
        FIND_PACKAGE_ARGS
        # gersemi: on
    )
    FetchContent_MakeAvailable(fmt)
endif()

if(BTOP_USE_SYSTEM_TOMLPLUSPLUS)
    find_package(tomlplusplus REQUIRED)
else()
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        # gersemi: off
        # Includes a fix for running clang-tidy.
        GIT_TAG 30172438cee64926dc41fdd9c11fb3ba5b2ba9de # v3.4.0
        GIT_SHALLOW
        FIND_PACKAGE_ARGS
        # gersemi: on
    )
    FetchContent_MakeAvailable(tomlplusplus)
endif()

if(BTOP_USE_SYSTEM_REFLECTCPP)
    find_package(reflectcpp REQUIRED)
else()
    set(REFLECTCPP_JSON OFF)
    set(REFLECTCPP_TOML ON)
    set(REFLECTCPP_USE_STD_EXPECTED ON)
    FetchContent_Declare(
        reflect-cpp
        GIT_REPOSITORY https://github.com/getml/reflect-cpp.git
        # gersemi: off
        # Includes a fix for running clang-tidy.
        GIT_TAG 09e17b2b8578e95625fc1fc92ec863d2583e1723 # v0.23.0
        GIT_SHALLOW
        GIT_SUBMODULES ""
        FIND_PACKAGE_ARGS NAMES reflectcpp
        # gersemi: on
    )
    FetchContent_MakeAvailable(reflect-cpp)
endif()

if(BTOP_TESTS)
    if(BTOP_USE_SYSTEM_GTEST)
        find_package(GTest REQUIRED)
    else()
        set(INSTALL_GTEST OFF)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            # gersemi: off
            GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723 # v1.17.0
            GIT_SHALLOW
            FIND_PACKAGE_ARGS NAMES GTest
            # gersemi: on
        )
        FetchContent_MakeAvailable(googletest)
    endif()
endif()
