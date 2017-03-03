LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_C_INCLUDES_STL) \
    $(MFX_HOME)/samples/sample_common/include \
    $(MFX_HOME)/samples/sample_user_modules/plugin_api

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_LIBVA) \
    $(MFX_CFLAGS_STL)

LOCAL_STATIC_LIBRARIES += libsample_common libmfx
LOCAL_SHARED_LIBRARIES := libdl libva libva-android

ifeq ($(LOCAL_CXX_STL),)
   LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := sample_mondello

include $(BUILD_EXECUTABLE)
