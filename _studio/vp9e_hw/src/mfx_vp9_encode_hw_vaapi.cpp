//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#if !defined (_WIN32) && !defined (_WIN64)

#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_vaapi.h"

namespace MfxHwVP9Encode
{
#if defined(MFX_VA_LINUX)

 /* ----------- Functions to convert MediaSDK into DDI --------------------- */

    DriverEncoder* CreatePlatformVp9Encoder()
    {
        return new VAAPIEncoder;
    }

    void FillSpsBuffer(mfxVideoParam const & par, VAEncSequenceParameterBufferVP9 & sps)
    {
        Zero(sps);

        sps.max_frame_width  = par.mfx.FrameInfo.CropW!=0 ? par.mfx.FrameInfo.CropW :  par.mfx.FrameInfo.Width;
        sps.max_frame_height = par.mfx.FrameInfo.CropH!=0 ? par.mfx.FrameInfo.CropH :  par.mfx.FrameInfo.Height;

        sps.kf_auto = 0;
        sps.kf_min_dist = 1;
        sps.kf_max_dist = par.mfx.GopRefDist;
        sps.bits_per_second = par.mfx.TargetKbps*1000;    //
        sps.intra_period  = par.mfx.GopPicSize;
    }

    mfxStatus FillPpsBuffer(
        Task const & task,
        mfxVideoParam const & par,
        VAEncPictureParameterBufferVP9 & pps,
        std::vector<ExtVASurface> const & reconQueue)
    {
        mfxExtCodingOptionVP9 * opts = GetExtBuffer(par);
        MFX_CHECK_NULL_PTR1(opts);

        Zero(pps);

        pps.frame_width_dst = pps.frame_width_src  = par.mfx.FrameInfo.CropW!=0 ? par.mfx.FrameInfo.CropW :  par.mfx.FrameInfo.Width;
        pps.frame_width_dst = pps.frame_height_src = par.mfx.FrameInfo.CropH!=0 ? par.mfx.FrameInfo.CropH :  par.mfx.FrameInfo.Height;

        MFX_CHECK(task.m_pRecFrame->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
        pps.reconstructed_frame = reconQueue[task.m_pRecFrame->idInPool].surface;

        pps.ref_flags.bits.force_kf = 0;
        {
            mfxU16 ridx = 0;

            while (ridx < 8)
                pps.reference_frames[ridx++] = 0xFF;

            ridx = 0;

            if (task.m_pRecRefFrames[REF_LAST] && task.m_pRecRefFrames[REF_LAST]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_LAST]->idInPool].surface;
                pps.ref_flags.bits.ref_last_idx = ridx;
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x01;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x01;
                ridx ++;
            }

            if (task.m_pRecRefFrames[REF_GOLD] && task.m_pRecRefFrames[REF_GOLD]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_GOLD]->idInPool].surface;
                pps.ref_flags.bits.ref_gf_idx = ridx;
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x02;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x02;
                ridx ++;
            }

            if (task.m_pRecRefFrames[REF_ALT] && task.m_pRecRefFrames[REF_ALT]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_ALT]->idInPool].surface;
                pps.ref_flags.bits.ref_arf_idx = ridx;
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x04;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x04;
                ridx ++;
            }
        }

        pps.pic_flags.bits.frame_type           = (task.m_sFrameParams.bIntra) ? 0 : 1;
        pps.pic_flags.bits.show_frame           = 1;
        pps.pic_flags.bits.error_resilient_mode = 0;
        pps.pic_flags.bits.intra_only           = 0;
        pps.pic_flags.bits.segmentation_enabled = (opts->EnableMultipleSegments == MFX_CODINGOPTION_ON);

        pps.luma_ac_qindex         = task.m_sFrameParams.QIndex;
        pps.luma_dc_qindex_delta   = (mfxI8)task.m_sFrameParams.QIndexDeltaLumaDC;
        pps.chroma_ac_qindex_delta = (mfxI8)task.m_sFrameParams.QIndexDeltaChromaAC;
        pps.chroma_dc_qindex_delta = (mfxI8)task.m_sFrameParams.QIndexDeltaChromaDC;

        pps.filter_level = task.m_sFrameParams.LFLevel;
        pps.sharpness_level = task.m_sFrameParams.Sharpness;

        for (mfxU16 i = 0; i < 4; i ++)
            pps.ref_lf_delta[i] = (mfxI8)task.m_sFrameParams.LFRefDelta[i];

        for (mfxU16 i = 0; i < 2; i ++)
            pps.mode_lf_delta[i] = (mfxI8)task.m_sFrameParams.LFRefDelta[i];

        pps.bit_offset_ref_lf_delta         = 0;
        pps.bit_offset_mode_lf_delta        = 4 * 4 + pps.bit_offset_mode_lf_delta;
        pps.bit_offset_lf_level             = 4 * 2 + pps.bit_offset_lf_level;
        pps.bit_offset_qindex               = 4 * 1 + pps.bit_offset_qindex;
        pps.bit_offset_first_partition_size = 4 * 1 + pps.bit_offset_first_partition_size;
        pps.bit_offset_segmentation         = 4 * 1 + pps.bit_offset_segmentation;

        pps.log2_tile_columns   = opts->Log2TileColumns;
        pps.log2_tile_rows      = opts->Log2TileRows;

        memset(pps.mb_segment_tree_probs, 128, sizeof(pps.mb_segment_tree_probs));
        memset(pps.segment_pred_probs, 128, sizeof(pps.segment_pred_probs));

        return MFX_ERR_NONE;
    }

