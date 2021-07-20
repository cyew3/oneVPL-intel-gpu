#!/usr/bin/env bash
set -ex

mkdir -p $MFX_HOME/build && cd $MFX_HOME/build
# Workaround to get latest cmake on system
if [ -f /opt/tools/cmake-latest/bin/cmake ]; then CMAKE=/opt/tools/cmake-latest/bin/cmake; else CMAKE=cmake;fi
$CMAKE \
  -DCMAKE_INSTALL_PREFIX=/opt/intel/mediasdk \
  -DCMAKE_C_FLAGS_RELEASE="-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -DNDEBUG" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -DNDEBUG" \
  -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON \
  -DBUILD_VAL_TOOLS=ON -DENABLE_OPENCL=OFF $MFX_HOME

make -j$(nproc)
