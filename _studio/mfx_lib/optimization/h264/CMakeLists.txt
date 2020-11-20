if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)

# =============================================================================
set ( H264PP_LIBS "" )
# =============================================================================

add_library(h264_dispatcher STATIC)

target_sources(h264_dispatcher
  PRIVATE
    include/mfx_h264_dispatcher.h
    src/mfx_h264_dispatcher.cpp
  )

target_include_directories(h264_dispatcher PUBLIC include)

target_link_libraries(h264_dispatcher
  PUBLIC
    mfx_static_lib
  PRIVATE
    ${IPPLIBS}
    h264_px
    h264_sse3
  )

# ==================================== h264_px ====================================

add_library(h264_px STATIC)

target_include_directories(h264_px PUBLIC include)

target_sources(h264_px
  PRIVATE
    src/px/mfx_h264_deblocking_px.cpp
    src/px/mfx_h264_interpolate_boundary_px.cpp
    src/px/mfx_h264_interpolate_chroma_px.cpp
    src/px/mfx_h264_reconstruct_chroma_px.cpp
  )

target_link_libraries(h264_px
  PUBLIC
    mfx_static_lib
  PRIVATE
    ${IPPLIBS}
  )

list( APPEND H264PP_LIBS h264_px )

# ==================================== h264_sse3 ====================================

add_library(h264_sse3 STATIC)

target_include_directories(h264_sse3 PUBLIC include)

target_sources(h264_sse3
  PRIVATE
    src/sse/mfx_h264_deblocking_sse.cpp
  )

target_link_libraries(h264_sse3
  PUBLIC
    mfx_static_lib
  PRIVATE
    ${IPPLIBS}
    mfx_require_ssse3_properties
  )

list( APPEND H264PP_LIBS h264_sse3 )

set_property(TARGET h264_sse3 h264_px h264_dispatcher PROPERTY FOLDER "optimization/avc")

# =============================================================================
set ( H264PP_LIBS ${H264PP_LIBS} PARENT_SCOPE )
# =============================================================================

