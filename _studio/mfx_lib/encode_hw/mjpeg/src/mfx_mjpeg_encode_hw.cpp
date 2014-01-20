/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
//          MJPEG HW encoder
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA_WIN)

#include "mfx_mjpeg_encode_hw.h"
#include "libmfx_core_d3d9.h"
#include "mfx_task.h"
#include "umc_defs.h"
#include "ipps.h"

using namespace MfxHwMJpegEncode;

MFXVideoENCODEMJPEG_HW::MFXVideoENCODEMJPEG_HW(VideoCORE *core, mfxStatus *sts)
{
    m_pCore        = core;
    m_bInitialized = false;
    m_deviceFailed = false;
    m_counter      = 1;

    *sts = (core ? MFX_ERR_NONE : MFX_ERR_NULL_PTR);
}

mfxStatus MFXVideoENCODEMJPEG_HW::Query(VideoCORE * core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR2(core, out);

    if (in == 0)
    {
        memset(&out->mfx, 0, sizeof(out->mfx));

        out->IOPattern             = 1;
        out->Protected             = 1;
        out->AsyncDepth            = 1;
        out->mfx.CodecId           = MFX_CODEC_JPEG;
        out->mfx.CodecProfile      = MFX_PROFILE_JPEG_BASELINE;

        out->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        out->mfx.FrameInfo.Width   = 0x1000;
        out->mfx.FrameInfo.Height  = 0x1000;

        ENCODE_CAPS_JPEG hwCaps = {0};
        mfxStatus sts = QueryHwCaps(core->GetVAType(), core->GetAdapterNumber(), hwCaps);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;
    }
    else
    {
        ENCODE_CAPS_JPEG hwCaps = {0};
        mfxStatus sts = QueryHwCaps(core->GetVAType(), core->GetAdapterNumber(), hwCaps);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        sts = CheckJpegParam(*in, hwCaps, core->IsExternalFrameAllocator());
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

// function to calculate required number of external surfaces and create request
mfxStatus MFXVideoENCODEMJPEG_HW::QueryIOSurf(VideoCORE * core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = MFX_ERR_NONE;

    ENCODE_CAPS_JPEG hwCaps = { 0 };
    sts = QueryHwCaps(core->GetVAType(), core->GetAdapterNumber(), hwCaps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxU32 inPattern = par->IOPattern;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request->NumFrameMin = 1;
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->NumFrameMin = 1;

        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
    }
    request->NumFrameSuggested = request->NumFrameMin;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG_HW::Init(mfxVideoParam *par)
{
    if (m_bInitialized || !m_pCore)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_ddi.reset( CreatePlatformMJpegEncoder( m_pCore ) );

    MFX_INTERNAL_CPY(&m_video, par, sizeof(mfxVideoParam));
    mfxStatus sts = m_ddi->CreateAuxilliaryDevice(
        m_pCore,
        DXVA2_Intel_Encode_JPEG,
        m_video.mfx.FrameInfo.Width,
        m_video.mfx.FrameInfo.Height);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;
    
    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    ENCODE_CAPS_JPEG caps = {0};
    sts = m_ddi->QueryEncodeCaps(caps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = CheckJpegParam(m_video, caps, m_pCore->IsExternalFrameAllocator());
    MFX_CHECK_STS(sts);

    sts = m_ddi->Reset(m_video);
    MFX_CHECK_STS(sts);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest request = { 0 };
    request.Info = m_video.mfx.FrameInfo;

    // for JPEG encoding, one raw buffer is enough. but regarding to
    // motion JPEG video, we'd better use multiple buffers to support async mode.
    mfxU16 surface_num = JPEG_VIDEO_SURFACE_NUM + m_video.AsyncDepth;

    // Allocate raw surfaces.
    // This is required only in case of system memory at input
    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        
        request.Type = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = surface_num;
        request.NumFrameSuggested = request.NumFrameMin;

        sts = m_pCore->AllocFrames(&request, &m_raw, true);
        MFX_CHECK(
            sts == MFX_ERR_NONE &&
            m_raw.NumFrameActual >= request.NumFrameMin,
            MFX_ERR_MEMORY_ALLOC);
    }

    // Allocate bitstream surfaces.
    request.Type = MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = surface_num;
    request.NumFrameSuggested = request.NumFrameMin;

    // driver may suggest too small buffer for bitstream
    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request);
    MFX_CHECK_STS(sts);
    request.Info.Width  = max(request.Info.Width,  m_video.mfx.FrameInfo.Width);
    request.Info.Height = max(request.Info.Height, m_video.mfx.FrameInfo.Height * 3 / 2);

    sts = m_pCore->AllocFrames(&request, &m_bitstream);
    MFX_CHECK(
        sts == MFX_ERR_NONE &&
        m_bitstream.NumFrameActual >= request.NumFrameMin,
        MFX_ERR_MEMORY_ALLOC);

    sts = m_ddi->Register(m_bitstream, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    sts = m_TaskManager.Init(surface_num);
    MFX_CHECK_STS(sts);

    m_bInitialized = TRUE;
    return sts;
}

mfxStatus MFXVideoENCODEMJPEG_HW::Reset(mfxVideoParam *par)
{
    mfxStatus sts = m_ddi->Reset(*par);
    MFX_CHECK_STS(sts);

    MFX_INTERNAL_CPY(&m_video, par, sizeof(mfxVideoParam));
    
    m_counter = 1;

    return sts;
}


mfxStatus MFXVideoENCODEMJPEG_HW::Close(void)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG_HW::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_video.mfx;
    par->Protected = m_video.Protected;
    par->IOPattern = m_video.IOPattern;
    par->AsyncDepth = m_video.AsyncDepth;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG_HW::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVideoENCODEMJPEG_HW::GetEncodeStat(mfxEncodeStat *stat)
{
    stat;
    return MFX_ERR_UNSUPPORTED;
}

// main function to run encoding process, syncronous 
mfxStatus MFXVideoENCODEMJPEG_HW::EncodeFrameCheck(
                               mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT pEntryPoints[],
                               mfxU32 &numEntryPoints)
{
    reordered_surface;pInternalParams;

    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        m_pCore->IsExternalFrameAllocator());
    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    DdiTask * task = 0;
    checkSts = m_TaskManager.AssignTask(task);
    MFX_CHECK_STS(checkSts);
    task->surface = surface;
    task->bs      = bs;
    task->m_statusReportNumber = m_counter++;

    // definition tasks for MSDK scheduler
    pEntryPoints[0].pState               = this;
    pEntryPoints[0].pParam               = task;
    // callback to run after complete task / depricated
    pEntryPoints[0].pCompleteProc        = 0;
    // callback to run after complete sub-task (for SW implementation makes sense) / (NON-OBLIGATORY)
    pEntryPoints[0].pGetSubTaskProc      = 0; 
    pEntryPoints[0].pCompleteSubTaskProc = 0;

    pEntryPoints[0].requiredNumThreads   = 1;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[0].pRoutineName = "Encode Submit";
    pEntryPoints[1].pRoutineName = "Encode Query";

    pEntryPoints[0].pRoutine = TaskRoutineSubmitFrame;
    pEntryPoints[1].pRoutine = TaskRoutineQueryFrame;

    numEntryPoints = 2;

    return MFX_ERR_NONE;
}


// Routine to submit task to HW.  asyncronous part of encdoing
mfxStatus MFXVideoENCODEMJPEG_HW::TaskRoutineSubmitFrame(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    MFXVideoENCODEMJPEG_HW & enc = *(MFXVideoENCODEMJPEG_HW*)state;
    DdiTask &task = *(DdiTask*)param;

    mfxStatus sts = enc.CheckDevice();
    MFX_CHECK_STS(sts);
    
    // D3D11 
    mfxHDLPair surfacePair = {0};
    // D3D9
    mfxHDL     surfaceHDL = 0;

    mfxHDL *pSurfaceHdl;

    if (MFX_HW_D3D11 == enc.m_pCore->GetVAType())
        pSurfaceHdl = (mfxHDL *)&surfacePair;
    else if (MFX_HW_D3D9 == enc.m_pCore->GetVAType())
        pSurfaceHdl = (mfxHDL *)&surfaceHDL;
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxFrameSurface1 * nativeSurf = task.surface;
    if (enc.m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        sts = FastCopyFrameBufferSys2Vid(
            enc.m_pCore,
            enc.m_raw.mids[task.m_idx],
            nativeSurf->Data,
            enc.m_video.mfx.FrameInfo);
        MFX_CHECK_STS(sts);

        sts = enc.m_pCore->GetFrameHDL(enc.m_raw.mids[task.m_idx], pSurfaceHdl);
    }
    else
    {
        if (MFX_IOPATTERN_IN_VIDEO_MEMORY == enc.m_video.IOPattern)
            sts = enc.m_pCore->GetExternalFrameHDL(nativeSurf->Data.MemId, pSurfaceHdl);
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;   
    }
    MFX_CHECK_STS(sts);

    if (MFX_HW_D3D11 == enc.m_pCore->GetVAType())
    {
        MFX_CHECK(surfacePair.first != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        sts = enc.m_ddi->Execute(task, (mfxHDL)pSurfaceHdl);
    }
    else // D3D9 remains only
    {
        MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        sts = enc.m_ddi->Execute(task, surfaceHDL);
    }

    return sts;
}

// Routine to query encdoing status from HW.  asyncronous part of encdoing
mfxStatus MFXVideoENCODEMJPEG_HW::TaskRoutineQueryFrame(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    mfxStatus sts;
    MFXVideoENCODEMJPEG_HW & enc = *(MFXVideoENCODEMJPEG_HW*)state;
    DdiTask &task = *(DdiTask*)param;

    sts = enc.m_ddi->QueryStatus(task);
    MFX_CHECK_STS(sts);

    sts = enc.m_ddi->UpdateBitstream(enc.m_bitstream.mids[task.m_idx], task);
    MFX_CHECK_STS(sts);

    return enc.m_TaskManager.RemoveTask(task);
}

mfxStatus MFXVideoENCODEMJPEG_HW::UpdateDeviceStatus(mfxStatus sts)
{
    if (sts == MFX_ERR_DEVICE_FAILED)
        m_deviceFailed = true;
    return sts;
}

mfxStatus MFXVideoENCODEMJPEG_HW::CheckDevice()
{
    return m_deviceFailed
        ? MFX_ERR_DEVICE_FAILED
        : MFX_ERR_NONE;
}

#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE
