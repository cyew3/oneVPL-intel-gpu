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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_ENABLE_HEVCE_UNITS_INFO)

#include "hevcehw_base_units_info_win.h"
#include "hevcehw_base_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows::Base;

void UnitsInfo::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;
        MFX_COPY_FIELD(EncodedUnitsInfo);
    });
}

void UnitsInfo::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtCodingOption3*)pSrc;
        auto& dst = *(mfxExtCodingOption3*)pDst;
        InheritOption(src.EncodedUnitsInfo,   dst.EncodedUnitsInfo);
    });
}

void UnitsInfo::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);

        auto& caps = Glob::EncodeCaps::Get(global);

        bool bChanged = CheckOrZero(pCO3->EncodedUnitsInfo
            , mfxU16(MFX_CODINGOPTION_UNKNOWN)
            , mfxU16(MFX_CODINGOPTION_ON * caps.SliceLevelReportSupport)
            , mfxU16(MFX_CODINGOPTION_OFF));

        MFX_CHECK(!bChanged, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });
}

void UnitsInfo::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);

        SetDefault(pCO3->EncodedUnitsInfo, MFX_CODINGOPTION_OFF);

        return MFX_ERR_NONE;
    });
}

void UnitsInfo::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetDDICallChains
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        MFX_CHECK(IsOn(CO3.EncodedUnitsInfo), MFX_ERR_NONE);

        auto& cc = DDIPacker::CC::Get(strg);
        cc.InitPPS.Push([](
            DDIPacker::CallChains::TInitPPS::TExt prev
            , const StorageR& glob
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC& pps)
        {
            prev(glob, pps);
            pps.bEnableSliceLevelReport = 1;
        });
        cc.InitFeedback.Push([](
            DDIPacker::CallChains::TInitFeedback::TExt prev
            , const StorageR& global
            , mfxU16 cacheSize
            , ENCODE_QUERY_STATUS_PARAM_TYPE /*fbType*/
            , mfxU32 maxSlices)
        {
            prev(global, cacheSize, QUERY_STATUS_PARAM_SLICE, maxSlices);
        });
        cc.ReadFeedback.Push([](
            DDIPacker::CallChains::TReadFeedback::TExt prev
            , const StorageR& global
            , StorageW& s_task
            , const void* pFB
            , mfxU32 size)
        {
            auto sts = prev(global, s_task, pFB, size);
            MFX_CHECK_STS(sts);

            auto& task = Task::Common::Get(s_task);
            ThrowAssert(!task.pBsOut, "task.pBsOut is NULL");

            mfxExtEncodedUnitsInfo* pUnitsInfo = ExtBuffer::Get(*task.pBsOut);
            MFX_CHECK(pUnitsInfo, sts);

            auto& fb        = *(ENCODE_QUERY_STATUS_SLICE_PARAMS_HEVC*)pFB;
            auto  pBegin    = pUnitsInfo->UnitInfo + std::min(pUnitsInfo->NumUnitsEncoded, pUnitsInfo->NumUnitsAlloc);

            pUnitsInfo->NumUnitsEncoded += fb.FrameLevelStatus.NumberSlices;

            auto pEnd       = pUnitsInfo->UnitInfo + std::min(pUnitsInfo->NumUnitsEncoded, pUnitsInfo->NumUnitsAlloc);
            auto pSliceSize = fb.pSliceSizes;

            std::for_each(pBegin, pEnd, [&](mfxEncodedUnitInfo& unit) { unit.Size = *pSliceSize++; });

            return sts;
        });

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)