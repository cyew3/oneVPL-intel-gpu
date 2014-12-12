/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/
#include "mfx_h265_encode_hw.h"
#include <assert.h>

namespace MfxHwH265Encode
{

mfxStatus CheckVideoParam(MfxVideoParam & par, ENCODE_CAPS_HEVC const & caps);
void      SetDefaults    (MfxVideoParam & par, ENCODE_CAPS_HEVC const & hwCaps);

Plugin::Plugin(bool CreateByDispatcher)
    : m_createdByDispatcher(CreateByDispatcher)
    , m_adapter(this)
{
}

Plugin::~Plugin()
{
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

mfxStatus Plugin::Init(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE, qsts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    m_ddi.reset( CreatePlatformH265Encoder(&m_core) );
    MFX_CHECK(m_ddi.get(), MFX_WRN_PARTIAL_ACCELERATION);

    sts = m_ddi->CreateAuxilliaryDevice(
        &m_core,
        GetGUID(*par),
        par->mfx.FrameInfo.Width,
        par->mfx.FrameInfo.Height);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_WRN_PARTIAL_ACCELERATION);

    sts = m_ddi->QueryEncodeCaps(m_caps);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_WRN_PARTIAL_ACCELERATION);
    
    m_vpar = *par;

    qsts = Query(&m_vpar, &m_vpar);
    MFX_CHECK(qsts >= MFX_ERR_NONE, qsts);

    SetDefaults(m_vpar, m_caps);

    m_vpar.SyncCalculableToVideoParam();
    m_vpar.SyncMfxToHeadersParam();

