//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
//

#if defined (_WIN32) || defined (_WIN64)

#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_d3d9.h"
#include "mfx_vp9_encode_hw_d3d11.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_WIN)

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
            case MFX_IMPL_VIA_D3D11:
                return new D3D11Encoder;
            default:
                return 0;
            }
        }

        return 0;
    };

D3D11Encoder::D3D11Encoder()
    : m_guid()
    , m_pmfxCore(NULL)
    , m_caps()
    , m_sps()
    , m_pps()
    , m_infoQueried(false)
    , m_descForFrameHeader()
    , m_seqParam()
    , m_width(0)
    , m_height(0)
{
} // D3D11Encoder::D3D11Encoder(VideoCORE* core)


D3D11Encoder::~D3D11Encoder()
{
    Destroy();

} // D3D11Encoder::~D3D11Encoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    mfxCoreInterface* pCore,
    GUID guid,
    mfxU32 width,
    mfxU32 height)
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::CreateAuxilliaryDevice +");

    m_pmfxCore = pCore;
    mfxStatus sts = MFX_ERR_NONE;
    ID3D11Device* device;
    D3D11_VIDEO_DECODER_DESC    desc = {};
    D3D11_VIDEO_DECODER_CONFIG  config = {};
    HRESULT hr;
    mfxU32 count = 0;

    m_guid = guid;
    m_width = width;
    m_height = height;

    // [0] Get device/context
    {
        sts = pCore->GetHandle(pCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        if (sts == MFX_ERR_NOT_FOUND)
            sts = pCore->CreateAccelerationDevice(pCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        MFX_CHECK_STS(sts);
        MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

        device->GetImmediateContext(&m_pContext);
        MFX_CHECK(m_pContext, MFX_ERR_DEVICE_FAILED);

        m_pVDevice = device;
        m_pVContext = m_pContext;
        MFX_CHECK(m_pVDevice, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(m_pVContext, MFX_ERR_DEVICE_FAILED);
    }

    // [1] Query supported decode profiles
    {
        bool isFound = false;

        UINT profileCount = m_pVDevice->GetVideoDecoderProfileCount();
        assert(profileCount > 0);

        for (UINT i = 0; i < profileCount; i++)
        {
            GUID profileGuid;

            hr = m_pVDevice->GetVideoDecoderProfile(i, &profileGuid);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

            if (m_guid == profileGuid)
            {
                isFound = true;
                break;
            }
        }
        MFX_CHECK(isFound, MFX_ERR_UNSUPPORTED);
    }

    // [2] Query the supported encode functions
    {
        desc.SampleWidth = m_width;
        desc.SampleHeight = m_height;
        desc.OutputFormat = DXGI_FORMAT_NV12;
        desc.Guid = m_guid;

        hr = m_pVDevice->GetVideoDecoderConfigCount(&desc, &count);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    // [3] CreateVideoDecoder
    {
        desc.SampleWidth = m_width;
        desc.SampleHeight = m_height;
        desc.OutputFormat = DXGI_FORMAT_NV12;
        desc.Guid = m_guid;

        config.ConfigDecoderSpecific = ENCODE_ENC_PAK;
        config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;

        hr = m_pVDevice->CreateVideoDecoder(&desc, &config, &m_pVDecoder);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    // [4] Query the encoding device capabilities
    {
        Zero(m_caps);

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
        ext.pPrivateInputData = 0;
        ext.PrivateInputDataSize = 0;
        ext.pPrivateOutputData = &m_caps;
        ext.PrivateOutputDataSize = sizeof(m_caps);
        ext.ResourceCount = 0;
        ext.ppResourceList = 0;

        // mainline driver returns an error on attempt to query CAPS for VP9 encoder for DX11
        // hardcoded caps are used to workaround this issue
        //hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);
        //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        HardcodeCaps(m_caps, pCore);
    }

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::CreateAuxilliaryDevice -");
    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::CreateAuxilliaryDevice(VideoCORE* pCore, GUID guid, mfxU32 width, mfxU32 height)

#define VP9_MAX_UNCOMPRESSED_HEADER_SIZE 1000

mfxStatus D3D11Encoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::CreateAccelerationService +");

    m_infoQueried = false;

    m_frameHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE);
    InitVp9SeqLevelParam(par, m_seqParam);

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::CreateAccelerationService -");
    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus D3D11Encoder::Reset(VP9MfxVideoParam const & par)
{
    par;
    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Reset(MfxVideoParam const & par)

mfxU32 D3D11Encoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_NV12;
} // mfxU32 D3D11Encoder::GetReconSurfFourCC()

mfxU8 ConvertDX9TypeToDX11Type(mfxU8 type)
{
    switch (type)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA;
    case D3DDDIFMT_INTELENCODE_QUANTDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA;
    case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA;
    case D3DDDIFMT_INTELENCODE_MBDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA;
    case D3DDDIFMT_INTELENCODE_SEIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA;
    case D3DDDIFMT_INTELENCODE_VUIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA;
    case D3DDDIFMT_INTELENCODE_VMESTATE:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE;
    case D3DDDIFMT_INTELENCODE_VMEINPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT;
    case D3DDDIFMT_INTELENCODE_VMEOUTPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT;
    case D3DDDIFMT_INTELENCODE_EFEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA;
    case D3DDDIFMT_INTELENCODE_STATDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA;
    case D3DDDIFMT_INTELENCODE_MBQPDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA;
    default:
        break;
    }
    return 0xFF;
}

mfxU32 ConvertDXGIFormatToMFXFourCC(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_NV12: return MFX_FOURCC_NV12;
    case DXGI_FORMAT_P010: return MFX_FOURCC_P010;
    case DXGI_FORMAT_YUY2: return MFX_FOURCC_YUY2;
    case DXGI_FORMAT_Y210: return MFX_FOURCC_P210;
    case DXGI_FORMAT_Y410: return MFX_FOURCC_Y410;
    case DXGI_FORMAT_P8: return MFX_FOURCC_P8;
    default:
        return 0;
    }
}

mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    frameWidth; frameHeight;
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::QueryCompBufferInfo +");

    HRESULT hr;

    type = (D3DDDIFORMAT)ConvertDX9TypeToDX11Type((mfxU8)type);


    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT cnt = {};

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_FORMAT_COUNT_ID;
        ext.pPrivateOutputData = &cnt;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);

        hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(cnt.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(cnt.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats = {};
        encodeFormats.CompressedBufferInfoSize = (int)(m_compBufInfo.size() * sizeof(ENCODE_COMP_BUFFER_INFO));
        encodeFormats.UncompressedFormatSize = (int)(m_uncompBufInfo.size() * sizeof(D3DDDIFORMAT));
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        Zero(ext);
        ext.Function = ENCODE_FORMATS_ID;
        ext.pPrivateOutputData = &encodeFormats;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);

        hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = ConvertDXGIFormatToMFXFourCC((DXGI_FORMAT)m_compBufInfo[i].CompressedFormats);

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::QueryCompBufferInfo -");

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Register +");

    mfxFrameAllocator & fa = m_pmfxCore->FrameAllocator;
    std::vector<mfxHDLPair> & queue = (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA) ? m_bsQueue : m_reconQueue;

    queue.resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxHDLPair handlePair = {};

        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL*)&handlePair);
        MFX_CHECK_STS(sts);

        queue[i] = handlePair;
    }

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Register -");

    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

