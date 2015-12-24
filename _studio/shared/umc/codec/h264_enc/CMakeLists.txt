mfx_include_dirs( )

list( APPEND cdirs
  brc color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/h264_enc/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( h264_enc sw static ) #???????????????????
#make_library( h264_enc none static )
