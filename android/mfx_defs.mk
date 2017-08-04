# Purpose:
#   Defines include paths, compilation flags, etc. to build Media SDK targets.
#
# Defined variables:
#   MFX_CFLAGS - common flags for all targets
#   MFX_C_INCLUDES - common include paths for all targets
#   MFX_LDFLAGS - common link flags for all targets
#   MFX_CFLAGS_LIBVA - LibVA support flags (to build apps with or without LibVA support)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================
# Common definitions

MFX_CFLAGS := -DANDROID

ifdef RESOURCES_LIMIT
  MFX_CFLAGS += \
    -DMFX_RESOURCES_LIMIT=$(RESOURCES_LIMIT)
endif

ifeq ($(MFX_ANDROID_PLATFORM),)
  MFX_ANDROID_PLATFORM:=$(TARGET_BOARD_PLATFORM)
endif

# Passing Android-dependency information to the code
MFX_CFLAGS += \
  -DMFX_ANDROID_VERSION=$(MFX_ANDROID_VERSION) \
  -DMFX_ANDROID_PLATFORM=$(MFX_ANDROID_PLATFORM) \
  -include $(MFX_HOME)/mdp_msdk-lib/android/include/mfx_config.h

ifeq ($(MFX_OMX_PAVP),true)
  MFX_CFLAGS += \
    -DMFX_OMX_PAVP
endif

# Setting version information for the binaries
ifeq ($(MFX_VERSION),)
  MFX_VERSION = "6.0.010"
endif

MFX_CFLAGS += \
  -DMFX_FILE_VERSION=\"`echo $(MFX_VERSION) | cut -f 1 -d.``date +.%-y.%-m.%-d`\" \
  -DMFX_PRODUCT_VERSION=\"$(MFX_VERSION)\"

# LibVA support.
MFX_CFLAGS_LIBVA := -DLIBVA_SUPPORT -DLIBVA_ANDROID_SUPPORT

ifneq ($(filter $(MFX_ANDROID_VERSION), MFX_O),)
  MFX_CFLAGS_LIBVA += -DANDROID_O
endif

# Setting usual paths to include files
MFX_C_INCLUDES := \
  $(LOCAL_PATH)/include \
  $(MFX_HOME)/mdp_msdk-api/include \
  $(MFX_HOME)/mdp_msdk-lib/android/include

# Setting usual link flags
MFX_LDFLAGS := \
  -z noexecstack \
  -z relro -z now

#  Security
MFX_CFLAGS += \
  -fstack-protector \
  -fPIE -fPIC -pie \
  -O2 -D_FORTIFY_SOURCE=2 \
  -Wformat -Wformat-security

# Setting vendor
LOCAL_MODULE_OWNER := intel

# =============================================================================

# Android OS specifics
include $(MFX_HOME)/mdp_msdk-lib/android/build/mfx_android_os.mk

# STL support definitions
include $(MFX_HOME)/mdp_msdk-lib/android/build/mfx_stl.mk

# Definitions specific to Media SDK internal things (do not apply for samples)
include $(MFX_HOME)/mdp_msdk-lib/android/build/mfx_defs_internal.mk
