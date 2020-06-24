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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base_ddi_packer_win.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_d3d9_win.h"
#include "hevcehw_base_lpla_analysis.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows::Base;

void LpLookAheadAnalysis::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetCallChains
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& ddiCC = Base::DDIPacker::CC::Get(global);
        using TCC = Base::DDIPacker::CallChains;

        ddiCC.UpdateSPS.Push([this](
            TCC::TUpdateSPS::TExt
            , const StorageR& global
            , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& sps)
        {
            auto& par = Glob::VideoParam::Get(global);
            const mfxExtLplaParam* lpla = ExtBuffer::Get(par);

            if (!m_lplaEnabled)
            {
                return;
            }

            //when HEVC VDEnc as lookahead pass, the encoder works in CQP mode, but need to set
            //BRC parameters in the sps for the lookahead kernel for analysis
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                sps.InitVBVBufferFullnessInBit = 8000 * lpla->InitialDelayInKB;
                sps.VBVBufferSizeInBit = 8000 * lpla->BufferSizeInKB;
                sps.TargetBitRate = lpla->TargetKbps;
                sps.LookaheadDepth = (UCHAR)lpla->LookAheadDepth;
            }

            // need to disable SAO for lowpower lookahead analysis since the alogrithm doesn't support
            sps.SAO_enabled_flag = 0;
        });

        ddiCC.UpdateCqmHint.Push([this](
            TCC::TUpdateCqmHint::TExt
            , TaskCommonPar& task
            , const LOOKAHEAD_INFO& pLPLA)
        {
            if (!m_lplaEnabled)
            {
                return;
            }

            if (pLPLA.ValidInfo)
            {
                task.LplaStatus.ValidInfo = pLPLA.ValidInfo;
                task.LplaStatus.CqmHint = pLPLA.CqmHint;
                task.LplaStatus.TargetFrameSize = pLPLA.TargetFrameSize;
            }
            else
            {
                task.LplaStatus.ValidInfo = 0;
                task.LplaStatus.CqmHint = CQM_HINT_INVALID;
                task.LplaStatus.TargetFrameSize = 0;
            }
        });

        return MFX_ERR_NONE;
    });
}

void LpLookAheadAnalysis::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& strg, StorageRW&)
    {
        auto& caps = Glob::EncodeCaps::Get(strg);
        const mfxExtLplaParam* lpla = ExtBuffer::Get(par);

        m_lplaEnabled = lpla && lpla->LookAheadDepth > 0 && caps.LookaheadAnalysisSupport;
        if (m_lplaEnabled)
        {
            auto pHEVC = (mfxExtHEVCParam&)ExtBuffer::Get(par);
            pHEVC.SampleAdaptiveOffset = 1;
        }
    });
}

void LpLookAheadAnalysis::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_LP_LOOKAHEAD].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtLplaParam*)pSrc;
        auto& buf_dst = *(mfxExtLplaParam*)pDst;

        MFX_COPY_FIELD(LookAheadDepth);
        MFX_COPY_FIELD(InitialDelayInKB);
        MFX_COPY_FIELD(BufferSizeInKB);
        MFX_COPY_FIELD(TargetKbps);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption2*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption2*)pDst;
        MFX_COPY_FIELD(LookAheadDepth);
    });
}

void LpLookAheadAnalysis::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_LP_LOOKAHEAD].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtLplaParam*)pSrc;
        auto& dst = *(mfxExtLplaParam*)pDst;

        InheritOption(src.LookAheadDepth, dst.LookAheadDepth);
        InheritOption(src.InitialDelayInKB, dst.InitialDelayInKB);
        InheritOption(src.BufferSizeInKB, dst.BufferSizeInKB);
        InheritOption(src.TargetKbps, dst.TargetKbps);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtCodingOption2*)pSrc;
        auto& dst = *(mfxExtCodingOption2*)pDst;

        InheritOption(src.LookAheadDepth, dst.LookAheadDepth);
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)