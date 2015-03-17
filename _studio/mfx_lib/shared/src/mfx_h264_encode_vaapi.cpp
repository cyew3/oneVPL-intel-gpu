/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder VAAPI
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>
#include <va/va_enc.h>
#include "libmfx_core_vaapi.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"


//#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
#include "mfxfei.h"
#include <va/va_enc_h264.h>
//#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
//#include <va/vendor/va_intel_statistics.h>

#include "mfx_h264_preenc.h"

#define TMP_DISABLE 0

#endif

#if defined(_DEBUG)
//#define mdprintf fprintf
#define mdprintf(...)
#else
#define mdprintf(...)
#endif

#define SKIP_FRAME_SUPPORT
#define ROLLING_INTRA_REFRESH_SUPPORT

#ifndef MFX_VA_ANDROID
    #define MAX_FRAME_SIZE_SUPPORT
    #define TRELLIS_QUANTIZATION_SUPPORT
#endif

using namespace MfxHwH264Encode;

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
VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type)
{
    switch (type)
    {
        case MFX_PROFILE_AVC_BASELINE: return VAProfileH264Baseline;
        case MFX_PROFILE_AVC_MAIN:     return VAProfileH264Main;
        case MFX_PROFILE_AVC_HIGH:     return VAProfileH264High;
        default:
            //assert(!"Unsupported profile type");
            return VAProfileH264High; //VAProfileNone;
    }

} // VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)

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

    hrd_param->initial_buffer_fullness = par.calcParam.initialDelayInKB * 8000;
    hrd_param->buffer_size = par.calcParam.bufferSizeInKB * 8000;

    vaUnmapBuffer(vaDisplay, hrdBuf_id);

    return MFX_ERR_NONE;
} // void SetHRD(...)

void FillBrcStructures(
    MfxVideoParam const & par,
    VAEncMiscParameterRateControl & vaBrcPar,
    VAEncMiscParameterFrameRate   & vaFrameRate)
{
    Zero(vaBrcPar);
    Zero(vaFrameRate);
    vaBrcPar.bits_per_second = par.calcParam.maxKbps * 1000;
    if(par.calcParam.maxKbps)
        vaBrcPar.target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);
    vaFrameRate.framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);
}

mfxStatus SetRateControl(
    MfxVideoParam const & par,
    mfxU32       mbbrc,
    mfxU8        minQP,
    mfxU8        maxQP,
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

    rate_param->bits_per_second = par.calcParam.maxKbps * 1000;
    rate_param->window_size     = par.mfx.Convergence * 100;

    rate_param->min_qp = minQP;
    rate_param->max_qp = maxQP;

    if(par.calcParam.maxKbps)
        rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);
/*
 * MBBRC control
 * Control VA_RC_MB 0: default, 1: enable, 2: disable, other: reserved
 */
    rate_param->rc_flags.bits.mb_rate_control = mbbrc & 0xf;
    rate_param->rc_flags.bits.reset = isBrcResetRequired;

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

static mfxStatus SetMaxFrameSize(
    const UINT   userMaxFrameSize,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameSizeBuf_id)
{
    VAEncMiscParameterBuffer             *misc_param;
    VAEncMiscParameterBufferMaxFrameSize *p_maxFrameSize;

    if ( frameSizeBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, frameSizeBuf_id);
    }

    VAStatus vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferMaxFrameSize),
                   1,
                   NULL,
                   &frameSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, frameSizeBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeMaxFrameSize;
    p_maxFrameSize = (VAEncMiscParameterBufferMaxFrameSize *)misc_param->data;

    p_maxFrameSize->max_frame_size = userMaxFrameSize*8;    // in bits for libva

    vaUnmapBuffer(vaDisplay, frameSizeBuf_id);

    return MFX_ERR_NONE;
}

static mfxStatus SetTrellisQuantization(
    mfxU32       trellis,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & trellisBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer        *misc_param;
    VAEncMiscParameterQuantization  *trellis_param;

    if ( trellisBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, trellisBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterQuantization),
                   1,
                   NULL,
                   &trellisBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, trellisBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeQuantization;
    trellis_param = (VAEncMiscParameterQuantization *)misc_param->data;

    trellis_param->quantization_flags.value = trellis;

    vaUnmapBuffer(vaDisplay, trellisBuf_id);

    return MFX_ERR_NONE;
} // void SetTrellisQuantization(...)

static mfxStatus SetRollingIntraRefresh(
    IntraRefreshState const & rirState,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rirBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRIR    *rir_param;

    if ( rirBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, rirBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRIR),
                   1,
                   NULL,
                   &rirBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, rirBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRIR;
    rir_param = (VAEncMiscParameterRIR *)misc_param->data;

    rir_param->rir_flags.value             = rirState.refrType;
    rir_param->intra_insertion_location    = rirState.IntraLocation;
    rir_param->intra_insert_size           = rirState.IntraSize;
    rir_param->qp_delta_for_inserted_intra = mfxU8(rirState.IntRefQPDelta);

    vaUnmapBuffer(vaDisplay, rirBuf_id);

    return MFX_ERR_NONE;
} // void SetRollingIntraRefresh(...)