#if 0 // segmentation support is disabled
    mfxStatus FillSegMap(
        Task const & task,
        mfxVideoParam const & par,
        mfxCoreInterface *    pCore,
        VAEncMiscParameterVP9SegmentMapParams & segPar)
    {
        if (task.ddi_frames.m_pSegMap_hw == 0)
            return MFX_ERR_NONE; // segment map isn't required
        mfxFrameData segMap = { 0 };
        FrameLocker lockSegMap(pCore, segMap, task.ddi_frames.m_pSegMap_hw->pSurface->Data.MemId);
        mfxU8 *pBuf = task.ddi_frames.m_pSegMap_hw->pSurface->Data.Y;
        if (pBuf == 0)
            return MFX_ERR_MEMORY_ALLOC;

        // fill segmentation map from ROI
        mfxExtEncoderROI *pExtRoi = GetExtBuffer(par);
        mfxExtCodingOptionVP9 *pOptVP9 = GetExtBuffer(par);

        Zero(segPar);

        if (pExtRoi->NumROI)
        {
            mfxU32 w8 = par.mfx.FrameInfo.Width >> 3;
            mfxU32 h8 = par.mfx.FrameInfo.Height >> 3;
            mfxU32 segMapPitch = ALIGN(w8, 64);

            for (mfxI8 i = task.m_sFrameParams.NumSegments - 1; i >= 0; i --)
            {
                VAEncSegParamVP9Private & segva  = segPar.seg_data[i];
                mfxSegmentParamVP9 & segmfx = pOptVP9->Segment[i];

                segva.seg_flags.bits.segment_reference_enabled = (mfxU8)(segmfx.ReferenceAndSkipCtrl & 0x01);
                segva.seg_flags.bits.segment_reference         = (mfxU8)((segmfx.ReferenceAndSkipCtrl >> 1) & 3);
                segva.seg_flags.bits.segment_reference_skipped = (mfxU8)((segmfx.ReferenceAndSkipCtrl >> 4) & 1);
                segva.segment_lf_level_delta = task.m_sFrameParams.LFDeltaSeg[i];
                segva.segment_qindex_delta = task.m_sFrameParams.QIndexDeltaSeg[i];

                for (mfxU32 r = pExtRoi->ROI[i].Top >> 3; r < IPP_MIN(pExtRoi->ROI[i].Bottom >> 3, h8); r ++)
                {
                    for (mfxU32 c = pExtRoi->ROI[i].Left / 8; c < IPP_MIN(pExtRoi->ROI[i].Right / 8, w8); c ++)
                    {
                        pBuf[r * segMapPitch + c] = mfxU8(i);
                    }
                }
            }
#if 0
            printf("\n\n");
            for (mfxU32 row = 0; row < h8; row ++)
            {
                for (mfxU32 col = 0; col < w8; col ++)
                {
                    printf("%02x(%3d) ", pBuf[segMapPitch * row + col], task.m_sFrameParams.QIndexDeltaSeg[pBuf[segMapPitch * row + col]]);
                }
                printf("\n");
            }
            printf("\n\n");
            fflush(0);
#endif
        }

        return MFX_ERR_NONE;
    }
