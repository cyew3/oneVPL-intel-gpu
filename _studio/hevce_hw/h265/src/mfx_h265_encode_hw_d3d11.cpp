//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"


#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{
D3D11Encoder::D3D11Encoder()
    : m_core(0)
    , m_guid()
    , m_width(0)
    , m_height(0)
    , m_caps()
    , m_capsQuery()
    , m_capsGet()
    , m_infoQueried(false)
    , m_pavp(false)
    , m_widi(false)
    , m_maxSlices(0)
    , m_sps()
    , m_pps()
{
}

D3D11Encoder::~D3D11Encoder()
{
    Destroy();
}

mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAuxilliaryDevice");
    m_core = core;
    mfxStatus sts = MFX_ERR_NONE;
    ID3D11Device* device;
    D3D11_VIDEO_DECODER_DESC    desc = {};
    D3D11_VIDEO_DECODER_CONFIG  config = {};
    HRESULT hr;
    mfxU32 count = 0;
    
    m_guid      = guid;
    m_width     = width;
    m_height    = height;

    // [0] Get device/context
    {
        sts = core->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        if (sts == MFX_ERR_NOT_FOUND)
            sts = core->CreateAccelerationDevice(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        MFX_CHECK_STS(sts);
        MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

        device->GetImmediateContext(&m_context);
        MFX_CHECK(m_context, MFX_ERR_DEVICE_FAILED);

        m_vdevice  = device;
        m_vcontext = m_context;
        MFX_CHECK(m_vdevice, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(m_vcontext, MFX_ERR_DEVICE_FAILED);
    }

    // [1] Query supported decode profiles
    {
        bool isFound = false;

        UINT profileCount;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
            profileCount = m_vdevice->GetVideoDecoderProfileCount();
        }
        assert( profileCount > 0 );

        for (UINT i = 0; i < profileCount; i ++)
        {  
            GUID profileGuid;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
                hr = m_vdevice->GetVideoDecoderProfile(i, &profileGuid);
            }
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
        desc.SampleWidth  = m_width;
        desc.SampleHeight = m_height;
        desc.OutputFormat = DXGI_FORMAT_NV12;
        desc.Guid         = m_guid; 

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
            hr = m_vdevice->GetVideoDecoderConfigCount(&desc, &count);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    // [3] CreateVideoDecoder
    {
        desc.SampleWidth  = m_width;
        desc.SampleHeight = m_height;
        desc.OutputFormat = DXGI_FORMAT_NV12;
        desc.Guid         = m_guid; 

        config.ConfigDecoderSpecific         = m_widi ? (ENCODE_ENC_PAK | ENCODE_WIDI) : ENCODE_ENC_PAK;  
        config.guidConfigBitstreamEncryption = m_pavp ? DXVA2_INTEL_PAVP : DXVA_NoEncrypt;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
            hr = m_vdevice->CreateVideoDecoder(&desc, &config, &m_vdecoder);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    // [4] Query the encoding device capabilities
    {
        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
        ext.pPrivateInputData = 0;
        ext.PrivateInputDataSize = 0;
        ext.pPrivateOutputData = &m_caps;
        ext.PrivateOutputDataSize = sizeof(m_caps);
        ext.ResourceCount = 0;
        ext.ppResourceList = 0;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
            hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAccelerationService");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION ext = {};

    if (   m_pavp != (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP)
        || m_widi != par.WiDi)
    {
        // Have to create new device

        m_widi = par.WiDi;
        m_pavp = (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP);

        mfxStatus sts = CreateAuxilliaryDevice(m_core, m_guid, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height);
        MFX_CHECK_STS(sts);

        if (m_pavp)
        {
            const mfxExtPAVPOption& PAVP = par.m_ext.PAVP;

            D3D11_AES_CTR_IV      initialCounter = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
            PAVP_ENCRYPTION_MODE  encryptionMode = { PAVP.EncryptionType,   PAVP.CounterType         };
            ENCODE_ENCRYPTION_SET encryptSet     = { PAVP.CounterIncrement, &initialCounter, &encryptionMode};

            D3D11_VIDEO_DECODER_EXTENSION encryptParam = {};
            encryptParam.Function = ENCODE_ENCRYPTION_SET_ID;
            encryptParam.pPrivateInputData     = &encryptSet;
            encryptParam.PrivateInputDataSize  = sizeof(ENCODE_ENCRYPTION_SET);

            hr = m_vcontext->DecoderExtension(m_vdecoder, &encryptParam);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
    }

    ext.Function = ENCODE_ENC_CTRL_CAPS_ID;
    ext.pPrivateOutputData = &m_capsQuery;
    ext.PrivateOutputDataSize = sizeof(m_capsQuery);

    hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    ext.Function = ENCODE_ENC_CTRL_GET_ID;
    ext.pPrivateOutputData = &m_capsGet;
    ext.PrivateOutputDataSize = sizeof(m_capsGet);

    hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());
    
    m_maxSlices = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize) * CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Reset(MfxVideoParam const & par, bool bResetBRC)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_sps.bResetBRC = bResetBRC;

    return MFX_ERR_NONE;
}

mfxU8 convertDX9TypeToDX11Type(mfxU8 type)
{
    switch(type)
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

mfxU32 convertDXGIFormatToMFXFourCC(DXGI_FORMAT format)
{
    switch(format)
    {
    case DXGI_FORMAT_NV12: return MFX_FOURCC_NV12;
    case DXGI_FORMAT_P010: return MFX_FOURCC_P010;
    case DXGI_FORMAT_YUY2: return MFX_FOURCC_YUY2;
    case DXGI_FORMAT_Y210: return MFX_FOURCC_P210;
    case DXGI_FORMAT_P8  : return MFX_FOURCC_P8;
    default:
        return 0;
    }
}

mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    HRESULT hr;

    type = (D3DDDIFORMAT)convertDX9TypeToDX11Type((mfxU8)type);


    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT cnt = {};
              
        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_FORMAT_COUNT_ID; 
        ext.pPrivateOutputData = &cnt;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);

        
        hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(cnt.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(cnt.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats = {};
        encodeFormats.CompressedBufferInfoSize  = (int)(m_compBufInfo.size() * sizeof(ENCODE_COMP_BUFFER_INFO));
        encodeFormats.UncompressedFormatSize    = (int)(m_uncompBufInfo.size() * sizeof(D3DDDIFORMAT));
        encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];

        Zero(ext);
        ext.Function = ENCODE_FORMATS_ID; 
        ext.pPrivateOutputData = &encodeFormats;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);

        hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
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

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = convertDXGIFormatToMFXFourCC((DXGI_FORMAT)m_compBufInfo[i].CompressedFormats);

    return MFX_ERR_NONE;  
}

mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS_HEVC & caps)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_CHECK_WITH_ASSERT(m_core, MFX_ERR_NOT_INITIALIZED);

    mfxFrameAllocator & fa = m_core->FrameAllocator();
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
        // Reserve space for feedback reports.
        mfxU32 feedbackSize = FeedbackSize(m_caps.SliceLevelReportSupport ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME, m_maxSlices);
        m_feedbackUpdate.Reset(response.NumFrameActual, feedbackSize);
        m_feedbackCached.Reset(response.NumFrameActual, feedbackSize);
    }

    return MFX_ERR_NONE;
}

