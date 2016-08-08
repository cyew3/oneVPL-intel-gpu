set( sources "" )
mfx_include_dirs( )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

file( GLOB_RECURSE sources "src/*.c" "src/*.cpp" )

set( defs "${WARNING_FLAGS}" )
make_library( media_buffers none static )
set( defs "" )
