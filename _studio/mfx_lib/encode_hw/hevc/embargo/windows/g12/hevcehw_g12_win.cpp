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

#include "hevcehw_g12_win.h"
#include "hevcehw_g12_rext.h"
#include "hevcehw_g12_scc_win.h"
#include "hevcehw_g12_caps.h"
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
#include "hevcehw_g11_blocking_sync_win.h"
#endif
#include "hevcehw_g11_iddi.h"
#include "hevcehw_g12_sao.h"
#include "hevcehw_g12_qp_modulation_win.h"
#if defined(MFX_ENABLE_MFE)
#include "hevcehw_g12_mfe_win.h"
#endif //defined(MFX_ENABLE_MFE)
#include "hevcehw_g11_protected_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

using TBaseImpl = Gen12::MFXVideoENCODEH265_HW<Gen12::TPrevGenImpl>;

Windows::Gen12::MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    const mfxU32 DEFAULT_H265_TIMEOUT_MS_SIM = 3600000; // 1 hour
    GetFeature<Gen11::BlockingSync>(Gen11::FEATURE_BLOCKING_SYNC).SetTimeout(DEFAULT_H265_TIMEOUT_MS_SIM);
#endif

    // TODO: temporal fix for miracast issue (hang in CP + MMC case).
    //
    // In case of MFX_MEMTYPE_PROTECTED we MUST specify D3D11_RESOURCE_MISC_HW_PROTECTED flag
    //
    // But now there is no enough time to investigate behavior on other platforms therefore
    // TGL condition was added
    //
    // After TGL alpha code freeze TGL condition MUST be removed to setup protected flag on ALL platforms (which is correct behavior),
    // because it is supposed to be used for any render target that will get protected output at some point in time
    if (core.GetHWType() == MFX_HW_TGL_LP)
    {
        GetFeature<Gen11::Protected>(Gen11::FEATURE_PROTECTED).SetRecFlag(MFX_MEMTYPE_PROTECTED);
    }

    decltype(m_features) newFeatures;

    newFeatures.emplace_back(new RExt(FEATURE_REXT));
    newFeatures.emplace_back(new SCC(FEATURE_SCC));
    newFeatures.emplace_back(new Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
    newFeatures.emplace_back(new QpModulation(FEATURE_QP_MODULATION));

#if defined(MFX_ENABLE_MFE)
    if (core.GetVAType() == MFX_HW_D3D11)
    {
        m_features.emplace_back(new MFE(FEATURE_MFE));
    }
#endif //defined(MFX_ENABLE_MFE)

    InternalInitFeatures(status, mode, newFeatures);
}

mfxStatus Windows::Gen12::MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseImpl::Init(par);
    MFX_CHECK_STS(sts);

    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    Reorder(
        st
        , { Gen11::FEATURE_DDI, Gen11::IDDI::BLK_SubmitTask }
        , { FEATURE_SCC, SCC::BLK_PatchDDITask });
#if defined(MFX_ENABLE_MFE)
    if (m_core.GetVAType() == MFX_HW_D3D11)
    {
        Reorder(
            st
            , { Gen11::FEATURE_DDI, Gen11::IDDI::BLK_SubmitTask }
            , { FEATURE_MFE, MFE::BLK_UpdateDDITask });
    }
#endif //defined(MFX_ENABLE_MFE)

    return MFX_ERR_NONE;
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)