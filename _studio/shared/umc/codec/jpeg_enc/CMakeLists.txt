mfx_include_dirs( )

list( APPEND cdirs
  jpeg_common
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/jpeg_enc/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( jpeg_enc none static )
set( defs "" )
