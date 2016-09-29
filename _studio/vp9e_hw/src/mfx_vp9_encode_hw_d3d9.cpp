/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#if defined (AS_VP9E_PLUGIN)

#if defined (_WIN32) || defined (_WIN64)

#include "mfx_vp9_encode_hw_d3d9.h"

namespace MfxHwVP9Encode
{

    DriverEncoder* CreatePlatformVp9Encoder(mfxCoreInterface* pCore)
         {
        if (pCore)
            {
            mfxCoreParam par = {};

            if (pCore->GetCoreParam(pCore->pthis, &par))
                return 0;

                switch (par.Impl & 0xF00)
                {
                case MFX_IMPL_VIA_D3D9:
                    return new D3D9Encoder;
                /*case MFX_IMPL_VIA_D3D11: // no DX11 support so far
                    return new D3D11Encoder;*/
                default:
                    return 0;
                }
            }
            return 0;
        };

#if defined (MFX_VA_WIN)

void FillSpsBuffer(
    VP9MfxVideoParam const & par,
    ENCODE_CAPS_VP9 const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 & sps)
{
    Zero(sps);

    sps.wMaxFrameWidth    = par.mfx.FrameInfo.Width;
    sps.wMaxFrameHeight   = par. mfx.FrameInfo.Height;
    sps.GopPicSize        = par.mfx.GopPicSize;
    sps.TargetUsage       = (UCHAR)par.mfx.TargetUsage;
    sps.RateControlMethod = (UCHAR)par.mfx.RateControlMethod; // TODO: make correct mapping for mfx->DDI

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        sps.TargetBitRate[0]           = par.mfx.TargetKbps; // no support for temporal scalability. TODO: add support for temporal scalability
        sps.MaxBitRate                 = par.mfx.MaxKbps; // TODO: understand how to use MaxBitRate for temporal scalability
        sps.VBVBufferSizeInBit         = par.mfx.BufferSizeInKB * 8000; // TODO: understand how to use for temporal scalability
        sps.InitVBVBufferFullnessInBit = par.mfx.InitialDelayInKB * 8000; // TODO: understand how to use for temporal scalability
    }

    sps.FrameRate[0].Numerator   =  par.mfx.FrameInfo.FrameRateExtN; // no support for temporal scalability. TODO: add support for temporal scalability
    sps.FrameRate[0].Denominator = par.mfx.FrameInfo.FrameRateExtD;  // no support for temporal scalability. TODO: add support for temporal scalability

    sps.NumTemporalLayersMinus1 = 0;
}

