/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder VAAPI
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>

#include "libmfx_core_vaapi.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"

#if defined(MFX_VA_ANDROID) && (MFX_ANDROID_VERSION == MFX_HONEYCOMB_VPG)
    // buffer names
    #undef VAEncSequenceParameterBufferH264
    #define VAEncSequenceParameterBufferH264 VAEncSequenceParameterBufferH264Ext

    #undef VAEncPictureParameterBufferH264
    #define VAEncPictureParameterBufferH264 VAEncPictureParameterBufferH264Ext

    #undef VAEncSliceParameterBufferH264
    #define VAEncSliceParameterBufferH264 VAEncSliceParameterBufferH264Ext

    // buffer types
    #undef VAEncSequenceParameterBufferType
    #define VAEncSequenceParameterBufferType VAEncSequenceParameterBufferExtType

    #undef VAEncPictureParameterBufferType
    #define VAEncPictureParameterBufferType VAEncPictureParameterBufferExtType

    #undef VAEncSliceParameterBufferType
    #define VAEncSliceParameterBufferType VAEncSliceParameterBufferExtType
#endif

#define VAAPI_SLICE_TYPE_P     0
#define VAAPI_SLICE_TYPE_B     1
#define VAAPI_SLICE_TYPE_I     2

// aya: temporary

#define ENTROPY_MODE_CAVLC      0
#define ENTROPY_MODE_CABAC      1

enum {
    VAAPI_RATECONTROL_CBR = 0,
    VAAPI_RATECONTROL_VBR = 1,
    VAAPI_RATECONTROL_CQP = 2
};


using namespace MfxHwH264Encode;
static const mfxU8 CABAC_INIT_IDC_TABLE[] = { 0, 2, 2, 1, 0, 0, 0, 0  };


static
mfxU8 ConvertFrameTypeMFX2VAAPI(mfxU8 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I: return VAAPI_SLICE_TYPE_I;
    case MFX_FRAMETYPE_P: return VAAPI_SLICE_TYPE_P;
    case MFX_FRAMETYPE_B: return VAAPI_SLICE_TYPE_B;
    default: assert(!"Unsupported frame type"); return 0;
    }

} // mfxU8 ConvertFrameTypeMFX2VAAPI(mfxU8 type)


static
mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
{
    switch (rateControl)
    {
    case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
    case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
    case MFX_RATECONTROL_AVBR: return VA_RC_VBR;
    case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
    default: assert(!"Unsupported RateControl"); return 0;
    }

} // mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)


static
VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)
{
    switch (type)
    {
        case MFX_PROFILE_AVC_BASELINE: return VAProfileH264Baseline;
        case MFX_PROFILE_AVC_MAIN:     return VAProfileH264Main;
        case MFX_PROFILE_AVC_HIGH:     return VAProfileH264High;
        default: assert(!"Unsupported profile type"); return VAProfileNone;
    }

} // mfxU8 ConvertFrameTypeMFX2VAAPI(mfxU8 type)


