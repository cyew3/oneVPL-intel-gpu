mfx_include_dirs( )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})
make_library( color_space_converter sw static )
make_library( color_space_converter hw static )
set( defs "" )
