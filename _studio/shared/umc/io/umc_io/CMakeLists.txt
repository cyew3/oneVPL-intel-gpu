mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vc1_common/include )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vc1_common/src/umc_vc1_spl_frame_constr.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vc1_common/src/umc_vc1_spl_tbl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vc1_common/src/umc_vc1_common.cpp
)
set( defs "${WARNING_FLAGS}" )
make_library( umc_io none static )
set( defs "" )
