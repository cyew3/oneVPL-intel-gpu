add_executable(simple_11_decode_vp_n_outputs)

target_sources(simple_11_decode_vp_n_outputs
  PRIVATE
    src/simple_decode_vp_n_outputs.cpp
)

set_target_properties(simple_11_decode_vp_n_outputs
  PROPERTIES
    FOLDER "tutorials"
)

target_include_directories (simple_11_decode_vp_n_outputs
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(simple_11_decode_vp_n_outputs
  PRIVATE
    tutorials_common
    Threads::Threads
)
