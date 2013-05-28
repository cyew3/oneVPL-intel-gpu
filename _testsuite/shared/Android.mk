LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
 
include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk
 
LOCAL_SRC_FILES := $(addprefix src/, \
    cc_utils.cpp \
    memory_allocator.cpp \
    mfx_io_utils.cpp \
    shared_utils.cpp \
    test_h264_dec_command_line.cpp \
    test_statistics.cpp)
  
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libshared_utils
 
include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
 
include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk
 
LOCAL_SRC_FILES := $(addprefix src/, \
    cc_utils.cpp \
    memory_allocator.cpp \
    mfx_io_utils.cpp \
    shared_utils.cpp \
    test_h264_dec_command_line.cpp \
    test_statistics.cpp)

LOCAL_CFLAGS += -DLUCAS_DLL
  
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libshared_utils_lucas
 
include $(BUILD_STATIC_LIBRARY)

