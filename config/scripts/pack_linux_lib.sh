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
'cp -P $build_dir/mediasdk/__bin/release/asg-hevc $package_dir/opt/intel/mediasdk/bin/asg-hevc'
'cp -P $build_dir/mediasdk/__bin/release/hevc_fei_extractor $package_dir/opt/intel/mediasdk/bin/hevc_fei_extractor'
'cp -P $build_dir/mediasdk/__bin/release/metrics_calc_lite $package_dir/opt/intel/mediasdk/bin/metrics_calc_lite'
'cp -P $build_dir/mediasdk/__bin/release/mfx-tracer-config $package_dir/opt/intel/mediasdk/bin/mfx-tracer-config'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so $package_dir/opt/intel/mediasdk/lib64/libmfx-tracer.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so.${VERSION_MAJOR} $package_dir/opt/intel/mediasdk/lib64/libmfx-tracer.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx-tracer.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/opt/intel/mediasdk/lib64/libmfx-tracer.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so $package_dir/opt/intel/mediasdk/lib64/libmfx.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so.${VERSION_MAJOR} $package_dir/opt/intel/mediasdk/lib64/libmfx.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/opt/intel/mediasdk/lib64/libmfx.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so $package_dir/opt/intel/mediasdk/lib64/libmfxhw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so.${VERSION_MAJOR} $package_dir/opt/intel/mediasdk/lib64/libmfxhw64.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxhw64.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/opt/intel/mediasdk/lib64/libmfxhw64.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so $package_dir/opt/intel/mediasdk/lib64/libmfxsw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so.${VERSION_MAJOR} $package_dir/opt/intel/mediasdk/lib64/libmfxsw64.so.${VERSION_MAJOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR} $package_dir/opt/intel/mediasdk/lib64/libmfxsw64.so.${VERSION_MAJOR}.${VERSION_MINOR}'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_h264la_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_h264la_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevcd_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_hevcd_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevce_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_hevce_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_hevc_fei_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_hevc_fei_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp8d_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_vp8d_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp9d_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_vp9d_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_vp9e_hw64.so $package_dir/opt/intel/mediasdk/lib64/mfx/libmfx_vp9e_hw64.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx.pc $package_dir/opt/intel/mediasdk/lib64/pkgconfig/libmfx.pc'
'cp -P $build_dir/mediasdk/__lib/release/libmfxhw64.pc $package_dir/opt/intel/mediasdk/lib64/pkgconfig/libmfxhw64.pc'
'cp -P $build_dir/mediasdk/__lib/release/libmfxsw64.pc $package_dir/opt/intel/mediasdk/lib64/pkgconfig/libmfxsw64.pc'
'cp -P $build_dir/mediasdk/__bin/release/mfx.pc $package_dir/opt/intel/mediasdk/lib64/pkgconfig/mfx.pc'
'cp -P $build_dir/mediasdk/plugins.cfg $package_dir/opt/intel/mediasdk/share/mfx/plugins.cfg'
'cp -P $build_dir/mediasdk/__bin/release/libcttmetrics.so $package_dir/opt/intel/mediasdk/share/mfx/samples/libcttmetrics.so'
'cp -P $build_dir/mediasdk/__bin/release/libmfx_wayland.so $package_dir/opt/intel/mediasdk/share/mfx/samples/libmfx_wayland.so'
'cp -P $build_dir/mediasdk/__bin/release/libsample_rotate_plugin.so $package_dir/opt/intel/mediasdk/share/mfx/samples/libsample_rotate_plugin.so'
'cp -P $build_dir/mediasdk/__lib/release/libvpp_plugin.a $package_dir/opt/intel/mediasdk/share/mfx/samples/libvpp_plugin.a'
'cp -P $build_dir/mediasdk/__bin/release/metrics_monitor $package_dir/opt/intel/mediasdk/share/mfx/samples/metrics_monitor'
'cp -P $build_dir/mediasdk/__bin/release/sample_decode $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_decode'
'cp -P $build_dir/mediasdk/__bin/release/sample_encode $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_encode'
'cp -P $build_dir/mediasdk/__bin/release/sample_encode_mod $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_encode_mod'
'cp -P $build_dir/mediasdk/__bin/release/sample_fei $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_fei'
'cp -P $build_dir/mediasdk/__bin/release/sample_hevc_fei $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_hevc_fei'
'cp -P $build_dir/mediasdk/__bin/release/sample_hevc_fei_abr $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_hevc_fei_abr'
'cp -P $build_dir/mediasdk/__bin/release/sample_multi_transcode $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_multi_transcode'
'cp -P $build_dir/mediasdk/__bin/release/sample_multi_transcode_mod $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_multi_transcode_mod'
'cp -P $build_dir/mediasdk/__bin/release/sample_vpp $package_dir/opt/intel/mediasdk/share/mfx/samples/sample_vpp'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so $package_dir/opt/intel/mediasdk/lib64/libmfx-gen.so'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so.1.2 $package_dir/opt/intel/mediasdk/lib64/libmfx-gen.so.1.2'
'cp -P $build_dir/vpl/__bin/release/libmfx-gen.so.1.2.1 $package_dir/opt/intel/mediasdk/lib64/libmfx-gen.so.1.2.1'
'cp -P $headers_dir/include/mfxadapter.h $package_dir/opt/intel/mediasdk/include/mfx/mfxadapter.h'
'cp -P $headers_dir/include/mfxastructures.h $package_dir/opt/intel/mediasdk/include/mfx/mfxastructures.h'
'cp -P $headers_dir/include/mfxaudio++.h $package_dir/opt/intel/mediasdk/include/mfx/mfxaudio++.h'
'cp -P $headers_dir/include/mfxaudio.h $package_dir/opt/intel/mediasdk/include/mfx/mfxaudio.h'
'cp -P $headers_dir/include/mfxbrc.h $package_dir/opt/intel/mediasdk/include/mfx/mfxbrc.h'
'cp -P $headers_dir/include/mfxcamera.h $package_dir/opt/intel/mediasdk/include/mfx/mfxcamera.h'
'cp -P $headers_dir/include/mfxcommon.h $package_dir/opt/intel/mediasdk/include/mfx/mfxcommon.h'
'cp -P $headers_dir/include/mfxdefs.h $package_dir/opt/intel/mediasdk/include/mfx/mfxdefs.h'
'cp -P $headers_dir/include/mfxdispatcherprefixedfunctions.h $package_dir/opt/intel/mediasdk/include/mfx/mfxdispatcherprefixedfunctions.h'
'cp -P $headers_dir/include/mfxenc.h $package_dir/opt/intel/mediasdk/include/mfx/mfxenc.h'
'cp -P $headers_dir/include/mfxenctools.h $package_dir/opt/intel/mediasdk/include/mfx/mfxenctools.h'
'cp -P $headers_dir/include/mfxfei.h $package_dir/opt/intel/mediasdk/include/mfx/mfxfei.h'
'cp -P $headers_dir/include/mfxfeih265.h $package_dir/opt/intel/mediasdk/include/mfx/mfxfeih265.h'
'cp -P $headers_dir/include/mfxfeihevc.h $package_dir/opt/intel/mediasdk/include/mfx/mfxfeihevc.h'
'cp -P $headers_dir/include/mfxhdcp.h $package_dir/opt/intel/mediasdk/include/mfx/mfxhdcp.h'
'cp -P $headers_dir/include/mfxjpeg.h $package_dir/opt/intel/mediasdk/include/mfx/mfxjpeg.h'
'cp -P $headers_dir/include/mfxla.h $package_dir/opt/intel/mediasdk/include/mfx/mfxla.h'
'cp -P $headers_dir/include/mfxmvc.h $package_dir/opt/intel/mediasdk/include/mfx/mfxmvc.h'
'cp -P $headers_dir/include/mfxpak.h $package_dir/opt/intel/mediasdk/include/mfx/mfxpak.h'
'cp -P $headers_dir/include/mfxpcp.h $package_dir/opt/intel/mediasdk/include/mfx/mfxpcp.h'
'cp -P $headers_dir/include/mfxplugin++.h $package_dir/opt/intel/mediasdk/include/mfx/mfxplugin++.h'
'cp -P $headers_dir/include/mfxplugin.h $package_dir/opt/intel/mediasdk/include/mfx/mfxplugin.h'
'cp -P $headers_dir/include/mfxplugin_internal.h $package_dir/opt/intel/mediasdk/include/mfx/mfxplugin_internal.h'
'cp -P $headers_dir/include/mfxprivate.h $package_dir/opt/intel/mediasdk/include/mfx/mfxprivate.h'
'cp -P $headers_dir/include/mfxsc.h $package_dir/opt/intel/mediasdk/include/mfx/mfxsc.h'
'cp -P $headers_dir/include/mfxscd.h $package_dir/opt/intel/mediasdk/include/mfx/mfxscd.h'
'cp -P $headers_dir/include/mfxsession.h $package_dir/opt/intel/mediasdk/include/mfx/mfxsession.h'
'cp -P $headers_dir/include/mfxstructures.h $package_dir/opt/intel/mediasdk/include/mfx/mfxstructures.h'
'cp -P $headers_dir/include/mfxvideo++.h $package_dir/opt/intel/mediasdk/include/mfx/mfxvideo++.h'
'cp -P $headers_dir/include/mfxvideo.h $package_dir/opt/intel/mediasdk/include/mfx/mfxvideo.h'
'cp -P $headers_dir/include/mfxvp8.h $package_dir/opt/intel/mediasdk/include/mfx/mfxvp8.h'
'cp -P $headers_dir/include/mfxvp9.h $package_dir/opt/intel/mediasdk/include/mfx/mfxvp9.h'
'cp -P $headers_dir/include/mfxvstructures.h $package_dir/opt/intel/mediasdk/include/mfx/mfxvstructures.h'
'cp -P $headers_dir/include/mfxwidi.h $package_dir/opt/intel/mediasdk/include/mfx/mfxwidi.h'
)


# Create arg in cmd for API header location
VERSION_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VERSION_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')

package_dir=$path_to_save/tmp_linux_lib_files
package_name="mediasdk"

DIRECTORIES=(
$package_dir/opt/intel/mediasdk/share/mfx/samples
$package_dir/opt/intel/mediasdk/include/mfx
$package_dir/opt/intel/mediasdk/lib64/mfx
$package_dir/opt/intel/mediasdk/bin
$package_dir/opt/intel/mediasdk/lib64/pkgconfig
$package_dir/DEBIAN
)

mkdir -p ${DIRECTORIES[*]}

for command in "${COPY_CMDS[@]}"
  do
    eval $command
  done

echo "Package: $package_name
Version: 11.0.$BUILD_NUMBER
Section: default
Priority: optional
Architecture: amd64
Description: no description" > $package_dir/DEBIAN/control

dpkg-deb --build $package_dir $path_to_save
rm -rf $package_dir
