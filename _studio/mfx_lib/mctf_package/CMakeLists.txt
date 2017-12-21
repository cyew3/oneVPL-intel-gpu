set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )
add_definitions( -DMFX_VA)
mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include )

set( sources "" )
set( sources.plus "" )

file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/mctf/src/*.cpp"
                        "${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/src/*.cpp"
 )
list( APPEND sources ${srcs})
list (REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/src/genx_mc.cpp")
list (REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/src/genx_me.cpp")
list (REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/src/genx_sd.cpp")

set( LIBS "" )
list( APPEND LIBS
      asc
)
set( defs "${defs} ${WARNING_FLAGS}")
make_library( mctf  none static )
set( defs "" )
