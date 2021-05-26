// Copyright (c) 2020-2021 Intel Corporation
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

#include "hevcehw_g12_scc_mode.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

void SCCMode::Query1NoCaps(const FeatureBlocks & /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(par);
        MFX_CHECK(pCODDI, MFX_ERR_NONE);
        MFX_CHECK(!IsOff(par.mfx.LowPower), MFX_ERR_NONE);

        mfxU32 changed = 0;

        // IBC or Palette mode can be only controlled for SCC Profile
        if (par.mfx.CodecProfile != MFX_PROFILE_HEVC_SCC)
        {
            if (IsOn(pCODDI->IBC) || IsOn(pCODDI->Palette))
            {
                changed += 1;
                pCODDI->IBC     = MFX_CODINGOPTION_OFF;
                pCODDI->Palette = MFX_CODINGOPTION_OFF;
            }
            MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

            return MFX_ERR_NONE;
        }

        auto& sccflags = Glob::SCCFlags::GetOrConstruct(strg);
        if (IsOff(pCODDI->IBC))
            sccflags.IBCEnable = 0;

        if (IsOff(pCODDI->Palette))
            sccflags.PaletteEnable = 0;

        // SCC feature switch is controlled by CodecProfile. If both IBC and Palette mode are turned off, it should be fixed as default setting.
        if (sccflags.IBCEnable == 0 && sccflags.PaletteEnable == 0)
        {
            changed += 1;
            sccflags.IBCEnable     = 1;
            sccflags.PaletteEnable = 1;
            pCODDI->IBC            = MFX_CODINGOPTION_ON;
            pCODDI->Palette        = MFX_CODINGOPTION_ON;
        }
        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });
}

void SCCMode::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_DDI].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOptionDDI*)pSrc;
        auto& buf_dst = *(mfxExtCodingOptionDDI*)pDst;
        MFX_COPY_FIELD(IBC);
        MFX_COPY_FIELD(Palette);
    });
}

void SCCMode::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_DDI].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtCodingOptionDDI*)pSrc;
        auto& dst = *(mfxExtCodingOptionDDI*)pDst;

        InheritOption(src.IBC, dst.IBC);
        InheritOption(src.Palette, dst.Palette);
    });
}

void SCCMode::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(par);
        MFX_CHECK(pCODDI, MFX_ERR_NONE);
        MFX_CHECK(!IsOff(par.mfx.LowPower), MFX_ERR_NONE);

        if (par.mfx.CodecProfile != MFX_PROFILE_HEVC_SCC)
        {
            SetDefault(pCODDI->IBC, (mfxU16)MFX_CODINGOPTION_OFF);
            SetDefault(pCODDI->Palette, (mfxU16)MFX_CODINGOPTION_OFF);
        }
        else
        {
            SetDefault(pCODDI->IBC, (mfxU16)MFX_CODINGOPTION_ON);
            SetDefault(pCODDI->Palette, (mfxU16)MFX_CODINGOPTION_ON);
        }

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
