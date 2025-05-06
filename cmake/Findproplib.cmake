# SPDX-License-Identifier: Apache-2.0
#
# Find proplib – property container object library
#

if(BSD)
  find_path(proplib_INCLUDE_DIR NAMES prop/proplib.h)
  find_library(proplib_LIBRARY NAMES libprop prop)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(proplib REQUIRED_VARS proplib_LIBRARY proplib_INCLUDE_DIR)

  if(proplib_FOUND AND NOT TARGET proplib::proplib)
    add_library(proplib::proplib UNKNOWN IMPORTED)
    set_target_properties(proplib::proplib PROPERTIES
      IMPORTED_LOCATION "${proplib_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${proplib_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(proplib_INCLUDE_DIR proplib_LIBRARY)
endif()

