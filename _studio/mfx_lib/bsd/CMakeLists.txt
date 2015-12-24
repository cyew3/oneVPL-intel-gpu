set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

list( APPEND vdirs
  h264 mpeg2 vc1
  )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/bsd/${dir}/include )
endforeach()

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( bsd none static )

