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
#include "hevcehw_g12_rext_win.h"
#include "hevcehw_g12_scc_win.h"
#include "hevcehw_g12_caps_win.h"
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE)
#include "hevcehw_base_blocking_sync_win.h"
#endif
#include "hevcehw_base_iddi.h"
#include "hevcehw_g12_sao.h"
#include "hevcehw_g12_qp_modulation_win.h"
#include "hevcehw_base_protected_win.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_parser.h"
#include "hevcehw_base_recon_info_win.h"
#include "hevcehw_g12_scc_mode.h"

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
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        GetFeature<Base::Protected>(Base::FEATURE_PROTECTED).SetRecFlag(
            MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME
            | MFX_MEMTYPE_PROTECTED);
#endif //!MFX_PROTECTED_FEATURE_DISABLE
    }

    TFeatureList newFeatures;

    newFeatures.emplace_back(new RExt(FEATURE_REXT));
    newFeatures.emplace_back(new SCC(FEATURE_SCC));
    newFeatures.emplace_back(new Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
    newFeatures.emplace_back(new QpModulation(FEATURE_QP_MODULATION));
    newFeatures.emplace_back(new SCCMode(FEATURE_SCCMODE));

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseImpl::m_features.splice(TBaseImpl::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = BQ<BQ_Query1NoCaps>::Get(*this);

        Reorder(
            qnc
            , { Base::FEATURE_LEGACY, Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });
        Reorder(
            qnc
            , { Base::FEATURE_LEGACY, Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_SCC, SCC::BLK_SetLowPowerDefault });
        Reorder(
            qnc
            , { Base::FEATURE_PARSER, Base::Parser::BLK_LoadSPSPPS }
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS });
        Reorder(
            qnc
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS }
            , { FEATURE_SCCMODE, SCCMode::BLK_CheckAndFix });

        auto& qwc = BQ<BQ_Query1WithCaps>::Get(*this);
        Reorder(
            qwc
            , { Base::FEATURE_DDI_PACKER, Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps });
        Reorder(
            qwc
            , { Base::FEATURE_DDI_PACKER, Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps }
            , PLACE_AFTER);
        Reorder(
            qwc
            , { Base::FEATURE_LEGACY, Base::Legacy::BLK_CheckHeaders }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCapsExt });
    }

    if (mode & INIT)
    {
        auto& iint = BQ<BQ_InitInternal>::Get(*this);
        Reorder(
            iint
            , { Base::FEATURE_LEGACY, Base::Legacy::BLK_SetSPS }
            , { FEATURE_SCC, SCC::BLK_SetSPSExt });
        Reorder(
            iint
            , { Base::FEATURE_LEGACY, Base::Legacy::BLK_SetPPS }
            , { FEATURE_SCC, SCC::BLK_SetPPSExt });
        Reorder(
            iint
            , { Base::FEATURE_RECON_INFO, Base::ReconInfo::BLK_SetRecInfo }
            , { FEATURE_REXT, RExt::BLK_SetRecInfo }
            , PLACE_AFTER);
    }
}

mfxStatus MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseImpl::Init(par);
    MFX_CHECK_STS(sts);

    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    Reorder(
        st
        , { Base::FEATURE_DDI, Base::IDDI::BLK_SubmitTask }
        , { FEATURE_SCC, SCC::BLK_PatchDDITask });

    return MFX_ERR_NONE;
}

}}} //namespace HEVCEHW::Windows::Gen12

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
