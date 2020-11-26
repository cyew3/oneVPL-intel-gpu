#!/bin/bash
set -ex

VERSION_MINOR=$(cat /opt/src/msdk_repos/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VERSION_MAJOR=$(cat /opt/src/msdk_repos/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')

BIN_FILES=(
    "acctv3_read"
    "artificial_stream_generator"
    "asg-hevc"
    "avc_cmp"
    "avcrnd"
    "bd_conformance"
    "bitstreams_parser"
    "bo"
    "bs_trace"
    "calc_crc"
    "cocoto"
    "cocotor"
    "ConvertNV12"
    "ConvertYUY2"
    "csc_tool"
    "decode_fbf"
    "detail_enh"
    "djpeg"
    "fieldweavingtool"
    "FSEP"
    "h263_qp_extractor"
    "h264_ff_dumper"
    "h264_fs_dumper"
    "h264_gop_dumper"
    "h264_hrd_stream_checker"
    "h264_parser_fei"
    "h264_qp_extractor"
    "hevc_fei_extractor"
    "hevc_fs_dumper"
    "hevc_qp_dump"
    "hevc_qp_extractor"
    "hevc_random_encoder_10ht50"
    "hevc_random_encoder_10ht62"
    "hevc_random_encoder_eval_10ht50"
    "hevc_random_encoder_eval_ht50"
    "hevc_random_encoder_ht50"
    "hevc_random_encoder_ht62"
    "hevc_stream_generator"
    "hevc_stream_info"
#    "hevce_tests"   #deprecated
    "hevcrnd"
    "init"
    "interlace_maker"
    "libbs_parser.so"
    "libcttmetrics.so"
    "libmfx_player_lucas.so"
    "libmfx_transcoder_lucas.so"
    "libmfxsw64.so"
    "libmfxsw64.so.${VERSION_MAJOR}"
    "libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR}"
    "libmock_plugin.so"
#    "libmsdk_ts.so"   #deprecated
    "liboutline.so"
    "libsample_rotate_plugin.so"
    "libva_fuzzer.so"
    "libva_hook.so"
#    "libva_wrapper.so"   # now it's libva_wrapper.a
    "metrics_calc_lite"
    "metrics_monitor"
    "mfx_player"
    "mfx_transcoder"
    "mjpeg_parser"
    "mpeg2_hrd_stream_checker"
    "mpeg2_parser"
    "mpeg2_stream_info"
    "mpg2rnd"
    "msdk_gmock"
    "msdk_ts"
    "mvc_tools_split"
    "outline_diff"
    "pci_inspect"
    "proc_amp"
    "psinspect"
    "PTIRapp"
    "ref_compose"
    "ref_frc"
    "ref_ltbx"
    "reorgia"
    "sample_decode"
    "sample_encode"
    "sample_encode_mod"
    "sample_fei"
    "sample_hevc_fei"
    "sample_hevc_fei_abr"
    "sample_multi_transcode"
    "sample_multi_transcode_mod"
    "sample_vpp"
    "sampler"
    "stream_cut"
    "stream_maker"
    "telecine"
    "test_behavior"
    "test_comp"
    "test_muxer"
    "test_session_priority"
    "test_splitter"
    "test_thread_safety"
    "test_usage_models"
    "test_vpp"
    "test_vpp_multisession"
    "uc_conformance"
    "vaapi_buf"
    "vaapi_ctx"
    "vaapi_init"
    "vaapi_srf"
    "vp8_stream_info"
    "vp9_TL_extractor"
    "vp9_stream_info"
	)
	
LIB_FILES=(
    'libsample_spl_mux_dispatcher.a'
    )

while getopts b:l:v: flag
do
    case "${flag}" in
        b) binaries_dir=${OPTARG};;
		l) libraries_dir=${OPTARG};;
        v) validation_tools_dir=${OPTARG};;
    esac
done

package_name=drop.tgz
package_dir=/opt/src/output/drop/to_drop

mkdir -p $package_dir/imports/mediasdk
cp -v $validation_tools_dir/{mediasdkDir.exe,mediasdkDirLinux.exe,setupMsdkTool.sh} $package_dir
cd $binaries_dir && cp -v ${BIN_FILES[*]} $package_dir/imports/mediasdk
cd $libraries_dir && cp -v ${LIB_FILES[*]} $package_dir/imports/mediasdk

# Needed to publish artifacts in QuickBuild, сopying each file causes an error
tar -cvzf /opt/src/output/drop/$package_name -C $package_dir .
rm -r $package_dir
