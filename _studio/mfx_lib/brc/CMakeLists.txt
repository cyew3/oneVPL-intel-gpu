set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

list( APPEND vdirs
  h264 mpeg2 vc1
  )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/brc/include)
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/brc/${dir}/include )
endforeach()

set( defs "${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( brc none static )
set( defs "" )