#define MAX_NUM_COMP_BUFFERS_VP9 6 // SPS, PPS, bitstream, uncompressed header, segment map, per-segment parameters

mfxStatus D3D11Encoder::Execute(
    Task const & task,
    mfxHDL       surface)
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Execute +");

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(MAX_NUM_COMP_BUFFERS_VP9);

    // prepare resource list
    // it contains resources in video memory that needed for the encoding operation
    mfxU32       RES_ID_BITSTREAM = 0;          // bitstream surface takes first place in resourceList
    mfxU32       RES_ID_ORIGINAL  = 1;
    mfxU32       RES_ID_REFERENCE = 2;          // then goes all reference frames from dpb
    mfxU32       RES_ID_RECONSTRUCT = RES_ID_REFERENCE + task.m_pRecFrame->idInPool;

    mfxU32 resourceCount = mfxU32(RES_ID_REFERENCE + m_reconQueue.size());
    std::vector<ID3D11Resource*> resourceList;
    resourceList.resize(resourceCount);

    resourceList[RES_ID_BITSTREAM] = static_cast<ID3D11Resource *>(m_bsQueue[task.m_pOutBs->idInPool].first);
    resourceList[RES_ID_ORIGINAL] = static_cast<ID3D11Resource *>(surface);

    for (mfxU32 i = 0; i < m_reconQueue.size(); i++)
    {
        resourceList[RES_ID_REFERENCE + i] = static_cast<ID3D11Resource *>(m_reconQueue[i].first);
    }

    // [1]. buffers in system memory (configuration buffers)
    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    const VP9MfxVideoParam& curMfxPar = *task.m_pParam;

    FillSpsBuffer(curMfxPar, m_caps, m_sps, task);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA);
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

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    compBufferDesc[bufCnt].pCompBuffer = &m_pps;

    ENCODE_INPUT_DESC encodeInputDesc;
    encodeInputDesc.ArraSliceOriginal = 0;
    encodeInputDesc.IndexOriginal = RES_ID_ORIGINAL;
    encodeInputDesc.ArraySliceRecon = (UINT)(size_t(m_reconQueue[task.m_pRecFrame->idInPool].second));
    encodeInputDesc.IndexRecon = RES_ID_RECONSTRUCT;
    compBufferDesc[bufCnt].pReserved = &encodeInputDesc;
    bufCnt++;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(RES_ID_BITSTREAM));
    compBufferDesc[bufCnt].pCompBuffer = &RES_ID_BITSTREAM;
    bufCnt++;

    m_descForFrameHeader = MakePackedByteBuffer(pBuf, bytesWritten);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer = &m_descForFrameHeader;
    bufCnt++;

    assert(bufCnt <= compBufferDesc.size());

    try
    {
        HRESULT hr;
        D3D11_VIDEO_DECODER_EXTENSION ext = {};

        ext.Function = ENCODE_ENC_PAK_ID;
        ext.pPrivateInputData = &encodeExecuteParams;
        ext.PrivateInputDataSize = sizeof(ENCODE_EXECUTE_PARAMS);
        ext.ResourceCount = (UINT)resourceList.size();
        ext.ppResourceList = &resourceList[0];
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
            hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
            hr = m_pVContext->DecoderEndFrame(m_pVDecoder);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Execute -");

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D11Encoder::QueryStatus(
    Task & task)
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::QueryStatus +");

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

            D3D11_VIDEO_DECODER_EXTENSION ext = {};

            ext.Function = ENCODE_QUERY_STATUS_ID;
            ext.pPrivateInputData = &feedbackDescr;
            ext.PrivateInputDataSize = sizeof(feedbackDescr);
            ext.pPrivateOutputData = &m_feedbackUpdate[0];
            ext.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));

            HRESULT hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
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

    VP9_LOG("\n (VP9_LOG) D3D11Encoder::QueryStatus -");
    return sts;
} // mfxStatus D3D11Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D11Encoder::Destroy()
{
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Destroy +");
    VP9_LOG("\n (VP9_LOG) D3D11Encoder::Destroy -");

    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::Destroy()

#endif // (MFX_VA_WIN)

} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // PRE_SI_TARGET_PLATFORM_GEN10