mfxStatus SetPrivateParams(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & privateParams_id,
    mfxEncodeCtrl const * pCtrl = 0)
{
    if (!IsSupported__VAEncMiscParameterPrivate()) return MFX_ERR_UNSUPPORTED;

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterPrivate *private_param;
    mfxExtCodingOption2 const * extOpt2  = GetExtBuffer(par);
    mfxExtCodingOption3 const * extOpt3  = GetExtBuffer(par);

    if ( privateParams_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, privateParams_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterPrivate),
                   1,
                   NULL,
                   &privateParams_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 privateParams_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypePrivate;
    private_param = (VAEncMiscParameterPrivate *)misc_param->data;

    private_param->target_usage = (unsigned int)(par.mfx.TargetUsage);
    private_param->useRawPicForRef = extOpt2 && IsOn(extOpt2->UseRawRef);

    if (extOpt3)
    {
        private_param->directBiasAdjustmentEnable       = IsOn(extOpt3->DirectBiasAdjustment);
        private_param->globalMotionBiasAdjustmentEnable = IsOn(extOpt3->GlobalMotionBiasAdjustment);

        if (private_param->globalMotionBiasAdjustmentEnable && extOpt3->MVCostScalingFactor < 4)
            private_param->HMEMVCostScalingFactor = extOpt3->MVCostScalingFactor;
    }

    if (pCtrl)
    {
        mfxExtCodingOption2 const * extOpt2rt  = GetExtBuffer(*pCtrl);
        mfxExtCodingOption3 const * extOpt3rt  = GetExtBuffer(*pCtrl);
        mfxExtAVCEncodeCtrl const * extPCQC    = GetExtBuffer(*pCtrl);

        if (extOpt2rt)
            private_param->useRawPicForRef = IsOn(extOpt2rt->UseRawRef);

        if (extOpt3rt)
        {
            private_param->directBiasAdjustmentEnable       = IsOn(extOpt3rt->DirectBiasAdjustment);
            private_param->globalMotionBiasAdjustmentEnable = IsOn(extOpt3rt->GlobalMotionBiasAdjustment);

            if (private_param->globalMotionBiasAdjustmentEnable && extOpt3rt->MVCostScalingFactor < 4)
                private_param->HMEMVCostScalingFactor = extOpt3rt->MVCostScalingFactor;
        }

        if (extPCQC)
        {
            private_param->skipCheckDisable = (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_DISABLE;
            private_param->FTQEnable        = (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_FTQ_ON;
            private_param->FTQOverride      = private_param->FTQEnable || (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_FTQ_OFF;

            if (extPCQC->SkipCheck & MFX_SKIP_CHECK_SET_THRESHOLDS)
            {
                if (private_param->FTQEnable)
                {
                    private_param->FTQSkipThresholdLUTInput = 1;
                    for (mfxU32 i = 0; i < 52; i ++)
                        private_param->FTQSkipThresholdLUT[i] = (mfxU8)extPCQC->SkipThreshold[i];
                }
                else
                {
                    private_param->NonFTQSkipThresholdLUTInput = 1;
                    Copy(private_param->NonFTQSkipThresholdLUT, extPCQC->SkipThreshold);
                }
            }

            if (IsOn(extPCQC->LambdaValueFlag))
            {
                private_param->lambdaValueLUTInput = 1;
                Copy(private_param->lambdaValueLUT, extPCQC->LambdaValue );
            }
        }
    }

    vaUnmapBuffer(vaDisplay, privateParams_id);

    return MFX_ERR_NONE;
} // void SetPrivateParams(...)

mfxStatus SetSkipFrame(
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & skipParam_id,
    mfxU8 skipFlag,
    mfxU8 numSkipFrames,
    mfxU32 sizeSkipFrames)
{
#ifdef SKIP_FRAME_SUPPORT
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterSkipFrame *skipParam;

    if (skipParam_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, skipParam_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
        vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterSkipFrame),
        1,
        NULL,
        &skipParam_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
        skipParam_id,
        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeSkipFrame;
    skipParam = (VAEncMiscParameterSkipFrame *)misc_param->data;

    skipParam->skip_frame_flag = skipFlag;
    skipParam->num_skip_frames = numSkipFrames;
    skipParam->size_skip_frames = sizeSkipFrames;

    vaUnmapBuffer(vaDisplay, skipParam_id);

    return MFX_ERR_NONE;
#else
    return MFX_ERR_UNSUPPORTED;
#endif
}

static mfxStatus SetROI(
    DdiTask const & task,
    std::vector<VAEncROI> & arrayVAEncROI,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & roiParam_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferROI *roi_Param;

    if (roiParam_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, roiParam_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
        vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferROI),
        1,
        NULL,
        &roiParam_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
        roiParam_id,
        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeROI;
    roi_Param = (VAEncMiscParameterBufferROI *)misc_param->data;
    memset(roi_Param, 0, sizeof(VAEncMiscParameterBufferROI));

    if (task.m_numRoi)
    {
        roi_Param->num_roi = task.m_numRoi;

        if (arrayVAEncROI.size() < task.m_numRoi)
        {
            arrayVAEncROI.resize(task.m_numRoi);
        }
        roi_Param->roi = Begin(arrayVAEncROI);
        memset(roi_Param->roi, 0, task.m_numRoi*sizeof(VAEncROI));

        for (mfxU32 i = 0; i < task.m_numRoi; i ++)
        {
            roi_Param->roi[i].roi_rectangle.x = task.m_roi[i].Left;
            roi_Param->roi[i].roi_rectangle.y = task.m_roi[i].Top;
            roi_Param->roi[i].roi_rectangle.width = task.m_roi[i].Right - task.m_roi[i].Left;
            roi_Param->roi[i].roi_rectangle.height = task.m_roi[i].Top - task.m_roi[i].Bottom;
            roi_Param->roi[i].roi_value = task.m_roi[i].Priority;
        }
        roi_Param->max_delta_qp = 51;
        roi_Param->min_delta_qp = -51;
    }
    vaUnmapBuffer(vaDisplay, roiParam_id);

    return MFX_ERR_NONE;
}

void FillConstPartOfPps(
    MfxVideoParam const & par,
    VAEncPictureParameterBufferH264 & pps)
{
    mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
    mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    assert( extPps != 0 );
    assert( extSps != 0 );

    if (!extPps || !extSps)
        return;

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
    VAEncPictureParameterBufferH264 & pps,
    std::vector<ExtVASurface> const & reconQueue)
{
    pps.frame_num = task.m_frameNum;

    pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
    pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);
    if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
        pps.CurrPic.flags = TFIELD == fieldId ? VA_PICTURE_H264_TOP_FIELD : VA_PICTURE_H264_BOTTOM_FIELD;
    else
        pps.CurrPic.flags = 0;

    mfxU32 i = 0;
    mfxU32 idx = 0;
    mfxU32 ref = 0;
#if 1
    for( i = 0; i < task.m_dpb[fieldId].Size(); i++ )
    {
        pps.ReferenceFrames[i].frame_idx = idx  = task.m_dpb[fieldId][i].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = task.m_dpb[fieldId][i].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[1];
    }
#else
    // [sefremov] Below RefPicList formation algo (based on active reference lists) shouldn't be used. For every frame MSDK should pass to driver whole DPB, not just active refs.
    // [sefremov] Driver immidiately releases DPB frame if it wasn't passed with RefPicList.
    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];
    assert(task.m_list0[fieldId].Size() + task.m_list1[fieldId].Size() <= 16);

    for (ref = 0; ref < list0.Size() && i < 16; i++, ref++)
    {
        pps.ReferenceFrames[i].frame_idx = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = dpb[list0[ref] & 0x7f].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = dpb[list0[ref] & 0x7f].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = dpb[list0[ref] & 0x7f].m_poc[1];
    }

    for (ref = 0; ref < list1.Size() && i < 16; i++, ref++)
    {
        pps.ReferenceFrames[i].frame_idx = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = dpb[list1[ref] & 0x7f].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = dpb[list1[ref] & 0x7f].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = dpb[list1[ref] & 0x7f].m_poc[1];
    }
#endif
    for (; i < 16; i++)
    {
        pps.ReferenceFrames[i].picture_id = VA_INVALID_ID;
        pps.ReferenceFrames[i].frame_idx = 0xff;
        pps.ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
        pps.ReferenceFrames[i].TopFieldOrderCnt   = 0;
        pps.ReferenceFrames[i].BottomFieldOrderCnt   = 0;
    }
} // void UpdatePPS(...)

void UpdateSlice(
    ENCODE_CAPS const &                         hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par,
    std::vector<ExtVASurface> const & reconQueue)
{
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    if (task.m_numSlice[fieldId])
        slice.resize(task.m_numSlice[fieldId]);
    mfxU32 numSlice = slice.size();
    mfxU32 idx = 0, ref = 0;

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(par);
    assert( extDdi != 0 );

    SliceDivider divider = MakeSliceDivider(
        hwCaps.SliceStructure,
        task.m_numMbPerSlice,
        numSlice,
        sps.picture_width_in_mbs,
        sps.picture_height_in_mbs / numPics);

    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];

    for( size_t i = 0; i < slice.size(); ++i, divider.Next() )
    {
        slice[i].macroblock_address = divider.GetFirstMbInSlice();
        slice[i].num_macroblocks = divider.GetNumMbInSlice();
        slice[i].macroblock_info = VA_INVALID_ID;

        for (ref = 0; ref < list0.Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        for (ref = 0; ref < list1.Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList1[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = mfxU8(IPP_MAX(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1 = mfxU8(IPP_MAX(1, task.m_list1[fieldId].Size()) - 1);
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

        slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

        slice[i].disable_deblocking_filter_idc = 0;
        slice[i].slice_alpha_c0_offset_div2 = 0;
        slice[i].slice_beta_offset_div2 = 0;
    }

} // void UpdateSlice(...)

void UpdateSliceSizeLimited(
    ENCODE_CAPS const &                         hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par,
    std::vector<ExtVASurface> const & reconQueue)
{
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    mfxU32 idx = 0, ref = 0;

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(par);
    assert( extDdi != 0 );

    size_t numSlices = task.m_SliceInfo.size();
    if (numSlices != slice.size())
    {
        size_t old_size = slice.size();
        slice.resize(numSlices);
        for (size_t i = old_size; i < slice.size(); ++i)
        {
            slice[i] = slice[0];
            //slice[i].slice_id = (mfxU32)i;
        }
    }

    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];

    for( size_t i = 0; i < slice.size(); ++i)
    {
        slice[i].macroblock_address = task.m_SliceInfo[i].startMB;
        slice[i].num_macroblocks = task.m_SliceInfo[i].numMB;
        slice[i].macroblock_info = VA_INVALID_ID;

        for (ref = 0; ref < list0.Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        for (ref = 0; ref < list1.Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList1[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = mfxU8(IPP_MAX(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1 = mfxU8(IPP_MAX(1, task.m_list1[fieldId].Size()) - 1);
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

        slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
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
, m_vaContextEncode(VA_INVALID_ID)
, m_vaConfig(VA_INVALID_ID)
, m_spsBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_rateParamBufferId(VA_INVALID_ID)
, m_frameRateId(VA_INVALID_ID)
, m_maxFrameSizeId(VA_INVALID_ID)
, m_quantizationId(VA_INVALID_ID)
, m_rirId(VA_INVALID_ID)
, m_privateParamsId(VA_INVALID_ID)
, m_miscParameterSkipBufferId(VA_INVALID_ID)
, m_roiBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_mbqpBufferId(VA_INVALID_ID)
, m_mbNoSkipBufferId(VA_INVALID_ID)
, m_packedAudHeaderBufferId(VA_INVALID_ID)
, m_packedAudBufferId(VA_INVALID_ID)
, m_packedSpsHeaderBufferId(VA_INVALID_ID)
, m_packedSpsBufferId(VA_INVALID_ID)
, m_packedPpsHeaderBufferId(VA_INVALID_ID)
, m_packedPpsBufferId(VA_INVALID_ID)
, m_packedSeiHeaderBufferId(VA_INVALID_ID)
, m_packedSeiBufferId(VA_INVALID_ID)
, m_packedSkippedSliceHeaderBufferId(VA_INVALID_ID)
, m_packedSkippedSliceBufferId(VA_INVALID_ID)
, m_userMaxFrameSize(0)
, m_mbbrc(0)
, m_curTrellisQuantization(0)
, m_newTrellisQuantization(0)
, m_numSkipFrames(0)
, m_sizeSkipFrames(0)
, m_skipMode(0)
, m_isENCPAK(false)
{
    m_videoParam.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH; // QueryHwCaps will use this value
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()


void VAAPIEncoder::FillSps(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferH264 & sps)
{
        mfxExtSpsHeader const * extSps = GetExtBuffer(par);
        assert( extSps != 0 );
        if (!extSps)
            return;

        sps.picture_width_in_mbs  = par.mfx.FrameInfo.Width >> 4;
        sps.picture_height_in_mbs = par.mfx.FrameInfo.Height >> 4;

        sps.level_idc   = par.mfx.CodecLevel;

        sps.intra_period = par.mfx.GopPicSize;
        sps.ip_period    = par.mfx.GopRefDist;

        sps.bits_per_second     = par.calcParam.targetKbps * 1000;

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

//#if VA_CHECK_VERSION(0, 34, 0)
//after backport of va to OTC staging 0.33 also has the same fields. TODO: fully remove after validation
        sps.sar_height        = extSps->vui.sarHeight;
        sps.sar_width         = extSps->vui.sarWidth;
        sps.aspect_ratio_idc  = extSps->vui.aspectRatioIdc;
//#endif
/*
 *  In Windows DDI Trellis Quantization in SPS, while for VA in miscEnc.
 *  keep is here to have processed in Execute
 */
        mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(par);
        assert( extOpt2 != 0 );
        m_newTrellisQuantization = extOpt2 ? extOpt2->Trellis : 0;

} // void FillSps(...)

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE* core,
    GUID /*guid*/,
    mfxU32 width,
    mfxU32 height,
    bool /*isTemporal*/)
{
    m_core = core;

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);
    if(hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    m_width  = width;
    m_height = height;

    memset(&m_caps, 0, sizeof(m_caps));

    m_caps.BRCReset = 1; // no bitrate resolution control
    m_caps.HeaderInsertion = 0; // we will privide headers (SPS, PPS) in binary format to the driver

    //VAConfigAttrib attrs[5];
    std::vector<VAConfigAttrib> attrs;

    attrs.push_back( {VAConfigAttribRTFormat, 0});
    attrs.push_back( {VAConfigAttribRateControl, 0});
    attrs.push_back( {VAConfigAttribEncQuantization, 0});
    attrs.push_back( {VAConfigAttribEncIntraRefresh, 0});
    attrs.push_back( {VAConfigAttribMaxPictureHeight, 0});
    attrs.push_back( {VAConfigAttribMaxPictureWidth, 0});
    attrs.push_back( {VAConfigAttribEncInterlaced, 0});
    attrs.push_back( {VAConfigAttribEncMaxRefFrames, 0});
    attrs.push_back( {VAConfigAttribEncMaxSlices, 0});
#ifdef SKIP_FRAME_SUPPORT
    attrs.push_back( {VAConfigAttribEncSkipFrame, 0});
#endif

    VAStatus vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile),
                          VAEntrypointEncSlice,
                          Begin(attrs), attrs.size());
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_caps.VCMBitrateControl = attrs[1].value & VA_RC_VCM ? 1 : 0; //Video conference mode
    m_caps.TrelisQuantization  = (attrs[2].value & (~VA_ATTRIB_NOT_SUPPORTED));
    m_caps.vaTrellisQuantization = attrs[2].value;
    m_caps.RollingIntraRefresh = (attrs[3].value & (~VA_ATTRIB_NOT_SUPPORTED)) ? 1 : 0 ;
    m_caps.vaRollingIntraRefresh = attrs[3].value;
#ifdef SKIP_FRAME_SUPPORT
    m_caps.SkipFrame = (attrs[9].value & (~VA_ATTRIB_NOT_SUPPORTED)) ? 1 : 0 ;
#endif

    m_caps.UserMaxFrameSizeSupport = 1; // no request on support for libVA
    m_caps.MBBRCSupport = 1;            // starting 16.3 Beta, enabled in driver by default for TU-1,2
    m_caps.MbQpDataSupport = 1;

    vaExtQueryEncCapabilities pfnVaExtQueryCaps = NULL;
    pfnVaExtQueryCaps = (vaExtQueryEncCapabilities)vaGetLibFunc(m_vaDisplay,VPG_EXT_QUERY_ENC_CAPS);
    /* This is for 16.3.* approach.
     * It was used private libVA function to get information which feature is supported
     * */
    if (pfnVaExtQueryCaps)
    {
        VAEncQueryCapabilities VaEncCaps;
        memset(&VaEncCaps, 0, sizeof(VaEncCaps));
        VaEncCaps.size = sizeof(VAEncQueryCapabilities);
        vaSts = pfnVaExtQueryCaps(m_vaDisplay, VAProfileH264Baseline, &VaEncCaps);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        m_caps.MaxPicWidth  = VaEncCaps.MaxPicWidth;
        m_caps.MaxPicHeight = VaEncCaps.MaxPicHeight;
        m_caps.SliceStructure = VaEncCaps.EncLimits.bits.SliceStructure;
        m_caps.NoInterlacedField = VaEncCaps.EncLimits.bits.NoInterlacedField;
        m_caps.MaxNum_Reference = VaEncCaps.MaxNum_ReferenceL0;
        m_caps.MaxNum_Reference1 = VaEncCaps.MaxNum_ReferenceL1;
    }
    else /* this is LibVA legacy approach. Should be supported from 16.4 driver */
    {
#ifdef MFX_VA_ANDROID
        // To replace by vaQueryConfigAttributes()
        // when the driver starts to support VAConfigAttribMaxPictureWidth/Height
        m_caps.MaxPicWidth  = 1920;
        m_caps.MaxPicHeight = 1200;
#else
        if ((attrs[5].value != VA_ATTRIB_NOT_SUPPORTED) && (attrs[5].value != 0))
            m_caps.MaxPicWidth  = attrs[5].value;
        else
            m_caps.MaxPicWidth = 1920;

        if ((attrs[4].value != VA_ATTRIB_NOT_SUPPORTED) && (attrs[4].value != 0))
            m_caps.MaxPicHeight = attrs[4].value;
        else
            m_caps.MaxPicHeight = 1088;
#endif
        if (attrs[8].value != VA_ATTRIB_NOT_SUPPORTED)
            m_caps.SliceStructure = attrs[8].value;
        else
            m_caps.SliceStructure = m_core->GetHWType() == MFX_HW_HSW ? 2 : 1; // 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice

        if (attrs[6].value != VA_ATTRIB_NOT_SUPPORTED)
            m_caps.NoInterlacedField = attrs[6].value;
        else
            m_caps.NoInterlacedField = 0;

        if (attrs[7].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            m_caps.MaxNum_Reference = attrs[7].value & 0xffff;
            m_caps.MaxNum_Reference1 = (attrs[7].value >>16) & 0xffff;
        }
        else
        {
            m_caps.MaxNum_Reference = 1;
            m_caps.MaxNum_Reference1 = 1;
        }
    } /* if (pfnVaExtQueryCaps) */

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    if(IsMvcProfile(par.mfx.CodecProfile))
        return MFX_WRN_PARTIAL_ACCELERATION;

    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces wheh call vaCreateContext().
     * Here we don't have this info.
     */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                Begin(pEntrypoints),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    //first check for ENCPAK support
    //check for ext buffer for FEI
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                isFEI  = params->Func == MFX_FEI_FUNCTION_ENCPAK;
                break;
            }
        }
        if (isFEI){
            for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
            {
                if( VAEntrypointEncFEIIntel == pEntrypoints[entrypointsIndx] )
                {
                        m_isENCPAK = true;
                        break;
                }
            }
            if(isFEI && !m_isENCPAK)
                return MFX_ERR_UNSUPPORTED;
        }
    }
#endif
    VAEntrypoint entryPoint = VAEntrypointEncSlice;
    if( ! m_isENCPAK )
    {
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
    }
    else
    {
        entryPoint = (VAEntrypoint) VAEntrypointEncFEIIntel;
    }

    // Configuration
    VAConfigAttrib attrib[3];
    mfxI32 numAttrib = 0;
    mfxU32 flag = VA_PROGRESSIVE;

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    numAttrib += 2;

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    if ( m_isENCPAK )
    {
            attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
            numAttrib++;
    }
#endif
    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                          entryPoint,
                          &attrib[0], numAttrib);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    if( NULL == extOpt2 )
    {
        assert( extOpt2 );
        return (MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    if(m_isENCPAK){
        //check function
        if(!(attrib[2].value & VA_ENC_FUNCTION_ENC_PAK_INTEL)){
                return MFX_ERR_DEVICE_FAILED;
        }else{
            attrib[2].value = VA_ENC_FUNCTION_ENC_PAK_INTEL;
        }
    }

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

    if (m_videoParam.Protected && IsSupported__VAHDCPEncryptionParameterBuffer())
        flag |= VA_HDCP_ENABLED;

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        flag,
        Begin(reconSurf),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU16 maxNumSlices = GetMaxNumSlices(par);

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
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId), MFX_ERR_DEVICE_FAILED);

    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    mfxExtCodingOption3 const *extOpt3 = GetExtBuffer(par);

    if (extOpt3)
    {
        if (IsOn(extOpt3->EnableMBQP))
            m_mbqp_buffer.resize(((m_width / 16 + 63) & ~63) * ((m_height / 16 + 7) & ~7));

        if (IsOn(extOpt3->MBDisableSkipMap))
            m_mb_noskip_buffer.resize(((m_width / 16 + 63) & ~63) * ((m_height / 16 + 7) & ~7));
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)
{
//    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Reset");

    m_videoParam = par;
    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    if( NULL == extOpt2 )
    {
        assert( extOpt2 );
        return (MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    FillSps(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    bool isBrcResetRequired = !Equal(m_vaBrcPar, oldBrcPar) || !Equal(m_vaFrameRate, oldFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId, isBrcResetRequired), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId), MFX_ERR_DEVICE_FAILED);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

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

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    VAConfigID config = VA_INVALID_ID;

    VAConfigAttrib attrib[2];
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[1].value = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    VAStatus vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        VAEntrypointEncSlice,
        attrib,
        2,
        &config);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VAProcessingRateParams proc_rate_buf = { };
    mfxU32 & processing_rate = mbPerSec[0];

    proc_rate_buf.proc_buf_enc.level_idc = par.mfx.CodecLevel ? par.mfx.CodecLevel : 0xff;
    proc_rate_buf.proc_buf_enc.quality_level = par.mfx.TargetUsage ? par.mfx.TargetUsage : 0xffff;
    proc_rate_buf.proc_buf_enc.intra_period = par.mfx.GopPicSize ? par.mfx.GopPicSize : 0xffff;
    proc_rate_buf.proc_buf_enc.ip_period = par.mfx.GopRefDist ? par.mfx.GopRefDist : 0xffff;

    vaSts = vaQueryProcessingRate(m_vaDisplay, config, &proc_rate_buf, &processing_rate);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaDestroyConfig(m_vaDisplay, config);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryInputTilingSupport(mfxVideoParam const & par, mfxU32 &inputTiling)
{
    VAConfigID config = VA_INVALID_ID;

    VAConfigAttrib attrib[2];
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[1].value = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    VAStatus vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        VAEntrypointEncSlice,
        attrib,
        2,
        &config);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VAProfile    profile;
    VAEntrypoint entrypoint;
    mfxI32       numAttribs;
    mfxU32       maxNumAttribs = vaMaxNumConfigAttributes(m_vaDisplay);

    std::vector<VAConfigAttrib> attrs;
    attrs.resize(maxNumAttribs);

    vaSts = vaQueryConfigAttributes(m_vaDisplay, config, &profile, &entrypoint, Begin(attrs), &numAttribs);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT((mfxU32)numAttribs < maxNumAttribs, MFX_ERR_UNDEFINED_BEHAVIOR);

    if (entrypoint == VAEntrypointEncSlice)
    {
        for(mfxI32 i=0; i<numAttribs; i++)
        {
            if (attrs[i].type == VAConfigAttribInputTiling)
                inputTiling = attrs[i].value;
        }
    }

    vaDestroyConfig(m_vaDisplay, config);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryHWGUID(VideoCORE * core, GUID guid, bool isTemporal)
{
    core;
    guid;
    isTemporal;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

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
    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type )
    {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
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
//static int debug_frame_bum = 0;

mfxStatus VAAPIEncoder::Execute(
//mfxStatus VAAPIFEIENCEncoder::Execute(
    mfxHDL          surface,
    DdiTask const & task,
    mfxU32          fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");

    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VASurfaceID reconSurface;
    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VABufferID  codedBuffer;
    std::vector<VABufferID> configBuffers;
    std::vector<mfxU32> packedBufferIndexes;
    mfxU32      i;
    mfxU16      buffersCount = 0;
    mfxU32      packedDataSize = 0;
    VAStatus    vaSts;
    mfxU8 skipFlag  = task.SkipFlag();
    mfxU16 skipMode = m_skipMode;
    mfxExtCodingOption2     const * ctrlOpt2      = GetExtBuffer(task.m_ctrl);
    mfxExtMBDisableSkipMap  const * ctrlNoSkipMap = GetExtBuffer(task.m_ctrl);

    if (ctrlOpt2 && ctrlOpt2->SkipFrame <= MFX_SKIPFRAME_BRC_ONLY)
        skipMode = ctrlOpt2->SkipFrame;

    if (skipMode == MFX_SKIPFRAME_BRC_ONLY)
    {
        skipFlag = 0; // encode current frame as normal
        m_numSkipFrames += (mfxU8)task.m_ctrl.SkipFrame;
    }

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

        UpdatePPS(task, fieldId, m_pps, m_reconQueue);

        if (task.m_SliceInfo.size())
            UpdateSliceSizeLimited(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);
        else
            UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

        if (slice_size_old != m_slice.size())
        {
            m_sliceBufferId.resize(m_slice.size());
            m_packeSliceHeaderBufferId.resize(m_slice.size());
            m_packedSliceBufferId.resize(m_slice.size());
        }

        configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    }
    /* for debug only */
    //fprintf(stderr, "----> Encoding frame = %u, type = %u\n", debug_frame_bum++, ConvertMfxFrameType2SliceType( task.m_type[fieldId]) -5 );

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
    unsigned int idxRecon = task.m_idxRecon;
    if( idxRecon < m_reconQueue.size())
    {
        reconSurface = m_reconQueue[ idxRecon ].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    m_pps.coded_buf = codedBuffer;
    m_pps.CurrPic.picture_id = reconSurface;

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    //FEI buffers pack
    //VAEncFEIMVBufferTypeIntel                 = 1001,
    //VAEncFEIModeBufferTypeIntel,
    //VAEncFEIDistortionBufferTypeIntel,
    //VAEncFEIMBControlBufferTypeIntel,
    //VAEncFEIMVPredictorBufferTypeIntel,

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    VABufferID vaFeiMCODEOutId = VA_INVALID_ID;

    VABufferID outBuffers[3];
    int numOutBufs = 0;
    outBuffers[0] = VA_INVALID_ID;
    outBuffers[1] = VA_INVALID_ID;
    outBuffers[2] = VA_INVALID_ID;

    if (m_isENCPAK)
    {
        int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

        //find ext buffers
        const mfxEncodeCtrl& ctrl = task.m_ctrl;
        mfxExtFeiEncMBCtrl* mbctrl = NULL;
        mfxExtFeiEncMVPredictors* mvpred = NULL;
        mfxExtFeiEncFrameCtrl* frameCtrl = NULL;
        mfxExtFeiEncQP* mbqp = NULL;
        mfxExtFeiEncMBStat* mbstat = &((mfxExtFeiEncMBStat*)(task.m_feiDistortion))[fieldId];
        mfxExtFeiEncMV* mvout = &((mfxExtFeiEncMV*)(task.m_feiMVOut))[fieldId];
        mfxExtFeiPakMBCtrl* mbcodeout = &((mfxExtFeiPakMBCtrl*)(task.m_feiMBCODEOut))[fieldId];

        for (int i = 0; i < ctrl.NumExtParam; i++)
        {
            if (ctrl.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL)
            {
                frameCtrl = &((mfxExtFeiEncFrameCtrl*) (ctrl.ExtParam[i]))[fieldId];
            }
            if (ctrl.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED)
            {
                mvpred = &((mfxExtFeiEncMVPredictors*) (ctrl.ExtParam[i]))[fieldId];
            }
            if (ctrl.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB)
            {
                mbctrl = &((mfxExtFeiEncMBCtrl*) (ctrl.ExtParam[i]))[fieldId];
            }
            if (ctrl.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
            {
                mbqp = &((mfxExtFeiEncQP*) (ctrl.ExtParam[i]))[fieldId];
            }
        }

        if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
        {
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
                    sizeof(VAEncMVPredictorH264Intel)*numMB,  //limitation from driver, num elements should be 1
                    1,
                    mvpred->MB,
                    &vaFeiMVPredId );
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = vaFeiMVPredId;
        }

        if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
        {
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    (VABufferType)VAEncFEIMBControlBufferTypeIntel,
                    sizeof (VAEncFEIMBControlH264Intel)*numMB,  //limitation from driver, num elements should be 1
                    1,
                    mbctrl->MB,
                    &vaFeiMBControlId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = vaFeiMBControlId;
        }

        if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
        {
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    (VABufferType)VAEncQpBufferType,
                    sizeof (VAEncQpBufferH264)*numMB, //limitation from driver, num elements should be 1
                    1,
                    mbqp->QP,
                    &vaFeiMBQPId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = vaFeiMBQPId;
        }

        //output buffer for MB distortions
        if (mbstat != NULL)
        {
            if (m_vaFeiMBStatId.size() == 0 )
            {
                m_vaFeiMBStatId.resize(16);
                for( mfxU32 ii = 0; ii < 16; ii++ )
                {
                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                            sizeof (VAEncFEIDistortionBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                            1,
                            NULL, //should be mapped later
                            //&vaFeiMBStatId);
                            &m_vaFeiMBStatId[ii]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }
            } // if (m_vaFeiMBStatId.size() == 0 )
            //outBuffers[numOutBufs++] = vaFeiMBStatId;
            outBuffers[numOutBufs++] = m_vaFeiMBStatId[idxRecon];
            //mdprintf(stderr, "MB Stat bufId=%d\n", vaFeiMBStatId);
            //configBuffers[buffersCount++] = vaFeiMBStatId;
            configBuffers[buffersCount++] = m_vaFeiMBStatId[idxRecon];
        }

        //output buffer for MV
        if (mvout != NULL)
        {
            if (m_vaFeiMVOutId.size() == 0 )
            {
                m_vaFeiMVOutId.resize(16);
                for( mfxU32 ii = 0; ii < 16; ii++ )
                {
                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            (VABufferType)VAEncFEIMVBufferTypeIntel,
                            sizeof (VAMotionVectorIntel)*16*numMB, //limitation from driver, num elements should be 1
                            1,
                            NULL, //should be mapped later
                            &m_vaFeiMVOutId[ii]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }
            } // if (m_vaFeiMVOutId.size() == 0 )
            //outBuffers[numOutBufs++] = vaFeiMVOutId;
            outBuffers[numOutBufs++] = m_vaFeiMVOutId[idxRecon];
            //mdprintf(stderr, "MV Out bufId=%d\n", vaFeiMVOutId);
            //configBuffers[buffersCount++] = vaFeiMVOutId;
            configBuffers[buffersCount++] = m_vaFeiMVOutId[idxRecon];
        }

        //output buffer for MBCODE (Pak object cmds)
        if (mbcodeout != NULL)
        {
            if (m_vaFeiMCODEOutId.size() == 0 )
            {
                m_vaFeiMCODEOutId.resize(16);
                for( mfxU32 ii = 0; ii < 16; ii++ )
                {
                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            (VABufferType)VAEncFEIModeBufferTypeIntel,
                            sizeof (VAEncFEIModeBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                            1,
                            NULL, //should be mapped later
                            &m_vaFeiMCODEOutId[ii]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }
            } //if (m_vaFeiMCODEOutId.size() == 0 )
            //outBuffers[numOutBufs++] = vaFeiMCODEOutId;
            outBuffers[numOutBufs++] = m_vaFeiMCODEOutId[idxRecon];
            //mdprintf(stderr, "MCODE Out bufId=%d\n", vaFeiMCODEOutId);
            //configBuffers[buffersCount++] = vaFeiMCODEOutId;
            configBuffers[buffersCount++] = m_vaFeiMCODEOutId[idxRecon];
        }

        if (frameCtrl != NULL)
        {
            VAEncMiscParameterBuffer *miscParam;
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                    1,
                    NULL,
                    &vaFeiFrameControlId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaMapBuffer(m_vaDisplay,
                vaFeiFrameControlId,
                (void **)&miscParam);

            miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
            VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
            memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

            vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_PAK_INTEL;
            vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
            vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
            vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
            vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
            vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
            vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
            vaFeiFrameControl->len_sp = frameCtrl->LenSP;
            vaFeiFrameControl->max_len_sp = frameCtrl->MaxLenSP;

            vaFeiFrameControl->distortion = m_vaFeiMBStatId[idxRecon];
            vaFeiFrameControl->mv_data = m_vaFeiMVOutId[idxRecon];
            vaFeiFrameControl->mb_code_data = m_vaFeiMCODEOutId[idxRecon];
            vaFeiFrameControl->qp = vaFeiMBQPId;
            vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
            vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
            vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
            vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
            vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
            vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
            vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
            vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
            vaFeiFrameControl->num_mv_predictors = frameCtrl->NumMVPredictors;
            vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
            vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
            vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
            vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
            vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
            vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;
            //MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);

            vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

            configBuffers[buffersCount++] = vaFeiFrameControlId;
        }
    }
#endif
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
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
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

    if (m_caps.HeaderInsertion == 1 && skipFlag == NO_SKIP)
    {
        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

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
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }
    }
    else
    {
        // AUD
        if (task.m_insertAud[fieldId])
        {
            ENCODE_PACKEDHEADER_DATA const & packedAud = m_headerPacker.PackAud(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedAud.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedAud.DataLength*8;

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
                                packedAud.DataLength, 1, packedAud.pData,
                                &m_packedAudBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedAudHeaderBufferId;
            configBuffers[buffersCount++] = m_packedAudBufferId;
        }
        // SPS
        if (task.m_insertSps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
            ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderSequence;
            packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSps.DataLength*8;

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
                                packedSps.DataLength, 1, packedSps.pData,
                                &m_packedSpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSpsBufferId;
        }

        if (task.m_insertPps[fieldId])
        {
            // PPS
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
            ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderPicture;
            packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedPps.DataLength*8;

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
                                packedPps.DataLength, 1, packedPps.pData,
                                &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedPpsBufferId;
        }

        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

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
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }

        if (skipFlag != NO_SKIP)
        {
            // The whole slice
            ENCODE_PACKEDHEADER_DATA const & packedSkippedSlice = m_headerPacker.PackSkippedSlice(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedSkippedSlice.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSkippedSlice.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSkippedSliceHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSkippedSliceHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSkippedSliceBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                packedSkippedSlice.DataLength, 1, packedSkippedSlice.pData,
                &m_packedSkippedSliceBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSkippedSliceHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSkippedSliceBufferId;

            if (PAVP_MODE == skipFlag)
            {
                m_numSkipFrames = 1;
                m_sizeSkipFrames = (skipMode != MFX_SKIPFRAME_INSERT_NOTHING) ? packedDataSize : 0;
            }
        }
        else
        {
            if ((m_core->GetHWType() >= MFX_HW_HSW) && (m_core->GetHWType() != MFX_HW_VLV))
            {
                //Slice headers only
                std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
                for (size_t i = 0; i < packedSlices.size(); i++)
                {
                    ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

                    packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
                    packed_header_param_buffer.has_emulation_bytes = 0;
                    packed_header_param_buffer.bit_length = packedSlice.DataLength; // DataLength is already in bits !

                    //MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderParameterBufferType,
                            sizeof(packed_header_param_buffer),
                            1,
                            &packed_header_param_buffer,
                            &m_packeSliceHeaderBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    //MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
                    vaSts = vaCreateBuffer(m_vaDisplay,
                                        m_vaContextEncode,
                                        VAEncPackedHeaderDataBufferType,
                                        (packedSlice.DataLength + 7) / 8, 1, packedSlice.pData,
                                        &m_packedSliceBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
                    configBuffers[buffersCount++] = m_packedSliceBufferId[i];
                }
            }
        }
    }

    configBuffers[buffersCount++] = m_hrdBufferId;
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(m_videoParam, m_mbbrc, task.m_minQP, task.m_maxQP,
                                                         m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_rateParamBufferId;
    configBuffers[buffersCount++] = m_frameRateId;

#ifdef MAX_FRAME_SIZE_SUPPORT
/*
 * Limit frame size by application/user level
 */
    if (m_userMaxFrameSize != task.m_maxFrameSize)
    {
        m_userMaxFrameSize = (UINT)task.m_maxFrameSize;
//        if (task.m_frameOrder)
//            m_sps.bResetBRC = true;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetMaxFrameSize(m_userMaxFrameSize, m_vaDisplay,
                                                              m_vaContextEncode, m_maxFrameSizeId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_maxFrameSizeId;
    }
#endif

#ifdef TRELLIS_QUANTIZATION_SUPPORT
/*
 *  By default (0) - driver will decide.
 *  1 - disable trellis quantization
 *  x0E - enable for any type of frames
 */
    if (m_newTrellisQuantization != m_curTrellisQuantization)
    {
        m_curTrellisQuantization = m_newTrellisQuantization;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetTrellisQuantization(m_curTrellisQuantization, m_vaDisplay,
                                                                     m_vaContextEncode, m_quantizationId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_quantizationId;
    }
#endif

#ifdef ROLLING_INTRA_REFRESH_SUPPORT
 /*
 *   RollingIntraRefresh
 */
    if (memcmp(&task.m_IRState, &m_RIRState, sizeof(m_RIRState)))
    {
        m_RIRState = task.m_IRState;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRollingIntraRefresh(m_RIRState, m_vaDisplay,
                                                                     m_vaContextEncode, m_rirId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_rirId;
    }
#endif

    if (task.m_numRoi)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetROI(task, m_arrayVAEncROI, m_vaDisplay, m_vaContextEncode, m_roiBufferId),
                              MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_roiBufferId;
    }

    if (task.m_isMBQP)
    {
        const mfxExtMBQP *mbqp = GetExtBuffer(task.m_ctrl);
        mfxU32 mbW = m_sps.picture_width_in_mbs;
        mfxU32 mbH = m_sps.picture_height_in_mbs;
        //width(64byte alignment) height(8byte alignment)
        mfxU32 bufW = ((mbW + 63) & ~63);
        mfxU32 bufH = ((mbH + 7) & ~7);

        if (   mbqp && mbqp->QP && mbqp->NumQPAlloc >= mbW * mbH
            && m_mbqp_buffer.size() >= (bufW * bufH))
        {

            Zero(m_mbqp_buffer);
            for (mfxU32 mbRow = 0; mbRow < mbH; mbRow ++)
                for (mfxU32 mbCol = 0; mbCol < mbW; mbCol ++)
                    m_mbqp_buffer[mbRow * bufW + mbCol].qp_y = mbqp->QP[mbRow * mbW + mbCol];

            MFX_DESTROY_VABUFFER(m_mbqpBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof(VAEncQpBufferH264),
                (bufW * bufH),
                &m_mbqp_buffer[0],
                &m_mbqpBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_mbqpBufferId;
        }
    }

    if (ctrlNoSkipMap)
    {
        mfxU32 mbW = m_sps.picture_width_in_mbs;
        mfxU32 mbH = m_sps.picture_height_in_mbs;
        mfxU32 bufW = ((mbW + 63) & ~63);
        mfxU32 bufH = ((mbH + 7) & ~7);

        if (   m_mb_noskip_buffer.size() >= (bufW * bufH)
            && ctrlNoSkipMap->Map
            && ctrlNoSkipMap->MapSize >= (mbW * mbH))
        {
            Zero(m_mb_noskip_buffer);
            for (mfxU32 mbRow = 0; mbRow < mbH; mbRow ++)
                MFX_INTERNAL_CPY(&m_mb_noskip_buffer[mbRow * bufW], &ctrlNoSkipMap->Map[mbRow * mbW], mbW);

            MFX_DESTROY_VABUFFER(m_mbNoSkipBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    (VABufferType)VAEncMacroblockDisableSkipMapBufferType,
                    (bufW * bufH),
                    1,
                    &m_mb_noskip_buffer[0],
                    &m_mbNoSkipBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_mbNoSkipBufferId;
        }
    }

    if (ctrlOpt2)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetPrivateParams(m_videoParam, m_vaDisplay,
                                                               m_vaContextEncode, m_privateParamsId, &task.m_ctrl), MFX_ERR_DEVICE_FAILED);
    }
    if (VA_INVALID_ID != m_privateParamsId) configBuffers[buffersCount++] = m_privateParamsId;

    assert(buffersCount <= configBuffers.size());

    mfxU32 storedSize = 0;

    if (skipFlag != NORMAL_MODE)
    {
#ifdef SKIP_FRAME_SUPPORT
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetSkipFrame(m_vaDisplay, m_vaContextEncode, m_miscParameterSkipBufferId,
                                                           skipFlag ? skipFlag : !!m_numSkipFrames,
                                                           m_numSkipFrames, m_sizeSkipFrames), MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_miscParameterSkipBufferId;

        m_numSkipFrames  = 0;
        m_sizeSkipFrames = 0;
#endif
        mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
        mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
        mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
        mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
        mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);
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
                Begin(configBuffers),
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
    }
    else
    {
        VACodedBufferSegment *codedBufferSegment;
        {
            codedBuffer = m_bsQueue[task.m_idxBs[fieldId]].surface;
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
            vaSts = vaMapBuffer(
                m_vaDisplay,
                codedBuffer,
                (void **)(&codedBufferSegment));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        codedBufferSegment->next = 0;
        codedBufferSegment->reserved = 0;
        codedBufferSegment->status = 0;

        MFX_CHECK_WITH_ASSERT(codedBufferSegment->buf, MFX_ERR_DEVICE_FAILED);

        mfxU8 *  bsDataStart = (mfxU8 *)codedBufferSegment->buf;
        mfxU8 *  bsDataEnd   = bsDataStart;

        if (skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            for (size_t i = 0; i < packedBufferIndexes.size(); i++)
            {
                size_t headerIndex = packedBufferIndexes[i];

                void *pBufferHeader;
                vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex], &pBufferHeader);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                void *pData;
                vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex + 1], &pData);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                if (pBufferHeader && pData)
                {
                    VAEncPackedHeaderParameterBuffer const & header = *(VAEncPackedHeaderParameterBuffer const *)pBufferHeader;

                    mfxU32 lenght = (header.bit_length+7)/8;

                    assert(mfxU32(bsDataStart + m_width*m_height - bsDataEnd) > lenght);
                    assert(header.has_emulation_bytes);

                    MFX_INTERNAL_CPY(bsDataEnd, pData, lenght);
                    bsDataEnd += lenght;
                }

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex + 1]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }

        storedSize = mfxU32(bsDataEnd - bsDataStart);
        codedBufferSegment->size = storedSize;

        m_numSkipFrames ++;
        m_sizeSkipFrames += (skipMode != MFX_SKIPFRAME_INSERT_NOTHING) ? storedSize : 0;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
            vaUnmapBuffer( m_vaDisplay, codedBuffer );
        }
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = (NORMAL_MODE == skipFlag) ? VA_INVALID_SURFACE : *inputSurface;
        currentFeedback.idxBs   = task.m_idxBs[fieldId];
        currentFeedback.size    = storedSize;
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
        if (m_isENCPAK)
        {
            currentFeedback.mv        = m_vaFeiMVOutId[idxRecon];
            currentFeedback.mbstat    = m_vaFeiMBStatId[idxRecon];
            currentFeedback.mbcode    = m_vaFeiMCODEOutId[idxRecon];
        }
#endif
        m_feedbackCache.push_back( currentFeedback );
    }

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
#endif
    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    mfxStatus sts = MFX_ERR_NONE;
    VAStatus vaSts;
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 waitSize;
    mfxU32 indxSurf;
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
#endif
    UMC::AutomaticUMCMutex guard(m_guard);

    for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
    {
        ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

        if( currentFeedback.number == task.m_statusReportNumber[fieldId] )
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            waitSize    = currentFeedback.size;
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
#endif
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

    if (waitSurface != VA_INVALID_SURFACE) // Not skipped frame
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

                task.m_bsDataLength[fieldId] = codedBufferSegment->size;

                if (m_videoParam.Protected && IsSupported__VAHDCPEncryptionParameterBuffer() && codedBufferSegment->next)
                {
                    VACodedBufferSegment *nextSegment = (VACodedBufferSegment*)codedBufferSegment->next;

                    bool bIsTearDown = ((VA_CODED_BUF_STATUS_BAD_BITSTREAM & codedBufferSegment->status) || (VA_CODED_BUF_STATUS_TEAR_DOWN & nextSegment->status));

                    if (bIsTearDown)
                    {
                        task.m_bsDataLength[fieldId] = 0;
                        sts = MFX_ERR_DEVICE_LOST;
                    }
                    else
                    {
                        VAHDCPEncryptionParameterBuffer *HDCPParam = NULL;
                        mfxAES128CipherCounter CipherCounter = {0, 0};
                        bool isProtected = false;

                        if (nextSegment->status & VA_CODED_BUF_STATUS_PRIVATE_DATA_HDCP &&
                            NULL != nextSegment->buf)
                        {
                            HDCPParam = (VAHDCPEncryptionParameterBuffer*)nextSegment->buf;
                            mfxU64* Count = (mfxU64*)&HDCPParam->counter[0];
                            mfxU64* IV = (mfxU64*)&HDCPParam->counter[2];
                            CipherCounter.Count = *Count;
                            CipherCounter.IV = *IV;

                            isProtected = HDCPParam->bEncrypted;
                        }

                        task.m_aesCounter[0] = CipherCounter;
                        task.m_notProtected = !isProtected;
                    }
                }

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                    vaUnmapBuffer( m_vaDisplay, codedBuffer );
                }

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
                //FEI buffers pack
                //VAEncFEIModeBufferTypeIntel,
                //VAEncFEIDistortionBufferTypeIntel,
                if (m_isENCPAK)
                {
                    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;
                    //find ext buffers
//                    mfxBitstream* bs = task.m_bs;
                    mfxExtFeiEncMBStat* mbstat = &((mfxExtFeiEncMBStat*)(task.m_feiDistortion))[fieldId];
                    mfxExtFeiEncMV* mvout = &((mfxExtFeiEncMV*)(task.m_feiMVOut))[fieldId];
                    mfxExtFeiPakMBCtrl* mbcodeout = &((mfxExtFeiPakMBCtrl*)(task.m_feiMBCODEOut))[fieldId];

                    if (mbstat != NULL && vaFeiMBStatId != VA_INVALID_ID)
                    {
                        VAEncFEIDistortionBufferH264Intel* mbs;
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                            vaSts = vaMapBuffer(
                                    m_vaDisplay,
                                    vaFeiMBStatId,
                                    (void **) (&mbs));
                        }
                        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                        //copy to output in task here MVs
                        memcpy_s(mbstat->MB, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB,
                                        mbs, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB);
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                            vaUnmapBuffer(m_vaDisplay, vaFeiMBStatId);
                        }

                        //MFX_DESTROY_VABUFFER(vaFeiMBStatId, m_vaDisplay);
                        vaFeiMBStatId = VA_INVALID_ID;
                    }

                    if (mvout != NULL && vaFeiMVOutId != VA_INVALID_ID)
                    {
                        VAMotionVectorIntel* mvs;
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                            vaSts = vaMapBuffer(
                                    m_vaDisplay,
                                    vaFeiMVOutId,
                                    (void **) (&mvs));
                        }

                        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                        //copy to output in task here MVs
                        memcpy_s(mvout->MB, sizeof (VAMotionVectorIntel) * 16 * numMB,
                                       mvs, sizeof (VAMotionVectorIntel) * 16 * numMB);
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                            vaUnmapBuffer(m_vaDisplay, vaFeiMVOutId);
                        }

                        //MFX_DESTROY_VABUFFER(vaFeiMVOutId, m_vaDisplay);
                        vaFeiMVOutId = VA_INVALID_ID;
                    }

                    if (mbcodeout != NULL && vaFeiMBCODEOutId != VA_INVALID_ID)
                    {
                        VAEncFEIModeBufferH264Intel* mbcs;
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                            vaSts = vaMapBuffer(
                                m_vaDisplay,
                                vaFeiMBCODEOutId,
                                (void **) (&mbcs));
                        }
                        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                        //copy to output in task here MVs
                        memcpy_s(mbcodeout->MB, sizeof (VAEncFEIModeBufferH264Intel) * numMB,
                                         mbcs, sizeof (VAEncFEIModeBufferH264Intel) * numMB);
                        {
                            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                            vaUnmapBuffer(m_vaDisplay, vaFeiMBCODEOutId);
                        }

                        //MFX_DESTROY_VABUFFER(vaFeiMBCODEOutId, m_vaDisplay);
                        vaFeiMBCODEOutId = VA_INVALID_ID;
                    }
                }
#endif
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
    else
    {
        task.m_bsDataLength[fieldId] = waitSize;
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
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
    MFX_DESTROY_VABUFFER(m_frameRateId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_maxFrameSizeId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_quantizationId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rirId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_privateParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_miscParameterSkipBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_roiBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_mbqpBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_mbNoSkipBufferId, m_vaDisplay);
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
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceBufferId, m_vaDisplay);

    for( mfxU32 i = 0; i < m_vaFeiMBStatId.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_vaFeiMBStatId[i], m_vaDisplay);
    }

    for( mfxU32 i = 0; i < m_vaFeiMVOutId.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_vaFeiMVOutId[i], m_vaDisplay);
    }

    for( mfxU32 i = 0; i < m_vaFeiMCODEOutId.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_vaFeiMCODEOutId[i], m_vaDisplay);
    }


    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Destroy()

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

VAAPIFEIPREENCEncoder::VAAPIFEIPREENCEncoder()
: VAAPIEncoder()
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPREENCEncoder::~VAAPIFEIPREENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPREENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPREENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_ENCPAK)
        return CreateENCPAKAccelerationService(par);
    else if (m_codingFunction == MFX_FEI_FUNCTION_PREENC)
        return CreatePREENCAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPREENCEncoder::CreatePREENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    bool bEncodeEnable = false;
