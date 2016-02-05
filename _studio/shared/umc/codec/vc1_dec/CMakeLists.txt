mfx_include_dirs( )

list( APPEND cdirs
  vc1_common color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/vc1_dec/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 

make_library( vc1_dec hw static )
make_library( vc1_dec sw static )
#make_library( vc1_dec_elk none static )
set( defs "" )
