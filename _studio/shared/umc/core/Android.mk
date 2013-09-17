LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_LOCAL_DIRS = \
    vm \
    vm_plus \
    umc

MFX_LOCAL_SRC_FILES = \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.c))) \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

MFX_LOCAL_C_INCLUDES = \
  $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/include))

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)
LOCAL_C_INCLUDES += $(MFX_LOCAL_C_INCLUDES)

LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL)
LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libumc_core_merged

include $(BUILD_STATIC_LIBRARY)
