LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

ifeq ($(MFX_ANDROID_NDK_BUILD), true)

# =============================================================================

include $(CLEAR_VARS)
 
include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk
 
LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))
 
LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include 

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_LDFLAGS += -lippvc_l -lippcc_l -lippdc_l -lippi_l -lipps_l -lippcore_l -ldl

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
    LOCAL_MODULE := mfx_transcoder_$(MFX_IMPL)
else
    LOCAL_MODULE := mfx_transcoder
endif

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
 
include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk
 
LOCAL_SRC_FILES := $(filter-out src/mfx_transcoder.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))
 
LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include 

LOCAL_CFLAGS += -DLUCAS_DLL

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_trans_pipeline_lucas_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    mfx_transcoder.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_HOME)/_testsuite/mfx_player/include \
    $(MFX_HOME)/_testsuite/shared/include \
    $(MFX_HOME)/samples/sample_common/include

LOCAL_CFLAGS += -DLUCAS_DLL

LOCAL_LDFLAGS += -lippvc_l -lippcc_l -lippdc_l -lippi_l -lipps_l -lippcore_l -ldl

ifeq ($(MFX_IMPL), hw)
    LOCAL_LDFLAGS += -lva -lva-android
endif

LOCAL_STATIC_LIBRARIES += \
    libmfx \
    libmfx_trans_pipeline_lucas_$(MFX_IMPL) \
    libmfx_pipeline_lucas_$(MFX_IMPL) \
    libshared_utils_lucas \
    libsample_common \
    libdispatch_trace \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged

LOCAL_MODULE_TAGS := optional
ifeq ($(MFX_IMPL), sw)
    LOCAL_MODULE := mfx_transcoder_pipeline_$(MFX_IMPL)
else
    LOCAL_MODULE := mfx_transcoder_pipeline
endif

include $(BUILD_SHARED_LIBRARY)

endif # ifeq ($(MFX_ANDROID_NDK_BUILD), true)
