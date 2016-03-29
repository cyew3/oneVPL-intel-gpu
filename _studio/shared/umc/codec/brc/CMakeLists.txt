mfx_include_dirs( )

list( APPEND cdirs
  h264_enc
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/brc/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${WARNING_FLAGS}" )
make_library( bitrate_control none static )
set( defs "" )
