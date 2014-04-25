/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp8_encode_utils_hybrid_hw.h"
#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE_HW) 
#include "libmfx_core_vaapi.h"
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"
#include "assert.h"

namespace MFX_VP8ENC
{    
    mfxStatus QueryHwCaps(VideoCORE* core, ENCODE_CAPS_VP8 & caps)
    {
        // [SE] removed creation of temporal core for querying HW CAPS
        // [SE] will use main core object
        //std::auto_ptr<VideoCORE>      pCore(FactoryCORE::CreateCORE(va_type, adapterNum, 1));
        
        std::auto_ptr<DriverEncoder> ddi;

        ddi.reset(CreatePlatformVp8Encoder(core));
        MFX_CHECK(ddi.get() != NULL, MFX_WRN_PARTIAL_ACCELERATION);

        mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, DXVA2_Intel_Encode_VP8, 640, 480);
        MFX_CHECK_STS(sts);
            
        sts = ddi.get()->QueryEncodeCaps(caps);
        MFX_CHECK_STS(sts);

#if defined (VP8_HYBRID_FALL_TO_SW)
        return MFX_ERR_UNSUPPORTED;
#else // VP8_HYBRID_FALL_TO_SW
        return MFX_ERR_NONE;
#endif // VP8_HYBRID_FALL_TO_SW
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
            sts =  MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;       
        }
        return sts;    
    }

    DriverEncoder* CreatePlatformVp8Encoder( VideoCORE* core )
    {
#if defined (MFX_VA_WIN) 
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
#endif
#if defined(MFX_VA_LINUX)
        if (MFX_HW_VAAPI <= core->GetVAType())
        {
           return new VAAPIEncoder;
        }
#endif
        return NULL;
    } 
   


