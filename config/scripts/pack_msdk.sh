#!/bin/bash
set -ex

while getopts b:t:h: flag
do
    case "${flag}" in
        b) binaries_dir=${OPTARG};;
		t) test_behavior=${OPTARG};;
		h) hevce_tests=${OPTARG};;
    esac
done

cp -v $test_behavior/test_behavior $binaries_dir
cd $binaries_dir && rm -rf libmfx.* mfx.*

package_dir=/opt/src/output/to_deb_pkg

mkdir -p $package_dir/usr/local/lib/x86_64-linux-gnu && mv -fv $binaries_dir/*.* $_
mkdir -p $package_dir/usr/local/bin && mv -fv $binaries_dir/* $_

cp -rv /opt/src/mediasdk/config/scripts/DEBIAN $package_dir

mkdir -p /opt/src/output/deb && dpkg-deb --build $package_dir $_
