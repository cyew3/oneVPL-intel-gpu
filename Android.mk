ifeq ($(BOARD_HAVE_MEDIASDK_SRC),true)
  MFX_HOME:= $(call my-dir)/..

  # MFX_IPP will be set in BoardConfig.mk. Notes:
  #  - for BYT/VLV MFX_IPP:=p8

  # Setting version information for the binaries
  ifeq ($(MFX_VERSION),)
    MFX_VERSION = "6.0.010"
  endif

  # Android version preference:
  ifneq ($(filter 8.% O ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_O
  endif
  ifneq ($(filter 7.% N ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_N
  endif
  ifneq ($(filter 6.% M ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_MM
  endif
  ifneq ($(filter 5.% L ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_MCG_LD
    endif
  ifneq ($(filter 4.4 4.4.4 ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_MCG_KK
  endif
  ifneq ($(filter 4.2.2 4.3 ,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_MCG_JB
  endif

  # Placement of IPP and other stuff to build Media SDK:
  MEDIASDK_ROOT:= $(MFX_HOME)

  # Recursively call sub-folder Android.mk
  include $(call all-subdir-makefiles)
endif
