//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
//

#include <mutex>
#include "mfx_vp9_encode_hw_d3d9.h"
#include "mfx_vp9_encode_hw_par.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

#if defined (_WIN32) || defined (_WIN64)

namespace MfxHwVP9Encode
{

#if defined (MFX_VA_WIN)

mfxU16 MapBitDepthToDDI(mfxU16 depth)
{
    switch (depth)
    {
    case BITDEPTH_8:
        return 0;
    case BITDEPTH_10:
        return 1;
    case BITDEPTH_12:
        return 2;
    default:
        return 0;
    }
}

mfxU16 MapChromaFormatToDDI(mfxU16 format)
{
    switch (format)
    {
    case MFX_CHROMAFORMAT_YUV420:
        return 0;
    case MFX_CHROMAFORMAT_YUV422:
        return 1;
    case MFX_CHROMAFORMAT_YUV444:
        return 2;
    default:
        return 0;
    }
}

mfxU8 MapRateControlMethodToDDI(mfxU16 brcMethod)
{
    switch (brcMethod)
    {
    case MFX_RATECONTROL_CBR:
        return 1;
    case MFX_RATECONTROL_VBR:
        return 2;
    case MFX_RATECONTROL_CQP:
        return 3;
    case MFX_RATECONTROL_ICQ:
        return 15;
    default:
        return 0;
    }
}

void FillSpsBuffer(
    VP9MfxVideoParam const & par,
    ENCODE_CAPS_VP9 const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 & sps,
    Task const & task,
    mfxU16 maxWidth,
    mfxU16 maxHeight)
{
    Zero(sps);

    sps.wMaxFrameWidth    = maxWidth;
    sps.wMaxFrameHeight   = maxHeight;
    sps.GopPicSize        = par.mfx.GopPicSize;
    sps.TargetUsage       = (UCHAR)par.mfx.TargetUsage;
    sps.RateControlMethod = MapRateControlMethodToDDI(par.mfx.RateControlMethod);
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    mfxExtCodingOption3 opt3 = GetExtBufferRef(par);
    sps.SeqFlags.fields.SourceBitDepth = MapBitDepthToDDI(par.mfx.FrameInfo.BitDepthLuma);
    sps.SeqFlags.fields.EncodedBitDepth = MapBitDepthToDDI(opt3.TargetBitDepthLuma);
    sps.SeqFlags.fields.SourceFormat = MapChromaFormatToDDI(par.mfx.FrameInfo.ChromaFormat);
    sps.SeqFlags.fields.EncodedFormat = MapChromaFormatToDDI(opt3.TargetChromaFormatPlus1 - 1);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    sps.SeqFlags.fields.bResetBRC = task.m_resetBrc;
    sps.SeqFlags.fields.MBBRC = 2; // 2 is for MBBRC DISABLED

    sps.SeqFlags.fields.EnableDynamicScaling = 1;

    if (IsBitrateBasedBRC(par.mfx.RateControlMethod))
    {
        mfxExtCodingOption2 opt2 = GetExtBufferRef(par);
        if (par.m_numLayers)
        {
            mfxExtVP9TemporalLayers const & tl = GetExtBufferRef(par);
            for (mfxU16 i = 0; i < par.m_numLayers; i++)
            {
                assert(tl.Layer[i].FrameRateScale);
                assert(tl.Layer[i].TargetKbps);
                sps.TargetBitRate[i] = tl.Layer[i].TargetKbps;
            }
        }
        else
        {
            sps.TargetBitRate[0] = par.m_targetKbps;
        }

        sps.MaxBitRate = par.m_maxKbps;

        if (IsOn(opt2.MBBRC))
        {
            sps.SeqFlags.fields.MBBRC = 1;
        }
        if (IsBufferBasedBRC(par.mfx.RateControlMethod))
        {
            sps.VBVBufferSizeInBit = par.m_bufferSizeInKb * 8000; // TODO: understand how to use for temporal scalability
            sps.InitVBVBufferFullnessInBit = par.m_initialDelayInKb * 8000; // TODO: understand how to use for temporal scalability
        }
    }
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
    {
        sps.ICQQualityFactor = (mfxU8)par.mfx.ICQQuality; // it's guaranteed that ICQQuality is within range [0, 255]
    }

    mfxU32 nom = par.mfx.FrameInfo.FrameRateExtN;
    mfxU32 denom = par.mfx.FrameInfo.FrameRateExtD;

    if (par.m_numLayers > 1)
    {
        mfxExtVP9TemporalLayers const & tl = GetExtBufferRef(par);
        assert(tl.Layer[0].FrameRateScale == 1);
        assert(tl.Layer[1].FrameRateScale);
        sps.FrameRate[par.m_numLayers - 1].Numerator = nom;
        sps.FrameRate[par.m_numLayers - 1].Denominator = denom;

        for (mfxI16 i = par.m_numLayers - 2; i >= 0; i--)
        {
            mfxU16 l2lRatio = tl.Layer[i + 1].FrameRateScale / tl.Layer[i].FrameRateScale;
            assert(l2lRatio);
            if (nom % l2lRatio == 0)
            {
                nom /= l2lRatio;
            }
            else
            {
                denom *= l2lRatio;
            }

            sps.FrameRate[i].Numerator = nom;
            sps.FrameRate[i].Denominator = denom;
        }
    }
    else
    {
        sps.FrameRate[0].Numerator = nom;
        sps.FrameRate[0].Denominator = denom;
    }

    sps.NumTemporalLayersMinus1 = static_cast<mfxU8>(par.m_numLayers ? par.m_numLayers - 1 : 0);
}

mfxU32 MapSegIdBlockSizeToDDI(mfxU16 size)
{
    switch (size)
    {
    case MFX_VP9_SEGMENT_ID_BLOCK_SIZE_8x8:
        return BLOCK_8x8;
    case MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32:
        return BLOCK_32x32;
    case MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64:
        return BLOCK_64x64;
    case MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16:
    default:
        return BLOCK_16x16;
    }
}

mfxU16 MapSegmentRefControlToDDI(mfxU16 refAndSkipCtrl)
{
    mfxU16 refControl = refAndSkipCtrl & segmentRefMask;
    switch (refControl)
    {
    case MFX_VP9_REF_LAST:
        return 1;
    case MFX_VP9_REF_GOLDEN:
        return 2;
    case MFX_VP9_REF_ALTREF:
        return 3;
    case MFX_VP9_REF_INTRA:
    default:
        return 0;
    }
}

void FillPpsBuffer(
    VP9MfxVideoParam const & par,
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_VP9 & pps,
    BitOffsets const &offsets)
{
    Zero(pps);

    VP9FrameLevelParam const &framePar = task.m_frameParam;

    //pps.MinLumaACQIndex = 0;
    //pps.MaxLumaACQIndex = 127;

    // per-frame part of pps structure
    pps.SrcFrameWidthMinus1 = static_cast<mfxU16>(task.m_frameParam.width) - 1;
    pps.SrcFrameHeightMinus1 = static_cast<mfxU16>(task.m_frameParam.height) - 1;
    pps.DstFrameWidthMinus1 = pps.SrcFrameWidthMinus1;
    pps.DstFrameHeightMinus1 = pps.SrcFrameHeightMinus1;

    // for VP9 encoder driver uses CurrOriginalPic to get input frame from raw surfaces chain. It's incorrect behavior. Workaround on MSDK level is to set CurrOriginalPic = 0.
    // this WA works for synchronous encoding only. For async encoding fix in driver is required.
    pps.CurrOriginalPic      = 0;
    pps.CurrReconstructedPic = (UCHAR)task.m_pRecFrame->idInPool;

    mfxU16 refIdx = 0;

    while (refIdx < 8)
    {
        pps.RefFrameList[refIdx++] = 0xff;
    }

    refIdx = 0;

    pps.RefFlags.fields.refresh_frame_flags = 0;
    for (mfxU8 i = 0; i < DPB_SIZE; i++)
    {
        pps.RefFlags.fields.refresh_frame_flags |= (framePar.refreshRefFrames[i] << i);
    }

    if (task.m_pRecRefFrames[REF_LAST])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_LAST]->idInPool;
        pps.RefFlags.fields.LastRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x01;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x01;
        refIdx ++;
    }

    if (task.m_pRecRefFrames[REF_GOLD])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_GOLD]->idInPool;
        pps.RefFlags.fields.GoldenRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x02;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x02;
        refIdx ++;
    }

    if (task.m_pRecRefFrames[REF_ALT])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_ALT]->idInPool;
        pps.RefFlags.fields.AltRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x04;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x04;
        refIdx ++;
    }

    pps.PicFlags.fields.frame_type           = framePar.frameType;
    pps.PicFlags.fields.show_frame           = framePar.showFrame;
    pps.PicFlags.fields.error_resilient_mode = framePar.errorResilentMode;
    pps.PicFlags.fields.intra_only           = framePar.intraOnly;
    pps.PicFlags.fields.refresh_frame_context = framePar.refreshFrameContext;
    pps.PicFlags.fields.frame_context_idx     = framePar.frameContextIdx;
    pps.PicFlags.fields.allow_high_precision_mv = framePar.allowHighPrecisionMV;
    if (pps.PicFlags.fields.show_frame == 0)
    {
        pps.PicFlags.fields.super_frame = 1;
    }
    pps.PicFlags.fields.allow_high_precision_mv = framePar.allowHighPrecisionMV;

    pps.PicFlags.fields.segmentation_enabled = framePar.segmentation != NO_SEGMENTATION;

    mfxExtCodingOption2 opt2 = GetExtBufferRef(par);
    if (framePar.segmentation == APP_SEGMENTATION)
    {
        // segment map is provided by application
        pps.PicFlags.fields.seg_id_block_size = BLOCK_16x16; // only 16x16 granularity for segmentation map is supported
        pps.PicFlags.fields.segmentation_update_map = task.m_frameParam.segmentationUpdateMap;
        pps.PicFlags.fields.segmentation_temporal_update = task.m_frameParam.segmentationTemporalUpdate;
        pps.PicFlags.fields.segmentation_update_data = task.m_frameParam.segmentationUpdateData;
    }

    pps.LumaACQIndex        = framePar.baseQIndex;
    pps.LumaDCQIndexDelta   = framePar.qIndexDeltaLumaDC;
    pps.ChromaACQIndexDelta = framePar.qIndexDeltaChromaAC;
    pps.ChromaDCQIndexDelta = framePar.qIndexDeltaChromaDC;

    pps.filter_level = framePar.lfLevel;
    pps.sharpness_level = framePar.sharpness;

    for (mfxU16 i = 0; i < 4; i ++)
    {
        pps.LFRefDelta[i] = framePar.lfRefDelta[i];
    }

    for (mfxU16 i = 0; i < 2; i ++)
    {
        pps.LFModeDelta[i] = framePar.lfModeDelta[i];
    }

    pps.log2_tile_columns = framePar.log2TileCols;
    pps.log2_tile_rows    = framePar.log2TileRows;

    pps.temporal_id = static_cast<mfxU8>(task.m_frameParam.temporalLayer);

    pps.StatusReportFeedbackNumber = task.m_taskIdForDriver;

    pps.BitOffsetForLFLevel = offsets.BitOffsetForLFLevel;
    pps.BitOffsetForLFModeDelta = offsets.BitOffsetForLFModeDelta;
    pps.BitOffsetForLFRefDelta = offsets.BitOffsetForLFRefDelta;
    pps.BitOffsetForQIndex = offsets.BitOffsetForQIndex;
    pps.BitOffsetForFirstPartitionSize = offsets.BitOffsetForFirstPartitionSize;
    pps.BitOffsetForSegmentation = offsets.BitOffsetForSegmentation;
    pps.BitSizeForSegmentation = offsets.BitSizeForSegmentation;
}

