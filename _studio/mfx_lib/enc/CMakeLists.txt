set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 mpeg2 vc1
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/enc/${dir}/include )
endforeach()
include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/mpeg2_enc/include )

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( enc none static )

