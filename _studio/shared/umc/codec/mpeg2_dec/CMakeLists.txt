mfx_include_dirs( )

list( APPEND cdirs
  color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../${dir}/include )
endforeach( )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
make_library( mpeg2_dec sw static )
make_library( mpeg2_dec hw static )
set( defs "" )
