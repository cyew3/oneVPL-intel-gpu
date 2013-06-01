LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

ifeq ($(MFX_ANDROID_NDK_BUILD), true)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/test_thread_safety/include \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/mfx_transcoder/include \
    $(MFX_HOME)/_studio/mfx_dispatch/include \
    $(MFX_HOME)/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_LDFLAGS += -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l -ldl

ifeq ($(MFX_IMPL), hw)
    LOCAL_LDFLAGS += -lva -lva-android
endif

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline_$(MFX_IMPL) \
    libmfx_pipeline_$(MFX_IMPL) \
    libshared_utils \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged

LOCAL_MODULE_TAGS := optional
ifeq ($(MFX_IMPL), sw)
    LOCAL_MODULE := test_thread_safety_$(MFX_IMPL)
else
    LOCAL_MODULE := test_thread_safety
endif

include $(BUILD_EXECUTABLE)

# =============================================================================

endif # ifeq ($(MFX_ANDROID_NDK_BUILD), true)
