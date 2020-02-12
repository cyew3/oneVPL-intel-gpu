LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))) \
    $(addprefix common/src/, $(notdir $(wildcard $(LOCAL_PATH)/common/src/*.cpp)))

LOCAL_C_INCLUDES := \
    $(MFX_INCLUDES_INTERNAL) \
    $(MFX_INCLUDES_LIBVA) \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/shared/include \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/test_vpp/common/include \

LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_LIBVA)

LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_LDFLAGS := $(MFX_LDFLAGS)

LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

LOCAL_STATIC_LIBRARIES := \
    libmfx \
    libshared_utils \
    libumc_core_merged \
    libippdc_l \
    libippcore_l \
    libippcc_l

LOCAL_SHARED_LIBRARIES := libdl libva libva-android

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := test_vpp
LOCAL_MODULE_STEM_32 := test_vpp32
LOCAL_MODULE_STEM_64 := test_vpp64

include $(BUILD_EXECUTABLE)
