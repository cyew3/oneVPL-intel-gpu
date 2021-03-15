#!/bin/bash
set -ex

# build number arg is optional; only last number of this arg will be used for versioning: 0.0.0 -> 0
build_number="0.0.0"
while getopts b:h:m:p:n: flag
do
    case "${flag}" in
        b) build_dir=${OPTARG};;
        h) headers_path=${OPTARG};;
        m) msdk_1x_archive=${OPTARG};;
        p) path_to_save=${OPTARG};;
        n) build_number=${OPTARG};;
    esac
done

# TODO: copy headers by file
# TODO: get mfx api for libmfx.so

declare -a COPY_CMDS=(
'cp -P $build_dir/__bin/release/libmfx-gen.so $package_dir/lib64/libmfx-gen.so'
'cp -P $build_dir/__bin/release/libmfx-gen.so.1.${VPL_MAJOR} $package_dir/lib64/libmfx-gen.so.1.${VPL_MAJOR}'
'cp -P $build_dir/__bin/release/libmfx-gen.so.1.${VPL_MAJOR}.${VPL_MINOR} $package_dir/lib64/libmfx-gen.so.1.${VPL_MAJOR}.${VPL_MINOR}'
'cp -P $build_dir/__lib/release/libmfx-gen.pc $package_dir/lib64/pkgconfig/libmfx-gen.pc'
'cp -P $build_dir/__bin/release/libvpl.so $package_dir/lib64/libvpl.so'
'cp -P $build_dir/__bin/release/libvpl.so.1.${VPL_MAJOR} $package_dir/lib64/libvpl.so.1.${VPL_MAJOR}'
'cp -P $build_dir/__bin/release/libvpl.so.1.${VPL_MAJOR}.${VPL_MINOR} $package_dir/lib64/libvpl.so.1.${VPL_MAJOR}.${VPL_MINOR}'
'cp -rP $headers_path/vpl $package_dir/include/'
'cp -rP $msdk_binaries/lib64/libmfx.so* $package_dir/lib64'
)

# Create arg in cmd for API header location
# TODO: remove hardcoded path to lib repository
VPL_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/vpl/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VPL_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/vpl/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')

RELEASE_VERSION=$(cat /opt/src/sources/mdp_msdk-lib/_studio/product.ver)

package_name="l_MSDK_p_$RELEASE_VERSION.${build_number##*.}_vpl"
tmp_dir=${path_to_save}/tmp_drop_linux_lib_files
package_dir=${tmp_dir}/${package_name}/l_MSDK
msdk_binaries=${tmp_dir}/msdk_binaries/l_MSDK

mkdir -p $package_dir/lib64/pkgconfig $package_dir/include $msdk_binaries

tar xvfz $msdk_1x_archive --directory=${tmp_dir}/msdk_binaries

for command in "${COPY_CMDS[@]}"
  do
    eval $command
  done

tar -cvzf ${path_to_save}/${package_name}.tgz -C "${tmp_dir}/msdk_binaries" l_MSDK
rm -rf $tmp_dir