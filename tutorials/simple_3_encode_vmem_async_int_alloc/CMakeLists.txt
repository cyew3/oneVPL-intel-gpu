add_executable(simple_3_encode_vmem_int_alloc)

target_sources(simple_3_encode_vmem_int_alloc
  PRIVATE
    src/simple_encode_vmem_async_int_alloc.cpp
)

set_target_properties(simple_3_encode_vmem_int_alloc
  PROPERTIES
    FOLDER "tutorials"
)

target_include_directories (simple_3_encode_vmem_int_alloc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(simple_3_encode_vmem_int_alloc
  PRIVATE
    tutorials_common
    Threads::Threads
)
