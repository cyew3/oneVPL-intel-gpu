/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/
#include "mfx_h265_encode_hw.h"
#include <assert.h>
#include <vm_time.h>

namespace MfxHwH265Encode
{


mfxStatus CheckVideoParam(MfxVideoParam & par, ENCODE_CAPS_HEVC const & caps, bool bInit = false);
void      SetDefaults    (MfxVideoParam & par, ENCODE_CAPS_HEVC const & hwCaps);
void      InheritDefaultValues(MfxVideoParam const & parInit, MfxVideoParam &  parReset);

Plugin::Plugin(bool CreateByDispatcher)
    : m_createdByDispatcher(CreateByDispatcher)
    , m_adapter(this)
    , m_bInit(false)
{
}

Plugin::~Plugin()
{
    Close();
}

mfxStatus Plugin::PluginInit(mfxCoreInterface *core)
{
    MFX_CHECK_NULL_PTR1(core);

    m_core = *core;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::PluginClose()
{
   if (m_createdByDispatcher)
       Release();

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetPluginParam(mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->PluginUID          = MFX_PLUGINID_HEVCE_HW;
    par->PluginVersion      = 1;
    par->ThreadPolicy       = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum       = 1;
    par->APIVersion.Major   = MFX_VERSION_MAJOR;
    par->APIVersion.Minor   = MFX_VERSION_MINOR;
    par->Type               = MFX_PLUGINTYPE_VIDEO_ENCODE;
    par->CodecId            = MFX_CODEC_HEVC;

    return MFX_ERR_NONE;
}

mfxU16 MaxRec(MfxVideoParam const & par)
{
    return par.AsyncDepth + par.mfx.NumRefFrame + ((par.AsyncDepth > 1)? 1: 0);
}

mfxU16 MaxRaw(MfxVideoParam const & par)
{
    return par.AsyncDepth + par.mfx.GopRefDist -1  + par.RawRef * par.mfx.NumRefFrame + ((par.AsyncDepth > 1)? 1: 0);
}

mfxU16 MaxBs(MfxVideoParam const & par)
{
    return par.AsyncDepth + ((par.AsyncDepth > 1)? 1: 0); // added buffered task between submit into ddi and query
}

mfxU16 MaxTask(MfxVideoParam const & par)
{
    return par.AsyncDepth + par.mfx.GopRefDist - 1 + ((par.AsyncDepth > 1)? 1: 0); 
}

mfxStatus LoadSPSPPS(MfxVideoParam& par, mfxExtCodingOptionSPSPPS* pSPSPPS)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (!pSPSPPS)
        return MFX_ERR_NONE;

    if (pSPSPPS->SPSBuffer)
    {
        BitstreamReader bs(pSPSPPS->SPSBuffer, pSPSPPS->SPSBufSize);
        sts = HeaderReader::ReadSPS(bs, par.m_sps);
        MFX_CHECK_STS(sts);

        if (pSPSPPS->PPSBuffer)
        {
            bs.Reset(pSPSPPS->PPSBuffer, pSPSPPS->PPSBufSize);
            sts = HeaderReader::ReadPPS(bs, par.m_pps);
            MFX_CHECK_STS(sts);
        }

        Zero(par.m_vps);
        ((LayersInfo&)par.m_vps) = ((LayersInfo&)par.m_sps);
        par.m_vps.video_parameter_set_id = par.m_sps.video_parameter_set_id;

        par.SyncHeadersToMfxParam();
    }
    return sts;
}
mfxStatus Plugin::InitImpl(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE, qsts = MFX_ERR_NONE;

    m_ddi.reset( CreatePlatformH265Encoder(&m_core) );
    MFX_CHECK(m_ddi.get(), MFX_ERR_UNSUPPORTED);

    sts = ExtBuffer::CheckBuffers(*par);
    MFX_CHECK_STS(sts);

    m_vpar = *par;

    sts = m_ddi->CreateAuxilliaryDevice(
        &m_core,
        GetGUID(*par),
        m_vpar.m_ext.HEVCParam.PicWidthInLumaSamples,
        m_vpar.m_ext.HEVCParam.PicHeightInLumaSamples);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_DEVICE_FAILED);

    sts = m_ddi->QueryEncodeCaps(m_caps);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_DEVICE_FAILED);