void FillSps(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferH264 & sps)
{
        mfxExtSpsHeader const * extSps = GetExtBuffer(par);


        sps.picture_width_in_mbs  = par.mfx.FrameInfo.Width >> 4;
        sps.picture_height_in_mbs = par.mfx.FrameInfo.Height >> 4;

        sps.level_idc   = par.mfx.CodecLevel;

        sps.intra_period = par.mfx.GopPicSize;
        sps.ip_period    = par.mfx.GopRefDist;

        sps.bits_per_second     = par.mfx.TargetKbps * 1000;

        sps.time_scale      = extSps->vui.timeScale;
        sps.num_units_in_tick = extSps->vui.numUnitsInTick;

        sps.max_num_ref_frames   =  mfxU8((extSps->maxNumRefFrames + 1) / 2);
        sps.seq_parameter_set_id = extSps->seqParameterSetId;
        sps.seq_fields.bits.chroma_format_idc    = extSps->chromaFormatIdc;
        sps.bit_depth_luma_minus8    = extSps->bitDepthLumaMinus8;
        sps.bit_depth_chroma_minus8  = extSps->bitDepthChromaMinus8;

        sps.seq_fields.bits.log2_max_frame_num_minus4               = extSps->log2MaxFrameNumMinus4;
        sps.seq_fields.bits.pic_order_cnt_type                      = extSps->picOrderCntType;
        sps.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4       = extSps->log2MaxPicOrderCntLsbMinus4;

        sps.num_ref_frames_in_pic_order_cnt_cycle   = extSps->numRefFramesInPicOrderCntCycle;
        sps.offset_for_non_ref_pic                  = extSps->offsetForNonRefPic;
        sps.offset_for_top_to_bottom_field          = extSps->offsetForTopToBottomField;

        Copy(sps.offset_for_ref_frame,  extSps->offsetForRefFrame);

        sps.frame_crop_left_offset                  = mfxU16(extSps->frameCropLeftOffset);
        sps.frame_crop_right_offset                 = mfxU16(extSps->frameCropRightOffset);
        sps.frame_crop_top_offset                   = mfxU16(extSps->frameCropTopOffset);
        sps.frame_crop_bottom_offset                = mfxU16(extSps->frameCropBottomOffset);
        sps.seq_fields.bits.seq_scaling_matrix_present_flag         = extSps->seqScalingMatrixPresentFlag;

        sps.seq_fields.bits.delta_pic_order_always_zero_flag        = extSps->deltaPicOrderAlwaysZeroFlag;

        sps.seq_fields.bits.frame_mbs_only_flag                     = extSps->frameMbsOnlyFlag;
        sps.seq_fields.bits.mb_adaptive_frame_field_flag            = extSps->mbAdaptiveFrameFieldFlag;
        sps.seq_fields.bits.direct_8x8_inference_flag               = extSps->direct8x8InferenceFlag;

        sps.vui_parameters_present_flag                 = extSps->vuiParametersPresentFlag;
        sps.vui_fields.bits.timing_info_present_flag    = extSps->vui.flags.timingInfoPresent;
        sps.vui_fields.bits.bitstream_restriction_flag  = extSps->vui.flags.bitstreamRestriction;
        sps.vui_fields.bits.log2_max_mv_length_horizontal  = extSps->vui.log2MaxMvLengthHorizontal;
        sps.vui_fields.bits.log2_max_mv_length_vertical  = extSps->vui.log2MaxMvLengthVertical;

        sps.frame_cropping_flag                     = extSps->frameCroppingFlag;

#if VA_CHECK_VERSION(0, 34, 0)
        sps.sar_height        = extSps->vui.sarHeight;
        sps.sar_width         = extSps->vui.sarWidth;
        sps.aspect_ratio_idc  = extSps->vui.aspectRatioIdc;
#endif

} // void FillSps(...)

mfxStatus SetHRD(
    MfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & hrdBuf_id)
{
#ifndef MFX_VA_ANDROID
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *hrd_param;

    if ( hrdBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, hrdBuf_id);
    }
    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                   1,
                   NULL,
                   &hrdBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 hrdBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeHRD;
    hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    hrd_param->initial_buffer_fullness = par.mfx.InitialDelayInKB * 8000;
    hrd_param->buffer_size = par.mfx.BufferSizeInKB * 8000;

    vaUnmapBuffer(m_vaDisplay, hrdBuf_id);
#endif // #ifndef MFX_VA_ANDROID
    return MFX_ERR_NONE;
} // void SetHRD(...)

mfxStatus SetRateControl(
    MfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & rateParamBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;

    if ( rateParamBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, rateParamBuf_id);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                   1,
                   NULL,
                   &rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 rateParamBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRateControl;
    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

    rate_param->bits_per_second = par.mfx.MaxKbps * 1000;
    rate_param->window_size     = par.mfx.Convergence * 100;
    if(par.mfx.MaxKbps)
        rate_param->target_percentage = par.mfx.TargetKbps / par.mfx.MaxKbps * 100;

    vaUnmapBuffer(m_vaDisplay, rateParamBuf_id);
} // void SetRateControl(...)

mfxStatus SetFrameRate(
    MfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & frameRateBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, frameRateBuf_id);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 frameRateBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    frameRate_param->framerate = mfxU16(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);

    vaUnmapBuffer(m_vaDisplay, frameRateBuf_id);
} // void SetFrameRate(...)