mfxStatus FillSegmentMap(Task const & task,
                         mfxCoreInterface* m_pmfxCore)
{
    mfxFrameData segMap = {};

    FrameLocker lock(m_pmfxCore, segMap, task.m_pSegmentMap->pSurface->Data.MemId);
    if (segMap.Y == 0)
    {
        return MFX_ERR_LOCK_MEMORY;
    }

    mfxExtVP9Segmentation const & seg = GetActualExtBufferRef(*task.m_pParam, task.m_ctrl);

    mfxFrameInfo const & dstFi = task.m_pSegmentMap->pSurface->Info;
    mfxU32 dstW = dstFi.Width;
    mfxU32 dstH = dstFi.Height;
    mfxU32 dstPitch = segMap.Pitch;

    mfxCoreParam corePar = {};
    m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &corePar);

    mfxFrameInfo const & srcFi = task.m_pRawFrame->pSurface->Info;
    mfxU16 srcBlockSize = MapIdToBlockSize(seg.SegmentIdBlockSize);
    mfxU32 srcW = (srcFi.Width + srcBlockSize - 1) / srcBlockSize;
    mfxU32 srcH = (srcFi.Height + srcBlockSize - 1) / srcBlockSize;
    // driver seg map is always in 16x16 blocks because of HW limitation
    const mfxU16 dstBlockSize = 16;
    mfxU16 ratio = srcBlockSize / dstBlockSize;

    if (seg.NumSegmentIdAlloc < srcW * srcH || seg.SegmentId == 0 ||
        srcW != (dstW + ratio - 1) / ratio || srcH != (dstH + ratio - 1) / ratio)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // for now application seg map is accepted in 32x32 and 64x64 blocks
    // and driver seg map is always in 16x16 blocks
    // need to map one to another

    for (mfxU32 i = 0; i < dstH; i++)
    {
        for (mfxU32 j = 0; j < dstW; j++)
        {
            segMap.Y[i * dstPitch + j] = seg.SegmentId[(i / ratio) * srcW + j / ratio];
        }
    }

    return MFX_ERR_NONE;
}

