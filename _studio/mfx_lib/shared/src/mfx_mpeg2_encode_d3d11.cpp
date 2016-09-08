/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          MPEG2 encoder
//
*/

#include "mfx_common.h"

#if defined(MFX_VA) && defined(MFX_D3D11_ENABLED)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENC)|| defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include <atlbase.h>
#include "assert.h"

#include "mfx_mpeg2_encode_d3d11.h"
#include "libmfx_core_interface.h"
#include "vm_time.h"




// aya: temporal solution
#ifndef D3DDDIFMT_NV12
#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MAKEFOURCC('N', 'V', '1', '2'))
#endif

#ifndef MFX_CHECK_WITH_ASSERT
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#endif

    template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
    template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }

#define CHECK_HRES(hRes) \
        if (FAILED(hRes))\
            return MFX_ERR_DEVICE_FAILED;

using namespace MfxHwMpeg2Encode;

enum 
{
    MFX_FOURCC_P8_MBDATA = MFX_MAKEFOURCC('P','8','M','B'),
};


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

D3D11Encoder::D3D11Encoder(VideoCORE* core) 
: m_core(core) 
, m_pVideoDevice(0)
, m_pVideoContext(0)
, m_pDecoder(0)
, m_pVDOView(NULL)
, m_infoQueried(false)
, m_bENC_PAK(false)
, m_guid()
, m_layout()
, m_reconFrames()
, m_feedback()
{
    memset (&m_allocResponseMB,0,sizeof(mfxFrameAllocResponse));
    memset (&m_caps,0,sizeof(ENCODE_CAPS));
    memset (&m_allocResponseBS,0,sizeof(mfxFrameAllocResponse));
    //memset (&m_recFrames,0,sizeof(mfxRecFrames));
    //m_pDevice = 0;
    memset (&m_rawFrames,0,sizeof(mfxRawFrames));
    m_bENC_PAK = false;

} // D3D11Encoder::D3D11Encoder(VideoCORE* core) : m_core(core)



D3D11Encoder::~D3D11Encoder()
{
    Close();

    for (size_t i = 0; i < m_allocResponseMB.NumFrameActual; i++)
    {
        m_core->FreeFrames( &m_realResponseMB[i] );
    }
    for (size_t i = 0; i < m_allocResponseBS.NumFrameActual; i++)
    {
        m_core->FreeFrames( &m_realResponseBS[i] );
    }

    memset (&m_allocResponseMB,0,sizeof(mfxFrameAllocResponse));
    memset (&m_allocResponseBS,0,sizeof(mfxFrameAllocResponse));
} // D3D11Encoder::~D3D11Encoder()


mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS & caps)
{
    MFX_CHECK_NULL_PTR1(m_core);    

    if(!m_pDecoder)
    {
        D3D11Interface* pD3d11     = QueryCoreInterface<D3D11Interface>(m_core);                         
        ID3D11VideoDevice* pDevice = pD3d11->GetD3D11VideoDevice(true);
        HRESULT hRes;

        // Query supported decode profiles
        UINT profileCount = pDevice->GetVideoDecoderProfileCount();
        assert( profileCount > 0 );

        bool isProfileFound = false;    
        GUID profileGuid;
        for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
        {
            hRes = pDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
            CHECK_HRES(hRes);
            if( DXVA2_Intel_Encode_MPEG2 == profileGuid )
            {
                isProfileFound = true;
                break;
            }
        }

        if( !isProfileFound )
        {
            return MFX_ERR_UNSUPPORTED;
        }

        D3D11_VIDEO_DECODER_DESC video_desc;
        video_desc.SampleWidth  = 720;//aya: fake
        video_desc.SampleHeight = 576;//aya: fake
        video_desc.OutputFormat = DXGI_FORMAT_NV12;
        video_desc.Guid = DXVA2_Intel_Encode_MPEG2; //aya:??? guid

        UINT configCount = 0;

        hRes = pDevice->GetVideoDecoderConfigCount(&video_desc, &configCount);
        CHECK_HRES(hRes);

        bool isMpeg2ConfigFound = false;
        D3D11_VIDEO_DECODER_CONFIG video_config;

        for (UINT i = 0; i < configCount; i++)
        {               
            hRes = pDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
            CHECK_HRES(hRes);

            if( (ENCODE_ENC & video_config.ConfigDecoderSpecific) ||
                (ENCODE_ENC_PAK & video_config.ConfigDecoderSpecific))
            {
                isMpeg2ConfigFound = true;
                break;
            }
        }

        if(!isMpeg2ConfigFound)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        
        CComPtr<ID3D11VideoDecoder> pDecoder;        
        hRes = pDevice->CreateVideoDecoder(&video_desc, &video_config, &pDecoder);
        CHECK_HRES(hRes);

        // [3] Query the encoding device capabilities 
        D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
        decoderExtParam.Function = 0x110; //ENCODE_QUERY_ACCEL_CAPS_ID = 0x110;
        decoderExtParam.pPrivateInputData = 0;
        decoderExtParam.PrivateInputDataSize = 0;
        decoderExtParam.pPrivateOutputData = &m_caps;
        decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_CAPS);
        decoderExtParam.ResourceCount = 0;
        decoderExtParam.ppResourceList = 0;

        hRes = DecoderExtension(pD3d11->GetD3D11VideoContext(true), pDecoder, decoderExtParam);
        CHECK_HRES(hRes);        
    }

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS & caps)


