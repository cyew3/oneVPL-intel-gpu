LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
  $(addprefix config/, $(notdir $(wildcard $(LOCAL_PATH)/config/*.cpp))) \
  $(addprefix dumps/, $(notdir $(wildcard $(LOCAL_PATH)/dumps/*.cpp))) \
  $(addprefix loggers/, $(notdir $(wildcard $(LOCAL_PATH)/loggers/*.cpp))) \
  $(addprefix tracer/, $(notdir $(wildcard $(LOCAL_PATH)/tracer/*.cpp))) \
  $(addprefix wrappers/, $(notdir $(wildcard $(LOCAL_PATH)/wrappers/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_STL)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_STL) \
    -Wno-unknown-pragmas

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)
   -Wl,--version-script=$(LOCAL_PATH)/libmfx.map

LOCAL_SHARED_LIBRARIES := libdl liblog

ifeq ($(MFX_NDK),true)
   LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx-tracer

include $(BUILD_SHARED_LIBRARY)
