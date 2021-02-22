add_library(pak STATIC)
set_property(TARGET pak PROPERTY FOLDER "sw_fallback")

target_include_directories(pak
  PUBLIC
    mpeg2/include
    h264/include
    ${MSDK_UMC_ROOT}/codec/mpeg2_enc/include
  )

target_sources(pak
  PRIVATE
    mpeg2/include/mfx_mpeg2_pak.h
    mpeg2/include/mfx_mpeg2_pak_tabs.h

    h264/include/mfx_h264_pak_deblocking.h
    h264/include/mfx_h264_pak.h
    h264/include/mfx_h264_pak_pack.h
    h264/include/mfx_h264_pak_pred.h
    h264/include/mfx_h264_pak_utils.h

    mpeg2/src/mfx_mpeg2_pak.cpp
    mpeg2/src/mfx_mpeg2_pak_tabs.cpp

    h264/src/mfx_h264_pak.cpp
    h264/src/mfx_h264_pak_deblocking.cpp
    h264/src/mfx_h264_pak_pack.cpp
    h264/src/mfx_h264_pak_pred.cpp
    h264/src/mfx_h264_pak_utils.cpp
  )

target_link_libraries(pak
  PUBLIC
    umc
    mfx_static_lib
  PRIVATE
    mfx_sdl_properties
  )

include(sources_ext.cmake OPTIONAL)
