#!/bin/bash
# - $1 - reference mediasdk tree
# - $2 - where to gen drop

mkdir $2

DIRS_TO_COPY=" \
  include \
  cxx \
  opensource \
  android \
  _studio/mfx_lib \
  _studio/shared/include \
  _studio/shared/src \
  _studio/shared/mfx_trace \
  _studio/shared/umc/core \
  _studio/shared/umc/codec \
  _studio/shared/umc/io \
  _studio/shared/umc/test_suite/spy_test_component \
  _testsuite/mfx_player \
  _testsuite/mfx_transcoder \
  _testsuite/shared \
  samples/sample_common \
  samples/sample_decode \
  samples/sample_encode \
  samples/sample_user_modules/plugin_api \
  "

FILES_TO_COPY=" \
  Android.mk \
  _studio/Android.mk \
  _studio/shared/Android.mk \
  _studio/shared/umc/Android.mk
  _testsuite/Android.mk \
  samples/Android.mk \
  "

PATHS_TO_REMOVE=" \
  android/install.sh \
  android/scripts \
  android/patches \
  android/droid_tree \
  _studio/mfx_lib/audio_decode \
  _studio/mfx_lib/audio_encode \
  _studio/mfx_lib/genx/h264_encode/src/embed_isa.c \
  _studio/mfx_lib/genx/h264_encode/src/genx_hsw_simple_me.cpp \
  _studio/mfx_lib/genx/h264_encode/src/genx_hsw_simple_me_proto.cpp \
  _studio/mfx_lib/vpp/include/videovme*.h \
  _studio/mfx_lib/vpp/include/meforgen7_5.h \
  _studio/mfx_lib/vpp/src/vme \
  _studio/shared/umc/test_suite/spy_test_component/outline.* \
  _testsuite/mfx_player/scripts \
  _testsuite/mfx_player/props \
  "

for i in $DIRS_TO_COPY
do
  mkdir -p $2/$i
  cp -rd $1/$i/* $2/$i/
done

for i in $FILES_TO_COPY
do
  cp $1/$i $2/$i
done

for i in $PATHS_TO_REMOVE
do
  rm -rf $2/$i
done

cd $2
find . -name ".svn" | xargs rm -rf
# Windows build system files
find . -name "*.vcproj*" | xargs rm -rf
find . -name "*.vcxproj*" | xargs rm -rf
find . -name "*.sln" | xargs rm -rf
# Linux build system files (CMakeLists.txt) and other files
find . -name "*.txt" | xargs rm -rf
find . -name "SConscript*" | xargs rm -rf
# misc...
find . -name "*.rtf" | xargs rm -rf
find . -name "*.def" | xargs rm -rf
find . -name "*.pl" | xargs rm -rf
find . -name "*.pm" | xargs rm -rf
find . -name "*.bat" | xargs rm -rf
find . -name "*.html" | xargs rm -rf
find . -name "*.rc" | xargs rm -rf
find . -name "*.props" | xargs rm -rf
find . -name ".bdsignore" | xargs rm -rf
find . -name "*.kate-swp" | xargs rm -rf
find . -name "*.rej" | xargs rm -rf
find . -name "*.orig" | xargs rm -rf
find . -name "*~" | xargs rm -rf

find . -name "*.h" | xargs dos2unix
find . -name "*.hpp" | xargs dos2unix
find . -name "*.c" | xargs dos2unix
find . -name "*.cpp" | xargs dos2unix
