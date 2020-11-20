if(MFX_DISABLE_SW_FALLBACK)
  return()
endif()

add_library(aac_dec)

target_include_directories(aac_dec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(aac_dec
  PRIVATE
    include/aac_dec_decoding_fp.h
    include/aac_dec_fp.h
    include/aac_dec_huff_tables_fp.h
    include/aac_dec_ltp_fp.h
    include/aac_dec_own_fp.h
    include/aac_dec_ps_fp.h
    include/aac_dec_sbr_fp.h
    include/aac_dec_tns_fp.h
    include/als_dec.h
    include/als_dec_own.h
    include/als_dec_tabs.h
    include/bsac_dec.h
    include/bsac_dec_tabs.h
    include/sbr_dec_tabs_fp.h
    include/umc_aac_decoder.h
    src/aac_dec_api_fp.c
    src/aac_dec_decoding_fp.c
    src/aac_dec_huff_tables_fp.c
    src/aac_dec_ltp_fp.c
    src/aac_dec_ps_fp.c
    src/aac_dec_sbr_fp.c
    src/aac_dec_ssr_fp.c
    src/aac_dec_tns_fp.c
    src/als_dec_api.c
    src/als_dec_parsers.c
    src/als_dec_reconstruct.c
    src/als_dec_tables.c
    src/bsac_dec_api.c
    src/bsac_dec_tables.c
    src/ps_dec_qmf_fp.c
    src/ps_dec_tabs_fp.c
    src/sbr_dec_dequant_fp.c
    src/sbr_dec_filter_qmf_fp.c
    src/sbr_dec_hf_adjust_fp.c
    src/sbr_dec_hf_gen_fp.c
    src/sbr_dec_noise_tabs_fp.c
    src/sbr_dec_qmf_tabs_fp.c
    src/umc_aac_decoder.cpp
)

target_link_libraries(aac_dec
  PUBLIC
    mfx_static_lib
    aac_common
)

set_property(TARGET aac_dec PROPERTY FOLDER "umc")
