#!/bin/bash
set -ex

# TODO: remove hardcoded path to lib repository
MSDK_MINOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MINOR " '$2 ~ /^[0-9]+$/ { print $2 }')
MSDK_MAJOR=$(cat /opt/src/sources/mdp_msdk-lib/api/include/mfxdefs.h | awk -F "MFX_VERSION_MAJOR " '$2 ~ /^[0-9]+$/ { print $2 }')
RELEASE_VERSION=$(cat /opt/src/sources/mdp_msdk-lib/_studio/product.ver)

BIN_FILES=(
    "mfx_player"
    "mfx_transcoder"
    "msdk_gmock"
    "sample_encode"
    "sample_multi_transcode"
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

package_name="vpl_tools_drop_$RELEASE_VERSION.${build_number##*.}.zip"
package_dir=$path_to_save/to_drop

mkdir -p $package_dir

unzip $msdk_1x_archive -d $package_dir

cd $binaries_dir
cp -Pv ${BIN_FILES[*]} $package_dir/imports/mediasdk

# Needed to publish artifacts in QuickBuild, сopying each file causes an error
# zip cannot pack relative paths
cd $package_dir && zip -r $path_to_save/$package_name *
cd $path_to_save && rm -r $package_dir
