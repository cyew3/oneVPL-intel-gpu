LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

MFX_LOCAL_DIRS = h265

MFX_LOCAL_SRC_FILES = $(addprefix h265/src/, \
    export.cpp \
    mfx_h265_encode_hw.cpp \
    mfx_h265_encode_hw_bs.cpp \
    mfx_h265_encode_hw_ddi.cpp \
    mfx_h265_encode_hw_ddi_trace.cpp \
    mfx_h265_encode_hw_par.cpp \
    mfx_h265_encode_hw_utils.cpp \
    mfx_h265_encode_vaapi.cpp)

MFX_LOCAL_C_INCLUDES = \
  $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/include)) \
  $(addprefix _studio/mfx_lib/shared/include/, \
    mfx_platform_headers.h)

MFX_LOCAL_STATIC_LIBRARIES := \
    libumc_io_merged_hw \
    libmfx_lib_merged_hw \
    libumc_core_merged \
    libmfx_trace_hw \
    libsafec

MFX_LOCAL_LDFLAGS += \
  $(MFX_LDFLAGS_INTERNAL_HW) \
  -Wl,--version-script=$(LOCAL_PATH)/libmfx_h265e_plugin.map \
  -Wl,--no-warn-shared-textrel \
  -lippj_l -lippvc_l -lippcc_l -lippcv_l -lippi_l -lipps_l -lippcore_l -lippmsdk_l

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)

LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)

LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES)

LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS)
LOCAL_LDFLAGS_32 :=  $(MFX_LDFLAGS_INTERNAL_HW_32)

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
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_LOCAL_SRC_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)

LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES)

LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS)
LOCAL_LDFLAGS_64 :=  $(MFX_LDFLAGS_INTERNAL_HW_64)

LOCAL_SHARED_LIBRARIES := libva libdl

ifeq ($(MFX_NDK),true)
  LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
endif

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmfx_hevce_hw64
LOCAL_MODULE_TARGET_ARCH := x86_64
LOCAL_MULTILIB := 64

include $(BUILD_SHARED_LIBRARY)