#if !TMP_DISABLE
    //entry point for preENC
    vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            VAProfileNone, //specific for statistic
            Begin(pEntrypoints),
            &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    for (entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++) {
        if (VAEntrypointStatisticsIntel == pEntrypoints[entrypointsIndx]) {
            bEncodeEnable = true;
            break;
        }
    }

    if (!bEncodeEnable)
        return MFX_ERR_DEVICE_FAILED;

    //check attributes of the configuration
    VAConfigAttrib attrib[2];
    //attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].type = (VAConfigAttribType)VAConfigAttribStatisticsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0], 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    //if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
    //    return MFX_ERR_DEVICE_FAILED;

    //attrib[0].value = VA_RT_FORMAT_YUV420;

    //check statistic attributes
    VAConfigAttribValStatisticsIntel statVal;
    statVal.value = attrib[0].value;
    //temp to be removed
    // But lets leave for a while
    mdprintf(stderr,"Statistics attributes:\n");
    mdprintf(stderr,"max_past_refs: %d\n", statVal.bits.max_num_past_references);
    mdprintf(stderr,"max_future_refs: %d\n", statVal.bits.max_num_future_references);
    mdprintf(stderr,"num_outputs: %d\n", statVal.bits.num_outputs);
    mdprintf(stderr,"interlaced: %d\n\n", statVal.bits.interlaced);
    //attrib[0].value |= ((2 - 1/*MV*/ - 1/*mb*/) << 8);

    vaSts = vaCreateConfig(
            m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0],
            1,
            &m_vaConfig); //don't configure stat attribs
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

