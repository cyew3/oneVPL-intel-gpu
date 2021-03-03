add_executable(simple_4_vpp_resize_vmem_int_alloc)

target_sources(simple_4_vpp_resize_vmem_int_alloc
  PRIVATE
    src/simple_vpp_vmem.cpp
)

set_target_properties(simple_4_vpp_resize_vmem_int_alloc
  PROPERTIES
    FOLDER "tutorials"
)

target_include_directories (simple_4_vpp_resize_vmem_int_alloc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(simple_4_vpp_resize_vmem_int_alloc
  PRIVATE
    tutorials_common
    Threads::Threads
)
