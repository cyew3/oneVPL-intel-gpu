/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp8_encode_ddi_hw.h"
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE) 

namespace MFX_VP8ENC
{    
    mfxStatus QueryHwCaps(eMFXVAType va_type, mfxU32 adapterNum, ENCODE_CAPS_VP8 & caps)
    {
        std::auto_ptr<VideoCORE>      pCore(FactoryCORE::CreateCORE(va_type, adapterNum, 1));
        MFX_CHECK(pCore.get() != NULL, MFX_ERR_INVALID_HANDLE);

        std::auto_ptr<DriverEncoder> ddi;

        ddi.reset(CreatePlatformVp8Encoder(pCore.get()));
        MFX_CHECK(ddi.get() != NULL, MFX_WRN_PARTIAL_ACCELERATION);
            
        mfxStatus sts = ddi.get()->QueryEncodeCaps(caps);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    }
    mfxStatus CheckVideoParam(mfxVideoParam const & par, ENCODE_CAPS_VP8 const &caps)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxExtCodingOptionVP8 * opts = GetExtBuffer(par);
        MFX_CHECK_NULL_PTR1(opts);

        MFX_CHECK(par.mfx.FrameInfo.Height <= caps.MaxPicHeight, MFX_WRN_PARTIAL_ACCELERATION);
        MFX_CHECK(par.mfx.FrameInfo.Width  <= caps.MaxPicWidth,  MFX_WRN_PARTIAL_ACCELERATION);
        if (opts->EnableMultipleSegments && !caps.SegmentationAllowed)
        {
            opts->EnableMultipleSegments = 0;
            sts =  MFX_WRN_VIDEO_PARAM_CHANGED;       
        }
        return sts;    
    }

    DriverEncoder* CreatePlatformVp8Encoder( VideoCORE* core )
    {
#if   defined (MFX_VA_WIN) 
        if(MFX_HW_D3D9 == core->GetVAType())
        {       
            return new D3D9Encoder;
        }
#if defined  (MFX_D3D11_ENABLED)
        else if( MFX_HW_D3D11 == core->GetVAType() )
        {
            return new D3D11Encoder;
        }
#endif
        else
        {
            return NULL;
        }
#else
        return NULL;
#endif
    } 
    /* ----------- Functions to convert MediaSDK into DDI --------------------- */

    void FillSpsBuffer(mfxVideoParam const & par, ENCODE_SET_SEQUENCE_PARAMETERS_VP8 & sps)
    {
        Zero(sps);

        sps.wFrameWidth  = par.mfx.FrameInfo.Width;
        sps.wFrameHeight = par.mfx.FrameInfo.Height; 
        sps.GopPicSize  = par.mfx.GopPicSize;

        sps.GopRefDist  = mfxU8(par.mfx.GopRefDist); 

        sps.TargetUsage = mfxU8(par.mfx.TargetUsage);

        sps.RateControlMethod  = mfxU8(par.mfx.RateControlMethod);
        sps.TargetBitRate      = mfxU16(par.mfx.TargetKbps);
        sps.MaxBitRate         = mfxU16(par.mfx.MaxKbps);
        sps.MinBitRate         = mfxU16(par.mfx.TargetKbps);

        sps.InitVBVBufferFullnessInBit     = par.mfx.InitialDelayInKB * 8000;
        sps.VBVBufferSizeInBit             = par.mfx.BufferSizeInKB   * 8000;

        sps.bResetBRC                      = 0;
        //sps.UserMaxFrameSize               = extDdi->UserMaxFrameSize;
        sps.AVBRAccuracy                   = par.mfx.Accuracy;
        sps.AVBRConvergence                = par.mfx.Convergence * 100;

    } 

    mfxStatus FillPpsBuffer(Task const & task,mfxVideoParam const & par, ENCODE_SET_PICTURE_PARAMETERS_VP8 & pps)
    {
        mfxExtCodingOptionVP8 * opts = GetExtBuffer(par);
        MFX_CHECK_NULL_PTR1(opts);

        Zero(pps);

        pps.version          = 5;//MSDK_VP8_API->version;
        pps.color_space      = 0;

        pps.CurrOriginalPic.Index7Bits              = mfxU8(task.m_pRecFrame->idInPool);
        pps.CurrOriginalPic.AssociatedFlag          = 0 ;

        pps.CurrReconstructedPic.Index7Bits         = mfxU8(task.m_pRecFrame->idInPool);
        pps.CurrReconstructedPic.AssociatedFlag     = 0;

        pps.LastRefPic.bPicEntry                    = 0xff;
        if(task.m_pRecRefFrames[REF_BASE])    
        {
            pps.LastRefPic.Index7Bits      = mfxU8(task.m_pRecRefFrames[REF_BASE]->idInPool);
            pps.LastRefPic.AssociatedFlag  = 0;
        }

        pps.GoldenRefPic.bPicEntry                    = 0xff;
        if(task.m_pRecRefFrames[REF_GOLD])
        {
            pps.GoldenRefPic.Index7Bits      = mfxU8(task.m_pRecRefFrames[REF_GOLD]->idInPool);
            pps.GoldenRefPic.AssociatedFlag  = 0;
        }

        pps.AltRefPic.bPicEntry                    = 0xff;
        if(task.m_pRecRefFrames[REF_ALT])
        {
            pps.AltRefPic.Index7Bits      = mfxU8(task.m_pRecRefFrames[REF_ALT]->idInPool);
            pps.AltRefPic.AssociatedFlag  = 0;
        }

        pps.frame_type = (task.m_sFrameParams.bIntra) ? 0 : 1;

        //pps.StatusReportFeedbackNumber = task.m_frameOrder;

        pps.segmentation_enabled     = opts->EnableMultipleSegments;

        pps.filter_type              = 0; //the same as in C model
        pps.loop_filter_adj_enable   = 0;
        pps.CodedCoeffTokenPartition = 0;

        if (pps.frame_type)
        {
            pps.refresh_golden_frame = 0; 
            pps.refresh_alternate_frame = 0; 
            pps.copy_buffer_to_golden = 1;            
            pps.copy_buffer_to_alternate = 2; 
            pps.refresh_last = 1;                             
        
        }

        pps.sign_bias_golden         = 0;
        pps.sign_bias_alternate      = 0;
        pps.mb_no_coeff_skip         = 0;
        pps.sharpness_level          = 1;// the same as in C model


        pps.loop_filter_level[0] = 10;  // the same as in C model
        pps.loop_filter_level[1] = 14;  // the same as in C model
        pps.loop_filter_level[2] = 25;  // the same as in C model
        pps.loop_filter_level[3] = 30;  // the same as in C model

        pps.ref_lf_delta[0] = 5;    // the same as in C model
        pps.ref_lf_delta[1] = -1;   // the same as in C model
        pps.ref_lf_delta[2] = 2;    // the same as in C model
        pps.ref_lf_delta[3] = 2;    // the same as in C model

        pps.mode_lf_delta[0] = 1;   // the same as in C model 
        pps.mode_lf_delta[1] = 0;   // the same as in C model 
        pps.mode_lf_delta[2] = 2;   // the same as in C model
        pps.mode_lf_delta[3] = 0;   // the same as in C model

        return MFX_ERR_NONE;
      
    } 
    mfxStatus FillQuantBuffer(Task const & task, mfxVideoParam const & par,ENCODE_SET_Qmatrix_VP8 & quant)
    {
        MFX_CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CBR, MFX_ERR_UNSUPPORTED);
        USHORT QP = task.m_sFrameParams.bIntra ? par.mfx.QPI:par.mfx.QPP;
        for (int i = 0; i < 4; i ++)
        {
            for (int j = 0; j < 4; j ++)
            {
                quant.Qvalue[i][j]= QP;
            }
        }
        return MFX_ERR_NONE;
    }




