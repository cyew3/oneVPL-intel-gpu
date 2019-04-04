if(NOT OPEN_SOURCE)
  list( APPEND vdirs
    h264 h265 mjpeg mpeg2 vc1 vp8 vp9 av1
    )
  list( APPEND cdirs
    h265_dec h264_dec mpeg2_dec vp9_dec av1_dec vc1_dec jpeg_dec vc1_common color_space_converter jpeg_common
    )
  mfx_include_dirs( )
  foreach( dir ${vdirs} )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
  endforeach()
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../vpp/include )

  foreach( dir ${cdirs} )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
  endforeach( )

  set(UMC_CODECS ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/)

  set(sources "")
  list(APPEND sources
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_bitstream_cabac.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_bitstream.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_conversion.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_ipplevel.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_ippwrap.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_sei.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_tables_cabac.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_tables.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_direct_pred.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_frame_ex.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_log.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_mfx_sw_supplier.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_cabac_mt.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_cavlc_mt.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_deblocking.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_deblocking_mbaff.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_deblocking_prepare.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_deblocking_table.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_decode_mb_cabac.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_decode_mb.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_decode_mb_types_cabac.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_decode_mb_types.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_hp.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_mblevel_calc.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_mt.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_mt_reconstruct_mv.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_store.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_slice_ex.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_task_broker_mt.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_vda_supplier.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_widevine_decrypter.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_widevine_supplier.cpp
    )

  list(APPEND sources
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_cenc.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_dxva_widevine.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_intel.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_ms.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_widevine.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_widevine_decrypter.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_widevine_slice_decoding.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_widevine_supplier.cpp
    )

  list(APPEND sources
    ${UMC_CODECS}/jpeg_dec/src/jpegdec.cpp
    ${UMC_CODECS}/jpeg_dec/src/mfx_mjpeg_task.cpp
    ${UMC_CODECS}/jpeg_dec/src/umc_mjpeg_mfx_decode.cpp
    )

  list(APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/mpeg2/sw/src/mfx_mpeg2_decode_internal.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg1_dec.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_blk.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_defs.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_mb422.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_mb.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_mc.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_mv.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_pic.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_slice.cpp
    ${UMC_CODECS}/mpeg2_dec/sw/src/umc_mpeg2_dec_tbl.cpp
    )

  list(APPEND sources
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_acinter.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_acintra.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_blk_order_tbl.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_cbpcy_tbl.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_dc_tbl.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_interlaced_cbpcy_tables.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_interlace_mb_mode_tables.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_interlace_mv_tables.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_mv_block_pattern_tables.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_mvdiff_tbl.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_ttmb_tbl.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_zigzag_tbl.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_choose_table.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_deblock_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_deblock_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_deblock.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_blk_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_blk_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_blk.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_cbp.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_debug.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_intens_comp_tbl.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_job.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_bpic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_bpic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_interpolate_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_interpolate_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_interpolate.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_ipic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_ipic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_ppic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mb_ppic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mc.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mv_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mv_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_mv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_run_level_tbl.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_thread.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_smoothing_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_smoothing.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_video_decoder_sw.cpp
    )

  set( sources.plus "" )

  set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
  make_library( decode_ext sw static )
  make_library( decode_ext hw static )
  set( defs "" )
endif()
