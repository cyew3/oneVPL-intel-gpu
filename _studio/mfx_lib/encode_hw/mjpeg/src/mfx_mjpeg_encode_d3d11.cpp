//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN) && defined (MFX_D3D11_ENABLED)

#define CHECK_HRES(hRes) \
        if (FAILED(hRes))\
            return MFX_ERR_DEVICE_FAILED;

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"

#include "mfx_mjpeg_encode_d3d11.h"
#include "libmfx_core_interface.h"

#include "mfx_mjpeg_encode_hw_utils.h"
#include "fast_copy.h"

using namespace MfxHwMJpegEncode;

mfxU16 ownConvertD3DFMT_TO_MFX(DXGI_FORMAT format);

namespace
{
    HRESULT DecoderExtension(
        ID3D11VideoContext *            context,
        ID3D11VideoDecoder *            decoder,
        D3D11_VIDEO_DECODER_EXTENSION & param)
    {
#ifdef DEBUG
        printf("\rDecoderExtension: context=%p decoder=%p function=%d\n", context, decoder, param.Function); fflush(stdout);
#endif 
        HRESULT hr = context->DecoderExtension(decoder, &param);
#ifdef DEBUG
        printf("\rDecoderExtension: hresult=%d\n", hr); fflush(stdout);
#endif 
        return hr;
    }
};

D3D11Encoder::D3D11Encoder()
: m_core(0)
, m_pVideoDevice(0)
, m_pVideoContext(0)
, m_pDecoder(0)
, m_infoQueried(false)
, m_guid(GUID_NULL)
, m_width(0)
, m_height(0)
{
    memset(&m_caps, 0, sizeof(m_caps));
}

D3D11Encoder::~D3D11Encoder()
{
    Destroy();
}

mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);

    MFX_CHECK_NULL_PTR1(pD3d11);

    mfxStatus sts = Init(
        DXVA2_Intel_Encode_JPEG,
        pD3d11->GetD3D11VideoDevice(isTemporal), 
        pD3d11->GetD3D11VideoContext(isTemporal), 
        width,
        height);

    m_core = core;
    return sts;
}

