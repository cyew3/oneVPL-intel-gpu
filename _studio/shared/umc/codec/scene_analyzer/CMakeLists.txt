mfx_include_dirs( )

include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/scene_analyzer/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( defs "${defs} -Wall -Werror" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( scene_analyzer none static )
set( defs "" )