#if defined (MFX_VA_WIN)
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

 /* ----------- Functions to convert MediaSDK into DDI --------------------- */

    void FillSpsBuffer(mfxVideoParam const & par, ENCODE_SET_SEQUENCE_PARAMETERS_VP8 & sps)
    {
        Zero(sps);

        sps.wFrameWidth  = par.mfx.FrameInfo.CropW!=0 ? par.mfx.FrameInfo.CropW :  par.mfx.FrameInfo.Width;
        sps.wFrameHeight = par.mfx.FrameInfo.CropH!=0 ? par.mfx.FrameInfo.CropH :  par.mfx.FrameInfo.Height; 
        sps.GopPicSize  = par.mfx.GopPicSize;

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
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            sps.FramesPer100Sec[0]             = mfxU16(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    } 

    mfxStatus FillPpsBuffer(TaskHybridDDI const & task, mfxVideoParam const & par, ENCODE_SET_PICTURE_PARAMETERS_VP8 & pps)
    {
        mfxExtCodingOptionVP8 * opts = GetExtBuffer(par);
        mfxExtCodingOptionVP8Param * VP8Par = GetExtBuffer(par);
        MFX_CHECK_NULL_PTR1(opts);

        Zero(pps);

        pps.version          = VP8Par->VP8Version;
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
            pps.ref_frame_ctrl =  pps.ref_frame_ctrl | 0x01;
        }

        pps.GoldenRefPic.bPicEntry                    = 0xff;
        if(task.m_pRecRefFrames[REF_GOLD])
        {
            pps.GoldenRefPic.Index7Bits      = mfxU8(task.m_pRecRefFrames[REF_GOLD]->idInPool);
            pps.GoldenRefPic.AssociatedFlag  = 0;
            if (task.m_pRecRefFrames[REF_GOLD] != task.m_pRecRefFrames[REF_BASE])
                pps.ref_frame_ctrl =  pps.ref_frame_ctrl | 0x02;
        }

        pps.AltRefPic.bPicEntry                    = 0xff;
        if(task.m_pRecRefFrames[REF_ALT])
        {
            pps.AltRefPic.Index7Bits      = mfxU8(task.m_pRecRefFrames[REF_ALT]->idInPool);
            pps.AltRefPic.AssociatedFlag  = 0;
            if (task.m_pRecRefFrames[REF_ALT] != task.m_pRecRefFrames[REF_GOLD] &&
                task.m_pRecRefFrames[REF_ALT] != task.m_pRecRefFrames[REF_BASE])
                pps.ref_frame_ctrl =  pps.ref_frame_ctrl | 0x04;
        }

        pps.frame_type = (task.m_sFrameParams.bIntra) ? 0 : 1;

        pps.StatusReportFeedbackNumber = task.m_frameOrder + 1;

        pps.segmentation_enabled     = opts->EnableMultipleSegments;

        pps.filter_type              = task.m_sFrameParams.LFType;
        pps.loop_filter_adj_enable   = VP8Par->RefTypeLFDelta[0] || VP8Par->RefTypeLFDelta[1] || VP8Par->RefTypeLFDelta[2] || VP8Par->RefTypeLFDelta[3] ||
            VP8Par->MBTypeLFDelta[0] || VP8Par->MBTypeLFDelta[1] || VP8Par->MBTypeLFDelta[2] || VP8Par->MBTypeLFDelta[3];

        // hardcode loop_filter_adj_enable value for key-frames to align with C-model
        if (pps.frame_type == 0)
            pps.loop_filter_adj_enable = 1;

        pps.CodedCoeffTokenPartition = VP8Par->NumPartitions;

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
        pps.mb_no_coeff_skip         = 1;
        pps.sharpness_level          = task.m_sFrameParams.Sharpness;

        for (int i = 0; i < 4; i ++)
        {
            pps.loop_filter_level[i] = task.m_sFrameParams.LFLevel[i];
            if (pps.loop_filter_adj_enable)
            {
                pps.ref_lf_delta[i]      = VP8Par->RefTypeLFDelta[i];
                pps.mode_lf_delta[i]     = VP8Par->MBTypeLFDelta[i];
            }
        }

        return MFX_ERR_NONE;
      
    } 
    mfxStatus FillQuantBuffer(TaskHybridDDI const & task, mfxVideoParam const & par,ENCODE_SET_QUANT_DATA_VP8 & quant)
    {
        MFX_CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CQP, MFX_ERR_UNSUPPORTED);
        UCHAR QP = UCHAR(task.m_sFrameParams.bIntra ? par.mfx.QPI:par.mfx.QPP);
        mfxExtCodingOptionVP8Param * VP8Par = GetExtBuffer(par);
        for (int i = 0; i < 4; i ++)
        {
            quant.QIndex[i] = QP + VP8Par->SegmentQPDelta[i];
        }
        for (int i = 0; i < 5; i ++)
        {
            quant.QIndexDelta[i] = VP8Par->CTQPDelta[i];
        }
        return MFX_ERR_NONE;
    }

    mfxStatus FillFrameUpdateBuffer(TaskHybridDDI const & task,ENCODE_CPUPAK_FRAMEUPDATE_VP8 & frmUpdate)
    {
        frmUpdate.PrevFrameSize = (UINT)task.m_prevFrameSize;
        frmUpdate.TwoPrevFrameFlag = task.m_brcUpdateDelay == 2 ? 1 : 0;
        for (mfxU8 i = 0; i < 4; i++)
        {
            frmUpdate.IntraModeCost[i] = task.m_costs.IntraModeCost[i];
            frmUpdate.InterModeCost[i] = task.m_costs.InterModeCost[i];
            frmUpdate.RefFrameCost[i]  = task.m_costs.RefFrameCost[i];
        }
        frmUpdate.IntraNonDCPenalty16x16 = task.m_costs.IntraNonDCPenalty16x16;
        frmUpdate.IntraNonDCPenalty4x4 = task.m_costs.IntraNonDCPenalty4x4;

        return MFX_ERR_NONE;
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
#if !defined (VP8_HYBRID_DUMP_READ)
    sts = auxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);
#endif

#if !defined (VP8_HYBRID_DUMP_READ)
    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);
#endif

    ENCODE_CAPS_VP8 caps = { 0, };
#if !defined (VP8_HYBRID_DUMP_READ)
    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif

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

#if !defined (VP8_HYBRID_DUMP_READ)
    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif

    Zero(m_capsQuery);
#if !defined (VP8_HYBRID_DUMP_READ)
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif

    Zero(m_capsGet);
#if !defined (VP8_HYBRID_DUMP_READ)
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif

    Zero(m_layout);
#if !defined (VP8_HYBRID_DUMP_READ)
    hr = m_auxDevice->Execute(MBDATA_LAYOUT_ID, (void *)0, m_layout);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_quant);
    Zero(m_frmUpdate);

    FillSpsBuffer(par, m_sps);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::CreateAccelerationService(mfxVideoParam const & par)


mfxStatus D3D9Encoder::Reset(mfxVideoParam const & par)
{
    Zero(m_sps);
    Zero(m_pps);
    Zero(m_quant);
    Zero(m_frmUpdate);


    FillSpsBuffer(par, m_sps);
    m_video = par;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Reset(mfxVideoParam const & par)

mfxU32 D3D9Encoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_NV12;
}

mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    frameWidth; frameHeight;
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    if (type == D3DDDIFMT_INTELENCODE_SEGMENTMAP || 
        type == D3DDDIFMT_INTELENCODE_DISTORTIONDATA)
    {
        // [SE]: WA to have same pipeline for Windows/Linux
        request.NumFrameMin = request.NumFrameSuggested = 0;
        return MFX_ERR_UNSUPPORTED;
    }

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
mfxStatus D3D9Encoder::QueryMBLayout(MBDATA_LAYOUT &layout)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    layout = m_layout;
    return MFX_ERR_NONE;
}

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

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA ||
        type == D3DDDIFMT_INTELENCODE_MBDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;

}