#endif // segmentation support is disabled

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

void FillBrcStructures(
    mfxVideoParam const & par,
    VAEncMiscParameterRateControl & vaBrcPar,
    VAEncMiscParameterFrameRate   & vaFrameRate)
{
    Zero(vaBrcPar);
    Zero(vaFrameRate);
    vaBrcPar.bits_per_second = par.mfx.MaxKbps * 1000;
    if(par.mfx.MaxKbps)
        vaBrcPar.target_percentage = (unsigned int)(100.0 * (mfxF64)par.mfx.TargetKbps / (mfxF64)par.mfx.MaxKbps);
    vaFrameRate.framerate = (par.mfx.FrameInfo.FrameRateExtD << 16 )| par.mfx.FrameInfo.FrameRateExtN;
}

mfxStatus SetRateControl(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & rateParamBuf_id,
    bool         isBrcResetRequired = false)
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

    if(par.mfx.MaxKbps)
        rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.mfx.TargetKbps / (mfxF64)par.mfx.MaxKbps);

    rate_param->rc_flags.bits.reset = isBrcResetRequired;

    vaUnmapBuffer(m_vaDisplay, rateParamBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetHRD(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & hrdBuf_id)
{
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

    return MFX_ERR_NONE;
}

mfxStatus SetQualityLevel(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & qualityLevelBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferQualityLevel *quality_param;

    if ( qualityLevelBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, qualityLevelBuf_id);
    }
    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferQualityLevel),
                   1,
                   NULL,
                   &qualityLevelBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 qualityLevelBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeQualityLevel;
    quality_param = (VAEncMiscParameterBufferQualityLevel *)misc_param->data;

    quality_param->quality_level = par.mfx.TargetUsage;

    vaUnmapBuffer(m_vaDisplay, qualityLevelBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetFrameRate(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & frameRateBufId)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBufId != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, frameRateBufId);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBufId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 frameRateBufId,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    frameRate_param->framerate = (par.mfx.FrameInfo.FrameRateExtD << 16 )| par.mfx.FrameInfo.FrameRateExtN;

    vaUnmapBuffer(m_vaDisplay, frameRateBufId);

    return MFX_ERR_NONE;
}

mfxStatus SendMiscBufferFrameUpdate(
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VAEncMiscParameterVP9CpuPakFrameUpdate & frameUpdateBuf,
    VABufferID & frameUpdateBufId)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterVP9CpuPakFrameUpdate *frameUpdate_param;

    if ( frameUpdateBufId != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, frameUpdateBufId);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterVP9CpuPakFrameUpdate),
                   1,
                   NULL,
                   &frameUpdateBufId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 frameUpdateBufId,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeVP9CpuPakFrameUpdate;
    frameUpdate_param = (VAEncMiscParameterVP9CpuPakFrameUpdate *)misc_param->data;
    *frameUpdate_param = frameUpdateBuf;

    vaUnmapBuffer(m_vaDisplay, frameUpdateBufId);

    return MFX_ERR_NONE;
}


