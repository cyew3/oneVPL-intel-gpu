/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
*/

#include "mfx_common.h"

#if defined(MFX_VA_LINUX)

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_hevc.h>

#include "libmfx_core_vaapi.h"
#include "mfx_h265_encode_vaapi.h"
#include "mfx_h265_encode_hw_utils.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{
mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
{
    switch (rateControl)
    {
        case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
        case MFX_RATECONTROL_CBR:  return VA_RC_CBR | VA_RC_MB;
        case MFX_RATECONTROL_VBR:  return VA_RC_VBR | VA_RC_MB;
        default: assert(!"Unsupported RateControl"); return 0;
    }
}

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type)
{
    switch (type)
    {
        case MFX_PROFILE_HEVC_MAIN:  return VAProfileHEVCMain;
        default: return VAProfileHEVCMain;
    }
}

mfxStatus SetHRD(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id)
{

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *hrd_param;

    if ( hrdBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, hrdBuf_id);
    }
    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                   1,
                   NULL,
                   &hrdBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 hrdBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeHRD;
    hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        hrd_param->initial_buffer_fullness = par.InitialDelayInKB * 8000;
        hrd_param->buffer_size = par.BufferSizeInKB * 8000;
    }
    else
    {
        hrd_param->initial_buffer_fullness = 0;
        hrd_param->buffer_size = 0;
    }

    vaUnmapBuffer(vaDisplay, hrdBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetRateControl(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rateParamBuf_id,
    bool         isBrcResetRequired = false)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;

    if ( rateParamBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, rateParamBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                   1,
                   NULL,
                   &rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 rateParamBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRateControl;
    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        rate_param->bits_per_second = par.MaxKbps * 1000;
        if(par.MaxKbps)
            rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.TargetKbps / (mfxF64)par.MaxKbps);
        rate_param->window_size     = par.mfx.Convergence * 100;
        rate_param->rc_flags.bits.reset = isBrcResetRequired;

#ifdef PARALLEL_BRC_support
        rate_param->rc_flags.bits.enable_parallel_brc =  (par.AsyncDepth > 1) && (par.mfx.GopRefDist > 1) && (par.mfx.GopRefDist <= 8) && par.isBPyramid();
#else
        rate_param->rc_flags.bits.enable_parallel_brc = 0;
#endif
    }

    rate_param->initial_qp = par.m_pps.init_qp_minus26 + 26;

    vaUnmapBuffer(vaDisplay, rateParamBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetFrameRate(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, frameRateBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 frameRateBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    frameRate_param->framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);

    vaUnmapBuffer(vaDisplay, frameRateBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetBRCParallel(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & BRCParallel_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterParallelRateControl *BRCParallel_param;

    if ( BRCParallel_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, BRCParallel_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterParallelRateControl),
                   1,
                   NULL,
                   &BRCParallel_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 BRCParallel_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeParallelBRC;
    BRCParallel_param = (VAEncMiscParameterParallelRateControl *)misc_param->data;

    if (!par.isBPyramid())
    {
        BRCParallel_param->num_b_in_gop[1] = par.mfx.GopRefDist - 1;
        BRCParallel_param->num_b_in_gop[2] = 0;
        BRCParallel_param->num_b_in_gop[3] = 0;
    }
    else if (par.mfx.GopRefDist <=  8)
    {
        static UINT B[9]  = {0,0,1,1,1,1,1,1,1};
        static UINT B1[9] = {0,0,0,1,2,2,2,2,2};
        static UINT B2[9] = {0,0,0,0,0,1,2,3,4};

        BRCParallel_param->num_b_in_gop[0]  = B[par.mfx.GopRefDist];
        BRCParallel_param->num_b_in_gop[1]  = B1[par.mfx.GopRefDist];
        BRCParallel_param->num_b_in_gop[2]  = B2[par.mfx.GopRefDist];
    }
    else
    {
        BRCParallel_param->num_b_in_gop[0]  = 0;
        BRCParallel_param->num_b_in_gop[1]  = 0;
        BRCParallel_param->num_b_in_gop[2]  = 0;
    }

    vaUnmapBuffer(vaDisplay, BRCParallel_id);

    return MFX_ERR_NONE;
}

mfxStatus SetQualityLevelParams(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & qualityParams_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferQualityLevel *quality_param;

    if ( qualityParams_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, qualityParams_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferQualityLevel),
                   1,
                   NULL,
                   &qualityParams_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 qualityParams_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeQualityLevel;
    quality_param = (VAEncMiscParameterBufferQualityLevel *)misc_param->data;

    quality_param->quality_level = (unsigned int)(par.mfx.TargetUsage);

    vaUnmapBuffer(vaDisplay, qualityParams_id);

    return MFX_ERR_NONE;
}

void FillConstPartOfPps(
    MfxVideoParam const & par,
    VAEncPictureParameterBufferHEVC & pps)
{
    Zero(pps);

    for(mfxU32 i = 0; i < 16; i++ )
    {
        pps.reference_frames[i].picture_id = VA_INVALID_ID;
    }

    pps.last_picture = 0;
    pps.pic_init_qp = (mfxU8)(par.m_pps.init_qp_minus26 + 26);
    pps.diff_cu_qp_delta_depth  = (mfxU8)par.m_pps.diff_cu_qp_delta_depth;
    pps.pps_cb_qp_offset        = (mfxU8)par.m_pps.cb_qp_offset;
    pps.pps_cr_qp_offset        = (mfxU8)par.m_pps.cr_qp_offset;
    pps.num_tile_columns_minus1 = (mfxU8)par.m_pps.num_tile_columns_minus1;
    pps.num_tile_rows_minus1    = (mfxU8)par.m_pps.num_tile_rows_minus1;
    for (mfxU32 i = 0; i < 19; ++i)
        pps.column_width_minus1[i] = par.m_pps.column_width[i];
    for (mfxU32 i = 0; i < 21; ++i)
        pps.row_height_minus1[i] = par.m_pps.row_height[i];
    pps.log2_parallel_merge_level_minus2 = (mfxU8)par.m_pps.log2_parallel_merge_level_minus2;
    pps.ctu_max_bitsize_allowed = 0;
    pps.num_ref_idx_l0_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l1_default_active_minus1;
    pps.slice_pic_parameter_set_id = 0;

    pps.pic_fields.bits.dependent_slice_segments_enabled_flag      = par.m_pps.dependent_slice_segments_enabled_flag;
    pps.pic_fields.bits.sign_data_hiding_enabled_flag              = par.m_pps.sign_data_hiding_enabled_flag;
    pps.pic_fields.bits.constrained_intra_pred_flag                = par.m_pps.constrained_intra_pred_flag;
    pps.pic_fields.bits.transform_skip_enabled_flag                = par.m_pps.transform_skip_enabled_flag;
    pps.pic_fields.bits.cu_qp_delta_enabled_flag                   = par.m_pps.cu_qp_delta_enabled_flag;
    pps.pic_fields.bits.weighted_pred_flag                         = par.m_pps.weighted_pred_flag;
    pps.pic_fields.bits.weighted_bipred_flag                       = par.m_pps.weighted_pred_flag;
    pps.pic_fields.bits.transquant_bypass_enabled_flag             = par.m_pps.transquant_bypass_enabled_flag;
    pps.pic_fields.bits.tiles_enabled_flag                         = par.m_pps.tiles_enabled_flag;
    pps.pic_fields.bits.entropy_coding_sync_enabled_flag           = par.m_pps.entropy_coding_sync_enabled_flag;
    pps.pic_fields.bits.loop_filter_across_tiles_enabled_flag      = par.m_pps.loop_filter_across_tiles_enabled_flag;
    pps.pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = par.m_pps.loop_filter_across_slices_enabled_flag;
    pps.pic_fields.bits.scaling_list_data_present_flag             = par.m_pps.scaling_list_data_present_flag;
    pps.pic_fields.bits.screen_content_flag = 0;
    pps.pic_fields.bits.enable_gpu_weighted_prediction = 0;
    pps.pic_fields.bits.no_output_of_prior_pics_flag = 0;
}

void UpdatePPS(
    Task const & task,
    VAEncPictureParameterBufferHEVC & pps,
    std::vector<ExtVASurface> const & reconQueue)
{
    //pps.nal_unit_type
    pps.pic_fields.bits.idr_pic_flag  = !!(task.m_frameType & MFX_FRAMETYPE_IDR);
    pps.pic_fields.bits.coding_type   = task.m_codingType;
    pps.pic_fields.bits.reference_pic_flag = !!(task.m_frameType & MFX_FRAMETYPE_REF) /*(task.m_codingType != CODING_TYPE_B) ? 1 : 0*/;

    if (task.m_sh.temporal_mvp_enabled_flag)
        pps.collocated_ref_pic_index = task.m_refPicList[!task.m_sh.collocated_from_l0_flag][task.m_sh.collocated_ref_idx];
    else
        pps.collocated_ref_pic_index = 0xFF;

    pps.decoded_curr_pic.picture_id     = reconQueue.size() > task.m_idxRec ?  reconQueue[task.m_idxRec].surface : VA_INVALID_SURFACE;
    pps.decoded_curr_pic.pic_order_cnt =  task.m_poc;
    pps.decoded_curr_pic.flags = 0;

    for (mfxU32 i = 0; i < 15; ++i)
    {
        pps.reference_frames[i].picture_id   = (task.m_dpb[0][i].m_idxRec >= reconQueue.size()) ? VA_INVALID_SURFACE : reconQueue[task.m_dpb[0][i].m_idxRec].surface;
        pps.reference_frames[i].pic_order_cnt = task.m_dpb[0][i].m_poc;
        pps.reference_frames[i].flags = 0;

        if (task.m_dpb[0][i].m_ltr)
            pps.reference_frames[i].flags |= VA_PICTURE_HEVC_LONG_TERM_REFERENCE;

        if (IDX_INVALID == task.m_dpb[0][i].m_idxRec)
        {
            pps.reference_frames[i].picture_id = VA_INVALID_SURFACE;
            pps.reference_frames[i].flags = VA_PICTURE_HEVC_INVALID ; //VA_PICTURE_HEVC_INVALID/VA_PICTURE_HEVC_FIELD_PIC/VA_PICTURE_HEVC_BOTTOM_FIELD/VA_PICTURE_HEVC_LONG_TERM_REFERENCE/VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE
            pps.reference_frames[i].pic_order_cnt = 0;
        }
    }
}

void FillSliceBuffer(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferHEVC const & sps,
    VAEncPictureParameterBufferHEVC const & pps,
    std::vector<VAEncSliceParameterBufferHEVC> & slices)
{
    slices.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slices.size(); i ++)
    {
        VAEncSliceParameterBufferHEVC & slice = slices[i];
        Zero(slice);

        slice.slice_segment_address   = par.m_slice[i].SegmentAddress;
        slice.num_ctu_in_slice        = par.m_slice[i].NumLCU;
        slice.slice_fields.bits.last_slice_of_pic_flag  = (i == slices.size() - 1);
    }
}

void UpdateSlice(
    Task const &                               task,
    VAEncSequenceParameterBufferHEVC const     & sps,
    VAEncPictureParameterBufferHEVC const      & pps,
    std::vector<VAEncSliceParameterBufferHEVC> & slices)
{
    for (mfxU16 i = 0; i < slices.size(); i ++)
    {
        VAEncSliceParameterBufferHEVC & slice = slices[i];

        slice.slice_type                 = task.m_sh.type;
        slice.slice_pic_parameter_set_id = pps.slice_pic_parameter_set_id;
        if (slice.slice_type != 2)
        {
            slice.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

            if (slice.slice_type == 0)
                slice.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
            else
                slice.num_ref_idx_l1_active_minus1 = 0;

        }
        else
            slice.num_ref_idx_l0_active_minus1 = slice.num_ref_idx_l1_active_minus1 = 0;

        for (mfxU32 ref = 0; ref < 15; ref++)
        {
            mfxU32 ind = task.m_refPicList[0][ref];
            if ( ind < 15)
            {
                slice.ref_pic_list0[ref].picture_id    = pps.reference_frames[ind].picture_id;
                slice.ref_pic_list0[ref].pic_order_cnt = pps.reference_frames[ind].pic_order_cnt;
                slice.ref_pic_list0[ref].flags         = pps.reference_frames[ind].flags;
            }
            else
            {
                slice.ref_pic_list0[ref].picture_id    = VA_INVALID_ID;
                slice.ref_pic_list0[ref].pic_order_cnt = 0;
                slice.ref_pic_list0[ref].flags         = VA_PICTURE_HEVC_INVALID;
            }
        }
        for (mfxU32 ref = 0; ref < 15; ref++)
        {
            mfxU32 ind = task.m_refPicList[1][ref];
            if ( ind < 15)
            {
                slice.ref_pic_list1[ref].picture_id    = pps.reference_frames[ind].picture_id;
                slice.ref_pic_list1[ref].pic_order_cnt = pps.reference_frames[ind].pic_order_cnt;
                slice.ref_pic_list1[ref].flags         = pps.reference_frames[ind].flags;
            }
            else
            {
                slice.ref_pic_list1[ref].picture_id    = VA_INVALID_ID;
                slice.ref_pic_list1[ref].pic_order_cnt = 0;
                slice.ref_pic_list1[ref].flags         = VA_PICTURE_HEVC_INVALID;
            }
        }
/*
        slice.luma_log2_weight_denom;
        slice.delta_chroma_log2_weight_denom
        slice.delta_luma_weight_l0
        slice.luma_offset_l0
        slice.delta_chroma_weight_l0
        slice.chroma_offset_l0
        slice.delta_luma_weight_l1
        slice.luma_offset_l1
        slice.delta_chroma_weight_l1
        slice.chroma_offset_l1
*/
        slice.max_num_merge_cand   = 5 - task.m_sh.five_minus_max_num_merge_cand;
        slice.slice_qp_delta       = task.m_sh.slice_qp_delta;
        slice.slice_cb_qp_offset   = task.m_sh.slice_cb_qp_offset;
        slice.slice_cr_qp_offset   = task.m_sh.slice_cr_qp_offset;
        slice.slice_beta_offset_div2     = task.m_sh.beta_offset_div2;
        slice.slice_tc_offset_div2       = task.m_sh.tc_offset_div2;

        slice.slice_fields.bits.dependent_slice_segment_flag         = task.m_sh.dependent_slice_segment_flag;
        //slice.colour_plane_id
        slice.slice_fields.bits.slice_temporal_mvp_enabled_flag = task.m_sh.temporal_mvp_enabled_flag;
        slice.slice_fields.bits.slice_sao_luma_flag                  = task.m_sh.sao_luma_flag;
        slice.slice_fields.bits.slice_sao_chroma_flag                = task.m_sh.sao_chroma_flag;
        slice.slice_fields.bits.num_ref_idx_active_override_flag =
                slice.num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_default_active_minus1 ||
                slice.num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_default_active_minus1;
        slice.slice_fields.bits.mvd_l1_zero_flag                     = task.m_sh.mvd_l1_zero_flag;
        slice.slice_fields.bits.cabac_init_flag                      = task.m_sh.cabac_init_flag;
        slice.slice_fields.bits.slice_deblocking_filter_disabled_flag = task.m_sh.deblocking_filter_disabled_flag;
        //slice.slice_loop_filter_across_slices_enabled_flag
        slice.slice_fields.bits.collocated_from_l0_flag              = task.m_sh.collocated_from_l0_flag;
    }
}

VAAPIEncoder::VAAPIEncoder()
: m_core(NULL)
, m_vaDisplay(0)
, m_vaContextEncode(VA_INVALID_ID)
, m_vaConfig(VA_INVALID_ID)
, m_spsBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_rateParamBufferId(VA_INVALID_ID)
, m_BRCParallelParamBufferId(VA_INVALID_ID)
, m_frameRateId(VA_INVALID_ID)
, m_qualityLevelBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_packedAudHeaderBufferId(VA_INVALID_ID)
, m_packedAudBufferId(VA_INVALID_ID)
, m_packedSpsHeaderBufferId(VA_INVALID_ID)
, m_packedSpsBufferId(VA_INVALID_ID)
, m_packedVpsHeaderBufferId(VA_INVALID_ID)
, m_packedVpsBufferId(VA_INVALID_ID)
, m_packedPpsHeaderBufferId(VA_INVALID_ID)
, m_packedPpsBufferId(VA_INVALID_ID)
, m_packedSeiHeaderBufferId(VA_INVALID_ID)
, m_packedSeiBufferId(VA_INVALID_ID)
, m_packedSkippedSliceHeaderBufferId(VA_INVALID_ID)
, m_packedSkippedSliceBufferId(VA_INVALID_ID)
{
}

VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

}

