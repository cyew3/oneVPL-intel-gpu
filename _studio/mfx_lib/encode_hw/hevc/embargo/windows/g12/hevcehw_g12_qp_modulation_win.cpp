// Copyright (c) 2019-2020 Intel Corporation
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

#include "hevcehw_g12_qp_modulation_win.h"
#include "hevcehw_g12_data.h"
#include "hevcehw_g9_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

void Windows::Gen12::QpModulation::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetCallChains,
        [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& ddiCC = Gen9::DDIPacker::CC::Get(strg);
        using TCC = Gen9::DDIPacker::CallChains;
        auto hwType = Glob::VideoCore::Get(strg).GetHWType();

        ddiCC.InitSPS.Push([hwType](
            TCC::TInitSPS::TExt prev
            , const StorageR& glob
            , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& sps)
        {
            prev(glob, sps);
            
            // NumOfBInGop is deprecated from Gen12
            sps.NumOfBInGop[0] = 0;
            sps.NumOfBInGop[1] = 0;
            sps.NumOfBInGop[2] = 0;

            const mfxVideoParam& par = Glob::VideoParam::Get(glob);
            const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);
            Defaults::Param dflts(
                par
                , Glob::EncodeCaps::Get(glob)
                , hwType
                , Glob::Defaults::Get(glob));
            mfxU16 numTL = dflts.base.GetNumTemporalLayers(dflts);

            sps.HierarchicalFlag = (CO2.BRefType == MFX_B_REF_PYRAMID) || (numTL > 1 && numTL < 4);
            sps.GopRefDist = (IsOn(par.mfx.LowPower) && sps.LowDelayMode && sps.HierarchicalFlag) ? (1 << (numTL - 1)) : sps.GopRefDist; // distance between anchor frames for driver
        });

        ddiCC.UpdatePPS.Push([this](
            TCC::TUpdatePPS::TExt prev
            , const StorageR& global
            , const StorageR& s_task
            , const ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& sps
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC& pps)
        {
            prev(global, s_task, sps, pps);

            auto& task           = Task::Common::Get(s_task);
            auto& bsSPS          = Glob::SPS::Get(global);
            bool bHLByCodingType = !sps.LowDelayMode && sps.HierarchicalFlag;
            bool bHLByTemporalID = sps.LowDelayMode && bsSPS.max_sub_layers_minus1;
            bool bHLByPyrLevel   = sps.LowDelayMode && !bHLByTemporalID;

            // QP modulation feature; used in low delay mode only
            SetIf(pps.HierarchLevelPlus1, bHLByTemporalID, task.TemporalID + 1);
            SetIf(pps.HierarchLevelPlus1, bHLByPyrLevel, task.PyramidLevel + 1);

            if (bHLByCodingType)
            {
                ThrowAssert(Check<mfxU8, 1, 2, 3, 4, 5>(task.CodingType), "invalid coding type");

                pps.HierarchLevelPlus1  = (task.CodingType - 1) * (task.CodingType > 3);
                pps.HierarchLevelPlus1 += 2 * (task.CodingType == 3 && !task.isLDB);
                pps.HierarchLevelPlus1 += !pps.HierarchLevelPlus1;

                pps.CodingType = std::min(task.CodingType, mfxU8(3));
            }
        });

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)