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

#include "mfx_common.h"

#if defined (MFX_VA_WIN)

#include "libmfx_core_interface.h"
#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_d3d9.h"
#include "mfx_vp9_encode_hw_d3d11.h"

#include "mfx_enc_common.h"

namespace MfxHwVP9Encode
{

#define MFX_VP9E_HW_D3D11_DEFAULT_WIDTH  (1920)
#define MFX_VP9E_HW_D3D11_DEFAULT_HEIGHT (1088)

DriverEncoder* CreatePlatformVp9Encoder(VideoCORE* pCore)
{
    if (pCore)
    {
        switch (pCore->GetVAType())
        {
        case MFX_HW_D3D9:
            return new D3D9Encoder;
        case MFX_HW_D3D11:
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
    , m_platform()
    , m_sps()
    , m_pps()
    , m_seg()
    , m_infoQueried(false)
    , m_descForFrameHeader()
    , m_seqParam()
    , m_width(0)
    , m_height(0)
{
    m_pVDecoder.Release();
    m_pVDevice.Release();
    m_pVContext.Release();
} // D3D11Encoder::D3D11Encoder(VideoCORE* core)


D3D11Encoder::~D3D11Encoder()
{
    Destroy();

} // D3D11Encoder::~D3D11Encoder()

mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    VideoCORE* pCore,
    GUID guid,
    VP9MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAuxilliaryDevice");

    MFX_CHECK_NULL_PTR1(pCore);

    m_pmfxCore = pCore;

    D3D11_VIDEO_DECODER_DESC    desc = {};
    D3D11_VIDEO_DECODER_CONFIG  config = {};
    HRESULT hr;
    mfxU32 count = 0;

    m_guid = guid;

    if (par.mfx.FrameInfo.Width && par.mfx.FrameInfo.Height)
    {
        m_width = par.mfx.FrameInfo.Width;
        m_height = par.mfx.FrameInfo.Height;
    }
    else
    {
        // default width and height are used because GetVideoDecoderConfigCount()
        //  does not work with null-values
        m_width = MFX_VP9E_HW_D3D11_DEFAULT_WIDTH;
        m_height = MFX_VP9E_HW_D3D11_DEFAULT_HEIGHT;
    }

    // [0] Get device/context
    {
        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_pmfxCore);
        MFX_CHECK(pD3d11, MFX_ERR_DEVICE_FAILED)

        m_pVDevice = pD3d11->GetD3D11VideoDevice();
        m_pVContext = pD3d11->GetD3D11VideoContext();
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
        MFX_CHECK(count > 0, MFX_ERR_UNSUPPORTED);
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
        hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        PrintDdiToLogOnce(m_caps);

        HardcodeCaps(m_caps, pCore, par);
    }

    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::CreateAuxilliaryDevice(VideoCORE* pCore, GUID guid, VP9MfxVideoParam const & par)

#define VP9_MAX_UNCOMPRESSED_HEADER_SIZE 1000

mfxStatus D3D11Encoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAccelerationService");

    m_infoQueried = false;

    m_frameHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE);
    InitVp9SeqLevelParam(par, m_seqParam);

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    mfxStatus sts = D3DXCommonEncoder::InitBlockingSync(m_pmfxCore);
    MFX_CHECK_STS(sts);
#endif

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
    case D3DDDIFMT_INTELENCODE_MBSEGMENTMAP:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBSEGMENTMAP;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    case D3DDDIFMT_INTELENCODE_SYNCOBJECT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SYNCOBJECT;
#endif
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
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryCompBufferInfo");

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

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)

mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Register");

    std::vector<mfxHDLPair> & queue = (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA) ? m_bsQueue :
        (type == D3DDDIFMT_INTELENCODE_MBSEGMENTMAP) ? m_segmapQueue : m_reconQueue;

    queue.resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxHDLPair handlePair = {};

        mfxStatus sts = m_pmfxCore->GetFrameHDL(response.mids[i], (mfxHDL*)&handlePair);
        MFX_CHECK_STS(sts);

        queue[i] = handlePair;
    }

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
        if (m_bIsBlockingTaskSyncEnabled)
        {
            m_EventCache->Init(response.NumFrameActual);
        }
#endif
    }

    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

#define MAX_NUM_COMP_BUFFERS_VP9 6 // SPS, PPS, bitstream, uncompressed header, segment map, per-segment parameters

