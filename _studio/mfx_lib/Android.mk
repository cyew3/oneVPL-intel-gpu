LOCAL_PATH:= $(MFX_HOME)/mdp_msdk-lib/_studio

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

MFX_LOCAL_DECODERS := h265 h264 mpeg2/hw mpeg2/sw vc1 mjpeg vp8 vp9
MFX_LOCAL_ENCODERS := h264 mpeg2 mjpeg mvc svc vp8 vp9

# Setting subdirectories to march thru
MFX_LOCAL_DIRS := \
    $(addprefix enc/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix encode/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix pak/, $(MFX_LOCAL_ENCODERS)) \
    scheduler \
    fei

MFX_OPTIMIZATION_DIRS := \
    optimization/h265/src \
    optimization/h264/src \
    optimization/h264/src/px \
    optimization/h264/src/sse

MFX_LOCAL_DIRS_IMPL := \
    $(addprefix decode/, $(MFX_LOCAL_DECODERS)) \
    vpp

MFX_LOCAL_DIRS_HW := \
    $(addprefix enc_hw/, $(MFX_LOCAL_ENCODERS)) \
    $(addprefix encode_hw/, $(MFX_LOCAL_ENCODERS)) \
    encode_hw/h265 \
    genx/h264_encode \
    mctf_package/mctf \
    cmrt_cross_platform

