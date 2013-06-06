LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

MFX_DECODERS = h265 h264 mpeg2 vc1 mjpeg vp8
MFX_ENCODERS = h264 mpeg2 vc1 mjpeg mvc svc

# Setting subdirectories to march thru
MFX_DIRS = \
    $(addprefix brc/, $(MFX_DECODERS)) \
    $(addprefix bsd/, $(MFX_DECODERS)) \
    $(addprefix dec/, $(MFX_DECODERS)) \
    $(addprefix enc/, $(MFX_ENCODERS)) \
    $(addprefix encode/, $(MFX_ENCODERS)) \
    $(addprefix pak/, $(MFX_ENCODERS)) \
    scheduler

UMC_DIRS = \
    mpeg2_enc h264_enc jpeg_enc \
    brc color_space_converter jpeg_common

MFX_DIRS_IMPL = \
    $(addprefix decode/, $(MFX_DECODERS)) \
    vpp

MFX_DIRS_HW = \
    $(addprefix enc_hw/, $(MFX_ENCODERS)) \
    $(addprefix encode_hw/, $(MFX_ENCODERS))

ifeq ($(MFX_IMPL), hw)
    MFX_DIRS_IMPL += $(MFX_DIRS_HW)
endif

UMC_DIRS_IMPL = \
    h265_dec h264_dec mpeg2_dec vc1_dec jpeg_dec \
    vc1_common vc1_spl jpeg_common \
    scene_analyzer color_space_converter

MFX_SHARED_FILES_IMPL = $(addprefix shared/src/, \
    mfx_brc_common.cpp \
    mfx_common_int.cpp \
    mfx_enc_common.cpp \
    mfx_h264_enc_common.cpp \
    mfx_mpeg2_dec_common.cpp \
    mfx_vc1_dec_common.cpp \
    mfx_vc1_enc_common.cpp \
    mfx_common_decode_int.cpp)

MFX_SHARED_FILES_HW = $(addprefix shared/src/, \
    mfx_h264_enc_common_hw.cpp \
    mfx_h264_encode_vaapi.cpp \
    mfx_h264_encode_factory.cpp)

ifeq ($(MFX_IMPL), hw)
    MFX_SHARED_FILES_IMPL += $(MFX_SHARED_FILES_HW)
endif

MFX_LIB_SHARED_FILES_1 = $(addprefix shared/src/, \
    libmfxsw.cpp \
    libmfxsw_async.cpp \
    libmfxsw_brc.cpp \
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
    mfx_user_plugin.cpp)

MFX_LIB_SHARED_FILES_2 = $(addprefix ../shared/src/, \
    auxiliary_device.cpp \
    fast_copy.cpp \
    fast_compositing_ddi.cpp \
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

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(foreach dir, $(MFX_DIRS), $(wildcard $(LOCAL_PATH)/$(dir)/include)) \
    $(foreach dir, $(UMC_DIRS), $(wildcard $(MFX_HOME)/_studio/shared/umc/codec/$(dir)/include))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_lib_merged

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/src/*.cpp))) \
    $(MFX_SHARED_FILES_IMPL) \
    $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/vpp/src/vme/*.cpp))

LOCAL_C_INCLUDES += \
    $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include)) \
    $(foreach dir, $(UMC_DIRS) $(UMC_DIRS_IMPL), $(wildcard $(MFX_HOME)/_studio/shared/umc/codec/$(dir)/include)) \
    vpp/include/vme

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_lib_merged_$(MFX_IMPL)

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)

include $(MFX_HOME)/android/mfx_stl.mk
include $(MFX_HOME)/android/mfx_defs_internal.mk

LOCAL_SRC_FILES := $(MFX_LIB_SHARED_FILES_1) $(MFX_LIB_SHARED_FILES_2)

LOCAL_C_INCLUDES += \
    $(foreach dir, $(MFX_DIRS) $(MFX_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/$(dir)/include)) \
    $(foreach dir, $(UMC_DIRS) $(UMC_DIRS_IMPL), $(wildcard $(MFX_HOME)/_studio/shared/umc/codec/$(dir)/include))

LOCAL_STATIC_LIBRARIES += \
    libmfx_lib_merged \
    libmfx_lib_merged_$(MFX_IMPL) \
    libumc_codecs_merged_$(MFX_IMPL) \
    libumc_codecs_merged \
    libumc_io_merged_$(MFX_IMPL) \
    libumc_core_merged \
    libmfx_trace_$(MFX_IMPL)

LOCAL_LDFLAGS += \
    -Wl,--version-script=$(LOCAL_PATH)/libmfx.map \
    -lippj_l -lippvc_l -lippcc_l -lippcv_l -lippi_l -lipps_l -lippcore_l -lippmsdk_l

ifeq ($(MFX_IMPL), hw)

    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libva libdl
    else
        LOCAL_LDFLAGS += -lva -ldl
    endif

else
    ifeq ($(MFX_ANDROID_NDK_BUILD), false)
        LOCAL_SHARED_LIBRARIES := libstlport-mfx libgabi++-mfx libdl
    else
        LOCAL_LDFLAGS += -ldl
    endif
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx$(MFX_IMPL)32

include $(BUILD_SHARED_LIBRARY)


