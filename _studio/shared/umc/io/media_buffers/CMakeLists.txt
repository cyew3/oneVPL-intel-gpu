set( sources "" )
mfx_include_dirs( )

include_directories (${CMAKE_SOURCE_DIR}/shared/umc/io/media_buffers/include)

file( GLOB_RECURSE sources "src/*.c" "src/*.cpp" )

set( defs "-Wall -Werror" )
make_library( media_buffers none static )
set( defs "" )