mfxStatus D3D11Encoder::Execute(
    Task const & task,
    mfxHDLPair   pair)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Execute");

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(MAX_NUM_COMP_BUFFERS_VP9);

    ID3D11Resource * surface = static_cast<ID3D11Resource *>(pair.first);
    UINT subResourceIndex = (UINT)(UINT_PTR)(pair.second);

    // prepare resource list
    // it contains resources in video memory that needed for the encoding operation
    mfxU32       RES_ID_BITSTREAM = 0;          // bitstream surface takes first place in resourceList
    mfxU32       RES_ID_ORIGINAL  = 1;
    mfxU32       RES_ID_REFERENCE = 2;          // then goes all reference frames from dpb
    mfxU32       RES_ID_RECONSTRUCT = RES_ID_REFERENCE + task.m_pRecFrame->idInPool;
    mfxU32       RES_ID_SEGMAP = RES_ID_REFERENCE + static_cast<mfxU32>(m_reconQueue.size());

    mfxU32 resourceCount = mfxU32(RES_ID_REFERENCE + m_reconQueue.size());
    if (task.m_frameParam.segmentation == APP_SEGMENTATION)
    {
        resourceCount++;
    }
    std::vector<ID3D11Resource*> resourceList;
    resourceList.resize(resourceCount);

    resourceList[RES_ID_BITSTREAM] = static_cast<ID3D11Resource *>(m_bsQueue[task.m_pOutBs->idInPool].first);
    resourceList[RES_ID_ORIGINAL] = surface;

    for (mfxU32 i = 0; i < m_reconQueue.size(); i++)
    {
        resourceList[RES_ID_REFERENCE + i] = static_cast<ID3D11Resource *>(m_reconQueue[i].first);
    }

    if (task.m_frameParam.segmentation == APP_SEGMENTATION)
    {
        resourceList[RES_ID_SEGMAP] = static_cast<ID3D11Resource *>(m_segmapQueue[task.m_pSegmentMap->idInPool].first);
    }

    // [1]. buffers in system memory (configuration buffers)
    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode = 0;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
#else
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 1;
#endif

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    const VP9MfxVideoParam& curMfxPar = *task.m_pParam;

    FillSpsBuffer(curMfxPar, m_caps, m_sps, task, static_cast<mfxU16>(m_width), static_cast<mfxU16>(m_height));

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA);
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

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    compBufferDesc[bufCnt].pCompBuffer = &m_pps;

    ENCODE_INPUT_DESC encodeInputDesc;
    encodeInputDesc.ArraSliceOriginal = subResourceIndex;
    encodeInputDesc.IndexOriginal = RES_ID_ORIGINAL;
    encodeInputDesc.ArraySliceRecon = (UINT)(size_t(m_reconQueue[task.m_pRecFrame->idInPool].second));
    encodeInputDesc.IndexRecon = RES_ID_RECONSTRUCT;
    compBufferDesc[bufCnt].pReserved = &encodeInputDesc;
    bufCnt++;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(RES_ID_BITSTREAM));
    compBufferDesc[bufCnt].pCompBuffer = &RES_ID_BITSTREAM;
    bufCnt++;

    if (task.m_frameParam.segmentation == APP_SEGMENTATION)
    {
        mfxStatus sts = FillSegmentMap(task, m_pmfxCore);
        MFX_CHECK_STS(sts);

        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBSEGMENTMAP);
        compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(RES_ID_SEGMAP));
        compBufferDesc[bufCnt].pCompBuffer = &RES_ID_SEGMAP;
        bufCnt++;

        FillSegmentParam(task, m_seg);
        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBSEGMENTMAPPARMS);
        compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_seg));
        compBufferDesc[bufCnt].pCompBuffer = &m_seg;
        bufCnt++;
    }

    m_descForFrameHeader = MakePackedByteBuffer(pBuf, bytesWritten);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer = &m_descForFrameHeader;
    bufCnt++;

    assert(bufCnt <= compBufferDesc.size());

    HRESULT hr;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    // allocate the event
    if (m_bIsBlockingTaskSyncEnabled)
    {
        Task & task1 = const_cast<Task &>(task);
        task1.m_GpuEvent.m_gpuComponentId = GPU_COMPONENT_ENCODE;
        m_EventCache->GetEvent(task1.m_GpuEvent.gpuSyncEvent);

        D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
        decoderExtParams.Function = DXVA2_SET_GPU_TASK_EVENT_HANDLE;
        decoderExtParams.pPrivateInputData = &task1.m_GpuEvent;
        decoderExtParams.PrivateInputDataSize = sizeof(task1.m_GpuEvent);
        decoderExtParams.pPrivateOutputData = NULL;
        decoderExtParams.PrivateOutputDataSize = 0;
        decoderExtParams.ResourceCount = 0;
        decoderExtParams.ppResourceList = NULL;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "SendGpuEventHandle");
            hr = m_pVContext->DecoderExtension(m_pVDecoder, &decoderExtParams);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
#endif

    try
    {
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
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL,"FATAL: error status from the driver on DecoderExtension(): ", "%x", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
            hr = m_pVContext->DecoderEndFrame(m_pVDecoder);
        }
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "FATAL: error status from the driver on DecoderEndFrame(): ", "%x", hr);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FATAL: exception from the driver on executing task!\n");
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D11Encoder::QueryStatusAsync(
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

            D3D11_VIDEO_DECODER_EXTENSION ext = {};

            ext.Function = ENCODE_QUERY_STATUS_ID;
            ext.pPrivateInputData = &feedbackDescr;
            ext.PrivateInputDataSize = sizeof(feedbackDescr);
            ext.pPrivateOutputData = &m_feedbackUpdate[0];
            ext.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));

            HRESULT hr = m_pVContext->DecoderExtension(m_pVDecoder, &ext);

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            if (FAILED(hr))
            {
                MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "FATAL: error status from the driver received on quering task status: ", "%x", hr);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FATAL: exception from the driver on quering task status!\n");
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
} // mfxStatus D3D11Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D11Encoder::Destroy()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Destroy");

    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::Destroy()
} // MfxHwVP9Encode

#endif // (MFX_VA_WIN)
