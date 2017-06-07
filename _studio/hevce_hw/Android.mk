LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

MFX_LOCAL_DIRS = h265

MFX_LOCAL_SRC_FILES = $(addprefix h265/src/, \
    export.cpp \
    mfx_h265_encode_hw.cpp \
    mfx_h265_encode_hw_brc.cpp \
    mfx_h265_encode_hw_bs.cpp \
    mfx_h265_encode_hw_ddi.cpp \
    mfx_h265_encode_hw_ddi_trace.cpp \
    mfx_h265_encode_hw_par.cpp \
    mfx_h265_encode_hw_utils.cpp \
    mfx_h265_encode_vaapi.cpp)

MFX_LOCAL_C_INCLUDES = \
  $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/include)) \
  $(addprefix _studio/mfx_lib/shared/include/, mfx_platform_headers.h) \
  $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/brc/include

MFX_LOCAL_STATIC_LIBRARIES := \
    libmfx_trace_hw \
    libumc_io_merged_hw \
    libmfx_lib_merged_hw \
    libumc_core_merged \
    libsafec \
    libippj_l \
    libippvc_l \
    libippcc_l \
    libippcv_l \
    libippi_l \
    libipps_l \
    libippmsdk_l \
    libippcore_l

MFX_LOCAL_LDFLAGS += \
  $(MFX_LDFLAGS) \
  -Wl,--version-script=$(LOCAL_PATH)/libmfx_h265e_plugin.map \
  -Wl,--no-warn-shared-textrel

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)

LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -DMFX_ENABLE_H265_VIDEO_ENCODE
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)

LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES)

LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libva libdl

ifeq ($(MFX_NDK),true)
  LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmfx_hevce_hw32
LOCAL_MODULE_TARGET_ARCH := x86
LOCAL_MULTILIB := 32

include $(BUILD_SHARED_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)

LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -DMFX_ENABLE_H265_VIDEO_ENCODE
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES)

LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libva libdl

ifeq ($(MFX_NDK),true)
  LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmfx_hevce_hw64
LOCAL_MODULE_TARGET_ARCH := x86_64
LOCAL_MULTILIB := 64

include $(BUILD_SHARED_LIBRARY)
