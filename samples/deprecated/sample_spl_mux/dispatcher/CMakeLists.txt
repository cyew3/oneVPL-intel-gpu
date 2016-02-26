include_directories (
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/dispatcher/include
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/api
)

list(APPEND sources
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/dispatcher/src/spl_mux_dispatcher.c
)

set(defs "${WARNING_FLAGS}")
make_library( sample_spl_mux_dispatcher none static )
set(defs "")
