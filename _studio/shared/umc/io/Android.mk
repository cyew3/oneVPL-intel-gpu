LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_DIRS_IMPL = \
    media_buffers \
    umc_io

MFX_DIRS_HW = \
    umc_va

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

ifeq ($(MFX_IMPL), hw)
    MFX_DIRS_IMPL += $(MFX_DIRS_HW)
endif

LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

LOCAL_C_INCLUDES += $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libumc_io_merged_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)
