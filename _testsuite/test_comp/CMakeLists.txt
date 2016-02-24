mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# =============================================================================

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/simple_composite/src/*.cpp" )
file( GLOB_RECURSE srcs2 "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp")
list( APPEND sources ${srcs})
list( APPEND sources ${srcs2})

set (CMAKE_CXX_FLAGS "-std=c++0x")
set( LIBS "" )
list( APPEND LIBS
    mfx
    pthread
    dl
)

set( defs "${defs} ${WARNING_FLAGS}" )
make_executable( test_comp universal )
set( defs "" )
