# Purpose:
#   Defines include paths, compilation flags, etc. to build Media SDK
# internal targets (libraries, test applications, etc.).
#
# Defined variables:
#   MFX_CFLAGS_INTERNAL - all flags needed to build MFX SW targets
#   MFX_CFLAGS_INTERNAL_HW - all flags needed to build MFX HW targets
#   MFX_C_INCLUDES_INTERNAL - all include paths needed to build MFX SW targets
#   MFX_C_INCLUDES_INTERNAL_HW - all include paths needed to build MFX HW targets
#   MFX_LDFLAGS_INTERNAL - all link flags needed to build MFX SW targets
#   MFX_LDFLAGS_INTERNAL_HW - all link flags needed to build MFX HW targets
#   MFX_CFLAGS_LUCAS - flags needed to build MFX Lucas targets

MFX_CFLAGS_INTERNAL := \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_STL)
ifeq ($(TARGET_BOARD_PLATFORM), clovertrail)
    MFX_CFLAGS_INTERNAL += -mssse3
else
    MFX_CFLAGS_INTERNAL += -msse4.2
endif

MDF_ROOT := $(MFX_HOME)/thirdparty/mdf

# See http://software.intel.com/en-us/articles/intel-integrated-performance-primitives-intel-ipp-understanding-cpu-optimized-code-used-in-intel-ipp
MFX_CFLAGS_INTERNAL_32 := -DLINUX32
IPP_ROOT_32 := $(MEDIASDK_ROOT)/ipp/linux/ia32

  ifneq ($(filter $(MFX_IPP), px),) # C optimized for all IA-32 processors; i386+
  # no include file for that case
#    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_px.h
  endif
  ifneq ($(filter $(MFX_IPP), a6),) # SSE; Pentium III
  # no include file for that case
#    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_a6.h
  endif
  ifneq ($(filter $(MFX_IPP), w7),) # SSE2; P4, Xeon, Centrino
    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_w7.h
  endif
  ifneq ($(filter $(MFX_IPP), t7),) # SSE3; Prescott, Yonah
  # no include file for that case
#    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_t7.h
  endif
  ifneq ($(filter $(MFX_IPP), v8),) # Supplemental SSE3; Core 2, Xeon 5100, Atom
    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_v8.h
  endif
  ifneq ($(filter $(MFX_IPP), s8),) # Supplemental SSE3 (compiled for Atom); Atom
    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_s8.h
  endif
  ifneq ($(filter $(MFX_IPP), p8),) # SSE4.1, SSE4.2, AES-NI; Penryn Nehalem, Westmere
    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_p8.h
  endif
  ifneq ($(filter $(MFX_IPP), g9),) # AVX; Sandy Bridge
    MFX_CFLAGS_INTERNAL_32 += -include $(IPP_ROOT_32)/tools/staticlib/ipp_g9.h
  endif



MFX_CFLAGS_INTERNAL_64 := -DLINUX32 -DLINUX64
IPP_ROOT_64 := $(MEDIASDK_ROOT)/ipp/linux/em64t

  ifneq ($(filter $(MFX_IPP), mx),)
  # no include file for that case
#    MFX_CFLAGS_INTERNAL_64 += -include $(IPP_ROOT_64)/tools/staticlib/ipp_mx.h
  endif
  ifneq ($(filter $(MFX_IPP), e9),) # AVX; Sandy Bridge
    MFX_CFLAGS_INTERNAL_64 += -include $(IPP_ROOT_64)/tools/staticlib/ipp_e9.h
  endif
ifneq ($(MFX_IPP),)
    ifeq ($(filter w7 v8 s8 p8 g9 e9, $(MFX_IPP)),)
        $(error Wrong cpu optimization level)
    endif
endif

MFX_CFLAGS_INTERNAL_HW := \
    $(MFX_CFLAGS_INTERNAL) \
    -DMFX_VA \
    -Wno-error=non-virtual-dtor
MFX_CFLAGS_INTERNAL_HW_32 := $(MFX_CFLAGS_INTERNAL_32)
MFX_CFLAGS_INTERNAL_HW_64 := $(MFX_CFLAGS_INTERNAL_64)

MFX_CFLAGS_LUCAS := -DLUCAS_DLL

MFX_C_INCLUDES_INTERNAL :=  \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_STL) \
    $(MFX_HOME)/_studio/shared/include \
    $(MFX_HOME)/_studio/shared/umc/core/umc/include \
    $(MFX_HOME)/_studio/shared/umc/core/vm/include \
    $(MFX_HOME)/_studio/shared/umc/core/vm_plus/include \
    $(MFX_HOME)/_studio/shared/umc/io/media_buffers/include \
    $(MFX_HOME)/_studio/shared/umc/io/umc_io/include \
    $(MFX_HOME)/_studio/shared/umc/io/umc_va/include \
    $(MFX_HOME)/_studio/mfx_lib/shared/include \
    $(MFX_HOME)/_studio/mfx_lib/optimization/h264/include \
    $(MFX_HOME)/_studio/mfx_lib/optimization/h265/include \
    $(MFX_HOME)/thirdparty/libvpx/libvpx \
    $(MDF_ROOT)/runtime/include \
    $(MDF_ROOT)/compiler/include \
    $(MDF_ROOT)/compiler/include/cm \
    $(MFX_HOME)/thirdparty/SafeStringStaticLibrary/include
MFX_C_INCLUDES_INTERNAL_32 :=  \
    $(MFX_HOME)/thirdparty/libvpx/android/x86 \
    $(MEDIASDK_ROOT)/ipp/linux/ia32/include
MFX_C_INCLUDES_INTERNAL_64 :=  \
    $(MFX_HOME)/thirdparty/libvpx/android/x86_64 \
    $(MEDIASDK_ROOT)/ipp/linux/em64t/include

MFX_C_INCLUDES_INTERNAL_HW := \
    $(MFX_C_INCLUDES_INTERNAL) \
    $(MFX_C_INCLUDES_LIBVA)
MFX_C_INCLUDES_INTERNAL_HW_32 := $(MFX_C_INCLUDES_INTERNAL_32)
MFX_C_INCLUDES_INTERNAL_HW_64 := $(MFX_C_INCLUDES_INTERNAL_64)

MFX_LDFLAGS_INTERNAL := \
    $(MFX_LDFLAGS)
MFX_LDFLAGS_INTERNAL_32 := \
    -L$(MEDIASDK_ROOT)/ipp/linux/ia32/lib \
    -L$(MFX_HOME)/thirdparty/libvpx/android/x86/
MFX_LDFLAGS_INTERNAL_64 := \
    -L$(MEDIASDK_ROOT)/ipp/linux/em64t/lib \
    -L$(MFX_HOME)/thirdparty/libvpx/android/x86_64/

MFX_LDFLAGS_INTERNAL_HW := $(MFX_LDFLAGS_INTERNAL)
MFX_LDFLAGS_INTERNAL_HW_32 := $(MFX_LDFLAGS_INTERNAL_32)
MFX_LDFLAGS_INTERNAL_HW_64 := $(MFX_LDFLAGS_INTERNAL_64)
