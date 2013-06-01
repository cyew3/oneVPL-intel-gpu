#!/bin/bash
# - $1 - reference mediasdk tree
# - $2 - where to gen drop

mkdir $2

cp -rd $1/Android.mk $2/

cp -rd $1/include $2/
cp -rd $1/cxx $2/
cp -rd $1/opensource $2/

cp -rd $1/android $2/
rm -rf $2/android/install.sh
rm -rf $2/android/scripts
rm -rf $2/android/patches
rm -rf $2/android/droid_tree

mkdir $2/_studio
cp -rd $1/_studio/Android.mk $2/_studio/
cp -rd $1/_studio/mfx_lib $2/_studio/
rm -rf $2/_studio/mfx_lib/vpp/include/videovme*.h
rm -rf $2/_studio/mfx_lib/vpp/include/meforgen7_5.h
rm -rf $2/_studio/mfx_lib/vpp/src/vme

mkdir $2/_studio/shared
cp -rd $1/_studio/shared/Android.mk $2/_studio/shared/
cp -rd $1/_studio/shared/include $2/_studio/shared/
cp -rd $1/_studio/shared/src $2/_studio/shared/
cp -rd $1/_studio/shared/mfx_trace $2/_studio/shared/

mkdir $2/_studio/shared/umc
cp -rd $1/_studio/shared/umc/Android.mk $2/_studio/shared/umc/
cp -rd $1/_studio/shared/umc/core $2/_studio/shared/umc/
cp -rd $1/_studio/shared/umc/codec $2/_studio/shared/umc/
cp -rd $1/_studio/shared/umc/io $2/_studio/shared/umc/

mkdir $2/_studio/shared/umc/test_suite
#cp -rd $1/_studio/shared/umc/test_suite/Android.mk $2/_studio/shared/umc/test_suite/
cp -rd $1/_studio/shared/umc/test_suite/spy_test_component $2/_studio/shared/umc/test_suite/
rm -rf $2/_studio/shared/umc/test_suite/spy_test_component/outline.*

mkdir $2/_testsuite
cp -rd $1/_testsuite/Android.mk $2/_testsuite/
cp -rd $1/_testsuite/mfx_player $2/_testsuite/
rm -rf $2/_testsuite/mfx_player/scripts
rm -rf $2/_testsuite/mfx_player/props
cp -rd $1/_testsuite/mfx_transcoder $2/_testsuite/
cp -rd $1/_testsuite/shared $2/_testsuite/

mkdir $2/samples
cp -rd $1/samples/Android.mk $2/samples/
cp -rd $1/samples/sample_common $2/samples/
cp -rd $1/samples/sample_decode $2/samples/
cp -rd $1/samples/sample_encode $2/samples/
cp -rd $1/samples/sample_openmax_plugins $2/samples/
rm -rf $2/samples/sample_openmax_plugins/omx_stagefright

mkdir $2/samples/sample_user_modules
#cp -rd $1/samples/sample_user_modules/Android.mk $2/samples/sample_user_modules/
cp -rd $1/samples/sample_user_modules/plugin_api $2/samples/sample_user_modules/

cd $2
find . -name ".svn" | xargs rm -rf
# windows build system files
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
find . -name "*~" | xargs rm -rf

find . -name "*.h" | xargs dos2unix
find . -name "*.hpp" | xargs dos2unix
find . -name "*.c" | xargs dos2unix
find . -name "*.cpp" | xargs dos2unix
