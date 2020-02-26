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

#include "hevcehw_g12_win.h"
#include "hevcehw_g12_rext.h"
#include "hevcehw_g12_scc_win.h"
#include "hevcehw_g12_caps.h"
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
#include "hevcehw_g9_blocking_sync_win.h"
#endif
#include "hevcehw_g9_iddi.h"
#include "hevcehw_g12_sao.h"
#include "hevcehw_g12_qp_modulation_win.h"
#include "hevcehw_g9_protected_win.h"
#include "hevcehw_g9_data.h"
#include "hevcehw_g9_legacy.h"
#include "hevcehw_g9_parser.h"

using namespace HEVCEHW::Gen12;

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12
{

MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
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
        GetFeature<Gen9::Protected>(Gen9::FEATURE_PROTECTED).SetRecFlag(
            MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME
            | MFX_MEMTYPE_PROTECTED);
    }

    TFeatureList newFeatures;

    newFeatures.emplace_back(new RExt(FEATURE_REXT));
    newFeatures.emplace_back(new SCC(FEATURE_SCC));
    newFeatures.emplace_back(new Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
    newFeatures.emplace_back(new QpModulation(FEATURE_QP_MODULATION));
    
    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseImpl::m_features.splice(TBaseImpl::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = BQ<BQ_Query1NoCaps>::Get(*this);

        Reorder(
            qnc
            , { Gen9::FEATURE_LEGACY, Gen9::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });
        Reorder(
            qnc
            , { Gen9::FEATURE_LEGACY, Gen9::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_SCC, SCC::BLK_SetLowPowerDefault });
        Reorder(
            qnc
            , { Gen9::FEATURE_PARSER, Gen9::Parser::BLK_LoadSPSPPS }
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS });

        auto& qwc = BQ<BQ_Query1WithCaps>::Get(*this);
        Reorder(
            qwc
            , { Gen9::FEATURE_DDI_PACKER, Gen9::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps });
        Reorder(
            qwc
            , { Gen9::FEATURE_DDI_PACKER, Gen9::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps }
            , PLACE_AFTER);
    }

    if (mode & INIT)
    {
        auto& iint = BQ<BQ_InitInternal>::Get(*this);
        Reorder(
            iint
            , { Gen9::FEATURE_LEGACY, Gen9::Legacy::BLK_SetSPS }
            , { FEATURE_SCC, SCC::BLK_SetSPSExt });
        Reorder(
            iint
            , { Gen9::FEATURE_LEGACY, Gen9::Legacy::BLK_SetPPS }
            , { FEATURE_SCC, SCC::BLK_SetPPSExt });
    }
}

mfxStatus MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseImpl::Init(par);
    MFX_CHECK_STS(sts);

    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    Reorder(
        st
        , { Gen9::FEATURE_DDI, Gen9::IDDI::BLK_SubmitTask }
        , { FEATURE_SCC, SCC::BLK_PatchDDITask });

    return MFX_ERR_NONE;
}

}}} //namespace HEVCEHW::Windows::Gen12

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)