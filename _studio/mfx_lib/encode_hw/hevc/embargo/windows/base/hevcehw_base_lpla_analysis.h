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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_LP_LOOKAHEAD)

#include "hevcehw_base.h"
#include "mfx_lp_lookahead.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Base
{
class LpLookAheadAnalysis
    : public FeatureBase
{
public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(SetCallChains)
#define DECL_FEATURE_NAME "Base_LpLookAheadAnalysis"
#include "hevcehw_decl_blocks.h"

    LpLookAheadAnalysis(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
    {}
protected:
    virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
    virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
    virtual void SetSupported(ParamSupport& par) override;
    virtual void SetInherited(ParamInheritance& par) override;
    bool m_lplaEnabled = false;
};
} //Base
} //Windows
} //namespace HEVCEHW

#endif