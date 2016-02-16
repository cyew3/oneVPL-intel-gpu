set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

# =============================================================================
set ( H264PP_LIBS "" )
# =============================================================================

set( defs "-Wall -Werror" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  src/mfx_h264_dispatcher.cpp
)

make_library( h264_dispatcher none static )
set( defs "" )

# =============================================================================

set( defs "-Wall -Werror" )
set( sources "" )
set( sources.plus "" )

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/src/px )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( h264_px none static )
set( defs "" )
list( APPEND H264PP_LIBS h264_px )

# =============================================================================

set( defs "-Wall -Werror" )
set( sources "" )
set( sources.plus "" )

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/src/sse )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs} )
endforeach()

make_library( h264_sse3 none static )
append_property( h264_sse3 COMPILE_FLAGS "-mssse3")
append_property( h264_sse3 LINK_FLAGS "-mssse3" )
set( defs "" )
list( APPEND H264PP_LIBS h264_sse3 )

# =============================================================================
set ( H264PP_LIBS ${H264PP_LIBS} PARENT_SCOPE )
# =============================================================================

