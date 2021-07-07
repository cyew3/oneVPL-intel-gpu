// Copyright (c) 2016-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <mutex>
#include "libmfx_core_interface.h"
#include "mfx_vp9_encode_hw_d3d9.h"
#include "mfx_vp9_encode_hw_par.h"

#if defined (MFX_VA_WIN)

namespace MfxHwVP9Encode
{

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
    mfxExtCodingOption3 opt3 = GetExtBufferRef(par);
    sps.SeqFlags.fields.SourceBitDepth = MapBitDepthToDDI(par.mfx.FrameInfo.BitDepthLuma);
    sps.SeqFlags.fields.EncodedBitDepth = MapBitDepthToDDI(opt3.TargetBitDepthLuma);
    sps.SeqFlags.fields.SourceFormat = MapChromaFormatToDDI(par.mfx.FrameInfo.ChromaFormat);
    sps.SeqFlags.fields.EncodedFormat = MapChromaFormatToDDI(opt3.TargetChromaFormatPlus1 - 1);

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
    VP9MfxVideoParam const & /*par*/,
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

    pps.CurrOriginalPic      = (UCHAR)task.m_pRecFrame->idInPool;
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

    VM_ASSERT(offsets.BitOffsetUncompressedHeader % 8 == 0);
    pps.UncompressedHeaderByteOffset = offsets.BitOffsetUncompressedHeader >> 3;
}

mfxStatus FillSegmentMap(Task const & task,
                         VideoCORE* m_pmfxCore)
{
    // for now application seg map is accepted in 64x64 blocks, for DG2+ in 32x32 and 64x64 blocks
    // and driver seg map is always in 16x16 blocks
    // need to map one to another

    mfxFrameData segMap = {};

    FrameLocker lock(m_pmfxCore, segMap, task.m_pSegmentMap->pSurface->Data.MemId);
    if (segMap.Y == 0)
    {
        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    mfxExtVP9Segmentation const & seg = GetActualExtBufferRef(*task.m_pParam, task.m_ctrl);

    mfxFrameInfo const & dstFi = task.m_pSegmentMap->pSurface->Info;
    mfxU32 dstW = dstFi.Width;
    mfxU32 dstH = dstFi.Height;

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
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    mfxU32 dstPitch = 0;
    if (m_pmfxCore->GetVAType() == MFX_HW_D3D9)
    {
        // for D3D9 2D surface is used, need to take pitch into account
        dstPitch = segMap.Pitch;
    }
    else
    {
        // for D3D11 1D surface is used, no pitch here
        dstPitch = dstW;
    }

    // set segment IDs to the driver's surface
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

// this function is aimed to workaround all CAPS reporting problems in mainline driver
void HardcodeCaps(ENCODE_CAPS_VP9& caps, VideoCORE* pCore, VP9MfxVideoParam const & par)
{
//WA because of Direct3D 11 limitation
#if !defined(STRIP_EMBARGO)
    eMFXHWType platform = pCore->GetHWType();
    eMFXVAType vatype = pCore->GetVAType();

    if (platform >= MFX_HW_TGL_LP && vatype == MFX_HW_D3D11)
    {
        if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010 &&
            caps.MaxPicWidth > D3D11_REQ_TEXTURECUBE_DIMENSION / 2) 
        {
            caps.MaxPicWidth = D3D11_REQ_TEXTURECUBE_DIMENSION / 2;
        }
        if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_Y410 && 
            caps.MaxPicHeight > D3D11_REQ_TEXTURECUBE_DIMENSION * 2 / 3)
        {
            caps.MaxPicHeight = D3D11_REQ_TEXTURECUBE_DIMENSION * 2 / 3;
        }
    }
#else
    caps;
    pCore;
    par;
#endif // !defined(STRIP_EMBARGO)
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    VideoCORE* pCore,
    GUID guid,
    VP9MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAuxilliaryDevice");

    if (pCore == 0)
    {
        MFX_RETURN(MFX_ERR_NULL_PTR);
    }

    m_pmfxCore = pCore;
    m_guid = guid;

    IDirect3DDeviceManager9 *device = nullptr;
    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_pmfxCore, MFXICORED3D_GUID);
    MFX_CHECK_WITH_ASSERT(pID3D != nullptr, MFX_ERR_DEVICE_FAILED);

    device = pID3D->GetD3D9DeviceManager();
    MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

    std::unique_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    mfxStatus sts = auxDevice->Initialize(device);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid), &m_caps, sizeof(m_caps));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    PrintDdiToLogOnce(m_caps);

    MFX_CHECK(m_caps.EncodeFunc, MFX_ERR_DEVICE_FAILED);

    HardcodeCaps(m_caps, pCore, par);

    m_width  = par.mfx.FrameInfo.Width;
    m_height = par.mfx.FrameInfo.Height;

    m_auxDevice = std::move(auxDevice);

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, VP9MfxVideoParam const & par)


mfxStatus D3D9Encoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAccelerationService");

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

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    mfxStatus sts = D3DXCommonEncoder::InitBlockingSync(m_pmfxCore);
    MFX_CHECK_STS(sts);
