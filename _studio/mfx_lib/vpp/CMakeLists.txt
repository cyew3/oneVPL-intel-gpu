set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set( defs "-Wall -Werror" )

list( APPEND cdirs
  scene_analyzer
  )
mfx_include_dirs( )
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/vpp/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

make_library( vpp sw static )
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/cmrt_cross_platform/include )
make_library( vpp hw static )
set( defs "" )