void FillPpsBuffer(
    VP9MfxVideoParam const & par,
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_VP9 & pps)
{
    Zero(pps);

    VP9FrameLevelParam const &framePar = task.m_frameParam;

    //pps.MinLumaACQIndex = 0;
    //pps.MaxLumaACQIndex = 127;

    // per-frame part of pps structure
    pps.SrcFrameWidthMinus1 =  par.mfx.FrameInfo.Width - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling. TODO: fix for dynamic scaling.
    pps.SrcFrameHeightMinus1 = par.mfx.FrameInfo.Height - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling.
    pps.DstFrameWidthMinus1 =  par.mfx.FrameInfo.Width - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling. TODO: fix for dynamic scaling.
    pps.DstFrameHeightMinus1 = par.mfx.FrameInfo.Height - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling.

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
    pps.PicFlags.fields.show_frame           = framePar.showFarme;
    pps.PicFlags.fields.error_resilient_mode = framePar.errorResilentMode;
    pps.PicFlags.fields.intra_only           = framePar.intraOnly;
    pps.PicFlags.fields.refresh_frame_context = framePar.refreshFrameContext;
    pps.PicFlags.fields.allow_high_precision_mv = framePar.allowHighPrecisionMV;
    pps.PicFlags.fields.segmentation_enabled = 0; // segmentation isn't supported for now. TODO: enable segmentation

    pps.LumaACQIndex        = framePar.baseQIndex;
    pps.LumaDCQIndexDelta   = framePar.qIndexDeltaLumaDC;
    pps.ChromaACQIndexDelta = framePar.qIndexDeltaChromaAC;
    pps.ChromaACQIndexDelta = framePar.qIndexDeltaChromaDC;

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

    pps.StatusReportFeedbackNumber = task.m_frameOrder; // TODO: fix to unique value
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
: m_pmfxCore(NULL)
{
} // D3D9Encoder::D3D9Encoder(VideoCORE* core)


D3D9Encoder::~D3D9Encoder()
{
    Destroy();

} // D3D9Encoder::~D3D9Encoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

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
    // MFX_CHECK(m_caps.EncodeFunc, MFX_ERR_DEVICE_FAILED); TODO: need to uncomment for machine with support of VP9 HW Encode

    m_width  = width;
    m_height = height;

    m_auxDevice = auxDevice;

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAuxilliaryDevice -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus D3D9Encoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService +");

    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    m_video = par;

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

    FillSpsBuffer(par, m_caps, m_sps);

    m_uncompressedHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE);
    InitVp9SeqLevelParam(m_video, m_seqParam);

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus D3D9Encoder::Reset(VP9MfxVideoParam const & par)
{
    m_video = par;

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

#define MAX_NUM_COMP_BUFFERS_VP9 5 // SPS, PPS, uncompressed header, segment map, per-segment parameters

ENCODE_PACKEDHEADER_DATA MakePackedByteBuffer(mfxU8 * data, mfxU32 size)
{
    ENCODE_PACKEDHEADER_DATA desc = { 0 };
    desc.pData                  = data;
    desc.BufferSize             = size;
    desc.DataLength             = size;
    return desc;
}

mfxStatus D3D9Encoder::Execute(
    Task const & task,
    mfxHDL          surface)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Execute +");

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(MAX_NUM_COMP_BUFFERS_VP9);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_SPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_sps));
    compBufferDesc[bufCnt].pCompBuffer = &m_sps;
    bufCnt++;

    FillPpsBuffer(m_video, task, m_pps);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    compBufferDesc[bufCnt].pCompBuffer = &m_pps;
    bufCnt++;

    mfxU32 bitstream = task.m_pOutBs->idInPool;
    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    compBufferDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    // writing IVF and uncompressed header
    //vpx_write_bit_buffer localBuf;

    Zero(m_uncompressedHeaderBuf);
    mfxU8 * pBuff = &m_uncompressedHeaderBuf[0];
    mfxU16 bytesWritten = 0;

    mfxExtCodingOptionVP9 &opt = GetExtBufferRef(m_video);
    if (opt.WriteIVFHeaders != MFX_CODINGOPTION_OFF)
    {
        if (InsertSeqHeader(task))
        {
            AddSeqHeader(m_video.mfx.FrameInfo.Width,
                m_video.mfx.FrameInfo.Height,
                m_video.mfx.FrameInfo.FrameRateExtN,
                m_video.mfx.FrameInfo.FrameRateExtD,
                opt.NumFramesForIVF,
                pBuff);
            bytesWritten += IVF_SEQ_HEADER_SIZE_BYTES;
        }

        AddPictureHeader(pBuff + bytesWritten);
        bytesWritten += IVF_PIC_HEADER_SIZE_BYTES;
    }

    BitOffsets offsets;
    mfxU16 uncompHeaderBitSize;
    //write_uncompressed_header(&m_libvpxBasedVP9Param, &localBuf, offsets);
    WriteUncompressedHeader(pBuff + bytesWritten,
                            (mfxU32)m_uncompressedHeaderBuf.size() - bytesWritten,
                            task,
                            m_seqParam,
                            offsets,
                            uncompHeaderBitSize);

    m_pps.BitOffsetForLFLevel            = offsets.BitOffsetForLFLevel + bytesWritten * 8;
    m_pps.BitOffsetForLFModeDelta        = offsets.BitOffsetForLFModeDelta ? offsets.BitOffsetForLFModeDelta + bytesWritten * 8 : 0;
    m_pps.BitOffsetForLFRefDelta         = offsets.BitOffsetForLFRefDelta ? offsets.BitOffsetForLFRefDelta+ bytesWritten * 8 : 0;
    m_pps.BitOffsetForQIndex             = offsets.BitOffsetForQIndex + bytesWritten * 8;
    m_pps.BitOffsetForFirstPartitionSize = offsets.BitOffsetForFirstPartitionSize + bytesWritten * 8;
    m_pps.BitOffsetForSegmentation       = offsets.BitOffsetForSegmentation + bytesWritten * 8;
    m_pps.BitSizeForSegmentation         = offsets.BitSizeForSegmentation;

    bytesWritten += (uncompHeaderBitSize + 7) / 8;
    m_descForFrameHeader = MakePackedByteBuffer(pBuff, bytesWritten);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer = &m_descForFrameHeader;
    bufCnt++;

    try
    {
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_auxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Execute -");

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D9Encoder::QueryStatus(
    Task & task)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryStatus +");

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_frameOrder); // TODO: fix to unique status report number

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
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_frameOrder);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    mfxStatus sts;
    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize; // TODO: save bitstream size here
        m_feedbackCached.Remove(task.m_frameOrder);
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

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryStatus -");
    return sts;
} // mfxStatus D3D9Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D9Encoder::Destroy()
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy +");
    m_auxDevice.reset(0);
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy -");

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Destroy()

#endif // (MFX_VA_WIN)

} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // AS_VP9E_PLUGIN