void FillConstPartOfPps(
    MfxVideoParam const & par,
    VAEncPictureParameterBufferH264 & pps)
    {
        mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
        mfxExtSpsHeader const *       extSps = GetExtBuffer(par);

        pps.seq_parameter_set_id = extSps->seqParameterSetId;
        pps.pic_parameter_set_id = extPps->picParameterSetId;

        pps.last_picture = 0;// aya???
        pps.frame_num = 0;   // aya???

        pps.pic_init_qp = extPps->picInitQpMinus26 + 26;
        pps.num_ref_idx_l0_active_minus1            = extPps->numRefIdxL0DefaultActiveMinus1;
        pps.num_ref_idx_l1_active_minus1            = extPps->numRefIdxL1DefaultActiveMinus1;
        pps.chroma_qp_index_offset                  = extPps->chromaQpIndexOffset;
        pps.second_chroma_qp_index_offset           = extPps->secondChromaQpIndexOffset;

        pps.pic_fields.bits.deblocking_filter_control_present_flag  = 1;//aya: ???
        pps.pic_fields.bits.entropy_coding_mode_flag                = extPps->entropyCodingModeFlag;
        //assert(ENTROPY_MODE_CABAC == pps.pic_fields.bits.entropy_coding_mode_flag);

        pps.pic_fields.bits.pic_order_present_flag                  = extPps->bottomFieldPicOrderInframePresentFlag;
        pps.pic_fields.bits.weighted_pred_flag                      = extPps->weightedPredFlag;
        pps.pic_fields.bits.weighted_bipred_idc                     = extPps->weightedBipredIdc;
        pps.pic_fields.bits.constrained_intra_pred_flag             = extPps->constrainedIntraPredFlag;
        pps.pic_fields.bits.transform_8x8_mode_flag                 = extPps->transform8x8ModeFlag;
        pps.pic_fields.bits.pic_scaling_matrix_present_flag         = extPps->picScalingMatrixPresentFlag;

        mfxU32 i = 0;
        for( i = 0; i < 16; i++ )
        {
            pps.ReferenceFrames[i].picture_id = VA_INVALID_ID;
        }

    } // void FillConstPartOfPps(...)


void UpdatePPS(
    DdiTask const & task,
    mfxU32          fieldId,
    VAEncPictureParameterBufferH264 & pps)
{
    pps.frame_num = task.m_frameNum;

    pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    pps.CurrPic.TopFieldOrderCnt = task.GetPoc(0);
    pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(1);

    mfxU32 i = 0;
    for( i = 0; i < task.m_dpb[fieldId].Size(); i++ )
    {
        pps.ReferenceFrames[i].frame_idx        = task.m_dpb[fieldId][i].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].flags            = (task.m_dpb[fieldId][i].m_frameIdx >> 7) & 1;//aya: need convert???

        pps.ReferenceFrames[i].TopFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[1];
    }

    for (; i < 16; i++)
    {
        pps.ReferenceFrames[i].frame_idx = 0xff;
        pps.ReferenceFrames[i].TopFieldOrderCnt   = 0;
        pps.ReferenceFrames[i].BottomFieldOrderCnt   = 0;
    }

} // void UpdatePPS(...)


void UpdateSlice(
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par)
{
/*****************************************************************************************/
    //This is a workarround. See CQ 10772. OTC 520
    //mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    mfxU32 numPics = 1;
/*****************************************************************************************/
    mfxU32 numSlice = slice.size();

    SliceDividerSnbStyle divider(
        numSlice,
        sps.picture_width_in_mbs,
        sps.picture_height_in_mbs / numPics);

    for( size_t i = 0; i < slice.size(); ++i, divider.Next() )
    {
        slice[i].macroblock_address = divider.GetFirstMbInSlice();//0;
        slice[i].num_macroblocks = divider.GetNumMbInSlice();//sps.picture_width_in_mbs * sps.picture_height_in_mbs;

        // slice.id = ???

        mfxU32 ref = 0;
        for (ref = 0; ref < task.m_list0[fieldId].Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx = task.m_list0[fieldId][ref] & 0x7f;
            slice[i].RefPicList0[ref].flags     = (task.m_list0[fieldId][ref] >> 7) & 1;//aya: need convert???
        }

        for (ref = 0; ref < task.m_list1[fieldId].Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx = task.m_list1[fieldId][ref] & 0x7f;
            slice[i].RefPicList1[ref].flags     = (task.m_list1[fieldId][ref] >> 7) & 1;//aya: need convert???
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertFrameTypeMFX2VAAPI( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = mfxU8(IPP_MAX(1, task.m_list0[fieldId].Size()) - 1);//0;
        slice[i].num_ref_idx_l1_active_minus1 = mfxU8(IPP_MAX(1, task.m_list1[fieldId].Size()) - 1);//0;
        slice[i].num_ref_idx_active_override_flag   =
                    slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
                    slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));

        slice[i].delta_pic_order_cnt_bottom         = 0;
        slice[i].delta_pic_order_cnt[0]             = 0;
        slice[i].delta_pic_order_cnt[1]             = 0;
        slice[i].luma_log2_weight_denom             = 0;
        slice[i].chroma_log2_weight_denom           = 0;

        slice[i].cabac_init_idc                     = CABAC_INIT_IDC_TABLE[par.mfx.TargetUsage];
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

        slice[i].disable_deblocking_filter_idc = 0;
        slice[i].slice_alpha_c0_offset_div2 = 0;
        slice[i].slice_beta_offset_div2 = 0;
    }

} // void UpdateSlice(...)


