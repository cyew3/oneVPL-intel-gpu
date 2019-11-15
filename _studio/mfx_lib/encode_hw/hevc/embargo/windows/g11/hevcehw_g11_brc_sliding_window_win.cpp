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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "hevcehw_g11_brc_sliding_window_win.h"
#include "hevcehw_g11_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;
using namespace HEVCEHW::Windows::Gen11;

void BRCSlidingWindow::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetDefaultsCallChain
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& /*par*/, StorageW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::Get(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.CheckWinBRC.Push(
            [](Defaults::TCheckAndFix::TExt /*prev*/
            , const Defaults::Param& dpar
            , mfxVideoParam& par)
        {
            mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
            MFX_CHECK(pCO3 && (pCO3->WinBRCSize || pCO3->WinBRCMaxAvgKbps), MFX_ERR_NONE);

            mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
            bool   bExtBRC      = pCO2 && IsOn(pCO2->ExtBRC);
            bool   bVBR         = (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR);
            bool   bUnsupported = bVBR && bExtBRC && pCO3->WinBRCMaxAvgKbps && (pCO3->WinBRCMaxAvgKbps < TargetKbps(par.mfx));
            mfxU32 changed      = !bVBR || bUnsupported;

            pCO3->WinBRCSize       *= !changed;
            pCO3->WinBRCMaxAvgKbps *= !changed;

            MFX_CHECK(!bUnsupported, MFX_ERR_UNSUPPORTED);
            MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            MFX_CHECK(bVBR && !bExtBRC, MFX_ERR_NONE);

            auto   fr      = dpar.base.GetFrameRate(dpar);
            mfxU16 fps     = mfxU16(CeilDiv(std::get<0>(fr), std::get<1>(fr)));
            auto   maxKbps = dpar.base.GetMaxKbps(dpar);

            changed += pCO3->WinBRCSize && SetIf(pCO3->WinBRCSize, pCO3->WinBRCSize != fps, fps);
            changed += pCO3->WinBRCMaxAvgKbps && SetIf(pCO3->WinBRCMaxAvgKbps, pCO3->WinBRCMaxAvgKbps != maxKbps, maxKbps);

            MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            return MFX_ERR_NONE;
        });

        bSet = true;

        return MFX_ERR_NONE;
    });
}

void BRCSlidingWindow::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& /*s_task*/) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);

        bool bLowTolerance =
            CO3.WinBRCSize
            && (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR);

        MFX_CHECK(bLowTolerance, MFX_ERR_NONE);

        auto vaType = Glob::VideoCore::Get(global).GetVAType();
        auto& ddiSPS = Deref(GetDDICB<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC>(
            ENCODE_ENC_PAK_ID, DDIPar_In, vaType, Glob::DDI_SubmitParam::Get(global)));

        ddiSPS.FrameSizeTolerance = eFrameSizeTolerance_Low;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)