# SPDX-License-Identifier: Apache-2.0

enable_language(C)

add_library(igt OBJECT
  igt_perf.c
  intel_device_info.c
  intel_gpu_top.c
  intel_name_lookup_shim.c
)

set_target_properties(igt PROPERTIES
  C_STANDARD 11
  C_EXTENSIONS ON
)

if(COMPILER_SUPPORTS_LTO)
  set_target_properties(igt PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_Release ON
    INTERPROCEDURAL_OPTIMIZATION_RelWithDebInfo ON
  )
endif()

target_compile_definitions(igt PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(igt PRIVATE
  -fno-builtin-malloc
  -fno-builtin-calloc
)
target_include_directories(igt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(btop $<TARGET_OBJECTS:igt>)
