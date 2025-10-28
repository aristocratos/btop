enable_language(C)

add_library(igt OBJECT
  igt_perf.c
  intel_device_info.c
  intel_gpu_top.c
  intel_name_lookup_shim.c
)

if(BTOP_LTO)
  # We have checked LTO support already and it's supported :)
  set_target_properties(igt PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Disable all warnings
target_compile_options(igt PRIVATE -w)

# Link igt into btop
target_link_libraries(libbtop $<TARGET_OBJECTS:igt>)
