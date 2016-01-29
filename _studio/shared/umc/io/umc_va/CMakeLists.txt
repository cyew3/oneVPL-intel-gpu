mfx_include_dirs( )

include_directories (${CMAKE_SOURCE_DIR}/shared/umc/io/umc_va/include)

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})

set( defs "-Wall -Werror" )
make_library( umc_va_elk hw static )
set( defs "" )