#endif

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
#if !TMP_DISABLE
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
#endif
    //FillConstPartOfPps(par, m_pps);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIFEIPREENCEncoder::CreateENCPAKAccelerationService(MfxVideoParam const & par)
{
    if (0 == m_reconQueue.size()) {
        /* We need to pass reconstructed surfaces when call vaCreateContext().
         * Here we don't have this info.
         */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    bool bEncodeEnable = false;
    //ENC/PAK entry point
    vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            Begin(pEntrypoints),
            &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++)
    {
        if (VAEntrypointEncFEIIntel == pEntrypoints[entrypointsIndx]) {
            bEncodeEnable = true;
            break;
        }
    }

    if (!bEncodeEnable)
        return MFX_ERR_DEVICE_FAILED;

    // Configuration, check for BRC?
    VAConfigAttrib attrib[2];

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            (VAEntrypoint)VAEntrypointEncFEIIntel,
            &attrib[0], 2);
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
            (VAEntrypoint)VAEntrypointEncFEIIntel,
            attrib,
            2,
            &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIFEIPREENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIPREENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    //VAStatsStatisticsParameterBufferTypeIntel,
    //VAStatsStatisticsBufferTypeIntel,
    //VAStatsMotionVectorBufferTypeIntel,
    //VAStatsMVPredictorBufferTypeIntel,

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc Execute");

    VAStatus vaSts;
    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    //Add preENC buffers
    //preENC control
    VABufferID statMVid = VA_INVALID_ID;
    VABufferID statOUTid = VA_INVALID_ID;
    VABufferID mvPredid = VA_INVALID_ID;
    VABufferID qpid = VA_INVALID_ID;
    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    //buffer for MV output
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    //find output surfaces
    mfxExtFeiPreEncMV* mvsOut = NULL;
    mfxExtFeiPreEncMBStat* mbstatOut = NULL;
    for (int i = 0; i < out->NumExtParam; i++) {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV)
        {
            mvsOut = &((mfxExtFeiPreEncMV*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MB)
        {
            mbstatOut = &((mfxExtFeiPreEncMBStat*) (out->ExtParam[i]))[fieldId];
        }
    }

    //find in buffers
    mfxExtFeiPreEncCtrl* feiCtrl = NULL;
    mfxExtFeiEncQP* feiQP = NULL;
    mfxExtFeiPreEncMVPredictors* feiMVPred = NULL;
    for (int i = 0; i < in->NumExtParam; i++) {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_CTRL)
        {
            feiCtrl = &((mfxExtFeiPreEncCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
        {
            feiQP = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV_PRED)
        {
            feiMVPred = &((mfxExtFeiPreEncMVPredictors*) (in->ExtParam[i]))[fieldId];
        }
    }

    //should be adjusted

    VAStatsStatisticsParameter16x16Intel m_statParams;
    memset(&m_statParams, 0, sizeof (VAStatsStatisticsParameter16x16Intel));
    VABufferID statParamsId = VA_INVALID_ID;

    //task.m_yuv->Data.DataFlag
    //m_core->GetExternalFrameHDL(); //video mem
    if (feiCtrl != NULL) // KW fix actually
    {
        m_statParams.adaptive_search = feiCtrl->AdaptiveSearch;
        m_statParams.disable_statistics_output = (mbstatOut == NULL) || feiCtrl->DisableStatisticsOutput;
        m_statParams.disable_mv_output = (mvsOut == NULL) || feiCtrl->DisableMVOutput;
        m_statParams.mb_qp = (feiQP == NULL) && feiCtrl->MBQp;
        m_statParams.mv_predictor_ctrl = (feiMVPred == NULL) ? feiCtrl->MVPredictor : 0;
    }
    else
    {
        m_statParams.adaptive_search = 0;
        m_statParams.disable_statistics_output = 1;
        m_statParams.disable_mv_output = 1;
        m_statParams.mb_qp = 0;
        m_statParams.mv_predictor_ctrl = 0;
    } // if (feiCtrl != NULL) // KW fix actually


    VABufferID outBuffers[2];
    int numOutBufs = 0;
    outBuffers[0] = VA_INVALID_ID;
    outBuffers[1] = VA_INVALID_ID;

    if ((m_statParams.mv_predictor_ctrl) && (feiMVPred != NULL) && (feiMVPred->MB != NULL)) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMVPredictorBufferTypeIntel,
                sizeof (VAMotionVectorIntel),
                numMB,
                feiMVPred->MB,
                &mvPredid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.mv_predictor = mvPredid;
        configBuffers[buffersCount++] = mvPredid;
        mdprintf(stderr, "MVPred bufId=%d\n", mvPredid);
    }

    if ((m_statParams.mb_qp) && (feiQP != NULL) && (feiQP->QP != NULL)) {

        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264),
                numMB,
                feiQP->QP,
                &qpid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.qp = qpid;
        configBuffers[buffersCount++] = qpid;
        mdprintf(stderr, "Qp bufId=%d\n", qpid);
    }

    if (!m_statParams.disable_mv_output) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMotionVectorBufferTypeIntel,
                sizeof (VAMotionVectorIntel)*numMB * 16, //16 MV per MB
                1,
                NULL, //should be mapped later
                &statMVid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        outBuffers[numOutBufs++] = statMVid;
        mdprintf(stderr, "MV bufId=%d\n", statMVid);
        configBuffers[buffersCount++] = statMVid;
    }

    if (!m_statParams.disable_statistics_output) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsStatisticsBufferTypeIntel,
                sizeof (VAStatsStatistics16x16Intel) * numMB,
                1,
                NULL,
                &statOUTid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        outBuffers[numOutBufs++] = statOUTid;
        mdprintf(stderr, "StatOut bufId=%d\n", statOUTid);
        configBuffers[buffersCount++] = statOUTid;
    }

    m_statParams.frame_qp = feiCtrl->Qp;
    m_statParams.ft_enable = feiCtrl->FTEnable;
    m_statParams.num_past_references = in->NumFrameL0;
    //currently only video mem is used, all input surfaces should be in video mem
    m_statParams.past_references = NULL;
    //fprintf(stderr,"in vaid: %x ", *inputSurface);
    if (in->NumFrameL0)
    {
        VASurfaceID* l0surfs = new VASurfaceID [in->NumFrameL0];
        m_statParams.past_references = &l0surfs[0]; //L0 refs

        for (int i = 0; i < in->NumFrameL0; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L0Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            l0surfs[i] = *s;
        }
      //  fprintf(stderr,"l0vaid: %x ", l0surfs[0]);
    }
    m_statParams.num_future_references = in->NumFrameL1;
    m_statParams.future_references = NULL;
    if (in->NumFrameL1)
    {
        VASurfaceID* l1surfs = new VASurfaceID [in->NumFrameL1];
        m_statParams.future_references = &l1surfs[0]; //L0 refs
        for (int i = 0; i < in->NumFrameL1; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L1Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle;
            l1surfs[i] = *s;
        }
        //fprintf(stderr,"l1vaid: %x", l1surfs[0]);
    }

    //fprintf(stderr,"\n");
    m_statParams.input = *inputSurface;
    m_statParams.interlaced = 0;
    m_statParams.inter_sad = feiCtrl->InterSAD;
    m_statParams.intra_sad = feiCtrl->IntraSAD;
    m_statParams.len_sp = feiCtrl->LenSP;
    m_statParams.max_len_sp = feiCtrl->MaxLenSP;
    m_statParams.outputs = &outBuffers[0]; //bufIDs for outputs
    m_statParams.sub_pel_mode = feiCtrl->SubPelMode;
    m_statParams.sub_mb_part_mask = feiCtrl->SubMBPartMask;
    m_statParams.ref_height = feiCtrl->RefHeight;
    m_statParams.ref_width = feiCtrl->RefWidth;
    m_statParams.search_window = feiCtrl->SearchWindow;

