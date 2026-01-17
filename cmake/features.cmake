# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)

macro(check_feature_macro feature header flag symbol)
    set(CMAKE_CXX_STANDARD 23)
    check_cxx_symbol_exists("${feature}" "${header}" ${flag})
    if(NOT ${flag})
        message(SEND_ERROR "The CXX compiler doesn't support ${symbol}")
    endif()
endmacro()

macro(
    check_feature_macro_version
    feature
    version
    header
    flag
    symbol
)
    set(CMAKE_CXX_STANDARD 23)
    check_cxx_source_compiles(
        "
        #include <${header}>
        #if ${feature} < ${version}
        #error \"Feature ${feature} is not ${version}\"
        #endif
        int main() { return 0; }
        "
        ${flag}
    )
    if(NOT ${flag})
        message(SEND_ERROR "The CXX compiler doesn't support ${symbol}")
    endif()
endmacro()

check_feature_macro_version(__cpp_lib_optional 202110L version HAVE_CXX_OPTIONAL_MONADS std::optional::and_then)
check_feature_macro(__cpp_lib_expected version HAVE_CXX_EXPECTED std::expected)
check_feature_macro(__cpp_lib_ranges version HAVE_CXX_RANGES std::ranges)
check_feature_macro(__cpp_lib_ranges_to_container version HAVE_CXX_RANGES_TO_CONTAINER std::ranges::to)
check_feature_macro(__cpp_lib_string_contains version HAVE_CXX_STRING_CONTAINS std::string::contains)
