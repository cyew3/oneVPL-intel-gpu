add_library(enc_hw STATIC)

target_include_directories(enc_hw
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mpeg2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/h264/include
  )

target_sources(enc_hw
  PRIVATE
    h264/include/mfx_h264_enc_hw.h
    h264/src/mfx_h264_enc_hw.cpp

    mpeg2/include/mfx_mpeg2_enc_defs_hw.h
    mpeg2/include/mfx_mpeg2_enc_hw.h
    mpeg2/include/mfx_mpeg2_enc_me_hw.h
    mpeg2/src/mfx_mpeg2_enc_hw.cpp
    mpeg2/src/mfx_mpeg2_enc_me_hw.cpp
  )

target_link_libraries(enc_hw PRIVATE mfx_static_lib)

target_compile_definitions(enc_hw PRIVATE MFX_VA)


include(sources_ext.cmake OPTIONAL)