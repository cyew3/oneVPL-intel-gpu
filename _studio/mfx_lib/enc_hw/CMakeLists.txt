set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 mpeg2
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/enc_hw/${dir}/include )
endforeach()

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources.plus ${srcs})
endforeach()

make_library( enc hw static )

