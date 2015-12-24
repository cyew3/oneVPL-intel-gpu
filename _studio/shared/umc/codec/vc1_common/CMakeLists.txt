mfx_include_dirs( )

include_directories (${CMAKE_SOURCE_DIR}/shared/umc/codec/vc1_common/include)

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs}) 
make_library( vc1_common none static )
set( defs "" )
