// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_g11_alloc.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;

void Allocator::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [this](StorageRW&, StorageRW& local) -> mfxStatus
    {
        local.Insert(
            Tmp::MakeAlloc::Key
            , std::move(make_unique<MakeStorable<Tmp::MakeAlloc::TRef>>(
                [](VideoCORE& core) { return new MfxFrameAllocResponse(core); })));
        return MFX_ERR_NONE;
    });
}

MfxFrameAllocResponse::MfxFrameAllocResponse(VideoCORE& core)
    : mfxFrameAllocResponse({})
    , m_core(core)
{
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();
}

Resource MfxFrameAllocResponse::Acquire()
{
    Resource res;
    res.Idx = mfxU8(std::find(m_locked.begin(), m_locked.end(), 0u) - m_locked.begin());

    if (res.Idx >= NumFrameActual)
    {
        res.Idx = IDX_INVALID;
        return res;
    }

    Lock(res.Idx);
    ClearFlag(res.Idx);
    res.Mid = mids[res.Idx];

    return res;
}

void MfxFrameAllocResponse::Free()
{
    bool bRspQueue = m_core.GetVAType() == MFX_HW_D3D11 && !m_isOpaque;

    if (bRspQueue)
    {
        std::for_each(m_responseQueue.begin(), m_responseQueue.end()
            , [&](mfxFrameAllocResponse& rsp) { m_core.FreeFrames(&rsp); });
        m_responseQueue.clear();
        return;
    }

    if (mids)
    {
        NumFrameActual = m_numFrameActualReturnedByAllocFrames;
        m_core.FreeFrames(this);
        mids = 0;
    }
}

mfxStatus MfxFrameAllocResponse::Alloc(
    mfxFrameAllocRequest & req,
    bool                   isCopyRequired)
{
    mfxFrameAllocResponse & response = *this;

    req.NumFrameSuggested = req.NumFrameMin;

    if (m_core.GetVAType() == MFX_HW_D3D11)
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        // WA for RExt formats to fix CreateTexture2D failure
        bool bNoEncTarget =
               tmp.Info.FourCC == MFX_FOURCC_YUY2
            || tmp.Info.FourCC == MFX_FOURCC_P210
            || tmp.Info.FourCC == MFX_FOURCC_AYUV
            || tmp.Info.FourCC == MFX_FOURCC_Y210
            || tmp.Info.FourCC == MFX_FOURCC_Y410;

        tmp.Type &= ~(MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET * bNoEncTarget);

        mfxStatus sts = Catch(MFX_ERR_NONE,
            [&]()
        {
            std::transform(m_responseQueue.begin(), m_responseQueue.end(), m_mids.begin()
                , [&](mfxFrameAllocResponse& rsp)
            {
                mfxStatus asts = m_core.AllocFrames(&tmp, &rsp, isCopyRequired);
                ThrowIf(!!asts, asts);
                return rsp.mids[0];
            });
        });
        MFX_CHECK_STS(sts);

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = m_core.AllocFrames(&req, &response);
        MFX_CHECK_STS(sts);
    }

    MFX_CHECK(NumFrameActual >= req.NumFrameMin, MFX_ERR_MEMORY_ALLOC);

    m_locked.resize(req.NumFrameMin, 0);
    std::fill(m_locked.begin(), m_locked.end(), 0);

    m_flag.resize(req.NumFrameMin, 0);
    std::fill(m_flag.begin(), m_flag.end(), 0);

    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;
    m_isExternal = false;
    m_isOpaque = false;

    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::Alloc(
    const mfxFrameInfo & info
    , const decltype(mfxExtOpaqueSurfaceAlloc::In)& opq)
{
    mfxFrameAllocRequest req = {};
    req.Info = info;
    req.NumFrameMin = req.NumFrameSuggested = opq.NumSurface;
    req.Type = opq.Type;

    mfxStatus sts = m_core.AllocFrames(&req, this, opq.Surfaces, opq.NumSurface);
    MFX_CHECK_STS(sts);
    MFX_CHECK(NumFrameActual >= opq.NumSurface, MFX_ERR_MEMORY_ALLOC);

    m_info = info;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_isOpaque = true;

    return sts;
}

mfxU32 MfxFrameAllocResponse::Lock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
}

void MfxFrameAllocResponse::ClearFlag(mfxU32 idx)
{
   assert (idx < m_flag.size());
   if (idx < m_flag.size())
   {
        m_flag[idx] = 0;
   }
}
void MfxFrameAllocResponse::SetFlag(mfxU32 idx, mfxU32 flag)
{
   assert (idx < m_flag.size());
   if (idx < m_flag.size())
   {
        m_flag[idx] |= flag;
   }
}
mfxU32 MfxFrameAllocResponse::GetFlag (mfxU32 idx)
{
   assert (idx < m_flag.size());
   if (idx < m_flag.size())
   {
        return m_flag[idx];
   }
   return 0;
}

void MfxFrameAllocResponse::Unlock()
{
    std::fill(m_locked.begin(), m_locked.end(), 0);
    std::fill(m_flag.begin(), m_flag.end(), 0);
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


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)