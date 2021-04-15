#!/usr/bin/env bash
set -ex

mkdir -p $MFX_HOME/build && cd $MFX_HOME/build
/opt/tools/cmake-latest/bin/cmake \
  -DCMAKE_INSTALL_PREFIX=/opt/intel/mediasdk \
  -DCMAKE_C_FLAGS_RELEASE="-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -DNDEBUG" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -DNDEBUG" \
  -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON \
  -DBUILD_VAL_TOOLS=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.3 $MFX_HOME

make -j$(nproc) mfx-gen mfx_player mfx_transcoder msdk_gmock sample_multi_transcode sample_encode
