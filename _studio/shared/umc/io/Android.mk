LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_LOCAL_DIRS_IMPL = \
    media_buffers \
    umc_io

MFX_LOCAL_DIRS_HW = \
    umc_va

MFX_LOCAL_SRC_FILES_IMPL = \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

MFX_LOCAL_SRC_FILES_HW = \
  $(MFX_LOCAL_SRC_FILES_IMPL) \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

MFX_LOCAL_C_INCLUDES = \
  $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

MFX_LOCAL_C_INCLUDES_HW = \
  $(MFX_LOCAL_C_INCLUDES) \
  $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/$(dir)/include))

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES_HW)
  LOCAL_C_INCLUDES += $(MFX_LOCAL_C_INCLUDES_HW)

  LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL_HW)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libumc_io_merged_hw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

# This target actually should not be called *_sw, but that's simplifies
# calling of this lib. So, we should not cover this by MFX_IMPL_SW==true protection.

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES_IMPL)
LOCAL_C_INCLUDES += $(MFX_LOCAL_C_INCLUDES)

LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL)
LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libumc_io_merged_sw

include $(BUILD_STATIC_LIBRARY)
