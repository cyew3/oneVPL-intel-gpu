set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )

list( APPEND cdirs
  scene_analyzer
  )
mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

#==============MCTF========================================================
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mctf_package/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources
    ${srcs}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/src/mfx_scd.cpp
)

make_library( vpp sw static )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/field_copy/src )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/scd )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/copy_kernels/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmut )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include )
#==============MCTF========================================================
 set( LIBS "" )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/include )
   list( APPEND LIBS
        mctf
       )

make_library( vpp hw static )
set( defs "" )

