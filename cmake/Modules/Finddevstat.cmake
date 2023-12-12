# SPDX-License-Identifier: Apache-2.0
#
# Find devstat, the Device Statistics Library
#

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_path(devstat_INCLUDE_DIR NAMES devstat.h)
  find_library(devstat_LIBRARY NAMES devstat)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(devstat REQUIRED_VARS devstat_LIBRARY devstat_INCLUDE_DIR)

  if(devstat_FOUND AND NOT TARGET devstat::devstat)
    add_library(devstat::devstat UNKNOWN IMPORTED)
    set_target_properties(devstat::devstat PROPERTIES
      IMPORTED_LOCATION "${devstat_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${devstat_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(devstat_INCLUDE_DIR devstat_LIBRARY)
endif()

