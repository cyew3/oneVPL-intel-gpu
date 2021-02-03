#!/bin/bash
set -ex

BIN_FILES=(
  'acctv3_read'
  'artificial_stream_generator'
  'asg-hevc'
  'avcrnd'
  'avc_cmp'
  'bd_conformance'
  'bitstreams_parser'
  'bo'
  'bs_trace'
  'calc_crc'
  'cocoto'
  'cocotor'
  'ConvertNV12'
  'ConvertYUY2'
  'csc_tool'
  'decode_fbf'
  'detail_enh'
  'djpeg'
  'fieldweavingtool'
  'FSEP'
  'h263_qp_extractor'
  'h264_ff_dumper'
  'h264_fs_dumper'
  'h264_gop_dumper'
  'h264_hrd_stream_checker'
  'h264_parser_fei'
  'h264_qp_extractor'
  'hevcrnd'
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
  'init'
  'interlace_maker'
  'metrics_calc_lite'
  'mfx_player'
  'mfx_transcoder'
  'mjpeg_parser'
  'mpeg2_hrd_stream_checker'
  'mpeg2_parser'
  'mpeg2_stream_info'
  'mpg2rnd'
  'msdk_gmock'
  'msdk_ts'
  'mvc_tools_split'
  'outline_diff'
  'pci_inspect'
  'proc_amp'
  'psinspect'
  'PTIRapp'
  'ref_compose'
  'ref_frc'
  'ref_ltbx'
  'reorgia'
  'sampler'
  'stream_cut'
  'stream_maker'
  'telecine'
#  'test_behavior' #Get from artifactory
  'test_comp'
  'test_muxer'
  'test_session_priority'
  'test_splitter'
  'test_thread_safety'
  'test_usage_models'
  'test_vpp'
  'test_vpp_multisession'
  'uc_conformance'
  'vaapi_buf'
  'vaapi_ctx'
  'vaapi_init'
  'vaapi_srf'
  'vp8_stream_info'
  'vp9_stream_info'
  'vp9_TL_extractor'
  )

PLUGINS=(
  'libbs_parser.so'
  'libcttmetrics.so'
  'libmfx_player_lucas.so'
  'libmfx_transcoder_lucas.so'
  'libmock_plugin.so'
  'liboutline.so'
  'libva_fuzzer.so'
  )

# build number arg is optional; only last number of this arg will be used for versioning: 0.0.0 -> 0
build_number="0.0.0"
while getopts b:t:h:p:n: flag
do
    case "${flag}" in
        b) binaries_dir=${OPTARG};;
        t) test_behavior_dir=${OPTARG};;
        h) hevce_tests=${OPTARG};;
        p) path_to_save=${OPTARG};;
        n) build_number=${OPTARG};;
    esac
done

# TODO: remove hardcoded path to lib repository
RELEASE_VERSION=$(cat /opt/src/sources/mdp_msdk-lib/_studio/product.ver)

package_dir=$path_to_save/tools_to_deb_pkg
package_name="mediasdk-tools"

mkdir -p $package_dir/usr/local/lib/x86_64-linux-gnu $package_dir/usr/local/bin $package_dir/DEBIAN
cd $binaries_dir
cp -Pv ${BIN_FILES[*]} $package_dir/usr/local/bin
cp -Pv ${PLUGINS[*]} $package_dir/usr/local/lib/x86_64-linux-gnu
cp -Pv $test_behavior_dir/test_behavior $package_dir/usr/local/bin

echo "Package: $package_name
Version: $RELEASE_VERSION.${build_number##*.}
Section: default
Priority: optional
Architecture: amd64
Description: no description" > $package_dir/DEBIAN/control

dpkg-deb --build $package_dir $path_to_save
rm -rf $package_dir
