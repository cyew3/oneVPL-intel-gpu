LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Setting subdirectories to march thru
MFX_DIRS = \
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
    jpeg_common

MFX_DIRS_IMPL = \
    color_space_converter \
    mpeg2_dec \
    h264_dec \
    vc1_dec \
    jpeg_dec

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.c))) \
                   $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

LOCAL_C_INCLUDES += $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libumc_codecs_merged

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

LOCAL_C_INCLUDES += $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libumc_codecs_merged_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)
