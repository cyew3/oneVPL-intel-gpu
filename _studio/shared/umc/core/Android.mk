LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_DIRS = \
    vm \
    vm_plus \
    umc

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp))) \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.c)))

LOCAL_C_INCLUDES += $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

LOCAL_MODULE_TAGS := optional

ifeq ($(MFX_ANDROID_NDK_BUILD), false)
    LOCAL_MODULE := libumc_core_merged_native
else
    LOCAL_MODULE := libumc_core_merged
endif

include $(BUILD_STATIC_LIBRARY)
