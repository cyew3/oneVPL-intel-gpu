ifneq ($(MFX_ANDROID_HOME), )

include $(MFX_HOME)/android/mfx_android_os.mk

ifeq ($(MFX_IMPL), hw)
    LOCAL_CFLAGS += -DMFX_OMX_IMPL_HW
endif

endif
