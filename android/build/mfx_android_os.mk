# Purpose:
#   Defines include paths, compilation flags, etc. specific to Android OS.
#
# Defined variables:
#   MFX_C_INCLUDES_LIBVA - include paths to LibVA headers
#   MFX_C_INCLUDES_OMX - include paths to OMX headers (all needed for plug-ins)

MFX_C_INCLUDES_LIBVA := \
  $(TARGET_OUT_HEADERS)/libva

MFX_C_INCLUDES_OMX := \
  frameworks/native/include/media/openmax \
  frameworks/native/include/media/hardware

ifeq ($(MFX_ANDROID_VERSION), MFX_OTC_JB)
  MFX_C_INCLUDES_OMX += \
    $(TARGET_OUT_HEADERS)/libmixcodec
endif

ifeq ($(MFX_ANDROID_VERSION), MFX_MCG_JB)
  MFX_C_INCLUDES_OMX += \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder
endif

ifeq ($(MFX_ANDROID_VERSION), MFX_MCG_KK)
  MFX_C_INCLUDES_OMX += \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder
endif
