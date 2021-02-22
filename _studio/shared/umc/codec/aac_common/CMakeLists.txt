if(OPEN_SOURCE)
  return()
endif()

add_library(aac_common)

target_include_directories(aac_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(aac_common
  PRIVATE
    src/aaccmn_adif.c
    src/aaccmn_adts.c
    src/aaccmn_chmap.c
    src/aaccmn_huff.c
    src/aac_dec_api.c
    src/aac_dec_stream_elements.c
    src/aac_enc_elements.c
    src/aac_enc_huff_tables.c
    src/aac_enc_search.c
    src/aac_filterbank_fp.c
    src/aac_sfb_tables.c
    src/aac_win_tables_fp.c
    src/ps_dec_parser.c
    src/ps_huff_tabs.c
    src/sbr_dec_env_decoding.c
    src/sbr_dec_parser.c
    src/sbr_dec_reset.c
    src/sbr_freq_tabs.c
    src/sbr_huff_tabs.c
    src/sbr_pow_vec.c
    include/aaccmn_adif.h
    include/aaccmn_adts.h
    include/aaccmn_chmap.h
    include/aaccmn_huff.h
    include/aac_dec_own.h
    include/aac_enc_huff_tables.h
    include/aac_enc_own.h
    include/aac_enc_search.h
    include/aac_filterbank_fp.h
    include/aac_sfb_tables.h
    include/aac_status.h
    include/aac_wnd_tables_fp.h
    include/ps_dec_parser.h
    include/ps_dec_settings.h
    include/ps_dec_struct.h
    include/sbr_dec_parser.h
    include/sbr_dec_settings.h
    include/sbr_dec_struct.h
    include/sbr_freq_tabs.h
    include/sbr_huff_tabs.h
    include/sbr_settings.h
    include/sbr_struct.h
)

target_link_libraries(aac_common
  PUBLIC
    mfx_static_lib
    umc
    common
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET aac_common PROPERTY FOLDER "umc")