mfxStatus D3D9Encoder::Execute(TaskHybridDDI const &task, mfxHDL surface)
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
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 0;

    UINT& bufCnt = encodeExecuteParams.NumCompBuffers;    
    
    // input data
    if(task.m_frameOrder == 0)
    {
        m_sps.TargetUsage = 0; // [SE] should it be here?
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

    // frame update
    if (task.m_frameOrder)
    {
        FillFrameUpdateBuffer(task, m_frmUpdate);
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SLICEDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_frmUpdate));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &m_frmUpdate;
        bufCnt++;
    }

    //mfxU32 distort = task.m_pRecFrame->idInPool;
    //encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_DISTORTIONDATA;
    //encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(distort));
    //encodeCompBufferDesc[bufCnt].pCompBuffer = &distort;
    //bufCnt++;

    try
    {

        // [SE] WA for Windows hybrid VP8 HSW driver (remove 'protected' here)
#if defined (VP8_HYBRID_TIMING)
        TICK(submit[task.m_frameOrder % 2])
#endif // VP8_HYBRID_TIMING
#if defined (MFX_VA_WIN)
        EmulSurfaceRegister surfaceReg;
        Zero(surfaceReg);
        surfaceReg.type = D3DDDIFMT_INTELENCODE_MBDATA;
        surfaceReg.num_surf = hackResponse.NumFrameActual;

        mfxStatus sts = m_core->GetFrameHDL(hackResponse.mids[task.m_pRecFrame->idInPool], (mfxHDL *)&surfaceReg.surface[0]);
        sts = sts;
        HRESULT hrtemp = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg); // &m_uncompBufInfo[0]
        MFX_CHECK(SUCCEEDED(hrtemp), MFX_ERR_DEVICE_FAILED);
#endif
        //printf("start - begin frame %d\n", task.m_pRecFrame->idInPool);
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        //printf("end - begin frame %d (status %d)\n", task.m_pRecFrame->idInPool, hr);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        //printf("start - execute frame %d\n", task.m_pRecFrame->idInPool);
        hr = m_auxDevice->Execute(ENCODE_ENC_ID, encodeExecuteParams, (void *)0);
        //printf("end - execute frame %d (status %d)\n", task.m_pRecFrame->idInPool, hr);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    
        HANDLE handle;
        //printf("start - end frame %d\n", task.m_pRecFrame->idInPool);
        m_auxDevice->EndFrame(&handle);
        //printf("end - end frame %d\n", task.m_pRecFrame->idInPool);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

#if defined (VP8_HYBRID_TIMING)
        TOCK(submit[task.m_frameOrder % 2])
        TICK(hwAsync[task.m_frameOrder % 2])
#endif // VP8_HYBRID_TIMING

    return MFX_ERR_NONE;

} 


mfxStatus D3D9Encoder::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses. 

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_frameOrder + 1);

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
            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
#ifdef NEW_STATUS_REPORTING_DDI_0915
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
#else // NEW_STATUS_REPORTING_DDI_0915
                (void *)0,
                0,
#endif // NEW_STATUS_REPORTING_DDI_0915
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

        feedback = m_feedbackCached.Hit(task.m_frameOrder + 1);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        //aya: fixme
        //task.m_bsDataLength = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_frameOrder + 1);
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
/*
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

mfxU32 D3D11Encoder::GetReconSurfFourCC()
{
    return 0;
}

mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
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

    type; request; frameWidth; frameHeight;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameSizeInMBs = 0)


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
mfxStatus D3D11Encoder::QueryMBLayout(MBDATA_LAYOUT & /*layout*/)
{
    //MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    //layout = m_layout;
    return MFX_ERR_UNSUPPORTED;
}



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


mfxStatus D3D11Encoder::Execute(const TaskHybridDDI &task, mfxHDL surface)
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