    mfxExtCodingOptionSPSPPS* pSPSPPS = ExtBuffer::Get(*par);

    sts = LoadSPSPPS(m_vpar, pSPSPPS);
    MFX_CHECK_STS(sts);

    qsts = CheckVideoParam(m_vpar, m_caps, true);
    MFX_CHECK(qsts >= MFX_ERR_NONE, qsts);

    SetDefaults(m_vpar, m_caps);

    m_vpar.SyncCalculableToVideoParam();

    if (!pSPSPPS || !pSPSPPS->SPSBuffer)
        m_vpar.SyncMfxToHeadersParam();

    sts = CheckHeaders(m_vpar, m_caps);
    MFX_CHECK_STS(sts);

    m_hrd.Setup(m_vpar.m_sps, m_vpar.InitialDelayInKB);

    sts = m_ddi->CreateAccelerationService(m_vpar);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_DEVICE_FAILED);

    mfxFrameAllocRequest request = {};
    request.Info = m_vpar.mfx.FrameInfo;

    if (m_vpar.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = MaxRaw(m_vpar);

        sts = m_raw.Alloc(&m_core, request, true);
        MFX_CHECK_STS(sts);
    }
    else if (m_vpar.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc& opaq = m_vpar.m_ext.Opaque;

        sts = m_core.MapOpaqueSurface(opaq.In.NumSurface, opaq.In.Type, opaq.In.Surfaces);
        MFX_CHECK_STS(sts);

        if (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            request.Type        = MFX_MEMTYPE_D3D_INT;
            request.NumFrameMin = opaq.In.NumSurface;
            sts = m_raw.Alloc(&m_core, request, true);
        }
    }

    request.Type        = MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = MaxRec(m_vpar);
    
    if(request.Info.FourCC == MFX_FOURCC_RGB4)
    {
        //in case of ARGB input we need NV12 reconstruct allocation
        request.Info.FourCC = MFX_FOURCC_NV12;
        request.Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }
    sts = m_rec.Alloc(&m_core, request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_rec.NumFrameActual ? m_rec : m_raw, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request);
    MFX_CHECK_STS(sts);

    request.Type        = MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = MaxBs(m_vpar);

    sts = m_bs.Alloc(&m_core, request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_bs, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    m_task.Reset(MaxTask(m_vpar));

    m_frameOrder = 0;
    m_baseLayerOrder = 0;

    Fill(m_lastTask, IDX_INVALID);

    m_numBuffered = 0;
    m_NumberOfSlicesForOpt = m_vpar.mfx.NumSlice;

#if DEBUG_REC_FRAMES_INFO
    mfxExtDumpFiles * extDump = &m_vpar.m_ext.DumpFiles;
    if (vm_file * file = OpenFile(extDump->ReconFilename, _T("wb")))
    {
        vm_file_fclose(file);
    }
#endif

    m_bInit = true;
    return qsts;
}