#define ADD_CBD(id, buf, num)\
    assert(executeParams.NumCompBuffers < m_cbd.size());\
    m_cbd[executeParams.NumCompBuffers].CompressedBufferType = (D3DFORMAT)(id); \
    m_cbd[executeParams.NumCompBuffers].DataSize = (UINT)(sizeof(buf) * (num)); \
    m_cbd[executeParams.NumCompBuffers].pCompBuffer = &buf; \
    executeParams.NumCompBuffers++;

mfxStatus D3D11Encoder::Execute(Task const & task, mfxHDL surface)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Execute");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    ENCODE_PACKEDHEADER_DATA * pPH = 0;
    ENCODE_INPUT_DESC ein = {};
    ENCODE_EXECUTE_PARAMS executeParams = {};

    executeParams.pCompressedBuffers = &m_cbd[0];
    Zero(m_cbd);

    if (!m_sps.bResetBRC)
        m_sps.bResetBRC = task.m_resetBRC;
    
    FillPpsBuffer(task, m_pps);
    FillSliceBuffer(task, m_sps, m_pps, m_slice);

    mfxU32 RES_ID_BS  = 0;
    mfxU32 RES_ID_RAW = 1;
    mfxU32 RES_ID_REF = 2;
    mfxU32 RES_ID_REC = RES_ID_REF + task.m_idxRec;

    std::vector<ID3D11Resource*> resourceList(RES_ID_REF + m_reconQueue.size());

    resourceList[RES_ID_BS ] = (ID3D11Resource*)m_bsQueue[task.m_idxBs].first;
    resourceList[RES_ID_RAW] = (ID3D11Resource*)surface;
    
    for (mfxU32 i = 0; i < m_reconQueue.size(); i ++)
        resourceList[RES_ID_REF + i] = (ID3D11Resource*)m_reconQueue[i].first;

    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA,          m_sps,      1);
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA,          m_pps,      1);
    {
        // attach resources to PPSDATA
        ein.ArraSliceOriginal = 0;
        ein.IndexOriginal     = RES_ID_RAW;
        ein.ArraySliceRecon   = (UINT)(size_t(m_reconQueue[task.m_idxRec].second));
        ein.IndexRecon        = RES_ID_REC;
        m_cbd[executeParams.NumCompBuffers - 1].pReserved = &ein;
    }
    
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA,        m_slice[0], m_slice.size());
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA,    RES_ID_BS,  1);

    if (task.m_insertHeaders & INSERT_AUD)
    {
        pPH = PackHeader(task, AUD_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_VPS)
    {
        pPH = PackHeader(task, VPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }
    
    if (task.m_insertHeaders & INSERT_SPS)
    {
        pPH = PackHeader(task, SPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_PPS)
    {
        pPH = PackHeader(task, PPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if ((task.m_insertHeaders & INSERT_SEI) || (task.m_ctrl.NumPayload > 0))
    {
        pPH = PackHeader(task, PREFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
        }
    }

    for (mfxU32 i = 0; i < m_slice.size(); i ++)
    {
        pPH = PackSliceHeader(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA, *pPH, 1);
    }

    /*if (task.m_ctrl.NumPayload > 0)
    {
        pPH = PackHeader(task, SUFFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
        }
    }*/

    try
    {
#ifdef HEADER_PACKING_TEST
        surface;
        ENCODE_QUERY_STATUS_PARAMS fb = {task.m_statusReportNumber,};
        FrameLocker bs(m_core, task.m_midBs);

        for (mfxU32 i = 0; i < executeParams.NumCompBuffers; i ++)
        {
            pPH = (ENCODE_PACKEDHEADER_DATA*)executeParams.pCompressedBuffers[i].pCompBuffer;

            if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
            {
                memcpy(bs.Y + fb.bitstreamSize, pPH->pData, pPH->DataLength);
                fb.bitstreamSize += pPH->DataLength;
            }
            else if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA)
            {
                mfxU32 sz = m_width * m_height * 3 / 2 - fb.bitstreamSize;
                HeaderPacker::PackRBSP(bs.Y + fb.bitstreamSize, pPH->pData, sz, CeilDiv(pPH->DataLength, 8));
                fb.bitstreamSize += sz;
            }
        }

        m_feedbackUpdate[0] = fb;
        m_feedbackCached.Update(m_feedbackUpdate);

#else
        HRESULT hr;
        D3D11_VIDEO_DECODER_EXTENSION ext = {};

        ext.Function              = ENCODE_ENC_PAK_ID;
        ext.pPrivateInputData     = &executeParams;
        ext.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
        ext.ResourceCount         = (UINT)resourceList.size(); 
        ext.ppResourceList        = &resourceList[0];
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
            hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
            hr = m_vcontext->DecoderEndFrame(m_vdecoder);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    m_sps.bResetBRC = 0;

    return MFX_ERR_NONE;
}


mfxStatus D3D11Encoder::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses. 

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr = {};
        feedbackDescr.StatusParamType = (m_caps.SliceLevelReportSupport && m_pps.bEnableSliceLevelReport) ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME;

        for (;;)
        {
            HRESULT hr;
            feedbackDescr.SizeOfStatusParamStruct = FeedbackSize((ENCODE_QUERY_STATUS_PARAM_TYPE)feedbackDescr.StatusParamType, m_maxSlices);

            m_feedbackUpdate.Reset(m_feedbackUpdate.size(), feedbackDescr.SizeOfStatusParamStruct);

            try
            {
                D3D11_VIDEO_DECODER_EXTENSION ext = {};

                ext.Function              = ENCODE_QUERY_STATUS_ID;
                ext.pPrivateInputData     = &feedbackDescr;
                ext.PrivateInputDataSize  = sizeof(feedbackDescr);
                ext.pPrivateOutputData    = &m_feedbackUpdate[0];
                ext.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * m_feedbackUpdate.feedback_size());

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "ENCODE_QUERY_STATUS_ID");
                    hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
                }
            }
            catch (...)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            if (hr == E_INVALIDARG && feedbackDescr.StatusParamType == QUERY_STATUS_PARAM_SLICE)
            {
                feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
                continue;
            }

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            break;
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

        if (m_widi && m_caps.HWCounterAutoIncrementSupport)
        {
            task.m_aes_counter.Count = feedback->aes_counter.Counter;
            task.m_aes_counter.IV    = feedback->aes_counter.IV;
        }

        {
            mfxExtEncodedSlicesInfo* pESI = ExtBuffer::Get(task.m_ctrl);

            if (pESI)
            {
                pESI->NumEncodedSlice     = feedback->NumberSlices;
                pESI->NumSliceNonCopliant = feedback->NumSliceNonCompliant;
                pESI->SliceSizeOverflow   = feedback->SliceSizeOverflow;

                if (pESI->NumSliceSizeAlloc && pESI->SliceSize && m_caps.SliceLevelReportSupport)
                {
                    mfxU16* pSize = (mfxU16*)((mfxU8*)feedback + sizeof(ENCODE_QUERY_STATUS_PARAMS) + sizeof(UINT) * 4);
                    CopyN(pESI->SliceSize, pSize, Min(feedback->NumberSlices, pESI->NumSliceSizeAlloc));
                }
            }
        }

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

mfxStatus D3D11Encoder::Destroy()
{
    return MFX_ERR_NONE;
}

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)