mfxStatus D3D11Encoder::QueryStatus(Task & /*task*/)
{
    return MFX_ERR_UNSUPPORTED;
}
    /*//MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
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
#endif

#if defined(MFX_VA_LINUX)

#define ALIGN(x, align) (((x) + (align) - 1) & (~((align) - 1)))

 /* ----------- Functions to convert MediaSDK into DDI --------------------- */

    void FillSpsBuffer(mfxVideoParam const & par, VAEncSequenceParameterBufferVP8 & sps)
    {
        Zero(sps);

        sps.frame_width  = par.mfx.FrameInfo.CropW!=0 ? par.mfx.FrameInfo.CropW :  par.mfx.FrameInfo.Width;
        sps.frame_height = par.mfx.FrameInfo.CropH!=0 ? par.mfx.FrameInfo.CropH :  par.mfx.FrameInfo.Height; 
        sps.frame_width_scale = 0;
        sps.frame_height_scale = 0;

        /*OG: are those parameters used for full ENCODE Mode only ??? */

        sps.error_resilient = 0; 
        sps.kf_auto = 0; 
        sps.kf_min_dist = 1;
        sps.kf_max_dist = par.mfx.GopRefDist;
        sps.bits_per_second = par.mfx.TargetKbps*1000;    //     
        sps.intra_period  = par.mfx.GopPicSize;
    } 

    mfxStatus FillPpsBuffer(
        TaskHybridDDI const & task,
        mfxVideoParam const & par,
        VAEncPictureParameterBufferVP8 & pps,
        std::vector<ExtVASurface> const & reconQueue)
    {
        mfxExtCodingOptionVP8 * opts = GetExtBuffer(par);
        mfxExtCodingOptionVP8Param * VP8Par = GetExtBuffer(par);
        MFX_CHECK_NULL_PTR1(opts);

        Zero(pps);

        pps.pic_flags.bits.version     = VP8Par->VP8Version;
        pps.pic_flags.bits.color_space = 0;

        pps.ref_last_frame = pps.ref_gf_frame = pps.ref_arf_frame = VA_INVALID_SURFACE;

        MFX_CHECK(task.m_pRecFrame->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
        pps.reconstructed_frame = reconQueue[task.m_pRecFrame->idInPool].surface;

        pps.ref_flags.value = 0; // use all references
        if ( task.m_pRecRefFrames[REF_BASE])
        {
            MFX_CHECK(task.m_pRecRefFrames[REF_BASE]->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
            pps.ref_last_frame = reconQueue[task.m_pRecRefFrames[REF_BASE]->idInPool].surface;
        }
        if ( task.m_pRecRefFrames[REF_GOLD])
        {
            MFX_CHECK(task.m_pRecRefFrames[REF_GOLD]->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
            pps.ref_gf_frame = reconQueue[task.m_pRecRefFrames[REF_GOLD]->idInPool].surface;
        }
        if ( task.m_pRecRefFrames[REF_ALT])
        {
            MFX_CHECK(task.m_pRecRefFrames[REF_ALT]->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
            pps.ref_arf_frame = reconQueue[task.m_pRecRefFrames[REF_ALT]->idInPool].surface;
        }

        pps.pic_flags.bits.frame_type                      = (task.m_sFrameParams.bIntra) ? 0 : 1;
        pps.pic_flags.bits.segmentation_enabled           = opts->EnableMultipleSegments;
        
        pps.pic_flags.bits.loop_filter_type               = task.m_sFrameParams.LFType;
        pps.pic_flags.bits.loop_filter_adj_enable         = VP8Par->RefTypeLFDelta[0] || VP8Par->RefTypeLFDelta[1] || VP8Par->RefTypeLFDelta[2] || VP8Par->RefTypeLFDelta[3] ||
            VP8Par->MBTypeLFDelta[0] || VP8Par->MBTypeLFDelta[1] || VP8Par->MBTypeLFDelta[2] || VP8Par->MBTypeLFDelta[3];

        // hardcode loop_filter_adj_enable value for key-frames to align with C-model
        if (pps.pic_flags.bits.frame_type == 0)
            pps.pic_flags.bits.loop_filter_adj_enable = 1;
        
        pps.pic_flags.bits.num_token_partitions           = VP8Par->NumPartitions;
   
        if (pps.pic_flags.bits.frame_type)
        {
            pps.pic_flags.bits.refresh_golden_frame = 0; 
            pps.pic_flags.bits.refresh_alternate_frame = 0; 
            pps.pic_flags.bits.copy_buffer_to_golden = 1;            
            pps.pic_flags.bits.copy_buffer_to_alternate = 2; 
            pps.pic_flags.bits.refresh_last = 1;                   
        }  

        pps.pic_flags.bits.sign_bias_golden         = 0;
        pps.pic_flags.bits.sign_bias_alternate      = 0;
        pps.pic_flags.bits.mb_no_coeff_skip         = 1;
  
        pps.sharpness_level          = task.m_sFrameParams.Sharpness;;


        for (int i = 0; i < 4; i ++)
        {
            pps.loop_filter_level[i] = task.m_sFrameParams.LFLevel[i];
            if (pps.pic_flags.bits.loop_filter_adj_enable)
            {
                pps.ref_lf_delta[i]      = VP8Par->RefTypeLFDelta[i];
                pps.mode_lf_delta[i]     = VP8Par->MBTypeLFDelta[i];
            }
        }

        pps.pic_flags.bits.refresh_entropy_probs          = 0;
        pps.pic_flags.bits.forced_lf_adjustment           = 0;
        pps.pic_flags.bits.show_frame                     = 1;
        pps.pic_flags.bits.recon_filter_type              = 3;
        pps.pic_flags.bits.auto_partitions                = 0;
        pps.pic_flags.bits.clamping_type                  = 0;
        pps.pic_flags.bits.update_mb_segmentation_map     = 0;
        pps.pic_flags.bits.update_segment_feature_data    = 0;

        /*OG: is this parameters used for full ENCODE Mode only ??? */
        pps.clamp_qindex_high = 127;
        pps.clamp_qindex_low = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus FillQuantBuffer(TaskHybridDDI const & task, mfxVideoParam const & par,VAQMatrixBufferVP8& quant)
    {
        MFX_CHECK(par.mfx.RateControlMethod == MFX_RATECONTROL_CQP, MFX_ERR_UNSUPPORTED);
        mfxExtCodingOptionVP8Param * VP8Par = GetExtBuffer(par);

        for (int i = 0; i < 4; i ++)
        {
            quant.quantization_index[i] = task.m_sFrameParams.bIntra ? par.mfx.QPI:par.mfx.QPP
            + VP8Par->SegmentQPDelta[i];
        }
        // delta for YDC, Y2AC, Y2DC, UAC, UDC
        for (int i = 0; i < 5; i ++)
        {
            quant.quantization_index_delta[i] = VP8Par->CTQPDelta[i];
        }

        return MFX_ERR_NONE;
    }

#define LEFT(i) 3*i
#define RIGHT(i) 3*i + 1
#define ROIID(i) 3*i + 2

    mfxStatus FillSegMap(
        TaskHybridDDI const & task,
        mfxVideoParam const & par,
        VideoCORE           * core,
        VAEncMiscParameterVP8SegmentMapParams& segMapPar)
    {
        // TODO: get real segmentation map here, from ROI or another interface
        if (task.ddi_frames.m_pSegMap_hw == 0)
            return MFX_ERR_NONE; // segment map isn't required
        FrameLocker lockSegMap(core, task.ddi_frames.m_pSegMap_hw->pSurface->Data, false);
        mfxU32 frameWidthInMBs  = par.mfx.FrameInfo.Width / 16;
        mfxU32 frameHeightInMBs = par.mfx.FrameInfo.Height / 16;
        mfxU32 segMapPitch = ALIGN(frameWidthInMBs, 64);
        mfxU32 segMapAlign = segMapPitch - frameWidthInMBs;
        mfxU8 *bufPtr = task.ddi_frames.m_pSegMap_hw->pSurface->Data.Y;

        // fill segmentation map from ROI
        mfxExtEncoderROI * pExtRoi = GetExtBuffer(par);
        mfxU32 roiMBHorizLimits[9];
        mfxU32 numRoiPerRow;
        mfxU32 i = 0;

        memset(segMapPar.yac_quantization_index_delta, 0, 4);
        
        if (pExtRoi->NumROI)
        {
            for (i = 0; i < pExtRoi->NumROI; i ++)
            {
                segMapPar.yac_quantization_index_delta[i] = pExtRoi->ROI[i].Priority;
            }
            for (mfxU32 row = 0; row < frameHeightInMBs; row ++)
            {
                numRoiPerRow = 0;
                for (i = 0; i < pExtRoi->NumROI; i ++)
                {
                    if (row >= (pExtRoi->ROI[i].Top >> 4) && (row < pExtRoi->ROI[i].Bottom >> 4))
                    {
                        roiMBHorizLimits[LEFT(numRoiPerRow)] = pExtRoi->ROI[i].Left >> 4;
                        roiMBHorizLimits[RIGHT(numRoiPerRow)] = pExtRoi->ROI[i].Right >> 4;
                        roiMBHorizLimits[ROIID(numRoiPerRow)] = i;
                        numRoiPerRow ++;
                    }
                }
                if (numRoiPerRow)
                {
                    for (mfxU32 col = 0; col < frameWidthInMBs; col ++)
                    {
                        for (i = 0; i < numRoiPerRow; i ++)
                        {
                            if (col >= roiMBHorizLimits[LEFT(i)] && col < roiMBHorizLimits[RIGHT(i)])
                            {
                                bufPtr[segMapPitch * row + col] = roiMBHorizLimits[ROIID(i)];
                                break;
                            }
                        }
                        if (i == numRoiPerRow)
                        {
                            if (pExtRoi->NumROI < 4)
                                bufPtr[segMapPitch * row + col] = pExtRoi->NumROI;
                            else
                                return MFX_ERR_DEVICE_FAILED;
                        }
                    }
                }
                else
                    memset(&(bufPtr[segMapPitch * row]), pExtRoi->NumROI, frameWidthInMBs);

                memset(&(bufPtr[segMapPitch * row + frameWidthInMBs]), 0, segMapAlign);
            }            
        }

#if 0        
        printf("\n\n");
        for (mfxU32 row = 0; row < frameHeightInMBs; row ++)
        {
            for (mfxU32 col = 0; col < segMapPitch; col ++)
            {
                printf("%02x(%3d) ", bufPtr[segMapPitch * row + col], pExtRoi->ROI[bufPtr[segMapPitch * row + col]].Priority);
            }
            printf("\n");
        }
        printf("\n\n");
        fflush(0);
#endif
    }
    
    mfxStatus FillFrameUpdateBuffer(TaskHybridDDI const & task, VAEncHackTypeVP8HybridFrameUpdate & frmUpdate)
    {
        frmUpdate.prev_frame_size = (UINT)task.m_prevFrameSize;
        frmUpdate.two_prev_frame_flag = task.m_brcUpdateDelay == 2 ? 1 : 0;
        for (mfxU8 i = 0; i < 4; i++)
        {
            frmUpdate.intra_mode_cost[i] = task.m_costs.IntraModeCost[i];
            frmUpdate.inter_mode_cost[i] = task.m_costs.InterModeCost[i];
            frmUpdate.ref_frame_cost[i]  = task.m_costs.RefFrameCost[i];
        }
        frmUpdate.intra_non_dc_penalty_16x16 = task.m_costs.IntraNonDCPenalty16x16;
        frmUpdate.intra_non_dc_penalty_4x4 = task.m_costs.IntraNonDCPenalty4x4;

        return MFX_ERR_NONE;
    }    
    
mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
{
    switch (rateControl)
    {
    case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
    case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
    case MFX_RATECONTROL_AVBR: return VA_RC_VBR;
    case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
    default: assert(!"Unsupported RateControl"); return 0;
    }

} // mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
    
mfxStatus SetFrameRate(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & frameRateBufId)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBufId != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, frameRateBufId);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                   m_vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBufId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                 frameRateBufId,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    frameRate_param->framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);

    vaUnmapBuffer(m_vaDisplay, frameRateBufId);

    return MFX_ERR_NONE;
}

VAAPIEncoder::VAAPIEncoder()
: m_core(NULL)
, m_vaDisplay(0)
, m_vaContextEncode(0)
, m_vaConfig(0)
, m_spsBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_qMatrixBufferId(VA_INVALID_ID)
, m_frmUpdateBufferId(VA_INVALID_ID)
, m_segMapParBufferId(VA_INVALID_ID)
, m_frameRateBufferId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE* core,
    GUID /*guid*/,
    mfxU32 width,
    mfxU32 height)
{
    m_core = core;

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);
    if(hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    m_width  = width;
    m_height = height;

    // set encoder CAPS on our own for now
    memset(&m_caps, 0, sizeof(m_caps));
    m_caps.MaxPicWidth = 1920;
    m_caps.MaxPicHeight = 1080;    
    m_caps.HybridPakFunc = 1;
    m_caps.SegmentationAllowed = 1; // [SE] turn on segmentation by default, is it correct?
    
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(mfxVideoParam const & par)
{
    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces when call vaCreateContext().
     * Here we don't have this info.
     */
        m_video = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                VAProfileVP8Version0_3,
                Begin(pEntrypoints),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        // [SE] VAEntrypointHybridEncSlice is entry point for Hybrid VP8 encoder        
        if( VAEntrypointHybridEncSlice == pEntrypoints[entrypointsIndx] )        
        {
            bEncodeEnable = true;
            break;
        }
    }
    if( !bEncodeEnable )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // Configuration
    VAConfigAttrib attrib[2];

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          VAProfileVP8Version0_3,
                          (VAEntrypoint)VAEntrypointHybridEncSlice,
                          &attrib[0], 2);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod); // [SE] at the moment there is no BRC for VP8 on driver side

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        VAProfileVP8Version0_3,
        (VAEntrypoint)VAEntrypointHybridEncSlice,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        VA_PROGRESSIVE,
        Begin(reconSurf),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED); 

    Zero(m_sps);
    Zero(m_pps);
 
    //------------------------------------------------------------------

    FillSpsBuffer(par, m_sps);
    // pass frame rate in the same manner as for AVC (number of frames per 100 sec)
    m_frameRate.framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);

    vpgQueryBufferAttributes pfnVaQueryBufferAttr = NULL;
    pfnVaQueryBufferAttr = (vpgQueryBufferAttributes)vaGetLibFunc(m_vaDisplay, VPG_QUERY_BUFFER_ATTRIBUTES);

    if (pfnVaQueryBufferAttr)
    {
        //VAEncMbDataLayout VaMbLayout;
        memset(&m_layout, 0, sizeof(m_layout));
        mfxU32 bufferSize = sizeof(VAEncMbDataLayout);
        
        vaSts = pfnVaQueryBufferAttr(
          m_vaDisplay,
          m_vaContextEncode,
          (VABufferType)VAEncMbDataBufferType,
          &m_layout,
          &bufferSize);
        MFX_CHECK_WITH_ASSERT(bufferSize == sizeof(VAEncMbDataLayout), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    else
      return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(mfxVideoParam const & par)
{
    m_video = par;

    FillSpsBuffer(par, m_sps);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)

mfxU32 VAAPIEncoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_VP8_NV12;
} // mfxU32 VAAPIEncoder::GetReconSurfFourCC()

#define HYBRIDPAK_PER_MB_DATA_SIZE     816
#define HYBRIDPAK_PER_MB_MV_DATA_SIZE  64

DWORD CalculateMbSurfaceHeight (DWORD dwNumMBs)
{
    DWORD dwMemoryNeededPerMb           = HYBRIDPAK_PER_MB_DATA_SIZE + HYBRIDPAK_PER_MB_MV_DATA_SIZE; // Both MB code & Motion vectors are in same surface
    DWORD dwMemoryAllocatedPerMb        = ALIGN((HYBRIDPAK_PER_MB_DATA_SIZE + HYBRIDPAK_PER_MB_MV_DATA_SIZE), 32); // 2D Linear Surface's picth is atleast aligned to 32 bytes
    DWORD dwBytesNeededFor4KAlignment   = 0x1000 - ((HYBRIDPAK_PER_MB_DATA_SIZE * dwNumMBs) & 0xfff); // The MV data shall start at 4k aligned offset in the MB surface
    DWORD dwExtraBytesAvailableInBuffer = (dwMemoryAllocatedPerMb  -  dwMemoryNeededPerMb) * dwNumMBs; // Extra bytes available due to pitch alignment

    if (dwBytesNeededFor4KAlignment > dwExtraBytesAvailableInBuffer)
    {
        // Increase the Surface height to cater to the 4k alignment
        dwNumMBs += ((dwBytesNeededFor4KAlignment - dwExtraBytesAvailableInBuffer) + dwMemoryAllocatedPerMb - 1) / dwMemoryAllocatedPerMb;
    }

    return dwNumMBs;
}

mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    if (type == D3DDDIFMT_INTELENCODE_MBDATA)
    {
        mfxU32 frameSizeInMBs = (frameWidth * frameHeight) / 256;
        request.Info.FourCC = MFX_FOURCC_VP8_MBDATA; // vaCreateSurface is required to allocate surface for MB data
        request.Info.Width = ALIGN((HYBRIDPAK_PER_MB_DATA_SIZE + HYBRIDPAK_PER_MB_MV_DATA_SIZE), 32);
        request.Info.Height = CalculateMbSurfaceHeight(frameSizeInMBs + 1); // additional "+1" memory is used to return QP and loop filter levels from BRC to MSDK
    }
    else if (type == D3DDDIFMT_INTELENCODE_SEGMENTMAP)
    {
        request.Info.FourCC = MFX_FOURCC_VP8_SEGMAP;
        request.Info.Width  = ALIGN(frameWidth / 16, 64);
        request.Info.Height = frameHeight / 16;
    }

    if (type == D3DDDIFMT_INTELENCODE_DISTORTIONDATA)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // context_id required for allocation video memory (tmp solution) 
    request.reserved[0] = m_vaContextEncode;
    
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS_VP8& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::QueryMBLayout(MBDATA_LAYOUT & layout)
{
    layout = m_layout;

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if( D3DDDIFMT_INTELENCODE_MBDATA == type
    )
    {
        pQueue = &m_mbDataQueue;
    }
    else if (D3DDDIFMT_INTELENCODE_SEGMENTMAP == type)
    {
        pQueue = &m_segMapQueue;
    }
    else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++)
        {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);
            
            extSurf.surface = *pSurface;

            pQueue->push_back( extSurf );
        }
    }

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type &&
        D3DDDIFMT_INTELENCODE_MBDATA != type &&
        D3DDDIFMT_INTELENCODE_SEGMENTMAP != type)
    {
        sts = CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)
{
    memId;
    type;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)


