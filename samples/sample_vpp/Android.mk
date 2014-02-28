LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_LDFLAGS += -ldl

LOCAL_STATIC_LIBRARIES += libsample_common libmfx

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := sample_vpp

ifeq ($(MFX_IMPL), hw)

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libva libva-android libstlport-mfx libgabi++-mfx 
    else
        LOCAL_LDFLAGS += -lva -lva-android
    endif

else

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES :=  libstlport-mfx libgabi++-mfx
    endif

endif

include $(BUILD_EXECUTABLE)

