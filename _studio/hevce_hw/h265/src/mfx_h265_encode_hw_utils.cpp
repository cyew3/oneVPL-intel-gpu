//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_utils.h"
#include <assert.h>

namespace MfxHwH265Encode
{

MfxFrameAllocResponse::MfxFrameAllocResponse()
{
    Zero(*(mfxFrameAllocResponse*)this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    if (m_core == 0)
        return;

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    mfxCoreParam par = {};

    m_core->GetCoreParam(&par);

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
        {
            fa.Free(fa.pthis, &m_responseQueue[i]);
        }
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            fa.Free(fa.pthis, this);
        }
    }

} 

mfxStatus MfxFrameAllocResponse::Alloc(
    MFXCoreInterface *     core,
    mfxFrameAllocRequest & req,
    bool                   /*isCopyRequired*/)
{
    mfxFrameAllocator & fa = core->FrameAllocator();
    mfxCoreParam par = {};
    mfxFrameAllocResponse & response = *(mfxFrameAllocResponse*)this;

    core->GetCoreParam(&par);

    req.NumFrameSuggested = req.NumFrameMin;

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = fa.Alloc(fa.pthis, &tmp, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);
            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = fa.Alloc(fa.pthis, &req, &response);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;
    
    m_locked.resize(req.NumFrameMin, 0);

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;

    return MFX_ERR_NONE;
} 

mfxStatus MfxFrameAllocResponse::Alloc(
    MFXCoreInterface *     core,
    mfxFrameAllocRequest & req,
    mfxFrameSurface1 **    /*opaqSurf*/,
    mfxU32                 /*numOpaqSurf*/)
{
    req.NumFrameSuggested = req.NumFrameMin;

    assert(!"not implemented");
    mfxStatus sts = MFX_ERR_UNSUPPORTED;//core->AllocFrames(&req, this, opaqSurf, numOpaqSurf);
    MFX_CHECK_STS(sts);

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;

    return MFX_ERR_NONE;
}

mfxU32 MfxFrameAllocResponse::Lock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
}

void MfxFrameAllocResponse::Unlock()
{
    std::fill(m_locked.begin(), m_locked.end(), 0);
}

mfxU32 MfxFrameAllocResponse::Unlock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return mfxU32(-1);
    assert(m_locked[idx] > 0);
    return --m_locked[idx];
}

mfxU32 MfxFrameAllocResponse::Locked(mfxU32 idx) const
{
    return (idx < m_locked.size()) ? m_locked[idx] : 1;
}

mfxU32 FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom)
{
    for (mfxU32 i = startingFrom; i < pool.NumFrameActual; i++)
        if (pool.Locked(i) == 0)
            return i;
    return 0xFFFFFFFF;
}

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    if (index > pool.NumFrameActual)
        return 0;
    pool.Lock(index);
    return pool.mids[index];
}

void ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid)
{
    for (mfxU32 i = 0; i < pool.NumFrameActual; i++)
    {
        if (pool.mids[i] == mid)
        {
            pool.Unlock(i);
            break;
        }
    }
}

mfxStatus GetNativeHandleToRawSurface(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task,
    mfxHDLPair &          handle)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocator & fa = core.FrameAllocator();

    //mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);

    Zero(handle);
    mfxHDL * nativeHandle = &handle.first;

    mfxFrameSurface1 * surface = task.m_surf;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        assert(!"not implemented");
        //surface = core.GetNativeSurface(task.m_yuv);
        //if (surface == 0)
        //    return Error(MFX_ERR_UNDEFINED_BEHAVIOR);
        //
        //surface->Info            = task.m_yuv->Info;
        //surface->Data.TimeStamp  = task.m_yuv->Data.TimeStamp;
        //surface->Data.FrameOrder = task.m_yuv->Data.FrameOrder;
        //surface->Data.Corrupted  = task.m_yuv->Data.Corrupted;
        //surface->Data.DataFlag   = task.m_yuv->Data.DataFlag;
    }

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY /*||
        video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)*/)
        sts = fa.GetHDL(fa.pthis, task.m_midRaw, nativeHandle);
    else if (video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        sts = fa.GetHDL(fa.pthis, surface->Data.MemId, nativeHandle);
    //else if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY) // opaq with internal video memory
    //    sts = core.GetFrameHDL(surface->Data.MemId, nativeHandle);
    else
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    if (nativeHandle == 0)
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    return sts;
}

