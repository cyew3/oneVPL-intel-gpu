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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "hevcehw_base_weighted_prediction_win.h"
#include "hevcehw_base_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void Windows::Base::WeightPred::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) 
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& esSlice = Task::SSH::Get(s_task);
        auto& par     = Glob::DDI_SubmitParam::Get(global);
        auto  vaType  = Glob::VideoCore::Get(global).GetVAType();
        auto& ddiPPS  = Deref(GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_HEVC>(ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));

        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(Glob::VideoParam::Get(global));
        ddiPPS.bEnableGPUWeightedPrediction = IsOn(CO3.FadeDetection); // should be set for I as well

        bool bNeedPWT = (esSlice.type != 2 && (ddiPPS.weighted_bipred_flag || ddiPPS.weighted_pred_flag));
        MFX_CHECK(bNeedPWT, MFX_ERR_NONE);

        auto pDdiSlice = GetDDICB<ENCODE_SET_SLICE_HEADER_HEVC>(ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par);

        ThrowAssert(!pDdiSlice, "pDdiSlice is invalid");

        auto& ph    = Glob::PackedHeaders::Get(global);
        auto  itSSH = ph.SSH.begin();

        ThrowAssert(ph.SSH.size() < ddiPPS.NumSlices, "Num. packed slices is invalid");
                
        mfxI16 wY = (1 << esSlice.luma_log2_weight_denom);
        mfxI16 wC = (1 << esSlice.chroma_log2_weight_denom);

        auto SetPWT = [&](ENCODE_SET_SLICE_HEADER_HEVC& cs)
        {
            const mfxU16 Y = 0, Cb = 1, Cr = 2, W = 0, O = 1;

            cs.luma_log2_weight_denom         = (UCHAR)esSlice.luma_log2_weight_denom;
            cs.delta_chroma_log2_weight_denom = (CHAR)(esSlice.chroma_log2_weight_denom - cs.luma_log2_weight_denom);
            cs.PredWeightTableBitOffset       = itSSH->PackInfo.at(PACK_PWTOffset);
            cs.PredWeightTableBitLength       = itSSH->PackInfo.at(PACK_PWTLength);

            ++itSSH;

            for (mfxU16 l = 0; l < 2; l++)
            {
                for (mfxU16 i = 0; i < 15; ++i)
                {
                    cs.luma_offset[l][i]            = (CHAR)(esSlice.pwt[l][i][Y][O]);
                    cs.delta_luma_weight[l][i]      = (CHAR)(esSlice.pwt[l][i][Y][W] - wY);
                    cs.chroma_offset[l][i][0]       = (CHAR)(esSlice.pwt[l][i][Cb][O]);
                    cs.chroma_offset[l][i][1]       = (CHAR)(esSlice.pwt[l][i][Cr][O]);
                    cs.delta_chroma_weight[l][i][0] = (CHAR)(esSlice.pwt[l][i][Cb][W] - wC);
                    cs.delta_chroma_weight[l][i][1] = (CHAR)(esSlice.pwt[l][i][Cr][W] - wC);
                }
            }
        };

        std::for_each(pDdiSlice, pDdiSlice + ddiPPS.NumSlices, SetPWT);

        return MFX_ERR_NONE;
    });
}


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)