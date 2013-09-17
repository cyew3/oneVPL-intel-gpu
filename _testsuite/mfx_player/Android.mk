LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

MFX_PIPELINE_INCLUDES := \
    $(MFX_HOME)/opensource/mfx_dispatch/include \
    $(MFX_HOME)/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

MFX_PLAYER_INCLUDES := \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_PIPELINE_INCLUDES)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_PIPELINE_INCLUDES)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA) \
    $(MFX_CFLAGS_LUCAS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline_lucas

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_player.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_PLAYER_INCLUDES)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_pipeline \
    libshared_utils \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged
LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_player

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_player.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_PLAYER_INCLUDES)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA) \
    $(MFX_CFLAGS_LUCAS)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS_INTERNAL) \
    -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_pipeline_lucas \
    libshared_utils_lucas \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged
LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_player_pipeline

include $(BUILD_SHARED_LIBRARY)
