LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_LOCAL_DIRS = \
    aac_common \
    asf_spl \
    avi_spl \
    brc \
    common \
    demuxer \
    h264_enc \
    h264_spl \
    mpeg2_enc \
    mpeg4_spl \
    scene_analyzer \
    spl_common \
    vc1_common \
    vc1_enc \
    vc1_spl \
    jpeg_enc \
    vp8_dec \
    jpeg_common

MFX_LOCAL_DIRS_IMPL = \
    color_space_converter \
    mpeg2_dec \
    h265_dec \
    h264_dec \
    vc1_dec \
    jpeg_dec \
    vp9_dec

MFX_LOCAL_SRC_FILES = \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.c))) \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

MFX_LOCAL_SRC_FILES_IMPL = \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.c))) \
  $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

MFX_LOCAL_C_INCLUDES = \
  $(foreach dir, $(MFX_LOCAL_DIRS) $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)
LOCAL_C_INCLUDES := \
  $(MFX_LOCAL_C_INCLUDES) \
  $(MFX_C_INCLUDES_INTERNAL)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libumc_codecs_merged

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES_IMPL)
  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libumc_codecs_merged_hw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES_IMPL)
  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libumc_codecs_merged_sw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)
