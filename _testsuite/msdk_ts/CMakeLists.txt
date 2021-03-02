# =============================================================================
# msdk_gmock:

if (MFX_BUNDLED_IPP OR (NOT BUILD_SAMPLES))
  return()
endif()

set(common_linux_defs
  LINUX32
  LIBVA_SUPPORT
  LIBVA_DRM_SUPPORT
  $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:LINUX64>
)

disable_werror()

add_executable(msdk_gmock)

target_include_directories(msdk_gmock
  PUBLIC
    ${MFX_API_HOME}/mediasdk_structures
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gmock
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gmock/test_suites
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MFX_API_HOME}/mediasdk_structures
)

if (MFX_ENABLE_AV1_VIDEO_DECODE)
  set(AV1_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_decode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_decode_header.cpp
    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_film_grain.cpp
    >
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_get_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/av1d_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avc_sethandle_16.cpp
  )
  source_group("av1" FILES ${AV1_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_H264_VIDEO_DECODE)
  set(H264_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_bs_dataflag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_constraints.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_corruption.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_decode_header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_decorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_field_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_get_decode_stat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_hw_slice_groups_present.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_init_sfc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_payload_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_picturetype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_res_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avcd_reset.cpp
  )
  source_group("avc" FILES ${H264_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_H264_VIDEO_ENCODE)
  set (H264_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_adaptivegop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_adaptivemaxframesize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_advanced_ref_lists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_aspectratio_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_b_pyr_control.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_b_pyramid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_brc_panic_mode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_chromaloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_co2_disable_deblocking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_co2_use_rawref.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_constraint_vbr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_cqp_hrd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_default_lowpower.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_default_ref_lists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_dirty_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_dts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_dynamic_intra_refresh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_emulation_prevention_bytes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_encoder_roi.cpp

    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_custom_qmatrix.cpp
    >

    $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_et_gs_query_init.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_et_mds_encode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_et_mds_query_init.cpp
    >
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_extbrcadaptiveltr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_force_repartition_check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_get_video_param_tcbrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_idr_insertion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_input_rgb_colorspace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_inter_mbs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_interlace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_intra_refresh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_level_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_look_ahead.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_lookahead_dynres.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_lowdelay_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_max_bins_pcm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_max_frame_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_max_slice_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_maxframesize_ipb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mb_disable_skip_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mb_per_sec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mbbrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mbcontrol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mbext_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mbqp.cpp
    $<$<BOOL:${MFX_ENABLE_MFE}>:
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_close.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_encode_frame_async.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_get_video_param.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_init.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_query.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_query_io_surf.cpp
    >
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_move_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_nalu_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_nummbperslice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_oversized_surfaces.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_partial_output.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_pi_field_pair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_qp_offset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_qp_range.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_qp_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_repeat_pps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_reset_cbr_hrd_conf_vui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_roi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_skipframes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_skipframes_tektronix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_sliding_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_sps_frame_crop_offset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_start_new_sequence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_trellis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_trellis_cavlc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_vui_flag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_weight_prediction.cpp
  )
  if (MFX_ENABLE_MFE AND (NOT API_USE_VPL))
    list (APPEND H264_VIDEO_ENCODE_SRC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_close.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_encode_frame_async.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_get_video_param.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_init.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_query.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/avce_mfe_fei_query_io_surf.cpp
    )
  endif()
  source_group("avc" FILES ${H264_VIDEO_ENCODE_SRC})
endif()

if (NOT API_USE_VPL)
  set (CAMERA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_run_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/camera_unsupport.cpp
  )
  source_group("camera" FILES ${CAMERA_SRC})
endif()

if (MFX_ENABLE_H265_VIDEO_DECODE)
  set (H265_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_bs_data_flag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_corruption.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_decode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_decode_header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_decode_order.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_frametype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_get_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_low_latency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_pts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_shift.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevcd_stream_change.cpp
  )
  source_group("hevc" FILES ${H265_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_H265_VIDEO_ENCODE)
  set(H265_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_8b_420_nv12_qp_range.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_argb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_big_resolution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_brc_panic_mode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_co2_disable_deblocking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_co2_max_slice_size_in_ctu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_custom_qmatrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_default_lowpower.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_default_ref_lists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_dirty_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_encode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_explicit_weight_prediction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_frame_qp.cpp

    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_gaa_close.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_gaa_init.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_gaa_processframeasync.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_gaa_reset.cpp
    >

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_goppattern.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_hrd_violation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_idr_insertion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_interlace_invalid_scenarios.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_interlace_sei_sps_flags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_intra_refresh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_lcu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_level_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_levels_profile_support.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_load.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_lowdelay_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_lowlatency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_lterm_reflist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_max_slice_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_maxframesize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_maxpicwidth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_mbqp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_multislice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_p_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_p_pyramid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_pic_timing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_pts_dts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_qp_offset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_qp_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_ref_list1_checks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_reset_swvbr_hrd_conf_vui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_roi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_sao.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_scc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_sei_hdr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_skipframes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_slice_size_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_sliding_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_spspps_get.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_tiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_transformskip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_weight_prediction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevc_ctuqp_quality.cpp
  )
  source_group("hevc" FILES ${H265_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_MPEG2_VIDEO_DECODE)
  set(MPEG2_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_aspect_ratio_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_bs_dataflag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_corruption.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_no_i_frames.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_picturetype.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_res_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_timecode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2d_timestamp.cpp
  )
  source_group("mpeg2" FILES ${MPEG2_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
  set(MPEG2_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_aspectratio_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_brc_panic_mode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_headers_order.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_level_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_mbqp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_ratecontrol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_skipframes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2e_user_data.cpp
  )
  source_group("mpeg2" FILES ${MPEG2_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_MJPEG_VIDEO_DECODE)
  set(MJPEG_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpegd_bs_dataflag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpegd_colorformat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpegd_corruption.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpegd_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpegd_payload.cpp
  )
  source_group("mjpeg" FILES ${MJPEG_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
  set(MJPEG_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mjpege_tables.cpp
  )
  source_group("mjpeg" FILES ${MJPEG_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_VP9_VIDEO_DECODE)
  set(VP9_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_decode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_decode_header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_format_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_get_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_res_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9d_trans_opaque.cpp
  )
  source_group("vp9" FILES ${VP9_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VP9_VIDEO_ENCODE)
  set(VP9_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_big_resolution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_cqp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_dynamic_scaling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_encode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_goppattern.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_multiref.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_segmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_temporal_scalability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_tiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp9e_trans_opaque.cpp
  )
  source_group("vp9" FILES ${VP9_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_VPP)
  set(VPP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_chroma_siting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_composition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_crop_query_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_deinterlac_inv_telec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_deinterlacing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_donotuse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_douse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_field_weaving_splitting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_fieldcopy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_frc_interp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_getvideoparam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_getvppstat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_initclose_fieldcopy_robustness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_douse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_getvideoparam.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_query_extbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_query_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mctf_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_mirror.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_multi_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_oversized_surfaces.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_pts_suite.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_query_extbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_queryiosurf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_res_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_rotate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_uyvy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_video_signal_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp10bit.cpp

    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_frame_async_ex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_gpu_hang.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_sw_wrn_filter_skipped.cpp
    >
  )
  source_group("vpp" FILES ${VPP_SRC})
endif()

if (MFX_ENABLE_SCREEN_CAPTURE)
  set(SCREEN_CAPTURE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_decode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_decode_header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_dirty_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/screen_capture_query_io_surf.cpp
  )
  source_group("screen_capture" FILES ${SCREEN_CAPTURE_SRC})
endif()

if (MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
  set(H264_VIDEO_FEI_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_enc_memory_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_enc_query_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_advanced_ref_lists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_bp_sei.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_default_ref_lists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_direct_bias.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_disable_deblocking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_enablembqp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_encode_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_ext_coding_option.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_field_coding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_gpu_hang.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_mbqp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_memory_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_num_active_ref.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_oversized_surfaces.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_pi_field_pair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_ref_list_ctrl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_sei_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_timestamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encode_weighted_prediction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encpak_aud.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encpak_mmco.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encpak_pps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encpak_sei_payload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_encpak_sps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_multi_pak.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_pak_gpu_hang.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_pak_memory_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_pak_query_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_pak_queryiosurf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_pak_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_downsample.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_enable8x8stat_check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_gpu_hang.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_memory_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_mvmb_valid_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_process_frame_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/fei_preenc_query_init.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/lookahead_1N.cpp
  )
  source_group("fei" FILES ${H264_VIDEO_FEI_ENCODE_SRC})
endif()

if (MFX_ENABLE_VC1_VIDEO_DECODE)
  set(VC1_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vc1_sethandle_16.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vc1d_bs_dataflag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vc1d_res_change.cpp
  )
  source_group("vc1" FILES ${VC1_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VP8_VIDEO_DECODE)
  set(VP8_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vp8d_init.cpp
  )
  source_group("vp8" FILES ${VP8_VIDEO_DECODE_SRC})
endif()

target_sources(msdk_gmock
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_aux_dev.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_bitstream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_streams.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_surface_provider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_vpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_transcoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_utils.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_plugin.cpp

    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/get_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_preenc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_enc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_pak.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_encpak.cpp
    >

    ${AV1_VIDEO_DECODE_SRC}
    ${H264_VIDEO_DECODE_SRC}
    ${H264_VIDEO_ENCODE_SRC}
    ${H265_VIDEO_DECODE_SRC}
    ${H265_VIDEO_ENCODE_SRC}
    ${MPEG2_VIDEO_DECODE_SRC}
    ${MPEG2_VIDEO_ENCODE_SRC}
    ${MJPEG_VIDEO_DECODE_SRC}
    ${MJPEG_VIDEO_ENCODE_SRC}
    ${VC1_VIDEO_DECODE_SRC}
    ${VP8_VIDEO_DECODE_SRC}
    ${VP9_VIDEO_DECODE_SRC}
    ${VP9_VIDEO_DECODE_SRC}
    ${VP9_VIDEO_ENCODE_SRC}
    ${VPP_SRC}
    ${CAMERA_SRC}
    ${SCREEN_CAPTURE_SRC}
    ${H264_VIDEO_FEI_ENCODE_SRC}

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/encoders_extbrc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/feature_unsupported.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/jpegd_res_change.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mpeg2_sethandle_16.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/multisession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mvc_sethandle_16.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/mvce_view_output.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/opaque_dynamic_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_close.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_frame_async_ex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_get_video_param.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_query_io_surf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/ptir_reset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/query_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/query_impls_description.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/threading_mfxinitex.cpp

    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/trans_opaque.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/trans_opaque_deallocation.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/decoders_gpu_hang.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/encoders_gpu_hang.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/gpu_hang_transcoding.cpp
    >

    $<$<PLATFORM_ID:Linux>:
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_nalutype.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_num_active_references.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/vpp_fieldweaving_robustness.cpp
    >
)

if(MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
  target_include_directories(msdk_gmock PRIVATE ${MSDK_SAMPLES_ROOT}/sample_hevc_fei/include)
  target_sources(msdk_gmock
    PRIVATE
      ${MSDK_SAMPLES_ROOT}/sample_hevc_fei/src/fei_buffer_allocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_avc_intra_modes.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_ctu_force_flags.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_ctuqp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_ctuqp_invalid_params.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_ctuqp_quality.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_default_ref_lists.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_fast_intra_mode_inter_pred.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_force_ctu_split.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_frame_qp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_frame_qp_ipb.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_idr_insertion.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_invalid_runtime_parameters.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_nummbperslice.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_numslice.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_numslice_ipb.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/hevce_fei_payload.cpp
  )
endif()

target_compile_definitions(msdk_gmock
  PRIVATE
    _VARIADIC_MAX=10
    ENABLE_PS=1
    MFX_VERSION_USE_LATEST
    MFX_HAVE_EXTERNAL_CONFIG
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
)

target_link_libraries(msdk_gmock
  PRIVATE
    sample_common
    bs_parser_static
    gmock_160
    umc_va_hw
    mfx_static_lib
    mfx_shared_lib
    ${MFX_DISPATCHER}
    ${CMAKE_DL_LIBS}
    IPP::core IPP::s IPP::vc IPP::dc IPP::i IPP::cc IPP::cv IPP::j IPP::msdk
    ${CMAKE_THREAD_LIBS_INIT}
    mfx_serializer
)

set_property(TARGET msdk_gmock PROPERTY FOLDER "testsuite")

# =============================================================================
# msdk_ts application

add_executable(msdk_ts)

target_include_directories(msdk_ts
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(msdk_ts
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/msdk_ts_blocks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_blocks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_verif.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_libva_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_output.cpp
)

target_link_libraries(msdk_ts
  PUBLIC
    umc
    vm
    vm_plus
    demuxer
    shared_utils
    bs_parser_static
    ${CMAKE_DL_LIBS}
    IPP::i IPP::s IPP::vc IPP::dc  IPP::cc IPP::cv IPP::j IPP::msdk IPP::core
    sample_common
    ${CMAKE_THREAD_LIBS_INIT}
    mfx_static_lib
)

target_compile_definitions(msdk_ts
  PUBLIC
    USE_SAMPLE_ALLOCATOR=1
    MFX_VERSION_USE_LATEST
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
)

set_property(TARGET msdk_ts PROPERTY FOLDER "testsuite")

# =============================================================================
# msdk_ts_dll library


#find_package ( Perl )
#find_package ( PerlLibs )

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  set( PERL_EXECUTABLE "perl" )

  execute_process ( COMMAND ${PERL_EXECUTABLE}
          -MConfig -e "print \$Config{version}"
          OUTPUT_VARIABLE PERL_OUTPUT
          RESULT_VARIABLE PERL_RETURN_VALUE )
  if ( NOT PERL_RETURN_VALUE )
  set ( PERL_VERSION ${PERL_OUTPUT} )
  endif ( NOT PERL_RETURN_VALUE )

  #if ( PERL_INCLUDE_PATH MATCHES .*-NOTFOUND )
  execute_process ( COMMAND ${PERL_EXECUTABLE}
            -MConfig -e "print \$Config{archlibexp}"
            OUTPUT_VARIABLE PERL_OUTPUT
            RESULT_VARIABLE PERL_RETURN_VALUE )
  if ( NOT PERL_RETURN_VALUE )
    set ( PERL_INCLUDE_PATH ${PERL_OUTPUT}/CORE )
  endif ( NOT PERL_RETURN_VALUE )
  #endif ( PERL_INCLUDE_PATH MATCHES .*-NOTFOUND )

  #if ( PERL_LIBRARY MATCHES .*-NOTFOUND )
  execute_process ( COMMAND ${PERL_EXECUTABLE}
            -MConfig -e "print \$Config{libperl}"
            OUTPUT_VARIABLE PERL_OUTPUT
            RESULT_VARIABLE PERL_RETURN_VALUE )
  if ( NOT PERL_RETURN_VALUE )
    #set ( PERL_LIBRARY /usr/lib/${PERL_OUTPUT} )
    find_file( PERL_LIBRARY ${PERL_OUTPUT}
      PATHS "/usr/lib/" ${PERL_INCLUDE_PATH} )
  endif ( NOT PERL_RETURN_VALUE )
  #endif ( PERL_LIBRARY MATCHES .*-NOTFOUND )

  message ( STATUS "  PERL_VERSION:           " ${PERL_VERSION} )
  message ( STATUS "  PERL_EXECUTABLE:        " ${PERL_EXECUTABLE} )
  message ( STATUS "  PERL_INCLUDE_PATH:      " ${PERL_INCLUDE_PATH} )
  message ( STATUS "  PERL_LIBRARY:           " ${PERL_LIBRARY} )

  if(NOT PERL_VERSION)
  return()
  endif()

  add_library(msdk_ts_dll SHARED)

  target_include_directories(msdk_ts_dll
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../_studio/shared/umc/codec/demuxer/include
    ${PERL_INCLUDE_PATH}
  )


  target_sources(msdk_ts_dll
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dll_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_blocks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_verif.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_libva_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts_wrap.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_output.cpp
  )

  target_compile_definitions(msdk_ts_dll
  PUBLIC
    USE_SAMPLE_ALLOCATOR=1
    MFX_VERSION_USE_LATEST
    MSDK_TS_DLL_EXPORTS
  )

  target_link_libraries(msdk_ts_dll
  PUBLIC
    umc
    vm
    vm_plus
    bs_parser_static
    mfx_shared_lib
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    IPP::core IPP::s IPP::vc IPP::dc IPP::i IPP::cc IPP::cv IPP::j IPP::msdk
    sample_common
    ${PERL_LIBRARY}
  )

  set_property(TARGET msdk_ts_dll PROPERTY FOLDER "testsuite")
endif()