#endif

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
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::QueryCompBufferInfo");

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

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::Register");

    //mfxFrameAllocator & fa = m_pmfxCore->FrameAllocator;
    EmulSurfaceRegister surfaceReg = {};
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_pmfxCore->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
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
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        if (m_bIsBlockingTaskSyncEnabled)
        {
            m_EventCache->Init(response.NumFrameActual);
        }
#endif
    }

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

#define MAX_NUM_COMP_BUFFERS_VP9 6 // SPS, PPS, bitstream, uncompressed header, segment map, per-segment parameters

mfxStatus D3D9Encoder::Execute(
    Task const & task,
    mfxHDLPair   pair)
{
    MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::Execute, Frame:", "%d", task.m_frameOrder);

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(MAX_NUM_COMP_BUFFERS_VP9);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
#else
	encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 1;
#endif
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
    MFX_CHECK(bytesWritten != 0, MFX_ERR_MORE_DATA);

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

    if (task.m_frameParam.segmentation == APP_SEGMENTATION)
    {
        mfxStatus sts = FillSegmentMap(task, m_pmfxCore);
        MFX_CHECK_STS(sts);

        mfxU32 &segMap = task.m_pSegmentMap->idInPool;
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
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)pair.first, 0);
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "FATAL: error status from the driver on BeginFrame(): ", "%x", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        // allocate the event
        if (m_bIsBlockingTaskSyncEnabled)
        {
            Task & task1 = const_cast<Task &>(task);
            task1.m_GpuEvent.m_gpuComponentId = GPU_COMPONENT_ENCODE;
            m_EventCache->GetEvent(task1.m_GpuEvent.gpuSyncEvent);

            hr = m_auxDevice->Execute(DXVA2_SET_GPU_TASK_EVENT_HANDLE, task1.m_GpuEvent, (void *)0);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
#endif

        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "FATAL: error status from the driver on Execute(): ", "%x", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_auxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        MFX_LTRACE_X(MFX_TRACE_LEVEL_INTERNAL, "FATAL: exception from the driver on executing task!");
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D9Encoder::QueryStatusAsync(
    Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VP9 encode DDIQueryTask");

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
                MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "FATAL: error status from the driver received on quering task status: ", "%x", hr);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FATAL: exception from the driver on quering task status!");
            MFX_RETURN(MFX_ERR_DEVICE_FAILED);
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

    MFX_RETURN(sts);
} // mfxStatus D3D9Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D9Encoder::Destroy()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::Destroy");
    m_auxDevice.reset(0);

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Destroy()

void PrintDdiToLog(ENCODE_CAPS_VP9 const &caps)
{
    caps;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_DEFAULT, "*** DDI CAPS DUMP ***");
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** CodingLimitSet=", "%d" ,caps.CodingLimitSet);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** Color420Only=", "%d", caps.Color420Only);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** ForcedSegmentationSupport=", "%d", caps.ForcedSegmentationSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** FrameLevelRateCtrl=", "%d", caps.FrameLevelRateCtrl);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** BRCReset=", "%d", caps.BRCReset);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** AutoSegmentationSupport=", "%d", caps.AutoSegmentationSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** TemporalLayerRateCtrl=", "%d", caps.TemporalLayerRateCtrl);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** DynamicScaling=", "%d", caps.DynamicScaling);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** TileSupport=", "%d", caps.TileSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** NumScalablePipesMinus1=", "%d", caps.NumScalablePipesMinus1);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** YUV422ReconSupport=", "%d", caps.YUV422ReconSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** YUV444ReconSupport=", "%d", caps.YUV444ReconSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxEncodedBitDepth=", "%d", caps.MaxEncodedBitDepth);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** UserMaxFrameSizeSupport=", "%d", caps.UserMaxFrameSizeSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** SegmentFeatureSupport=", "%d", caps.SegmentFeatureSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** DirtyRectSupport=", "%d", caps.DirtyRectSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MoveRectSupport=", "%d", caps.MoveRectSupport);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** EncodeFunc=", "%d", caps.EncodeFunc);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** HybridPakFunc=", "%d", caps.HybridPakFunc);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** EncFunc=", "%d", caps.EncFunc);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxPicWidth=", "%d", caps.MaxPicWidth);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxPicHeight=", "%d", caps.MaxPicHeight);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxNumOfDirtyRect=", "%d", caps.MaxNumOfDirtyRect);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxNumOfMoveRect=", "%d", caps.MaxNumOfMoveRect);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_DEFAULT, "*** MaxNum_Reference0=", "%d", caps.MaxNum_Reference0);
}

std::once_flag PrintDdiToLog_flag;

void PrintDdiToLogOnce(ENCODE_CAPS_VP9 const &caps)
{
    caps;
    try
    {
        std::call_once(PrintDdiToLog_flag, PrintDdiToLog, caps);
    }
    catch (...)
    {
    }
}
} // MfxHwVP9Encode

#endif // (MFX_VA_WIN)