#if 0
    fprintf(stderr, "\n**** stat params:\n");
    fprintf(stderr, "input=%d\n", m_statParams.input);
    fprintf(stderr, "past_references=0x%x [", m_statParams.past_references);
    for (int i = 0; i < m_statParams.num_past_references; i++)
        fprintf(stderr, " %d", m_statParams.past_references[i]);
    fprintf(stderr, " ]\n");
    fprintf(stderr, "num_past_references=%d\n", m_statParams.num_past_references);
    fprintf(stderr, "future_references=0x%x [", m_statParams.future_references);
    for (int i = 0; i < m_statParams.num_future_references; i++)
        fprintf(stderr, " %d", m_statParams.future_references[i]);
    fprintf(stderr, " ]\n");
    fprintf(stderr, "num_future_references=%d\n", m_statParams.num_future_references);
    fprintf(stderr, "outputs=0x%x [", m_statParams.outputs);
    for (int i = 0; i < 2; i++)
        fprintf(stderr, " %d", m_statParams.outputs[i]);
    fprintf(stderr, " ]\n");

    fprintf(stderr, "mv_predictor=%d\n", m_statParams.mv_predictor);
    fprintf(stderr, "qp=%d\n", m_statParams.qp);
    fprintf(stderr, "frame_qp=%d\n", m_statParams.frame_qp);
    fprintf(stderr, "len_sp=%d\n", m_statParams.len_sp);
    fprintf(stderr, "max_len_sp=%d\n", m_statParams.max_len_sp);
    fprintf(stderr, "reserved0=%d\n", m_statParams.reserved0);
    fprintf(stderr, "sub_mb_part_mask=%d\n", m_statParams.sub_mb_part_mask);
    fprintf(stderr, "sub_pel_mode=%d\n", m_statParams.sub_pel_mode);
    fprintf(stderr, "inter_sad=%d\n", m_statParams.inter_sad);
    fprintf(stderr, "intra_sad=%d\n", m_statParams.intra_sad);
    fprintf(stderr, "adaptive_search=%d\n", m_statParams.adaptive_search);
    fprintf(stderr, "mv_predictor_ctrl=%d\n", m_statParams.mv_predictor_ctrl);
    fprintf(stderr, "mb_qp=%d\n", m_statParams.mb_qp);
    fprintf(stderr, "ft_enable=%d\n", m_statParams.ft_enable);
    fprintf(stderr, "reserved1=%d\n", m_statParams.reserved1);
    fprintf(stderr, "ref_width=%d\n", m_statParams.ref_width);
    fprintf(stderr, "ref_height=%d\n", m_statParams.ref_height);
    fprintf(stderr, "search_window=%d\n", m_statParams.search_window);
    fprintf(stderr, "reserved2=%d\n", m_statParams.reserved2);
    fprintf(stderr, "disable_mv_output=%d\n", m_statParams.disable_mv_output);
    fprintf(stderr, "disable_statistics_output=%d\n", m_statParams.disable_statistics_output);
    fprintf(stderr, "interlaced=%d\n", m_statParams.interlaced);
    fprintf(stderr, "reserved3=%d\n", m_statParams.reserved3);
    fprintf(stderr, "\n");
#endif

    //MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            (VABufferType)VAStatsStatisticsParameterBufferTypeIntel,
            sizeof (m_statParams)*numMB,
            1,
            &m_statParams,
            &statParamsId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    configBuffers[buffersCount++] = statParamsId;

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv = statMVid;
        currentFeedback.mbstat = statOUTid;
        m_statFeedbackCache.push_back(currentFeedback);
    }

    //should we release here or not
    if (m_statParams.past_references)
    {
        delete [] m_statParams.past_references;
    }
    if (m_statParams.future_references)
    {
        delete [] m_statParams.future_references;
    }

    //destroy buffers - this should be removed
    MFX_DESTROY_VABUFFER(statParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(qpid, m_vaDisplay);
    /* these allocated buffers will be destroyed in QueryStatus()
     * when driver returned result of preenc processing  */
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    //MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIPREENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID statMVid = VA_INVALID_ID;
    VABufferID statOUTid = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            statMVid = currentFeedback.mv;
            //statMVid = m_statMVId.pop_front()
            statOUTid = currentFeedback.mbstat;

            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            VAMotionVectorIntel* mvs;
            VAStatsStatistics16x16Intel* mbstat;

            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

            //find output surfaces
            mfxExtFeiPreEncMV* mvsOut = NULL;
            for (int i = 0; i < out->NumExtParam; i++) {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV)
                {
                    mvsOut = &((mfxExtFeiPreEncMV*)(out->ExtParam[i]))[fieldId];
                    break;
                }
            }

            mfxExtFeiPreEncMBStat* mbstatOut = NULL;
            for (int i = 0; i < out->NumExtParam; i++) {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MB)
                {
                    mbstatOut = &((mfxExtFeiPreEncMBStat*)(out->ExtParam[i]))[fieldId];
                    break;
                }
            }

            //find control buffer
            mfxExtFeiPreEncCtrl* feiCtrl = NULL;
            for (int i = 0; i < in->NumExtParam; i++) {
                if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_CTRL)
                {
                    feiCtrl = &((mfxExtFeiPreEncCtrl*)(in->ExtParam[i]))[fieldId];
                    break;
                }
            }

            if (feiCtrl && !feiCtrl->DisableMVOutput && mvsOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statMVid,
                            (void **) (&mvs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvsOut->MB, 16 * sizeof (VAMotionVectorIntel) * mvsOut->NumMBAlloc,
                                mvs, 16 * sizeof (VAMotionVectorIntel) * mvsOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statMVid);
                }

                MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
            }

            if (feiCtrl && !feiCtrl->DisableStatisticsOutput && mbstatOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statOUTid,
                            (void **) (&mbstat));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbstatOut->MB, sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc,
                                mbstat, sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statOUTid);
                }

                MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            return MFX_ERR_NONE;
#if 0
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
#endif
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)

#define NAL_REF_IDC_NONE        0
#define NAL_REF_IDC_LOW         1
#define NAL_REF_IDC_MEDIUM      2
#define NAL_REF_IDC_HIGH        3

#define NAL_NON_IDR             1
#define NAL_IDR                 5
#define NAL_SPS                 7
#define NAL_PPS                 8


#define SLICE_TYPE_P            0
#define SLICE_TYPE_B            1
#define SLICE_TYPE_I            2

#define ENTROPY_MODE_CAVLC      0
#define ENTROPY_MODE_CABAC      1

#define PROFILE_IDC_BASELINE    66
#define PROFILE_IDC_MAIN        77
#define PROFILE_IDC_HIGH        100

#define BITSTREAM_ALLOCATE_STEPPING     4096
static int frame_bit_rate = -1;

#define SID_REFERENCE_PICTURE_L0                (2 - 2)
#define SID_REFERENCE_PICTURE_L1                (3 - 2)
#define SID_RECON_PICTURE                       (4 - 2)

struct __bitstream {
    unsigned int *buffer;
    int bit_offset;
    int max_size_in_dword;
};

typedef struct __bitstream bitstream;

static unsigned int
swap32(unsigned int val)
{
    unsigned char *pval = (unsigned char *)&val;

    return ((pval[0] << 24)     |
            (pval[1] << 16)     |
            (pval[2] << 8)      |
            (pval[3] << 0));
}

static void
bitstream_start(bitstream *bs)
{
    bs->max_size_in_dword = BITSTREAM_ALLOCATE_STEPPING;
    bs->buffer = (unsigned int*)calloc(bs->max_size_in_dword * sizeof(int), 1);
    bs->bit_offset = 0;
}

static void
bitstream_end(bitstream *bs)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (bit_offset) {
        bs->buffer[pos] = swap32((bs->buffer[pos] << bit_left));
    }
}

static void
bitstream_put_ui(bitstream *bs, unsigned int val, int size_in_bits)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (!size_in_bits)
        return;

    bs->bit_offset += size_in_bits;

    if (bit_left > size_in_bits) {
        bs->buffer[pos] = (bs->buffer[pos] << size_in_bits | val);
    } else {
        size_in_bits -= bit_left;
        bs->buffer[pos] = (bs->buffer[pos] << bit_left) | (val >> size_in_bits);
        bs->buffer[pos] = swap32(bs->buffer[pos]);

        if (pos + 1 == bs->max_size_in_dword) {
            bs->max_size_in_dword += BITSTREAM_ALLOCATE_STEPPING;
            bs->buffer = (unsigned int*)realloc(bs->buffer, bs->max_size_in_dword * sizeof(unsigned int));
        }
        /* KW fix */
        if (NULL != bs->buffer)
            bs->buffer[pos + 1] = val;
    }
}

static void
bitstream_put_ue(bitstream *bs, unsigned int val)
{
    int size_in_bits = 0;
    int tmp_val = ++val;

    while (tmp_val) {
        tmp_val >>= 1;
        size_in_bits++;
    }

    bitstream_put_ui(bs, 0, size_in_bits - 1); // leading zero
    bitstream_put_ui(bs, val, size_in_bits);
}

static void
bitstream_put_se(bitstream *bs, int val)
{
    unsigned int new_val;

    if (val <= 0)
        new_val = -2 * val;
    else
        new_val = 2 * val - 1;

    bitstream_put_ue(bs, new_val);
}

static void
bitstream_byte_aligning(bitstream *bs, int bit)
{
    int bit_offset = (bs->bit_offset & 0x7);
    int bit_left = 8 - bit_offset;
    int new_val;

    if (!bit_offset)
        return;

    assert(bit == 0 || bit == 1);

    if (bit)
        new_val = (1 << bit_left) - 1;
    else
        new_val = 0;

    bitstream_put_ui(bs, new_val, bit_left);
}

static void
rbsp_trailing_bits(bitstream *bs)
{
    bitstream_put_ui(bs, 1, 1);
    bitstream_byte_aligning(bs, 0);
}

static void nal_start_code_prefix(bitstream *bs)
{
    bitstream_put_ui(bs, 0x00000001, 32);
}

static void nal_header(bitstream *bs, int nal_ref_idc, int nal_unit_type)
{
    bitstream_put_ui(bs, 0, 1);                /* forbidden_zero_bit: 0 */
    bitstream_put_ui(bs, nal_ref_idc, 2);
    bitstream_put_ui(bs, nal_unit_type, 5);
}