VAAPIEncoder::VAAPIEncoder()
: m_pmfxCore(NULL)
, m_vaDisplay(0)
, m_vaContextEncode(0)
, m_vaConfig(0)
, m_spsBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_coeffBufBufferId(VA_INVALID_ID)
, m_segMapBufferId(VA_INVALID_ID)
, m_segParBufferId(VA_INVALID_ID)
, m_frmUpdateBufferId(VA_INVALID_ID)
, m_frameRateBufferId(VA_INVALID_ID)
, m_rateCtrlBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_probUpdateBufferId(VA_INVALID_ID)
, m_qualityLevelBufferId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    mfxCoreInterface* pCore,
    GUID /*guid*/,
    mfxU32 width,
    mfxU32 height)
{
    m_pmfxCore = pCore;

    if(pCore)
    {
        mfxStatus mfxSts = pCore->GetHandle(pCore->pthis, MFX_HANDLE_VA_DISPLAY, &m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    m_width  = width;
    m_height = height;

    // set encoder CAPS on our own for now
    memset(&m_caps, 0, sizeof(m_caps));
    m_caps.MaxPicWidth = 1920;
    m_caps.MaxPicHeight = 1080;
    m_caps.HybridPakFunc = 1;
    m_caps.SegmentationAllowed = 1;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(mfxVideoParam const & par)
{
    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces when call vaCreateContext().
     * Here we don't have this info.
     */
        m_video = par;
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
                VAProfileVP9Profile0,
                Begin(pEntrypoints),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        // [SE] VAEntrypointHybridEncSlice is entry point for Hybrid VP9 encoder
        if( VAEntrypointHybridEncSlice == pEntrypoints[entrypointsIndx] )
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

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          VAProfileVP9Profile0,
                          (VAEntrypoint)VAEntrypointHybridEncSlice,
                          &attrib[0], 2);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod); // [SE] at the moment there is no BRC for VP9 on driver side

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        VAProfileVP9Profile0,
        (VAEntrypoint)VAEntrypointHybridEncSlice,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        VA_PROGRESSIVE,
        Begin(reconSurf),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    Zero(m_sps);
    Zero(m_pps);

    //------------------------------------------------------------------

    FillSpsBuffer(par, m_sps);
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    m_isBrcResetRequired = false;
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateCtrlBufferId);
    SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateBufferId);

    hybridQueryBufferAttributes pfnVaQueryBufferAttr = NULL;
    pfnVaQueryBufferAttr = (hybridQueryBufferAttributes)vaGetLibFunc(m_vaDisplay, FUNC_QUERY_BUFFER_ATTRIBUTES);

    if (pfnVaQueryBufferAttr)
    {
        // get layout of MB data
        memset(&m_mbdataLayout, 0, sizeof(m_mbdataLayout));
        mfxU32 bufferSize = sizeof(VAEncMbDataLayout);
        vaSts = pfnVaQueryBufferAttr(
          m_vaDisplay,
          m_vaContextEncode,
          (VABufferType)VAEncMbDataBufferType,
          &m_mbdataLayout,
          &bufferSize);
        MFX_CHECK_WITH_ASSERT(bufferSize == sizeof(VAEncMbDataLayout), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        // get layout of MB coefficients
        memset(&m_mbcoeffLayout, 0, sizeof(m_mbcoeffLayout));
        /*bufferSize = sizeof(VAEncMbCoeffLayout);
        vaSts = pfnVaQueryBufferAttr(
          m_vaDisplay,
          m_vaContextEncode,
          (VABufferType)VAEncMbCoeffBufferType,
          &m_mbcoeffLayout,
          &bufferSize);
        MFX_CHECK_WITH_ASSERT(bufferSize == sizeof(VAEncMbCoeffLayout), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);*/
    }
    else
      return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(mfxVideoParam const & par)
{
    m_video = par;

    FillSpsBuffer(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    m_isBrcResetRequired = !Equal(m_vaBrcPar, oldBrcPar) || !Equal(m_vaFrameRate, oldFrameRate);

    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateBufferId);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)

mfxU32 VAAPIEncoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_VP9_NV12;
} // mfxU32 VAAPIEncoder::GetReconSurfFourCC()

mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    if (type == D3DDDIFMT_INTELENCODE_MBDATA)
    {
        request.Info.FourCC = MFX_FOURCC_P8;

        mfxU32 widthBlocks8x8  = (frameWidth + 7)>>3;
        mfxU32 heightBlocks8x8 = (frameHeight + 7)>>3;
        mfxU64 frameSizeBlocks8x8 = widthBlocks8x8 * heightBlocks8x8;
        request.Info.Width = m_mbdataLayout.MbCodeStride;
        request.Info.Height = frameSizeBlocks8x8 + 1;
    } else if (type == D3DDDIFMT_INTELENCODE_MBCOEFF)
    {
        request.Info.FourCC = MFX_FOURCC_VP9_MBCOEFF; // vaCreateSurface is required to allocate surface for MB coefficients
        request.Info.Width = ALIGN(frameWidth * 2, 64);
        request.Info.Height = frameHeight * 2;
    }
#if 0 // segmentation support is disabled
    else if (type == D3DDDIFMT_INTELENCODE_SEGMENTMAP)
    {
        request.Info.FourCC = MFX_FOURCC_VP9_SEGMAP;
        request.Info.Width  = ALIGN(frameWidth >> 4, 64);
        request.Info.Height = frameHeight >> 4;
    }
#endif // segmentation support is disabled

    // context_id required for allocation video memory (tmp solution)
    request.reserved[0] = m_vaContextEncode;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if( D3DDDIFMT_INTELENCODE_MBDATA == type
    )
    {
        pQueue = &m_mbDataQueue;
    }
    else if (D3DDDIFMT_INTELENCODE_SEGMENTMAP == type)
    {
        pQueue = &m_segMapQueue;
    }
    else if (D3DDDIFMT_INTELENCODE_MBCOEFF == type)
    {
        pQueue = &m_mbCoeffQueue;
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
            sts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);

            extSurf.surface = *pSurface;

            pQueue->push_back( extSurf );
        }
    }

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type &&
        D3DDDIFMT_INTELENCODE_MBDATA != type &&
        D3DDDIFMT_INTELENCODE_SEGMENTMAP != type &&
        D3DDDIFMT_INTELENCODE_MBCOEFF != type)
    {
        sts = CreateAccelerationService(m_video);
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

mfxStatus VAAPIEncoder::Execute(
    Task const & task,
    mfxHDL          surface)
{
    VAStatus vaSts;
    VP9_LOG("\n (sefremov) VAAPIEncoder::Execute +");

    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VASurfaceID reconSurface;
    VABufferID mbdataBuffer;
    VABufferID mbcoeffBuffer;
    mfxU32 i;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT);
    mfxU16 buffersCount = 0;

    // update params
    FillPpsBuffer(task, m_video, m_pps, m_reconQueue);
    FillSegMap(task, m_video, m_pmfxCore, m_segPar);
    m_probUpdate = task.m_probs;

//===============================================================================================

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxInPool = task.m_pRecFrame->idInPool;
    if( idxInPool < m_mbDataQueue.size() &&
        m_mbDataQueue.size() == m_mbCoeffQueue.size())
    {
        mbdataBuffer = m_mbDataQueue[idxInPool].surface;
        mbcoeffBuffer = m_mbCoeffQueue[idxInPool].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    m_pps.coded_buf = mbdataBuffer;

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

        // 3. Output buffer for coefficients
        {
            m_coeffBuf.coeff_buf = mbcoeffBuffer;
            MFX_DESTROY_VABUFFER(m_coeffBufBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   (VABufferType)VAEncMbCoeffBufferType,
                                   sizeof(m_coeffBuf),
                                   1,
                                   &m_coeffBuf,
                                   &m_coeffBufBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_coeffBufBufferId;
        }

#if 0 // segmentation support is disabled
        if (task.ddi_frames.m_pSegMap_hw != 0)
        {
#if 0 // waiting for support in driver
            // 4. Segmentation map

            // segmentation map buffer is already allocated and filled. Need just to attach it
            configBuffers[buffersCount++] = m_segMapBufferId;

            // 5. Per-segment parameters
            MFX_DESTROY_VABUFFER(m_segParBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAQMatrixBufferType,
                                   sizeof(m_segPar),
                                   1,
                                   &m_segPar,
                                   &m_segParBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_segParBufferId;
#endif
        }
#endif // segmentation support is disabled

        // 6. Frame update
        if (task.m_frameOrder)
        {
            vaSts = SendMiscBufferFrameUpdate(m_vaDisplay,
                                              m_vaContextEncode,
                                              m_frmUpdate,
                                              m_frmUpdateBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            configBuffers[buffersCount++] = m_frmUpdateBufferId;
        }

        // 7. Probability update
        if (task.m_frameOrder)
        {
            MFX_DESTROY_VABUFFER(m_probUpdateBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAProbabilityBufferType,
                                   sizeof(m_probUpdate),
                                   1,
                                   &m_probUpdate,
                                   &m_probUpdateBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_probUpdateBufferId;
        }

        // 8. hrd parameters
        configBuffers[buffersCount++] = m_hrdBufferId;
        // 9. RC parameters
        SetRateControl(m_video, m_vaDisplay, m_vaContextEncode, m_rateCtrlBufferId, m_isBrcResetRequired);
        m_isBrcResetRequired = false; // reset BRC only once
        configBuffers[buffersCount++] = m_rateCtrlBufferId;
        // 10. frame rate
        configBuffers[buffersCount++] = m_frameRateBufferId;
        // 11. quality level
#if 0 // waiting for support in driver
        configBuffers[buffersCount++] = m_qualityLevelBufferId;
#endif
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        vaSts = vaBeginPicture(
            m_vaDisplay,
            m_vaContextEncode,
            *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        vaSts = vaRenderPicture(
            m_vaDisplay,
            m_vaContextEncode,
            Begin(configBuffers),
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "vaEndPicture");
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
        currentFeedback.surface = *inputSurface;
        currentFeedback.number = task.m_frameOrder;
        currentFeedback.idxBs    = idxInPool;
        m_feedbackCache.push_back( currentFeedback );
    }

    VP9_LOG("\n (sefremov) VAAPIEncoder::Execute -");
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    Task & task)
{
    VAStatus vaSts;
    VP9_LOG("\n (sefremov) VAAPIEncoder::QueryStatus +");

    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & mb data buffer
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
        {
            ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

            if( currentFeedback.number == task.m_frameOrder)
            {
                waitSurface = currentFeedback.surface;
                waitIdxBs   = currentFeedback.idxBs;

                isFound  = true;

                break;
            }
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    if( waitIdxBs >= m_mbDataQueue.size())
    {
        return MFX_ERR_UNKNOWN;
    }
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );
    }

    VP9_LOG("\n (sefremov) VAAPIEncoder::QueryStatus -");
    return MFX_ERR_NONE;
#else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    switch (surfSts)
    {
        case VASurfaceReady:
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
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_coeffBufBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_segParBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frmUpdateBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frameRateBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rateCtrlBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_hrdBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_probUpdateBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_qualityLevelBufferId, m_vaDisplay);

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

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Destroy()
#endif // (MFX_VA_LINUX)

} // MfxHwVP9Encode

#endif // !(_WIN32) && !(_WIN64)
