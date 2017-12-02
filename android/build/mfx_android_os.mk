# Purpose:
#   Defines include paths, compilation flags, etc. specific to Android OS.
#
# Defined variables:
#   MFX_C_INCLUDES_LIBVA - include paths to LibVA headers
#   MFX_C_INCLUDES_OMX - include paths to OMX headers (all needed for plug-ins)
#   MFX_HEADER_LIBRARIES_OMX - import OMX headers

MFX_C_INCLUDES_LIBVA := \
  $(TARGET_OUT_HEADERS)/libva

ifneq ($(filter MFX_O_MR1,$(MFX_ANDROID_VERSION)),)
  MFX_C_INCLUDES_LIBVA += \
    frameworks/native/libs/nativebase/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/arect/include
endif

MFX_C_INCLUDES_OMX := \
  frameworks/native/include/media/openmax \
  frameworks/native/include/media/hardware

UFO_ENABLE_GEN ?= gen7

# libpavp.h
ifeq ($(MFX_OMX_PAVP),true)
  MFX_C_INCLUDES_OMX += \
    $(TOP)/vendor/intel/hardware/PRIVATE/ufo/inc/libpavp \
    $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)/x86 \
    $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)/include/libpavp \
    $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)_dev/include/libpavp
endif

ifeq ($(BOARD_USES_GRALLOC1),true)
  MFX_C_INCLUDES_OMX += $(INTEL_MINIGBM)/cros_gralloc
endif

ifneq ($(filter MFX_O_MR1,$(MFX_ANDROID_VERSION)),)
  MFX_HEADER_LIBRARIES_OMX := \
    media_plugin_headers \
    libhardware_headers \
    libui_headers
endif
