set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 h265 mjpeg mpeg2 vc1 vp8 vp9
  )
list( APPEND cdirs
  h265_dec h264_dec mpeg2_dec vc1_dec jpeg_dec vc1_common color_space_converter jpeg_common
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/decode/${dir}/include )
endforeach()
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/vpp/include )

foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )

set( defs "-Wall -Werror" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( decode sw static )
make_library( decode hw static )
set( defs "" )