    sts = m_ddi->CreateAccelerationService(m_vpar);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_WRN_PARTIAL_ACCELERATION);

    mfxFrameAllocRequest request = {};
    request.Info = m_vpar.mfx.FrameInfo;

    if (m_vpar.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = m_vpar.AsyncDepth + m_vpar.mfx.GopRefDist - 1 + m_vpar.mfx.NumRefFrame;

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
    request.NumFrameMin = m_vpar.mfx.NumRefFrame + 1;

    sts = m_rec.Alloc(&m_core, request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_rec.NumFrameActual ? m_rec : m_raw, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request);
    MFX_CHECK_STS(sts);

    request.Type        = MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = 1;
    
    sts = m_bs.Alloc(&m_core, request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_bs, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    m_task.Reset(m_vpar.AsyncDepth + m_vpar.mfx.GopRefDist - 1);

    m_frameOrder = 0;

    Fill(m_lastTask.m_dpb[1], IDX_INVALID);

    return qsts;
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, mfxFrameAllocRequest * /*out*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR2(par, request);
    
    MfxVideoParam tmp = *par;
    ENCODE_CAPS_HEVC caps = {};

    switch (par->IOPattern & MFX_IOPATTERN_IN_MASK)
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        request->Type = MFX_MEMTYPE_SYS_EXT;
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        request->Type = MFX_MEMTYPE_D3D_EXT;
        break;
    case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
        request->Type = MFX_MEMTYPE_D3D_EXT|MFX_MEMTYPE_OPAQUE_FRAME;
        break;
    default: return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    sts = QueryHwCaps(&m_core, GetGUID(tmp), caps);
    MFX_CHECK_STS(sts);

    CheckVideoParam(tmp, caps);
    SetDefaults(tmp, caps);

    request->Info = tmp.mfx.FrameInfo;

    if (tmp.mfx.EncodedOrder)
        request->NumFrameMin = 1;
    else
        request->NumFrameMin = tmp.AsyncDepth + tmp.mfx.GopRefDist - 1 + tmp.mfx.NumRefFrame;

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
        out->Protected             = 1;
        out->AsyncDepth            = 1;
        out->mfx.CodecId           = 1;
        out->mfx.LowPower          = 1;
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
        out->mfx.FrameInfo.PicStruct     = 1;
    }
    else
    {
        MfxVideoParam tmp = *in;
        ENCODE_CAPS_HEVC caps = {};

        sts = QueryHwCaps(&m_core, GetGUID(tmp), caps);
        MFX_CHECK_STS(sts);

        sts = CheckVideoParam(tmp, caps);

        tmp.SyncCalculableToVideoParam();
        *out = tmp;

        tmp.GetExtBuffers(*out, true);
    }

    return sts;
}

mfxStatus Plugin::Reset(mfxVideoParam *par)
{
    par;
    assert(!"not implemented");
    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetVideoParam(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    *par = m_vpar;
    sts = m_vpar.GetExtBuffers(*par);

    return sts;
}

mfxStatus Plugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *thread_task)
{
    mfxStatus sts = MFX_ERR_NONE;
    Task* task = 0;
    
    //TODO: check par here


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

        m_core.IncreaseReference(&surface->Data);

        if (m_vpar.mfx.EncodedOrder)
        {
            task->m_frameType = task->m_ctrl.FrameType;
        }
        else
        {
            task->m_frameType = GetFrameType(m_vpar, m_frameOrder);
            if (task->m_frameType & MFX_FRAMETYPE_IDR)
                m_frameOrder = 0;
            task->m_poc = m_frameOrder;
        }

        m_frameOrder ++;
        task->m_stage |= FRAME_ACCEPTED;
    }

    task = m_task.Reorder(m_vpar, m_lastTask.m_dpb[1], !surface);
    MFX_CHECK(task, MFX_ERR_MORE_DATA);

    task->m_stage |= FRAME_REORDERED;
            
    task->m_idxRaw = (mfxU8)FindFreeResourceIndex(m_raw);
    task->m_idxRec = (mfxU8)FindFreeResourceIndex(m_rec);
    task->m_idxBs  = (mfxU8)FindFreeResourceIndex(m_bs);

    assert(task->m_idxRec != IDX_INVALID);
    assert(task->m_idxBs  != IDX_INVALID);

    task->m_midRaw = AcquireResource(m_raw, task->m_idxRaw);
    task->m_midRec = AcquireResource(m_rec, task->m_idxRec);
    task->m_midBs  = AcquireResource(m_bs,  task->m_idxBs);
    MFX_CHECK(task->m_midRec && task->m_midBs, MFX_ERR_UNDEFINED_BEHAVIOR);
            
    ConfigureTask(*task, m_lastTask, m_vpar);
    m_lastTask = *task;

    task->m_bs = bs;

    *thread_task = task;

    return sts;
}

mfxStatus Plugin::Execute(mfxThreadTask thread_task, mfxU32 /*uid_p*/, mfxU32 /*uid_a*/)
{
    assert(thread_task);
    Task& task = *(Task*)thread_task;
    mfxBitstream* bs = task.m_bs;
    mfxStatus sts = MFX_ERR_NONE;

    //submit
    if (task.m_stage == STAGE_SUBMIT)
    {
        mfxHDLPair surfaceHDL = {};

        sts = GetNativeHandleToRawSurface(m_core, m_vpar, task, surfaceHDL);
        MFX_CHECK_STS(sts);

        sts = CopyRawSurfaceToVideoMemory(m_core, m_vpar, task);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Execute(task, surfaceHDL.first);
        MFX_CHECK_STS(sts);

        task.m_stage |= FRAME_SUBMITTED;
    }
    
    //query
    if (task.m_stage == STAGE_QUERY)
    {
        sts = m_ddi->QueryStatus(task);
        MFX_CHECK_STS(sts);
        
        //update bitstream
        if (task.m_bsDataLength)
        {
            mfxFrameAllocator & fa = m_core.FrameAllocator();
            mfxFrameData codedFrame = {};
            mfxU32 bytesAvailable = bs->MaxLength - bs->DataOffset - bs->DataLength;
            mfxU32 bytes2copy     = task.m_bsDataLength;

            assert(bytesAvailable > bytes2copy);
            bytes2copy = MFX_MIN(bytes2copy, bytesAvailable);

            sts = fa.Lock(fa.pthis, task.m_midBs, &codedFrame);
            MFX_CHECK(codedFrame.Y, MFX_ERR_LOCK_MEMORY);

            memcpy(bs->Data + bs->DataOffset + bs->DataLength, codedFrame.Y, bytes2copy);

            sts = fa.Unlock(fa.pthis, task.m_midBs, &codedFrame);
            MFX_CHECK_STS(sts);

            bs->DataLength += bytes2copy;

            bs->TimeStamp       = task.m_surf->Data.TimeStamp;
            //bs->DecodeTimeStamp = MFX_TIMESTAMP_UNKNOWN;
            bs->PicStruct       = MFX_PICSTRUCT_PROGRESSIVE;
            bs->FrameType       = task.m_frameType;
        }

        task.m_stage |= FRAME_ENCODED;
    }

    return sts;
}

mfxStatus Plugin::FreeResources(mfxThreadTask thread_task, mfxStatus /*sts*/)
{
    assert(thread_task);
    Task& task = *(Task*)thread_task;

    ReleaseResource(m_bs,  task.m_midBs);

    if (!(task.m_frameType & MFX_FRAMETYPE_REF))
    {
        ReleaseResource(m_rec, task.m_midRec);
        m_core.DecreaseReference(&task.m_surf->Data);
        ReleaseResource(m_raw, task.m_midRaw);
    }
    else if (task.m_stage != STAGE_READY)
    {
        ReleaseResource(m_rec, task.m_midRec);

        for (mfxU16 i = 0; !isDpbEnd(task.m_dpb[1], i); i ++)
            if (task.m_dpb[1][i].m_idxRec == task.m_idxRec)
                Fill(task.m_dpb[1][i], IDX_INVALID);
    }

    for (mfxU16 i = 0, j = 0; !isDpbEnd(task.m_dpb[0], i); i ++)
    {
        for (j = 0; !isDpbEnd(task.m_dpb[1], j); j ++)
            if (task.m_dpb[0][i].m_idxRec == task.m_dpb[1][j].m_idxRec)
                break;

        if (isDpbEnd(task.m_dpb[1], j))
        {
            ReleaseResource(m_rec, task.m_dpb[0][i].m_midRec);
            m_core.DecreaseReference(&task.m_dpb[0][i].m_surf->Data);
            ReleaseResource(m_raw, task.m_dpb[0][i].m_midRaw);
        }
    }

    m_task.Ready(&task);

    return MFX_ERR_NONE;
}

mfxStatus Plugin::Close()
{
    mfxExtOpaqueSurfaceAlloc& opaq = m_vpar.m_ext.Opaque;

    m_rec.Free();
    m_raw.Free();
    m_bs.Free();

    if (m_ddi.get())
        delete m_ddi.release();

    m_frameOrder = 0;
    Zero(m_lastTask);
    Zero(m_caps);

    if (m_vpar.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && opaq.In.Surfaces)
    {
        m_core.UnmapOpaqueSurface(opaq.In.NumSurface, opaq.In.Type, opaq.In.Surfaces);
        Zero(opaq);
    }

    return MFX_ERR_NONE;
}

};