void FillSegmentParam(Task const & task,
                      ENCODE_SEGMENT_PARAMETERS & segPar)
{
    Zero(segPar);

    mfxExtVP9Segmentation const & seg = GetActualExtBufferRef(*task.m_pParam, task.m_ctrl);

    for (mfxU16 i = 0; i < seg.NumSegments; i++)
    {
        mfxVP9SegmentParam const & param = seg.Segment[i];
        mfxI16 qIndexDelta = param.QIndexDelta;
        // clamp Q index delta if required (1-255)
        CheckAndFixQIndexDelta(qIndexDelta, task.m_frameParam.baseQIndex);
        segPar.SegData[i].SegmentQIndexDelta = qIndexDelta;
        segPar.SegData[i].SegmentLFLevelDelta = static_cast<mfxI8>(param.LoopFilterLevelDelta);

        if (IsFeatureEnabled(param.FeatureEnabled, FEAT_REF))
        {
            segPar.SegData[i].SegmentFlags.fields.SegmentReferenceEnabled = 1;
            segPar.SegData[i].SegmentFlags.fields.SegmentReference = MapSegmentRefControlToDDI(param.ReferenceFrame);
        }

        if (IsFeatureEnabled(param.FeatureEnabled, FEAT_SKIP))
        {
            segPar.SegData[i].SegmentFlags.fields.SegmentSkipped = 1;
        }
    }
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    Zero(init);
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

void CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }

            if (cache == 0)
            {
                assert(!"no space in cache");
                throw std::logic_error(": no space in cache");
            }

            *cache = update[i];
        }
    }
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

void CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return;
        }
    }

    assert(!"wrong feedbackNumber");
}

D3D9Encoder::D3D9Encoder()
    : m_guid()
    , m_pmfxCore(NULL)
    , m_caps()
    , m_platform()
    , m_sps()
    , m_pps()
    , m_seg()
    , m_descForFrameHeader()
    , m_seqParam()
    , m_width(0)
    , m_height(0)
{
} // D3D9Encoder::D3D9Encoder(VideoCORE* core)

D3D9Encoder::~D3D9Encoder()
{
    Destroy();

} // D3D9Encoder::~D3D9Encoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

// this function is aimed to workaround all CAPS reporting problems in mainline driver
void HardcodeCaps(ENCODE_CAPS_VP9& caps, mfxCoreInterface* pCore)
{
    //mfxCoreParam corePar;
    //pCore->GetCoreParam(pCore->pthis, &corePar);

    mfxPlatform platform;
    pCore->QueryPlatform(pCore->pthis, &platform);
    caps;

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    if (platform.CodeName >= MFX_PLATFORM_ICELAKE)
    {
        // for now driver supports only 2 pipes for LP and HP configurations, but for HP it reports 4 (actual
        // support will be added to the driver later), until then this hardcode is required
        // TODO: remove this when actual support of 4 pipes is implemented in the driver for HP configuration
        caps.NumScalablePipesMinus1 = 1;
    }
#endif //PRE_SI_TARGET_PLATFORM_GEN11

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    if (platform.CodeName >= MFX_PLATFORM_TIGERLAKE)
    {
        // mainline driver doesn't report REXT support correctly for TGL
        // hardcode proper CAPS values for now
        caps.MaxEncodedBitDepth = 1; // bit depths 8 and 10 are supported
        caps.YUV444ReconSupport = 1;
    }
#endif //PRE_SI_TARGET_PLATFORM_GEN12

}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    mfxCoreInterface* pCore,
    GUID guid,
    mfxU32 width,
    mfxU32 height)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAuxilliaryDevice +");

    if (pCore == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    m_pmfxCore = pCore;
    m_guid = guid;

    IDirect3DDeviceManager9* device = 0;

    mfxStatus sts = pCore->GetHandle(pCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    if (sts == MFX_ERR_NOT_FOUND)
        sts = m_pmfxCore->CreateAccelerationDevice(pCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    MFX_CHECK_STS(sts);
    MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    sts = auxDevice->Initialize(device);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    Zero(m_caps);

    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid),& m_caps, sizeof(m_caps));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    PrintDdiToLogOnce(m_caps);

    MFX_CHECK(m_caps.EncodeFunc, MFX_ERR_DEVICE_FAILED);

    HardcodeCaps(m_caps, pCore);

    m_width  = width;
    m_height = height;

    m_auxDevice = auxDevice;

    Zero(m_platform);
    sts = pCore->QueryPlatform(pCore->pthis, &m_platform);
    MFX_CHECK_STS(sts);

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAuxilliaryDevice -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus D3D9Encoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService +");

    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc = {};
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = {};
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_frameHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE);
    InitVp9SeqLevelParam(par, m_seqParam);

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus D3D9Encoder::Reset(VP9MfxVideoParam const & par)
{
    par;
    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Reset(MfxVideoParam const & par)

mfxU32 D3D9Encoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_NV12;
} // mfxU32 D3D9Encoder::GetReconSurfFourCC()

mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    frameWidth; frameHeight;
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryCompBufferInfo +");

    ENCODE_FORMAT_COUNT encodeFormatCount;
    encodeFormatCount.CompressedBufferInfoCount = 0;
    encodeFormatCount.UncompressedFormatCount = 0;

    GUID guid = m_auxDevice->GetCurrentGuid();
    HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    std::vector<ENCODE_COMP_BUFFER_INFO> compBufferInfo;
    std::vector<D3DDDIFORMAT> uncompBufferInfo;
    compBufferInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
    uncompBufferInfo.resize(encodeFormatCount.UncompressedFormatCount);

    ENCODE_FORMATS encodeFormats;
    encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
    encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
    encodeFormats.pCompressedBufferInfo = &compBufferInfo[0];
    encodeFormats.pUncompressedFormats = &uncompBufferInfo[0];

    hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

    size_t i = 0;
    for (; i < compBufferInfo.size(); i++)
        if (compBufferInfo[i].Type == type)
            break;

    MFX_CHECK(i < compBufferInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = compBufferInfo[i].CreationWidth;
    request.Info.Height = compBufferInfo[i].CreationHeight;
    request.Info.FourCC = compBufferInfo[i].CompressedFormats;

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryCompBufferInfo -");

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus D3D9Encoder::QueryPlatform(mfxPlatform& platform)
{
    platform = m_platform;

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::QueryPlatform(mfxPlatform& platform)

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Register +");

    mfxFrameAllocator & fa = m_pmfxCore->FrameAllocator;
    EmulSurfaceRegister surfaceReg = {};
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i], MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Register -");

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

#define MAX_NUM_COMP_BUFFERS_VP9 6 // SPS, PPS, bitstream, uncompressed header, segment map, per-segment parameters

mfxStatus D3D9Encoder::Execute(
    Task const & task,
    mfxHDL          surface)
{
    VP9_LOG("\n (VP9_LOG) Frame %d D3D9Encoder::Execute +", task.m_frameOrder);

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(MAX_NUM_COMP_BUFFERS_VP9);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    const VP9MfxVideoParam& curMfxPar = *task.m_pParam;

    FillSpsBuffer(curMfxPar, m_caps, m_sps, task, static_cast<mfxU16>(m_width), static_cast<mfxU16>(m_height));

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_SPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_sps));
    compBufferDesc[bufCnt].pCompBuffer = &m_sps;
    bufCnt++;

    // prepare frame header: write IVF and uncompressed header, calculate bit offsets
    BitOffsets offsets;
    mfxU8 * pBuf = &m_frameHeaderBuf[0];
    Zero(m_frameHeaderBuf);

    mfxU16 bytesWritten = PrepareFrameHeader(curMfxPar, pBuf, (mfxU32)m_frameHeaderBuf.size(), task, m_seqParam, offsets);

    // fill PPS DDI structure for current frame
    FillPpsBuffer(curMfxPar, task, m_pps, offsets);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    compBufferDesc[bufCnt].pCompBuffer = &m_pps;
    bufCnt++;

    mfxU32 bitstream = task.m_pOutBs->idInPool;
    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    compBufferDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    mfxExtCodingOption2 opt2 = GetExtBufferRef(curMfxPar);
    if (task.m_frameParam.segmentation == APP_SEGMENTATION)
    {
        mfxStatus sts = FillSegmentMap(task, m_pmfxCore);
        MFX_CHECK_STS(sts);

        mfxU32 segMap = task.m_pSegmentMap->idInPool;
        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_MBSEGMENTMAP;
        compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(segMap));
        compBufferDesc[bufCnt].pCompBuffer = &segMap;
        bufCnt++;

        FillSegmentParam(task, m_seg);
        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_MBSEGMENTMAPPARMS;
        compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_seg));
        compBufferDesc[bufCnt].pCompBuffer = &m_seg;
        bufCnt++;
    }

    m_descForFrameHeader = MakePackedByteBuffer(pBuf, bytesWritten);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer = &m_descForFrameHeader;
    bufCnt++;

    try
    {
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        if (FAILED(hr))
        {
            VP9_LOG("\n FATAL: error status from the driver on BeginFrame(): [%x]!\n", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
        if (FAILED(hr))
        {
            VP9_LOG("\n FATAL: error status from the driver on Execute(): [%x]!\n", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_auxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        VP9_LOG("\n FATAL: exception from the driver on executing task!\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) Frame %d D3D9Encoder::Execute -", task.m_frameOrder);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D9Encoder::QueryStatus(
    Task & task)
{
    VP9_LOG("\n (VP9_LOG) Frame %d D3D9Encoder::QueryStatus +", task.m_frameOrder);

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_taskIdForDriver); // TODO: fix to unique status report number

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
            feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
            feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;

            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            if (FAILED(hr))
            {
                VP9_LOG("\n FATAL: error status from the driver received on quering task status: [%x]!\n", hr);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            VP9_LOG("\n FATAL: exception from the driver on quering task status!\n");
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_taskIdForDriver);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    mfxStatus sts;
    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize; // TODO: save bitstream size here
        m_feedbackCached.Remove(task.m_taskIdForDriver);
        sts = MFX_ERR_NONE;
        break;
    case ENCODE_NOTREADY:
        sts = MFX_WRN_DEVICE_BUSY;
        break;
    case ENCODE_ERROR:
        assert(!"task status ERROR");
        sts = MFX_ERR_DEVICE_FAILED;
        break;
    case ENCODE_NOTAVAILABLE:
    default:
        assert(!"bad feedback status");
        sts = MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) Frame %d D3D9Encoder::QueryStatus -", task.m_frameOrder);
    return sts;
} // mfxStatus D3D9Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D9Encoder::Destroy()
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy +");
    m_auxDevice.reset(0);
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy -");

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Destroy()

void PrintDdiToLog(ENCODE_CAPS_VP9 const &caps)
{
    caps;
    VP9_LOG("\n\n*** DDI CAPS DUMP ***\n");
    VP9_LOG("*** CodingLimitSet=%d\n", caps.CodingLimitSet);
    VP9_LOG("*** Color420Only=%d\n", caps.Color420Only);
    VP9_LOG("*** ForcedSegmentationSupport=%d\n", caps.ForcedSegmentationSupport);
    VP9_LOG("*** FrameLevelRateCtrl=%d\n", caps.FrameLevelRateCtrl);
    VP9_LOG("*** BRCReset=%d\n", caps.BRCReset);
    VP9_LOG("*** AutoSegmentationSupport=%d\n", caps.AutoSegmentationSupport);
    VP9_LOG("*** TemporalLayerRateCtrl=%d\n", caps.TemporalLayerRateCtrl);
    VP9_LOG("*** DynamicScaling=%d\n", caps.DynamicScaling);
    VP9_LOG("*** TileSupport=%d\n", caps.TileSupport);
    VP9_LOG("*** NumScalablePipesMinus1=%d\n", caps.NumScalablePipesMinus1);
    VP9_LOG("*** YUV422ReconSupport=%d\n", caps.YUV422ReconSupport);
    VP9_LOG("*** YUV444ReconSupport=%d\n", caps.YUV444ReconSupport);
    VP9_LOG("*** MaxEncodedBitDepth=%d\n", caps.MaxEncodedBitDepth);
    VP9_LOG("*** UserMaxFrameSizeSupport=%d\n", caps.UserMaxFrameSizeSupport);
    VP9_LOG("*** SegmentFeatureSupport=%d\n", caps.SegmentFeatureSupport);
    VP9_LOG("*** DirtyRectSupport=%d\n", caps.DirtyRectSupport);
    VP9_LOG("*** MoveRectSupport=%d\n", caps.MoveRectSupport);
    VP9_LOG("***\n");
    VP9_LOG("*** EncodeFunc=%d\n", caps.EncodeFunc);
    VP9_LOG("*** HybridPakFunc=%d\n", caps.HybridPakFunc);
    VP9_LOG("*** EncFunc=%d\n", caps.EncFunc);
    VP9_LOG("***\n");
    VP9_LOG("*** MaxPicWidth=%d\n", caps.MaxPicWidth);
    VP9_LOG("*** MaxPicHeight=%d\n", caps.MaxPicHeight);
    VP9_LOG("*** MaxNumOfDirtyRect=%d\n", caps.MaxNumOfDirtyRect);
    VP9_LOG("*** MaxNumOfMoveRect=%d\n", caps.MaxNumOfMoveRect);
    VP9_LOG("*** END OF DUMP ***\n");
}

std::once_flag PrintDdiToLog_flag;

void PrintDdiToLogOnce(ENCODE_CAPS_VP9 const &caps)
{
    caps;
#ifdef VP9_LOGGING
    try
    {
        std::call_once(PrintDdiToLog_flag, PrintDdiToLog, caps);
    }
    catch (...)
    {
    }
#endif
}

#endif // (MFX_VA_WIN)

} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // PRE_SI_TARGET_PLATFORM_GEN10
