// Copyright (c) 2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_superres.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace AV1EHW
{
void Superres::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        mfxExtAV1Param* pAV1Par = ExtBuffer::Get(out);
        MFX_CHECK(pAV1Par, MFX_ERR_NONE);

        MFX_CHECK(pAV1Par->SuperresScaleDenominator != 0, MFX_ERR_NONE);

        mfxU32 changed = 0;
        mfxU32 invalid = 0;
        auto&  caps    = Glob::EncodeCaps::Get(strg);

        changed += SetIf(pAV1Par->SuperresScaleDenominator,
                         IsOn(pAV1Par->EnableSuperres) &&
                             (pAV1Par->SuperresScaleDenominator < 9 ||
                              pAV1Par->SuperresScaleDenominator > 16),
                         DEFAULT_DENOM_FOR_SUPERRES_ON);

        changed += SetIf(pAV1Par->SuperresScaleDenominator,
                         IsOff(pAV1Par->EnableSuperres) &&
                             pAV1Par->SuperresScaleDenominator !=
                                 DEFAULT_DENOM_FOR_SUPERRES_OFF,
                         DEFAULT_DENOM_FOR_SUPERRES_OFF);

        invalid += !caps.SuperResSupport && IsOn(pAV1Par->EnableSuperres);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });
}

void Base::Superres::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
        if (!pAV1Par)
            return;

        SetDefault(pAV1Par->EnableSuperres, MFX_CODINGOPTION_OFF);

        mfxU8 defaultDenom = IsOn(pAV1Par->EnableSuperres) ?
            DEFAULT_DENOM_FOR_SUPERRES_ON :
            DEFAULT_DENOM_FOR_SUPERRES_OFF;  // default denom for superres off, no scaling will be performed

        SetDefault(pAV1Par->SuperresScaleDenominator, defaultDenom);
    });
}

void Base::Superres::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetSH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        MFX_CHECK(pAV1Par, MFX_ERR_NONE);

        MFX_CHECK(strg.Contains(Glob::SH::Key), MFX_ERR_NOT_FOUND);
        auto &sh = Glob::SH::Get(strg);

        sh.enable_superres = CO2Flag(pAV1Par->EnableSuperres);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetFH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        MFX_CHECK(pAV1Par, MFX_ERR_NONE);

        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_NOT_FOUND);
        auto& fh = Glob::FH::Get(strg);

        fh.use_superres = CO2Flag(pAV1Par->EnableSuperres) &&
            pAV1Par->SuperresScaleDenominator >= 9 &&
            pAV1Par->SuperresScaleDenominator <= 16;

        fh.frame_size_override_flag =
            fh.use_superres ? 1 : fh.frame_size_override_flag;

        fh.SuperresDenom = pAV1Par->SuperresScaleDenominator;
        fh.UpscaledWidth = pAV1Par->FrameWidth;

        return MFX_ERR_NONE;
    });
}

void Superres::ResetState(const FeatureBlocks& blocks, TPushRS Push)
{
    Push(BLK_ResetState
        , [this, &blocks](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        // todo: add reset support
        std::ignore = global;  // temporal code to fix compile warning

        return MFX_ERR_NONE;
    });
}
} // namespace AV1EHW

#endif // defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
