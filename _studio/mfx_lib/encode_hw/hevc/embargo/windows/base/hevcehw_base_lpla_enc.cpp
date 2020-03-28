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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_LP_LOOKAHEAD)

#include "hevcehw_base_lpla_enc.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows::Base;

void LpLookAheadEnc::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par = HEVCEHW::Base::Glob::VideoParam::Get(global);
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        MFX_CHECK(pCO2, MFX_ERR_NONE);
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);
        auto& caps = HEVCEHW::Base::Glob::EncodeCaps::Get(global);
        auto& lpla = HEVCEHW::Base::Glob::LpLookAhead::Get(global);
        //for game streaming scenario, if option enable lowpower lookahead, check encoder's capability
        if (pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && pCO2->LookAheadDepth > 0 &&
            (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR))
        {
            lpla.bIsLpLookaheadSupported = true;
        }

        if (lpla.bIsLpLookaheadSupported)
        {
            MFX_CHECK(caps.LookaheadBRCSupport, MFX_ERR_INVALID_VIDEO_PARAM);
        }

        MFX_CHECK(!lpla.bAnalysis && !bInitialized && lpla.bIsLpLookaheadSupported, MFX_ERR_NONE);

        //create and initialize lowpower lookahead module
        auto& core = HEVCEHW::Base::Glob::VideoCore::Get(global);
        lpla.pLpLookAhead.reset(new MfxLpLookAhead(&core));

        // create ext buffer to set the lookahead data buffer
        lplaParam = par;
        extBufLPLA.Header.BufferId = MFX_EXTBUFF_LP_LOOKAHEAD;
        extBufLPLA.Header.BufferSz = sizeof(extBufLPLA);
        extBufLPLA.LookAheadDepth = pCO2->LookAheadDepth;
        extBufLPLA.InitialDelayInKB = par.mfx.InitialDelayInKB;
        extBufLPLA.BufferSizeInKB = par.mfx.BufferSizeInKB;
        extBufLPLA.TargetKbps = par.mfx.TargetKbps;

        extBuffers[0] = &extBufLPLA.Header;
        lplaParam.NumExtParam = 1;
        lplaParam.ExtParam = &extBuffers[0];

        mfxStatus sts= lpla.pLpLookAhead->Init(&lplaParam);
        if (sts == MFX_ERR_NONE)
        {
            lpla.bIsLpLookaheadEnabled = true;
        }
        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
