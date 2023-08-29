# SPDX-License-Identifier: Apache-2.0
#
# Find libelf, the ELF Access Library
#

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_path(elf_INCLUDE_DIR NAMES libelf.h)
  find_library(elf_LIBRARY NAMES elf)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(elf REQUIRED_VARS elf_LIBRARY elf_INCLUDE_DIR)

  if(elf_FOUND AND NOT TARGET elf::elf)
    add_library(elf::elf UNKNOWN IMPORTED)
    set_target_properties(elf::elf PROPERTIES
      IMPORTED_LOCATION "${elf_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${elf_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(elf_INCLUDE_DIR elf_LIBRARY)
endif()

