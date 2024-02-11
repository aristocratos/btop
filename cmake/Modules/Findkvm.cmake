# SPDX-License-Identifier: Apache-2.0
#
# Find libkvm, the Kernel Data Access Library
#

if(BSD)
  find_path(kvm_INCLUDE_DIR NAMES kvm.h)
  find_library(kvm_LIBRARY NAMES kvm)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(kvm REQUIRED_VARS kvm_LIBRARY kvm_INCLUDE_DIR)

  if(kvm_FOUND AND NOT TARGET kvm::kvm)
    add_library(kvm::kvm UNKNOWN IMPORTED)
    set_target_properties(kvm::kvm PROPERTIES
      IMPORTED_LOCATION "${kvm_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${kvm_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(kvm_INCLUDE_DIR kvm_LIBRARY)
endif()

