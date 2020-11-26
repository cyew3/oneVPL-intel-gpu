#!/bin/bash
set -ex

TARGETS_TOOLS=(
    'ConvertNV12'
    'ConvertYUY2'
    'FSEP'
    'PTIRapp'
    'acctv3_read'
    'artificial_stream_generator'
    'asg-hevc'   # depends on libmfx.so
    'avc_cmp'
    'avcrnd'
    'bd_conformance'
    'bitstreams_parser'
    'bo'
    'bs_parser'
    'bs_trace'
    'calc_crc'
    'cocoto'
    'cocotor'
    'csc_tool'
    'cttmetrics'
    'decode_fbf'   # depends on libmfx.so
    'detail_enh'
    'djpeg'
    'fieldweavingtool'
    'h263_qp_extractor'
    'h264_ff_dumper'
    'h264_fs_dumper'
    'h264_gop_dumper'
    'h264_hrd_stream_checker'
    'h264_parser_fei'
    'h264_qp_extractor'
    'hevc_fei_extractor'
    'hevc_fs_dumper'
    'hevc_qp_dump'
    'hevc_qp_extractor'
    'hevc_random_encoder_10ht50'
    'hevc_random_encoder_10ht62'
    'hevc_random_encoder_eval_10ht50'
    'hevc_random_encoder_eval_ht50'
    'hevc_random_encoder_ht50'
    'hevc_random_encoder_ht62'
    'hevc_stream_generator'
    'hevc_stream_info'
    'hevcrnd'
    'init'
    'interlace_maker'
    'metrics_calc_lite'
    'metrics_monitor'
    'mfx_player'   # depends on libmfx.so
    'mfx_player_lucas'   # depends on libmfx.so
    'mfx_transcoder'   # depends on libmfx.so
    'mfx_transcoder_lucas'   # depends on libmfx.so
    'mfxsw64'
    'mjpeg_parser'
    'mock_plugin'
    'mpeg2_hrd_stream_checker'
    'mpeg2_parser'
    'mpeg2_stream_info'
    'mpg2rnd'
    'msdk_gmock'   # depends on libmfx.so
    'msdk_ts'   # depends on libmfx.so
#    'msdk_ts_dll',  # depends on libmfx.so, deprecated
    'mvc_tools_split'
    'outline'
    'outline_diff'
    'pci_inspect'
    'proc_amp'
    'psinspect'   # depends on libcttmetrics
    'ref_compose'
    'ref_frc'
    'ref_ltbx'
    'reorgia'
    'sample_decode'
    'sample_encode'
    'sample_encode_mod'
    'sample_fei'
    'sample_hevc_fei'
    'sample_hevc_fei_abr'
    'sample_multi_transcode'
    'sample_multi_transcode_mod'
    'sample_rotate_plugin'
    'sample_spl_mux_dispatcher'
    'sample_vpp'
    'sampler'
    'stream_cut'
    'stream_maker'
    'telecine'
#    'test_behavior'   # depends on libmfx.so, not building for now
#    'hevce_tests'   #deprecated
    'test_comp'   # depends on libmfx.so
    'test_muxer'
    'test_session_priority'   # depends on libmfx.so
    'test_splitter'
    'test_thread_safety'   # depends on libmfx.so
    'test_usage_models'   # depends on libmfx.so
    'test_vpp'   # depends on libmfx.so
    'test_vpp_multisession'   # depends on libmfx.so
    'uc_conformance'
    'va_fuzzer'
    'va_hook'
    'va_wrapper'
    'vaapi_buf'
    'vaapi_ctx'
    'vaapi_init'
    'vaapi_srf'
    'vp8_stream_info'
    'vp9_TL_extractor'
    'vp9_stream_info'
	)

for target in ${TARGETS_TOOLS[*]}
  do
    make -j$(nproc) ${target}
  done
