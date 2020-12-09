#!/bin/bash
set -ex

while getopts b:h:p: flag
do
    case "${flag}" in
        b) build_dir=${OPTARG};;
        h) headers_dir=${OPTARG};;
        p) path_to_save=${OPTARG};;
    esac
done

declare -a COPY_CMDS=(
'cp -P $build_dir/mediasdk/__bin/release/asg-hevc $package_dir/bin/asg-hevc'
'cp -P $build_dir/mediasdk/__bin/release/hevc_fei_extractor $package_dir/bin/hevc_fei_extractor'
'cp -P $build_dir/mediasdk/__bin/release/metrics_calc_lite $package_dir/bin/metrics_calc_lite'
'cp -P $build_dir/mediasdk/__bin/release/mfx-tracer-config $package_dir/bin/mfx-tracer-config'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so $package_dir/lib64/libmfx-tracer.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so.${VERSION_MAJOR} $package_dir/lib64/libmfx-tracer.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/lib64/libmfx-tracer.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so $package_dir/lib64/libmfx.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so.${VERSION_MAJOR} $package_dir/lib64/libmfx.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/lib64/libmfx.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so $package_dir/lib64/libmfxhw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so.${VERSION_MAJOR} $package_dir/lib64/libmfxhw64.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/lib64/libmfxhw64.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so $package_dir/lib64/libmfxsw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so.${VERSION_MAJOR} $package_dir/lib64/libmfxsw64.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/lib64/libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_h264la_hw64.so $package_dir/lib64/mfx/libmfx_h264la_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevcd_hw64.so $package_dir/lib64/mfx/libmfx_hevcd_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevce_hw64.so $package_dir/lib64/mfx/libmfx_hevce_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevc_fei_hw64.so $package_dir/lib64/mfx/libmfx_hevc_fei_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp8d_hw64.so $package_dir/lib64/mfx/libmfx_vp8d_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp9d_hw64.so $package_dir/lib64/mfx/libmfx_vp9d_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp9e_hw64.so $package_dir/lib64/mfx/libmfx_vp9e_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.pc $package_dir/lib64/pkgconfig/libmfx.pc'
'cp -P $build_dir/mediasdk/__lib/release/libmfxhw64.pc $package_dir/lib64/pkgconfig/libmfxhw64.pc'
'cp -P $build_dir/mediasdk/__lib/release/libmfxsw64.pc $package_dir/lib64/pkgconfig/libmfxsw64.pc'
'cp -P $build_dir/mediasdk/__bin/release/mfx.pc $package_dir/lib64/pkgconfig/mfx.pc'
'cp -P $build_dir/mediasdk/plugins.cfg $package_dir/share/mfx/plugins.cfg'
'cp -P $build_dir/mediasdk/__bin/release/libcttmetrics.so $package_dir/share/mfx/samples/libcttmetrics.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_wayland.so $package_dir/share/mfx/samples/libmfx_wayland.so'
'cp -P $build_dir/mediasdk/__bin/release/libsample_rotate_plugin.so $package_dir/share/mfx/samples/libsample_rotate_plugin.so'
'cp -P $build_dir/mediasdk/__lib/release/libvpp_plugin.a $package_dir/share/mfx/samples/libvpp_plugin.a'
'cp -P $build_dir/mediasdk/__bin/release/metrics_monitor $package_dir/share/mfx/samples/metrics_monitor'
'cp -P $build_dir/mediasdk/__bin/release/sample_decode $package_dir/share/mfx/samples/sample_decode'
'cp -P $build_dir/mediasdk/__bin/release/sample_encode $package_dir/share/mfx/samples/sample_encode'
'cp -P $build_dir/mediasdk/__bin/release/sample_encode_mod $package_dir/share/mfx/samples/sample_encode_mod'
'cp -P $build_dir/mediasdk/__bin/release/sample_fei $package_dir/share/mfx/samples/sample_fei'
'cp -P $build_dir/mediasdk/__bin/release/sample_hevc_fei $package_dir/share/mfx/samples/sample_hevc_fei'
'cp -P $build_dir/mediasdk/__bin/release/sample_hevc_fei_abr $package_dir/share/mfx/samples/sample_hevc_fei_abr'
'cp -P $build_dir/mediasdk/__bin/release/sample_multi_transcode $package_dir/share/mfx/samples/sample_multi_transcode'
'cp -P $build_dir/mediasdk/__bin/release/sample_multi_transcode_mod $package_dir/share/mfx/samples/sample_multi_transcode_mod'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so $package_dir/lib64/libmfx-gen.so'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so.1.2 $package_dir/lib64/libmfx-gen.so.1.2'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so.1.2.1 $package_dir/lib64/libmfx-gen.so.1.2.1'
'cp -P $build_dir/mediasdk/__bin/release/sample_vpp $package_dir/share/mfx/samples/sample_vpp'
'cp -P $headers_dir/include/mfxadapter.h $package_dir/include/mfx/mfxadapter.h'
'cp -P $headers_dir/include/mfxastructures.h $package_dir/include/mfx/mfxastructures.h'
'cp -P $headers_dir/include/mfxaudio++.h $package_dir/include/mfx/mfxaudio++.h'
'cp -P $headers_dir/include/mfxaudio.h $package_dir/include/mfx/mfxaudio.h'
'cp -P $headers_dir/include/mfxbrc.h $package_dir/include/mfx/mfxbrc.h'
'cp -P $headers_dir/include/mfxcamera.h $package_dir/include/mfx/mfxcamera.h'
'cp -P $headers_dir/include/mfxcommon.h $package_dir/include/mfx/mfxcommon.h'
'cp -P $headers_dir/include/mfxdefs.h $package_dir/include/mfx/mfxdefs.h'
'cp -P $headers_dir/include/mfxdispatcherprefixedfunctions.h $package_dir/include/mfx/mfxdispatcherprefixedfunctions.h'
'cp -P $headers_dir/include/mfxenc.h $package_dir/include/mfx/mfxenc.h'
'cp -P $headers_dir/include/mfxenctools.h $package_dir/include/mfx/mfxenctools.h'
'cp -P $headers_dir/include/mfxfei.h $package_dir/include/mfx/mfxfei.h'
'cp -P $headers_dir/include/mfxfeih265.h $package_dir/include/mfx/mfxfeih265.h'
'cp -P $headers_dir/include/mfxfeihevc.h $package_dir/include/mfx/mfxfeihevc.h'
'cp -P $headers_dir/include/mfxhdcp.h $package_dir/include/mfx/mfxhdcp.h'
'cp -P $headers_dir/include/mfxjpeg.h $package_dir/include/mfx/mfxjpeg.h'
'cp -P $headers_dir/include/mfxla.h $package_dir/include/mfx/mfxla.h'
'cp -P $headers_dir/include/mfxmvc.h $package_dir/include/mfx/mfxmvc.h'
'cp -P $headers_dir/include/mfxpak.h $package_dir/include/mfx/mfxpak.h'
'cp -P $headers_dir/include/mfxpcp.h $package_dir/include/mfx/mfxpcp.h'
'cp -P $headers_dir/include/mfxplugin++.h $package_dir/include/mfx/mfxplugin++.h'
'cp -P $headers_dir/include/mfxplugin.h $package_dir/include/mfx/mfxplugin.h'
'cp -P $headers_dir/include/mfxplugin_internal.h $package_dir/include/mfx/mfxplugin_internal.h'
'cp -P $headers_dir/include/mfxprivate.h $package_dir/include/mfx/mfxprivate.h'
'cp -P $headers_dir/include/mfxsc.h $package_dir/include/mfx/mfxsc.h'
'cp -P $headers_dir/include/mfxscd.h $package_dir/include/mfx/mfxscd.h'
'cp -P $headers_dir/include/mfxsession.h $package_dir/include/mfx/mfxsession.h'
'cp -P $headers_dir/include/mfxstructures.h $package_dir/include/mfx/mfxstructures.h'
'cp -P $headers_dir/include/mfxvideo++.h $package_dir/include/mfx/mfxvideo++.h'
'cp -P $headers_dir/include/mfxvideo.h $package_dir/include/mfx/mfxvideo.h'
'cp -P $headers_dir/include/mfxvp8.h $package_dir/include/mfx/mfxvp8.h'
'cp -P $headers_dir/include/mfxvp9.h $package_dir/include/mfx/mfxvp9.h'
'cp -P $headers_dir/include/mfxvstructures.h $package_dir/include/mfx/mfxvstructures.h'
'cp -P $headers_dir/include/mfxwidi.h $package_dir/include/mfx/mfxwidi.h'
)

# Create arg in cmd for API header location
VERSION_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VERSION_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')

package_name="I_MSDK_p_1234"
tmp_dir=${path_to_save}/tmp_drop_linux_lib_files
package_dir=${tmp_dir}/${package_name}/I_MSDK

DIRECTORIES=(
$package_dir/share/mfx/samples
$package_dir/lib64/mfx
$package_dir/share/mfx
$package_dir/lib64/pkgconfig
$package_dir/bin
$package_dir/include/mfx
)

mkdir -p ${DIRECTORIES[*]}

for command in "${COPY_CMDS[@]}"
  do
    eval $command
  done

tar -cvzf ${path_to_save}/${package_name}.tgz -C "${package_dir%/*}" .
rm -rf $tmp_dir