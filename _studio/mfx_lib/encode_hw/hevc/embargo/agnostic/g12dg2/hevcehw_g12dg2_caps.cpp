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

#include "hevcehw_g12dg2_caps.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12DG2;

void Caps::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    using Gen9::Glob;
    using Gen9::Defaults;

    Push(BLK_CheckLowPower,
        [](const mfxVideoParam& par, mfxVideoParam&, StorageRW& /*strg*/) -> mfxStatus
    {
        MFX_CHECK(par.mfx.LowPower != MFX_CODINGOPTION_OFF, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetLowPower.Push([](
            Defaults::TGetHWDefault<mfxU16>::TExt /*prev*/
            , const mfxVideoParam& /*par*/
            , eMFXHWType /*hw*/)
        {
            return mfxU16(MFX_CODINGOPTION_ON);
        });

        defaults.GetMaxNumRef.Push([](
            Gen9::Defaults::TChain<std::tuple<mfxU16, mfxU16>>::TExt
            , const Gen9::Defaults::Param& dpar)
        {
            const mfxU16 nRef[2][2][7] =
            {
                {   // DG2 VDENC P
                    { 2, 2, 2, 2, 2, 1, 1 },
                    { 2, 2, 2, 2, 2, 1, 1 }
                },
                {   // DG2 VDENC RA B
                    { 1, 1, 1, 1, 1, 1, 1 },
                    { 1, 1, 1, 1, 1, 1, 1 }
                }
            };
            bool    bBFrames = (dpar.mvp.mfx.GopRefDist > 1);
            mfxU16  tu       = dpar.mvp.mfx.TargetUsage;;

            CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, 4);
            --tu;

            /* Same way like on previous platforms... */
            mfxU16 numRefFrame = dpar.mvp.mfx.NumRefFrame + !dpar.mvp.mfx.NumRefFrame * 16;

            return std::make_tuple(
                std::min<mfxU16>(nRef[bBFrames][0][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[bBFrames][1][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference1, numRefFrame)));
        });

        bSet = true;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
