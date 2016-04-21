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
    $(MFX_HOME)/samples/samples_spl_mux/api

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_CFLAGS := $(MFX_TRANSODER_CFLAGS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)
LOCAL_C_INCLUDES := $(MFX_TRANSODER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_CFLAGS := \
    $(MFX_TRANSODER_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)
LOCAL_C_INCLUDES := $(MFX_TRANSODER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline_lucas

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_CFLAGS := $(MFX_TRANSODER_CFLAGS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)
LOCAL_C_INCLUDES := $(MFX_TRANSODER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_LDFLAGS := \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l
LOCAL_LDFLAGS_32 := $(MFX_LDFLAGS_INTERNAL_32)
LOCAL_LDFLAGS_64 := $(MFX_LDFLAGS_INTERNAL_64)

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline \
    libmfx_pipeline \
    libshared_utils \
    libdispatch_trace \
    libsample_spl_mux_dispatcher \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged \
    libsafec

LOCAL_SHARED_LIBRARIES := libdl libva libva-android
ifeq ($(MFX_NDK),true)
   LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_transcoder
LOCAL_MODULE_STEM_32 := mfx_transcoder32
LOCAL_MODULE_STEM_64 := mfx_transcoder64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_CFLAGS := \
    $(MFX_TRANSODER_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)
LOCAL_C_INCLUDES := $(MFX_TRANSODER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_LDFLAGS := \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l
LOCAL_LDFLAGS_32 := $(MFX_LDFLAGS_INTERNAL_32)
LOCAL_LDFLAGS_64 := $(MFX_LDFLAGS_INTERNAL_64)

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline_lucas \
    libmfx_pipeline_lucas \
    libshared_utils_lucas \
    libdispatch_trace \
    libsample_spl_mux_dispatcher \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged \
    libsafec

LOCAL_SHARED_LIBRARIES := libdl libva libva-android
ifeq ($(MFX_NDK),true)
   LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_transcoder_pipeline

include $(BUILD_SHARED_LIBRARY)
