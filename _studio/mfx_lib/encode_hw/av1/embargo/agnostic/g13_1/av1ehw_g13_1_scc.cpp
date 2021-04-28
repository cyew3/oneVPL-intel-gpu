// Copyright (c) 2021 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_data.h"
#include "av1ehw_g13_1_scc.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Gen13_1;

namespace AV1EHW
{
namespace Gen13_1
{

void SCC::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_AUXDATA].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1AuxData*)pSrc;
        auto& buf_dst = *(mfxExtAV1AuxData*)pDst;
        MFX_COPY_FIELD(Palette);
        MFX_COPY_FIELD(IBC);
    });
}

void SCC::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [](const mfxVideoParam&, mfxVideoParam& par, StorageRW& global) -> mfxStatus
    {
        mfxExtAV1AuxData* pAV1Aux = ExtBuffer::Get(par);
        MFX_CHECK(pAV1Aux, MFX_ERR_NONE);

        mfxU32 changed = 0;
        mfxU32 invalid = 0;

        const auto& caps = Glob::EncodeCaps::Get(global);

        if (IsOn(pAV1Aux->Palette))
        {
            invalid += SetIf(pAV1Aux->Palette, !caps.AV1ToolSupportFlags.fields.PaletteMode, 0);
        }

        if (IsOn(pAV1Aux->IBC))
        {
            invalid += SetIf(pAV1Aux->IBC, !caps.AV1ToolSupportFlags.fields.allow_intrabc, 0);

            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
            if (pAV1Par)
            {
                changed += SetIf(
                    pAV1Par->EnableSuperres,
                    IsOn(pAV1Par->EnableSuperres) && IsOn(pAV1Aux->IBC),
                    MFX_CODINGOPTION_OFF);
            }
        }

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });
}

void SCC::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_AUXDATA].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtAV1AuxData*)pSrc;
        auto& dst = *(mfxExtAV1AuxData*)pDst;

        InheritOption(src.Palette, dst.Palette);
        InheritOption(src.IBC, dst.IBC);
    });
}

void SCC::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtAV1AuxData* pAux = ExtBuffer::Get(par);
        if (!pAux)
            return;

        SetDefault(pAux->Palette, MFX_CODINGOPTION_OFF);
        SetDefault(pAux->IBC, MFX_CODINGOPTION_OFF);

        if (IsOn(pAux->IBC))
        {
            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
            if (!pAV1Par)
                return;

            SetDefault(pAV1Par->EnableSuperres, MFX_CODINGOPTION_OFF);
            SetDefault(pAV1Par->EnableRestoration, MFX_CODINGOPTION_OFF);
        }
    });
}

void SCC::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetFH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const mfxExtAV1AuxData* pAV1Aux = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        MFX_CHECK(pAV1Aux, MFX_ERR_NONE);

        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_NOT_FOUND);
        auto& fh = Glob::FH::Get(strg);

        fh.allow_screen_content_tools = CO2Flag(pAV1Aux->Palette);

        return MFX_ERR_NONE;
    });
}

void SCC::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
        {
            FH& fh = Task::FH::Get(s_task);
            const mfxExtAV1AuxData& AV1Aux = ExtBuffer::Get(Glob::VideoParam::Get(global));

            fh.allow_intrabc = FrameIsIntra(fh) && CO2Flag(AV1Aux.IBC);

            if (fh.allow_intrabc)
            {
                DisableLoopFilter(fh);
                DisableCDEF(fh);
                DisableLoopRestoration(fh);

                fh.allow_screen_content_tools = 1;
            }

            return MFX_ERR_NONE;
        });
}

} //namespace Gen13_1
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
