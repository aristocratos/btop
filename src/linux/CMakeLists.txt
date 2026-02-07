# SPDX-License-Identifier: Apache-2.0

target_sources(core PRIVATE btop_collect.cpp)

if(BTOP_GPU)
    add_subdirectory(intel_gpu_top)
endif()
