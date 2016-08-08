mfx_include_dirs( )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${defs} ${WARNING_FLAGS}" )
make_library( jpeg_common none static )
set( defs "" )
