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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "hevcehw_g12_embargo_lin.h"
#include "hevcehw_g12_rext_lin.h"
#include "hevcehw_g12_scc_lin.h"
#include "hevcehw_g12_caps_lin.h"
#include "hevcehw_base_iddi.h"
#include "hevcehw_g12_sao.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_parser.h"
#include "hevcehw_base_iddi_packer.h"

using namespace HEVCEHW::Gen12;

namespace HEVCEHW
{
namespace Linux
{
namespace Gen12_Embargo
{

MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
    TFeatureList newFeatures;

    newFeatures.emplace_back(new Linux::Gen12::RExt(FEATURE_REXT));
#ifdef MFX_ENABLE_HEVCE_SCC
    newFeatures.emplace_back(new Linux::Gen12::SCC(FEATURE_SCC));
#endif
    newFeatures.emplace_back(new Linux::Gen12::Caps(FEATURE_CAPS));
    newFeatures.emplace_back(new SAO(FEATURE_SAO));
    
    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseImpl::m_features.splice(TBaseImpl::m_features.end(), newFeatures);

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& qnc = BQ<BQ_Query1NoCaps>::Get(*this);

        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_CAPS, Caps::BLK_SetDefaultsCallChain });
#ifdef MFX_ENABLE_HEVCE_SCC
        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetLowPowerDefault }
            , { FEATURE_SCC, SCC::BLK_SetLowPowerDefault });
        Reorder(
            qnc
            , { HEVCEHW::Base::FEATURE_PARSER, HEVCEHW::Base::Parser::BLK_LoadSPSPPS }
            , { FEATURE_SCC, SCC::BLK_LoadSPSPPS });
#endif
        auto& qwc = BQ<BQ_Query1WithCaps>::Get(*this);
        Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps });
        Reorder(
            qwc
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps }
            , { FEATURE_CAPS, Caps::BLK_HardcodeCaps });
        FeatureBlocks::Reorder(
            qwc
            , { FEATURE_REXT, RExt::BLK_HardcodeCaps }
            , { HEVCEHW::Base::FEATURE_DDI_PACKER, HEVCEHW::Base::IDDIPacker::BLK_HardcodeCaps });
    }

#ifdef MFX_ENABLE_HEVCE_SCC
    if (mode & INIT)
    {
        auto& iint = BQ<BQ_InitInternal>::Get(*this);
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetSPS }
            , { FEATURE_SCC, SCC::BLK_SetSPSExt });
        Reorder(
            iint
            , { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetPPS }
            , { FEATURE_SCC, SCC::BLK_SetPPSExt });
    }
#endif
}

mfxStatus MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    auto sts = TBaseImpl::Init(par);
    MFX_CHECK_STS(sts);
#ifdef MFX_ENABLE_HEVCE_SCC
    auto& st = BQ<BQ_SubmitTask>::Get(*this);
    Reorder(
        st
        , { HEVCEHW::Base::FEATURE_DDI, HEVCEHW::Base::IDDI::BLK_SubmitTask }
        , { FEATURE_SCC, SCC::BLK_PatchDDITask });
#endif
    return MFX_ERR_NONE;
}

}}} //namespace HEVCEHW::Linux::Gen12

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