////////////////////////////////////////////////////////////////////////////////////////////
//                    HWEncoder based on VAAPI
////////////////////////////////////////////////////////////////////////////////////////////

using namespace MfxHwH264Encode;

VAAPIEncoder::VAAPIEncoder()
: m_core(NULL)
, m_vaDisplay(0)
, m_vaContextEncode(0)
, m_vaConfig(0)
, m_spsBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_rateParamBufferId(VA_INVALID_ID)
, m_frameRateId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
//, m_sliceBufferId(VA_INVALID_ID)
, m_packedSpsHeaderBufferId(VA_INVALID_ID)
, m_packedSpsBufferId(VA_INVALID_ID)
, m_packedPpsHeaderBufferId(VA_INVALID_ID)
, m_packedPpsBufferId(VA_INVALID_ID)
, m_packedSeiHeaderBufferId(VA_INVALID_ID)
, m_packedSeiBufferId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE* core,
    GUID guid,
    mfxU32 width,
    mfxU32 height)
{
    m_core = core;

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    //VAAPIInterface* pVAAPIInterface = QueryCoreInterface<VAAPIInterface>(m_core, MFXICOREVAAPI_GUID);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);

    // finish
    guid;

    m_width  = width;
    m_height = height;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    if(IsMvcProfile(par.mfx.CodecProfile))
        return MFX_WRN_PARTIAL_ACCELERATION;

    VAStatus vaSts;

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);

    mfxStatus mfxSts = hwcore->GetD3DService(m_width, m_height, &m_vaDisplay);
    MFX_CHECK_STS(mfxSts);

    // should be moved to core->IsGuidSupported()
    {
        VAEntrypoint* pEntrypoints = NULL;
        mfxI32 entrypointsCount = 0, entrypointsIndx = 0;
        mfxI32 maxNumEntrypoints   = vaMaxNumEntrypoints(m_vaDisplay);

        if(maxNumEntrypoints)
            pEntrypoints = (VAEntrypoint*)ippsMalloc_8u(maxNumEntrypoints*sizeof(VAEntrypoint));
        else
            return MFX_ERR_DEVICE_FAILED;

        vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            pEntrypoints,
            &entrypointsCount);

        bool bEncodeEnable = false;
        for( entrypointsIndx = 0; entrypointsIndx < entrypointsCount; entrypointsIndx++ )
        {
            if( VAEntrypointEncSlice == pEntrypoints[entrypointsIndx] )
            {
                bEncodeEnable = true;
                break;
            }
        }
        ippsFree(pEntrypoints);
        if( !bEncodeEnable )
        {
            return MFX_ERR_DEVICE_FAILED;// unsupport?
        }
    }
    // IsGuidSupported()

    // Configuration
    VAConfigAttrib attrib[2];

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                          VAEntrypointEncSlice,
                          &attrib[0], 2);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    if(vaRCType == VA_RC_VBR)
        vaRCType = VA_RC_CBR; // wo for OTC driver

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        VAEntrypointEncSlice,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        VA_PROGRESSIVE,
        0,
        0,
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_videoParam = par;

    m_slice.resize(par.mfx.NumSlice);// aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    for(int i = 0; i < m_sliceBufferId.size(); i++)
    {
        m_sliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    Zero(m_packedSps);
    Zero(m_packedPps);

    //------------------------------------------------------------------
    // aya: workarround(WO) hack, OTC driver doesn't support
    mfxExtPpsHeader * extPps = GetExtBuffer(m_videoParam);
    //extPps->weightedBipredIdc    = 0;
    //extPps->transform8x8ModeFlag = 0;

    mfxExtSpsHeader * extSpsWO = GetExtBuffer(m_videoParam);

#ifndef MFX_VA_ANDROID
    extSpsWO->frameMbsOnlyFlag = 1;
    extSpsWO->picOrderCntType = 0;
#endif

    m_videoParam.mfx.TargetUsage = 0;
    //------------------------------------------------------------------

    FillSps(m_videoParam, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    FillConstPartOfPps(m_videoParam, m_pps);

    // prepare config buffers {
    // SPS
    //{
        VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

        mfxExtSpsHeader * extSps = GetExtBuffer(m_videoParam);

        // pack sps and pps nal units here
        // they will not change

        OutputBitstream obsSPS(
            m_packedSps,
            m_packedSps + MAX_PACKED_SPSPPS_SIZE,
            true); // pack with emulation control

        WriteSpsHeader(obsSPS, *extSps);

        mfxU32 spsHeaderLengthInBits = obsSPS.GetNumBits();
        mfxU32 spsHeaderLength = (obsSPS.GetNumBits() + 7) / 8;

        unsigned int offset_in_bytes = 0;
        unsigned int length_in_bits = spsHeaderLengthInBits;

        //-------------------------------------------------

        packed_header_param_buffer.type = VAEncPackedHeaderSequence;
        packed_header_param_buffer.has_emulation_bytes = 1; // FIXME
        packed_header_param_buffer.bit_length  = length_in_bits;

        vaSts = vaCreateBuffer(m_vaDisplay,
                               m_vaContextEncode,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packed_header_param_buffer),
                               1,
                               &packed_header_param_buffer,
                               &m_packedSpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderDataBufferType,
                                   (length_in_bits + 7) / 8,
                                   1,
                                   m_packedSps,
                                   &m_packedSpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    // }

    // PPS
    // {

        OutputBitstream obsPPS(
            m_packedPps,
            m_packedPps + MAX_PACKED_SPSPPS_SIZE,
            true); // pack with emulation control

        WritePpsHeader(obsPPS, *extPps);

        mfxU32 ppsHeaderLengthInBits = obsPPS.GetNumBits();
        mfxU32 ppsHeaderLength = (obsPPS.GetNumBits() + 7) / 8;

        offset_in_bytes = 0;
        length_in_bits = ppsHeaderLengthInBits;
        //-------------------------------------------------------------

        offset_in_bytes = 0;

        packed_header_param_buffer.type = VAEncPackedHeaderPicture;
        packed_header_param_buffer.has_emulation_bytes = 1; // FIXME
        packed_header_param_buffer.bit_length  = length_in_bits;

        vaSts = vaCreateBuffer(m_vaDisplay,
                               m_vaContextEncode,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packed_header_param_buffer),
                               1,
                               &packed_header_param_buffer,
                               &m_packedPpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaCreateBuffer(m_vaDisplay,
                               m_vaContextEncode,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, m_packedPps,
                               &m_packedPpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    // }
    //}

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)
{
    m_videoParam = par;

    FillSps(m_videoParam, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    FillConstPartOfPps(m_videoParam, m_pps);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    type;

    // request linear bufer
    request.Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    request.reserved[0] = m_vaContextEncode;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)


mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)
{
    //caps;
    caps.MaxPicWidth  = 1920;
    caps.MaxPicHeight = 1088;
    caps.NoInterlacedField = 1; // no interlaced encoding
    caps.BRCReset = 0; // no bitrate resolution control
    caps.VCMBitrateControl = 0; //Video conference mode

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)


#if defined(AYA_DEBUG)
mfxStatus VAAPIEncoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)


mfxStatus VAAPIEncoder::SetEncCtrlCaps(
    ENCODE_ENC_CTRL_CAPS const & caps)
{
    caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::SetEncCtrlCaps(...)


mfxStatus VAAPIEncoder::QueryMbDataLayout(
    ENCODE_SET_SEQUENCE_PARAMETERS_H264 &sps,
    ENCODE_MBDATA_LAYOUT& layout)
{
    sps;
    layout;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryMbDataLayout(...)
#endif

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type )
    {
        pQueue = &m_bsQueue;
    }
    else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

        mfxStatus sts;
        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++)
        {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number  = i;
            extSurf.surface = *pSurface;

            pQueue->push_back( extSurf );
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)
{
    memId;
    type;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)


bool operator==(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return memcmp(&l, &r, sizeof(ENCODE_ENC_CTRL_CAPS)) == 0;
}

bool operator!=(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return !(l == r);
}

mfxStatus VAAPIEncoder::Execute(
    mfxHDL          surface,
    DdiTask const & task,
    mfxU32          fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");

    VAStatus vaSts;

    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VASurfaceID refSurface[2]; // 2 - SNB limitation
    VASurfaceID reconSurface;
    VABufferID codedBuffer;
    mfxU32 i;

    // update params
    UpdatePPS(task, fieldId, m_pps);
    UpdateSlice(task, fieldId, m_sps, m_pps, m_slice, m_videoParam);

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxBs = task.m_idxBs[fieldId];
    if( idxBs < m_bsQueue.size() )
    {
        codedBuffer = m_bsQueue[idxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    // find reconstructed surface
    int idxRecon = task.m_idxRecon;
    if( idxRecon < m_reconQueue.size())
    {
        reconSurface = m_reconQueue[ idxRecon ].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }


    //{ // workarround for OTC driver
        // L0
        mfxU32 dpbIndx = 0;
        mfxU32 refIndx = m_slice[0].RefPicList0[0].frame_idx;
        if( refIndx < 16 )
        {
            dpbIndx = task.m_dpb[fieldId][refIndx].m_frameIdx; // L0 -> DPB
        }
        else
        {
            dpbIndx = 127; // INVALID
        }

        if( dpbIndx < m_reconQueue.size())
        {
            refSurface[0] = m_reconQueue[ dpbIndx ].surface;
        }
        else
        {
            refSurface[0] = VA_INVALID_ID;
        }

        // L1
        refIndx = m_slice[0].RefPicList1[0].frame_idx;
        if( refIndx < 16 )
        {
            dpbIndx = task.m_dpb[fieldId][refIndx].m_frameIdx; // L1 -> DPB
        }
        else
        {
            dpbIndx = 127; // INVALID
        }
        if( dpbIndx < m_reconQueue.size())
        {
            refSurface[1] = m_reconQueue[ dpbIndx ].surface;
        }
        else
        {
            refSurface[1] = VA_INVALID_ID;
        }


        // update PPS - resources
        m_pps.coded_buf = codedBuffer;
        m_pps.CurrPic.picture_id = reconSurface;

        m_pps.ReferenceFrames[0].picture_id = refSurface[0];
        m_pps.ReferenceFrames[1].picture_id = refSurface[1];

        // wo for OTC driver
        if( VAAPI_SLICE_TYPE_I == m_slice[0].slice_type )
        {
            m_pps.ReferenceFrames[0].picture_id = VA_INVALID_ID;
            m_pps.ReferenceFrames[1].picture_id = VA_INVALID_ID;
        }
        else if( VAAPI_SLICE_TYPE_P == m_slice[0].slice_type )
        {
            m_pps.ReferenceFrames[0].picture_id = refSurface[0];
            m_pps.ReferenceFrames[1].picture_id = refSurface[0];
        }
        // end of wo

    //}
    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        if( VA_INVALID_ID == m_spsBufferId )
        {
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),
                                   1,
                                   &m_sps,
                                   &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }

        // 2. Picture level
        {
            //UpdatePPS_OLD(task, filedId, m_pps);// aya: see above

            if ( m_ppsBufferId != VA_INVALID_ID)
            {
                vaDestroyBuffer(m_vaDisplay, m_ppsBufferId);
                m_ppsBufferId = VA_INVALID_ID;
            }

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPictureParameterBufferType,
                                   sizeof(m_pps),
                                   1,
                                   &m_pps,
                                   &m_ppsBufferId);


            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        //--------------------------------------------------------------
        // PPS
        // {
            Zero(m_packedPps);
            mfxExtPpsHeader * extPps = GetExtBuffer(m_videoParam);

            OutputBitstream obsPPS(
                m_packedPps,
                m_packedPps + MAX_PACKED_SPSPPS_SIZE,
                true); // pack with emulation control

            WritePpsHeader(obsPPS, *extPps);

            mfxU32 ppsHeaderLengthInBits = obsPPS.GetNumBits();
            mfxU32 ppsHeaderLength = (obsPPS.GetNumBits() + 7) / 8;

            mfxU32 offset_in_bytes = 0;
            mfxU32 length_in_bits = ppsHeaderLengthInBits;
            offset_in_bytes = 0;

            VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

            packed_header_param_buffer.type = VAEncPackedHeaderPicture;
            packed_header_param_buffer.has_emulation_bytes = 1;//1; // FIXME
            packed_header_param_buffer.bit_length = length_in_bits;

            if ( m_packedPpsHeaderBufferId != VA_INVALID_ID)
            {
                vaDestroyBuffer(m_vaDisplay, m_packedPpsHeaderBufferId);
                m_packedPpsHeaderBufferId = VA_INVALID_ID;
            }

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderParameterBufferType,
                                   sizeof(packed_header_param_buffer),
                                   1,
                                   &packed_header_param_buffer,
                                   &m_packedPpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            if ( m_packedPpsBufferId != VA_INVALID_ID)
            {
                vaDestroyBuffer(m_vaDisplay, m_packedPpsBufferId);
                m_packedPpsBufferId = VA_INVALID_ID;
            }
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderDataBufferType,
                                   (length_in_bits + 7) / 8, 1, m_packedPps,
                                   &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        // }
        //--------------------------------------------------------------
        // SEI
#ifndef MFX_VA_ANDROID // FIXME (problem with VPG driver)
            if(sei.Size() > 0)
            {
                std::vector<mfxU8> packedSei(sei.Size());
                Zero(packedSei);
                OutputBitstream obsSEI(Begin(packedSei), End(packedSei), true); // pack with emulation control     

                obsSEI.PutRawBytes(sei.Buffer(), sei.Buffer() + sei.Size());

                length_in_bits = obsSEI.GetNumBits();

                packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
                packed_header_param_buffer.has_emulation_bytes = 1;
                packed_header_param_buffer.bit_length = length_in_bits;

                if ( m_packedSeiHeaderBufferId != VA_INVALID_ID)
                {
                    vaDestroyBuffer(m_vaDisplay, m_packedSeiHeaderBufferId);
                    m_packedSeiHeaderBufferId = VA_INVALID_ID;
                }

                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        VAEncPackedHeaderParameterBufferType,
                        sizeof(packed_header_param_buffer),
                        1,
                        &packed_header_param_buffer,
                        &m_packedSeiHeaderBufferId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                if ( m_packedSeiBufferId != VA_INVALID_ID)
                {
                    vaDestroyBuffer(m_vaDisplay, m_packedSeiBufferId);
                    m_packedSeiBufferId = VA_INVALID_ID;
                }
                vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncPackedHeaderDataBufferType,
                                    (length_in_bits + 7) / 8, 1, Begin(packedSei),
                                    &m_packedSeiBufferId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
#endif
        //--------------------------------------------------------------

        // 3. Slice level
        {
            //UpdateSlice_OLD(task, filedId, m_sps, m_pps, m_slice);// aya: see above
            for( i = 0; i < m_slice.size(); i++ )
            {
                if ( m_sliceBufferId[i] != VA_INVALID_ID)
                {
                    vaDestroyBuffer(m_vaDisplay, m_sliceBufferId[i]);
                    m_sliceBufferId[i] = VA_INVALID_ID;
                }

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
    }
    //------------------------------------------------------------------
    // buffer registration
    //------------------------------------------------------------------
    VABufferID configBuffers[MAX_CONFIG_BUFFERS_COUNT];
    int buffersCount = 0;

    configBuffers[buffersCount++] = m_spsBufferId;
#ifndef MFX_VA_ANDROID // TODO For different VPG and OTC
    configBuffers[buffersCount++] = m_hrdBufferId;
#endif
    configBuffers[buffersCount++] = m_rateParamBufferId;
    //configBuffers[buffersCount++] = m_frameRateId;
    configBuffers[buffersCount++] = m_ppsBufferId;

    if( task.m_insertSps[fieldId] )
    {
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSpsBufferId;
    }

    configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
    configBuffers[buffersCount++] = m_packedPpsBufferId;
#ifndef MFX_VA_ANDROID
    if(sei.Size() > 0)
    {
        configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSeiBufferId;
    }
#endif
    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
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
            configBuffers,
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

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        
        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.idxBs    = task.m_idxBs[fieldId];
        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);
    
    for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
    {
        ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

        if( currentFeedback.number == task.m_statusReportNumber[fieldId] )
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;

            isFound  = true;

            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    // find used bitstream
    VABufferID codedBuffer;
    if( waitIdxBs < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

#if 0
    //------------------------------------------
    // (2) Syncronization

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VASurfaceStatus surfSts = VASurfaceReady;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VACodedBufferSegment *codedBufferSegment;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
        vaSts = vaMapBuffer(
            m_vaDisplay,
            codedBuffer,
            (void **)(&codedBufferSegment));
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    task.m_bsDataLength[fieldId] = codedBufferSegment->size;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
        vaUnmapBuffer( m_vaDisplay, codedBuffer );
    }

    // remove task
    m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );

    return MFX_ERR_NONE;
#else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

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

            task.m_bsDataLength[fieldId] = codedBufferSegment->size;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                vaUnmapBuffer( m_vaDisplay, codedBuffer );
            }

            // remove task
            m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );
            return MFX_ERR_NONE;
            
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;
            
        case VASurfaceSkipped:  
        default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
    }
#endif

} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus VAAPIEncoder::Destroy()
{
    if( m_vaContextEncode )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextEncode );
        m_vaContextEncode = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }

    // destroy config buffers
    if( VA_INVALID_ID != m_spsBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_spsBufferId);
    }

    if( VA_INVALID_ID != m_hrdBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_hrdBufferId);
    }

    if( VA_INVALID_ID != m_rateParamBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_rateParamBufferId);
    }

    if( VA_INVALID_ID != m_frameRateId )
    {
        vaDestroyBuffer(m_vaDisplay, m_frameRateId);
    }

    if( VA_INVALID_ID != m_ppsBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_ppsBufferId);
    }

    for( mfxU32 i = 0; i < m_slice.size(); i++ )
    {
        if( VA_INVALID_ID != m_sliceBufferId[i] )
        {
            vaDestroyBuffer(m_vaDisplay, m_sliceBufferId[i]);
        }
        m_sliceBufferId[i] = VA_INVALID_ID;
    }

    //
    if( VA_INVALID_ID != m_packedSpsHeaderBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedSpsHeaderBufferId);
    }
    if( VA_INVALID_ID != m_packedSpsBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedSpsBufferId);
    }
    if( VA_INVALID_ID != m_packedPpsHeaderBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedPpsHeaderBufferId);
    }
    if( VA_INVALID_ID != m_packedPpsBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedPpsBufferId);
    }
    if( VA_INVALID_ID != m_packedSeiHeaderBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedSeiHeaderBufferId);
    }
    if( VA_INVALID_ID != m_packedSeiBufferId )
    {
        vaDestroyBuffer(m_vaDisplay, m_packedSeiBufferId);
    }

    m_spsBufferId   = VA_INVALID_ID;
    m_hrdBufferId   = VA_INVALID_ID;
    m_rateParamBufferId   = VA_INVALID_ID;
    m_frameRateId   = VA_INVALID_ID;
    m_ppsBufferId   = VA_INVALID_ID;

    m_packedSpsHeaderBufferId = VA_INVALID_ID;
    m_packedSpsBufferId       = VA_INVALID_ID;
    m_packedPpsHeaderBufferId = VA_INVALID_ID;
    m_packedPpsBufferId       = VA_INVALID_ID;
    m_packedSeiHeaderBufferId = VA_INVALID_ID;
    m_packedSeiBufferId       = VA_INVALID_ID;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Destroy()

#if defined (AYA_DEBUG)
mfxStatus MfxHwH264Encode::QueryHwCaps(mfxU32 adapterNum, ENCODE_CAPS & hwCaps)
{

    adapterNum;
    hwCaps;

    return MFX_ERR_NONE;

} // mfxStatus MfxHwH264Encode::QueryHwCaps(mfxU32 adapterNum, ENCODE_CAPS & hwCaps)
#endif

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
/* EOF */
