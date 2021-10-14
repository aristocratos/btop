cmake_minimum_required (VERSION 3.18)

file(STRINGS "src/btop.cpp" BTOP_VERSION_LINE REGEX "Version[ \\t\\r\\n]*=[ \\t\\r\\n]*\"[^\"]+\"")
string(REGEX REPLACE "^.*Version[ \\t\\r\\n]*=[ \\t\\r\\n]*\"([^\"]+)\".*$" "\\1" BTOP_VERSION "${BTOP_VERSION_LINE}")

message(STATUS "btop version: ${BTOP_VERSION}")

project("btop" 
	VERSION ${BTOP_VERSION} 
	DESCRIPTION "Resource monitor that shows usage and stats for processor, memory, disks, network and processes."
	HOMEPAGE_URL "https://github.com/aristocratos/btop"
	LANGUAGES CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(PLATFORM_SRCS_GLOB "src/linux/*.cpp")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	set(PLATFORM_SRCS_GLOB "src/freebsd/*.cpp")
elseif(APPLE)
	set(PLATFORM_SRCS_GLOB "src/osx/*.cpp")
else()
	message(FATAL_ERROR "Unsupported System: ${CMAKE_SYSTEM_NAME}")
endif()

file(GLOB SRCS CONFIGURE_DEPENDS "src/*.cpp" CONFIGURE_DEPENDS "${PLATFORM_SRCS_GLOB}")

add_executable(btop ${SRCS})

target_include_directories(btop PRIVATE include src)
target_compile_features(btop PUBLIC cxx_std_20)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(btop PRIVATE Threads::Threads)

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

function(target_compile_options_if_supported)
	list(SUBLIST ARGV 2 -1 COMPILE_OPTIONS)
	foreach(COMPILE_OPTION ${COMPILE_OPTIONS})
		check_cxx_compiler_flag(${COMPILE_OPTION} CXX_COMPILER_OPTION_CHECK_RESULT)
		if(CXX_COMPILER_OPTION_CHECK_RESULT)
			target_compile_options(${ARGV0} ${ARGV1} ${COMPILE_OPTION})
		endif()	
	endforeach()
endfunction()

function(target_link_options_if_supported)
	list(SUBLIST ARGV 2 -1 LINK_OPTIONS)
	foreach(LINK_OPTION ${LINK_OPTIONS})
		check_linker_flag(CXX ${LINK_OPTION} CXX_LINK_OPTION_CHECK_RESULT)
		if(CXX_LINK_OPTION_CHECK_RESULT)
			target_link_options(${ARGV0} ${ARGV1} ${LINK_OPTION})
		endif()	
	endforeach()
endfunction()

if(CMAKE_SYSTEM STREQUAL "x86_64")
	target_compile_options_if_supported(btop PRIVATE -fcf-protection -cfguard)
endif()

target_compile_options_if_supported(btop PRIVATE -Wall -Wextra -pedantic)
target_link_options_if_supported(btop PRIVATE -fexceptions -fstack-protector -fstack-clash-protection)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options_if_supported(btop PRIVATE -ftree-loop-vectorize -flto)
endif()

target_compile_definitions(btop PRIVATE _FORTIFY_SOURCE=2 _GLIBCXX_ASSERTIONS)

if(APPLE)
	target_link_options(btop PRIVATE -framework IOKit -framework CoreFoundation)
endif()

option(STATIC "statically linked executable" OFF)

if(STATIC)
	set(CMAKE_REQUIRED_LINK_OPTIONS -static -Werror -Wl,--fatal-warnings)
	check_cxx_source_compiles("#include <pwd.h>
		                       int main() { struct passwd* x = getpwuid(0); }" STATIC_GETPWUID)
	if(NOT STATIC_GETPWUID)
		target_compile_definitions(btop PRIVATE STATIC_BUILD)
	endif()
	target_link_libraries(btop PRIVATE -static)
endif()

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nobody <nobody@nowhere.no>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

include(CPack)

install(TARGETS btop RUNTIME
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES "README.md"
        DESTINATION  "share/btop/"
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
install(DIRECTORY "themes"
        DESTINATION  "share/btop/"
		DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
