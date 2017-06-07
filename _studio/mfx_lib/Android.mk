LOCAL_PATH:= $(MFX_HOME)/mdp_msdk-lib/_studio

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

MFX_LOCAL_DECODERS = h265 h264 mpeg2 vc1 mjpeg vp8 vp9
MFX_LOCAL_ENCODERS = h265 h264 mpeg2 vc1 mjpeg mvc svc vp8

# Setting subdirectories to march thru
MFX_LOCAL_DIRS = \
    $(addprefix brc/, $(MFX_LOCAL_DECODERS)) \
    $(addprefix bsd/, $(MFX_LOCAL_DECODERS)) \
    $(addprefix dec/, $(MFX_LOCAL_DECODERS)) \
    $(addprefix enc/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix encode/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix pak/, $(MFX_LOCAL_ENCODERS)) \
    scheduler \
    fei

MFX_OPTIMIZATION_DIRS = \
    optimization/h265/src \
    optimization/h264/src \
    optimization/h264/src/px \
    optimization/h264/src/sse

MFX_LOCAL_DIRS_IMPL = \
    $(addprefix decode/, $(MFX_LOCAL_DECODERS)) \
    vpp

MFX_LOCAL_DIRS_HW = \
    $(addprefix enc_hw/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix encode_hw/, $(MFX_LOCAL_ENCODERS)) \
    genx/h264_encode \
    cmrt_cross_platform

