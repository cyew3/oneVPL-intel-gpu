
add_library(enc STATIC)
set_property(TARGET enc PROPERTY FOLDER "sw_fallback")

target_include_directories(enc
  PUBLIC
    mpeg2/include
    h264/include
    ${MSDK_UMC_ROOT}/codec/mpeg2_enc/include
  )

target_sources(enc
  PRIVATE
    h264/include/mfx_h264_enc_enc.h
    h264/src/mfx_h264_enc_enc.cpp

    mpeg2/include/mfx_mpeg2_enc_defs.h
    mpeg2/include/mfx_mpeg2_enc.h
    mpeg2/include/mfx_mpeg2_enc_me.h
    mpeg2/src/mfx_mpeg2_enc.cpp
    mpeg2/src/mfx_mpeg2_enc_me.cpp
  )

target_link_libraries(enc PRIVATE mfx_static_lib)

include(sources_ext.cmake OPTIONAL)
