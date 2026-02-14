# SPDX-License-Identifier: Apache-2.0

add_library(gpu OBJECT)

target_sources(
    gpu
    PRIVATE igt_perf.c intel_device_info.c intel_gpu_top.c intel_name_lookup_shim.c
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            FILES
                drm.h
                drm_mode.h
                i915_drm.h
                i915_pciids.h
                i915_pciids_local.h
                igt_perf.h
                intel_chipset.h
                intel_gpu_top.h
                xe_pciids.h
)

set_target_properties(gpu PROPERTIES INTERPROCEDURAL_OPTIMIZATION ${BTOP_LTO})