mfxStatus D3D11Encoder::Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)
{
    mfxStatus   sts    = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(m_core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_core);

    MFX_CHECK_NULL_PTR1(pD3d11);
   
    sts = Init(DXVA2_Intel_Encode_MPEG2,
            ENCODE_ENC,
            pD3d11->GetD3D11VideoDevice(), 
            pD3d11->GetD3D11VideoContext(),
            pExecuteBuffers);

    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    sts = CreateMBDataBuffer(numRefFrames);
    MFX_CHECK_STS(sts);
    
    // aya
    //sts = CreateCompBuffers(pExecuteBuffers,numRefFrames);
    //MFX_CHECK_STS(sts);

    sts = QueryMbDataLayout();
    MFX_CHECK_STS(sts);

    m_bENC_PAK = false;

    return sts;

} // mfxStatus D3D11Encoder::Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)


mfxStatus D3D11Encoder::Init_MPEG2_ENCODE(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)
{
    mfxStatus   sts    = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(m_core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_core);

    MFX_CHECK_NULL_PTR1(pD3d11);

    sts = Init(DXVA2_Intel_Encode_MPEG2, 
            ENCODE_ENC_PAK, 
            pD3d11->GetD3D11VideoDevice(), 
            pD3d11->GetD3D11VideoContext(),
            pExecuteBuffers);

    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    sts = CreateBSBuffer(numRefFrames);
    MFX_CHECK_STS(sts);

    m_bENC_PAK = true;

    return sts;

} // mfxStatus D3D11Encoder::Init_MPEG2_ENCODE(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)


mfxStatus D3D11Encoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED; 

    if( ENCODE_ENC_PAK_ID == funcId )
    {
        sts = Init_MPEG2_ENCODE(pExecuteBuffers, numRefFrames);
        m_bENC_PAK = true;
    }
    else if( ENCODE_ENC_ID == funcId )
    {
        sts = Init_MPEG2_ENC(pExecuteBuffers, numRefFrames);
        m_bENC_PAK = false;
    }

    return sts;

} // mfxStatus D3D11Encoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)


mfxStatus D3D11Encoder::Init(
    GUID guid,
    ENCODE_FUNC func,
    ID3D11VideoDevice *pVideoDevice, 
    ID3D11VideoContext *pVideoContext,
    ExecuteBuffers* pExecuteBuffers)
{
    MFX_CHECK_NULL_PTR2(pVideoDevice, pVideoContext);

    m_pVideoDevice  = pVideoDevice;
    m_pVideoContext = pVideoContext;
    m_guid = guid;

    HRESULT hRes;

    m_feedback.Reset();

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
    video_desc.SampleWidth  = ((pExecuteBuffers->m_sps.FrameWidth +15)>>4)<<4;
    video_desc.SampleHeight = ((pExecuteBuffers->m_sps.FrameHeight+15)>>4)<<4;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_MPEG2; //aya:??? guid

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
    video_desc.SampleWidth  = pExecuteBuffers->m_sps.FrameWidth;
    video_desc.SampleHeight = pExecuteBuffers->m_sps.FrameHeight;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_MPEG2; 

    // D3D11_VIDEO_DECODER_CONFIG video_config;
#ifdef PAVP_SUPPORT
    video_config.guidConfigBitstreamEncryption = (pExecuteBuffers->m_encrypt.m_bEncryptionMode) ? DXVA2_INTEL_PAVP : DXVA_NoEncrypt;
#else    
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
#endif
    video_config.ConfigDecoderSpecific = (USHORT)func;//ENCODE_ENC_PAK;

    hRes  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    CHECK_HRES(hRes);

#if 1
    // [3] Query the encoding device capabilities 
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
    decoderExtParam.Function = 0x110; //ENCODE_QUERY_ACCEL_CAPS_ID = 0x110;
    decoderExtParam.pPrivateInputData = 0;
    decoderExtParam.PrivateInputDataSize = 0;
    decoderExtParam.pPrivateOutputData = &m_caps;
    decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_CAPS);
    decoderExtParam.ResourceCount = 0;
    decoderExtParam.ppResourceList = 0;

    hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
    CHECK_HRES(hRes);
#endif    