//---------------------------------------------------------
// service functionality 
//---------------------------------------------------------
void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    Zero(init);
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

mfxStatus CachedFeedback::Update(const FeedbackStorage& update)
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
            MFX_CHECK( cache != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            *cache = update[i];
        }
    }
    return MFX_ERR_NONE;
}


const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}


mfxStatus CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}


//---------------------------------------------------------
// D3D9 encoder functionality 
//---------------------------------------------------------

D3D9Encoder::D3D9Encoder()
: m_core(0)
, m_auxDevice(0)
//, m_pDdiData(0)
//, m_counter(0)
, m_infoQueried(false)
{
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    m_core = core;

    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);


    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDeviceHlp> auxDevice(new AuxiliaryDeviceHlp());
    sts = auxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    ENCODE_CAPS_VP8 caps = { 0, };
    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_guid   = guid;
    m_caps = caps;
    m_width  = width;
    m_height = height;
    m_auxDevice = auxDevice;

    return MFX_ERR_NONE;

} 

mfxStatus D3D9Encoder::CreateAccelerationService(mfxVideoParam const & par)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    m_video = par;

    DXVADDI_VIDEODESC desc;
    Zero(desc);
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format       = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice;
    Zero(encodeCreateDevice);
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_HybridPAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    {
        // aya: no encryption now
    }

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsQuery);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsGet);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_layout);
    hr = m_auxDevice->Execute(MBDATA_LAYOUT_ID, (void *)0, m_layout);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_sps);    
    Zero(m_pps);
    Zero(m_quant);

    FillSpsBuffer(par, m_sps);    

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::CreateAccelerationService(mfxVideoParam const & par)


