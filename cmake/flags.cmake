# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

function(add_cxx_flag_if_supported target visibility required flag)
    string(REGEX REPLACE "^-" "" _flag ${flag})
    string(REGEX REPLACE "(=|-)" "_" _flag ${_flag})
    string(TOUPPER ${_flag} _flag)
    set(_flag HAVE_${_flag}_FLAG)
    check_cxx_compiler_flag(${flag} ${_flag})
    if(${_flag})
        target_compile_options(${target} ${visibility} $<BUILD_INTERFACE:${flag}>)
    elseif(required)
        message(SEND_ERROR "Required flag ${flag} is not supported")
    endif()
endfunction()

function(add_cxx_flags_if_supported target visibility)
    set(options REQUIRED)
    cmake_parse_arguments(flags "${options}" "" "" ${ARGN})
    set(required ${flags_REQUIRED})
    set(flags ${flags_UNPARSED_ARGUMENTS})
    foreach(flag ${flags})
        add_cxx_flag_if_supported(${target} ${visibility} ${required} ${flag})
    endforeach()
endfunction()
