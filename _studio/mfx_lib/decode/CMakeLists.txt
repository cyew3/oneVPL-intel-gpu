set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 h265 mjpeg mpeg2 vc1 vp8 vp9
  )
list( APPEND cdirs
  h265_dec h264_dec mpeg2_dec vp9_dec vc1_dec jpeg_dec vc1_common color_space_converter jpeg_common
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../vpp/include )

foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( decode sw static )
make_library( decode hw static )
set( defs "" )