#ifdef PAVP_SUPPORT
    if (pExecuteBuffers->m_encrypt.m_bEncryptionMode)
    {
        D3D11_AES_CTR_IV      initialCounter    = {pExecuteBuffers->m_encrypt.m_InitialCounter.IV,pExecuteBuffers->m_encrypt.m_InitialCounter.Count};
        PAVP_ENCRYPTION_MODE  encryptionMode    = pExecuteBuffers->m_encrypt.m_PavpEncryptionMode;
        ENCODE_ENCRYPTION_SET encryptSet        = {pExecuteBuffers->m_encrypt.m_CounterAutoIncrement, &initialCounter, &encryptionMode};

        D3D11_VIDEO_DECODER_EXTENSION encryptParam;
        encryptParam.Function = ENCODE_ENCRYPTION_SET_ID; //ENCODE_QUERY_ACCEL_CAPS_ID = 0x110;
        encryptParam.pPrivateInputData     = &encryptSet;
        encryptParam.PrivateInputDataSize  = sizeof(ENCODE_ENCRYPTION_SET);
        encryptParam.pPrivateOutputData    = 0;
        encryptParam.PrivateOutputDataSize = 0;
        encryptParam.ResourceCount         = 0;
        encryptParam.ppResourceList        = 0;

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, encryptParam);
        CHECK_HRES(hRes);
    }
#endif

 

    // [6] specific encoder caps - aya:skipped

    // [7] Query encode service caps: see QueryCompBufferInfo


    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Init(...)


