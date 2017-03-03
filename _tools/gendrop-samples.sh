#!/bin/bash
# - $1 - reference mediasdk tree
# - $2 - where to gen drop

mkdir $2

DIRS_TO_COPY=" \
  samples/sample_common \
  samples/sample_decode \
  samples/sample_encode \
  samples/sample_vpp \
  samples/sample_videoconf \
  samples/sample_user_modules \
  samples/sample_spl_mux \
  samples/sample_utilities \
  samples/sample_full_transcode \
  samples/sample_win8ui_transcode \
  samples/sample_dshow_player \
  samples/sample_dshow_plugins \
  samples/sample_mfoundation_plugins \
  samples/sample_mfoundation_transcode \
  "

FILES_TO_COPY=" \
  samples/CMakeLists.txt \
  samples/set_INTELMEDIASDK_WINSDK_PATH.bat \
  samples/samples.sln
  samples/Android.mk
  "

PATHS_TO_REMOVE=" \
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
# misc...
find . -name "*.filters" | xargs rm -rf
find . -name "*.pptx" | xargs rm -rf
find . -name "*.rtf" | xargs rm -rf
find . -name "*.vsd" | xargs rm -rf
find . -name "*.pdf" | xargs rm -rf
find . -name "*.pl" | xargs rm -rf
find . -name "*.pm" | xargs rm -rf
find . -name "*.html" | xargs rm -rf
find . -name ".bdsignore" | xargs rm -rf
find . -name "*.kate-swp" | xargs rm -rf
find . -name "*.rej" | xargs rm -rf
find . -name "*.orig" | xargs rm -rf
find . -name "*~" | xargs rm -rf

find . -name "*.h" | xargs dos2unix
find . -name "*.hpp" | xargs dos2unix
find . -name "*.c" | xargs dos2unix
find . -name "*.cpp" | xargs dos2unix
find . -name "*.cl" | xargs dos2unix
find . -name "*.cs" | xargs dos2unix
find . -name "*.txt" | xargs unix2dos
