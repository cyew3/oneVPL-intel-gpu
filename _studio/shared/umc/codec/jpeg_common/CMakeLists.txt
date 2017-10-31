mfx_include_dirs( )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( jpeg_common none static )
set( defs "" )