mfxStatus D3D9Encoder::Reset(mfxVideoParam const & par)
{
    Zero(m_sps);    
    Zero(m_pps);
    Zero(m_quant);


    FillSpsBuffer(par, m_sps);
    m_video = par;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Reset(mfxVideoParam const & par)


mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
   MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount = 0;

        GUID guid = m_auxDevice->GetCurrentGuid();
        HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
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
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;

    return MFX_ERR_NONE;

} 

mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_VP8 & caps)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;

} 


mfxStatus D3D9Encoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    caps = m_capsQuery;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    //m_capsGet = caps; // DDI spec: "The application should use the same structure
    //                  // returned in a previous ENCODE_ENC_CTRL_GET_ID command."

    //HRESULT hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_SET_ID, &m_capsGet, sizeof(m_capsGet));
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    //return MFX_ERR_NONE;

    caps;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D9Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)


mfxStatus D3D9Encoder::Register( mfxMemId     /*memId*/, D3DDDIFORMAT /*type*/)
{
    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Register(...)


mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    EmulSurfaceRegister surfaceReg;
    Zero(surfaceReg);
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg); // &m_uncompBufInfo[0]
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;

} 


mfxStatus D3D9Encoder::Execute(Task const &task, mfxHDL surface)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    const mfxU32 NumCompBuffer = 10;
    ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NumCompBuffer];
    Zero(encodeCompBufferDesc);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = encodeCompBufferDesc;

    encodeExecuteParams.pCipherCounter = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT& bufCnt = encodeExecuteParams.NumCompBuffers;    
    
    // input data
    if(task.m_frameOrder == 0)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SPSDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_sps));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &m_sps;
        bufCnt++;
    }

    FillPpsBuffer(task, m_video, m_pps);
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PPSDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &m_pps;
    bufCnt++;

    FillQuantBuffer(task, m_video, m_quant);
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_QUANTDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_quant));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &m_quant;
    bufCnt++;

    // output data
    mfxU32 mb = task.m_pRecFrame->idInPool; // the same index as reconstructed frame index.
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_MBDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(mb));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &mb;
    bufCnt++;

    mfxU32 mv = task.m_pRecFrame->idInPool; // the same index as reconstructed frame index.
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_MOTIONVECTORBUFFER;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(mv));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &mv;
    bufCnt++;


    mfxU32 distort = task.m_pRecFrame->idInPool;
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_DISTORTIONDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(distort));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &distort;
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

    return MFX_ERR_NONE;

} 


/*mfxStatus D3D9Encoder::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

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
            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
                (void *)0,
                0,
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

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        //aya: fixme
        //task.m_bsDataLength = feedback->bitstreamSize;
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

    //return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryStatus(DdiTask & task)

// full encode support
mfxStatus D3D9Encoder::UpdateBitstream(
    mfxMemId       MemId,
    DdiTask      & task)
{
    //mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    //IppiSize     roi       = {task.m_bsDataLength, 1};
    //mfxFrameData bitstream = {0};

    //m_core->LockFrame(MemId, &bitstream);
    //MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    //IppStatus sts = ippiCopyManaged_8u_C1R(
    //    (Ipp8u *)bitstream.Y, task.m_bsDataLength,
    //    bsData, task.m_bsDataLength,
    //    roi, IPP_NONTEMPORAL_LOAD);
    //assert(sts == ippStsNoErr);

    //task.bs->DataLength += task.m_bsDataLength;
    //m_core->UnlockFrame(MemId, &bitstream);

    //return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;

    MemId; task;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D9Encoder::UpdateBitstream(...)*/ 


mfxStatus D3D9Encoder::Destroy()
{
    m_auxDevice.reset(0);

    return MFX_ERR_NONE;
    
} 

//---------------------------------------------------------
// services functions
//---------------------------------------------------------



