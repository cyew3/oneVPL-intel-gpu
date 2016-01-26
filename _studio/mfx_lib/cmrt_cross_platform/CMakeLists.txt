set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/cmrt_cross_platform/include )

set( defs "-Wall -Werror" )
set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})
make_library( cmrt_cross_platform hw static )
set( defs "" )
