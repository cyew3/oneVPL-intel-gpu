LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

MFX_TRANSODER_CFLAGS += \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA)

MFX_TRANSODER_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_CFLAGS += $(MFX_TRANSODER_CFLAGS)
LOCAL_C_INCLUDES += $(MFX_TRANSODER_INCLUDES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_CFLAGS += \
    $(MFX_TRANSODER_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_C_INCLUDES += $(MFX_TRANSODER_INCLUDES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline_lucas

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_CFLAGS += $(MFX_TRANSODER_CFLAGS)
LOCAL_C_INCLUDES += $(MFX_TRANSODER_INCLUDES)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline \
    libmfx_pipeline \
    libshared_utils \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged

LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_transcoder

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_CFLAGS += \
    $(MFX_TRANSODER_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_C_INCLUDES += $(MFX_TRANSODER_INCLUDES)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline_lucas \
    libmfx_pipeline_lucas \
    libshared_utils_lucas \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged

LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_transcoder_pipeline

include $(BUILD_SHARED_LIBRARY)
