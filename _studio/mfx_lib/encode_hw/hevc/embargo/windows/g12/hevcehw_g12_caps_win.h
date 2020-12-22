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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base.h"
#include "hevcehw_g12_data.h"
#include "hevcehw_g12_caps.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12
{

    using namespace HEVCEHW::Gen12;

    class Caps
        : public HEVCEHW::Gen12::Caps
    {
    public:
        Caps(mfxU32 FeatureId)
            : HEVCEHW::Gen12::Caps(FeatureId)
        {}

    protected:
        virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override
        {
            HEVCEHW::Gen12::Caps::Query1WithCaps(blocks, Push);

            Push(BLK_HardcodeCapsExt
                , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg)->mfxStatus
            {
                auto& caps = Glob::EncodeCaps::Get(strg);
                caps.SliceIPOnly = IsOn(par.mfx.LowPower) && (par.mfx.TargetUsage == 6 || par.mfx.TargetUsage == 7 || par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC);

                return MFX_ERR_NONE;
            });
        }
    };

} //Gen12
} //Windows
} //namespace HEVCEHW

#endif
