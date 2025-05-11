# SPDX-License-Identifier: Apache-2.0

enable_language(C)

target_sources(btop PRIVATE
  igt_perf.c
  intel_device_info.c
  intel_gpu_top.c
  intel_name_lookup_shim.c
)
