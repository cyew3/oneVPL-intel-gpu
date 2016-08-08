mfx_include_dirs( )

list( APPEND cdirs
  brc color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../${dir}/include )
endforeach( )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${WARNING_FLAGS}" )
make_library( h264_enc sw static )
#make_library( h264_enc none static )
set( defs "" )