mfxStatus D3D11Encoder::CreateAccelerationService(mfxVideoParam const & par)
{
    par;

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryBitstreamBufferInfo(mfxFrameAllocRequest& request)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount   = 0;
        
        //HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
        D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
        decoderExtParam.Function = ENCODE_FORMAT_COUNT_ID;
        decoderExtParam.pPrivateInputData = 0; //m_guid ???
        decoderExtParam.PrivateInputDataSize = 0; // sizeof(m_guid) ???
        decoderExtParam.pPrivateOutputData = &encodeFormatCount;
        decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);
        decoderExtParam.ResourceCount = 0;
        decoderExtParam.ppResourceList = 0;

        HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
        CHECK_HRES(hRes);
        //---------------------------------------------------

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize   = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo    = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats     = &m_uncompBufInfo[0];

        //D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
        decoderExtParam.Function = ENCODE_FORMATS_ID;
        decoderExtParam.pPrivateInputData = 0; //m_guid ???
        decoderExtParam.PrivateInputDataSize = 0; // sizeof(m_guid) ???
        decoderExtParam.pPrivateOutputData = &encodeFormats;
        decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);
        decoderExtParam.ResourceCount = 0;
        decoderExtParam.ppResourceList = 0;

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
        CHECK_HRES(hRes);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t bitstreamIndex = 0;
    for (size_t i = 0; i < m_compBufInfo.size(); i++)
    {
        switch((D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE)m_compBufInfo[i].Type)
        {
        case D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA:
            break;
        case D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA:
            break;
        case D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA:
            break;
        case D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA:
            bitstreamIndex = i;
            break;
        case D3D11_DDI_VIDEO_ENCODER_BUFFER_HUFFTBLDATA:
            break;
        default:
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    request.Info.Width  = m_compBufInfo[bitstreamIndex].CreationWidth;
    request.Info.Height = m_compBufInfo[bitstreamIndex].CreationHeight;
    request.Info.FourCC = ownConvertD3DFMT_TO_MFX( (DXGI_FORMAT)(m_compBufInfo[bitstreamIndex].CompressedFormats) ); // P8

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryEncodeCaps(JpegEncCaps & caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    caps.Baseline = m_caps.Baseline;
    caps.Sequential = m_caps.Sequential;
    caps.Huffman = m_caps.Huffman;

    caps.NonInterleaved = m_caps.NonInterleaved;
    caps.Interleaved = m_caps.Interleaved;

    caps.MaxPicWidth = m_caps.MaxPicWidth;
    caps.MaxPicHeight = m_caps.MaxPicHeight;

    caps.SampleBitDepth = m_caps.SampleBitDepth;
    caps.MaxNumComponent = m_caps.MaxNumComponent;
    caps.MaxNumScan = m_caps.MaxNumScan;
    caps.MaxNumHuffTable = m_caps.MaxNumHuffTable;
    caps.MaxNumQuantTable = m_caps.MaxNumQuantTable;

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::RegisterBitstreamBuffer(mfxFrameAllocResponse& response)
{
    m_bsQueue.resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {   
        mfxHDLPair handlePair;

        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);
        MFX_CHECK_STS(sts);

        m_bsQueue[i] = handlePair;
    }

    // Reserve space for feedback reports.
    m_feedbackUpdate.resize(response.NumFrameActual);
    m_feedbackCached.Reset(response.NumFrameActual);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Execute(DdiTask &task, mfxHDL surface)
{
    HRESULT hr  = S_OK;
    mfxHDLPair* inputPair = static_cast<mfxHDLPair*>(surface);
    ID3D11Resource* pInputD3D11Res = static_cast<ID3D11Resource*>(inputPair->first);
    ExecuteBuffers* pExecuteBuffers = task.m_pDdiData;
    // prepare resource list
    // it contains resources in video memory that needed for the encoding operation
    mfxU32       RES_ID_BITSTREAM   = 0;          // bitstream surface takes first place in resourceList
    mfxU32       RES_ID_ORIGINAL    = 1;

    mfxU32 resourceCount = 2;
    std::vector<ID3D11Resource*> resourceList;
    resourceList.resize(resourceCount);

    resourceList[RES_ID_BITSTREAM]   = static_cast<ID3D11Resource*>(m_bsQueue[task.m_idxBS].first);
    resourceList[RES_ID_ORIGINAL]    = pInputD3D11Res;

    // [1]. buffers in system memory (configuration buffers)    
    //const mfxU32 NUM_COMP_BUFFER = 16;
    //ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NUM_COMP_BUFFER];
    mfxU32 compBufferCount = 2 + 
        (pExecuteBuffers->m_pps.NumQuantTable ? 1 : 0) + 
        (pExecuteBuffers->m_pps.NumCodingTable ? 1 : 0) + 
        (pExecuteBuffers->m_pps.NumScan ? 1 : 0) + 
        (pExecuteBuffers->m_payload_list.size() ? 1 : 0);
    std::vector<ENCODE_COMPBUFFERDESC>  encodeCompBufferDesc;
    encodeCompBufferDesc.resize(compBufferCount);
    memset(&encodeCompBufferDesc[0], 0, sizeof(ENCODE_COMPBUFFERDESC) * compBufferCount);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    memset(&encodeExecuteParams, 0, sizeof(ENCODE_EXECUTE_PARAMS));
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc[0];

    // FIXME: need this until production driver moves to DDI 0.87
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    pExecuteBuffers->m_pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_pps));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_pps;
    ENCODE_INPUT_DESC encodeInputDesc;
    encodeInputDesc.ArraSliceOriginal = (UINT)((size_t)inputPair->second);
    encodeInputDesc.IndexOriginal     = RES_ID_ORIGINAL;
    encodeInputDesc.ArraySliceRecon   = 0;
    encodeInputDesc.IndexRecon        = 0;
    encodeCompBufferDesc[bufCnt].pReserved = &encodeInputDesc;
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    encodeCompBufferDesc[bufCnt].DataSize = RES_ID_BITSTREAM;
    encodeCompBufferDesc[bufCnt].pCompBuffer = &RES_ID_BITSTREAM;
    bufCnt++;

    mfxU16 i = 0;

    if(pExecuteBuffers->m_pps.NumQuantTable)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA);
        encodeCompBufferDesc[bufCnt].DataSize = 0;
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dqt_list[0];
        for (i = 0; i < pExecuteBuffers->m_pps.NumQuantTable; i++)
        {
            encodeCompBufferDesc[bufCnt].DataSize += mfxU32(sizeof(pExecuteBuffers->m_dqt_list[i]));
        }
        bufCnt++;
    }

    if(pExecuteBuffers->m_pps.NumCodingTable)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_HUFFTBLDATA);
        encodeCompBufferDesc[bufCnt].DataSize = 0;
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dht_list[0];
        for (i = 0; i < pExecuteBuffers->m_pps.NumCodingTable; i++)
        {
            encodeCompBufferDesc[bufCnt].DataSize += mfxU32(sizeof(pExecuteBuffers->m_dht_list[i]));
        }
        bufCnt++;
    }

    //single interleaved scans only are supported
    for (i = 0; i < pExecuteBuffers->m_pps.NumScan; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA);
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_scan_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_scan_list[i];
        bufCnt++;
    }

    if (pExecuteBuffers->m_payload_list.size())
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PAYLOADDATA);
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(pExecuteBuffers->m_payload_base.length);
        encodeCompBufferDesc[bufCnt].pCompBuffer = (void*)pExecuteBuffers->m_payload_base.data;
        bufCnt++;
    }
    
    // [2.4] send to driver
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
    decoderExtParams.Function              = ENCODE_ENC_PAK_ID;
    decoderExtParams.pPrivateInputData     = &encodeExecuteParams;
    decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
    decoderExtParams.pPrivateOutputData    = 0;
    decoderExtParams.PrivateOutputDataSize = 0;
    decoderExtParams.ResourceCount         = resourceCount;
    decoderExtParams.ppResourceList        = &resourceList[0];

    hr = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
    CHECK_HRES(hr);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryStatus(DdiTask & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses. 

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

#ifdef NEW_STATUS_REPORTING_DDI_0915
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };

            decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;
