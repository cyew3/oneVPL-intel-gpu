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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "hevcehw_g12_lin.h"
#include "hevcehw_g12_rext_lin.h"
#include "hevcehw_g12_scc_lin.h"
#include "hevcehw_g12_caps_lin.h"
#include "hevcehw_g12_sao.h"
#if defined(MFX_ENABLE_MFE)
#include "hevcehw_g12_mfe_lin.h"
#endif //defined(MFX_ENABLE_MFE)

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

Linux::Gen12::MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : HEVCEHW::Gen12::MFXVideoENCODEH265_HW<HEVCEHW::Gen12::TPrevGenImpl>(core, status, mode)
{
    decltype(m_features) newFeatures;

    newFeatures.emplace_back(new RExt(FEATURE_REXT));
    newFeatures.emplace_back(new SCC(FEATURE_SCC));
    newFeatures.emplace_back(new Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
#if defined(MFX_ENABLE_MFE)
    newFeatures.emplace_back(new MFE(FEATURE_MFE));
#endif //defined(MFX_ENABLE_MFE)

    InternalInitFeatures(status, mode, newFeatures);
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)