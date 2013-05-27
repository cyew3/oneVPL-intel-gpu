# Defines include paths, compilation flags, etc. to build targets
# against Media SDK library (b.e. sample applications).

include $(MFX_HOME)/android/mfx_env.mk

LOCAL_CFLAGS += -DANDROID

# Android OS itself does not support RTTI and exceptions
ifeq ($(MFX_ANDROID_NDK_BUILD), false)
    LOCAL_CFLAGS += -DMFX_NO_EXCEPTIONS
endif

# Passing Android-dependency information to the code
ifeq ($(MFX_ANDROID_VERSION), honeycomb)
    LOCAL_CFLAGS += -DHONEYCOMB
else
    ifeq ($(MFX_ANDROID_VERSION), honeycomb_vpg)
        LOCAL_CFLAGS += -DHONEYCOMB_VPG
    else
        LOCAL_CFLAGS += -DGINGERBREAD
    endif
endif

# LibVA support.
ifeq ($(MFX_IMPL), hw)
    LOCAL_CFLAGS += -DLIBVA_SUPPORT -DVAAPI_SURFACES_SUPPORT

    include $(MFX_HOME)/android/mfx_android_os.mk
endif

# Setting usual paths to include files
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(MFX_HOME)/include

# Setting version information for the binaries
ifeq ($(MFX_VERSION),)
   MFX_VERSION = "0.0.000.0000"
endif

LOCAL_CFLAGS += \
    -DMFX_FILE_VERSION=\"`echo $(MFX_VERSION) | cut -f 1 -d.``date +.%-y.%-m.%-d`\" \
    -DMFX_PRODUCT_VERSION=\"$(MFX_VERSION)\"
