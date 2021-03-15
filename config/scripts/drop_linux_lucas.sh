#!/bin/bash
set -ex

# TODO: remove hardcoded path to lib repository
VPL_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/vpl/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
VPL_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/vpl/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')

RELEASE_VERSION=$(cat /opt/src/sources/mdp_msdk-lib/_studio/product.ver)

VPL_FILES=(
    "libmfx-gen.so"
    "libmfx-gen.so.1.${VPL_MAJOR}"
    "libmfx-gen.so.1.${VPL_MAJOR}.${VPL_MINOR}"
)

# build number arg is optional; only last number of this arg will be used for versioning: 0.0.0 -> 0
build_number="0.0.0"
while getopts b:m:p:n: flag
do
    case "${flag}" in
        b) binaries_dir=${OPTARG};;
        m) msdk_1x_archive=${OPTARG};;
        p) path_to_save=${OPTARG};;
        n) build_number=${OPTARG};;
    esac
done

package_name="lucas_linux_drop_$RELEASE_VERSION.${build_number##*.}.zip"
package_dir=$path_to_save/lucas_drop

mkdir -p $package_dir

unzip $msdk_1x_archive -d $package_dir

cd $binaries_dir
cp -Pv ${VPL_FILES[*]} $package_dir/imports/mediasdk

# Needed to publish artifacts in QuickBuild, сopying each file causes an error
cd $package_name && zip -r $path_to_save/$package_name *
cd $path_to_save && rm -r $package_dir