static void sps_rbsp(bitstream *bs, VAEncSequenceParameterBufferH264 *seq_param, VAProfile profile)
{
    //VAEncSequenceParameterBufferH264 *seq_param = &avcenc_context.seq_param;
    int profile_idc = PROFILE_IDC_BASELINE;

    if (profile == VAProfileH264High)
        profile_idc = PROFILE_IDC_HIGH;
    else if (profile == VAProfileH264Main)
        profile_idc = PROFILE_IDC_MAIN;

    bitstream_put_ui(bs, profile_idc, 8);               /* profile_idc */
    bitstream_put_ui(bs, 0, 1);                         /* constraint_set0_flag */
    bitstream_put_ui(bs, 1, 1);                         /* constraint_set1_flag */
    bitstream_put_ui(bs, 0, 1);                         /* constraint_set2_flag */
    bitstream_put_ui(bs, 0, 1);                         /* constraint_set3_flag */
    bitstream_put_ui(bs, 0, 4);                         /* reserved_zero_4bits */
    bitstream_put_ui(bs, seq_param->level_idc, 8);      /* level_idc */
    bitstream_put_ue(bs, seq_param->seq_parameter_set_id);      /* seq_parameter_set_id */

    if (profile == VAProfileH264High)
    {
        bitstream_put_ue(bs, 1);  /* chroma_format_idc */
        bitstream_put_ue(bs, 0);  /* bit_depth_luma_minus8 */
        bitstream_put_ue(bs, 0);  /* bit_depth_chroma_minus8 */
        bitstream_put_ui(bs, 0, 1); /* qpprime_y_zero_transform_bypass_flag */
        bitstream_put_ui(bs, 0, 1); /* seq_scaling_matrix_present_flag */
    }

    bitstream_put_ue(bs, seq_param->seq_fields.bits.log2_max_frame_num_minus4); /* log2_max_frame_num_minus4 */

    bitstream_put_ue(bs, seq_param->seq_fields.bits.pic_order_cnt_type);        /* pic_order_cnt_type */

    if (seq_param->seq_fields.bits.pic_order_cnt_type == 0)
        bitstream_put_ue(bs, seq_param->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);     /* log2_max_pic_order_cnt_lsb_minus4 */
    else {
        assert(0);
    }

    bitstream_put_ue(bs, seq_param->max_num_ref_frames);        /* num_ref_frames */
    bitstream_put_ui(bs, 0, 1);                                 /* gaps_in_frame_num_value_allowed_flag */

    bitstream_put_ue(bs, seq_param->picture_width_in_mbs - 1);  /* pic_width_in_mbs_minus1 */
    bitstream_put_ue(bs, seq_param->picture_height_in_mbs - 1); /* pic_height_in_map_units_minus1 */
    bitstream_put_ui(bs, seq_param->seq_fields.bits.frame_mbs_only_flag, 1);    /* frame_mbs_only_flag */

    if (!seq_param->seq_fields.bits.frame_mbs_only_flag) {
        assert(0);
    }

    bitstream_put_ui(bs, seq_param->seq_fields.bits.direct_8x8_inference_flag, 1);      /* direct_8x8_inference_flag */
    bitstream_put_ui(bs, seq_param->frame_cropping_flag, 1);            /* frame_cropping_flag */

    if (seq_param->frame_cropping_flag) {
        bitstream_put_ue(bs, seq_param->frame_crop_left_offset);        /* frame_crop_left_offset */
        bitstream_put_ue(bs, seq_param->frame_crop_right_offset);       /* frame_crop_right_offset */
        bitstream_put_ue(bs, seq_param->frame_crop_top_offset);         /* frame_crop_top_offset */
        bitstream_put_ue(bs, seq_param->frame_crop_bottom_offset);      /* frame_crop_bottom_offset */
    }

    if ( frame_bit_rate < 0 ) {
        bitstream_put_ui(bs, 0, 1); /* vui_parameters_present_flag */
    } else {
        bitstream_put_ui(bs, 1, 1); /* vui_parameters_present_flag */
        bitstream_put_ui(bs, 0, 1); /* aspect_ratio_info_present_flag */
        bitstream_put_ui(bs, 0, 1); /* overscan_info_present_flag */
        bitstream_put_ui(bs, 0, 1); /* video_signal_type_present_flag */
        bitstream_put_ui(bs, 0, 1); /* chroma_loc_info_present_flag */
        bitstream_put_ui(bs, 1, 1); /* timing_info_present_flag */
        {
            bitstream_put_ui(bs, 15, 32);
            bitstream_put_ui(bs, 900, 32);
            bitstream_put_ui(bs, 1, 1);
        }
        bitstream_put_ui(bs, 1, 1); /* nal_hrd_parameters_present_flag */
        {
            // hrd_parameters
            bitstream_put_ue(bs, 0);    /* cpb_cnt_minus1 */
            bitstream_put_ui(bs, 4, 4); /* bit_rate_scale */
            bitstream_put_ui(bs, 6, 4); /* cpb_size_scale */

            bitstream_put_ue(bs, frame_bit_rate - 1); /* bit_rate_value_minus1[0] */
            bitstream_put_ue(bs, frame_bit_rate*8 - 1); /* cpb_size_value_minus1[0] */
            bitstream_put_ui(bs, 1, 1);  /* cbr_flag[0] */

            bitstream_put_ui(bs, 23, 5);   /* initial_cpb_removal_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* cpb_removal_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* dpb_output_delay_length_minus1 */
            bitstream_put_ui(bs, 23, 5);   /* time_offset_length  */
        }
        bitstream_put_ui(bs, 0, 1);   /* vcl_hrd_parameters_present_flag */
        bitstream_put_ui(bs, 0, 1);   /* low_delay_hrd_flag */

        bitstream_put_ui(bs, 0, 1); /* pic_struct_present_flag */
        bitstream_put_ui(bs, 0, 1); /* bitstream_restriction_flag */
    }

    rbsp_trailing_bits(bs);     /* rbsp_trailing_bits */
}

static void pps_rbsp(bitstream *bs, VAEncPictureParameterBufferH264 *pic_param, VAProfile profile)
{
    //VAEncPictureParameterBufferH264 *pic_param = &avcenc_context.pic_param;

    bitstream_put_ue(bs, pic_param->pic_parameter_set_id);      /* pic_parameter_set_id */
    bitstream_put_ue(bs, pic_param->seq_parameter_set_id);      /* seq_parameter_set_id */

    bitstream_put_ui(bs, pic_param->pic_fields.bits.entropy_coding_mode_flag, 1);  /* entropy_coding_mode_flag */

    bitstream_put_ui(bs, 0, 1);                         /* pic_order_present_flag: 0 */

    bitstream_put_ue(bs, 0);                            /* num_slice_groups_minus1 */

    bitstream_put_ue(bs, pic_param->num_ref_idx_l0_active_minus1);      /* num_ref_idx_l0_active_minus1 */
    bitstream_put_ue(bs, pic_param->num_ref_idx_l1_active_minus1);      /* num_ref_idx_l1_active_minus1 1 */

    bitstream_put_ui(bs, pic_param->pic_fields.bits.weighted_pred_flag, 1);     /* weighted_pred_flag: 0 */
    bitstream_put_ui(bs, pic_param->pic_fields.bits.weighted_bipred_idc, 2);    /* weighted_bipred_idc: 0 */

    bitstream_put_se(bs, pic_param->pic_init_qp - 26);  /* pic_init_qp_minus26 */
    bitstream_put_se(bs, 0);                            /* pic_init_qs_minus26 */
    bitstream_put_se(bs, 0);                            /* chroma_qp_index_offset */

    bitstream_put_ui(bs, pic_param->pic_fields.bits.deblocking_filter_control_present_flag, 1); /* deblocking_filter_control_present_flag */
    bitstream_put_ui(bs, 0, 1);                         /* constrained_intra_pred_flag */
    bitstream_put_ui(bs, 0, 1);                         /* redundant_pic_cnt_present_flag */

    /* more_rbsp_data */
    if (profile == VAProfileH264High)
    {
        bitstream_put_ui(bs, pic_param->pic_fields.bits.transform_8x8_mode_flag, 1);    /*transform_8x8_mode_flag */
        bitstream_put_ui(bs, 0, 1);                         /* pic_scaling_matrix_present_flag */
        bitstream_put_se(bs, pic_param->second_chroma_qp_index_offset );    /*second_chroma_qp_index_offset */
    }
    rbsp_trailing_bits(bs);
}


static int
build_packed_pic_buffer(unsigned char **header_buffer,VAEncPictureParameterBufferH264 *pic_param, VAProfile profile)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);
    nal_header(&bs, NAL_REF_IDC_HIGH, NAL_PPS);
    pps_rbsp(&bs, pic_param, profile);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int
build_packed_seq_buffer(unsigned char **header_buffer, VAEncSequenceParameterBufferH264 *seq_param,
                                VAProfile profile)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs);
    nal_header(&bs, NAL_REF_IDC_HIGH, NAL_SPS);
    sps_rbsp(&bs, seq_param, profile);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}


VAAPIFEIENCEncoder::VAAPIFEIENCEncoder()
: VAAPIEncoder()
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
, m_currentFrameNum(0)
, m_codedBufferId(VA_INVALID_ID)
, m_lastRefFrameOrder(0){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIENCEncoder::~VAAPIFEIENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_ENC)
        return CreateENCAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIENCEncoder::CreateENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_ENC_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

//#endif

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    //FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    //FillConstPartOfPps(par, m_pps);

    //if (m_caps.HeaderInsertion == 0)
    //    m_headerPacker.Init(par, m_caps);


    //FillSps(par, m_sps);