MFX_OPTIMIZATION_FILES := \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_OPTIMIZATION_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/*.cpp)))

MFX_LOCAL_SRC_FILES := \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp)))

MFX_LOCAL_SRC_FILES_IMPL := \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp))) \
    $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/mfx_lib/vpp/src/vme/*.cpp))

MFX_LOCAL_SRC_FILES_HW := \
    $(MFX_LOCAL_SRC_FILES_IMPL) \
    $(patsubst $(LOCAL_PATH)/%, %, $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/src/*.cpp))) \
    mfx_lib/encode_hw/hevc/hevcehw_disp.cpp \
    mfx_lib/encode_hw/hevc/agnostic/hevcehw_base.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_impl.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_alloc.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_constraints.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_dirty_rect.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_dpb_report.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_encoded_frame_info.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_ext_brc.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_hdr_sei.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_hrd.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_interlace.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_legacy.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_legacy_defaults.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_max_frame_size.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_packer.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_parser.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_roi.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_task.cpp \
    mfx_lib/encode_hw/hevc/agnostic/base/hevcehw_base_weighted_prediction.cpp \
    mfx_lib/encode_hw/hevc/agnostic/g12/hevcehw_g12_caps.cpp \
    mfx_lib/encode_hw/hevc/agnostic/g12/hevcehw_g12_rext.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_interlace_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_fei_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_roi_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_va_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_va_packer_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/base/hevcehw_base_weighted_prediction_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/g12/hevcehw_g12_lin.cpp \
    mfx_lib/encode_hw/hevc/linux/g12/hevcehw_g12_rext_lin.cpp \
    mfx_lib/encode_hw/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf.cpp \
    mfx_lib/encode_hw/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf_caps.cpp \
    mfx_lib/encode_hw/hevc/embargo/agnostic/g12/hevcehw_g12_scc.cpp \
    mfx_lib/encode_hw/hevc/embargo/agnostic/g12ats/hevcehw_g12ats_mfe.cpp \
    mfx_lib/encode_hw/hevc/embargo/agnostic/g12dg2/hevcehw_g12dg2_caps.cpp \
    mfx_lib/encode_hw/hevc/embargo/linux/g12/hevcehw_g12_embargo_lin.cpp \
    mfx_lib/encode_hw/hevc/embargo/linux/g12ats/hevcehw_g12ats_lin.cpp \
    mfx_lib/encode_hw/hevc/embargo/linux/g12ats/hevcehw_g12ats_mfe_lin.cpp \
    mfx_lib/encode_hw/hevc/embargo/linux/g12dg2/hevcehw_g12dg2_lin.cpp \
    mfx_lib/encode_hw/shared/ehw_resources_pool.cpp \
    mfx_lib/encode_hw/shared/ehw_task_manager.cpp \
    mfx_lib/encode_hw/shared/ehw_device_vaapi.cpp \
    mfx_lib/encode_hw/shared/ehw_utils_vaapi.cpp

MFX_LOCAL_INCLUDES := \
    $(foreach dir, $(MFX_LOCAL_DIRS), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include))

MFX_LOCAL_INCLUDES_IMPL := \
    $(MFX_LOCAL_INCLUDES) \
    $(foreach dir, $(MFX_LOCAL_DIRS_IMPL), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include))

MFX_LOCAL_INCLUDES_HW := \
    $(MFX_LOCAL_INCLUDES_IMPL) \
    $(foreach dir, $(MFX_LOCAL_DIRS_HW), $(wildcard $(LOCAL_PATH)/mfx_lib/$(dir)/include)) \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/asc/isa \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/field_copy/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/copy_kernels/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/mctf/isa \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/genx/mctf/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/asc/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/agnostic \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/agnostic/base \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/agnostic/g12 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/linux \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/linux/base \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/linux/g12 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/agnostic/g11lkf \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/agnostic/g12 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/agnostic/g12ats \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/agnostic/g12dg2 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/linux/g11lkf \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/linux/g12 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/linux/g12ats \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/hevc/embargo/linux/g12dg2 \
    $(MFX_HOME)/mdp_msdk-lib/_studio/mfx_lib/encode_hw/shared

MFX_LOCAL_STATIC_LIBRARIES_HW := \
    libmfx_lib_merged_hw \
    libumc_codecs_merged_hw \
    libumc_codecs_merged \
    libmfx_optimization \
    libumc_io_merged_hw \
    libumc_core_merged \
    libmfx_trace_hw \
    libasc \
    libgenx \
    libsafec \
    libippj_l \
    libippvc_l \
    libippcc_l \
    libippcv_l \
    libippi_l \
    libipps_l \
    libippmsdk_l \
    libippcore_l

MFX_LOCAL_STATIC_LIBRARIES_SW := \
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
    libippmsdk_l

MFX_LOCAL_LDFLAGS_HW := \
    $(MFX_LDFLAGS) \
    -Wl,--version-script=$(LOCAL_PATH)/mfx_lib/libmfxhw.map

MFX_LOCAL_LDFLAGS_SW := \
    $(MFX_LDFLAGS) \
    -Wl,--version-script=$(LOCAL_PATH)/mfx_lib/libmfxsw.map

# =============================================================================

UMC_DIRS := \
    mpeg2_enc h264_enc jpeg_enc \
    brc color_space_converter jpeg_common

UMC_DIRS_IMPL := \
    h265_dec h264_dec mpeg2_dec/hw mpeg2_dec/sw vc1_dec jpeg_dec vp8_dec vp9_dec \
    vc1_common vc1_spl jpeg_common \
    scene_analyzer color_space_converter

UMC_LOCAL_INCLUDES := \
    $(foreach dir, $(UMC_DIRS), $(wildcard $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/$(dir)/include))

UMC_LOCAL_INCLUDES_IMPL := \
    $(UMC_LOCAL_INCLUDES) \
    $(foreach dir, $(UMC_DIRS_IMPL), $(wildcard $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/$(dir)/include))

UMC_LOCAL_INCLUDES_HW := \
    $(UMC_LOCAL_INCLUDES_IMPL)

# =============================================================================

MFX_SHARED_FILES_IMPL := $(addprefix mfx_lib/shared/src/, \
    mfx_brc_common.cpp \
    mfx_common_int.cpp \
    mfx_enc_common.cpp \
    mfx_h264_enc_common.cpp \
    mfx_mpeg2_dec_common.cpp \
    mfx_vc1_dec_common.cpp \
    mfx_vpx_dec_common.cpp \
    mfx_common_decode_int.cpp)

MFX_SHARED_FILES_HW := \
    $(MFX_SHARED_FILES_IMPL)

MFX_SHARED_FILES_HW += $(addprefix mfx_lib/shared/src/, \
    mfx_h264_enc_common_hw.cpp \
    mfx_h264_encode_vaapi.cpp \
    mfx_h264_encode_factory.cpp)

GENX_FILES := $(addprefix mfx_lib/genx/asc/isa/, \
    asc_gen8_isa.cpp \
    asc_gen9_isa.cpp \
    asc_gen11_isa.cpp \
    asc_gen11lp_isa.cpp \
    asc_gen12_isa.cpp \
    asc_gen12lp_isa.cpp)

GENX_FILES += $(addprefix mfx_lib/genx/copy_kernels/src/, \
    embed_isa.c \
    genx_cht_copy_isa.cpp \
    genx_skl_copy_isa.cpp \
    genx_cnl_copy_isa.cpp \
    genx_icl_copy_isa.cpp \
    genx_icllp_copy_isa.cpp \
    genx_tgl_copy_isa.cpp \
    genx_tgllp_copy_isa.cpp)

GENX_FILES += $(addprefix mfx_lib/genx/field_copy/src/, \
    genx_fcopy_gen7_5_isa.cpp \
    genx_fcopy_gen8_isa.cpp \
    genx_fcopy_gen9_isa.cpp \
    genx_fcopy_gen10_isa.cpp \
    genx_fcopy_gen11_isa.cpp \
    genx_fcopy_gen11lp_isa.cpp \
    genx_fcopy_gen12_isa.cpp)

GENX_FILES += $(addprefix mfx_lib/genx/mctf/isa/, \
    mctf_mc_gen8_isa.cpp \
    mctf_mc_gen9_isa.cpp \
    mctf_mc_gen11_isa.cpp \
    mctf_mc_gen11lp_isa.cpp \
    mctf_mc_gen12_isa.cpp \
    mctf_mc_gen12lp_isa.cpp \
    mctf_me_gen8_isa.cpp \
    mctf_me_gen9_isa.cpp \
    mctf_me_gen11_isa.cpp \
    mctf_me_gen11lp_isa.cpp \
    mctf_me_gen12_isa.cpp \
    mctf_me_gen12lp_isa.cpp \
    mctf_sd_gen8_isa.cpp \
    mctf_sd_gen9_isa.cpp \
    mctf_sd_gen11_isa.cpp \
    mctf_sd_gen11lp_isa.cpp \
    mctf_sd_gen12_isa.cpp \
    mctf_sd_gen12lp_isa.cpp)

MFX_LIB_SHARED_FILES_1 := $(addprefix mfx_lib/shared/src/, \
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

MFX_LIB_SHARED_FILES_2 := $(addprefix shared/src/, \
    auxiliary_device.cpp \
    fast_copy.cpp \
    fast_compositing_ddi.cpp \
    cm_mem_copy.cpp \
    mfx_vpp_vaapi.cpp \
    libmfx_allocator.cpp \
    libmfx_allocator_vaapi.cpp \
    libmfx_core.cpp \
    libmfx_core_hw.cpp \
    libmfx_core_factory.cpp \
    libmfx_core_vaapi.cpp \
    mfx_umc_alloc_wrapper.cpp \
    mfx_umc_mjpeg_vpp.cpp \
    mfx_dxva2_device.cpp \
    mfx_static_assert_structs.cpp)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_OPTIMIZATION_FILES)

LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_INCLUDES) \
    $(UMC_LOCAL_INCLUDES) \
    $(MFX_INCLUDES_INTERNAL)
LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    -Wall -Werror
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_HEADER_LIBRARIES := libmfx_headers

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_optimization

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := \
    $(GENX_FILES)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_INCLUDES_HW) \
    $(UMC_LOCAL_INCLUDES_HW) \
    $(MFX_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -Wall -Werror
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libgenx

  include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(MFX_IMPL_HW), true)

# =============================================================================

ifeq ($(MFX_IMPL_HW), true)
  include $(CLEAR_VARS)
  include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

  LOCAL_SRC_FILES := \
    $(MFX_LOCAL_SRC_FILES) \
    $(MFX_LOCAL_SRC_FILES_HW) \
    $(MFX_SHARED_FILES_HW)

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_INCLUDES_HW) \
    $(UMC_LOCAL_INCLUDES_HW) \
    $(MFX_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -Wall -Werror
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

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
    $(MFX_LOCAL_INCLUDES_IMPL) \
    $(UMC_LOCAL_INCLUDES_IMPL) \
    $(MFX_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
  LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    -Wall -Werror
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers

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
    $(MFX_LOCAL_INCLUDES_HW) \
    $(UMC_LOCAL_INCLUDES_HW) \
    $(MFX_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -Wall -Werror
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_HW)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers
  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_HW)
  LOCAL_SHARED_LIBRARIES := libva libdl liblog

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
    $(MFX_LOCAL_INCLUDES_HW) \
    $(UMC_LOCAL_INCLUDES_HW) \
    $(MFX_INCLUDES_INTERNAL_HW)
  LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL_HW) \
    -Wall -Werror
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_HW)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers
  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_HW)
  LOCAL_SHARED_LIBRARIES := libva libdl liblog

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
    $(MFX_LOCAL_INCLUDES_IMPL) \
    $(UMC_LOCAL_INCLUDES_IMPL) \
    $(MFX_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    -Wall -Werror
  LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_SW)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers
  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_SW)
  LOCAL_SHARED_LIBRARIES := libdl liblog

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

  LOCAL_C_INCLUDES := \
    $(MFX_LOCAL_INCLUDES_IMPL) \
    $(UMC_LOCAL_INCLUDES_IMPL) \
    $(MFX_INCLUDES_INTERNAL)
  LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

  LOCAL_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    -Wall -Werror
  LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

  LOCAL_LDFLAGS := $(MFX_LOCAL_LDFLAGS_SW)

  LOCAL_HEADER_LIBRARIES := libmfx_headers liblog_headers
  LOCAL_STATIC_LIBRARIES := $(MFX_LOCAL_STATIC_LIBRARIES_SW)
  LOCAL_SHARED_LIBRARIES := libdl liblog

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfxsw64
  LOCAL_MODULE_TARGET_ARCH := x86_64
  LOCAL_MULTILIB := 64

  include $(BUILD_SHARED_LIBRARY)
endif # ifeq ($(MFX_IMPL_SW), true)
