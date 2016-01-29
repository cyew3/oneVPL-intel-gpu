mfx_include_dirs( )

include_directories (${CMAKE_SOURCE_DIR}/shared/umc/core/vm_plus/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "${WARNING_FLAGS}" )
make_library( vm_plus none static )
set( defs "" )
