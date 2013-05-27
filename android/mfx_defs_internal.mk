# Defines include paths, compilation flags, etc. to build Media SDK
# internal targets (libraries, test applications, etc.).

include $(MFX_HOME)/android/mfx_defs.mk

ifeq ($(MFX_IMPL), hw)
    MFX_ACCEL = -DMFX_VA -DUMC_VA_LINUX
endif

LOCAL_CFLAGS += \
    -DLINUX32 $(MFX_ACCEL)

IPP_ROOT := $(MEDIASDK_ROOT)/ipp/linux/ia32

# See http://software.intel.com/en-us/articles/intel-integrated-performance-primitives-intel-ipp-understanding-cpu-optimized-code-used-in-intel-ipp
ifneq ($(filter $(MFX_IPP), px),) # C optimized for all IA-32 processors; i386+
# no include file for that case
#    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_px.h
endif
ifneq ($(filter $(MFX_IPP), a6),) # SSE; Pentium III
# no include file for that case
#    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_a6.h
endif
ifneq ($(filter $(MFX_IPP), w7),) # SSE2; P4, Xeon, Centrino
    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_w7.h
endif
ifneq ($(filter $(MFX_IPP), t7),) # SSE3; Prescott, Yonah
# no include file for that case
#    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_t7.h
endif
ifneq ($(filter $(MFX_IPP), v8),) # Supplemental SSE3; Core 2, Xeon 5100, Atom
    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_v8.h
endif
ifneq ($(filter $(MFX_IPP), s8),) # Supplemental SSE3 (compiled for Atom); Atom
    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_s8.h
endif
ifneq ($(filter $(MFX_IPP), p8),) # SSE4.1, SSE4.2, AES-NI; Penryn Nehalem, Westmere
    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_p8.h
endif
ifneq ($(filter $(MFX_IPP), g9 snb ivb),) # AVX; Sandy Bridge
    LOCAL_CFLAGS += -include $(IPP_ROOT)/tools/staticlib/ipp_g9.h
endif

LOCAL_C_INCLUDES +=  \
    $(MFX_HOME)/_studio/shared/include \
    $(MFX_HOME)/_studio/shared/umc/core/umc/include \
    $(MFX_HOME)/_studio/shared/umc/core/vm/include \
    $(MFX_HOME)/_studio/shared/umc/core/vm_plus/include \
    $(MFX_HOME)/_studio/shared/umc/io/media_buffers/include \
    $(MFX_HOME)/_studio/shared/umc/io/umc_io/include \
    $(MFX_HOME)/_studio/shared/umc/io/umc_va/include \
    $(MFX_HOME)/_studio/mfx_lib/shared/include \
    $(IPP_ROOT)/include

LOCAL_LDFLAGS += \
    -L$(IPP_ROOT)/lib
