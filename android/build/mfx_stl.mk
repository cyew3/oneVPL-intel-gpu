# Purpose:
#   Defines STL support flags, include paths, etc.
#
# Defined variables:
#   MFX_CFLAGS_STL - cflags for STL support
#   MFX_C_INCLUDES_STL - include paths for STL support

MFX_CFLAGS_STL := -fexceptions -frtti

## Variant with stlport & gabi++
MFX_C_INCLUDES_STL := \
    ndk/sources/cxx-stl/stlport/stlport

## Variant with llvm
#MFX_C_INCLUDES_STL := \
#    ndk/sources/cxx-stl/llvm-libc++/include \
#    ndk/sources/cxx-stl/llvm-libc++/include/support/android
#MFX_CFLAGS_STL += -std=c++11

## Variant with prebuilt gnustl
#MFX_C_INCLUDES_STL := \
#    prebuilts/ndk/8/sources/cxx-stl/gnu-libstdc++/include \
#    prebuilts/ndk/8/sources/cxx-stl/gnu-libstdc++/libs/x86/include
