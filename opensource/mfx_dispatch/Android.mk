LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    main.cpp \
    mfx_critical_section.cpp \
    mfx_critical_section_linux.cpp \
    mfx_dispatcher.cpp \
    mfx_function_table.cpp \
    mfx_library_iterator.cpp \
    mfx_library_iterator_linux.cpp \
    mfx_load_dll.cpp \
    mfx_load_dll_linux.cpp \
    mfx_win_reg_key.cpp \
    mfx_dxva2_device.cpp \
    mfx_plugin_hive_linux.cpp \
    mfx_plugin_cfg_parser.cpp \
    mfx_load_plugin.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_STL)
LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_STL)
LOCAL_CFLAGS_32 := \
    $(MFX_CFLAGS_INTERNAL_32) \
    -DMFX_MODULES_DIR=\"/system/lib\"
LOCAL_CFLAGS_64 := \
    $(MFX_CFLAGS_INTERNAL_64) \
    -DMFX_MODULES_DIR=\"/system/lib64\"

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, \
    main.cpp \
    mfx_critical_section.cpp \
    mfx_critical_section_linux.cpp \
    mfx_dispatcher.cpp \
    mfx_dispatcher_log.cpp \
    mfx_function_table.cpp \
    mfx_library_iterator.cpp \
    mfx_library_iterator_linux.cpp \
    mfx_load_dll.cpp \
    mfx_load_dll_linux.cpp \
    mfx_win_reg_key.cpp \
    mfx_dxva2_device.cpp \
    mfx_plugin_hive_linux.cpp \
    mfx_plugin_cfg_parser.cpp \
    mfx_load_plugin.cpp)

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_STL)
LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_STL) \
    -DMFX_DISPATCHER -DMFX_DISPATCHER_LOG -DDXVA2DEVICE_LOG
LOCAL_CFLAGS_32 := \
    $(MFX_CFLAGS_INTERNAL_32) \
    -DMFX_MODULES_DIR=\"/system/lib\"
LOCAL_CFLAGS_64 := \
    $(MFX_CFLAGS_INTERNAL_64) \
    -DMFX_MODULES_DIR=\"/system/lib64\"

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libdispatch_trace

include $(BUILD_STATIC_LIBRARY)
