include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../api
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/spl_mux_dispatcher.c
)

set(defs "${WARNING_FLAGS}")
make_library( sample_spl_mux_dispatcher none static )
set(defs "")
