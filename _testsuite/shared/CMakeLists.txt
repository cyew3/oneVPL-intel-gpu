mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cc_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_io_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_h264_dec_command_line.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_statistics.cpp
  )

set( defs "-Wall -Werror ${API_FLAGS}" )
make_library( shared_utils none static )

set( defs "-DLUCAS_DLL ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( shared_utils_lucas none static )
set( defs "" )
