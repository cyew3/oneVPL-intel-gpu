//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_utils.h"
#include <algorithm>
#include <functional>
#include <list>
#include <assert.h>

namespace MfxHwH265Encode
{

mfxU32 CountL1(DpbArray const & dpb, mfxI32 poc)
{
    mfxU32 c = 0;
    for (mfxU32 i = 0; !isDpbEnd(dpb, i); i ++)
        c += dpb[i].m_poc > poc;
    return c;
}

template<class T> T Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    T begin,
    T end,
    bool flush)
{
    T top  = begin;
    T b0 = end; // 1st non-ref B with L1 > 0

    while ( top != end && (top->m_frameType & MFX_FRAMETYPE_B))
    {
        if (CountL1(dpb, top->m_poc))
        {
            if (   (top->m_frameType & MFX_FRAMETYPE_REF)
                && (b0 == end || (top->m_poc - b0->m_poc < 2)))
                return top;

            if (b0 == end && !(top->m_frameType & MFX_FRAMETYPE_REF))
                b0 = top;
        }

        top ++;
    }

    if (b0 != end)
        return b0;

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

    return top;
}

MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_core(0)
{
    Zero(*(mfxFrameAllocResponse*)this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();
} 

void MfxFrameAllocResponse::Free()
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
        m_responseQueue.resize(0);
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            fa.Free(fa.pthis, this);
            mids = 0;
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
    mfxExtOpaqueSurfaceAlloc const & opaq = video.m_ext.Opaque;

    Zero(handle);

    mfxHDL * nativeHandle = &handle.first;
    mfxFrameSurface1 * surface = task.m_surf_real;

    if (   video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
        sts = fa.GetHDL(fa.pthis, task.m_midRaw, nativeHandle);
    else if (   video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY
             || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        sts = fa.GetHDL(fa.pthis, surface->Data.MemId, nativeHandle);
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
    mfxStatus sts = MFX_ERR_NONE;
    mfxExtOpaqueSurfaceAlloc const & opaq = video.m_ext.Opaque;
    mfxFrameSurface1 * surface = task.m_surf_real;

    if (   video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
    {
        mfxFrameAllocator & fa = core.FrameAllocator();
        mfxFrameData d3dSurf = {};
        mfxFrameData sysSurf = surface->Data;
        d3dSurf.MemId = task.m_midRaw;
        bool sysLocked = !!sysSurf.Y;
        
        mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };

        if (!sysLocked)
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
    }

    return sts;
}

namespace ExtBuffer
{
    bool Construct(mfxVideoParam const & par, mfxExtHEVCParam& buf)
    {
        if (!Construct<mfxVideoParam, mfxExtHEVCParam>(par, buf))
        {
            buf.PicWidthInLumaSamples  = Align(Max(par.mfx.FrameInfo.Width,  par.mfx.FrameInfo.CropW), CODED_PIC_ALIGN_W);
            buf.PicHeightInLumaSamples = Align(Max(par.mfx.FrameInfo.Height, par.mfx.FrameInfo.CropH), CODED_PIC_ALIGN_H);

            return false;
        }

        return true;
    }
};

MfxVideoParam::MfxVideoParam()
    : BufferSizeInKB  (0)
    , InitialDelayInKB(0)
    , TargetKbps      (0)
    , MaxKbps         (0)
    , LTRInterval     (0)
    , LCUSize         (DEFAULT_LCU_SIZE)
    , BRef            (false)
{
    Zero(*(mfxVideoParam*)this);
    Zero(NumRefLX);
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);

    //Copy(m_vps, par.m_vps);
    //Copy(m_sps, par.m_sps);
    //Copy(m_pps, par.m_pps);

    BufferSizeInKB   = par.BufferSizeInKB;
    InitialDelayInKB = par.InitialDelayInKB;
    TargetKbps       = par.TargetKbps;
    MaxKbps          = par.MaxKbps;
    NumRefLX[0]      = par.NumRefLX[0];
    NumRefLX[1]      = par.NumRefLX[1];
    LTRInterval      = par.LTRInterval;
    LCUSize          = par.LCUSize;
    BRef             = par.BRef;
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

    ExtBuffer::Construct(par, m_ext.HEVCParam);
    ExtBuffer::Construct(par, m_ext.Opaque);
}

mfxStatus MfxVideoParam::GetExtBuffers(mfxVideoParam& par, bool /*query*/)
{
    ExtBuffer::Set(par, m_ext.HEVCParam);
    ExtBuffer::Set(par, m_ext.Opaque);

    return MFX_ERR_NONE;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = Max<mfxU32>(mfx.BRCParamMultiplier, 1);

    BufferSizeInKB = mfx.BufferSizeInKB * multiplier;
    LTRInterval    = 0;
    LCUSize        = DEFAULT_LCU_SIZE;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        InitialDelayInKB = mfx.InitialDelayInKB * multiplier;
        TargetKbps       = mfx.TargetKbps       * multiplier;
        MaxKbps          = mfx.MaxKbps          * multiplier;
    } else
    {
        InitialDelayInKB = 0;
        TargetKbps       = 0;
        MaxKbps          = 0;
    }

    if (mfx.NumRefFrame)
    {
        NumRefLX[1] = 1;
        NumRefLX[0] = mfx.NumRefFrame - (mfx.GopRefDist > 1);
    } else
    {
        NumRefLX[0] = 0;
        NumRefLX[1] = 0;
    }

    BRef = (mfx.GopRefDist > 3) && NumRefLX[0] >= 2;

    m_slice.resize(0);
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = BufferSizeInKB;

    mfx.NumRefFrame = NumRefLX[0] + (mfx.GopRefDist > 1) * NumRefLX[1];

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = Max(maxVal32, TargetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
            maxVal32 = Max(maxVal32, Max(MaxKbps, InitialDelayInKB));
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    mfx.BufferSizeInKB     = (mfxU16)CeilDiv(BufferSizeInKB, mfx.BRCParamMultiplier);

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        mfx.TargetKbps = (mfxU16)CeilDiv(TargetKbps, mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = (mfxU16)CeilDiv(InitialDelayInKB, mfx.BRCParamMultiplier);
            mfx.MaxKbps          = (mfxU16)CeilDiv(MaxKbps, mfx.BRCParamMultiplier);
        }
    }
    mfx.NumSlice = (mfxU16)m_slice.size();
}