#if defined(MFX_D3D11_ENABLED)
D3D11Encoder::D3D11Encoder()
: m_core(0)
//, m_pAuxDevice(0)
//, m_pDdiData(0)
//, m_counter(0)
//, m_infoQueried(false)
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
    mfxU32      height)
{
    m_core = core;

    guid; width; height;

    /*D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);


    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    AuxiliaryDevice *pAuxDevice = new AuxiliaryDevice();
    sts = pAuxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = pAuxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    memset(&m_caps, 0, sizeof(m_caps));
    HRESULT hr = pAuxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid), &m_caps, sizeof(m_caps));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_guid   = guid;
    m_width  = width;
    m_height = height;
    m_pAuxDevice = pAuxDevice;*/

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::CreateAuxilliaryDevice(...)


mfxStatus D3D11Encoder::CreateAccelerationService(mfxVideoParam const & par)
{
    par;
    /*MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format       = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice;
    memset(&encodeCreateDevice, 0, sizeof(encodeCreateDevice));
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_pAuxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, &m_guid, sizeof(m_guid), &encodeCreateDevice, sizeof(encodeCreateDevice));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    memset(&m_capsQuery, 0, sizeof(m_capsQuery));
    hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void*)0, 0, &m_capsQuery, sizeof(m_capsQuery));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    memset(&m_capsGet, 0, sizeof(m_capsGet));
    hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void*)0, 0, &m_capsGet, sizeof(m_capsGet));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);*/

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::CreateAccelerationService(mfxVideoParam const & par)


mfxStatus D3D11Encoder::Reset(mfxVideoParam const & par)
{
    /*mfxStatus sts;

    if (!m_pDdiData)
    {
        m_pDdiData = new MfxHwMJpegEncode::ExecuteBuffers;
    }
  
    sts = m_pDdiData->Init(&par);
    MFX_CHECK_STS(sts);

    m_counter = 0;
    return MFX_ERR_NONE;*/
    par;

    return MFX_ERR_UNSUPPORTED;
}


mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
   /* MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount   = 0;

        HRESULT hr = m_pAuxDevice->Execute(ENCODE_FORMAT_COUNT_ID, (void*)0, 0, &encodeFormatCount, sizeof(encodeFormatCount));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize   = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo    = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats     = &m_uncompBufInfo[0];

        hr = m_pAuxDevice->Execute(ENCODE_FORMATS_ID, (void*)0, 0, &encodeFormats, sizeof(encodeFormats));
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
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;

    return MFX_ERR_NONE;*/

    type; request;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)


mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS_VP8 & caps)
{
//    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryEncodeCaps(ENCODE_CAPS & caps)


mfxStatus D3D11Encoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
//    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    caps = m_capsQuery;
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    //m_capsGet = caps; // DDI spec: "The application should use the same structure
    //                  // returned in a previous ENCODE_ENC_CTRL_GET_ID command."

    //HRESULT hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_SET_ID, &m_capsGet, sizeof(m_capsGet));
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    //return MFX_ERR_NONE;

    caps;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)