mfxStatus CopyRawSurfaceToVideoMemory(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task)
{
    //mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);
    mfxFrameSurface1 * surface = task.m_surf;

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        /*|| video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)*/)
    {
        if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            assert(!"not implemented");
            //surface = core.GetNativeSurface(task.m_yuv);
            //if (surface == 0)
            //    return Error(MFX_ERR_UNDEFINED_BEHAVIOR);
            //
            //surface->Info            = task.m_yuv->Info;
            //surface->Data.TimeStamp  = task.m_yuv->Data.TimeStamp;
            //surface->Data.FrameOrder = task.m_yuv->Data.FrameOrder;
            //surface->Data.Corrupted  = task.m_yuv->Data.Corrupted;
            //surface->Data.DataFlag   = task.m_yuv->Data.DataFlag;
        }
        
        mfxFrameAllocator & fa = core.FrameAllocator();
        mfxStatus sts = MFX_ERR_NONE;
        mfxFrameData d3dSurf = {};
        mfxFrameData sysSurf = surface->Data;
        d3dSurf.MemId = task.m_midRaw;
        bool sysLocked = false;
        
        mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };

        if (!sysSurf.Y)
        {
            sts = fa.Lock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
            sysLocked = true;
        }
        
        sts = fa.Lock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);

        sts = core.CopyFrame(&surfDst, &surfSrc);

        if (sysLocked)
        {
            sts = fa.Unlock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
        }
        
        sts = fa.Unlock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);

        //core.LockExternalFrame(sysSurf.MemId, &sysSurf);
        //
        //if (sysSurf.Y == 0)
        //    return (MFX_ERR_LOCK_MEMORY);
        //
        //
        //{
        //    mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        //    mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };
        //    sts = core.DoFastCopyWrapper(&surfDst, MFX_MEMTYPE_D3D_INT, &surfSrc, MFX_MEMTYPE_SYS_EXT);
        //    MFX_CHECK_STS(sts);
        //}
        //
        //sts = core.UnlockExternalFrame(sysSurf.MemId, &sysSurf);
        //MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

MfxVideoParam::MfxVideoParam()
    : BufferSizeInKB  (0)
    , InitialDelayInKB(0)
    , TargetKbps      (0)
    , MaxKbps         (0)
{
    Zero(*(mfxVideoParam*)this);
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);

    BufferSizeInKB   = BufferSizeInKB;
    InitialDelayInKB = InitialDelayInKB;
    TargetKbps       = TargetKbps;
    MaxKbps          = MaxKbps;
}

MfxVideoParam::MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
}

void MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = MFX_MAX(mfx.BRCParamMultiplier, 1);

    BufferSizeInKB   = mfx.BufferSizeInKB   * multiplier;
    InitialDelayInKB = mfx.InitialDelayInKB * multiplier;
    TargetKbps       = mfx.TargetKbps       * multiplier;
    MaxKbps          = mfx.MaxKbps          * multiplier;
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = BufferSizeInKB;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = MFX_MAX(maxVal32, TargetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
            maxVal32 = MFX_MAX(maxVal32, MFX_MAX(MaxKbps, InitialDelayInKB));
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    mfx.BufferSizeInKB     = mfxU16((BufferSizeInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        mfx.TargetKbps = mfxU16((TargetKbps + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = mfxU16((InitialDelayInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);
            mfx.MaxKbps          = mfxU16((MaxKbps + mfx.BRCParamMultiplier - 1)          / mfx.BRCParamMultiplier);
        }
    }
}

MfxVideoParam& MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
    return *this;
}

void TaskManager::Reset(mfxU32 numTask)
{
    m_free.resize(numTask);
    m_reordering.resize(0);
    m_encoding.resize(0);
}

Task* TaskManager::New()
{
    UMC::AutomaticUMCMutex guard(m_listMutex);
    Task* pTask = 0;

    if (!m_free.empty())
    {
        pTask = &m_free.front();
        m_reordering.splice(m_reordering.end(), m_free, m_free.begin());
    }

    return pTask;
}

mfxU32 CountL1(DpbArray const & dpb, mfxI32 poc)
{
    mfxU32 c = 0;
    for (mfxU32 i = 0; !isDpbEnd(dpb, i); i ++)
        c += dpb[i].m_poc > poc;
    return c;
}

Task* TaskManager::Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    bool flush)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    TaskList::iterator begin = m_reordering.begin();
    TaskList::iterator end = m_reordering.end();
    TaskList::iterator top = begin;

    //TODO: add B-ref
    while ( top != end 
        && (top->m_frameType & MFX_FRAMETYPE_B)
        && !CountL1(dpb, top->m_poc))
        top ++;

    if (flush && top == end && begin != end)
    {
        if (par.mfx.GopOptFlag & MFX_GOP_STRICT)
            top = begin;
        else
        {
            top --;
            top->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }
    }

    if (top == end)
        return 0;

    m_encoding.splice(m_encoding.end(), m_reordering, top);

    return &m_encoding.back();
}

void TaskManager::Ready(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_encoding.begin(); it != m_encoding.end(); it ++)
    {
        if (pTask == &*it)
        {
            m_free.splice(m_free.end(), m_encoding, it);
            break;
        }
    }
}



mfxU8 GetFrameType(
    MfxVideoParam const & video,
    mfxU32                frameOrder)
{
    mfxU32 gopOptFlag = MFX_GOP_CLOSED;//video.mfx.GopOptFlag;
    mfxU32 gopPicSize = video.mfx.GopPicSize;
    mfxU32 gopRefDist = video.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval + 1);

    //TODO: add B-ref

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (frameOrder % idrPicDist == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        if ((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED) ||
            (frameOrder + 1) % idrPicDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    return (MFX_FRAMETYPE_B);
}

void ConfigureTask(
    Task &                task,
    Task const &          prevTask,
    MfxVideoParam const & par)
{
    const bool isI    = !!(task.m_frameType & MFX_FRAMETYPE_I);
    const bool isP    = !!(task.m_frameType & MFX_FRAMETYPE_P);
    const bool isB    = !!(task.m_frameType & MFX_FRAMETYPE_B);
    const bool isRef  = !!(task.m_frameType & MFX_FRAMETYPE_REF);

    // set coding type and QP
    if (isB)
    {
        task.m_codingType = 3; //TODO: add B1, B2
        task.m_qpY = (mfxU8)par.mfx.QPB;
    }
    else if (isP)
    {
        task.m_codingType = 2;
        task.m_qpY = (mfxU8)par.mfx.QPP;
    }
    else
    {
        assert(task.m_frameType & MFX_FRAMETYPE_I);
        task.m_codingType = 1;
        task.m_qpY = (mfxU8)par.mfx.QPI;
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        task.m_qpY = 0;
    else if (task.m_ctrl.QP)
        task.m_qpY = (mfxU8)task.m_ctrl.QP;

    Copy(task.m_dpb[0], prevTask.m_dpb[1]);

    //construct ref lists
    task.m_numRefActive[0] = task.m_numRefActive[1] = 0;
    Fill(task.m_refPicList, IDX_INVALID);

    if (!isI)
    {
        mfxU8 l0 = 0, l1 = 0;

        for (mfxU8 i = 0; !isDpbEnd(task.m_dpb[0], i); i ++)
        {
            if (task.m_poc > task.m_dpb[0][i].m_poc)
                task.m_refPicList[0][l0++] = i;
            else if (isB)
                task.m_refPicList[1][l1++] = i;
        }

        assert(l0 > 0);

        if (isB && !l1)
        {
            task.m_refPicList[1][l1++] = task.m_refPicList[0][l0-1];
        }

        task.m_numRefActive[0] = l0;
        task.m_numRefActive[1] = l1;
    }


    // update dpb
    if (isI)
        Fill(task.m_dpb[1], IDX_INVALID);
    else
        Copy(task.m_dpb[1], task.m_dpb[0]);

    if (isRef)
    {
        for (mfxU16 i = 0; i < MAX_DPB_SIZE; i ++)
        {
            if (task.m_dpb[1][i].m_idxRec == IDX_INVALID)
            {
                if (i && i == par.mfx.NumRefFrame)
                    memmove(task.m_dpb[1], task.m_dpb[1] + 1, (--i) * sizeof(DpbFrame));

                task.m_dpb[1][i] = task;
                break;
            }
        }
    }

}

}; //namespace MfxHwH265Encode