MFX_OPTIMIZATION_FILES = \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_OPTIMIZATION_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/*.cpp)))

MFX_LOCAL_SRC_FILES = \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp)))

MFX_LOCAL_SRC_FILES_IMPL = \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp))) \
    $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/mfx_lib/vpp/src/vme/*.cpp))

MFX_LOCAL_SRC_FILES_HW = \
    $(MFX_LOCAL_SRC_FILES_IMPL) \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp)))

MFX_LOCAL_C_INCLUDES = \
    $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include)) \
    $(MFX_HOME)/mdp_msdk-contrib/SafeStringStaticLibrary/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/field_copy/src \
    $(MFX_HOME)/mdp_msdk-api/mediasdk_structures

MFX_LOCAL_C_INCLUDES_IMPL = \
    $(MFX_LOCAL_C_INCLUDES) \
    $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include))

MFX_LOCAL_C_INCLUDES_HW = \
    $(MFX_LOCAL_C_INCLUDES_IMPL) \
    $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include))

MFX_LOCAL_STATIC_LIBRARIES_HW += \
    libmfx_lib_merged_hw \
    libumc_codecs_merged_hw \
    libumc_codecs_merged \
    libmfx_optimization \
    libumc_io_merged_hw \
    libumc_core_merged \
    libmfx_trace_hw \
    libsafec \
    libippj_l \
    libippvc_l \
    libippcc_l \
    libippcv_l \
    libippi_l \
    libipps_l \
    libippmsdk_l \
    libippcore_l \
    libvpx-mfx

MFX_LOCAL_STATIC_LIBRARIES_SW += \
    libmfx_lib_merged_sw \
    libumc_codecs_merged_sw \
    libumc_codecs_merged \
    libmfx_optimization \
    libumc_io_merged_sw \
    libumc_core_merged \
    libmfx_trace_sw \
    libsafec \
    libippj_l \
    libippvc_l \
    libippcc_l \
    libippcv_l \
    libippi_l \
    libipps_l \
    libippcore_l \
    libippmsdk_l \
    libvpx-mfx

MFX_LOCAL_LDFLAGS_HW += \
    $(MFX_LDFLAGS) \
    -Wl,--version-script=$(LOCAL_PATH)/mfx_lib/libmfx.map \
    -Wl,--no-warn-shared-textrel

MFX_LOCAL_LDFLAGS_SW += \
    $(MFX_LDFLAGS) \
    -Wl,--version-script=$(LOCAL_PATH)/mfx_lib/libmfx.map \
    -Wl,--no-warn-shared-textrel

# =============================================================================

UMC_DIRS = \
    mpeg2_enc h264_enc jpeg_enc \
    brc color_space_converter jpeg_common

UMC_DIRS_IMPL = \
    h265_dec h264_dec mpeg2_dec vc1_dec jpeg_dec vp8_dec vp9_dec\
    vc1_common vc1_spl jpeg_common \
    scene_analyzer color_space_converter

UMC_LOCAL_C_INCLUDES = \
    $(foreach dir, $(UMC_DIRS), $(wildcard $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/$(dir)/include))

UMC_LOCAL_C_INCLUDES_IMPL = \
    $(UMC_LOCAL_C_INCLUDES) \
    $(foreach dir, $(UMC_DIRS_IMPL), $(wildcard $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/$(dir)/include))

UMC_LOCAL_C_INCLUDES_HW = \
    $(UMC_LOCAL_C_INCLUDES_IMPL)

# =============================================================================

MFX_SHARED_FILES_IMPL = $(addprefix mfx_lib/shared/src/, \
    mfx_brc_common.cpp \
    mfx_common_int.cpp \
    mfx_enc_common.cpp \
    mfx_h264_enc_common.cpp \
    mfx_mpeg2_dec_common.cpp \
    mfx_vc1_dec_common.cpp \
    mfx_common_decode_int.cpp)

MFX_SHARED_FILES_HW = \
    $(MFX_SHARED_FILES_IMPL)

MFX_SHARED_FILES_HW += $(addprefix mfx_lib/shared/src/, \
    mfx_h264_enc_common_hw.cpp \
    mfx_h264_encode_vaapi.cpp \
    mfx_h264_encode_factory.cpp)

MFX_LIB_SHARED_FILES_1 = $(addprefix mfx_lib/shared/src/, \
    libmfxsw.cpp \
    libmfxsw_async.cpp \
    libmfxsw_decode.cpp \
    libmfxsw_enc.cpp \
    libmfxsw_encode.cpp \
    libmfxsw_pak.cpp \
    libmfxsw_plugin.cpp \
    libmfxsw_query.cpp \
    libmfxsw_session.cpp \
    libmfxsw_vpp.cpp \
    mfx_check_hardware_support.cpp \
    mfx_session.cpp \
    mfx_user_plugin.cpp \
    mfx_critical_error_handler.cpp)

MFX_LIB_SHARED_FILES_2 = $(addprefix shared/src/, \
    auxiliary_device.cpp \
    fast_copy.cpp \
    fast_compositing_ddi.cpp \
    cm_mem_copy.cpp \
    mfx_vpp_vaapi.cpp \
    libmfx_allocator.cpp \
    libmfx_allocator_vaapi.cpp \
    libmfx_core.cpp \
    libmfx_core_factory.cpp \
    libmfx_core_vaapi.cpp \
    mfx_umc_alloc_wrapper.cpp \
    mfx_dxva2_device.cpp)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(MFX_OPTIMIZATION_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES) \
    $(UMC_LOCAL_C_INCLUDES) \
    $(MFX_C_INCLUDES_INTERNAL)
LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_optimization

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := \
    $(MFX_LOCAL_SRC_FILES) \
    $(MFX_LOCAL_SRC_FILES_HW) \
    $(MFX_SHARED_FILES_HW)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES_HW) \
    $(UMC_LOCAL_C_INCLUDES_HW) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_lib_merged_hw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := \
    $(MFX_LOCAL_SRC_FILES) \
    $(MFX_LOCAL_SRC_FILES_IMPL) \
    $(MFX_SHARED_FILES_IMPL)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES_IMPL) \
    $(UMC_LOCAL_C_INCLUDES_IMPL) \
    $(MFX_C_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_lib_merged_sw

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LIB_SHARED_FILES_1) $(MFX_LIB_SHARED_FILES_2)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES_HW) \
    $(UMC_LOCAL_C_INCLUDES_HW) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_HW_32)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_HW_32)

  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_HW)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_HW)

  LOCAL_SHARED_LIBRARIES := libva libdl

  ifeq ($(MFX_NDK),true)
    LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfxhw32
  LOCAL_MODULE_TARGET_ARCH := x86
  LOCAL_MULTILIB := 32

  include $(BUILD_SHARED_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LIB_SHARED_FILES_1) $(MFX_LIB_SHARED_FILES_2)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES_HW) \
    $(UMC_LOCAL_C_INCLUDES_HW) \
    $(MFX_C_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_HW_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL_HW)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_HW_64)

  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_HW)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_HW)

  LOCAL_SHARED_LIBRARIES := libva libdl

  ifeq ($(MFX_NDK),true)
    LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfxhw64
  LOCAL_MODULE_TARGET_ARCH := x86_64
  LOCAL_MULTILIB := 64

  include $(BUILD_SHARED_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LIB_SHARED_FILES_1) $(MFX_LIB_SHARED_FILES_2)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_C_INCLUDES_IMPL) \
    $(UMC_LOCAL_C_INCLUDES_IMPL) \
    $(MFX_C_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_32 := $(MFX_C_INCLUDES_INTERNAL_32)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)

  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_SW)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_SW)

  LOCAL_SHARED_LIBRARIES := libdl

  ifeq ($(MFX_NDK),true)
    LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfxsw32
  LOCAL_MODULE_TARGET_ARCH := x86
  LOCAL_MULTILIB := 32

  include $(BUILD_SHARED_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)

# =============================================================================

ifeq ($(MFX_IMPL_SW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := $(MFX_LIB_SHARED_FILES_1) $(MFX_LIB_SHARED_FILES_2)

  LOCAL_C_INCLUDES += \
    $(MFX_LOCAL_C_INCLUDES_IMPL) \
    $(UMC_LOCAL_C_INCLUDES_IMPL) \
    $(MFX_C_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_64 := $(MFX_C_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := $(MFX_CFLAGS_INTERNAL)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_SW)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_SW)

  LOCAL_SHARED_LIBRARIES := libdl

  ifeq ($(MFX_NDK),true)
    LOCAL_SHARED_LIBRARIES += libstlport-mfx libgabi++-mfx
  endif

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfxsw64
  LOCAL_MODULE_TARGET_ARCH := x86_64
  LOCAL_MULTILIB := 64

  include $(BUILD_SHARED_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)
