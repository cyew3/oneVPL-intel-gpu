// Copyright (c) 2020-2021 Intel Corporation
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
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Base
{
class LpLookAheadEnc
    : public FeatureBase
{
public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(CheckLPLA)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(SetCallChains)\
    DECL_BLOCK(AddTask)\
    DECL_BLOCK(UpdateTask)\
    DECL_BLOCK(Close)

#define DECL_FEATURE_NAME "Base_LpLookAheadEnc"
#include "hevcehw_decl_blocks.h"

    LpLookAheadEnc(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
    {}
protected:
    virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
    virtual void Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push) override;
    virtual void Close(const FeatureBlocks& /*blocks*/, TPushCLS Push) override;

private:
    mfxExtBuffer*                    extBuffers[1];
    mfxVideoParam                    lplaParam               = {};
    mfxExtLplaParam                  extBufLPLA              = {};
    bool                             bInitialized            = false;
    bool                             bEncRun                 = false;
    bool                             bIsLpLookAheadSupported = false;
    bool                             bAnalysis               = false; // Hint for LookAhead Pass
    bool                             bIsLpLookaheadEnabled   = false; // Whether LPLA is enabled
    bool                             bIsLPLAEncToolsEnabled  = false; // Whether LPLA (GS) EncTools are enabled
    mfxU16                           LADepth                 = 0;
    mfxU16                           S_LA_SUBMIT             = 0;
    mfxU16                           S_LA_QUERY              = 0;
#if defined(MFX_ENABLE_LP_LOOKAHEAD)
    std::unique_ptr<MfxLpLookAhead>  pLpLookAhead;
#endif
    std::list<mfxLplastatus>         LpLaStatus;
};
} //Base
} //Windows
} //namespace HEVCEHW

#endif // defined(MFX_ENABLE_ENCTOOLS_LPLA)
#endif // defined(MFX_ENABLE_H265_VIDEO_ENCODE)