#ifdef NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateInputData     = &feedbackDescr;
            decoderExtParams.PrivateInputDataSize  = sizeof(feedbackDescr);
#else // NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateInputData     = 0;
            decoderExtParams.PrivateInputDataSize  = 0;
#endif // NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateOutputData    = &m_feedbackUpdate[0];
            decoderExtParams.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
            decoderExtParams.ResourceCount         = 0;
            decoderExtParams.ppResourceList        = 0;

            HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);

            MFX_CHECK(hRes != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D11Encoder::UpdateBitstream(
    mfxMemId       MemId,
    DdiTask      & task)
{
    mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    IppiSize     roi       = {task.m_bsDataLength, 1};
    mfxFrameData bitstream = {0};

    m_core->LockFrame(MemId, &bitstream);
    MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    mfxStatus sts = FastCopy::Copy(
        bsData, task.m_bsDataLength,
        (Ipp8u *)bitstream.Y, task.m_bsDataLength,
        roi, COPY_VIDEO_TO_SYS);
    assert(sts == MFX_ERR_NONE);

    task.bs->DataLength += task.m_bsDataLength;
    m_core->UnlockFrame(MemId, &bitstream);

    return sts;
}

mfxStatus D3D11Encoder::Destroy()
{
    SAFE_RELEASE(m_pDecoder);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Init(
    GUID guid,
    ID3D11VideoDevice *pVideoDevice, 
    ID3D11VideoContext *pVideoContext,
    mfxU32      width,
    mfxU32      height)
{
    MFX_CHECK_NULL_PTR2(pVideoDevice, pVideoContext);

    m_pVideoDevice  = pVideoDevice;
    m_pVideoContext = pVideoContext;
    m_guid = guid;

    HRESULT hRes;

    // [1] Query supported decode profiles
    UINT profileCount = m_pVideoDevice->GetVideoDecoderProfileCount( );
    assert( profileCount > 0 );

    bool isFound = false;
    GUID profileGuid;
    for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
    {
        hRes = m_pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        CHECK_HRES(hRes);
        if( guid == profileGuid )
        {
            isFound = true;
            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // [2] Query the supported encode functions
    D3D11_VIDEO_DECODER_DESC video_desc;
    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_JPEG;

    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    mfxU32 count;

    hRes = m_pVideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
    CHECK_HRES(hRes);

    //for (int i = 0; i < count; i++)
    //{               
    //    hRes = m_pVideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
    //    CHECK_HRES(hRes);

        // mfxSts = CheckDXVAConfig(video_desc->Guid, pConfig));
        // MFX_CHECK_STS( mfxSts );
    //}    

    // [2] Calling other D3D11 Video Decoder API (as for normal proc)

    // [4] CreateVideoDecoder
    // D3D11_VIDEO_DECODER_DESC video_desc;
    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_JPEG;

    // D3D11_VIDEO_DECODER_CONFIG video_config;
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
    video_config.ConfigDecoderSpecific = ENCODE_PAK;

    hRes  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    CHECK_HRES(hRes);

#if 1
    // [3] Query the encoding device capabilities 
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
    memset(&m_caps, 0, sizeof(m_caps));
    decoderExtParam.Function = 0x110; //ENCODE_QUERY_ACCEL_CAPS_ID = 0x110;
    decoderExtParam.pPrivateInputData = 0;
    decoderExtParam.PrivateInputDataSize = 0;
    decoderExtParam.pPrivateOutputData = &m_caps;
    decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_CAPS_JPEG);
    decoderExtParam.ResourceCount = 0;
    decoderExtParam.ppResourceList = 0;

    hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
    CHECK_HRES(hRes);
#endif

    // [5] Set encryption

    // [6] specific encoder caps


    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Init(...)

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN) && defined (MFX_D3D11_ENABLED)
