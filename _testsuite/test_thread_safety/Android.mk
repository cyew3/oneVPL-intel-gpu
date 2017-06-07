LOCAL_PATH:= $(call my-dir)

ifneq ($(MFX_IMPL),)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL)  \
    $(MFX_CFLAGS_LIBVA)

LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/test_thread_safety/include \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/mfx_player/include \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/mfx_transcoder/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_dispatch/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/shared/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_common/include

LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline \
    libmfx_pipeline \
    libshared_utils \
    libsample_common \
    libsample_spl_mux_dispatcher \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged \
    libmfx_trace_$(MFX_IMPL) \
    libsafec \
    libippvc_l \
    libippcc_l \
    libippcp_l \
    libippdc_l \
    libippi_l \
    libipps_l \
    libippcore_l

LOCAL_SHARED_LIBRARIES := libdl libva libva-android

ifeq ($(MFX_NDK),true)
   LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := test_thread_safety_$(MFX_IMPL)

include $(BUILD_EXECUTABLE)

endif # ($(MFX_IMPL),)

# =============================================================================