mfxU8 GetAspectRatioIdc(mfxU16 w, mfxU16 h)
{
    if (w ==   0 || h ==  0) return  0;
    if (w ==   1 && h ==  1) return  1;
    if (w ==  12 && h == 11) return  2;
    if (w ==  10 && h == 11) return  3;
    if (w ==  16 && h == 11) return  4;
    if (w ==  40 && h == 33) return  5;
    if (w ==  24 && h == 11) return  6;
    if (w ==  20 && h == 11) return  7;
    if (w ==  32 && h == 11) return  8;
    if (w ==  80 && h == 33) return  9;
    if (w ==  18 && h == 11) return 10;
    if (w ==  15 && h == 11) return 11;
    if (w ==  64 && h == 33) return 12;
    if (w == 160 && h == 99) return 13;
    if (w ==   4 && h ==  3) return 14;
    if (w ==   3 && h ==  2) return 15;
    if (w ==   2 && h ==  1) return 16;
    return 255;
}

struct FakeTask
{
    mfxI32 m_poc;
    mfxU16 m_frameType;
};

struct STRPSFreq : STRPS
{
    STRPSFreq(STRPS const & rps, mfxU32 n)
    {
        *(STRPS*)this = rps;
        N = n;
    }
    mfxU32 N;
};

class EqSTRPS
{
private:
    STRPS const & m_ref;
public:
    EqSTRPS(STRPS const & ref) : m_ref(ref) {};
    bool operator () (STRPSFreq const & cur) { return Equal<STRPS>(m_ref, cur); };
    EqSTRPS & operator = (EqSTRPS const &) { assert(0); return *this; }
};

bool Greater(STRPSFreq const & l, STRPSFreq const r)
{
    return (l.N > r.N);
}

mfxU32 NBitsUE(mfxU32 b)
{
    mfxU32 n = 1;

     if (!b)
         return n;

    b ++;

    while (b >> n)
        n ++;

    return n * 2 - 1;
};

template<class T> mfxU32 NBits(T const & list, mfxU8 nSet, STRPS const & rps, mfxU8 idx)
{
    mfxU32 n = (idx != 0);
    mfxU32 nPic = mfxU32(rps.num_negative_pics + rps.num_positive_pics);

    if (rps.inter_ref_pic_set_prediction_flag)
    {
        assert(idx > rps.delta_idx_minus1);
        STRPS const & ref = list[idx - rps.delta_idx_minus1 - 1];
        nPic = mfxU32(ref.num_negative_pics + ref.num_positive_pics);

        if (idx == nSet)
            n += NBitsUE(rps.delta_idx_minus1);

        n += 1;
        n += NBitsUE(rps.abs_delta_rps_minus1);
        n += nPic;

        for (mfxU32 i = 0; i <= nPic; i ++)
            if (!rps.pic[i].used_by_curr_pic_flag)
                n ++;

        return n;
    }

    n += NBitsUE(rps.num_negative_pics);
    n += NBitsUE(rps.num_positive_pics);

    for (mfxU32 i = 0; i < nPic; i ++)
        n += NBitsUE(rps.pic[i].delta_poc_sx_minus1) + 1;

    return n;
}

