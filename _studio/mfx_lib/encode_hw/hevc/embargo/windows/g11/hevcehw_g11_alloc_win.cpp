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

#include "hevcehw_g11_alloc_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Windows::Gen11;

void Allocator::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [](StorageRW&, StorageRW& local) -> mfxStatus
    {
        using MakeAlloc = HEVCEHW::Gen11::Tmp::MakeAlloc;
        auto CreateAllocator = [](VideoCORE& core) -> HEVCEHW::Gen11::IAllocation*
        {
            return new MfxFrameAllocResponse(core);
        };

        local.Insert(MakeAlloc::Key, new MakeAlloc::TRef(CreateAllocator));

        return MFX_ERR_NONE;
    });
}

mfxStatus MfxFrameAllocResponse::Alloc(
    mfxFrameAllocRequest & req,
    bool                   isCopyRequired)
{
    if (m_core.GetVAType() != MFX_HW_D3D11)
    {
        return HEVCEHW::Gen11::MfxFrameAllocResponse::Alloc(req, isCopyRequired);
    }

    req.NumFrameSuggested = req.NumFrameMin;

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

void MfxFrameAllocResponse::Free()
{
    bool bRspQueue = m_core.GetVAType() == MFX_HW_D3D11 && !m_isOpaque;

    if (!bRspQueue)
    {
        HEVCEHW::Gen11::MfxFrameAllocResponse::Free();
        return;
    }

    std::for_each(m_responseQueue.begin(), m_responseQueue.end()
        , [&](mfxFrameAllocResponse& rsp) { m_core.FreeFrames(&rsp); });
    m_responseQueue.clear();

    mids = nullptr;
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
