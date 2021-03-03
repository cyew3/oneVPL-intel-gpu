add_executable(simple_11_decode_vp_skip_original)

target_sources(simple_11_decode_vp_skip_original
  PRIVATE
    src/simple_decode_vp_skip_original.cpp
)

set_target_properties(simple_11_decode_vp_skip_original
  PROPERTIES
    FOLDER "tutorials"
)

target_include_directories (simple_11_decode_vp_skip_original
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(simple_11_decode_vp_skip_original
  PRIVATE
    tutorials_common
    Threads::Threads
)
