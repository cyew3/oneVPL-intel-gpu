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

#include "hevcehw_g12ats_win.h"
#include "hevcehw_g11_blocking_sync_win.h"
#if defined(MFX_ENABLE_MFE)
#include "hevcehw_g12ats_mfe_win.h"
#endif //defined(MFX_ENABLE_MFE)
#include "hevcehw_g11_data.h"
#include "hevcehw_g11_iddi.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12ATS
{

MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    const mfxU32 DEFAULT_H265_TIMEOUT_MS_SIM = 3600000; // 1 hour
    GetFeature<Gen11::BlockingSync>(HEVCEHW::Gen11::FEATURE_BLOCKING_SYNC).SetTimeout(DEFAULT_H265_TIMEOUT_MS_SIM);
#endif

    TFeatureList newFeatures;

#if defined(MFX_ENABLE_MFE)
    if (core.GetVAType() == MFX_HW_D3D11)
    {
        m_features.emplace_back(new MFE(FEATURE_MFE));
    }
#endif //defined(MFX_ENABLE_MFE)
    
    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    m_features.splice(m_features.end(), newFeatures);
}

mfxStatus MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseImpl::Init(par);
    MFX_CHECK_STS(sts);

#if defined(MFX_ENABLE_MFE)
    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    if (m_core.GetVAType() == MFX_HW_D3D11)
    {
        Reorder(
            st
            , { HEVCEHW::Gen11::FEATURE_DDI, HEVCEHW::Gen11::IDDI::BLK_SubmitTask }
            , { FEATURE_MFE, MFE::BLK_UpdateDDITask });
    }
#endif //defined(MFX_ENABLE_MFE)

    return MFX_ERR_NONE;
}

}}} //namespace HEVCEHW::Windows::Gen12ATS

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)