mfxStatus D3D11Encoder::Register( mfxMemId     /*memId*/, D3DDDIFORMAT /*type*/)
{
    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Register(...)


mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    //EmulSurfaceRegister surfaceReg;
    //memset(&surfaceReg, 0, sizeof(surfaceReg));
    //surfaceReg.type = type;
    //surfaceReg.num_surf = response.NumFrameActual;

    //MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    //for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    //{
    //    mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
    //    MFX_CHECK_STS(sts);
    //    MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    //}

    //HRESULT hr = m_pAuxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    //m_pAuxDevice->EndFrame(0);

    //if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    //{
    //    // Reserve space for feedback reports.
    //    m_feedbackUpdate.resize(response.NumFrameActual);
    //    m_feedbackCached.Reset(response.NumFrameActual);
    //}

    //return MFX_ERR_NONE;

    response; type;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus D3D11Encoder::Execute(const Task &task, mfxHDL surface)
{
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);
    //ExecuteBuffers *pExecuteBuffers = m_pDdiData;

    //const mfxU32 NumCompBuffer = 16;
    //ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NumCompBuffer];
    //memset(&encodeCompBufferDesc, 0, sizeof(encodeCompBufferDesc));

    //ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    //memset(&encodeExecuteParams, 0, sizeof(encodeExecuteParams));
    //encodeExecuteParams.NumCompBuffers = 0;
    //encodeExecuteParams.pCompressedBuffers = encodeCompBufferDesc;

    //// FIXME: need this until production driver moves to DDI 0.87
    //encodeExecuteParams.pCipherCounter = 0;
    //encodeExecuteParams.PavpEncryptionMode.eCounterMode = 0;
    //encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    //UINT& bufCnt = encodeExecuteParams.NumCompBuffers;

    //task.m_statusReportNumber = (m_counter ++) << 8;
    //pExecuteBuffers->m_pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

    //encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_PPSDATA;
    //encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_pps));
    //encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_pps;
    //bufCnt++;

    //mfxU32 bitstream = task.m_idxBS;
    //encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    //encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    //encodeCompBufferDesc[bufCnt].pCompBuffer = &bitstream;
    //bufCnt++;

    //mfxU16 i = 0;

    //for (i = 0; i < pExecuteBuffers->m_pps.NumQuantTable; i++)
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_QUANTDATA;
    //    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_dqt_list[i]));
    //    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dqt_list[i];
    //    bufCnt++;
    //}

    //for (i = 0; i < pExecuteBuffers->m_pps.NumCodingTable; i++)
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_HUFFTBLDATA;
    //    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_dht_list[i]));
    //    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dht_list[i];
    //    bufCnt++;
    //}

    //for (i = 0; i < pExecuteBuffers->m_pps.NumScan; i++)
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_SCANDATA;
    //    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_scan_list[i]));
    //    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_scan_list[i];
    //    bufCnt++;
    //}

    //if (pExecuteBuffers->m_payload_data_present)
    //{
    //    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PAYLOADDATA;
    //    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(pExecuteBuffers->m_payload.size);
    //    encodeCompBufferDesc[bufCnt].pCompBuffer = (void*)pExecuteBuffers->m_payload.data;
    //    bufCnt++;
    //}

    //try
    //{
    //    HRESULT hr = m_pAuxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
    //    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    //    hr = m_pAuxDevice->Execute(ENCODE_ENC_PAK_ID, &encodeExecuteParams, sizeof(encodeExecuteParams));
    //    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    //    HANDLE handle;
    //    m_pAuxDevice->EndFrame(&handle);
    //}
    //catch (...)
    //{
    //    return MFX_ERR_DEVICE_FAILED;
    //}

    //return MFX_ERR_NONE;

    task; surface;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::Execute(DdiTask &task, mfxHDL surface)


/*mfxStatus D3D11Encoder::QueryStatus(Task & task)
{
    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    //MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    //// After SNB once reported ENCODE_OK for a certain feedbackNumber
    //// it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    //// As we won't get all bitstreams we need to cache all other statuses. 

    //// first check cache.
    //const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    //// if task is not in cache then query its status
    //if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    //{
    //    try
    //    {
    //        HRESULT hr = m_pAuxDevice->Execute(
    //            ENCODE_QUERY_STATUS_ID,
    //            (void *)0,
    //            0,
    //            &m_feedbackUpdate[0],
    //            (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
    //        MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
    //        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    //    }
    //    catch (...)
    //    {
    //        return MFX_ERR_DEVICE_FAILED;
    //    }

    //    // Put all with ENCODE_OK into cache.
    //    m_feedbackCached.Update(m_feedbackUpdate);

    //    feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
    //    MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    //}

    //switch (feedback->bStatus)
    //{
    //case ENCODE_OK:
    //    task.m_bsDataLength = feedback->bitstreamSize;
    //    m_feedbackCached.Remove(task.m_statusReportNumber);
    //    return MFX_ERR_NONE;

    //case ENCODE_NOTREADY:
    //    return MFX_WRN_DEVICE_BUSY;

    //case ENCODE_NOTAVAILABLE:
    //case ENCODE_ERROR:
    //default:
    //    assert(!"bad feedback status");
    //    return MFX_ERR_DEVICE_FAILED;
    //}

    task;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::QueryStatus(DdiTask & task)


mfxStatus D3D11Encoder::UpdateBitstream(
    mfxMemId       MemId,
    Task          & task)
{
    //mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    //IppiSize     roi       = {task.m_bsDataLength, 1};
    //mfxFrameData bitstream = {0};

    //m_core->LockFrame(MemId, &bitstream);
    //MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    //IppStatus sts = ippiCopyManaged_8u_C1R(
    //    (Ipp8u *)bitstream.Y, task.m_bsDataLength,
    //    bsData, task.m_bsDataLength,
    //    roi, IPP_NONTEMPORAL_LOAD);
    //assert(sts == ippStsNoErr);

    //task.bs->DataLength += task.m_bsDataLength;
    //m_core->UnlockFrame(MemId, &bitstream);

    //return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;

    MemId; task;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::UpdateBitstream(...) */


mfxStatus D3D11Encoder::Destroy()
{
    //mfxStatus sts = MFX_ERR_NONE;

    //if (m_pAuxDevice)
    //{
    //    sts = m_pAuxDevice->ReleaseAccelerationService();
    //    m_pAuxDevice->Release();
    //    delete m_pAuxDevice;
    //    m_pAuxDevice = 0;
    //}
    //if (m_pDdiData)
    //{
    //    m_pDdiData->Close();
    //    delete m_pDdiData;
    //    m_pDdiData = 0;
    //}

    //return sts;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::Destroy()
#endif 
}

#endif 