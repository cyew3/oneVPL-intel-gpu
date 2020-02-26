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

#include "hevcehw_g11lkf.h"

#if defined(MFX_VA_LINUX)
#include "hevcehw_g11lkf_lin.h"
#else
#include "hevcehw_g11lkf_win.h"
#endif
#include "hevcehw_g9_data.h"
#include "hevcehw_g9_legacy.h"
#include "hevcehw_g11lkf_caps.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11LKF;

namespace HEVCEHW
{
    namespace Gen11LKF
    {
        enum eFeatureId
        {
            FEATURE_CAPS = Gen9::eFeatureId::NUM_FEATURES
            , NUM_FEATURES
        };
    }
}

template<class TBaseGen>
MFXVideoENCODEH265_HW<TBaseGen>::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseGen(core, status, mode)
{
    status = MFX_ERR_UNKNOWN;

    decltype(TBaseGen::m_features) newFeatures;

    newFeatures.emplace_back(new Caps(FEATURE_CAPS));

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseGen::m_features.splice(TBaseGen::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(*this);

        FeatureBlocks::Reorder(
            qnc
            , { HEVCEHW::Gen9::FEATURE_LEGACY, HEVCEHW::Gen9::Legacy::BLK_SetLowPowerDefault }
            , { HEVCEHW::Gen11LKF::FEATURE_CAPS, HEVCEHW::Gen11LKF::Caps::BLK_SetDefaultsCallChain });
    }

    status = MFX_ERR_NONE;
}

template class Gen11LKF::MFXVideoENCODEH265_HW<Gen11LKF::TPrevGenImpl>;

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
