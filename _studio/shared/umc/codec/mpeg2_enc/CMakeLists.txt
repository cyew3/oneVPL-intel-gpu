mfx_include_dirs( )

list( APPEND cdirs
  color_space_converter brc
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../${dir}/include )
endforeach( )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( mpeg2_enc none static )
set( defs "" )
