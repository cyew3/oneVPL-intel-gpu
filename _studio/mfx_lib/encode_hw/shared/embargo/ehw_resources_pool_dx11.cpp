// Copyright (c) 2020 Intel Corporation
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

#include "ehw_resources_pool_dx11.h"
#include <algorithm>

namespace MfxEncodeHW
{

mfxStatus ResPoolDX11::Alloc(
    const mfxFrameAllocRequest & request
    , bool bCopyRequired)
{
    auto req = request;
    req.NumFrameSuggested = req.NumFrameMin;

    mfxFrameAllocRequest tmp = req;
    tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

    m_responseQueue.resize(req.NumFrameMin);
    m_mids.resize(req.NumFrameMin);

    // WA for RExt formats to fix CreateTexture2D failure
    bool bNoEncTarget = !!m_noEncTargetWA.count(tmp.Info.FourCC);

    tmp.Type &= ~(MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET * bNoEncTarget);

    bool bAllocFailed = std::any_of(m_responseQueue.begin(), m_responseQueue.end()
        , [&](mfxFrameAllocResponse& rsp) { return MFX_ERR_NONE != m_core.AllocFrames(&tmp, &rsp, bCopyRequired); });
    MFX_CHECK(!bAllocFailed, MFX_ERR_MEMORY_ALLOC);

    std::transform(m_responseQueue.begin(), m_responseQueue.end(), m_mids.begin()
        , [&](mfxFrameAllocResponse& rsp) { return rsp.mids[0]; });

    m_locked.resize(req.NumFrameMin, 0);
    std::fill(m_locked.begin(), m_locked.end(), 0);

    m_flag.resize(req.NumFrameMin, 0);
    std::fill(m_flag.begin(), m_flag.end(), 0);

    m_response.mids           = &m_mids[0];
    m_response.NumFrameActual = req.NumFrameMin;
    m_numFrameActual          = m_response.NumFrameActual;
    m_info                    = req.Info;
    m_bExternal               = false;
    m_bOpaque                 = false;

    return MFX_ERR_NONE;
}

void ResPoolDX11::Free()
{
    std::for_each(m_responseQueue.begin(), m_responseQueue.end()
        , [&](mfxFrameAllocResponse& rsp) { m_core.FreeFrames(&rsp); });
    m_responseQueue.clear();

    m_response.mids = nullptr;
}


}//namespace MfxEncodeHW