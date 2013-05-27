# Sets Build System enviroment variables. Should be included
# in all Android.mk files which define build targets prior
# to any other directives.

# In Media SDK there are projects which should be built either
# using Android NDK or Android OS build system.
ifeq ($(NDK_PROJECT_PATH), )
    MFX_ANDROID_NDK_BUILD = false
else
    MFX_ANDROID_NDK_BUILD = true
endif

# By default we are building SW path only.
ifeq ($(filter $(MFX_IMPL), sw hw),)
    MFX_IMPL = sw
endif

# Sometimes code depends on Android version.
ifeq ($(filter $(MFX_ANDROID_VERSION), gingerbread honeycomb honeycomb_vpg),)
    MFX_ANDROID_VERSION = gingerbread
endif
