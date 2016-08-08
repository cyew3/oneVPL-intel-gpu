set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set( defs "${WARNING_FLAGS}" )

list( APPEND cdirs
  scene_analyzer
  )
mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

make_library( vpp sw static )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/field_copy/src )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/scd )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmut )	
make_library( vpp hw static )
set( defs "" )

