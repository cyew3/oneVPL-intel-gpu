mfx_include_dirs( )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( scene_analyzer none static )
set( defs "" )
