LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libmfx_headers
LOCAL_EXPORT_C_INCLUDE_DIRS :=  \
  $(MFX_HOME)/mdp_msdk-lib/api/include \
  $(MFX_HOME)/mdp_msdk-lib/android/include

include $(BUILD_HEADER_LIBRARY)
