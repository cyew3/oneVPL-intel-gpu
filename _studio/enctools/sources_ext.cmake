if(OPEN_SOURCE)
  return()
endif()

mfx_include_dirs( )

set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/aenc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc_agop_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc.cpp
)
set( sources.plus "" )

set( defs "${WARNING_FLAGS}" )

make_library(aenc none static)

target_include_directories(aenc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)