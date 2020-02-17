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

#pragma once
#include "ehw_resources_pool.h"
#include <set>

namespace MfxEncodeHW
{
class ResPoolDX11
    : public ResPool
{
public:
    ResPoolDX11(VideoCORE& core)
        : ResPool(core)
    {}

    ~ResPoolDX11()
    {
        Free();
    }

    virtual mfxStatus Alloc(
        const mfxFrameAllocRequest & req
        , bool bCopyRequired) override;

    virtual void Free() override;

    std::set<mfxU32> m_noEncTargetWA = std::set<mfxU32>({
        MFX_FOURCC_YUY2
        , MFX_FOURCC_P210
        , MFX_FOURCC_AYUV
        , MFX_FOURCC_Y210
        , MFX_FOURCC_Y410 });
};

} // namespace MfxEncodeHW