bool operator==(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return memcmp(&l, &r, sizeof(ENCODE_ENC_CTRL_CAPS)) == 0;
}

bool operator!=(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return !(l == r);
}

mfxStatus VAAPIEncoder::Execute(    
    TaskHybridDDI const & task,
    mfxHDL          surface)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");

    VAStatus vaSts;

    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VASurfaceID reconSurface;
    VABufferID codedBuffer;
    mfxU32 i;    

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT);
    mfxU16 buffersCount = 0;

    // update params
    FillPpsBuffer(task, m_video, m_pps, m_reconQueue);
    FillQuantBuffer(task, m_video, m_quant);
    FillSegMap(task, m_video, m_core, m_segMapPar);
    FillFrameUpdateBuffer(task, m_frmUpdate);

//===============================================================================================    

    //------------------------------------------------------------------
    // find bitstream    
    mfxU32 idxInPool = task.m_pRecFrame->idInPool;    
    if( idxInPool < m_mbDataQueue.size() )
    {
        codedBuffer = m_mbDataQueue[idxInPool].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    // [SE] should we recieve MB data through coded_buf?
    m_pps.coded_buf = codedBuffer;

    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        {
            MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),
                                   1,
                                   &m_sps,
                                   &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_spsBufferId;
        }

        // 2. Picture level
        {
            MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPictureParameterBufferType,
                                   sizeof(m_pps),
                                   1,
                                   &m_pps,
                                   &m_ppsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_ppsBufferId;
        }
        
        // 3. Quantization matrix
        {
            MFX_DESTROY_VABUFFER(m_qMatrixBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAQMatrixBufferType,
                                   sizeof(m_quant),
                                   1,
                                   &m_quant,
                                   &m_qMatrixBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_qMatrixBufferId;
        }

        // 4. Segmentation map
        if (task.ddi_frames.m_pSegMap_hw != 0)
        {
            // segmentation map buffer is already allocated and filled. Need just to attach it
            configBuffers[buffersCount++] = m_segMapQueue[idxInPool].surface;
        }
        if (task.m_frameOrder)
        // 5. Per-segment parameters (temporal "hacky" solution)
        {
            MFX_DESTROY_VABUFFER(m_segMapParBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   (VABufferType)VAEncMiscParameterTypeVP8SegmentMapParams,
                                   sizeof(m_segMapPar),
                                   1,
                                   &m_segMapPar,
                                   &m_segMapParBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_segMapParBufferId;
        }

        // 6. Frame update (temporal "hacky" solution)
        if (task.m_frameOrder)
        {
            MFX_DESTROY_VABUFFER(m_frmUpdateBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   (VABufferType)VAEncHackTypeVP8HybridFrameUpdate,
                                   sizeof(m_frmUpdate),
                                   1,
                                   &m_frmUpdate,
                                   &m_frmUpdateBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_frmUpdateBufferId;
        }

        // 7. Frame rate (temporal "hacky" solution)
        if (m_video.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            MFX_DESTROY_VABUFFER(m_frameRateBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                (VABufferType)VAEncHackTypeVP8HybridFrameRate,
                                sizeof(m_frameRate),
                                1,
                                &m_frameRate,
                                &m_frameRateBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_frameRateBufferId;
        }
    }

    assert(buffersCount <= configBuffers.size());
#if defined (VP8_HYBRID_TIMING)
    TICK(submit[task.m_frameOrder % 2])
#endif // VP8_HYBRID_TIMING

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaBeginPicture");
        vaSts = vaBeginPicture(
            m_vaDisplay,
            m_vaContextEncode,
            *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaRenderPicture");
        vaSts = vaRenderPicture(
            m_vaDisplay,
            m_vaContextEncode,
            Begin(configBuffers),
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    
#if defined (VP8_HYBRID_TIMING)
        TOCK(submit[task.m_frameOrder % 2])
        TICK(hwAsync[task.m_frameOrder % 2])
#endif // VP8_HYBRID_TIMING    

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        ExtVASurface currentFeedback;
        currentFeedback.surface = *inputSurface;
        currentFeedback.number = task.m_frameOrder;
        currentFeedback.idxBs    = idxInPool;
        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & mb data buffer
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
        {
            ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

            if( currentFeedback.number == task.m_frameOrder)
            {
                waitSurface = currentFeedback.surface;
                waitIdxBs   = currentFeedback.idxBs;

                isFound  = true;

                break;
            }
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    // find used mb data buffer
    VABufferID codedBuffer;
    if( waitIdxBs < m_mbDataQueue.size())
    {
        codedBuffer = m_mbDataQueue[waitIdxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

#if 1
    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
#else
    VASurfaceStatus surfSts = VASurfaceSkipped;
    while (surfSts != VASurfaceReady)
    {
        vaSts = vaQuerySurfaceStatus(m_vaDisplay,  waitSurface, &surfSts);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
#endif

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );
    }

    return MFX_ERR_NONE;
#else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    switch (surfSts)
    {
        case VASurfaceReady:
            // remove task
            m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );
            return MFX_ERR_NONE;

        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
        default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
    }
#endif
} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus VAAPIEncoder::Destroy()
{
    if( m_vaContextEncode )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextEncode );
        m_vaContextEncode = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }

    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);    
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_qMatrixBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_segMapParBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frmUpdateBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frameRateBufferId, m_vaDisplay);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Destroy()
#endif
}

#endif 
