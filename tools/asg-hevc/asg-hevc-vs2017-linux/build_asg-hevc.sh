
#!/bin/bash


if [ "$1" != "debug" ] && [ "$1" != "release" ] ; then

    echo "$1: provide build configuration"

    exit 1
fi


export MFX_HOME=`pwd`

export MEDIASDK_ROOT=/msdk/MEDIASDK_ROOT

export MFX_MDF_PATH=$MEDIASDK_ROOT/cmrt_linux

export LIBVA_REV=${LIBVA_REV:-"va-unified"}

export CMAKE_VTUNE_HOME=$MFX_HOME/mdp_msdk-contrib/itt

export INTELMEDIASDK_FFMPEG_ROOT=$MEDIASDK_ROOT/ffmpeg

export PKG_CONFIG_PATH=$MFX_HOME/mdp_msdk-contrib/libva/$LIBVA_REV:$PKG_CONFIG_PATH

export PKG_CONFIG_TOP_BUILD_DIR=$PKG_CONFIG_PATH


cp mdp_msdk-tools/builder/CMakeLists.txt ./

mkdir -p bin/lin_x64


if [[ ! -d "__cmake" ]]; then

    perl mdp_msdk-tools/builder/build_mfx.pl --cmake=intel64.make.$1

fi

make -C __cmake/intel64.make.$1 -j8 asg-hevc

status=$?

if [[ $status -ne 0 ]] ; then

    exit $status

fi

cp -u __cmake/intel64.make.$1/__bin/$1/asg-hevc bin/lin_x64/asg-hevc
