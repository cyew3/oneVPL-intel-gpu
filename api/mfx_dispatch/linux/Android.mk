LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    mfxloader.cpp \
    mfxparser.cpp

LOCAL_C_INCLUDES := $(MFX_INCLUDES)

LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
LOCAL_CFLAGS_32 := \
    $(MFX_CFLAGS_INTERNAL_32) \
    -DMFX_MODULES_DIR=\"/system/vendor/lib\"
LOCAL_CFLAGS_64 := \
    $(MFX_CFLAGS_INTERNAL_64) \
    -DMFX_MODULES_DIR=\"/system/vendor/lib64\"

LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    mfxloader.cpp \
    mfxparser.cpp

LOCAL_C_INCLUDES := $(MFX_INCLUDES)

LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    -DMFX_DISPATCHER_LOG \
    -DDXVA2DEVICE_LOG
LOCAL_CFLAGS_32 := \
    $(MFX_CFLAGS_INTERNAL_32) \
    -DMFX_MODULES_DIR=\"/system/vendor/lib\"
LOCAL_CFLAGS_64 := \
    $(MFX_CFLAGS_INTERNAL_64) \
    -DMFX_MODULES_DIR=\"/system/vendor/lib64\"

LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libdispatch_trace

include $(BUILD_STATIC_LIBRARY)
