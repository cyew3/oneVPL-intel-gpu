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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "hevcehw_g12_scc.h"
#include "va/va.h"

namespace HEVCEHW
{  
namespace Linux
{
namespace Gen12
{
class SCC
    : public HEVCEHW::Gen12::SCC
{
public:
    SCC(mfxU32 FeatureId)
        : HEVCEHW::Gen12::SCC(FeatureId)
    {}
protected:
    virtual void SetExtraGUIDs(StorageRW& strg) override
    {
        auto& g2va = HEVCEHW::Gen12::Glob::GuidToVa::GetOrConstruct(strg);
        g2va[DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main]       = { VAProfileHEVCSccMain,    VAEntrypointEncSliceLP };
        g2va[DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10]     = { VAProfileHEVCSccMain10,  VAEntrypointEncSliceLP };
        g2va[DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444]    = { VAProfileHEVCSccMain444, VAEntrypointEncSliceLP };
        g2va[DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10] = { mfxU32(VAProfileNone),   VAEntrypointEncSliceLP }; //no VA support
    };
};
} //Gen12
} //Linux
} //namespace HEVCEHW

#endif
