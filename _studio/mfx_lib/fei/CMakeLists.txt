set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/cmrt_cross_platform/include )
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/encode_hw/h264/include )
include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/brc/include )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_la )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

set( defs "-DMFX_VA -DAS_H264LA_PLUGIN -Wall -Werror" )
make_library( h264_la none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_enc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA -Wall -Werror" )
make_library( h264_enc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
include_directories ( ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_preenc )
foreach( dir ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_preenc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA -Wall -Werror" )
make_library( h264_preenc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_pak )
foreach( dir ${CMAKE_SOURCE_DIR}/mfx_lib/fei/h264_pak )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA -Wall -Werror" )
make_library( h264_pak none static )

set( defs "" )


