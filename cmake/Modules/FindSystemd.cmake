# SPDX-License-Identifier: Apache-2.0
#
# Find systemd
#

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_path(Systemd_INCLUDE_DIR NAMES systemd/sd-journal.h)
  find_library(Systemd_LIBRARY NAMES systemd)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Systemd REQUIRED_VARS Systemd_LIBRARY Systemd_INCLUDE_DIR)

  if(Systemd_FOUND AND NOT TARGET Systemd::Systemd)
    add_library(Systemd::Systemd UNKNOWN IMPORTED)
    set_target_properties(Systemd::Systemd PROPERTIES
      IMPORTED_LOCATION "${Systemd_LIBRARY}"
    )
  endif()

  if(Systemd_FOUND AND NOT TARGET Systemd::Journald)
    add_library(Systemd::Journald ALIAS Systemd::Systemd)
  endif()

  mark_as_advanced(Systemd_INCLUDE_DIR Systemd_LIBRARY)
endif()

