#!/bin/bash
set -ex

# TODO: remove hardcoded path to lib repository
VERSION_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VERSION_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')
RELEASE_VERSION=$(cat /opt/src/sources/mdp_msdk-lib/_studio/product.ver)

BIN_FILES=(
    "acctv3_read"
    "artificial_stream_generator"
    "asg-hevc"
    "avcrnd"
    "avc_cmp"
    "bd_conformance"
    "bitstreams_parser"
    "bo"
    "bs_trace"
    "calc_crc"
    "cocoto"
    "cocotor"
    "compare_struct"
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
    "hevcrnd"
    "hevc_fei_extractor"
    "hevc_fs_dumper"
    "hevc_qp_dump"
    "hevc_qp_extractor"
    "hevc_stream_info"
    "init"
    "interlace_maker"
    "ldecod"
    "ldecodQP"
    "ldecodRPL"
    "libbs_parser.so"
    "libcttmetrics.so"
    "libmfx-tracer.so"
    "libmfx-tracer.so.${VERSION_MAJOR}"
    "libmfx-tracer.so.${VERSION_MAJOR}.${VERSION_MINOR}"
    "libmfx.pc"
    "libmfx.so"
    "libmfx.so.${VERSION_MAJOR}"
    "libmfx.so.${VERSION_MAJOR}.${VERSION_MINOR}"
    "libmfxhw64.so"
    "libmfxhw64.so.${VERSION_MAJOR}"
    "libmfxhw64.so.${VERSION_MAJOR}.${VERSION_MINOR}"
    "libmfxsw64.so"
    "libmfxsw64.so.${VERSION_MAJOR}"
    "libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR}"
    "libmfx_h264la_hw64.so"
    "libmfx_hevcd_hw64.so"
    "libmfx_hevce_hw64.so"
    "libmfx_hevc_fei_hw64.so"
    "libmfx_player_lucas.so"
    "libmfx_plugin.so"
    "libmfx_transcoder_lucas.so"
    "libmfx_vp8d_hw64.so"
    "libmfx_vp9d_hw64.so"
    "libmfx_vp9e_hw64.so"
    "libmfx_wayland.so"
    "libmock_plugin.so"
    "liboutline.so"
    "libsample_rotate_plugin.so"
    "libva_fuzzer.so"
    "libva_hook.so"
    "metrics_calc_lite"
    "metrics_monitor"
    "mfx-tracer-config"
    "mfx.pc"
    "mfx_dispatch_test"
    "mfx_player"
    "mfx_tracer_test"
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
    "sampler"
    "sample_decode"
    "sample_encode"
    "sample_encode_mod"
    "sample_fei"
    "sample_hevc_fei"
    "sample_hevc_fei_abr"
    "sample_multi_transcode"
    "sample_multi_transcode_mod"
    "sample_vpp"
    "scheduler_tests"
    "stream_cut"
    "stream_maker"
    "telecine"
    "test_comp"
    "test_monitor"
    "test_session_priority"
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
    "vp9_stream_info"
    "vp9_TL_extractor"
	)

LIB_FILES=(
    'libaenc.a'
    )

VPL_FILES=(
    'libmfx-gen.so'
    'libmfx-gen.so.1.2'
    'libmfx-gen.so.1.2.1'
)

# build number arg is optional; only last number of this arg will be used for versioning: 0.0.0 -> 0
build_number="0.0.0"
while getopts b:l:v:o:p:n: flag
do
    case "${flag}" in
        b) binaries_dir=${OPTARG};;
        l) libraries_dir=${OPTARG};;
        v) validation_tools_dir=${OPTARG};;
        o) onevpl_dir=${OPTARG};;
        p) path_to_save=${OPTARG};;
        n) build_number=${OPTARG};;
    esac
done

package_name="lucas_linux_drop_$RELEASE_VERSION.${build_number##*.}.tgz"
package_dir=$path_to_save/lucas_drop

mkdir -p $package_dir/imports/mediasdk
cp -Pv $validation_tools_dir/{mediasdkDir.exe,mediasdkDirLinux.exe,setupMsdkTool.sh} $package_dir
cd $binaries_dir
cp -Pv ${BIN_FILES[*]} $package_dir/imports/mediasdk
cd $libraries_dir
cp -Pv ${LIB_FILES[*]} $package_dir/imports/mediasdk
cd $onevpl_dir
cp -Pv ${VPL_FILES[*]} $package_dir/imports/mediasdk

# Needed to publish artifacts in QuickBuild, сopying each file causes an error
tar -cvzf $path_to_save/$package_name -C $package_dir .
rm -r $package_dir