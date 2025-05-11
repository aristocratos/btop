# SPDX-License-Identifier: Apache-2.0

option(BTOP_GPU "Enable GPU support" ON)
option(BTOP_LTO "Enable LTO" ON)
option(BTOP_STATIC "Build a static binary" OFF)

include(CMakeDependentOption)
cmake_dependent_option(BTOP_RSMI_STATIC "Link to ROCm SMI statically" OFF "BTOP_GPU" OFF)
