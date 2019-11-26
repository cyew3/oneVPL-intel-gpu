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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_g12.h"

#if defined(MFX_VA_LINUX)
#include "hevcehw_g12_lin.h"
#else
#include "hevcehw_g12_win.h"
#endif
#include "hevcehw_g12_rext.h"
#include "hevcehw_g12_scc.h"
#include "hevcehw_g11_legacy.h"
#include "hevcehw_g11_parser.h"
#include "hevcehw_g11_iddi_packer.h"
#include "hevcehw_g11_iddi.h"
#include "hevcehw_g12_caps.h"
#include "hevcehw_g12_sao.h"

namespace HEVCEHW
{
using namespace HEVCEHW::Gen12;

template<class TBaseGen>
MFXVideoENCODEH265_HW<TBaseGen>::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseGen(core, status, mode)
{
}

template<class TBaseGen>
void MFXVideoENCODEH265_HW<TBaseGen>::InternalInitFeatures(
    mfxStatus& status
    , eFeatureMode mode
    , TFeatureList& newFeatures)
{
    status = MFX_ERR_UNKNOWN;

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseGen::m_features.splice(TBaseGen::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(*this);

        qnc.splice(qnc.begin(), qnc, FeatureBlocks::Get(qnc, { FEATURE_CAPS, Caps::BLK_CheckLowPower }));

        FeatureBlocks::Reorder(
            qnc
            , { Gen11::FEATURE_LEGACY, Gen11::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });
        FeatureBlocks::Reorder(
            qnc
            , { Gen11::FEATURE_LEGACY, Gen11::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_SCC, SCC::BLK_SetLowPowerDefault });
        FeatureBlocks::Reorder(
            qnc
            , { Gen11::FEATURE_PARSER, Gen11::Parser::BLK_LoadSPSPPS }
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS });

        auto& qwc = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(*this);
        FeatureBlocks::Reorder(
            qwc
            , { Gen11::FEATURE_DDI_PACKER, Gen11::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps });
        FeatureBlocks::Reorder(
            qwc
            , { Gen11::FEATURE_DDI_PACKER, Gen11::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps });
    }

    if (mode & INIT)
    {
        auto& iint = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(*this);
        FeatureBlocks::Reorder(
            iint
            , { Gen11::FEATURE_LEGACY, Gen11::Legacy::BLK_SetSPS }
            , { FEATURE_SCC, SCC::BLK_SetSPSExt });
        FeatureBlocks::Reorder(
            iint
            , { Gen11::FEATURE_LEGACY, Gen11::Legacy::BLK_SetPPS }
            , { FEATURE_SCC, SCC::BLK_SetPPSExt });
    }

    status = MFX_ERR_NONE;
}

template<class TBaseGen>
mfxStatus MFXVideoENCODEH265_HW<TBaseGen>::Init(mfxVideoParam *par)
{
    auto sts = TBaseGen::Init(par);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

template class Gen12::MFXVideoENCODEH265_HW<Gen12::TPrevGenImpl>;
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
