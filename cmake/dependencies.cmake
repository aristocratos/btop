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
