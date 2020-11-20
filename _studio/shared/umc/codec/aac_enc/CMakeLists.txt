if(OPEN_SOURCE)
  return()
endif()

add_library(aac_enc)

target_include_directories(aac_enc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(aac_enc
  PRIVATE
    src/aac_enc_api_fp.c
    src/aac_enc_ltp_fp.c
    src/aac_enc_psychoacoustic_fp.c
    src/aac_enc_psy_tables_fp.c
    src/aac_enc_quantization_fp.c
    src/aac_enc_thr.c
    src/sbr_enc_bitstream.c
    src/sbr_enc_dbg.c
    src/sbr_enc_delta_coding.c
    src/sbr_enc_encoder_fp.c
    src/sbr_enc_env_estimation_fp.c
    src/sbr_enc_frame_gen.c
    src/sbr_enc_invf_estimation_fp.c
    src/sbr_enc_noisef_estimation_fp.c
    src/sbr_enc_qmf_fp.c
    src/sbr_enc_resampler_fp.c
    src/sbr_enc_sin_estimation_fp.c
    src/sbr_enc_ton_estimation_fp.c
    src/sbr_enc_tuning_tabs.c
    src/sbr_enc_utility_fp.c
    src/umc_aac_encoder.cpp
    include/aac_enc_fp.h
    include/aac_enc_ltp_fp.h
    include/aac_enc_own_fp.h
    include/aac_enc_psychoacoustic_fp.h
    include/aac_enc_quantization_fp.h
    include/saacenc_tns.h
    include/sbr_enc_api_fp.h
    include/sbr_enc_dbg.h
    include/sbr_enc_own_fp.h
    include/sbr_enc_settings.h
    include/sbr_enc_struct.h
    include/umc_aac_encoder.h
)

target_link_libraries(aac_enc
  PUBLIC
    mfx_static_lib
    aac_common
)

set_property(TARGET aac_enc PROPERTY FOLDER "umc")
