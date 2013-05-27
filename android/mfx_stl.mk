ifeq ($(MFX_ANDROID_NDK_BUILD), false)
    include external/stlport/libstlport.mk

    LOCAL_LDFLAGS += -lstlport
endif
