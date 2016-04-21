LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES := $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)
  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_trace_hw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES := \
      $(MFX_C_INCLUDES_INTERNAL) \
      $(MFX_HOME)/thirdparty/SafeStringStaticLibrary/include
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)
  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_trace_sw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)
