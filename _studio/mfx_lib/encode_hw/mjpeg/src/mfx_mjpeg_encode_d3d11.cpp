/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder: support of DXVA D3D11 driver
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_D3D11_ENABLED)

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
, m_pDdiData(0)
, m_counter(0)
, m_pVideoDevice(0)
, m_pVideoContext(0)
, m_pDecoder(0)
, m_infoQueried(false)
, m_pVDOView(0)
{
}

D3D11Encoder::~D3D11Encoder()
{
    Destroy();
}

mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);

    MFX_CHECK_NULL_PTR1(pD3d11);

    mfxStatus sts = Init(
        guid,
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

mfxStatus D3D11Encoder::Reset(mfxVideoParam const & par)
{
    mfxStatus sts;

    if (!m_pDdiData)
    {
        m_pDdiData = new MfxHwMJpegEncode::ExecuteBuffers;
    }
  
    sts = m_pDdiData->Init(&par);
    MFX_CHECK_STS(sts);

    m_counter = 0;
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    type;

    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount = 0;
        
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
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        //D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
        decoderExtParam.Function = ENCODE_FORMATS_ID; 
        decoderExtParam.pPrivateInputData = 0; //m_guid ???
        decoderExtParam.PrivateInputDataSize = 0; // sizeof(m_guid) ???
        decoderExtParam.pPrivateOutputData = &encodeFormats;
        decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);
        decoderExtParam.ResourceCount = 0;
        decoderExtParam.ppResourceList = 0;

        //hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
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

    // FIXME: !!! aya:  
    // D3D11_BIND_VIDEO_ENCODER must be used core->AllocFrames()
    //     Desc.BindFlags = D3D11_BIND_ENCODER;
    //     hr = pSelf->m_pD11Device->CreateTexture2D(&Desc, NULL, &pSelf->m_SrfPool);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS_JPEG & caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);
    caps = m_caps;
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);
    //caps = m_capsQuery;
    caps;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D11Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);
    //m_capsGet = caps; // DDI spec: "The application should use the same structure
    //                  // returned in a previous ENCODE_ENC_CTRL_GET_ID command."
    //HRESULT hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_SET_ID, &m_capsGet, sizeof(m_capsGet));
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    caps;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D11Encoder::Register(mfxMemId /*memId*/, D3DDDIFORMAT /*type*/)
{
    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::Register(...)

mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    m_bsQueue.resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {   
        mfxHDLPair handlePair;

        //mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);
        
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);
        MFX_CHECK_STS(sts);

        m_bsQueue[i] = handlePair;
    }

    if( type != D3DDDIFMT_NV12 )
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Execute(DdiTask &task, mfxHDL surface)
{
    HRESULT hr  = S_OK;
    mfxHDLPair* inputPair = static_cast<mfxHDLPair*>(surface);//aya: has to be corrected
    ID3D11Resource* pInputD3D11Res = static_cast<ID3D11Resource*>(inputPair->first);
    ExecuteBuffers* pExecuteBuffers = m_pDdiData;

    // BeginFrame()
    //{ ---------------------------------------------------------
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC outputDesc;
        outputDesc.DecodeProfile = m_guid;
        outputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2D.ArraySlice = (UINT)0; 
        SAFE_RELEASE(m_pVDOView);

        hr = m_pVideoDevice->CreateVideoDecoderOutputView(
            pInputD3D11Res, 
            &outputDesc,
            &m_pVDOView);
        CHECK_HRES(hr);
        
        hr = m_pVideoContext->DecoderBeginFrame(m_pDecoder, m_pVDOView, 0, 0);
        CHECK_HRES(hr);    
    //}

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
        (pExecuteBuffers->m_pps.NumScan ? 1 : 0);
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

    task.m_statusReportNumber = (m_counter++);// << 8;
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
    
    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_HUFFTBLDATA);
    encodeCompBufferDesc[bufCnt].DataSize = 0;
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dht_list[0];
    for (i = 0; i < pExecuteBuffers->m_pps.NumCodingTable; i++)
    {
        encodeCompBufferDesc[bufCnt].DataSize += mfxU32(sizeof(pExecuteBuffers->m_dht_list[i]));
    }
    bufCnt++;

    for (i = 0; i < pExecuteBuffers->m_pps.NumScan; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA);
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_scan_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_scan_list[i];
        bufCnt++;
    }

    //if (pExecuteBuffers->m_payload_data_present)
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PAYLOADDATA);
    //    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(pExecuteBuffers->m_payload.size);
    //    encodeCompBufferDesc[bufCnt].pCompBuffer = (void*)pExecuteBuffers->m_payload.data;
    //    bufCnt++;
    //}
    
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

    hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
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

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };

            decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;
            decoderExtParams.pPrivateInputData     = 0;
            decoderExtParams.PrivateInputDataSize  = 0;
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

    IppStatus sts = ippiCopyManaged_8u_C1R(
        (Ipp8u *)bitstream.Y, task.m_bsDataLength,
        bsData, task.m_bsDataLength,
        roi, IPP_NONTEMPORAL_LOAD);
    assert(sts == ippStsNoErr);

    task.bs->DataLength += task.m_bsDataLength;
    m_core->UnlockFrame(MemId, &bitstream);

    return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Destroy()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pDdiData)
    {
        m_pDdiData->Close();
        delete m_pDdiData;
        m_pDdiData = 0;
    }

    SAFE_RELEASE(m_pDecoder);
    SAFE_RELEASE(m_pVDOView);

    return sts;
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

    D3D11_VIDEO_DECODER_CONFIG video_config = {0}; // aya:!!!!!!!!
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

    // [2] Calling other D3D11 Video Decoder API (as for normal proc) - aya:FIXME:skipped

    //hRes = CheckVideoDecoderFormat(NV12); //aya???
    //CHECK_HRES(hRes);

    // [4] CreateVideoDecoder
    // D3D11_VIDEO_DECODER_DESC video_desc;
    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_JPEG; 

    // D3D11_VIDEO_DECODER_CONFIG video_config;
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;// aya: encrypto will be added late
    video_config.ConfigDecoderSpecific = ENCODE_PAK;

    hRes  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    CHECK_HRES(hRes);

#if 1
    // [3] Query the encoding device capabilities 
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
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

    // [5] Set encryption - aya:skipped

    // [6] specific encoder caps - aya:skipped

    // [7] Query encode service caps: see QueryCompBufferInfo


    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Init(...)

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_D3D11_ENABLED)
/* EOF */
