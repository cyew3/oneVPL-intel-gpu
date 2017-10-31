mfx_include_dirs( )

list( APPEND cdirs
  vc1_common color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../${dir}/include )
endforeach( )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 

make_library( vc1_dec hw static )
make_library( vc1_dec sw static )

set( defs "" )
