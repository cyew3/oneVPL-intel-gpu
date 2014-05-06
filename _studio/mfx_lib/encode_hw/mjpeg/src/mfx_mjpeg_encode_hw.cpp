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
#include "mfx_enc_common.h"
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

    memset(&m_vFirstParam, 0, sizeof(mfxVideoParam));
    memset(&m_vParam, 0, sizeof(mfxVideoParam));

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

    MFX_INTERNAL_CPY(&m_vFirstParam, par, sizeof(mfxVideoParam));
    MFX_INTERNAL_CPY(&m_vParam, &m_vFirstParam, sizeof(mfxVideoParam));

    mfxStatus sts = m_ddi->CreateAuxilliaryDevice(
        m_pCore,
        DXVA2_Intel_Encode_JPEG,
        m_vParam.mfx.FrameInfo.Width,
        m_vParam.mfx.FrameInfo.Height);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;
    
    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    ENCODE_CAPS_JPEG caps = {0};
    sts = m_ddi->QueryEncodeCaps(caps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = CheckJpegParam(m_vParam, caps, m_pCore->IsExternalFrameAllocator());
    MFX_CHECK_STS(sts);

    sts = m_ddi->CreateAccelerationService(m_vParam);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest request = { 0 };
    request.Info = m_vParam.mfx.FrameInfo;

    // for JPEG encoding, one raw buffer is enough. but regarding to
    // motion JPEG video, we'd better use multiple buffers to support async mode.
    mfxU16 surface_num = JPEG_VIDEO_SURFACE_NUM + m_vParam.AsyncDepth;

    // Allocate raw surfaces.
    // This is required only in case of system memory at input
    if (m_vParam.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
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
    request.Info.Width  = max(request.Info.Width,  m_vParam.mfx.FrameInfo.Width);
    request.Info.Height = max(request.Info.Height, m_vParam.mfx.FrameInfo.Height * 3 / 2);

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
    mfxStatus sts;

    if(!m_bInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    if(par == NULL)
        return MFX_ERR_NULL_PTR;

    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    mfxExtJPEGQuantTables*    jpegQT       = (mfxExtJPEGQuantTables*)   GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT       = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    mfxVideoParam checked;
    mfxExtJPEGQuantTables checked_jpegQT;
    mfxExtJPEGHuffmanTables checked_jpegHT;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtBuffer *ptr_checked_ext[3] = {0,};
    mfxU16 ext_counter = 0;
    checked = *par;

    if (jpegQT)
    {
        checked_jpegQT = *jpegQT;
        ptr_checked_ext[ext_counter++] = &checked_jpegQT.Header;
    } 
    else 
    {
        memset(&checked_jpegQT, 0, sizeof(checked_jpegQT));
        checked_jpegQT.Header.BufferId = MFX_EXTBUFF_JPEG_QT;
        checked_jpegQT.Header.BufferSz = sizeof(checked_jpegQT);
    }
    if (jpegHT)
    {
        checked_jpegHT = *jpegHT;
        ptr_checked_ext[ext_counter++] = &checked_jpegHT.Header;
    } 
    else 
    {
        memset(&checked_jpegHT, 0, sizeof(checked_jpegHT));
        checked_jpegHT.Header.BufferId = MFX_EXTBUFF_JPEG_HUFFMAN;
        checked_jpegHT.Header.BufferSz = sizeof(checked_jpegHT);
    }
    if (opaqAllocReq) 
    {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    checked.ExtParam = ptr_checked_ext;
    checked.NumExtParam = ext_counter;

    sts = Query(m_pCore, par, &checked);

    if (sts != MFX_ERR_NONE && sts != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && sts != MFX_WRN_PARTIAL_ACCELERATION)
    {
        if (sts == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return sts;
    }

    par = &checked; // from now work with fixed copy of input!

    if (par->mfx.FrameInfo.FrameRateExtN == 0 ||
        par->mfx.FrameInfo.FrameRateExtD == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if(par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // check that new params don't require allocation of additional memory
    if (par->mfx.FrameInfo.Width > m_vFirstParam.mfx.FrameInfo.Width ||
        par->mfx.FrameInfo.Height > m_vFirstParam.mfx.FrameInfo.Height ||
        m_vFirstParam.mfx.FrameInfo.FourCC != par->mfx.FrameInfo.FourCC ||
        m_vFirstParam.mfx.FrameInfo.ChromaFormat != par->mfx.FrameInfo.ChromaFormat)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    sts = m_TaskManager.Reset();
    MFX_CHECK_STS(sts);

    m_vParam.mfx = par->mfx;

    m_vParam.IOPattern = par->IOPattern;
    m_vParam.Protected = 0;

    if(par->AsyncDepth != m_vFirstParam.AsyncDepth)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!par->mfx.FrameInfo.FrameRateExtD || !par->mfx.FrameInfo.FrameRateExtN) 
        return MFX_ERR_INVALID_VIDEO_PARAM;
    
    m_counter = 1;

    return sts;
}


mfxStatus MFXVideoENCODEMJPEG_HW::Close(void)
{
    mfxStatus sts = m_TaskManager.Close();
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG_HW::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vParam.mfx;
    par->Protected = m_vParam.Protected;
    par->IOPattern = m_vParam.IOPattern;
    par->AsyncDepth = m_vParam.AsyncDepth;

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
        m_vParam,
        ctrl,
        surface,
        bs,
        m_pCore->IsExternalFrameAllocator());
    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    DdiTask * task = 0;
    checkSts = m_TaskManager.AssignTask(task);
    MFX_CHECK_STS(checkSts);

    if(!task->m_pDdiData)
    {
        task->m_pDdiData = new MfxHwMJpegEncode::ExecuteBuffers;
        checkSts = task->m_pDdiData->Init(&m_vParam);
        MFX_CHECK_STS(checkSts);
    }

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


mfxStatus MFXVideoENCODEMJPEG_HW::CheckEncodeFrameParam(
    mfxVideoParam const & video,
    mfxEncodeCtrl       * ctrl,
    mfxFrameSurface1    * surface,
    mfxBitstream        * bs,
    bool                  isExternalFrameAllocator)
{
    mfxStatus sts = MFX_ERR_NONE;
    UMC::Status umc_sts = UMC::UMC_OK;
    MFX_CHECK_NULL_PTR1(bs);

    mfxExtJPEGQuantTables*   jpegQT = NULL;
    mfxExtJPEGHuffmanTables* jpegHT = NULL;

    if (surface != 0)
    {
        MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16) < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Y != 0 || isExternalFrameAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (surface->Info.Width != video.mfx.FrameInfo.Width || surface->Info.Height != video.mfx.FrameInfo.Height)
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    else
    {
        sts = MFX_ERR_MORE_DATA;
    }

    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam > 0)
    {
        jpegQT = (mfxExtJPEGQuantTables*)   GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_QT );
        jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    }

    if(jpegQT)
    {
        // TBD: set external quant table
        // umc_sts = pMJPEGVideoEncoder->SetQuantTableExtBuf(jpegQT);
    }
    else if(!m_vParam.mfx.Quality)
    {
        umc_sts = UMC::UMC_ERR_FAILED;
    }

    if(umc_sts != UMC::UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(jpegHT)
    {
        // TBD: set external Huffman table
        // umc_sts = pMJPEGVideoEncoder->SetHuffmanTableExtBuf(jpegHT);
    }

    if(umc_sts != UMC::UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return sts;
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
    if (enc.m_vParam.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        sts = FastCopyFrameBufferSys2Vid(
            enc.m_pCore,
            enc.m_raw.mids[task.m_idx],
            nativeSurf->Data,
            enc.m_vParam.mfx.FrameInfo);
        MFX_CHECK_STS(sts);

        sts = enc.m_pCore->GetFrameHDL(enc.m_raw.mids[task.m_idx], pSurfaceHdl);
    }
    else
    {
        if (MFX_IOPATTERN_IN_VIDEO_MEMORY == enc.m_vParam.IOPattern)
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
