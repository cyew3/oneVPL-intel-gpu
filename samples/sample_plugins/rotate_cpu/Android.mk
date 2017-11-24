LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_STL) \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_common/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_plugins/rotate_cpu/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_LIBVA) \
    $(MFX_CFLAGS_STL)

LOCAL_STATIC_LIBRARIES += libsample_common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libsample_rotate_cpu

include $(BUILD_STATIC_LIBRARY)
