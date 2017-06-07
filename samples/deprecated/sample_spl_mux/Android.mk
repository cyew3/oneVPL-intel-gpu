LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(addprefix dispatcher/src/, $(notdir $(wildcard $(LOCAL_PATH)/dispatcher/src/*.c)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_common/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/deprecated/sample_spl_mux/api \
    $(MFX_HOME)/mdp_msdk-lib/samples/deprecated/sample_spl_mux/dispatcher/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libsample_spl_mux_dispatcher

include $(BUILD_STATIC_LIBRARY)
