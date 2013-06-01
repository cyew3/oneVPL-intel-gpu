# Defines include paths, compilation flags, etc. to build targets
# against Media SDK library (b.e. sample applications).

include $(MFX_HOME)/android/mfx_env.mk

LOCAL_CFLAGS += -DANDROID

ifneq ($(LOCAL_PATH), $(MFX_HOME)/_testsuite/plugins/openmax/sgf_addons)
    LOCAL_CFLAGS += -frtti
endif

# Passing Android-dependency information to the code
LOCAL_CFLAGS += -DMFX_ANDROID_VERSION=$(MFX_ANDROID_VERSION) -include $(MFX_HOME)/android/include/mfx_config.h

# LibVA support.
ifeq ($(MFX_IMPL), hw)
    LOCAL_CFLAGS += -DLIBVA_SUPPORT -DLIBVA_ANDROID_SUPPORT

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
