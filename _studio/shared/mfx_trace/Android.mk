LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL_HW)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_trace_hw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL)
  LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_trace_sw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)
