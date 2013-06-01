ifeq ($(MFX_ANDROID_NDK_BUILD), false)

LOCAL_CFLAGS += -fexceptions

## Variant with stlport & gabi++
LOCAL_C_INCLUDES += \
    ndk/sources/cxx-stl/stlport/stlport

## Variant with llvm
#LOCAL_C_INCLUDES += \
#    ndk/sources/cxx-stl/llvm-libc++/include \
#    ndk/sources/cxx-stl/llvm-libc++/include/support/android \
#    $(LOCAL_C_INCLUDES)
#LOCAL_CFLAGS += -std=c++11

## Variant with prebuilt gnustl
#LOCAL_C_INCLUDES := \
#    prebuilts/ndk/8/sources/cxx-stl/gnu-libstdc++/include \
#    prebuilts/ndk/8/sources/cxx-stl/gnu-libstdc++/libs/x86/include \
#    $(LOCAL_C_INCLUDES)

endif