mfxStatus D3D11Encoder::GetBuffersInfo()
{
    /*HRESULT hr = 0;
    ENCODE_FORMAT_COUNT encodeFormatCount = {0};
    GUID    curr_guid = m_pDevice->GetCurrentGuid();

    hr = m_pDevice->Execute(ENCODE_FORMAT_COUNT_ID, &curr_guid,sizeof(GUID),&encodeFormatCount,sizeof(encodeFormatCount));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    MFX_CHECK(encodeFormatCount.CompressedBufferInfoCount>0 && encodeFormatCount.CompressedBufferInfoCount>0, MFX_ERR_DEVICE_FAILED);

    m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
    m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

    ENCODE_FORMATS encodeFormats = {0};
    encodeFormats.CompressedBufferInfoSize  = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
    encodeFormats.UncompressedFormatSize    = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
    encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
    encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];
      
    hr = m_pDevice->Execute(ENCODE_FORMATS_ID, 0,0,&encodeFormats,sizeof(encodeFormats));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);*/

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

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::GetBuffersInfo() 


mfxStatus D3D11Encoder::QueryMbDataLayout()
{
    /*HRESULT hr = 0;

    memset(&m_layout, 0, sizeof(m_layout));
    hr = m_pDevice->Execute(MBDATA_LAYOUT_ID, 0, 0,&m_layout,sizeof(m_layout));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);*/

    memset(&m_layout, 0, sizeof(m_layout));

    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
    decoderExtParam.Function = MBDATA_LAYOUT_ID; 
    decoderExtParam.pPrivateInputData = 0; //m_guid ???
    decoderExtParam.PrivateInputDataSize = 0; // sizeof(m_guid) ???
    decoderExtParam.pPrivateOutputData = &m_layout;
    decoderExtParam.PrivateOutputDataSize = sizeof(ENCODE_MBDATA_LAYOUT);
    decoderExtParam.ResourceCount = 0;
    decoderExtParam.ppResourceList = 0;

    HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
    CHECK_HRES(hRes);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryMbDataLayout()


mfxStatus D3D11Encoder::QueryCompBufferInfo(D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type, mfxFrameAllocRequest* pRequest)
{
    type; pRequest;

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }
    if (i == m_compBufInfo.size())
        return MFX_ERR_UNSUPPORTED;

    pRequest->Info.Width = m_compBufInfo[i].CreationWidth;
    pRequest->Info.Height = m_compBufInfo[i].CreationHeight;
    pRequest->Info.FourCC = MFX_FOURCC_NV12;
    pRequest->NumFrameMin = m_compBufInfo[i].NumBuffer;
    pRequest->NumFrameSuggested = m_compBufInfo[i].NumBuffer;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest)


mfxStatus D3D11Encoder::CreateMBDataBuffer(mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    // aya: d3d11 issue: P8 must be allocated 
    if (m_allocResponseMB.NumFrameActual == 0)
    {
        mfxFrameAllocRequest tmp = request;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;
        // aya: it is wa for d3d11. 
        // because P8 data (bitstream) for h264 encoder should be allocated by CreateBuffer()  
        // but P8 data (MBData) for MPEG2 encoder should be allocated by CreateTexture2D()
        tmp.Info.FourCC = D3DFMT_P8; //MFX_FOURCC_P8_MBDATA;
        tmp.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;

        m_midMB.resize(request.NumFrameMin);
        m_realResponseMB.resize(request.NumFrameMin);

        for (int i = 0; i < request.NumFrameMin; i++)
        {             
            mfxStatus sts = m_core->AllocFrames(&tmp, &m_realResponseMB[i]);
            MFX_CHECK_STS(sts);
            m_midMB[i] = m_realResponseMB[i].mids[0];
        }

        //m_allocResponseMB      = m_realResponseMB[0];
        m_allocResponseMB.mids = &m_midMB[0];
        m_allocResponseMB.NumFrameActual = request.NumFrameMin;
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseMB, D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::CreateMBDataBuffer(mfxU32 numRefFrames)
mfxStatus D3D11Encoder::CreateBSBuffer(mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    sts = QueryCompBufferInfo(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;
    
    if (m_allocResponseBS.NumFrameActual == 0)
    {
        mfxFrameAllocRequest tmp = request;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        tmp.Info.FourCC = D3DFMT_P8;
        tmp.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;

        m_midBS.resize(request.NumFrameMin);
        m_realResponseBS.resize(request.NumFrameMin);

        for (int i = 0; i < request.NumFrameMin; i++)
        {             
            mfxStatus sts = m_core->AllocFrames(&tmp, &m_realResponseBS[i]);
            MFX_CHECK_STS(sts);
            m_midBS[i] = m_realResponseBS[i].mids[0];
        }

        m_allocResponseBS.mids = &m_midBS[0];
        m_allocResponseBS.NumFrameActual = request.NumFrameMin;
    }
    else
    {
        if (m_allocResponseBS.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseBS, D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::CreateMBDataBuffer(mfxU32 numRefFrames)

mfxStatus D3D11Encoder::CreateCompBuffers(
            ExecuteBuffers* /*pExecuteBuffers*/, 
            mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseMB);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }

    sts = Register(&m_allocResponseMB, D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::CreateCompBuffers(...)


mfxStatus D3D11Encoder::Register(
    const mfxFrameAllocResponse* pResponse, 
    D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type)
{
    type;

    // aya: workarround for d3d11
    MFX_CHECK(type == D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA || type == D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA ||type == D3DDDIFMT_NV12, MFX_ERR_NULL_PTR );       

    // we should register allocated HW bitstreams and recon surfaces
    std::vector<mfxHDLPair> & queue = (type == D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA)
        ? m_mbQueue 
        : ((type == D3DDDIFMT_NV12) ? m_reconQueue : m_bsQueue);

    //std::vector<mfxHDLPair> & queue = m_mbQueue;
    
    queue.resize(pResponse->NumFrameActual);

    //aya:wo_d3d11
    for (mfxU32 i = 0; i < pResponse->NumFrameActual; i++)
    {   
        mfxHDLPair handlePair;        
        
        mfxStatus sts = m_core->GetFrameHDL(pResponse->mids[i], (mfxHDL *)&handlePair);
        MFX_CHECK_STS(sts);
                        
        queue[i] = handlePair;
    }

    //if( type != D3DDDIFMT_NV12 )
    //{
    //    // reserved space for feedback reports
    //    m_feedbackUpdate.resize( response.NumFrameActual );
    //    m_feedbackCached.Reset( response.NumFrameActual );
    //}

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Register(...)


mfxI32 D3D11Encoder::GetRecFrameIndex (mfxMemId memID)
{
    for (int i = 0; i <m_reconFrames.NumFrameActual;i++)
    {
        if (m_reconFrames.mids[i] == memID)
            return i;    
    }

    return -1;

} // mfxI32 D3D11Encoder::GetRecFrameIndex (mfxMemId memID)


mfxI32 D3D11Encoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)
{
    for (int i = 0; i <m_rawFrames.NumFrameActual;i++)
    {
        if (m_rawFrames.mids[i] == memID)
            return i;    
    }
    if (bAddFrames && m_rawFrames.NumFrameActual < NUM_FRAMES)
    {
        m_rawFrames.mids[m_rawFrames.NumFrameActual] = memID;
        return m_rawFrames.NumFrameActual++;
    }

    return -1;

} // mfxI32 D3D11Encoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)


mfxStatus D3D11Encoder::SetFrames (ExecuteBuffers* pExecuteBuffers)
{
    pExecuteBuffers;

    mfxStatus sts = MFX_ERR_NONE;

    mfxI32 ind = 0;

    if (pExecuteBuffers->m_RecFrameMemID)
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RecFrameMemID);
    }
    else
    {
        ind = 0xff;
    }

    pExecuteBuffers->m_pps.CurrReconstructedPic.Index7Bits     =  mfxU8(ind < 0 ? 0 : ind);
    pExecuteBuffers->m_pps.CurrReconstructedPic.AssociatedFlag =  0;
    pExecuteBuffers->m_idxMb = (DWORD)ind;
    pExecuteBuffers->m_idxBs = (DWORD)ind;

    if (pExecuteBuffers->m_bUseRawFrames)
    {
        ind = GetRawFrameIndex(pExecuteBuffers->m_CurrFrameMemID,true);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
    }
    //else CurrOriginalPic == CurrReconstructedPic 

    pExecuteBuffers->m_pps.CurrOriginalPic.Index7Bits     =  mfxU8(ind);
    pExecuteBuffers->m_pps.CurrOriginalPic.AssociatedFlag =  0;

    if (pExecuteBuffers->m_RefFrameMemID[0])
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[0]);

        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[0].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[0].AssociatedFlag = 0;
    }
    else
    {
        pExecuteBuffers->m_pps.RefFrameList[0].bPicEntry = 0xff;
    }

    if (pExecuteBuffers->m_RefFrameMemID[1])
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[1]);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[1].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[1].AssociatedFlag = 0;
    }
    else  
    {
        pExecuteBuffers->m_pps.RefFrameList[1].bPicEntry = 0xff;
    }

    if (pExecuteBuffers->m_bExternalCurrFrame)
    //if( true )
    {
        sts = m_core->GetExternalFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurfacePair);
    }
    else
    {
        sts = m_core->GetFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurfacePair);    
    }
    MFX_CHECK_STS(sts);

    return sts;

} // mfxStatus D3D11Encoder::SetFrames (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::Execute_ENC (ExecuteBuffers* pExecuteBuffers, mfxU8* , mfxU32 )
{
    pExecuteBuffers;
    //MFX::AutoTimer timer(__FUNCTION__);

    mfxStatus sts = MFX_ERR_NONE;

    sts = Execute(pExecuteBuffers, ENCODE_ENC_ID,0,0);
    MFX_CHECK_STS(sts);    

    return sts;

} // mfxStatus D3D11Encoder::Execute_ENC (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::Execute(
    ExecuteBuffers* pExecuteBuffers, 
    mfxU32 /*func*/,
    mfxU8* pUserData, mfxU32 userDataLen)
{
    HRESULT hr = S_OK;
    mfxHDLPair* inputPair = &(pExecuteBuffers->m_pSurfacePair);
    ID3D11Resource*   pInputD3D11Res = static_cast<ID3D11Resource*>(inputPair->first);
    ENCODE_PACKEDHEADER_DATA payload = {0};

    // [1] BeginFrame()
    //{ ---------------------------------------------------------
        /*D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC outputDesc;
        outputDesc.DecodeProfile = m_guid;
        outputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2D.ArraySlice = (UINT)(size_t(inputPair->second)); 
        SAFE_RELEASE(m_pVDOView);

        hr = m_pVideoDevice->CreateVideoDecoderOutputView(
            pInputD3D11Res, 
            &outputDesc,
            &m_pVDOView);
        CHECK_HRES(hr);
        
        hr = m_pVideoContext->DecoderBeginFrame(m_pDecoder, m_pVDOView, 0, 0);
        CHECK_HRES(hr); */   
    //}
    
    // [2] sps/pps/slice headers preparation    
    //FillSps();
    //FillPps();
    //FillSliceHeader(); 

    // [3] prepare resource list
    // it contains resources in video memory that needed for the encoding operation    
    mfxU32       RES_ID_BITSTREAM   = 0;          // bitstream surface takes first place in resourceList
    mfxU32       RES_ID_MBLOCK      = 0;
    mfxU32       RES_ID_ORIGINAL    = 1;    
    mfxU32       RES_ID_REFERENCE   = 2;          // then goes all reference frames from dpb    
    // aya: reconIdx == idxMb
    mfxU32       idxRecon = pExecuteBuffers->m_idxMb;
    mfxU32       RES_ID_RECONSTRUCT = RES_ID_REFERENCE + idxRecon;    
    

    mfxU32 resourceCount = mfxU32(RES_ID_REFERENCE + m_reconQueue.size());    
    std::vector<ID3D11Resource*> resourceList;
    resourceList.resize(resourceCount);

    if (m_bENC_PAK)
    {
        resourceList[RES_ID_BITSTREAM]   = static_cast<ID3D11Resource*>(m_bsQueue[pExecuteBuffers->m_idxBs].first);
    }
    else
    {
        resourceList[RES_ID_MBLOCK]   = static_cast<ID3D11Resource*>(m_mbQueue[pExecuteBuffers->m_idxMb].first);
    }
    resourceList[RES_ID_ORIGINAL] = pInputD3D11Res;    
    
    for (mfxU32 i = 0; i < m_reconQueue.size(); i++)
    {        
        resourceList[RES_ID_REFERENCE + i] = static_cast<ID3D11Resource*>(m_reconQueue[i].first);
    }


    // [4]. buffers in system memory (configuration buffers)    
    mfxU32 compBufferCount = 10;//mfxU32(10 + m_packedSlice.size());    //aya fixme
    std::vector<ENCODE_COMPBUFFERDESC>  encodeCompBufferDesc;
    encodeCompBufferDesc.resize(compBufferCount);
    Zero(encodeCompBufferDesc);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    Zero(encodeExecuteParams);
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc[0];

    // FIXME: need this until production driver moves to DDI 0.87
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;


    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    if (pExecuteBuffers->m_bAddSPS)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA);
        encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(pExecuteBuffers->m_sps));
        encodeCompBufferDesc[bufCnt].pCompBuffer          = &pExecuteBuffers->m_sps;
        bufCnt++;
        pExecuteBuffers->m_bAddSPS = 0;

        if (m_bENC_PAK && 
            (pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[0] || 
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[1] || 
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[2] || 
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[3] ))
        {
            encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_QUANTDATA;
            encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_quantMatrix);
            encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_quantMatrix;
            bufCnt++;       
        }
    }

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(pExecuteBuffers->m_pps));
    encodeCompBufferDesc[bufCnt].pCompBuffer          = &pExecuteBuffers->m_pps;
    //encodeCompBufferDesc[bufCnt].pReserved            = &RES_ID_RECONSTRUCT;
    //bufCnt++;

    ENCODE_INPUT_DESC encodeInputDesc;
    encodeInputDesc.ArraSliceOriginal = (UINT)((size_t)inputPair->second);
    encodeInputDesc.IndexOriginal     = RES_ID_ORIGINAL;
    encodeInputDesc.ArraySliceRecon   = (UINT)(size_t(m_reconQueue[idxRecon].second));
    encodeInputDesc.IndexRecon        = RES_ID_RECONSTRUCT;
    encodeCompBufferDesc[bufCnt].pReserved            = &encodeInputDesc;
    bufCnt++;


    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = sizeof(*pExecuteBuffers->m_pSlice) * pExecuteBuffers->m_pps.NumSlice;
    encodeCompBufferDesc[bufCnt].pCompBuffer          = pExecuteBuffers->m_pSlice;
    bufCnt++;

    if (!m_bENC_PAK)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA);
        encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(RES_ID_MBLOCK));
        encodeCompBufferDesc[bufCnt].pCompBuffer          = &RES_ID_MBLOCK;
        bufCnt++;
    }
    else
    {   
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
        encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA));
        encodeCompBufferDesc[bufCnt].pCompBuffer          = &RES_ID_BITSTREAM;
        bufCnt++;

        if (userDataLen > 0 && pUserData)
        {
            payload.pData = pUserData;
            payload.DataLength = payload.BufferSize = userDataLen;

            encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
            encodeCompBufferDesc[bufCnt].DataSize       = sizeof (payload);
            encodeCompBufferDesc[bufCnt].pCompBuffer    = &payload;
            bufCnt++;        
        }
    }


    //// individual header by individual pCompBuffer
    //for( mfxU32 i = 0; i < m_packedSlice.size(); i++ )
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA);
    //    //encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA) * m_packedSlice.size());
    //    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    //    encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_packedSlice[i];
    //    bufCnt++;
    //}

    // [2.4] send to driver
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
    decoderExtParams.Function              = m_bENC_PAK ? ENCODE_ENC_PAK_ID : ENCODE_ENC_ID;
    decoderExtParams.pPrivateInputData     = &encodeExecuteParams;
    decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
    decoderExtParams.pPrivateOutputData    = 0;
    decoderExtParams.PrivateOutputDataSize = 0;
    decoderExtParams.ResourceCount         = resourceCount; 
    decoderExtParams.ppResourceList        = &resourceList[0];

    //printf("before:\n");
    ////printf("Raw = %d\n", m_pps.CurrOriginalPic.Index7Bits);
    ////printf("Recon = %d\n", m_pps.CurrReconstructedPic.Index7Bits);
    //printf("indexRaw = %d (%p), sliceRaw = %d, indexRecon = %d (%p), sliceRecon = %d\n",
    //    encodeInputDesc.IndexOriginal, resourceList[encodeInputDesc.IndexOriginal],
    //    encodeInputDesc.ArraSliceOriginal,
    //    encodeInputDesc.IndexRecon, resourceList[encodeInputDesc.IndexRecon],
    //    encodeInputDesc.ArraySliceRecon);
    //printf("RefFrameList = ");
    //for (mfxU32 i = 0; i<16 && m_pps.RefFrameList[i].bPicEntry != 0xff; i++)
    //    printf("%d ", m_pps.RefFrameList[i].Index7Bits);
    //printf("\n");

    hr = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);

    //printf("after:\n");
    //printf("Raw = %d\n", m_pps.CurrOriginalPic.Index7Bits);
    //printf("Recon = %d\n", m_pps.CurrReconstructedPic.Index7Bits);
    //printf("indexRaw = %d (%p), sliceRaw = %d, indexRecon = %d (%p), sliceRecon = %d\n",
    //    encodeInputDesc.IndexOriginal, resourceList[encodeInputDesc.IndexOriginal],
    //    encodeInputDesc.ArraSliceOriginal,
    //    encodeInputDesc.IndexRecon, resourceList[encodeInputDesc.IndexRecon],
    //    encodeInputDesc.ArraySliceRecon);
    ////printf("RefFrameList = ");
    ////for (mfxU32 i = 0; i<16 && m_pps.RefFrameList[i].bPicEntry != 0xff; i++)
    ////    printf("%d ", m_pps.RefFrameList[i].Index7Bits);
    ////printf("\n");
    //printf("\n");

    CHECK_HRES(hr);

    hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
    CHECK_HRES(hr);   

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Execute(...)