void VAAPIEncoder::FillSps(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferHEVC & sps)
{
    Zero(sps);

    sps.general_profile_idc = par.m_sps.general.profile_idc;
    sps.general_level_idc   = par.m_sps.general.level_idc;
    sps.general_tier_flag   = par.m_sps.general.tier_flag;
    sps.intra_period         = par.mfx.GopPicSize;
    sps.intra_idr_period     = par.mfx.GopPicSize*par.mfx.IdrInterval;
    sps.ip_period            = mfxU8(par.mfx.GopRefDist);
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        sps.bits_per_second   = par.TargetKbps * 1000;
    }
    sps.pic_width_in_luma_samples  = par.m_sps.pic_width_in_luma_samples;
    sps.pic_height_in_luma_samples = par.m_sps.pic_height_in_luma_samples;

    sps.seq_fields.bits.chroma_format_idc                   = par.m_sps.chroma_format_idc;
    sps.seq_fields.bits.separate_colour_plane_flag          = par.m_sps.separate_colour_plane_flag;
    sps.seq_fields.bits.bit_depth_luma_minus8               = (mfxU8)par.m_sps.bit_depth_luma_minus8;
    sps.seq_fields.bits.bit_depth_chroma_minus8             = (mfxU8)par.m_sps.bit_depth_chroma_minus8;
    sps.seq_fields.bits.scaling_list_enabled_flag           = par.m_sps.scaling_list_enabled_flag;
    sps.seq_fields.bits.strong_intra_smoothing_enabled_flag = par.m_sps.strong_intra_smoothing_enabled_flag;
    sps.seq_fields.bits.amp_enabled_flag                    = par.m_sps.amp_enabled_flag;
    sps.seq_fields.bits.sample_adaptive_offset_enabled_flag = par.m_sps.sample_adaptive_offset_enabled_flag;
    sps.seq_fields.bits.pcm_enabled_flag                    = par.m_sps.pcm_enabled_flag;
    sps.seq_fields.bits.pcm_loop_filter_disabled_flag       = 1;//par.m_sps.pcm_loop_filter_disabled_flag;
    sps.seq_fields.bits.sps_temporal_mvp_enabled_flag       = par.m_sps.temporal_mvp_enabled_flag;

    sps.log2_min_luma_coding_block_size_minus3 = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;

    sps.log2_diff_max_min_luma_coding_block_size   = (mfxU8)par.m_sps.log2_diff_max_min_luma_coding_block_size;
    sps.log2_min_transform_block_size_minus2    = (mfxU8)par.m_sps.log2_min_transform_block_size_minus2;
    sps.log2_diff_max_min_transform_block_size  = (mfxU8)par.m_sps.log2_diff_max_min_transform_block_size;
    sps.max_transform_hierarchy_depth_inter     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_inter;
    sps.max_transform_hierarchy_depth_intra     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_intra;
    sps.pcm_sample_bit_depth_luma_minus1        = (mfxU8)par.m_sps.pcm_sample_bit_depth_luma_minus1;
    sps.pcm_sample_bit_depth_chroma_minus1      = (mfxU8)par.m_sps.pcm_sample_bit_depth_chroma_minus1;
    sps.log2_min_pcm_luma_coding_block_size_minus3 = (mfxU8)par.m_sps.log2_min_pcm_luma_coding_block_size_minus3;
    sps.log2_max_pcm_luma_coding_block_size_minus3 = (mfxU8)(par.m_sps.log2_min_pcm_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_pcm_luma_coding_block_size);

    sps.vui_parameters_present_flag = m_sps.vui_parameters_present_flag;
    sps.vui_fields.bits.aspect_ratio_info_present_flag = par.m_sps.vui.aspect_ratio_info_present_flag;
    sps.vui_fields.bits.neutral_chroma_indication_flag = par.m_sps.vui.neutral_chroma_indication_flag;
    sps.vui_fields.bits.field_seq_flag = par.m_sps.vui.field_seq_flag;
    sps.vui_fields.bits.vui_timing_info_present_flag= par.m_sps.vui.timing_info_present_flag;
    sps.vui_fields.bits.bitstream_restriction_flag = par.m_sps.vui.bitstream_restriction_flag;
    sps.vui_fields.bits.tiles_fixed_structure_flag = par.m_sps.vui.tiles_fixed_structure_flag ;
    sps.vui_fields.bits.motion_vectors_over_pic_boundaries_flag = par.m_sps.vui.motion_vectors_over_pic_boundaries_flag;
    sps.vui_fields.bits.restricted_ref_pic_lists_flag = par.m_sps.vui.restricted_ref_pic_lists_flag;
    sps.vui_fields.bits.log2_max_mv_length_horizontal = par.m_sps.vui.log2_max_mv_length_horizontal;
    sps.vui_fields.bits.log2_max_mv_length_vertical = par.m_sps.vui.log2_max_mv_length_vertical;


    sps.aspect_ratio_idc = par.m_sps.vui.aspect_ratio_idc;
    sps.sar_width = par.m_sps.vui.sar_width;
    sps.sar_height = par.m_sps.vui.sar_height;
    sps.vui_num_units_in_tick = par.m_sps.vui.num_units_in_tick;
    sps.vui_time_scale = par.m_sps.vui.time_scale;
    sps.min_spatial_segmentation_idc = par.m_sps.vui.min_spatial_segmentation_idc;
    sps.max_bytes_per_pic_denom = par.m_sps.vui.max_bytes_per_pic_denom;
    sps.max_bits_per_min_cu_denom = par.m_sps.vui.max_bits_per_min_cu_denom;
}

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID /*guid*/,
    mfxU32 width,
    mfxU32 height)
{
    m_core = core;

    mfxStatus sts = core->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*)&m_vaDisplay);
    MFX_CHECK_STS(sts);

    m_width  = width;
    m_height = height;

    memset(&m_caps, 0, sizeof(m_caps));

    m_caps.BRCReset = 1;
    m_caps.MaxNum_Reference0 = 1; //FIXME
    m_caps.MaxNum_Reference1 = 1; //FIXME
    m_caps.MaxPicWidth = 1920; //FIXME
    m_caps.MaxPicHeight = 1920; //FIXME

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    if (0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces wheh call vaCreateContext().
     * Here we don't have this info.
     */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    VAStatus vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                &*pEntrypoints.begin(),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


    VAEntrypoint entryPoint = VAEntrypointEncSlice;
    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        if( VAEntrypointEncSlice == pEntrypoints[entrypointsIndx] )
        {
            bEncodeEnable = true;
            break;
        }
    }
    if( !bEncodeEnable )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // Configuration
    VAConfigAttrib attrib[2];
    mfxI32 numAttrib = 0;
    mfxU32 flag = VA_PROGRESSIVE;

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    numAttrib = 2;

   vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                          entryPoint,
                          &attrib[0], numAttrib);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        entryPoint,
        attrib,
        numAttrib,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(unsigned int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        flag,
        &*reconSurf.begin(),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU16 maxNumSlices = par.m_slice.size();

    m_slice.resize(maxNumSlices);
    m_sliceBufferId.resize(maxNumSlices);
    m_packeSliceHeaderBufferId.resize(maxNumSlices);
    m_packedSliceBufferId.resize(maxNumSlices);
    for(int i = 0; i < maxNumSlices; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);

#ifdef PARALLEL_BRC_support
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetBRCParallel(par, m_vaDisplay, m_vaContextEncode, m_BRCParallelParamBufferId), MFX_ERR_DEVICE_FAILED);
#endif
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityLevelParams(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId), MFX_ERR_DEVICE_FAILED);

    FillConstPartOfPps(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)
{
    m_videoParam = par;

    FillSps(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    bool isBrcResetRequired = !Equal(m_vaBrcPar, oldBrcPar) || !Equal(m_vaFrameRate, oldFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId, isBrcResetRequired), MFX_ERR_DEVICE_FAILED);

#ifdef PARALLEL_BRC_support
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetBRCParallel(par, m_vaDisplay, m_vaContextEncode, m_BRCParallelParamBufferId), MFX_ERR_DEVICE_FAILED);
#endif
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityLevelParams(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId), MFX_ERR_DEVICE_FAILED);

    FillConstPartOfPps(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    type;

    // request linear bufer
    request.Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    request.AllocId = m_vaContextEncode;

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS_HEVC& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type )
        pQueue = &m_bsQueue;
    else
        pQueue = &m_reconQueue;

    // we should register allocated HW bitstreams and recon surfaces
    MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

    ExtVASurface extSurf;
    VASurfaceID *pSurface = NULL;

    mfxFrameAllocator & allocator = m_core->FrameAllocator();

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {

        sts = allocator.GetHDL(allocator.pthis, response.mids[i], (mfxHDL *)&pSurface);
        MFX_CHECK_STS(sts);

        extSurf.number  = i;
        extSurf.surface = *pSurface;

        pQueue->push_back( extSurf );
    }

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type )
    {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)
{
    memId;
    type;

    return MFX_ERR_UNSUPPORTED;
}

