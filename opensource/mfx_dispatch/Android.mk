LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk
 
ifeq ($(MFX_ANDROID_NDK_BUILD), true)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    main.cpp \
    mfx_critical_section.cpp \
    mfx_dispatcher.cpp \
    mfx_function_table.cpp \
    mfx_library_iterator.cpp \
    mfx_load_dll.cpp \
    mfx_win_reg_key.cpp) \
    ../shared/src/mfx_dxva2_device.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    main.cpp \
    mfx_critical_section.cpp \
    mfx_dispatcher.cpp \
    mfx_dispatcher_log.cpp \
    mfx_function_table.cpp \
    mfx_library_iterator.cpp \
    mfx_load_dll.cpp \
    mfx_win_reg_key.cpp) \
    ../shared/src/mfx_dxva2_device.cpp

LOCAL_CFLAGS += -DMFX_DISPATCHER -DMFX_DISPATCHER_LOG -DDXVA2DEVICE_LOG

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libdispatch_trace

include $(BUILD_STATIC_LIBRARY)

endif # ifeq ($(MFX_ANDROID_NDK_BUILD), true)