//mfxStatus D3D11Encoder::Execute_VME (ExecuteBuffers* pExecuteBuffers)
//{
//    pExecuteBuffers;
//    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Submit_VME");
//
//    mfxStatus sts = MFX_ERR_NONE;
//
//    //sts = Execute(pExecuteBuffers, ENCODE_ENC_ID);
//    //MFX_CHECK_STS(sts);    
//
//    return sts;
//
//} // mfxStatus D3D11Encoder::Execute_VME (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData, mfxU32 userDataLen)
{
    pExecuteBuffers;
    //MFX::AutoTimer timer(__FUNCTION__);

    mfxStatus sts = MFX_ERR_NONE;

    sts = Execute(pExecuteBuffers, ENCODE_ENC_PAK_ID,pUserData,userDataLen);
    MFX_CHECK_STS(sts);    

    return sts;

} // mfxStatus D3D11Encoder::Execute_ENCODE (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::Execute(ExecuteBuffers* pExecuteBuffers,mfxU8* pUserData, mfxU32 userDataLen)
{
    mfxStatus sts = MFX_ERR_NONE;

    if( IsFullEncode() )
    {
        sts = Execute_ENCODE(pExecuteBuffers, pUserData, userDataLen);
    }
    else
    {
        sts = Execute_ENC(pExecuteBuffers, 0, 0);
    }

    return sts;

} // mfxStatus D3D11Encoder::Execute(ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::Close()
{
    SAFE_RELEASE(m_pDecoder);
    SAFE_RELEASE(m_pVDOView);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Close()


mfxStatus D3D11Encoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMB");
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = {0};
    ENCODE_QUERY_STATUS_PARAMS queryStatus = {0};

    //pExecuteBuffers->m_idxMb = 0;
    if (pExecuteBuffers->m_idxMb >= DWORD(m_allocResponseMB.NumFrameActual))
    {
        return MFX_ERR_UNSUPPORTED;
    } 
    if (pExecuteBuffers->m_pps.picture_coding_type != CODING_TYPE_I)
    {
        while (1)
        {
            if (m_feedback.isUpdateNeeded())
            {
                 D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
#ifdef NEW_STATUS_REPORTING_DDI_0915
                ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
                feedbackDescr.SizeOfStatusParamStruct = sizeof(ENCODE_QUERY_STATUS_PARAMS);
                feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915
                decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;

#ifdef NEW_STATUS_REPORTING_DDI_0915
                decoderExtParams.pPrivateInputData     = &feedbackDescr;
                decoderExtParams.PrivateInputDataSize  = sizeof(feedbackDescr);
#else // NEW_STATUS_REPORTING_DDI_0915
                decoderExtParams.pPrivateInputData     = 0;
                decoderExtParams.PrivateInputDataSize  = 0;
#endif // NEW_STATUS_REPORTING_DDI_0915

                decoderExtParams.pPrivateOutputData    = m_feedback.GetPointer();
                decoderExtParams.PrivateOutputDataSize = m_feedback.GetSize();
                decoderExtParams.ResourceCount         = 0;
                decoderExtParams.ppResourceList        = 0;

                HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
               
                if (hRes == D3DERR_WASSTILLDRAWING)
                {
                    vm_time_sleep(0);
                    continue;
                }
                MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
            }
            MFX_CHECK(m_feedback.GetFeedback(pExecuteBuffers->m_pps.StatusReportFeedbackNumber,queryStatus),MFX_ERR_DEVICE_FAILED);
            //if (m_feedback.GetFeedback(pExecuteBuffers->m_pps.StatusReportFeedbackNumber,queryStatus))
            //{
                //printf("work_around %d\n", i++);
            //    vm_time_sleep(10);
            //    continue;        
            //}
            if (queryStatus.bStatus == ENCODE_NOTREADY)
            {
                    vm_time_sleep(0);
                    continue;
            }
            MFX_CHECK(queryStatus.bStatus == ENCODE_OK, MFX_ERR_DEVICE_FAILED);
            break;
        }
    }


    Frame.MemId = m_allocResponseMB.mids[pExecuteBuffers->m_idxMb];

    sts = m_core->LockFrame(Frame.MemId,&Frame);
    MFX_CHECK_STS(sts);

    int numMB = 0;
    for (int i = 0; i<(int)pExecuteBuffers->m_pps.NumSlice;i++)
    {
        numMB = numMB + (int)pExecuteBuffers->m_pSlice[i].NumMbsForSlice;
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMBData");
        mfxFrameSurface1 src = {0};
        mfxFrameSurface1 dst = {0};

        src.Data        = Frame;
        src.Data.Y     += m_layout.MB_CODE_offset;
        src.Data.Pitch  = mfxU16(m_layout.MB_CODE_stride);
        src.Data.PitchHigh = 0;
        src.Info.Width  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        src.Info.Height = mfxU16(numMB);
        src.Info.FourCC = MFX_FOURCC_P8;
        dst.Data.Y      = (mfxU8 *)pExecuteBuffers->m_pMBs;
        dst.Data.Pitch  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        dst.Info.Width  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        dst.Info.Height = mfxU16(numMB);
        dst.Info.FourCC = MFX_FOURCC_P8;

        sts = m_core->DoFastCopy(&dst, &src);
        MFX_CHECK_STS(sts);
    }

    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;

} // mfxStatus D3D11Encoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D11Encoder::FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt)
{
    MFX::AutoTimer timer("CopyBS");
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = {0};
    ENCODE_QUERY_STATUS_PARAMS queryStatus = {0};

    MFX_CHECK(nBitstream < DWORD(m_allocResponseBS.NumFrameActual),MFX_ERR_NOT_FOUND);
    
    
    if (m_feedback.isUpdateNeeded())
    {
         D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
#ifdef NEW_STATUS_REPORTING_DDI_0915
        ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
        feedbackDescr.SizeOfStatusParamStruct = sizeof(ENCODE_QUERY_STATUS_PARAMS);
        feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

        decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;
#ifdef NEW_STATUS_REPORTING_DDI_0915
        decoderExtParams.pPrivateInputData     = &feedbackDescr;
        decoderExtParams.PrivateInputDataSize  = sizeof(feedbackDescr);
#else // NEW_STATUS_REPORTING_DDI_0915
        decoderExtParams.pPrivateInputData     = 0;
        decoderExtParams.PrivateInputDataSize  = 0;
#endif // NEW_STATUS_REPORTING_DDI_0915        
        decoderExtParams.pPrivateOutputData    = m_feedback.GetPointer();
        decoderExtParams.PrivateOutputDataSize = m_feedback.GetSize();
        decoderExtParams.ResourceCount         = 0;
        decoderExtParams.ppResourceList        = 0;

        HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
       
        MFX_CHECK(hRes != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
    }
    MFX_CHECK(m_feedback.GetFeedback(nFeedback,queryStatus),MFX_ERR_DEVICE_FAILED);        
    MFX_CHECK(queryStatus.bStatus != ENCODE_NOTREADY, MFX_WRN_DEVICE_BUSY);
    MFX_CHECK(queryStatus.bStatus == ENCODE_OK, MFX_ERR_DEVICE_FAILED);

    Frame.MemId = m_allocResponseBS.mids[nBitstream];
    sts = m_core->LockFrame(Frame.MemId,&Frame);
    MFX_CHECK_STS(sts);


#ifdef PAVP_SUPPORT
    if (pEncrypt->m_bEncryptionMode)
    {
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData);
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData->Data);
        MFX_CHECK(pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset + queryStatus.bitstreamSize < pBitstream->EncryptedData->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        memcpy_s(pBitstream->EncryptedData->Data + pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset, pBitstream->EncryptedData->MaxLength, Frame.Y, queryStatus.bitstreamSize);
        pBitstream->EncryptedData->DataLength += queryStatus.bitstreamSize;
        pBitstream->EncryptedData->CipherCounter.IV = pEncrypt->m_aesCounter.IV;
        pBitstream->EncryptedData->CipherCounter.Count = pEncrypt->m_aesCounter.Count;
        pEncrypt->m_aesCounter.Increment();
        if (pEncrypt->m_aesCounter.IsWrapped())
        {
              pEncrypt->m_aesCounter.ResetWrappedFlag();
        }            
    }
    else
#endif
    {
        MFX_CHECK(pBitstream->DataLength + pBitstream->DataOffset + queryStatus.bitstreamSize < pBitstream->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        memcpy_s(pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset, pBitstream->MaxLength, Frame.Y, queryStatus.bitstreamSize);
        pBitstream->DataLength += queryStatus.bitstreamSize;     
    }

  

    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;
} 


mfxStatus D3D11Encoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)
{
    m_reconFrames.NumFrameActual  = pResponse->NumFrameActual;
    MFX_CHECK(m_reconFrames.NumFrameActual <NUM_FRAMES,MFX_ERR_UNSUPPORTED);

    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        m_reconFrames.mids[i] = pResponse->mids[i];
        m_reconFrames.indexes[i] = (mfxU16)i;
    }

    return Register(pResponse, (D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE)D3DDDIFMT_NV12);// aya: FIXME

    //return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)

#endif // #if defined(MFX_ENABLE_MPEG2_VIDEO_ENC)|| defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#endif // #if defined(MFX_VA) && defined(MFX_D3D11_ENABLED)
/* EOF */
