mfx_include_dirs( )

list( APPEND cdirs
  color_space_converter
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )
include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/mpeg2_dec/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${defs} ${WARNING_FLAGS}" )
make_library( mpeg2_dec sw static )
make_library( mpeg2_dec hw static )
set( defs "" )
