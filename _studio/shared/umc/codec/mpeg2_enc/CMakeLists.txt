add_library(mpeg2_enc STATIC)

target_sources(mpeg2_enc
  PRIVATE
    include/umc_mpeg2_enc_debug.h
    include/umc_mpeg2_enc_defs.h
    include/umc_mpeg2_enc.h
    include/umc_mpeg2_enc_me.h
    include/umc_mpeg2_newfunc.h
    include/umc_mpeg2_video_encoder.h
    src/umc_mpeg2_enc_b.cpp
    src/umc_mpeg2_enc_b_me.cpp
    src/umc_mpeg2_enc_cme.cpp
    src/umc_mpeg2_enc.cpp
    src/umc_mpeg2_enc_debug.cpp
    src/umc_mpeg2_enc_i.cpp
    src/umc_mpeg2_enc_me.cpp
    src/umc_mpeg2_enc_params.cpp
    src/umc_mpeg2_enc_p.cpp
    src/umc_mpeg2_enc_p_me.cpp
    src/umc_mpeg2_enc_put.cpp
    src/umc_mpeg2_enc_rc.cpp
    src/umc_mpeg2_newfunc.cpp
    src/umc_mpeg2_video_encoder.cpp
  )

target_include_directories(mpeg2_enc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../brc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../color_space_converter/include
  )

target_link_libraries(mpeg2_enc
  PUBLIC
    umc
    umc_io
    mfx_static_lib
  PRIVATE
    mfx_sdl_properties
  )
set_property(TARGET mpeg2_enc PROPERTY FOLDER "umc")
