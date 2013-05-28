LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/opensource/mfx_dispatch/include \
    $(MFX_HOME)/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_player.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_LDFLAGS += -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l -ldl

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_pipeline_$(MFX_IMPL) \
    libshared_utils \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged

LOCAL_MODULE_TAGS := optional

ifeq ($(MFX_IMPL), sw)

    LOCAL_MODULE := mfx_player_$(MFX_IMPL)

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl
    else
        LOCAL_LDFLAGS += -ldl
    endif

else

    LOCAL_MODULE := mfx_player

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android
    else
        LOCAL_LDFLAGS += -ldl -lva -lva-android
    endif

endif


include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/opensource/mfx_dispatch/include \
    $(MFX_HOME)/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_CFLAGS += -DLUCAS_DLL

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline_lucas_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)

# =============================================================================
include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_player.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_CFLAGS += -DLUCAS_DLL

LOCAL_LDFLAGS += -lippvc_l -lippcc_l -lippcp_l -lippdc_l -lippi_l -lipps_l -lippcore_l -ldl

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_pipeline_lucas_$(MFX_IMPL) \
    libshared_utils_lucas \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged

ifeq ($(MFX_IMPL), sw)

    LOCAL_MODULE := mfx_player_pipeline_$(MFX_IMPL)

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl
    else
        LOCAL_LDFLAGS += -ldl
    endif

else

    LOCAL_MODULE := mfx_player_pipeline

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl libva libva-android
    else
        LOCAL_LDFLAGS += -ldl -lva -lva-android
    endif

endif

include $(BUILD_SHARED_LIBRARY)
