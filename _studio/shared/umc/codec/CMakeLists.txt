if(MFX_DISABLE_SW_FALLBACK)
  return()
endif()

# =============================== umc_acodecs_merged ==============================================

add_library(umc_acodecs_merged INTERFACE)

target_include_directories(umc_acodecs_merged
  INTERFACE
    common/include
    vc1_common/include
  )

target_link_libraries(umc_acodecs_merged INTERFACE mfx_static_lib vm vm_plus aac_common aac_dec aac_enc mp3_common mp3_dec)

# ================================== umc_codecs_merged ===========================================

add_library( umc_codecs_merged STATIC )
set_property(TARGET umc_codecs_merged PROPERTY FOLDER "umc")

list(APPEND common_include
  asf_spl/include
  avi_spl/include
  common/include
  mpeg4_spl/include
  spl_common/include
  vc1_spl/include
)

list( APPEND common_sources
  asf_spl/src/umc_asf_fb.cpp
  asf_spl/src/umc_asf_parser.cpp
  asf_spl/src/umc_asf_spl.cpp

  avi_spl/src/umc_avi_spl_chunk.cpp
  avi_spl/src/umc_avi_splitter.cpp

  common/src/aaccmn_cce.c
  common/src/aac_enc_search.c
  common/src/aac_win_tables_int.c
  common/src/bstream.c
  common/src/functions_cc.c
  common/src/mp4cmn_config.c
  common/src/mp4cmn_const.c
  common/src/mp4cmn_pce.c

  mpeg4_spl/src/umc_mp4_parser.cpp
  mpeg4_spl/src/umc_mp4_spl_common.cpp
  mpeg4_spl/src/umc_mp4_spl.cpp
  mpeg4_spl/src/umc_mp4_video_parser.cpp

  spl_common/src/umc_index_spl.cpp

  vc1_spl/src/umc_vc1_spl.cpp
)

target_sources(umc_codecs_merged
  PRIVATE
    ${common_sources}
  )

target_include_directories(umc_codecs_merged
  PUBLIC
    ${common_include}
  )

target_compile_definitions(umc_codecs_merged
  PRIVATE
    AS_HEVCD_PLUGIN
    AS_HEVCE_PLUGIN
    AS_H264LA_PLUGIN
  )

target_link_libraries(umc_codecs_merged PUBLIC mfx_static_lib media_buffers vc1_common demuxer h264_spl aac_common)
# ================================ umc_splmux_merged =============================================

add_library( umc_splmux_merged STATIC )
set_property(TARGET umc_splmux_merged PROPERTY FOLDER "umc")

target_sources(umc_splmux_merged
  PRIVATE
    ${common_sources}
  )

target_include_directories(umc_splmux_merged
  PRIVATE
    ${common_include}
  )

target_link_libraries(umc_splmux_merged PUBLIC mfx_static_lib media_buffers vc1_common demuxer h264_spl aac_common)