#if !TMP_DISABLE
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
#endif
    //FillConstPartOfPps(par, m_pps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    mfxStatus mfxSts = MFX_ERR_NONE;

    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    /*--------------------------------------------------------------*/
    //avcenc_context.current_input_surface = next_input_id;
    unsigned int length_in_bits, offset_in_bytes;
    unsigned char *packed_seq_buffer = NULL, *packed_slc_buffer=NULL;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    //if (0 == m_currentFrameNum)
    if ((0 == task.m_frameNum) && (0 == task.m_frameOrder))
    {
        /* sps init */
        m_sps.seq_parameter_set_id = 0;
        m_sps.level_idc = 41;
        m_sps.intra_period = m_videoParam.mfx.GopPicSize;
        m_sps.ip_period    = m_videoParam.mfx.GopRefDist;
        m_sps.max_num_ref_frames = 4;
        m_sps.picture_width_in_mbs = m_videoParam.mfx.FrameInfo.Width/16;
        m_sps.picture_height_in_mbs = m_videoParam.mfx.FrameInfo.Height/16;
        m_sps.seq_fields.bits.frame_mbs_only_flag = 1;

        if (frame_bit_rate > 0)
            m_sps.bits_per_second = 1024 * frame_bit_rate; /* use kbps as input */
        else
            m_sps.bits_per_second = 0;

        m_sps.time_scale = 900;
        m_sps.num_units_in_tick = 15;          /* Tc = num_units_in_tick / time_sacle */

        m_sps.frame_cropping_flag = 0;
        m_sps.frame_crop_left_offset = 0;
        m_sps.frame_crop_right_offset = 0;
        m_sps.frame_crop_top_offset = 0;
        m_sps.frame_crop_bottom_offset = 0;

        m_sps.seq_fields.bits.pic_order_cnt_type = 0;
        m_sps.seq_fields.bits.direct_8x8_inference_flag = 1;

        m_sps.seq_fields.bits.log2_max_frame_num_minus4 = 0;

        m_sps.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = 2;

        if (frame_bit_rate > 0)
            m_sps.vui_parameters_present_flag = 1; //HRD info located in vui
        else
            m_sps.vui_parameters_present_flag = 0;


        //assert(slice_type == SLICE_TYPE_I);
        length_in_bits = build_packed_seq_buffer(&packed_seq_buffer, &m_sps,
                                        ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile));
        offset_in_bytes = 0;
        packed_header_param_buffer.type = VAEncPackedHeaderSequence;
        packed_header_param_buffer.bit_length = length_in_bits;
        packed_header_param_buffer.has_emulation_bytes = 1;//0;
        vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderParameterBufferType,
                                   sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                                   &m_packedSpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_packedSpsHeaderBufferId=%d\n", m_packedSpsHeaderBufferId);
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncPackedHeaderDataBufferType,
                                   (length_in_bits + 7) / 8, 1, packed_seq_buffer,
                                   &m_packedSpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_packedSpsBufferId=%d\n", m_packedSpsBufferId);
        configBuffers[buffersCount++] = m_packedSpsBufferId;

        free(packed_seq_buffer);
    }

    /* sequence parameter set */
    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                               VAEncSequenceParameterBufferType,
                               sizeof(m_sps),1,
                               &m_sps,
                               &m_spsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
    configBuffers[buffersCount++] = m_spsBufferId;

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    VABufferID vaFeiMCODEOutId = VA_INVALID_ID;

    //find ext buffers
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiEncMBCtrl* mbctrl = NULL;
    mfxExtFeiEncMVPredictors* mvpred = NULL;
    mfxExtFeiEncFrameCtrl* frameCtrl = NULL;
    mfxExtFeiEncQP* mbqp = NULL;
    mfxExtFeiEncMBStat* mbstat = NULL;
    mfxExtFeiEncMV* mvout = NULL;
    mfxExtFeiPakMBCtrl* mbcodeout = NULL;

    /* in buffers */
    for (int i = 0; i < in->NumExtParam; i++)
    {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL)
        {
            frameCtrl = &((mfxExtFeiEncFrameCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED)
        {
            mvpred = &((mfxExtFeiEncMVPredictors*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB)
        {
            mbctrl = &((mfxExtFeiEncMBCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
        {
            mbqp = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
        }
    }

    for (int i = 0; i < out->NumExtParam; i++)
    {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
        {
            mvout = &((mfxExtFeiEncMV*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
        {
            mbstat = &((mfxExtFeiEncMBStat*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
        {
            mbcodeout = &((mfxExtFeiPakMBCtrl*) (out->ExtParam[i]))[fieldId];
        }
    }

    if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
                sizeof(VAEncMVPredictorH264Intel)*numMB,  //limitation from driver, num elements should be 1
                1,
                mvpred->MB,
                &vaFeiMVPredId );
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMVPredId;
        mdprintf(stderr, "vaFeiMVPredId=%d\n", vaFeiMVPredId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIMBControlBufferTypeIntel,
                sizeof (VAEncFEIMBControlH264Intel)*numMB,  //limitation from driver, num elements should be 1
                1,
                mbctrl->MB,
                &vaFeiMBControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBControlId;
        mdprintf(stderr, "vaFeiMBControlId=%d\n", vaFeiMBControlId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264)*numMB, //limitation from driver, num elements should be 1
                1,
                mbqp->QP,
                &vaFeiMBQPId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBQPId;
        mdprintf(stderr, "vaFeiMBQPId=%d\n", vaFeiMBQPId);
    }

    //output buffer for MB distortions
    if (mbstat != NULL)
    {
        if (m_vaFeiMBStatId.size() == 0 )
        {
            //m_vaFeiMBStatId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                        sizeof (VAEncFEIDistortionBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        &vaFeiMBStatId);
                        //&m_vaFeiMBStatId[ii]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMBStatId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMBStatId;
        //outBuffers[numOutBufs++] = m_vaFeiMBStatId[idxRecon];
        mdprintf(stderr, "MB Stat bufId=%d\n", vaFeiMBStatId);
        configBuffers[buffersCount++] = vaFeiMBStatId;
        //configBuffers[buffersCount++] = m_vaFeiMBStatId[idxRecon];
    }

    //output buffer for MV
    if (mvout != NULL)
    {
        //if (m_vaFeiMVOutId.size() == 0 )
        {
            //m_vaFeiMVOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIMVBufferTypeIntel,
                        sizeof (VAMotionVectorIntel)*16*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        //&m_vaFeiMVOutId[ii]);
                        &vaFeiMVOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMVOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMVOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMVOutId[idxRecon];
        mdprintf(stderr, "MV Out bufId=%d\n", vaFeiMVOutId);
        configBuffers[buffersCount++] = vaFeiMVOutId;
        //configBuffers[buffersCount++] = m_vaFeiMVOutId[idxRecon];
    }

    //output buffer for MBCODE (Pak object cmds)
    if (mbcodeout != NULL)
    {
        //if (m_vaFeiMCODEOutId.size() == 0 )
        {
            //m_vaFeiMCODEOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIModeBufferTypeIntel,
                        sizeof (VAEncFEIModeBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        //&m_vaFeiMCODEOutId[ii]);
                        &vaFeiMCODEOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } //if (m_vaFeiMCODEOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMCODEOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMCODEOutId[idxRecon];
        mdprintf(stderr, "MCODE Out bufId=%d\n", vaFeiMCODEOutId);
        configBuffers[buffersCount++] = vaFeiMCODEOutId;
        //configBuffers[buffersCount++] = m_vaFeiMCODEOutId[idxRecon];
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->max_len_sp = frameCtrl->MaxLenSP;

        vaFeiFrameControl->distortion = vaFeiMBStatId;
        vaFeiFrameControl->mv_data = vaFeiMVOutId;
        vaFeiFrameControl->mb_code_data = vaFeiMCODEOutId;
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
        vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
        vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
        vaFeiFrameControl->num_mv_predictors = frameCtrl->NumMVPredictors;
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);
    }
//    mfxU32 idxRecon = task.m_idxBs[fieldId];

    int ref_counter_l0 = 0;
    int ref_counter_l1 = 0;
    /* to fill L0 List*/
    if (in->NumFrameL0)
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            m_pps.ReferenceFrames[ref_counter_l0].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n ",
                    ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
        }
    }
    /* to fill L1 List*/
    if (in->NumFrameL1)
    {
        /* continue to fill ref list */
        for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle;
            m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n",ref_counter_l0 + ref_counter_l1,
                                    m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id);
        }
    }

    int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;
    mdprintf(stderr,"slice_type=%d\n",slice_type);
    /* slice parameters */
    {
        int i = 0, j = 0;

        mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        /* To check task.m_numSlic */
        if (task.m_numSlice[fieldId])
            m_slice.resize(task.m_numSlice[fieldId]);
        mfxU32 numSlice = m_slice.size();
        mfxU32 idx = 0, ref = 0;

        SliceDivider divider = MakeSliceDivider(
            m_caps.SliceStructure,
            task.m_numMbPerSlice,
            numSlice,
            m_sps.picture_width_in_mbs,
            m_sps.picture_height_in_mbs / numPics);

        for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )
        {
            m_slice[i].macroblock_address = divider.GetFirstMbInSlice();
            m_slice[i].num_macroblocks = divider.GetNumMbInSlice();
            m_slice[i].macroblock_info = VA_INVALID_ID;
            m_slice[i].pic_parameter_set_id = 0;
            m_slice[i].slice_type = slice_type;
            m_slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));
            m_slice[i].direct_spatial_mv_pred_flag = 1;  // to match header NALU

            m_slice[i].num_ref_idx_l0_active_minus1 = 0;
            m_slice[i].num_ref_idx_l1_active_minus1 = 0;

            if ( slice_type == SLICE_TYPE_I ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = -1;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_P ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_B ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = in->NumFrameL1;
            }

            m_slice[i].cabac_init_idc = 0;
            m_slice[i].slice_qp_delta = 0;
            m_slice[i].disable_deblocking_filter_idc = 0;
            m_slice[i].slice_alpha_c0_offset_div2 = 2;
            m_slice[i].slice_beta_offset_div2 = 2;
            m_slice[i].idr_pic_id = 0;
            /*    */
            for (j=0; j<32; j++)
            {
                m_slice[i].RefPicList0[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[j].flags = VA_PICTURE_H264_INVALID;
                m_slice[i].RefPicList1[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[j].flags = VA_PICTURE_H264_INVALID;
            }

            if ((in->NumFrameL0) && ((slice_type == SLICE_TYPE_P) || (slice_type == SLICE_TYPE_P)) )
            {
                for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList0[ref_counter_l0].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
            if ( (in->NumFrameL1) && (slice_type == SLICE_TYPE_B) )
            {
                for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
        } // for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )

        for( size_t i = 0; i < m_slice.size(); i++ )
        {
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            //configBuffers[buffersCount++] = m_sliceBufferId[i];
            mdprintf(stderr, "m_sliceBufferId[%d]=%d\n", i, m_sliceBufferId[i]);
        }
    } /* slice parameters */

    int i = 0;;
    // Picture level
    //unsigned int length_in_bits, offset_in_bytes;
    //VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    unsigned char *packed_pic_buffer = NULL;
    /* hard coded, ENC does not generate real reconstruct surface,
     * and this surface should be unchanged
     * BUT (!) this surface should be from reconstruct surface pool which was passed to
     * component when vaCreateContext was called */
    m_pps.CurrPic.picture_id = m_reconQueue[0].surface;
    m_pps.CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    mdprintf(stderr,"m_pps.CurrPic.picture_id = %d\n",m_pps.CurrPic.picture_id);
    m_pps.pic_init_qp = 26;
    m_pps.pic_fields.bits.entropy_coding_mode_flag = 1; //ENTROPY_MODE_CABAC;
    m_pps.pic_fields.bits.transform_8x8_mode_flag = 1;
    m_pps.pic_fields.bits.deblocking_filter_control_present_flag = 1;

    /*FIXME*/
    //pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
    //pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);
    int display_num = task.m_frameOrder;
    m_pps.CurrPic.TopFieldOrderCnt = display_num * 2;
    m_pps.CurrPic.BottomFieldOrderCnt = display_num * 2+1;
    Cur_POC[0] = display_num * 2;
    Cur_POC[1] = display_num * 2 + 1;

    /*FIXME*/
    /* Need to allocated coded buffer
     * For ENC it should be NULL, but lets keep for a while */
    if (VA_INVALID_ID == m_codedBufferId)
    {
        int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
        int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
        int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncCodedBufferType,
                                codedbuf_size,
                                1,
                                NULL,
                                &m_codedBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_codedBufferId;
        mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId);
    }
    m_pps.coded_buf = m_codedBufferId;
    m_pps.frame_num = task.m_frameNum;
    m_pps.pic_fields.bits.idr_pic_flag =  (task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    m_pps.pic_fields.bits.reference_pic_flag = (slice_type != SLICE_TYPE_B);

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPictureParameterBufferType,
                            sizeof(m_pps),
                            1,
                            &m_pps,
                            &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_ppsBufferId;
    mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

    length_in_bits = build_packed_pic_buffer(&packed_pic_buffer, &m_pps,
                                ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile));
    offset_in_bytes = 0;
    packed_header_param_buffer.type = VAEncPackedHeaderPicture;
    packed_header_param_buffer.bit_length = length_in_bits;
    packed_header_param_buffer.has_emulation_bytes = 1;//0;

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packed_header_param_buffer),
                               1,
                               &packed_header_param_buffer,
                               &m_packedPpsHeaderBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
    mdprintf(stderr, "m_packedPpsHeaderBufferId=%d\n", m_packedPpsHeaderBufferId);

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1,
                               packed_pic_buffer,
                               &m_packedPpsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_packedPpsBufferId;
    mdprintf(stderr, "m_packedPpsBufferId=%d\n", m_packedPpsBufferId);

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
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
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = vaFeiMVOutId;
        currentFeedback.mbstat    = vaFeiMBStatId;
        currentFeedback.mbcode    = vaFeiMCODEOutId;
        m_statFeedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            mfxExtFeiEncMBStat* mbstat = NULL;
            mfxExtFeiEncMV* mvout = NULL;
            mfxExtFeiPakMBCtrl* mbcodeout = NULL;
            int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

            /* NO Bitstream in ENC */
            task.m_bsDataLength[fieldId] = 0;

            for (int i = 0; i < out->NumExtParam; i++)
            {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
                {
                    mvout = &((mfxExtFeiEncMV*) (out->ExtParam[i]))[fieldId];
                }
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
                {
                    mbstat = &((mfxExtFeiEncMBStat*) (out->ExtParam[i]))[fieldId];
                }
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
                {
                    mbcodeout = &((mfxExtFeiPakMBCtrl*) (out->ExtParam[i]))[fieldId];
                }
            }

            if (mbstat != NULL && vaFeiMBStatId != VA_INVALID_ID)
            {
                VAEncFEIDistortionBufferH264Intel* mbs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMBStatId,
                            (void **) (&mbs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbstat->MB, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB,
                                mbs, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBStatId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMBStatId, m_vaDisplay);
                vaFeiMBStatId = VA_INVALID_ID;
            }

            if (mvout != NULL && vaFeiMVOutId != VA_INVALID_ID)
            {
                VAMotionVectorIntel* mvs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMVOutId,
                            (void **) (&mvs));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvout->MB, sizeof (VAMotionVectorIntel) * 16 * numMB,
                               mvs, sizeof (VAMotionVectorIntel) * 16 * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMVOutId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMVOutId, m_vaDisplay);
                vaFeiMVOutId = VA_INVALID_ID;
            }

            if (mbcodeout != NULL && vaFeiMBCODEOutId != VA_INVALID_ID)
            {
                VAEncFEIModeBufferH264Intel* mbcs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        vaFeiMBCODEOutId,
                        (void **) (&mbcs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbcodeout->MB, sizeof (VAEncFEIModeBufferH264Intel) * numMB,
                                  mbcs, sizeof (VAEncFEIModeBufferH264Intel) * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBCODEOutId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMBCODEOutId, m_vaDisplay);
                vaFeiMBCODEOutId = VA_INVALID_ID;
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);

            VABufferID tempID;
            int next_is_bpic = 0;
            int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;

            /* Prepare for next picture */
            //tempID = m_reconQueue[SID_RECON_PICTURE].surface;
            if ((task.m_frameOrder - m_lastRefFrameOrder) >1)
                next_is_bpic = 1;
            if (task.m_frameOrder > m_lastRefFrameOrder)
                m_lastRefFrameOrder = task.m_frameOrder;


//            if (slice_type != SLICE_TYPE_B) {
//                if (next_is_bpic) {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L1].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L1].surface = tempID;
//                    ref1_POC[0] = Cur_POC[0];
//                    ref1_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                } else {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L0].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L0].surface = tempID;
//                    ref0_POC[0] = Cur_POC[0];
//                    ref0_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                }
//            } else {
//                if (!next_is_bpic) {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L0].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L0].surface = m_reconQueue[SID_REFERENCE_PICTURE_L1].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L1].surface = tempID;
//                    ref0_POC[0] = ref1_POC[0];
//                    ref0_POC[1] = ref1_POC[1];
//                    ref1_POC[0] = Cur_POC[0];
//                    ref1_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                }
//            }

        }
            return MFX_ERR_NONE;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif //#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)


#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

VAAPIFEIPAKEncoder::VAAPIFEIPAKEncoder()
: VAAPIEncoder()
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
, m_currentFrameNum(0)
, m_codedBufferId(VA_INVALID_ID)
, m_lastRefFrameOrder(0){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPAKEncoder::~VAAPIFEIPAKEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPAKEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPAKEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_PAK)
        return CreatePAKAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPAKEncoder::CreatePAKAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_PAK_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

//#endif

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    //FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    //FillConstPartOfPps(par, m_pps);

    //if (m_caps.HeaderInsertion == 0)
    //    m_headerPacker.Init(par, m_caps);


    //FillSps(par, m_sps);
#if !TMP_DISABLE
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
#endif
    //FillConstPartOfPps(par, m_pps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIPAKEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIFEIPAKEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;

    mfxStatus mfxSts = MFX_ERR_NONE;

    //VASurfaceID *inputSurface = (VASurfaceID*) surface;
    VASurfaceID *inputSurface = NULL;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    /*--------------------------------------------------------------*/
    //avcenc_context.current_input_surface = next_input_id;
    unsigned int length_in_bits, offset_in_bytes;
    unsigned char *packed_seq_buffer = NULL, *packed_slc_buffer=NULL;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    //if (0 == m_currentFrameNum)
    if ((0 == task.m_frameNum) && (0 == task.m_frameOrder))
    {
        /* sps init */
        m_sps.seq_parameter_set_id = 0;
        m_sps.level_idc = 41;
        m_sps.intra_period = m_videoParam.mfx.GopPicSize;
        m_sps.ip_period    = m_videoParam.mfx.GopRefDist;
        m_sps.max_num_ref_frames = 1;
        m_sps.picture_width_in_mbs = m_videoParam.mfx.FrameInfo.Width/16;
        m_sps.picture_height_in_mbs = m_videoParam.mfx.FrameInfo.Height/16;
        m_sps.seq_fields.bits.chroma_format_idc = 1;
        m_sps.seq_fields.bits.frame_mbs_only_flag = 1;

        if (frame_bit_rate > 0)
            m_sps.bits_per_second = 1024 * frame_bit_rate; /* use kbps as input */
        else
            m_sps.bits_per_second = 0;

        m_sps.time_scale = 900;
        m_sps.num_units_in_tick = 15;          /* Tc = num_units_in_tick / time_sacle */

        m_sps.frame_cropping_flag = 0;
        m_sps.frame_crop_left_offset = 0;
        m_sps.frame_crop_right_offset = 0;
        m_sps.frame_crop_top_offset = 0;
        m_sps.frame_crop_bottom_offset = 0;

        m_sps.seq_fields.bits.pic_order_cnt_type = 0;
        m_sps.seq_fields.bits.direct_8x8_inference_flag = 1;

        m_sps.seq_fields.bits.log2_max_frame_num_minus4 = 4;

        m_sps.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = 0;

        m_sps.seq_fields.bits.delta_pic_order_always_zero_flag = 1;

        if (frame_bit_rate > 0)
            m_sps.vui_parameters_present_flag = 1; //HRD info located in vui
        else
            m_sps.vui_parameters_present_flag = 0;

        m_sps.vui_parameters_present_flag = 1;
        m_sps.vui_fields.bits.aspect_ratio_info_present_flag = 0;
        m_sps.vui_fields.bits.timing_info_present_flag = 1;
        m_sps.vui_fields.bits.bitstream_restriction_flag = 1;
        m_sps.vui_fields.bits.log2_max_mv_length_horizontal = 13;
        m_sps.vui_fields.bits.log2_max_mv_length_vertical = 9;
        m_sps.aspect_ratio_idc = 1;
        m_sps.sar_width = 1;
        m_sps.sar_height = 1;
        m_sps.num_units_in_tick = 1;
        m_sps.time_scale = 60;

        //assert(slice_type == SLICE_TYPE_I);
        length_in_bits = build_packed_seq_buffer(&packed_seq_buffer, &m_sps,
                                        ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile));
        offset_in_bytes = 0;
        packed_header_param_buffer.type = VAEncPackedHeaderSequence;
        packed_header_param_buffer.bit_length = length_in_bits;
        packed_header_param_buffer.has_emulation_bytes = 1;//0;
        vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderParameterBufferType,
                                   sizeof(packed_header_param_buffer), 1, &packed_header_param_buffer,
                                   &m_packedSpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_packedSpsHeaderBufferId=%d\n", m_packedSpsHeaderBufferId);
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncPackedHeaderDataBufferType,
                                   (length_in_bits + 7) / 8, 1, packed_seq_buffer,
                                   &m_packedSpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_packedSpsBufferId=%d\n", m_packedSpsBufferId);
        configBuffers[buffersCount++] = m_packedSpsBufferId;

        free(packed_seq_buffer);
    }

    /* sequence parameter set */
    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                               VAEncSequenceParameterBufferType,
                               sizeof(m_sps),1,
                               &m_sps,
                               &m_spsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
    configBuffers[buffersCount++] = m_spsBufferId;

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    //buffer for MV output
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    mfxPAKInput* in = (mfxPAKInput*)task.m_userData[0];
    mfxPAKOutput* out = (mfxPAKOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    VABufferID vaFeiMCODEOutId = VA_INVALID_ID;

    //find ext buffers
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiEncMBCtrl* mbctrl = NULL;
    mfxExtFeiEncMVPredictors* mvpred = NULL;
    mfxExtFeiEncFrameCtrl* frameCtrl = NULL;
    mfxExtFeiEncQP* mbqp = NULL;
    mfxExtFeiEncMBStat* mbstat = NULL;
    mfxExtFeiEncMV* mvout = NULL;
    mfxExtFeiPakMBCtrl* mbcodeout = NULL;


//    /* in buffers */
    for (int i = 0; i < out->NumExtParam; i++)
    {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL)
        {
            frameCtrl = &((mfxExtFeiEncFrameCtrl*) (out->ExtParam[i]))[fieldId];
        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED)
//        {
//            mvpred = &((mfxExtFeiEncMVPredictors*) (in->ExtParam[i]))[fieldId];
//        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB)
//        {
//            mbctrl = &((mfxExtFeiEncMBCtrl*) (in->ExtParam[i]))[fieldId];
//        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
//        {
//            mbqp = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
//        }
    }

    for (int i = 0; i < in->NumExtParam; i++)
    {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
        {
            mvout = &((mfxExtFeiEncMV*) (in->ExtParam[i]))[fieldId];
        }
        /* Does not need statistics for PAK */
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
//        {
//            mbstat = &((mfxExtFeiEncMBStat*) (in->ExtParam[i]))[fieldId];
//        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
        {
            mbcodeout = &((mfxExtFeiPakMBCtrl*) (in->ExtParam[i]))[fieldId];
        }
    }

//    if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
//                sizeof(VAEncMVPredictorH264Intel)*numMB,  //limitation from driver, num elements should be 1
//                1,
//                mvpred->MB,
//                &vaFeiMVPredId );
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMVPredId;
//        mdprintf(stderr, "vaFeiMVPredId=%d\n", vaFeiMVPredId);
//    }
//
//    if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType)VAEncFEIMBControlBufferTypeIntel,
//                sizeof (VAEncFEIMBControlH264Intel)*numMB,  //limitation from driver, num elements should be 1
//                1,
//                mbctrl->MB,
//                &vaFeiMBControlId);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMBControlId;
//        mdprintf(stderr, "vaFeiMBControlId=%d\n", vaFeiMBControlId);
//    }
//
//    if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType)VAEncQpBufferType,
//                sizeof (VAEncQpBufferH264)*numMB, //limitation from driver, num elements should be 1
//                1,
//                mbqp->QP,
//                &vaFeiMBQPId);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMBQPId;
//        mdprintf(stderr, "vaFeiMBQPId=%d\n", vaFeiMBQPId);
//    }

    //output buffer for MB distortions
    if (mbstat != NULL)
    {
        if (m_vaFeiMBStatId.size() == 0 )
        {
            //m_vaFeiMBStatId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                        sizeof (VAEncFEIDistortionBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        &vaFeiMBStatId);
                        //&m_vaFeiMBStatId[ii]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMBStatId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMBStatId;
        //outBuffers[numOutBufs++] = m_vaFeiMBStatId[idxRecon];
        mdprintf(stderr, "MB Stat bufId=%d\n", vaFeiMBStatId);
        configBuffers[buffersCount++] = vaFeiMBStatId;
        //configBuffers[buffersCount++] = m_vaFeiMBStatId[idxRecon];
    }

    //output buffer for MV
    if (mvout != NULL)
    {
        //if (m_vaFeiMVOutId.size() == 0 )
        {
            //m_vaFeiMVOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIMVBufferTypeIntel,
                        sizeof (VAMotionVectorIntel)*16*numMB, //limitation from driver, num elements should be 1
                        1,
                        mvout->MB, //should be mapped later
                        //&m_vaFeiMVOutId[ii]);
                        &vaFeiMVOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMVOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMVOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMVOutId[idxRecon];
        mdprintf(stderr, "MV Out bufId=%d\n", vaFeiMVOutId);
        //configBuffers[buffersCount++] = vaFeiMVOutId;
        //configBuffers[buffersCount++] = m_vaFeiMVOutId[idxRecon];
    }

    //output buffer for MBCODE (Pak object cmds)
    if (mbcodeout != NULL)
    {
        //if (m_vaFeiMCODEOutId.size() == 0 )
        {
            //m_vaFeiMCODEOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIModeBufferTypeIntel,
                        sizeof (VAEncFEIModeBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        mbcodeout->MB, //should be mapped later
                        //&m_vaFeiMCODEOutId[ii]);
                        &vaFeiMCODEOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } //if (m_vaFeiMCODEOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMCODEOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMCODEOutId[idxRecon];
        mdprintf(stderr, "MCODE Out bufId=%d\n", vaFeiMCODEOutId);
        //configBuffers[buffersCount++] = vaFeiMCODEOutId;
        //configBuffers[buffersCount++] = m_vaFeiMCODEOutId[idxRecon];
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        //vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_INTEL;
        vaFeiFrameControl->function = VA_ENC_FUNCTION_PAK_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->max_len_sp = frameCtrl->MaxLenSP;

        vaFeiFrameControl->distortion = VA_INVALID_ID;//vaFeiMBStatId;
        vaFeiFrameControl->mv_data = vaFeiMVOutId;
        vaFeiFrameControl->mb_code_data = vaFeiMCODEOutId;
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
        vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
        vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
        vaFeiFrameControl->num_mv_predictors = frameCtrl->NumMVPredictors;
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);
    }
//    mfxU32 idxRecon = task.m_idxBs[fieldId];

    int ref_counter_l0 = 0;
    int ref_counter_l1 = 0;
    /* to fill L0 List*/
    if (in->NumFrameL0)
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            m_pps.ReferenceFrames[ref_counter_l0].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n ",
                    ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
        }
    }
    /* to fill L1 List*/
    if (in->NumFrameL1)
    {
        /* continue to fill ref list */
        for (ref_counter_l1 = 0 ; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle;
            m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n", ref_counter_l0 + ref_counter_l1,
                    m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id);
        }
    }

    int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;
    mdprintf(stderr,"slice_type=%d\n",slice_type);
    /* slice parameters */
    {
        int i = 0, j = 0;

        mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        if (task.m_numSlice[fieldId])
            m_slice.resize(task.m_numSlice[fieldId]);
        mfxU32 numSlice = m_slice.size();
        mfxU32 idx = 0, ref = 0;

        SliceDivider divider = MakeSliceDivider(
            m_caps.SliceStructure,
            task.m_numMbPerSlice,
            numSlice,
            m_sps.picture_width_in_mbs,
            m_sps.picture_height_in_mbs / numPics);

        for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )
        {
            m_slice[i].macroblock_address = divider.GetFirstMbInSlice();
            m_slice[i].num_macroblocks = divider.GetNumMbInSlice();
            m_slice[i].macroblock_info = VA_INVALID_ID;
            //m_slice[i].pic_parameter_set_id = 0;
            m_slice[i].slice_type = slice_type;
            m_slice[i].pic_parameter_set_id = m_pps.pic_parameter_set_id;
            /* Debug FIXME*/
            m_slice[i].idr_pic_id = 0;
            m_slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));
            mdprintf(stderr,"m_slice[i].pic_order_cnt_lsb=%d\n",m_slice[i].pic_order_cnt_lsb);
            //m_slice[i].pic_order_cnt_lsb = m_currentFrameNum * 2;
            //task.m_frameNum*
            m_slice[i].direct_spatial_mv_pred_flag = 1;  // to match header NALU

            m_slice[i].num_ref_idx_l0_active_minus1 = 0;
            m_slice[i].num_ref_idx_l1_active_minus1 = 0;

            if ( slice_type == SLICE_TYPE_I ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = -1;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_P ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_B ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = in->NumFrameL1;
            }

            m_slice[i].cabac_init_idc = 0;
            m_slice[i].slice_qp_delta = 0;
            m_slice[i].disable_deblocking_filter_idc = 0;
            m_slice[i].slice_alpha_c0_offset_div2 = 2;
            m_slice[i].slice_beta_offset_div2 = 2;
            m_slice[i].idr_pic_id = 0;
            /*    */
            for (j=0; j<32; j++)
            {
                m_slice[i].RefPicList0[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[j].flags = VA_PICTURE_H264_INVALID;
                m_slice[i].RefPicList1[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[j].flags = VA_PICTURE_H264_INVALID;
            }

            if ((in->NumFrameL0) && ((slice_type == SLICE_TYPE_P) || (slice_type == SLICE_TYPE_P)) )
            {
                for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList0[ref_counter_l0].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
            if ( (in->NumFrameL1) && (slice_type == SLICE_TYPE_B) )
            {
                for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
        } // for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )

        for( size_t i = 0; i < m_slice.size(); i++ )
        {
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            //configBuffers[buffersCount++] = m_sliceBufferId[i];
            mdprintf(stderr, "m_sliceBufferId[%d]=%d\n", i, m_sliceBufferId[i]);
        }
    } /* slice parameters */

    //static void avcenc_update_picture_parameter(int slice_type, int frame_num, int display_num, int is_idr)
    {
        int i;
        // Picture level
        unsigned int length_in_bits, offset_in_bytes;
        VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
        unsigned char *packed_pic_buffer = NULL;
        mfxHDL recon_handle;
        mfxSts = m_core->GetExternalFrameHDL(out->OutSurface->Data.MemId, &recon_handle);
        m_pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;

        m_pps.CurrPic.picture_id = *(VASurfaceID*) recon_handle; //id in the memory by ptr
        m_pps.CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        mdprintf(stderr,"m_pps.CurrPic.picture_id = %d\n",m_pps.CurrPic.picture_id);
        /* PPS update */
        m_pps.pic_init_qp = 26;
        m_pps.pic_fields.bits.entropy_coding_mode_flag = 1; //ENTROPY_MODE_CABAC;
        m_pps.pic_fields.bits.transform_8x8_mode_flag = 1;
        m_pps.pic_fields.bits.deblocking_filter_control_present_flag = 1;
        /*FIXME*/
        m_pps.pic_fields.bits.weighted_bipred_idc = 2;

        m_pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
        m_pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);

        /* Need to allocated coded buffer
         */
        if (VA_INVALID_ID == m_codedBufferId)
        {
            int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
            int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
            int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncCodedBufferType,
                                    codedbuf_size,
                                    1,
                                    NULL,
                                    &m_codedBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            //configBuffers[buffersCount++] = m_codedBufferId;
            mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId);
        }
        m_pps.coded_buf = m_codedBufferId;
        m_pps.frame_num = m_currentFrameNum;
        //m_pps.frame_num++;
        //m_pps.pic_fields.bits.idr_pic_flag =  (task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
        //mdprintf(stderr, "m_pps->pic_fields.bits.idr_pic_flag=%d\n", m_pps.pic_fields.bits.idr_pic_flag);
        m_pps.pic_fields.bits.reference_pic_flag = (slice_type != SLICE_TYPE_B);

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPictureParameterBufferType,
                                sizeof(m_pps),
                                1,
                                &m_pps,
                                &m_ppsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_ppsBufferId;
        mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

        if (SLICE_TYPE_B != slice_type)
            m_currentFrameNum++;

        length_in_bits = build_packed_pic_buffer(&packed_pic_buffer, &m_pps,
                                    ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile));
            offset_in_bytes = 0;
            packed_header_param_buffer.type = VAEncPackedHeaderPicture;
            packed_header_param_buffer.bit_length = length_in_bits;
            packed_header_param_buffer.has_emulation_bytes = 1;//0;

            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                       VAEncPackedHeaderParameterBufferType,
                                       sizeof(packed_header_param_buffer),
                                       1,
                                       &packed_header_param_buffer,
                                       &m_packedPpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
            mdprintf(stderr, "m_packedPpsHeaderBufferId=%d\n", m_packedPpsHeaderBufferId);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                       VAEncPackedHeaderDataBufferType,
                                       (length_in_bits + 7) / 8, 1,
                                       packed_pic_buffer,
                                       &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = m_packedPpsBufferId;
            mdprintf(stderr, "m_packedPpsBufferId=%d\n", m_packedPpsBufferId);
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------

    mfxHDL handle_1;
    mfxSts = m_core->GetExternalFrameHDL(in->InSurface->Data.MemId, &handle_1);
    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
    inputSurface = (VASurfaceID*) handle_1; //id in the memory by ptr

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
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
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = vaFeiMVOutId;
        //currentFeedback.mbstat    = vaFeiMBStatId;
        currentFeedback.mbcode    = vaFeiMCODEOutId;
        m_statFeedbackCache.push_back(currentFeedback);
        //m_feedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIFEIPAKEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            /* first to destroy used MBCode and MV buffers */
            MFX_DESTROY_VABUFFER(vaFeiMBCODEOutId, m_vaDisplay);
            MFX_DESTROY_VABUFFER(vaFeiMVOutId, m_vaDisplay);

            VACodedBufferSegment *codedBufferSegment;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                vaSts = vaMapBuffer(
                    m_vaDisplay,
                    m_codedBufferId,
                    (void **)(&codedBufferSegment));
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            task.m_bsDataLength[fieldId] = codedBufferSegment->size;
            memcpy_s(task.m_bs->Data + task.m_bs->DataOffset, codedBufferSegment->size,
                                     codedBufferSegment->buf, codedBufferSegment->size);
            task.m_bs->DataLength = codedBufferSegment->size;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                vaUnmapBuffer( m_vaDisplay, m_codedBufferId );
            }


            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);

            //VABufferID tempID;
            int next_is_bpic = 0;
            int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;

            /* Prepare for next picture */
            //tempID = m_reconQueue[SID_RECON_PICTURE].surface;
            if ((task.m_frameOrder - m_lastRefFrameOrder) >1)
                next_is_bpic = 1;
            if (task.m_frameOrder > m_lastRefFrameOrder)
                m_lastRefFrameOrder = task.m_frameOrder;


//            if (slice_type != SLICE_TYPE_B) {
//                if (next_is_bpic) {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L1].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L1].surface = tempID;
//                    ref1_POC[0] = Cur_POC[0];
//                    ref1_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                } else {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L0].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L0].surface = tempID;
//                    ref0_POC[0] = Cur_POC[0];
//                    ref0_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                }
//            } else {
//                if (!next_is_bpic) {
//                    m_reconQueue[SID_RECON_PICTURE].surface = m_reconQueue[SID_REFERENCE_PICTURE_L0].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L0].surface = m_reconQueue[SID_REFERENCE_PICTURE_L1].surface;
//                    m_reconQueue[SID_REFERENCE_PICTURE_L1].surface = tempID;
//                    ref0_POC[0] = ref1_POC[0];
//                    ref0_POC[1] = ref1_POC[1];
//                    ref1_POC[0] = Cur_POC[0];
//                    ref1_POC[1] = Cur_POC[1];
//                    //next_input_id = GetNextInput(next_input_id );
//                }
//            }

        }
            return MFX_ERR_NONE;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)

#endif //defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
/* EOF */

