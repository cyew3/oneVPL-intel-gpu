mfx_include_dirs( )

list( APPEND cdirs
  color_space_converter brc
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/mpeg2_enc/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN -Wall -Werror" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( mpeg2_enc none static )
set( defs "" )
