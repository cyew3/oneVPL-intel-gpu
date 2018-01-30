set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/h264/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h264_common )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h264_la )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

set( defs "-DMFX_VA -DAS_H264LA_PLUGIN ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( h264_la none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h264_common )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( h264_common none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h264_enc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( h264_enc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/h264_preenc )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h264_preenc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( h264_preenc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h264_pak )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h264_pak )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA ${API_FLAGS} ${WARNING_FLAGS}" )
make_library( h264_pak none static )

set( defs "" )


