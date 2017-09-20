ifeq ($(BOARD_HAVE_MEDIASDK_SRC),true)
  MFX_HOME:= $(call my-dir)/..

  # MFX_IPP will be set in BoardConfig.mk. Notes:
  #  - for BYT/VLV MFX_IPP:=p8

  # Setting version information for the binaries
  ifeq ($(MFX_VERSION),)
    MFX_VERSION = "6.0.010"
  endif

  # Placement of IPP and other stuff to build Media SDK:
  MEDIASDK_ROOT:= $(MFX_HOME)

  # Recursively call sub-folder Android.mk
  include $(call all-subdir-makefiles)
endif
