ifeq ($(BOARD_HAVE_MEDIASDK_SRC),true)
  MFX_HOME:= $(call my-dir)/..

  # Setting IPP library optimization to use
  # See http://software.intel.com/en-us/articles/intel-integrated-performance-primitives-intel-ipp-understanding-cpu-optimized-code-used-in-intel-ipp
  # and http://software.intel.com/en-us/articles/understanding-simd-optimization-layers-and-dispatching-in-the-intel-ipp-70-library
  MFX_IPP_OPT_LEVEL_32 := p8
  MFX_IPP_OPT_LEVEL_64 := y8

  # Setting version information for the binaries
  ifeq ($(MFX_VERSION),)
    MFX_VERSION = "6.0.010"
  endif

  # Placement of IPP and other stuff to build Media SDK:
  MEDIASDK_ROOT:= $(MFX_HOME)

  # Recursively call sub-folder Android.mk
  include $(call all-subdir-makefiles)
endif