bool operator==(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return memcmp(&l, &r, sizeof(ENCODE_ENC_CTRL_CAPS)) == 0;
}

bool operator!=(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return !(l == r);
}

mfxStatus VAAPIEncoder::Execute(Task const & task, mfxHDL surface)
{
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");
    }

    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VASurfaceID reconSurface;
    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VABufferID  codedBuffer;
    std::vector<VABufferID> configBuffers;
    mfxU32      i;
    mfxU16      buffersCount = 0;
    mfxU32      packedDataSize = 0;
    VAStatus    vaSts;

    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);

    // update params
    {
        size_t slice_size_old = m_slice.size();
        //Destroy old buffers
        for(size_t i=0; i<slice_size_old; i++){
            MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
            MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
        }

        UpdatePPS(task, m_pps, m_reconQueue);

        if (slice_size_old != m_slice.size())
        {
            m_sliceBufferId.resize(m_slice.size());
            m_packeSliceHeaderBufferId.resize(m_slice.size());
            m_packedSliceBufferId.resize(m_slice.size());
        }
        UpdateSlice(task, m_sps, m_pps, m_slice);

        configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    }

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxBs = task.m_idxBs;
    if (idxBs < m_bsQueue.size())
        codedBuffer = m_bsQueue[idxBs].surface;
    else
        return MFX_ERR_UNKNOWN;

    m_pps.coded_buf = codedBuffer;

    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        {
            MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),
                                   1,
                                   &m_sps,
                                   &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = m_spsBufferId;
        }

        // 2. Picture level
        {
            MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPictureParameterBufferType,
                                   sizeof(m_pps),
                                   1,
                                   &m_pps,
                                   &m_ppsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = m_ppsBufferId;
        }

        // 3. Slice level
        for( i = 0; i < m_slice.size(); i++ )
        {
            MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    {
        // AUD
        if (task.m_insertHeaders & INSERT_AUD)
        {
            ENCODE_PACKEDHEADER_DATA * packedAud = PackHeader(task, AUD_NUT);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedAud->SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedAud->DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedAudHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedAud->DataLength, 1, packedAud->pData,
                                &m_packedAudBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedAudHeaderBufferId;
            configBuffers[buffersCount++] = m_packedAudBufferId;
        }
        // VPS
        if (task.m_insertHeaders & INSERT_VPS)
        {

            ENCODE_PACKEDHEADER_DATA * packedVps = PackHeader(task, VPS_NUT);

            packed_header_param_buffer.type = VAEncPackedHeaderHEVC_VPS;
            packed_header_param_buffer.has_emulation_bytes = 1;//!packedVps->SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedVps->DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedVpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedVpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedVpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedVps->DataLength, 1, packedVps->pData,
                                &m_packedVpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedVpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedVpsBufferId;
        }
        // SPS
        if (task.m_insertHeaders & INSERT_SPS)
        {
            ENCODE_PACKEDHEADER_DATA * packedSps = PackHeader(task, SPS_NUT);

            packed_header_param_buffer.type = VAEncPackedHeaderHEVC_SPS;
            packed_header_param_buffer.has_emulation_bytes = 1;//!packedSps->SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSps->DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedSps->DataLength, 1, packedSps->pData,
                                &m_packedSpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSpsBufferId;
        }
        if (task.m_insertHeaders & INSERT_PPS)
        {
            // PPS
            ENCODE_PACKEDHEADER_DATA * packedPps = PackHeader(task, PPS_NUT);

            packed_header_param_buffer.type = VAEncPackedHeaderHEVC_PPS;
            packed_header_param_buffer.has_emulation_bytes = !packedPps->SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedPps->DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedPpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedPps->DataLength, 1, packedPps->pData,
                                &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedPpsBufferId;
        }
        // SEI
        if (task.m_insertHeaders & INSERT_SEI)
        {
            ENCODE_PACKEDHEADER_DATA * packedSei = PackHeader(task, PREFIX_SEI_NUT);

            packed_header_param_buffer.type = VAEncPackedHeaderHEVC_SEI;
            packed_header_param_buffer.has_emulation_bytes = !packedSei->SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSei->DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedSei->DataLength, 1, packedSei->pData,
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }

        //Slice headers only
        for (size_t i = 0; i < m_slice.size(); i++)
        {
            mfxU32 sliceQpDeltaBitOffset = 0;
            ENCODE_PACKEDHEADER_DATA * packedSlice = PackSliceHeader(task, i, &sliceQpDeltaBitOffset);

            packed_header_param_buffer.type = VAEncPackedHeaderHEVC_Slice;
            packed_header_param_buffer.has_emulation_bytes = 0;
            packed_header_param_buffer.bit_length = packedSlice->DataLength;

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packeSliceHeaderBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                (packedSlice->DataLength + 7) / 8, 1, packedSlice->pData,
                                &m_packedSliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
            configBuffers[buffersCount++] = m_packedSliceBufferId[i];
        }
    }

    configBuffers[buffersCount++] = m_hrdBufferId;
    configBuffers[buffersCount++] = m_rateParamBufferId;

#ifdef PARALLEL_BRC_support
    configBuffers[buffersCount++] = m_BRCParallelParamBufferId;
#endif
    configBuffers[buffersCount++] = m_frameRateId;
    configBuffers[buffersCount++] = m_qualityLevelBufferId;

    assert(buffersCount <= configBuffers.size());

    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|ENCODE|AVC|PACKET_START|", "%p|%d", m_vaContextEncode, task.m_statusReportNumber);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaBeginPicture");

        vaSts = vaBeginPicture(
            m_vaDisplay,
            m_vaContextEncode,
            *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaRenderPicture");
        vaSts = vaRenderPicture(
            m_vaDisplay,
            m_vaContextEncode,
            &*configBuffers.begin(),
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for( i = 0; i < m_slice.size(); i++)
        {
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &m_sliceBufferId[i],
                1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaEndPicture");

        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|ENCODE|AVC|PACKET_END|", "%d|%d", m_vaContextEncode, task.m_statusReportNumber);

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber;
        currentFeedback.surface = *inputSurface;
        currentFeedback.idxBs   = task.m_idxBs;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryStatus(Task & task)
{

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    mfxStatus sts = MFX_ERR_NONE;

    VAStatus vaSts;
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_feedbackCache.size(); ++indxSurf)
    {
        ExtVASurface currentFeedback = m_feedbackCache[indxSurf];

        if(currentFeedback.number == task.m_statusReportNumber)
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            isFound  = true;
            break;
        }
    }

    if (!isFound)
        return MFX_ERR_UNKNOWN;

    // find used bitstream
    VABufferID codedBuffer;
    if( waitIdxBs < m_bsQueue.size())
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    else
        return MFX_ERR_UNKNOWN;

    if (waitSurface != VA_INVALID_SURFACE)
    {
        VASurfaceStatus surfSts = VASurfaceSkipped;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
        guard.Unlock();

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaSyncSurface");
            vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        surfSts = VASurfaceReady;
#else

        vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        if (VASurfaceReady == surfSts)
        {
            m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
            guard.Unlock();
        }
#endif

        switch (surfSts)
        {
            case VASurfaceReady:
                VACodedBufferSegment *codedBufferSegment;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        codedBuffer,
                        (void **)(&codedBufferSegment));
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }

                task.m_bsDataLength = codedBufferSegment->size;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                    vaUnmapBuffer( m_vaDisplay, codedBuffer );
                }
                return sts;

            case VASurfaceRendering:
            case VASurfaceDisplaying:
                return MFX_WRN_DEVICE_BUSY;

            case VASurfaceSkipped:
            default:
                assert(!"bad feedback status");
                return MFX_ERR_DEVICE_FAILED;
        }
    }

    return sts;
}

mfxStatus VAAPIEncoder::Destroy()
{
    if (m_vaContextEncode != VA_INVALID_ID)
    {
        vaDestroyContext(m_vaDisplay, m_vaContextEncode);
        m_vaContextEncode = VA_INVALID_ID;
    }

    if (m_vaConfig != VA_INVALID_ID)
    {
        vaDestroyConfig(m_vaDisplay, m_vaConfig);
        m_vaConfig = VA_INVALID_ID;
    }

    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_hrdBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rateParamBufferId, m_vaDisplay);

#ifdef PARALLEL_BRC_support
    MFX_DESTROY_VABUFFER(m_BRCParallelParamBufferId, m_vaDisplay);
#endif

    MFX_DESTROY_VABUFFER(m_frameRateId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_qualityLevelBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    for( mfxU32 i = 0; i < m_slice.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
    }
    MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedVpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedVpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceBufferId, m_vaDisplay);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Destroy()

}
#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