mfxStatus Plugin::Init(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    MFX_CHECK(par->mfx.CodecId == MFX_CODEC_HEVC, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = InitImpl(par);

    if (!m_bInit)
        FreeResources();

    return sts;
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, mfxFrameAllocRequest * /*out*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR2(par, request);

    MFX_CHECK(par->mfx.CodecId == MFX_CODEC_HEVC, MFX_ERR_UNSUPPORTED);


    MfxVideoParam tmp = *par;
    ENCODE_CAPS_HEVC caps = {};

    sts = ExtBuffer::CheckBuffers(*par);
    MFX_CHECK_STS(sts);

    switch (par->IOPattern & MFX_IOPATTERN_IN_MASK)
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        request->Type = MFX_MEMTYPE_SYS_EXT;
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        request->Type = MFX_MEMTYPE_D3D_EXT;
        break;
    case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME;
        break;
    default: return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    sts = QueryHwCaps(&m_core, GetGUID(tmp), caps);
    MFX_CHECK_STS(sts);

    CheckVideoParam(tmp, caps);
    SetDefaults(tmp, caps);

    request->Info = tmp.mfx.FrameInfo;

    request->NumFrameMin = MaxRaw(tmp);

    request->NumFrameSuggested = request->NumFrameMin;

    return sts;
}

mfxStatus Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);

    if (!in)
    {
        Zero(out->mfx);

        out->IOPattern             = 1;
        //out->Protected             = 1;
        out->AsyncDepth            = 1;
        //out->mfx.CodecId           = 1;
        //out->mfx.LowPower          = 1;
        out->mfx.CodecLevel        = 1;
        out->mfx.CodecProfile      = 1;
        out->mfx.TargetUsage       = 1;
        out->mfx.GopPicSize        = 1;
        out->mfx.GopRefDist        = 1;
        out->mfx.GopOptFlag        = 1;
        out->mfx.IdrInterval       = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.InitialDelayInKB  = 1;
        out->mfx.BufferSizeInKB    = 1;
        out->mfx.TargetKbps        = 1;
        out->mfx.MaxKbps           = 1;
        out->mfx.NumSlice          = 1;
        out->mfx.NumRefFrame       = 1;
        out->mfx.EncodedOrder      = 1;

        out->mfx.FrameInfo.FourCC        = 1;
        out->mfx.FrameInfo.Width         = 1;
        out->mfx.FrameInfo.Height        = 1;
        out->mfx.FrameInfo.CropX         = 1;
        out->mfx.FrameInfo.CropY         = 1;
        out->mfx.FrameInfo.CropW         = 1;
        out->mfx.FrameInfo.CropH         = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW  = 1;
        out->mfx.FrameInfo.AspectRatioH  = 1;
        out->mfx.FrameInfo.ChromaFormat  = 1;
        //out->mfx.FrameInfo.PicStruct     = 1;
    }
    else
    {
        MfxVideoParam tmp = *in;
        ENCODE_CAPS_HEVC caps = {};
        mfxExtEncoderCapability * enc_cap = ExtBuffer::Get(*in);

        if (enc_cap!=0)
        {
            return MFX_ERR_UNSUPPORTED;        
        }

        MFX_CHECK(in->mfx.CodecId == MFX_CODEC_HEVC, MFX_ERR_UNSUPPORTED);

        // matching ExtBuffers
        sts = ExtBuffer::CheckBuffers(*in, *out);
        MFX_CHECK_STS(sts);

        if (m_ddi.get())
        {
            sts = m_ddi->QueryEncodeCaps(caps);
            MFX_CHECK_STS(sts);
        }
        else
        {
            sts = QueryHwCaps(&m_core, GetGUID(tmp), caps);
            MFX_CHECK_STS(sts);
        }
        mfxExtCodingOptionSPSPPS* pSPSPPS = ExtBuffer::Get(*in);
        if (pSPSPPS && pSPSPPS->SPSBuffer)
        {
            sts = LoadSPSPPS(tmp, pSPSPPS);
            MFX_CHECK_STS(sts);

             sts = CheckHeaders(tmp, caps);
             MFX_CHECK_STS(sts);
        }
        sts = CheckVideoParam(tmp, caps);

        tmp.FillPar(*out, true);
    }

    return sts;
}
mfxStatus   Plugin::WaitingForAsyncTasks(bool bResetTasks)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    // sheduler must wait untial all async tasks will be ready.
    MFX_CHECK(m_task.GetSubmittedTask() == NULL, MFX_ERR_UNDEFINED_BEHAVIOR);
    
    if (!bResetTasks)
       return sts;

    for (;;)
    {
        //waiting for async tasks are finished
        Task* task = m_task.GetTaskForQuery();
        if (!task)
            break;
       
        m_task.SubmitForQuery(task);
        task->m_stage = STAGE_READY;
        if (task->m_surf)
            FreeTask(*task);
        m_task.Ready(task);
 
    }

    // drop other tasks (not submitted into async part)
    for (;;)
    {
        Task* pTask = m_task.Reorder(m_vpar, m_lastTask.m_dpb[0], true);
        if (!pTask)
            break;
        pTask->m_stage = FRAME_REORDERED;
        m_task.Submit(pTask);
    }
    // free reordered tasks
    for (;;)
    {
        Task* task = m_task.GetSubmittedTask();
        if (!task)
            break;

        m_task.SubmitForQuery(task);
        task->m_stage = STAGE_READY;
        if (task->m_surf)
            FreeTask(*task);
        m_task.Ready(task);
    }


    m_task.Reset();

    m_frameOrder = 0;
    m_raw.Unlock();
    m_rec.Unlock();
    m_bs.Unlock();

    Fill(m_lastTask, 0xFF);
    m_numBuffered = 0;
    return sts;


}

mfxStatus  Plugin::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    sts = ExtBuffer::CheckBuffers(*par);
    MFX_CHECK_STS(sts);

    MfxVideoParam parNew = *par;

    mfxExtEncoderResetOption * pResetOpt = ExtBuffer::Get(*par);
    mfxExtCodingOptionSPSPPS* pSPSPPS = ExtBuffer::Get(*par);

    sts = LoadSPSPPS(parNew, pSPSPPS);
    MFX_CHECK_STS(sts);

    InheritDefaultValues(m_vpar, parNew);
    parNew.SyncVideoToCalculableParam();

    sts = CheckVideoParam(parNew, m_caps);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    SetDefaults(parNew, m_caps);

    parNew.SyncCalculableToVideoParam();

    if (!pSPSPPS || !pSPSPPS->SPSBuffer)
        parNew.SyncMfxToHeadersParam(m_NumberOfSlicesForOpt);

    sts = CheckHeaders(parNew, m_caps);
    MFX_CHECK_STS(sts);


    MFX_CHECK(
        parNew.mfx.CodecProfile           != MFX_CODEC_HEVC
        && m_vpar.AsyncDepth                 == parNew.AsyncDepth
        && m_vpar.mfx.GopRefDist             >= parNew.mfx.GopRefDist
        //&& m_vpar.mfx.NumSlice               >= parNew.mfx.NumSlice
        && m_vpar.mfx.NumRefFrame            >= parNew.mfx.NumRefFrame
        && m_vpar.mfx.RateControlMethod      == parNew.mfx.RateControlMethod
        && m_rec.m_info.Width                >= parNew.mfx.FrameInfo.Width
        && m_rec.m_info.Height               >= parNew.mfx.FrameInfo.Height
        && m_vpar.mfx.FrameInfo.ChromaFormat == parNew.mfx.FrameInfo.ChromaFormat
        && m_vpar.IOPattern                  == parNew.IOPattern
        ,  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    if (m_vpar.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        m_vpar.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        MFX_CHECK(
            m_vpar.InitialDelayInKB == parNew.InitialDelayInKB &&
            m_vpar.BufferSizeInKB   == parNew.BufferSizeInKB,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    mfxU32 tempLayerIdx = 0;
    bool changeLyncLayers = false;
    bool isIdrRequired = false;

    // check if change of temporal scalability required by new parameters
    if (m_vpar.isTL() && parNew.isTL())
    {
        // calculate temporal layer for next frame
        tempLayerIdx     = m_vpar.GetTId(m_frameOrder);
        changeLyncLayers = m_vpar.NumTL() != parNew.NumTL();
    }

    // check if IDR required after change of encoding parameters
    bool isSpsChanged = m_vpar.m_sps.vui_parameters_present_flag == 0 ?
        memcmp(&m_vpar.m_sps, &parNew.m_sps, sizeof(SPS) - sizeof(VUI)) != 0 :
    !Equal(m_vpar.m_sps, parNew.m_sps);

    isIdrRequired = isSpsChanged
        || (tempLayerIdx != 0 && changeLyncLayers)
        || m_vpar.mfx.GopPicSize != parNew.mfx.GopPicSize
        || m_vpar.m_ext.CO2.IntRefType != parNew.m_ext.CO2.IntRefType;
    
    /*
    printf("isIdrRequired %d, isSpsChanged %d \n", isIdrRequired, isSpsChanged);
    if (isSpsChanged)
    {
        printf("addres of pic_width_in_luma_samples %d\n", (mfxU8*)&m_vpar.m_sps.pic_width_in_luma_samples -(mfxU8*)&m_vpar.m_sps);
        printf("addres of log2_diff_max_min_pcm_luma_coding_block_size %d\n", (mfxU8*)&m_vpar.m_sps.log2_diff_max_min_pcm_luma_coding_block_size -(mfxU8*)&m_vpar.m_sps);
        printf("addres of lt_ref_pic_poc_lsb_sps[0] %d\n", (mfxU8*)&m_vpar.m_sps.lt_ref_pic_poc_lsb_sps[0] -(mfxU8*)&m_vpar.m_sps);
        printf("addres of vui[0] %d\n", (mfxU8*)&m_vpar.m_sps.vui -(mfxU8*)&m_vpar.m_sps);

        for (int t = 0; t < sizeof(m_vpar.m_sps); t++)
        {
            if (((mfxU8*)&m_vpar.m_sps)[t]  != ((mfxU8*)&parNew.m_sps)[t])
                printf("Difference in %d, %d, %d\n",t, ((mfxU8*)&m_vpar.m_sps)[t], ((mfxU8*)&parNew.m_sps)[t]);        
        }    
    }
    */

    if (isIdrRequired && pResetOpt && IsOff(pResetOpt->StartNewSequence))
        return MFX_ERR_INVALID_VIDEO_PARAM; // Reset can't change parameters w/o IDR. Report an error

    bool brcReset =
        m_vpar.TargetKbps != parNew.TargetKbps ||
        m_vpar.MaxKbps    != parNew.MaxKbps;

    if (brcReset &&
        m_vpar.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // waiting for submitted in driver tasks
    MFX_CHECK_STS(WaitingForAsyncTasks(isIdrRequired || (pResetOpt && IsOn(pResetOpt->StartNewSequence))));

    if (isIdrRequired || (pResetOpt && IsOn(pResetOpt->StartNewSequence)))
    {
#if DEBUG_REC_FRAMES_INFO
        mfxExtDumpFiles * extDump = &m_vpar.m_ext.DumpFiles;
        if (vm_file * file = OpenFile(extDump->ReconFilename, _T("wb")))
        {
            vm_file_fclose(file);
        }
#endif
    }

    m_vpar = parNew;

    m_hrd.Reset(m_vpar.m_sps);
    m_ddi->Reset(m_vpar);

    return sts;
}

mfxStatus Plugin::GetVideoParam(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    sts = m_vpar.FillPar(*par);
    return sts;
}

struct LessInc
{
    mfxU32* m_cnt;
    mfxU32  m_ref;

    LessInc(mfxU32 ref, mfxU32* cnt)
    {
        m_ref = ref;
        m_cnt = cnt;
    }

   inline bool operator() (mfxU32 l)
    {
        if (l < m_ref)
        {
            if (m_cnt)
                (*m_cnt)++;
            return true;
        }
        return false;
    }
};

mfxStatus Plugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *thread_task)
{
    mfxStatus sts = MFX_ERR_NONE;
    Task* task = 0;

    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(bs);

    MFX_CHECK((mfxU8)FindFreeResourceIndex(m_bs)  != IDX_INVALID, MFX_WRN_DEVICE_BUSY);
    MFX_CHECK((mfxU8)FindFreeResourceIndex(m_rec) != IDX_INVALID, MFX_WRN_DEVICE_BUSY);

    if (surface)
    {
        MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
        //MFX_CHECK(surface->Info.Width  >=  m_vpar.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        //MFX_CHECK(surface->Info.Height >=  m_vpar.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);
    }
    if (m_vpar.Protected == 0)
    {
        mfxU32 bufferSizeInKB =  m_vpar.BufferSizeInKB;

        MFX_CHECK(bs->DataOffset <= bs->MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(bs->DataOffset + bs->DataLength + bufferSizeInKB * 1000u <= bs->MaxLength,
            MFX_ERR_NOT_ENOUGH_BUFFER);
        MFX_CHECK_NULL_PTR1(bs->Data);
    }

    if (surface)
    {
        task = m_task.New();
        MFX_CHECK(task, MFX_WRN_DEVICE_BUSY);
        Zero(*task);

        task->m_surf = surface;
        if (ctrl)
            task->m_ctrl = *ctrl;

        if (m_vpar.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            task->m_surf_real = 0;

            sts = m_core.GetRealSurface(task->m_surf, &task->m_surf_real);
            MFX_CHECK_STS(sts);

            if (task->m_surf_real == 0)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            task->m_surf_real->Info            = task->m_surf->Info;
            task->m_surf_real->Data.TimeStamp  = task->m_surf->Data.TimeStamp;
            task->m_surf_real->Data.FrameOrder = task->m_surf->Data.FrameOrder;
            task->m_surf_real->Data.Corrupted  = task->m_surf->Data.Corrupted;
            task->m_surf_real->Data.DataFlag   = task->m_surf->Data.DataFlag;
        }
        else
            task->m_surf_real = task->m_surf;

        if (m_vpar.mfx.EncodedOrder)
        {
            task->m_frameType = task->m_ctrl.FrameType;
            MFX_CHECK(surface->Data.FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(task->m_frameType != MFX_FRAMETYPE_UNKNOWN, MFX_ERR_UNDEFINED_BEHAVIOR);
            m_frameOrder = surface->Data.FrameOrder;

            if (surface->Data.FrameOrder > 0)
            {
                m_reorderEmu.remove_if(LessInc(surface->Data.FrameOrder, &m_displOrder));

                task->m_dpb_output_delay = surface->Data.FrameOrder - m_displOrder - 1;

                if (task->m_dpb_output_delay)
                    m_reorderEmu.push_back(surface->Data.FrameOrder);
                else
                    m_displOrder++;
            }
            else
            {
                task->m_dpb_output_delay = 0;
                m_displOrder = 0;
                m_reorderEmu.resize(0);
            }
        }
        else
        {
            task->m_frameType = GetFrameType(m_vpar, m_frameOrder);

            if (task->m_ctrl.FrameType & MFX_FRAMETYPE_IDR)
                task->m_frameType = MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR;
        }

        if (task->m_frameType & MFX_FRAMETYPE_IDR)
            m_lastIDR = m_frameOrder;

        task->m_poc = m_frameOrder - m_lastIDR;
        task->m_fo =  m_frameOrder;
        task->m_bpo = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

        m_core.IncreaseReference(&surface->Data);

        m_frameOrder ++;
        task->m_stage |= FRAME_ACCEPTED;
    }
    if (!m_vpar.mfx.EncodedOrder)
        task = m_task.Reorder(m_vpar, m_lastTask.m_dpb[1], !surface);

    if (!task)
    {
        if (m_numBuffered && !surface)
        {
            task = m_task.New();
            MFX_CHECK(task, MFX_WRN_DEVICE_BUSY);
            Zero(*task);
            task->m_bs = bs;
            *thread_task = task;
            task->m_stage |= FRAME_REORDERED;
            m_task.Submit(task);

            m_numBuffered --;
            return MFX_ERR_NONE;

        }
        else
            return MFX_ERR_MORE_DATA;
    }

    task->m_idxRaw = (mfxU8)FindFreeResourceIndex(m_raw);
    task->m_idxRec = (mfxU8)FindFreeResourceIndex(m_rec);
    task->m_idxBs  = (mfxU8)FindFreeResourceIndex(m_bs);
    MFX_CHECK(task->m_idxBs  != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(task->m_idxRec != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);


    task->m_midRaw = AcquireResource(m_raw, task->m_idxRaw);
    task->m_midRec = AcquireResource(m_rec, task->m_idxRec);
    task->m_midBs  = AcquireResource(m_bs,  task->m_idxBs);
    MFX_CHECK(task->m_midRec && task->m_midBs, MFX_ERR_UNDEFINED_BEHAVIOR);

    task->m_bs = bs;

    ConfigureTask(*task, m_lastTask, m_vpar, m_baseLayerOrder);
    m_lastTask = *task;

    *thread_task = task;

    task->m_stage |= FRAME_REORDERED;

    if ((surface!=0) && (m_numBuffered < (mfxU32)((m_vpar.AsyncDepth > 1) ? 1 : 0) ))
    {
        m_numBuffered ++;
        sts = MFX_ERR_MORE_DATA_SUBMIT_TASK;
        task->m_bs = 0;
    }

    m_task.Submit(task);
    return sts;
}

mfxStatus Plugin::Execute(mfxThreadTask thread_task, mfxU32 /*uid_p*/, mfxU32 /*uid_a*/)
{
    Task* taskForExecute = m_task.GetSubmittedTask();
    Task* inputTask = (Task*) thread_task;
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(inputTask);       


   if (inputTask == taskForExecute && taskForExecute->m_surf)
   {
        mfxHDLPair surfaceHDL = {};

        taskForExecute->m_initial_cpb_removal_delay  = m_hrd.GetInitCpbRemovalDelay();
        taskForExecute->m_initial_cpb_removal_offset = m_hrd.GetInitCpbRemovalDelayOffset();

#ifndef HEADER_PACKING_TEST
        sts = GetNativeHandleToRawSurface(m_core, m_vpar, *taskForExecute, surfaceHDL.first);
        MFX_CHECK_STS(sts);

        sts = CopyRawSurfaceToVideoMemory(m_core, m_vpar, *taskForExecute);
        MFX_CHECK_STS(sts);
#endif
        sts = m_ddi->Execute(*taskForExecute, surfaceHDL.first);
        MFX_CHECK_STS(sts);

        taskForExecute->m_stage |= FRAME_SUBMITTED;
        m_task.SubmitForQuery(taskForExecute);
    }

   if (inputTask->m_bs)
   {
        mfxBitstream* bs = inputTask->m_bs;
        Task* taskForQuery = m_task.GetTaskForQuery();
        MFX_CHECK (taskForQuery, MFX_ERR_UNDEFINED_BEHAVIOR);




        sts = m_ddi->QueryStatus(*taskForQuery);
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;

        MFX_CHECK_STS(sts);

        //update bitstream
        if (taskForQuery->m_bsDataLength)
        {
            mfxFrameAllocator & fa = m_core.FrameAllocator();
            mfxFrameData codedFrame = {};
            mfxU32 bytesAvailable = bs->MaxLength - bs->DataOffset - bs->DataLength;
            mfxU32 bytes2copy     = taskForQuery->m_bsDataLength;
            mfxI32 dpbOutputDelay = taskForQuery->m_fo +  GetNumReorderFrames(m_vpar.mfx.GopRefDist-1,m_vpar.isBPyramid()) - taskForQuery->m_eo;

            MFX_CHECK(bytesAvailable >= bytes2copy, MFX_ERR_NOT_ENOUGH_BUFFER);

            sts = fa.Lock(fa.pthis, taskForQuery->m_midBs, &codedFrame);
            MFX_CHECK(codedFrame.Y, MFX_ERR_LOCK_MEMORY);

            memcpy_s(bs->Data + bs->DataOffset + bs->DataLength, bytes2copy, codedFrame.Y, bytes2copy);

            sts = fa.Unlock(fa.pthis, taskForQuery->m_midBs, &codedFrame);
            MFX_CHECK_STS(sts);

            bs->DataLength += bytes2copy;

            m_hrd.RemoveAccessUnit(bytes2copy, !!(taskForQuery->m_frameType & MFX_FRAMETYPE_IDR));

            bs->TimeStamp       = taskForQuery->m_surf->Data.TimeStamp;
            
            bs->DecodeTimeStamp = CalcDTSFromPTS(this->m_vpar.mfx.FrameInfo, (mfxU16)dpbOutputDelay, bs->TimeStamp);;
            bs->PicStruct       = MFX_PICSTRUCT_PROGRESSIVE;
            bs->FrameType       = taskForQuery->m_frameType;
        }

#if DEBUG_REC_FRAMES_INFO

        mfxExtDumpFiles * extDump = &m_vpar.m_ext.DumpFiles;

        if (vm_file * file = OpenFile(extDump->ReconFilename, _T("ab")))
        {
            mfxFrameData data = { 0 };
            mfxFrameAllocator & fa = m_core.FrameAllocator();

            data.MemId = m_rec.mids[taskForQuery->m_idxRec];
            sts = fa.Lock(fa.pthis,  m_rec.mids[taskForQuery->m_idxRec], &data);
            MFX_CHECK(data.Y, MFX_ERR_LOCK_MEMORY);

            WriteFrameData(file, data, m_vpar.mfx.FrameInfo);
            fa.Unlock(fa.pthis,  m_rec.mids[taskForQuery->m_idxRec], &data);

            vm_file_fclose(file);
        }
        if (vm_file * file = OpenFile(extDump->InputFramesFilename, _T("ab")))
        {
            mfxFrameAllocator & fa = m_core.FrameAllocator();
            if (taskForQuery->m_surf)
            {
                mfxFrameData data = { 0 };
                if (taskForQuery->m_surf->Data.Y)
                {
                    data = taskForQuery->m_surf->Data;
                }
                else
                {
                     sts = fa.Lock(fa.pthis,  taskForQuery->m_surf->Data.Y, &data);
                     MFX_CHECK(data.Y, MFX_ERR_LOCK_MEMORY);
                }
                WriteFrameData(file, data, m_vpar.mfx.FrameInfo);
                vm_file_fclose(file);

                if (!taskForQuery->m_surf->Data.Y)
                {
                    fa.Unlock(fa.pthis,  taskForQuery->m_surf->Data.Y, &data);
                }
            }
        }
#endif // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing

        taskForQuery->m_stage |= FRAME_ENCODED;
        FreeTask(*taskForQuery);
        m_task.Ready(taskForQuery);

        if (!inputTask->m_surf)
        {
            //formal task
             m_task.SubmitForQuery(inputTask);
             m_task.Ready(inputTask);
        }
   }
   return sts;
}

mfxStatus Plugin::FreeResources(mfxThreadTask /*thread_task*/, mfxStatus /*sts*/)
{
    return MFX_ERR_NONE;
}
mfxStatus Plugin::FreeTask(Task &task)
{

    if (task.m_midBs)
    {
        ReleaseResource(m_bs,  task.m_midBs);
        task.m_midBs = 0;
    }

    if (!m_vpar.RawRef)
    {
        if (task.m_surf)
        {
            m_core.DecreaseReference(&task.m_surf->Data);
            task.m_surf = 0;
        }
        if (task.m_midRaw)
        {
            ReleaseResource(m_raw, task.m_midRaw);
            task.m_midRaw =0;
        }
    }

    if (!(task.m_frameType & MFX_FRAMETYPE_REF))
    {
        if (task.m_midRec)
        {
            ReleaseResource(m_rec, task.m_midRec);
            task.m_midRec = 0;
        }
        if (m_vpar.RawRef)
        {
            if (task.m_surf)
            {
                m_core.DecreaseReference(&task.m_surf->Data);
                task.m_surf = 0;
            }
            if (task.m_midRaw)
            {
                ReleaseResource(m_raw, task.m_midRaw);
                task.m_midRaw = 0;
            }

        }
    }
    for (mfxU16 i = 0, j = 0; !isDpbEnd(task.m_dpb[TASK_DPB_BEFORE], i); i ++)
    {
        for (j = 0; !isDpbEnd(task.m_dpb[TASK_DPB_AFTER], j); j ++)
            if (task.m_dpb[TASK_DPB_BEFORE][i].m_idxRec == task.m_dpb[TASK_DPB_AFTER][j].m_idxRec)
                break;

        if (isDpbEnd(task.m_dpb[TASK_DPB_AFTER], j))
        {
            if (task.m_dpb[TASK_DPB_BEFORE][i].m_midRec)
            {
                ReleaseResource(m_rec, task.m_dpb[TASK_DPB_BEFORE][i].m_midRec);
                task.m_dpb[TASK_DPB_BEFORE][i].m_midRec = 0;
            }

            if (m_vpar.RawRef)
            {
                if (task.m_dpb[TASK_DPB_BEFORE][i].m_surf)
                {
                    m_core.DecreaseReference(&task.m_dpb[TASK_DPB_BEFORE][i].m_surf->Data);
                    task.m_dpb[TASK_DPB_BEFORE][i].m_surf = 0;
                }
                if (task.m_dpb[TASK_DPB_BEFORE][i].m_midRaw)
                {
                    ReleaseResource(m_raw, task.m_dpb[TASK_DPB_BEFORE][i].m_midRaw);
                    task.m_dpb[TASK_DPB_BEFORE][i].m_midRaw = 0;
                }
            }
        }
    }

    return MFX_ERR_NONE;
}
void Plugin::FreeResources()
{
    mfxExtOpaqueSurfaceAlloc& opaq = m_vpar.m_ext.Opaque;

    m_rec.Free();
    m_raw.Free();
    m_bs.Free();

    m_ddi.reset();

    m_frameOrder = 0;
    Zero(m_lastTask);
    Zero(m_caps);

    if (m_vpar.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && opaq.In.Surfaces)
    {
        m_core.UnmapOpaqueSurface(opaq.In.NumSurface, opaq.In.Type, opaq.In.Surfaces);
        Zero(opaq);
    }
}

mfxStatus Plugin::Close()
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_STS(WaitingForAsyncTasks(true));
    FreeResources();

    m_bInit = false;
    return MFX_ERR_NONE;
}

};