template<class T> void OptimizeSTRPS(T const & list, mfxU8 n, STRPS& oldRPS, mfxU8 idx)
{
    if (idx == 0)
        return;

    STRPS newRPS;
    mfxI8 k = 0, i = 0, j = 0;

    for (k = idx - 1; k >= 0; k --)
    {
        STRPS const & refRPS = list[k];

        if ((refRPS.num_negative_pics + refRPS.num_positive_pics + 1)
             < (oldRPS.num_negative_pics + oldRPS.num_positive_pics))
            continue;

        newRPS = oldRPS;
        newRPS.inter_ref_pic_set_prediction_flag = 1;
        newRPS.delta_idx_minus1 = (idx - k - 1);

        std::list<mfxI16> dPocs[2];
        mfxI16 dPoc = 0;
        bool found = false;

        for (i = 0; i < oldRPS.num_negative_pics + oldRPS.num_positive_pics; i ++)
        {
            dPoc = oldRPS.pic[i].DeltaPocSX;
            if (dPoc)
                dPocs[dPoc > 0].push_back(dPoc);

            for (j = 0; j < refRPS.num_negative_pics + refRPS.num_positive_pics; j ++)
            {
                dPoc = oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX;
                if (dPoc)
                    dPocs[dPoc > 0].push_back(dPoc);
            }
        }

        dPocs[0].sort(std::greater<mfxI16>());
        dPocs[1].sort(std::less<mfxI16>());
        dPocs[0].unique();
        dPocs[1].unique();

        dPoc = 0;

        while ((!dPocs[0].empty() || !dPocs[1].empty()) && !found)
        {
            dPoc *= -1;
            bool positive = (dPoc > 0 && !dPocs[1].empty()) || dPocs[0].empty();
            dPoc = dPocs[positive].front();
            dPocs[positive].pop_front();

            for (i = 0; i <= refRPS.num_negative_pics + refRPS.num_positive_pics; i ++)
                newRPS.pic[i].used_by_curr_pic_flag = newRPS.pic[i].use_delta_flag = 0;

            i = 0;
            for (j = refRPS.num_negative_pics + refRPS.num_positive_pics - 1; 
                 j >= refRPS.num_negative_pics; j --)
            {
                if (   oldRPS.pic[i].DeltaPocSX < 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (dPoc < 0 && oldRPS.pic[i].DeltaPocSX == dPoc)
            {
                j = refRPS.num_negative_pics + refRPS.num_positive_pics;
                newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                newRPS.pic[j].use_delta_flag = 1;
                i ++;
            }

            for (j = 0; j < refRPS.num_negative_pics; j ++)
            {
                if (   oldRPS.pic[i].DeltaPocSX < 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (i != oldRPS.num_negative_pics)
                continue;

            for (j = refRPS.num_negative_pics - 1; j >= 0; j --)
            {
                if (   oldRPS.pic[i].DeltaPocSX > 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (dPoc > 0 && oldRPS.pic[i].DeltaPocSX == dPoc)
            {
                j = refRPS.num_negative_pics + refRPS.num_positive_pics;
                newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                newRPS.pic[j].use_delta_flag = 1;
                i ++;
            }

            for (j = refRPS.num_negative_pics;
                 j < refRPS.num_negative_pics + refRPS.num_positive_pics; j ++)
            {
                if (   oldRPS.pic[i].DeltaPocSX > 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            found = (i == oldRPS.num_negative_pics + oldRPS.num_positive_pics);
        }

        if (found)
        {
            newRPS.delta_rps_sign       = (dPoc < 0);
            newRPS.abs_delta_rps_minus1 = Abs(dPoc) - 1;

            if (NBits(list, n, newRPS, idx) < NBits(list, n, oldRPS, idx))
                oldRPS = newRPS;
        }

        if (idx < n)
            break;
    }
}

void ReduceSTRPS(std::vector<STRPSFreq> & sets, mfxU32 NumSlice)
{
    if (sets.empty())
        return;

    STRPS  rps = sets.back();
    mfxU32 n = sets.back().N; //current RPS used for N frames
    mfxU8  nSet = mfxU8(sets.size());
    mfxU32 bits0 = //bits for RPS in SPS and SSHs
        NBits(sets, nSet, rps, nSet - 1) //bits for RPS in SPS
        + (CeilLog2(nSet) - CeilLog2(nSet - 1)) //diff of bits for STRPS num in SPS
        + (nSet > 1) * (NumSlice * CeilLog2((mfxU32)sets.size()) * n); //bits for RPS idx in SSHs
    
    //emulate removal of current RPS from SPS
    nSet --; 
    rps.inter_ref_pic_set_prediction_flag = 0;
    OptimizeSTRPS(sets, nSet, rps, nSet);

    mfxU32 bits1 = NBits(sets, nSet, rps, nSet) * NumSlice * n;//bits for RPS in SSHs (no RPS in SPS)

    if (bits1 < bits0)
    {
        sets.pop_back();
        ReduceSTRPS(sets, NumSlice);
    }
}

void MfxVideoParam::SyncMfxToHeadersParam()
{
    PTL& general = m_vps.general;
    SubLayerOrdering& slo = m_vps.sub_layer[0];

    Zero(m_vps);
    m_vps.video_parameter_set_id    = 0;
    m_vps.reserved_three_2bits      = 3;
    m_vps.max_layers_minus1         = 0;
    m_vps.max_sub_layers_minus1     = 0;
    m_vps.temporal_id_nesting_flag  = 1;
    m_vps.reserved_0xffff_16bits    = 0xFFFF;
    m_vps.sub_layer_ordering_info_present_flag = 0;

    m_vps.timing_info_present_flag          = 1;
    m_vps.num_units_in_tick                 = mfx.FrameInfo.FrameRateExtD;
    m_vps.time_scale                        = mfx.FrameInfo.FrameRateExtN;
    m_vps.poc_proportional_to_timing_flag   = 0;
    m_vps.num_hrd_parameters                = 0;

    general.profile_space               = 0;
    general.tier_flag                   = !!(mfx.CodecLevel & MFX_TIER_HEVC_HIGH);
    general.profile_idc                 = (mfxU8)mfx.CodecProfile;
    general.profile_compatibility_flags = 0;
    general.progressive_source_flag     = !!(mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);
    general.interlaced_source_flag      = !!(mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF));
    general.non_packed_constraint_flag  = 0;
    general.frame_only_constraint_flag  = 0;
    general.level_idc                   = (mfxU8)(mfx.CodecLevel & 0xFF) * 3;

    slo.max_dec_pic_buffering_minus1    = mfx.NumRefFrame;
    slo.max_num_reorder_pics            = mfx.GopRefDist - 1;
    slo.max_latency_increase_plus1      = 0;

    assert(0 == m_vps.max_sub_layers_minus1);

    Zero(m_sps);
    ((LayersInfo&)m_sps) = ((LayersInfo&)m_vps);
    m_sps.video_parameter_set_id   = m_vps.video_parameter_set_id;
    m_sps.max_sub_layers_minus1    = m_vps.max_sub_layers_minus1;
    m_sps.temporal_id_nesting_flag = m_vps.temporal_id_nesting_flag;

    m_sps.seq_parameter_set_id              = 0;
    m_sps.chroma_format_idc                 = mfx.FrameInfo.ChromaFormat;
    m_sps.separate_colour_plane_flag        = 0;
    m_sps.pic_width_in_luma_samples         = m_ext.HEVCParam.PicWidthInLumaSamples;
    m_sps.pic_height_in_luma_samples        = m_ext.HEVCParam.PicHeightInLumaSamples;
    m_sps.conformance_window_flag           = 0;
    m_sps.bit_depth_luma_minus8             = Max(0, (mfxI32)mfx.FrameInfo.BitDepthLuma - 8);
    m_sps.bit_depth_chroma_minus8           = Max(0, (mfxI32)mfx.FrameInfo.BitDepthChroma - 8);
    m_sps.log2_max_pic_order_cnt_lsb_minus4 = (mfxU8)Clip3(0, 12, (mfxI32)CeilLog2(slo.max_num_reorder_pics + slo.max_dec_pic_buffering_minus1 + 1) - 3);

    m_sps.log2_min_luma_coding_block_size_minus3   = 0; // SKL
    m_sps.log2_diff_max_min_luma_coding_block_size = CeilLog2(LCUSize) - 3 - m_sps.log2_min_luma_coding_block_size_minus3;
    m_sps.log2_min_transform_block_size_minus2     = 0;
    m_sps.log2_diff_max_min_transform_block_size   = 3;
    m_sps.max_transform_hierarchy_depth_inter      = 2;
    m_sps.max_transform_hierarchy_depth_intra      = 2;
    m_sps.scaling_list_enabled_flag                = 0;
    m_sps.amp_enabled_flag                         = 1;
    m_sps.sample_adaptive_offset_enabled_flag      = 0; // SKL
    m_sps.pcm_enabled_flag                         = 0; // SKL

    assert(0 == m_sps.pcm_enabled_flag);

    {
        mfxU32 MaxPocLsb = (1<<(m_sps.log2_max_pic_order_cnt_lsb_minus4+4));
        std::list<FakeTask> frames;
        std::list<FakeTask>::iterator cur;
        std::vector<STRPSFreq> sets;
        std::vector<STRPSFreq>::iterator it;
        DpbArray dpb = {};
        DpbFrame tmp = {};
        mfxU8 rpl[2][MAX_DPB_SIZE] = {};
        mfxU8 nRef[2] = {};
        STRPS rps;
        mfxI32 STDist = Min<mfxI32>(mfx.GopPicSize, 128);
        bool moreLTR = !!LTRInterval;

        Fill(dpb, IDX_INVALID);

        for (mfxU32 i = 0; (moreLTR || sets.size() != 64); i ++)
        {
            FakeTask new_frame = {i, GetFrameType(*this, i)};

            frames.push_back(new_frame);

            cur = Reorder(*this, dpb, frames.begin(), frames.end(), false);

            if (cur == frames.end())
                continue;

            if (   (i > 0 && (cur->m_frameType & MFX_FRAMETYPE_I))
                || (!moreLTR && cur->m_poc >= STDist))
                break;

            if (!(cur->m_frameType & MFX_FRAMETYPE_I) && cur->m_poc < STDist)
            {
                ConstructRPL(*this, dpb, !!(cur->m_frameType & MFX_FRAMETYPE_B), cur->m_poc, rpl, nRef);

                Zero(rps);
                ConstructSTRPS(dpb, rpl, nRef, cur->m_poc, rps);

                it = std::find_if(sets.begin(), sets.end(), EqSTRPS(rps));

                if (it == sets.end())
                    sets.push_back(STRPSFreq(rps, 1));
                else
                    it->N ++;
            }

            if (cur->m_frameType & MFX_FRAMETYPE_REF)
            {
                tmp.m_poc = cur->m_poc;
                UpdateDPB(*this, tmp, dpb);

                if ( moreLTR && isLTR(dpb, LTRInterval, cur->m_poc))
                {
                    if (   mfxU32(m_sps.log2_max_pic_order_cnt_lsb_minus4+5) <= CeilLog2(m_sps.num_long_term_ref_pics_sps))
                        moreLTR = false;
                    else if (cur->m_poc >= (mfxI32)MaxPocLsb)
                    {
                        for (mfxU32 j = 0; j < m_sps.num_long_term_ref_pics_sps; j ++)
                        {
                            if (m_sps.lt_ref_pic_poc_lsb_sps[j] == mfxU16(cur->m_poc & (MaxPocLsb - 1)))
                            {
                                moreLTR = false;
                                break;
                            }
                        }
                    }

                    if (moreLTR)
                    {
                        m_sps.lt_ref_pic_poc_lsb_sps[m_sps.num_long_term_ref_pics_sps] = mfxU16(cur->m_poc & (MaxPocLsb - 1));
                        m_sps.used_by_curr_pic_lt_sps_flag[m_sps.num_long_term_ref_pics_sps] = 1;
                        m_sps.num_long_term_ref_pics_sps ++;

                        if (m_sps.num_long_term_ref_pics_sps == 32)
                            moreLTR = false;
                    }
                }
            }

            frames.erase(cur);
        }

        std::sort(sets.begin(), sets.end(), Greater);
        assert(sets.size() <= 64);

        for (mfxU8 i = 0; i < sets.size(); i ++)
            OptimizeSTRPS(sets, (mfxU8)sets.size(), sets[i], i);

        ReduceSTRPS(sets, mfx.NumSlice);

        for (it = sets.begin(); it != sets.end(); it ++)
            m_sps.strps[m_sps.num_short_term_ref_pic_sets++] = *it;
        
        m_sps.long_term_ref_pics_present_flag = !!LTRInterval;
    }

    m_sps.temporal_mvp_enabled_flag             = 1; // SKL ?
    m_sps.strong_intra_smoothing_enabled_flag   = 0; // SKL

    m_sps.vui_parameters_present_flag = 1;

    m_sps.vui.aspect_ratio_info_present_flag = 1;
    if (m_sps.vui.aspect_ratio_info_present_flag)
    {
        m_sps.vui.aspect_ratio_idc = GetAspectRatioIdc(mfx.FrameInfo.AspectRatioW, mfx.FrameInfo.AspectRatioH);
        if (m_sps.vui.aspect_ratio_idc == 255)
        {
            m_sps.vui.sar_width  = mfx.FrameInfo.AspectRatioW;
            m_sps.vui.sar_height = mfx.FrameInfo.AspectRatioH;
        }
    }

    {
        mfxFrameInfo& fi = mfx.FrameInfo;
        mfxU16 SubWidthC[4] = {1,2,2,1};
        mfxU16 SubHeightC[4] = {1,2,1,1};
        mfxU16 cropUnitX = SubWidthC[m_sps.chroma_format_idc];
        mfxU16 cropUnitY = SubHeightC[m_sps.chroma_format_idc];

        m_sps.vui.def_disp_win_left_offset      = (fi.CropX / cropUnitX);
        m_sps.vui.def_disp_win_right_offset     = (m_sps.pic_width_in_luma_samples - fi.CropW - fi.CropX) / cropUnitX;
        m_sps.vui.def_disp_win_top_offset       = (fi.CropY / cropUnitY);
        m_sps.vui.def_disp_win_bottom_offset    = (m_sps.pic_height_in_luma_samples - fi.CropH - fi.CropY) / cropUnitY;
        m_sps.vui.default_display_window_flag   =    m_sps.vui.def_disp_win_left_offset
                                                  || m_sps.vui.def_disp_win_right_offset
                                                  || m_sps.vui.def_disp_win_top_offset
                                                  || m_sps.vui.def_disp_win_bottom_offset;
    }

    m_sps.vui.timing_info_present_flag = !!m_vps.timing_info_present_flag;
    if (m_sps.vui.timing_info_present_flag)
    {
        m_sps.vui.num_units_in_tick = m_vps.num_units_in_tick;
        m_sps.vui.time_scale        = m_vps.time_scale;
    }

    Zero(m_pps);
    m_pps.seq_parameter_set_id = m_sps.seq_parameter_set_id;

    m_pps.pic_parameter_set_id                  = 0;
    m_pps.dependent_slice_segments_enabled_flag = 0;
    m_pps.output_flag_present_flag              = 0;
    m_pps.num_extra_slice_header_bits           = 0;
    m_pps.sign_data_hiding_enabled_flag         = 0;
    m_pps.cabac_init_present_flag               = 0;
    m_pps.num_ref_idx_l0_default_active_minus1  = NumRefLX[0] - 1;
    m_pps.num_ref_idx_l1_default_active_minus1  = 0;
    m_pps.init_qp_minus26                       = 0;
    m_pps.constrained_intra_pred_flag           = 0;
    m_pps.transform_skip_enabled_flag           = 0;
    m_pps.cu_qp_delta_enabled_flag              = 0;

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        m_pps.init_qp_minus26 = (mfx.GopRefDist == 1 ? mfx.QPP : mfx.QPB) - 26;

    m_pps.cb_qp_offset                          = 0;
    m_pps.cr_qp_offset                          = 0;
    m_pps.slice_chroma_qp_offsets_present_flag  = 0;
    m_pps.weighted_pred_flag                    = 0;
    m_pps.weighted_bipred_flag                  = 0;
    m_pps.transquant_bypass_enabled_flag        = 0;
    m_pps.tiles_enabled_flag                    = 0;
    m_pps.entropy_coding_sync_enabled_flag      = 0;

    m_pps.loop_filter_across_slices_enabled_flag      = 0;
    m_pps.deblocking_filter_control_present_flag      = 0;
    m_pps.scaling_list_data_present_flag              = 0;
    m_pps.lists_modification_present_flag             = 1;
    m_pps.log2_parallel_merge_level_minus2            = 0;
    m_pps.slice_segment_header_extension_present_flag = 0;

}

mfxU16 FrameType2SliceType(mfxU32 ft)
{
    if (ft & MFX_FRAMETYPE_B)
        return 0;
    if (ft & MFX_FRAMETYPE_P)
        return 1;
    return 2;
}

bool isCurrRef(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc)
{
    for (mfxU32 i = 0; i < 2; i ++)
        for (mfxU32 j = 0; j < numRefActive[i]; j ++)
            if (poc == DPB[RPL[i][j]].m_poc)
                return true;
    return false;
}

inline bool isCurrRef(Task const & task, mfxI32 poc)
{
    return isCurrRef(task.m_dpb[0], task.m_refPicList, task.m_numRefActive, poc);
}

void ConstructSTRPS(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc,
    STRPS& rps)
{
    mfxU32 i, nRef;

    for (i = 0, nRef = 0; !isDpbEnd(DPB, i); i ++)
    {
        if (DPB[i].m_ltr)
            continue;

        rps.pic[nRef].DeltaPocSX = (mfxI16)(DPB[i].m_poc - poc);
        rps.pic[nRef].used_by_curr_pic_sx_flag = isCurrRef(DPB, RPL, numRefActive, DPB[i].m_poc);

        rps.num_negative_pics += rps.pic[nRef].DeltaPocSX < 0;
        rps.num_positive_pics += rps.pic[nRef].DeltaPocSX > 0;
        nRef ++;
    }

    MFX_SORT_STRUCT(rps.pic, nRef, DeltaPocSX, >);
    MFX_SORT_STRUCT(rps.pic, rps.num_negative_pics, DeltaPocSX, <);

    for (i = 0; i < nRef; i ++)
    {
        mfxI16 prev  = (!i || i == rps.num_negative_pics) ? 0 : rps.pic[i-1].DeltaPocSX;
        rps.pic[i].delta_poc_sx_minus1 = Abs(rps.pic[i].DeltaPocSX - prev) - 1;
    }
}

bool Equal(STRPS const & l, STRPS const & r)
{
    //ignore inter_ref_pic_set_prediction_flag, check only DeltaPocSX

    if (   l.num_negative_pics != r.num_negative_pics
        || l.num_positive_pics != r.num_positive_pics)
        return false;

    for(mfxU8 i = 0; i < l.num_negative_pics + l.num_positive_pics; i ++)
        if (   l.pic[i].DeltaPocSX != r.pic[i].DeltaPocSX
            || l.pic[i].used_by_curr_pic_sx_flag != r.pic[i].used_by_curr_pic_sx_flag)
            return false;

    return true;
}

void MfxVideoParam::GetSliceHeader(Task const & task, Slice & s) const
{
    bool  isP   = !!(task.m_frameType & MFX_FRAMETYPE_P);
    bool  isB   = !!(task.m_frameType & MFX_FRAMETYPE_B);

    Zero(s);
    
    s.first_slice_segment_in_pic_flag = 1;
    s.no_output_of_prior_pics_flag    = 0;
    s.pic_parameter_set_id            = m_pps.pic_parameter_set_id;

    if (!s.first_slice_segment_in_pic_flag)
    {
        if (m_pps.dependent_slice_segments_enabled_flag)
        {
            s.dependent_slice_segment_flag = 0;
        }

        s.segment_address = 0;
    }

    s.type = FrameType2SliceType(task.m_frameType);

    if (m_pps.output_flag_present_flag)
        s.pic_output_flag = 1;

    assert(0 == m_sps.separate_colour_plane_flag);

    if (task.m_shNUT != IDR_W_RADL && task.m_shNUT != IDR_N_LP)
    {
        mfxU32 i, j;
        mfxU16 nLTR = 0, nSTR[2] = {}, nDPBST = 0, nDPBLT = 0;
        mfxI32 STR[2][MAX_DPB_SIZE] = {};                           // used short-term references
        mfxI32 LTR[MAX_NUM_LONG_TERM_PICS] = {};                    // used long-term references
        DpbArray const & DPB = task.m_dpb[0];                       // DPB before encoding
        mfxU8 const (&RPL)[2][MAX_DPB_SIZE] = task.m_refPicList;    // Ref. Pic. List

        ConstructSTRPS(DPB, RPL, task.m_numRefActive, task.m_poc, s.strps);

        s.pic_order_cnt_lsb = (task.m_poc & ~(0xFFFFFFFF << (m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4)));

        // count STRs and LTRs in DPB (including unused)
        for (i = 0; !isDpbEnd(DPB, i); i ++)
        {
            nDPBST += !DPB[i].m_ltr;
            nDPBLT += DPB[i].m_ltr;
        }

        //check for suitable ST RPS in SPS
        for (i = 0; i < m_sps.num_short_term_ref_pic_sets; i ++)
        {
            if (Equal(m_sps.strps[i], s.strps))
            {
                //use ST RPS from SPS
                s.short_term_ref_pic_set_sps_flag = 1;
                s.short_term_ref_pic_set_idx      = i;
                break;
            }
        }

        if (!s.short_term_ref_pic_set_sps_flag)
            OptimizeSTRPS(m_sps.strps, m_sps.num_short_term_ref_pic_sets, s.strps, m_sps.num_short_term_ref_pic_sets);

        for (i = 0; i < mfxU32(s.strps.num_negative_pics + s.strps.num_positive_pics); i ++)
        {
            bool isAfter = (s.strps.pic[i].DeltaPocSX > 0);
            if (s.strps.pic[i].used_by_curr_pic_sx_flag)
                STR[isAfter][nSTR[isAfter]++] = task.m_poc + s.strps.pic[i].DeltaPocSX;
        }

        if (nDPBLT)
        {
            assert(m_sps.long_term_ref_pics_present_flag);

            mfxU32 MaxPocLsb  = (1<<(m_sps.log2_max_pic_order_cnt_lsb_minus4+4));
            mfxU32 DeltaPocMsbCycleLt = 0;
            mfxI32 DPBLT[MAX_DPB_SIZE] = {};

            for (i = 0, j = 0; !isDpbEnd(DPB, i); i ++)
                if (DPB[i].m_ltr)
                    DPBLT[j++] = DPB[i].m_poc;

            assert(j == nDPBLT);

            MFX_SORT(DPBLT, nDPBLT, >);

            for (nLTR = 0, j = 0; j < nDPBLT; j ++)
            {
                bool found = false;

                for (i = 0; i < m_sps.num_long_term_ref_pics_sps; i ++)
                {
                    if (   mfxU16(DPBLT[j] & (MaxPocLsb - 1)) == m_sps.lt_ref_pic_poc_lsb_sps[i]
                        && isCurrRef(task, DPBLT[j]) == !!m_sps.used_by_curr_pic_lt_sps_flag[i])
                    {
                        Slice::LongTerm & curlt = s.lt[s.num_long_term_sps];
                        curlt.lt_idx_sps = i;
                        curlt.poc_lsb_lt = m_sps.lt_ref_pic_poc_lsb_sps[i];
                        curlt.used_by_curr_pic_lt_flag = !!m_sps.used_by_curr_pic_lt_sps_flag[i];
                        curlt.delta_poc_msb_cycle_lt = 
                            (task.m_poc - s.pic_order_cnt_lsb - (DPBLT[j] - curlt.poc_lsb_lt)) 
                            / MaxPocLsb - DeltaPocMsbCycleLt;
                        curlt.delta_poc_msb_present_flag = !!curlt.delta_poc_msb_cycle_lt;
                        DeltaPocMsbCycleLt += curlt.delta_poc_msb_cycle_lt;
                        s.num_long_term_sps ++;
                        
                        if (curlt.used_by_curr_pic_lt_flag)
                            LTR[nLTR++] = DPBLT[j];

                        found = true;
                        break;
                    }
                }

                if (!found)
                    break;
            }

            for (; j < nDPBLT; j ++)
            {
                Slice::LongTerm & curlt = s.lt[s.num_long_term_sps + s.num_long_term_pics];

                curlt.used_by_curr_pic_lt_flag = isCurrRef(task, DPBLT[j]);
                curlt.poc_lsb_lt = (DPBLT[j] & (MaxPocLsb - 1));
                curlt.delta_poc_msb_cycle_lt = 
                    (task.m_poc - s.pic_order_cnt_lsb - (DPBLT[j] - curlt.poc_lsb_lt)) 
                    / MaxPocLsb - DeltaPocMsbCycleLt;
                curlt.delta_poc_msb_present_flag = !!curlt.delta_poc_msb_cycle_lt;
                DeltaPocMsbCycleLt += curlt.delta_poc_msb_cycle_lt;
                s.num_long_term_pics ++;

                if (curlt.used_by_curr_pic_lt_flag)
                    LTR[nLTR++] = DPBLT[j];
            }
        }

        if (m_sps.temporal_mvp_enabled_flag)
            s.temporal_mvp_enabled_flag = 1;

        if (m_pps.lists_modification_present_flag)
        {
            mfxU16 rIdx = 0;
            mfxI32 RPLTempX[2][16] = {}; // default ref. list without modifications
            mfxU16 NumRpsCurrTempListX[2] = 
            {
                Max<mfxU16>(nSTR[0] + nSTR[1] + nLTR, task.m_numRefActive[0]),
                Max<mfxU16>(nSTR[0] + nSTR[1] + nLTR, task.m_numRefActive[1])
            };

            for (j = 0; j < 2; j ++)
            {
                rIdx = 0;
                while (rIdx < NumRpsCurrTempListX[j])
                {
                    for (i = 0; i < nSTR[j] && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = STR[j][i];
                    for (i = 0; i < nSTR[!j] && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = STR[!j][i];
                    for (i = 0; i < nLTR && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = LTR[i];
                }

                for (rIdx = 0; rIdx < task.m_numRefActive[j]; rIdx ++)
                {
                    for (i = 0; i < NumRpsCurrTempListX[j] && DPB[RPL[j][rIdx]].m_poc != RPLTempX[j][i]; i ++);
                    s.list_entry_lx[j][rIdx] = (mfxU8)i;
                    s.ref_pic_list_modification_flag_lx[j] |= (i != rIdx);
                }
            }
        }
    }

    if (m_sps.sample_adaptive_offset_enabled_flag)
    {
        s.sao_luma_flag   = 0;
        s.sao_chroma_flag = 0;
    }

    if (isP || isB)
    {
        s.num_ref_idx_active_override_flag = 
           (          m_pps.num_ref_idx_l0_default_active_minus1 + 1 != task.m_numRefActive[0]
            || isB && m_pps.num_ref_idx_l1_default_active_minus1 + 1 != task.m_numRefActive[1]);

        s.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

        if (isB)
            s.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;

        if (isB)
            s.mvd_l1_zero_flag = 0;

        if (m_pps.cabac_init_present_flag)
            s.cabac_init_flag = 0;

        if (s.temporal_mvp_enabled_flag)
            s.collocated_from_l0_flag = 1;

        assert(0 == m_pps.weighted_pred_flag);
        assert(0 == m_pps.weighted_bipred_flag);

        s.five_minus_max_num_merge_cand = 0;
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        s.slice_qp_delta = task.m_qpY - (m_pps.init_qp_minus26 + 26);

    if (m_pps.slice_chroma_qp_offsets_present_flag)
    {
        s.slice_cb_qp_offset = 0;
        s.slice_cr_qp_offset = 0;
    }

    if (m_pps.deblocking_filter_override_enabled_flag)
        s.deblocking_filter_override_flag = 0;

    assert(0 == s.deblocking_filter_override_flag);

    s.loop_filter_across_slices_enabled_flag = 0;

    if (m_pps.tiles_enabled_flag || m_pps.entropy_coding_sync_enabled_flag)
    {
        s.num_entry_point_offsets = 0;

        assert(0 == s.num_entry_point_offsets);
    }
}

void MfxVideoParam::SyncHeadersToMfxParam()
{
    assert(!"not implemented");
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

Task* TaskManager::Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    bool flush)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    TaskList::iterator begin = m_reordering.begin();
    TaskList::iterator end = m_reordering.end();
    TaskList::iterator top = MfxHwH265Encode::Reorder(par, dpb, begin, end, flush);

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

    if (video.BRef && (frameOrder % gopPicSize % gopRefDist - 1) % 2 == 1)
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);

    return (MFX_FRAMETYPE_B);
}

template<class T, class A> void Insert(A& _to, mfxU32 _where, T const & _what)
{
    memmove(&_to[_where + 1], &_to[_where], sizeof(_to) - (_where + 1) * sizeof(_to[0]));
    _to[_where] = _what;
}

template<class A> void Remove(A& _from, mfxU32 _where, mfxU32 _num = 1)
{
    memmove(&_from[_where], &_from[_where + _num], sizeof(_from) - (_where + _num) * sizeof(_from[0]));
    memset(&_from[sizeof(_from)/sizeof(_from[0]) - _num], IDX_INVALID, sizeof(_from[0]) * _num);
}

void UpdateDPB(
    MfxVideoParam const & par,
    DpbFrame const & task,
    DpbArray & dpb)
{
    mfxU16 end = 0; // DPB end
    mfxU16 st0 = 0; // first ST ref in DPB

    while (!isDpbEnd(dpb, end)) end ++;

    // frames stored in DPB in POC ascending order,
    // LTRs before STRs (use LTR-candidate as STR as long as it possible)
    if (par.BRef)
        MFX_SORT_STRUCT(dpb, end, m_poc, >);

    // sliding window over STRs
    if (end && end == par.mfx.NumRefFrame)
    {
        for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0 ++);

        if (par.LTRInterval)
        {
            // mark/replace LTR in DPB
            if (!st0)
            {
                dpb[st0].m_ltr = true;
                st0 ++;
            }
            else if (dpb[st0].m_poc - dpb[0].m_poc >= (mfxI32)par.LTRInterval)
            {
                dpb[st0].m_ltr = true;
                st0 = 0;
            }
        }

        Remove(dpb, st0 == end ? 0 : st0);
        end --;
    }

    dpb[end++] = task;
}

bool isLTR(
    DpbArray const & dpb,
    mfxU32 LTRInterval,
    mfxI32 poc)
{
    mfxI32 LTRCandidate = dpb[0].m_poc;

    for (mfxU16 i = 1; !isDpbEnd(dpb, i); i ++)
        if (   dpb[i].m_poc > LTRCandidate
            && dpb[i].m_poc - LTRCandidate >= (mfxI32)LTRInterval)
            LTRCandidate = dpb[i].m_poc;

    return (poc == LTRCandidate) || (LTRCandidate == 0 && poc >= (mfxI32)LTRInterval);
}

void ConstructRPL(
    MfxVideoParam const & par,
    DpbArray const & DPB,
    bool isB,
    mfxI32 poc,
    mfxU8 (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 (&numRefActive)[2])
{
    mfxU8& l0 = numRefActive[0];
    mfxU8& l1 = numRefActive[1];
    mfxU8 ltr = IDX_INVALID;
    mfxU8 NumStRefL0 = (mfxU8)(par.NumRefLX[0]);

    l0 = l1 = 0;

    for (mfxU8 i = 0; !isDpbEnd(DPB, i); i ++)
    {
        if (poc > DPB[i].m_poc)
        {
            if (DPB[i].m_ltr && ltr == IDX_INVALID)
                ltr = i;
            else
                RPL[0][l0++] = i;
        }
        else if (isB)
            RPL[1][l1++] = i;
    }

    NumStRefL0 -= (ltr != IDX_INVALID);

    if (l0 > NumStRefL0)
    {
        // remove the oldest reference unless it is LTR candidate
        Remove(RPL[0], (par.LTRInterval && ltr == IDX_INVALID && l0 > 1), l0 - NumStRefL0);
        l0 = NumStRefL0;
    }
    if (l1 > par.NumRefLX[1])
    {
        Remove(RPL[1], 0, l1 - par.NumRefLX[1]);
        l1 = (mfxU8)par.NumRefLX[1];
    }

    // reorder STRs to POC descending order
    for (mfxU8 lx = 0; lx < 2; lx ++)
        MFX_SORT_COMMON(RPL[lx], numRefActive[lx],
            DPB[RPL[lx][_i]].m_poc < DPB[RPL[lx][_j]].m_poc);

    if (ltr != IDX_INVALID)
    {
        // use LTR as 2nd reference
        Insert(RPL[0], !!l0, ltr);
        l0 ++;
    }

    assert(l0 > 0);

    if (isB && !l1)
        RPL[1][l1++] = RPL[0][l0-1];

    if (!isB)
    {
        for (mfxI16 i = l0 - 1; i >= 0 && l1 < par.NumRefLX[1]; i --)
            RPL[1][l1++] = RPL[0][i];
    }
}

mfxU8 GetCodingType(Task const & task)
{
    mfxU8 t = CODING_TYPE_B;

    assert(task.m_frameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));

    if (task.m_frameType & MFX_FRAMETYPE_I)
        return CODING_TYPE_I;

    if (task.m_frameType & MFX_FRAMETYPE_P)
        return CODING_TYPE_P;
    
    for (mfxU8 i = 0; i < 2; i ++)
    {
        for (mfxU32 j = 0; j < task.m_numRefActive[i]; j ++)
        {
            if (task.m_dpb[0][task.m_refPicList[i][j]].m_codingType > CODING_TYPE_B)
                return CODING_TYPE_B2;

            if (task.m_dpb[0][task.m_refPicList[i][j]].m_codingType == CODING_TYPE_B)
                t = CODING_TYPE_B1;
        }
    }

    return t;
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
    const bool isIDR  = !!(task.m_frameType & MFX_FRAMETYPE_IDR);

    // set coding type and QP
    if (isB)
    {
        task.m_qpY = (mfxU8)par.mfx.QPB;
    }
    else if (isP)
    {
        // encode P as GPB
        task.m_qpY = (mfxU8)par.mfx.QPP;
        task.m_frameType &= ~MFX_FRAMETYPE_P;
        task.m_frameType |= MFX_FRAMETYPE_B;
    }
    else
    {
        assert(task.m_frameType & MFX_FRAMETYPE_I);
        task.m_qpY = (mfxU8)par.mfx.QPI;
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        task.m_qpY = 0;
    else if (task.m_ctrl.QP)
        task.m_qpY = (mfxU8)task.m_ctrl.QP;

    Copy(task.m_dpb[0], prevTask.m_dpb[1]);

    //construct ref lists
    Zero(task.m_numRefActive);
    Fill(task.m_refPicList, IDX_INVALID);

    if (!isI)
        ConstructRPL(par, task.m_dpb[0], isB, task.m_poc, task.m_refPicList, task.m_numRefActive);

    task.m_codingType = GetCodingType(task);

    // update dpb
    if (isI)
        Fill(task.m_dpb[1], IDX_INVALID);
    else
        Copy(task.m_dpb[1], task.m_dpb[0]);

    if (isRef)
    {
        UpdateDPB(par, task, task.m_dpb[1]);

        // is current frame will be used as LTR
        task.m_ltr = isLTR(task.m_dpb[1], par.LTRInterval, task.m_poc);
    }

    task.m_shNUT = mfxU8(isIDR ? IDR_W_RADL 
        : task.m_poc >= 0 ? (isRef ? TRAIL_R : TRAIL_N)
        : (isRef ? RADL_R : RADL_N));

    par.GetSliceHeader(task, task.m_sh);
}

}; //namespace MfxHwH265Encode
