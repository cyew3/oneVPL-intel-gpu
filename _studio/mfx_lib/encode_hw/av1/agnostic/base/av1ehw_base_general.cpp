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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_general.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_constraints.h"
#include "fast_copy.h"
#include <algorithm>
#include <exception>
#include <numeric>
#include <set>

using namespace AV1EHW::Base;

namespace AV1EHW
{

void General::SetSupported(ParamSupport& blocks)
{

    blocks.m_mvpCopySupported.emplace_back(
        [](const mfxVideoParam* pSrc, mfxVideoParam* pDst) -> void
    {
        const auto& buf_src = *(const mfxVideoParam*)pSrc;
        auto& buf_dst = *(mfxVideoParam*)pDst;
        MFX_COPY_FIELD(IOPattern);
        MFX_COPY_FIELD(Protected);
        MFX_COPY_FIELD(AsyncDepth);
        MFX_COPY_FIELD(mfx.CodecId);
        MFX_COPY_FIELD(mfx.LowPower);
        MFX_COPY_FIELD(mfx.CodecLevel);
        MFX_COPY_FIELD(mfx.CodecProfile);
        MFX_COPY_FIELD(mfx.TargetUsage);
        MFX_COPY_FIELD(mfx.GopPicSize);
        MFX_COPY_FIELD(mfx.GopRefDist);
        MFX_COPY_FIELD(mfx.BRCParamMultiplier);
        MFX_COPY_FIELD(mfx.RateControlMethod);
        MFX_COPY_FIELD(mfx.InitialDelayInKB);
        MFX_COPY_FIELD(mfx.BufferSizeInKB);
        MFX_COPY_FIELD(mfx.TargetKbps);
        MFX_COPY_FIELD(mfx.MaxKbps);
        MFX_COPY_FIELD(mfx.NumRefFrame);
        MFX_COPY_FIELD(mfx.EncodedOrder);
        MFX_COPY_FIELD(mfx.FrameInfo.Shift);
        MFX_COPY_FIELD(mfx.FrameInfo.BitDepthLuma);
        MFX_COPY_FIELD(mfx.FrameInfo.BitDepthChroma);
        MFX_COPY_FIELD(mfx.FrameInfo.FourCC);
        MFX_COPY_FIELD(mfx.FrameInfo.Width);
        MFX_COPY_FIELD(mfx.FrameInfo.Height);
        MFX_COPY_FIELD(mfx.FrameInfo.CropX);
        MFX_COPY_FIELD(mfx.FrameInfo.CropY);
        MFX_COPY_FIELD(mfx.FrameInfo.CropW);
        MFX_COPY_FIELD(mfx.FrameInfo.CropH);
        MFX_COPY_FIELD(mfx.FrameInfo.FrameRateExtN);
        MFX_COPY_FIELD(mfx.FrameInfo.FrameRateExtD);
        MFX_COPY_FIELD(mfx.FrameInfo.AspectRatioW);
        MFX_COPY_FIELD(mfx.FrameInfo.AspectRatioH);
        MFX_COPY_FIELD(mfx.FrameInfo.ChromaFormat);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1Param*)pSrc;
        auto& buf_dst = *(mfxExtAV1Param*)pDst;
        MFX_COPY_FIELD(FrameWidth);
        MFX_COPY_FIELD(FrameHeight);

        MFX_COPY_FIELD(WriteIVFHeaders);
        MFX_COPY_FIELD(UseAnnexB);
        MFX_COPY_FIELD(PackOBUFrame);
        MFX_COPY_FIELD(InsertTemporalDelimiter);

        MFX_COPY_FIELD(EnableCdef);
        MFX_COPY_FIELD(EnableRestoration);
        MFX_COPY_FIELD(LoopFilterSharpness);
        MFX_COPY_FIELD(InterpFilter);
        MFX_COPY_FIELD(SegmentationMode);
        MFX_COPY_FIELD(DisableCdfUpdate);
        MFX_COPY_FIELD(DisableFrameEndUpdateCdf);

        MFX_COPY_FIELD(EnableSuperres);
        MFX_COPY_FIELD(SuperresScaleDenominator);
        MFX_COPY_FIELD(StillPictureMode);
        MFX_COPY_FIELD(SwitchInterval);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_AUXDATA].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1AuxData*)pSrc;
        auto& buf_dst = *(mfxExtAV1AuxData*)pDst;
        MFX_COPY_FIELD(Cdef.CdefDampingMinus3);
        MFX_COPY_FIELD(Cdef.CdefBits);

        MFX_COPY_FIELD(LoopFilter.LFLevelYVert);
        MFX_COPY_FIELD(LoopFilter.LFLevelYHorz);
        MFX_COPY_FIELD(LoopFilter.LFLevelU);
        MFX_COPY_FIELD(LoopFilter.LFLevelV);
        MFX_COPY_FIELD(LoopFilter.ModeRefDeltaEnabled);
        MFX_COPY_FIELD(LoopFilter.ModeRefDeltaUpdate);

        MFX_COPY_FIELD(QP.YDcDeltaQ);
        MFX_COPY_FIELD(QP.UDcDeltaQ);
        MFX_COPY_FIELD(QP.VDcDeltaQ);
        MFX_COPY_FIELD(QP.UAcDeltaQ);
        MFX_COPY_FIELD(QP.VAcDeltaQ);
        MFX_COPY_FIELD(QP.MinBaseQIndex);
        MFX_COPY_FIELD(QP.MaxBaseQIndex);

        MFX_COPY_FIELD(EnableOrderHint);
        MFX_COPY_FIELD(OrderHintBits);
        MFX_COPY_FIELD(ErrorResilientMode);
        MFX_COPY_FIELD(DisplayFormatSwizzle);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtOpaqueSurfaceAlloc*)pSrc;
        auto& buf_dst = *(mfxExtOpaqueSurfaceAlloc*)pDst;
        MFX_COPY_FIELD(In.Surfaces);
        MFX_COPY_FIELD(In.Type);
        MFX_COPY_FIELD(In.NumSurface);
        MFX_COPY_FIELD(Out.Surfaces);
        MFX_COPY_FIELD(Out.Type);
        MFX_COPY_FIELD(Out.NumSurface);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;

        MFX_COPY_FIELD(GPB);

        for (mfxU32 i = 0; i < 8; i++)
        {
            MFX_COPY_FIELD(NumRefActiveP[i]);
            MFX_COPY_FIELD(NumRefActiveBL0[i]);
            MFX_COPY_FIELD(NumRefActiveBL1[i]);
        }

        MFX_COPY_FIELD(TargetChromaFormatPlus1);
        MFX_COPY_FIELD(TargetBitDepthLuma);
        MFX_COPY_FIELD(TargetBitDepthChroma);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCODER_RESET_OPTION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtEncoderResetOption*)pSrc;
        auto& buf_dst = *(mfxExtEncoderResetOption*)pDst;
        MFX_COPY_FIELD(StartNewSequence);
    });
}

void General::SetInherited(ParamInheritance& par)
{
    par.m_mvpInheritDefault.emplace_back(
        [](const mfxVideoParam* pSrc, mfxVideoParam* pDst)
    {
        auto& parInit = *pSrc;
        auto& parReset = *pDst;

#define INHERIT_OPT(OPT) InheritOption(parInit.OPT, parReset.OPT)
#define INHERIT_BRC(OPT) { OPT tmp(parReset.mfx); InheritOption(OPT(parInit.mfx), tmp); }

        INHERIT_OPT(AsyncDepth);
        //INHERIT_OPT(mfx.BRCParamMultiplier);
        INHERIT_OPT(mfx.CodecId);
        INHERIT_OPT(mfx.CodecProfile);
        INHERIT_OPT(mfx.CodecLevel);
        INHERIT_OPT(mfx.NumThread);
        INHERIT_OPT(mfx.TargetUsage);
        INHERIT_OPT(mfx.GopPicSize);
        INHERIT_OPT(mfx.GopRefDist);
        INHERIT_OPT(mfx.RateControlMethod);
        INHERIT_OPT(mfx.BufferSizeInKB);
        INHERIT_OPT(mfx.NumRefFrame);

        mfxU16 RC = parInit.mfx.RateControlMethod
            * (parInit.mfx.RateControlMethod == parReset.mfx.RateControlMethod);
        static const std::map<
            mfxU16 , std::function<void(const mfxVideoParam&, mfxVideoParam&)>
        > InheritBrcOpt =
        {
            {
                mfxU16(MFX_RATECONTROL_CBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_VBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_CQP)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.QPI);
                    INHERIT_OPT(mfx.QPP);
                    INHERIT_OPT(mfx.QPB);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_ICQ)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.ICQQuality);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_LA_ICQ)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.ICQQuality);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_VCM)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_QVBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
        };
        auto itInheritBrcOpt = InheritBrcOpt.find(RC);

        if (itInheritBrcOpt != InheritBrcOpt.end())
            itInheritBrcOpt->second(parInit, parReset);

        INHERIT_OPT(mfx.FrameInfo.FourCC);
        INHERIT_OPT(mfx.FrameInfo.Width);
        INHERIT_OPT(mfx.FrameInfo.Height);
        INHERIT_OPT(mfx.FrameInfo.CropX);
        INHERIT_OPT(mfx.FrameInfo.CropY);
        INHERIT_OPT(mfx.FrameInfo.CropW);
        INHERIT_OPT(mfx.FrameInfo.CropH);
        INHERIT_OPT(mfx.FrameInfo.FrameRateExtN);
        INHERIT_OPT(mfx.FrameInfo.FrameRateExtD);
        INHERIT_OPT(mfx.FrameInfo.AspectRatioW);
        INHERIT_OPT(mfx.FrameInfo.AspectRatioH);

#undef INHERIT_OPT
#undef INHERIT_BRC
    });

#define INIT_EB(TYPE)\
    if (!pSrc || !pDst) return;\
    auto& ebInit = *(TYPE*)pSrc;\
    auto& ebReset = *(TYPE*)pDst;
#define INHERIT_OPT(OPT) InheritOption(ebInit.OPT, ebReset.OPT);

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_PARAM].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1Param);
        INHERIT_OPT(WriteIVFHeaders);
        INHERIT_OPT(UseAnnexB);
        INHERIT_OPT(PackOBUFrame);
        INHERIT_OPT(InsertTemporalDelimiter);

        INHERIT_OPT(EnableCdef);
        INHERIT_OPT(EnableRestoration);
        INHERIT_OPT(LoopFilterSharpness);
        INHERIT_OPT(InterpFilter);
        INHERIT_OPT(SegmentationMode);
        INHERIT_OPT(DisableCdfUpdate);
        INHERIT_OPT(DisableFrameEndUpdateCdf);

        INHERIT_OPT(EnableSuperres);
        INHERIT_OPT(SuperresScaleDenominator);
        INHERIT_OPT(StillPictureMode);
        INHERIT_OPT(SwitchInterval);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_AUXDATA].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1AuxData);
        INHERIT_OPT(Cdef.CdefDampingMinus3);
        INHERIT_OPT(Cdef.CdefBits);

        INHERIT_OPT(LoopFilter.LFLevelYVert);
        INHERIT_OPT(LoopFilter.LFLevelYHorz);
        INHERIT_OPT(LoopFilter.LFLevelU);
        INHERIT_OPT(LoopFilter.LFLevelV);
        INHERIT_OPT(LoopFilter.ModeRefDeltaEnabled);
        INHERIT_OPT(LoopFilter.ModeRefDeltaUpdate);

        INHERIT_OPT(QP.YDcDeltaQ);
        INHERIT_OPT(QP.UDcDeltaQ);
        INHERIT_OPT(QP.VDcDeltaQ);
        INHERIT_OPT(QP.UAcDeltaQ);
        INHERIT_OPT(QP.VAcDeltaQ);
        INHERIT_OPT(QP.MinBaseQIndex);
        INHERIT_OPT(QP.MaxBaseQIndex);

        INHERIT_OPT(EnableOrderHint);
        INHERIT_OPT(OrderHintBits);
        INHERIT_OPT(ErrorResilientMode);
        INHERIT_OPT(DisplayFormatSwizzle);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [this](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtCodingOption3);

        INHERIT_OPT(GPB);

        for (mfxU32 i = 0; i < 8; i++)
        {
            INHERIT_OPT(NumRefActiveP[i]);
            INHERIT_OPT(NumRefActiveBL0[i]);
            INHERIT_OPT(NumRefActiveBL1[i]);
        }

        INHERIT_OPT(TargetChromaFormatPlus1);
        INHERIT_OPT(TargetBitDepthLuma);
        INHERIT_OPT(TargetBitDepthChroma);
    });
#undef INIT_EB
#undef INHERIT_OPT
}

void General::Query0(const FeatureBlocks& blocks, TPushQ0 Push)
{
    using namespace std::placeholders;
    Push(BLK_Query0, std::bind(&General::CheckQuery0, this, std::cref(blocks), _1));
}

void General::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        PushDefaults(defaults);

        bSet = true;

        m_pQNCDefaults = &defaults;
        m_hw = Glob::VideoCore::Get(strg).GetHWType();

        return MFX_ERR_NONE;
    });

    Push(BLK_PreCheckCodecId,
        [&blocks, this](const mfxVideoParam& in, mfxVideoParam&, StorageRW& /*strg*/) -> mfxStatus
    {
        return m_pQNCDefaults->PreCheckCodecId(in);
    });

    Push(BLK_PreCheckChromaFormat,
        [this](const mfxVideoParam& in, mfxVideoParam&, StorageW&) -> mfxStatus
    {
        return m_pQNCDefaults->PreCheckChromaFormat(in);
    });

    Push(BLK_PreCheckLowPower,
        [&blocks, this](const mfxVideoParam& in, mfxVideoParam&, StorageRW& /*strg*/) -> mfxStatus
    {
        return m_pQNCDefaults->PreCheckLowPower(in);
    });

    Push(BLK_PreCheckExtBuffers
        , [this, &blocks](const mfxVideoParam& in, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckBuffers(blocks, in, &out);
    });

    Push(BLK_CopyConfigurable
        , [this, &blocks](const mfxVideoParam& in, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CopyConfigurable(blocks, in, out);
    });

    Push(BLK_SetLowPowerDefault
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& /*strg*/) -> mfxStatus
    {
        bool bChanged = out.mfx.LowPower && out.mfx.LowPower != MFX_CODINGOPTION_ON;
        out.mfx.LowPower = MFX_CODINGOPTION_ON;
        MFX_CHECK(!bChanged, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    });

    Push(BLK_SetGUID
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageRW& strg) -> mfxStatus
    {
        MFX_CHECK(!strg.Contains(Glob::GUID::Key), MFX_ERR_NONE);

        if (strg.Contains(Glob::RealState::Key))
        {
            //don't change GUID in Reset
            auto& initPar = Glob::RealState::Get(strg);
            strg.Insert(Glob::GUID::Key, make_storable<GUID>(Glob::GUID::Get(initPar)));
            return MFX_ERR_NONE;
        }

        VideoCORE& core = Glob::VideoCore::Get(strg);
        auto pGUID = make_storable<GUID>();
        auto& defaults = Glob::Defaults::Get(strg);
        EncodeCapsAv1 fakeCaps;
        Defaults::Param defPar(out, fakeCaps, core.GetHWType(), defaults);

        MFX_CHECK(defaults.GetGUID(defPar, *pGUID), MFX_ERR_NONE);
        strg.Insert(Glob::GUID::Key, std::move(pGUID));

        return MFX_ERR_NONE;
    });
}

void General::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckFormat
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        m_pQWCDefaults.reset(
            new Defaults::Param(
                out
                , Glob::EncodeCaps::Get(strg)
                , Glob::VideoCore::Get(strg).GetHWType()
                , Glob::Defaults::Get(strg)));

        auto sts = m_pQWCDefaults->base.CheckFourCC(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckInputFormatByFourCC(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckTargetChromaFormat(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckTargetBitDepth(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        return m_pQWCDefaults->base.CheckFourCCByTargetFormat(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckLevel
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return m_pQWCDefaults->base.CheckLevel(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckSurfSize
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return m_pQWCDefaults->base.CheckSurfSize(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckCodedPicSize
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckCodedPicSize(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckTU
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckTU(out, m_pQWCDefaults->caps);
    });

    Push(BLK_CheckDeltaQ
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckDeltaQ(out);
    });

    Push(BLK_CheckFrameOBU
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckFrameOBU(out, caps);
    });

    Push(BLK_CheckOrderHint
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckOrderHint(out, caps);
    });

    Push(BLK_CheckOrderHintBits
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckOrderHintBits(out);
    });

    Push(BLK_CheckCDEF
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckCDEF(out, caps);
    });

    Push(BLK_CheckGopRefDist
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckGopRefDist(out, m_pQWCDefaults->caps);
    });

    Push(BLK_CheckGPB
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckGPB(out);
    });

    Push(BLK_CheckNumRefFrame
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckNumRefFrame(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckIOPattern
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckIOPattern(out);
    });

    Push(BLK_CheckBRC
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckBRC(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckCrops
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckCrops(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckShift
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckShift(out, ExtBuffer::Get(out));
    });

    Push(BLK_CheckFrameRate
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckFrameRate(out);
    });

    Push(BLK_CheckProfile
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return m_pQWCDefaults->base.CheckProfile(*m_pQWCDefaults, out);
    });
}

static mfxStatus RunQuery1NoCapsQueue(const FeatureBlocks& blocks, const mfxVideoParam& in, StorageRW& strg)
{
    mfxStatus sts = MFX_ERR_NONE;

    auto pPar = make_storable<ExtBuffer::Param<mfxVideoParam>>(in);
    auto& par = *pPar;
    const auto& query = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, query, in, par, strg);
    MFX_CHECK(sts != MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    strg.Insert(Glob::VideoParam::Key, std::move(pPar));

    return MFX_ERR_NONE;
}

static mfxStatus RunSetDefaultsQueue(const FeatureBlocks& blocks, StorageRW& strg, StorageRW& local)
{
    auto& par = Glob::VideoParam::Get(strg);
    Glob::EncodeCaps::GetOrConstruct(strg);
    auto& qSD = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);

    return RunBlocks(IgnoreSts, qSD, par, strg, local);
};

static mfxStatus RunQuery1WithCapsQueue(const FeatureBlocks& blocks, const mfxVideoParam& in, StorageRW& strg)
{
    mfxStatus sts = MFX_ERR_NONE;

    auto& par = Glob::VideoParam::Get(strg);
    auto& queryWC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryWC, in, par, strg);
    MFX_CHECK(sts != MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
};

void General::QueryIOSurf(const FeatureBlocks& blocks, TPushQIS Push)
{
    Push(BLK_Query1NoCaps
        , [this, &blocks](const mfxVideoParam& in, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        return RunQuery1NoCapsQueue(blocks, in, strg);
    });

    Push(BLK_SetDefaults
        , [this, &blocks](const mfxVideoParam&, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        StorageRW local;

        return RunSetDefaultsQueue(blocks, strg, local);
    });

    Push(BLK_Query1WithCaps
        , [this, &blocks](const mfxVideoParam& in, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        return RunQuery1WithCapsQueue(blocks, in, strg);
    });

    Push(BLK_SetFrameAllocRequest
        , [this, &blocks](const mfxVideoParam&, mfxFrameAllocRequest& req, StorageRW& strg) -> mfxStatus
    {
        ExtBuffer::Param<mfxVideoParam>& par = Glob::VideoParam::Get(strg);
        auto fourCC = par.mfx.FrameInfo.FourCC;

        req.Info = par.mfx.FrameInfo;
        SetDefault(req.Info.Shift, (fourCC == MFX_FOURCC_P010 || fourCC == MFX_FOURCC_Y210));

        bool bSYS = par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        bool bVID = par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY;
        bool bOPQ = par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY;

        req.Type =
            bSYS * (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME)
            + bVID * (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME)
            + bOPQ * (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME);
        MFX_CHECK(req.Type, MFX_ERR_INVALID_VIDEO_PARAM);

        req.NumFrameMin = GetMaxRaw(par);
        req.NumFrameSuggested = req.NumFrameMin;

        return MFX_ERR_NONE;
    });
}

void General::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& strg, StorageRW&)
    {
        auto& core = Glob::VideoCore::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);
        auto& defchain = Glob::Defaults::Get(strg);
        SetDefaults(par, Defaults::Param(par, caps, core.GetHWType(), defchain), core.IsExternalFrameAllocator());
    });
}

void General::InitExternal(const FeatureBlocks& blocks, TPushIE Push)
{
    Push(BLK_Query1NoCaps
        , [this, &blocks](const mfxVideoParam& in, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return RunQuery1NoCapsQueue(blocks, in, strg);
    });

    Push(BLK_AttachMissingBuffers
        , [this, &blocks](const mfxVideoParam&, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        for (auto& eb : blocks.m_ebCopySupported)
            par.NewEB(eb.first, false);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaults
        , [this, &blocks](const mfxVideoParam&, StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        return RunSetDefaultsQueue(blocks, strg, local);
    });

    Push(BLK_Query1WithCaps
        , [this, &blocks](const mfxVideoParam& in, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return RunQuery1WithCapsQueue(blocks, in, strg);
    });
}

void General::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetReorder
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        using namespace std::placeholders;
        auto& par = Glob::VideoParam::Get(strg);

        auto pReorderer = make_storable<Reorderer>();
        pReorderer->Push(
            [&](Reorderer::TExt, TTaskIt begin, TTaskIt end, bool bFlush)
        {
            return ReorderWrap(par, begin, end, bFlush);
        });
        pReorderer->BufferSize = par.mfx.GopRefDist > 1 ? par.mfx.GopRefDist - 1 : 0;
        //pReorderer->DPB = &m_prevTask.DPB.After;

        strg.Insert(Glob::Reorder::Key, std::move(pReorderer));

        return MFX_ERR_NONE;
    });

    Push(BLK_SetSH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        if (!strg.Contains(Glob::SH::Key))
        {
            auto pSH = make_storable<SH>();

            SetSH(
                Glob::VideoParam::Get(strg)
                , Glob::VideoCore::Get(strg).GetHWType()
                , Glob::EncodeCaps::Get(strg)
                , *pSH);

            strg.Insert(Glob::SH::Key, std::move(pSH));
        }

        // TODO: clarify if we need additional check of SequenceHeader here

        return MFX_ERR_NONE;
    });

    Push(BLK_SetFH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        if (!strg.Contains(Glob::FH::Key))
        {
            std::unique_ptr<MakeStorable<FH>> pFH(new MakeStorable<FH>);
            SetFH(
                Glob::VideoParam::Get(strg)
                , Glob::VideoCore::Get(strg).GetHWType()
                , Glob::SH::Get(strg)
                , *pFH);
            strg.Insert(Glob::FH::Key, std::move(pFH));
        }

        // TODO: clarify if we need additional check of FrameHeader here

        return MFX_ERR_NONE;
    });

    Push(BLK_SetRecInfo
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        mfxFrameAllocRequest rec = {}, raw = {};

        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        if (GetRecInfo(par, pCO3, Glob::VideoCore::Get(strg).GetHWType(), rec.Info))
        {
            auto& recInfo = Tmp::RecInfo::GetOrConstruct(local, rec);
            SetDefault(recInfo.NumFrameMin, GetMaxRec(par));
        }

        raw.Info = par.mfx.FrameInfo;
        auto& rawInfo = Tmp::RawInfo::GetOrConstruct(local, raw);
        SetDefault(rawInfo.NumFrameMin, GetMaxRaw(par));
        SetDefault(rawInfo.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
                | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                | MFX_MEMTYPE_INTERNAL_FRAME));

        return MFX_ERR_NONE;
    });
}

void General::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_AllocRaw
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        auto& rawInfo = Tmp::RawInfo::Get(local);
        auto AllocRaw = [&](mfxU16 NumFrameMin)
        {
            std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));
            mfxFrameAllocRequest req = rawInfo;
            req.NumFrameMin = NumFrameMin;

            sts = pAlloc->Alloc(req, true);
            MFX_CHECK_STS(sts);

            strg.Insert(Glob::AllocRaw::Key, std::move(pAlloc));

            return MFX_ERR_NONE;
        };

        if (par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        {
            sts = AllocRaw(rawInfo.NumFrameMin);
            MFX_CHECK_STS(sts);
        }
        else if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            const mfxExtOpaqueSurfaceAlloc& osa = ExtBuffer::Get(par);
            std::unique_ptr<IAllocation> pAllocOpq(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));

            sts = pAllocOpq->AllocOpaque(par.mfx.FrameInfo, osa.In.Type, osa.In.Surfaces, osa.In.NumSurface);
            MFX_CHECK_STS(sts);

            strg.Insert(Glob::AllocOpq::Key, std::move(pAllocOpq));

            if (osa.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                sts = AllocRaw(osa.In.NumSurface);
                MFX_CHECK_STS(sts);
            }
        }

        return sts;
    });

    Push(BLK_AllocRec
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));

        MFX_CHECK(local.Contains(Tmp::RecInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto& req = Tmp::RecInfo::Get(local);

        SetDefault(req.NumFrameMin, GetMaxRec(par));
        SetDefault(req.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME
            | MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET));

        sts = pAlloc->Alloc(req, false);
        MFX_CHECK_STS(sts);

        strg.Insert(Glob::AllocRec::Key, std::move(pAlloc));

        return sts;
    });

    Push(BLK_AllocBS
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));

        MFX_CHECK(local.Contains(Tmp::BSAllocInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto& req = Tmp::BSAllocInfo::Get(local);

        SetDefault(req.NumFrameMin, GetMaxBS(par));
        SetDefault(req.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME));

        mfxU32 minBS = GetMinBsSize(par, ExtBuffer::Get(par), ExtBuffer::Get(par));

        if (mfxU32(req.Info.Width * req.Info.Height) < minBS)
        {
            MFX_CHECK(req.Info.Width != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            req.Info.Height = (mfxU16)CeilDiv<mfxU32>(minBS, req.Info.Width);
        }

        sts = pAlloc->Alloc(req, false);
        MFX_CHECK_STS(sts);

        strg.Insert(Glob::AllocBS::Key, std::move(pAlloc));

        return sts;
    });
}

void General::Reset(const FeatureBlocks& blocks, TPushR Push)
{
    Push(BLK_ResetInit
        , [this, &blocks](
            const mfxVideoParam& par
            , StorageRW& global
            , StorageRW& local) -> mfxStatus
    {
        mfxStatus wrn = MFX_ERR_NONE;
        auto& init = Glob::RealState::Get(global);
        auto pParNew = make_storable<ExtBuffer::Param<mfxVideoParam>>(par);
        ExtBuffer::Param<mfxVideoParam>& parNew = *pParNew;
        auto& parOld = Glob::VideoParam::Get(init);
        auto& core = Glob::VideoCore::Get(init);

        global.Insert(Glob::ResetHint::Key, make_storable<ResetHint>(ResetHint{}));
        auto& hint = Glob::ResetHint::Get(global);

        const mfxExtEncoderResetOption* pResetOpt = ExtBuffer::Get(par);

        hint.Flags = RF_IDR_REQUIRED * (pResetOpt && IsOn(pResetOpt->StartNewSequence));


        m_GetMaxRef = [&](const mfxVideoParam& par)
        {
            auto& def = Glob::Defaults::Get(init);
            auto hw = core.GetHWType();
            auto& caps = Glob::EncodeCaps::Get(init);
            return def.GetMaxNumRef(Defaults::Param(par, caps, hw, def));
        };

        std::for_each(std::begin(blocks.m_ebCopySupported)
            , std::end(blocks.m_ebCopySupported)
            , [&](decltype(*std::begin(blocks.m_ebCopySupported)) eb) { parNew.NewEB(eb.first, false); });

        std::for_each(std::begin(blocks.m_mvpInheritDefault)
            , std::end(blocks.m_mvpInheritDefault)
            , [&](decltype(*std::begin(blocks.m_mvpInheritDefault)) inherit) { inherit(&parOld, &parNew); });


        std::for_each(std::begin(blocks.m_ebInheritDefault)
            , std::end(blocks.m_ebInheritDefault)
            , [&](decltype(*std::begin(blocks.m_ebInheritDefault)) eb)
        {
            auto pEbNew = ExtBuffer::Get(parNew, eb.first);
            auto pEbOld = ExtBuffer::Get(parOld, eb.first);

            MFX_CHECK(pEbNew && pEbOld, MFX_ERR_NONE);

            std::for_each(std::begin(eb.second)
                , std::end(eb.second)
                , [&](decltype(*std::begin(eb.second)) inherit) { inherit(parOld, pEbOld, parNew, pEbNew); });

            return MFX_ERR_NONE;
        });

        auto& qInitExternal = FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks);
        auto& qInitInternal = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);

        auto sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, qInitExternal, parNew, global, local);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);
        wrn = sts;

        sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, qInitInternal, global, local);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        return GetWorstSts(sts, wrn);
    });

    Push(BLK_ResetCheck
        , [this, &blocks](
            const mfxVideoParam& par
            , StorageRW& global
            , StorageRW& local) -> mfxStatus
    {
        auto& init = Glob::RealState::Get(global);
        auto& parOld = Glob::VideoParam::Get(init);
        auto& parNew = Glob::VideoParam::Get(global);
        auto& hint = Glob::ResetHint::Get(global);
        auto defOld = GetRTDefaults(init);
        auto defNew = GetRTDefaults(global);

        const mfxExtEncoderResetOption* pResetOpt = ExtBuffer::Get(par);

        const mfxExtHEVCParam (&hevcPar)[2] = { ExtBuffer::Get(parOld), ExtBuffer::Get(parNew) };
        MFX_CHECK(hevcPar[0].LCUSize == hevcPar[1].LCUSize, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); // LCU Size Can't be changed

        MFX_CHECK(parOld.AsyncDepth                 == parNew.AsyncDepth,                   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.GopRefDist             >= parNew.mfx.GopRefDist,               MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.NumRefFrame            >= parNew.mfx.NumRefFrame,              MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.RateControlMethod      == parNew.mfx.RateControlMethod,        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.FrameInfo.ChromaFormat == parNew.mfx.FrameInfo.ChromaFormat,   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.IOPattern                  == parNew.IOPattern,                    MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        MFX_CHECK(local.Contains(Tmp::RecInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto  recOld = Glob::AllocRec::Get(init).GetInfo();
        auto& recNew = Tmp::RecInfo::Get(local).Info;
        MFX_CHECK(recOld.Width  >= recNew.Width,  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(recOld.Height >= recNew.Height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(recOld.FourCC == recNew.FourCC, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        MFX_CHECK(
            !(   parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
              || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
              || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            ||(  (mfxU32)InitialDelayInKB(parOld.mfx) == (mfxU32)InitialDelayInKB(parNew.mfx)
              && (mfxU32)BufferSizeInKB(parOld.mfx) == (mfxU32)BufferSizeInKB(parNew.mfx))
            , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        bool isIdrRequired = false;

        // check if IDR required after change of encoding parameters
        const mfxExtCodingOption2(&CO2)[2] = { ExtBuffer::Get(parOld), ExtBuffer::Get(parNew) };

        isIdrRequired =
               (hint.Flags & RF_SPS_CHANGED)
            || (hint.Flags & RF_IDR_REQUIRED)
            || parOld.mfx.GopPicSize != parNew.mfx.GopPicSize
            || CO2[0].IntRefType != CO2[1].IntRefType;

        hint.Flags |= RF_IDR_REQUIRED * isIdrRequired;

        MFX_CHECK(!isIdrRequired || !(pResetOpt && IsOff(pResetOpt->StartNewSequence))
            , MFX_ERR_INVALID_VIDEO_PARAM); // Reset can't change parameters w/o IDR. Report an error

        bool brcReset =
            (      parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            && (   (mfxU32)TargetKbps(parOld.mfx) != (mfxU32)TargetKbps(parNew.mfx)
                || (mfxU32)BufferSizeInKB(parOld.mfx) != (mfxU32)BufferSizeInKB(parNew.mfx)
                || (mfxU32)InitialDelayInKB(parOld.mfx) != (mfxU32)InitialDelayInKB(parNew.mfx)
                || parOld.mfx.FrameInfo.FrameRateExtN != parNew.mfx.FrameInfo.FrameRateExtN
                || parOld.mfx.FrameInfo.FrameRateExtD != parNew.mfx.FrameInfo.FrameRateExtD);

        brcReset |=
            (      parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            && ((mfxU32)MaxKbps(parOld.mfx) != (mfxU32)MaxKbps(parNew.mfx));

        MFX_CHECK(
           !(   brcReset
             && parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
             && !isIdrRequired)
            , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        hint.Flags |= RF_BRC_RESET * (brcReset || isIdrRequired);

        return MFX_ERR_NONE;
    });
}

void General::ResetState(const FeatureBlocks& blocks, TPushRS Push)
{
    Push(BLK_ResetState
        , [this, &blocks](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& real = Glob::RealState::Get(global);
        auto& parInt = Glob::VideoParam::Get(real);
        auto& parNew = Glob::VideoParam::Get(global);
        auto& hint = Glob::ResetHint::Get(global);

        CopyConfigurable(blocks, parNew, parInt);
        Glob::SH::Get(real) = Glob::SH::Get(global);
        Glob::FH::Get(real) = Glob::FH::Get(global);

        m_forceHeaders |= !!(hint.Flags & RF_PPS_CHANGED) * INSERT_PPS;

        MFX_CHECK(hint.Flags & RF_IDR_REQUIRED, MFX_ERR_NONE);

        Glob::AllocRec::Get(real).UnlockAll();
        Glob::AllocBS::Get(real).UnlockAll();

        if (real.Contains(Glob::AllocRaw::Key))
            Glob::AllocRaw::Get(real).UnlockAll();

        ResetState();

        return MFX_ERR_NONE;
    });
}

void General::FrameSubmit(const FeatureBlocks& blocks, TPushFS Push)
{
    Push(BLK_CheckSurf
        , [this, &blocks](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* pSurf
            , mfxBitstream& /*bs*/
            , StorageW& global
            , StorageRW& /*local*/) -> mfxStatus
    {
        MFX_CHECK(pSurf, MFX_ERR_NONE);

        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(LumaIsNull(pSurf) == (pSurf->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(pSurf->Info.Width >= par.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pSurf->Info.Height >= par.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);
        
        return MFX_ERR_NONE;
    });

    Push(BLK_CheckBS
        , [this, &blocks](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* /*pSurf*/
            , mfxBitstream& bs
            , StorageW& global
            , StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        BsDataInfo bsData = {};

        bsData.Data       = bs.Data;
        bsData.DataLength = bs.DataLength;
        bsData.DataOffset = bs.DataOffset;
        bsData.MaxLength  = bs.MaxLength;

        if (local.Contains(Tmp::BsDataInfo::Key))
            bsData = Tmp::BsDataInfo::Get(local);

        MFX_CHECK(bsData.DataOffset <= bsData.MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(bsData.DataOffset + bsData.DataLength + BufferSizeInKB(par.mfx) * 1000u <= bsData.MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        MFX_CHECK_NULL_PTR1(bsData.Data);

        return MFX_ERR_NONE;
    });
}

void General::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR& /*global*/
            , StorageRW& task) -> mfxStatus
    {
        task.Insert(Task::Common::Key, new Task::Common::TRef);
        task.Insert(Task::FH::Key, new MakeStorable<Task::FH::TRef>);
        return MFX_ERR_NONE;
    });
}

void General::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* pCtrl
            , mfxFrameSurface1* pSurf
            , mfxBitstream* pBs
            , StorageW& global
            , StorageW& task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        auto& core = Glob::VideoCore::Get(global);
        auto& tpar = Task::Common::Get(task);

        auto stage = tpar.stage;
        tpar = TaskCommonPar();
        tpar.stage = stage;
        tpar.pBsOut = pBs;

        MFX_CHECK(pSurf, MFX_ERR_NONE);

        tpar.pSurfIn = pSurf;

        if (pCtrl)
            tpar.ctrl = *pCtrl;

        if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            tpar.pSurfReal = core.GetNativeSurface(tpar.pSurfIn);
            MFX_CHECK(tpar.pSurfReal, MFX_ERR_UNDEFINED_BEHAVIOR);

            tpar.pSurfReal->Info             = tpar.pSurfIn->Info;
            tpar.pSurfReal->Data.TimeStamp   = tpar.pSurfIn->Data.TimeStamp;
            tpar.pSurfReal->Data.FrameOrder  = tpar.pSurfIn->Data.FrameOrder;
            tpar.pSurfReal->Data.Corrupted   = tpar.pSurfIn->Data.Corrupted;
            tpar.pSurfReal->Data.DataFlag    = tpar.pSurfIn->Data.DataFlag;
        }
        else
            tpar.pSurfReal = tpar.pSurfIn;

        core.IncreaseReference(&tpar.pSurfIn->Data);

        tpar.DPB.resize(par.mfx.NumRefFrame);

        return MFX_ERR_NONE;
    });
}

void General::PreReorderTask(const FeatureBlocks& blocks, TPushPreRT Push)
{
    Push(BLK_PrepareTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        auto& task = Task::Common::Get(s_task);
        auto  dflts = GetRTDefaults(global);

        if (par.mfx.EncodedOrder)
        {
            assert(false && "THIS HEVC CODE IS STRIPPED FOR AV1!!!");
        }

        auto sts = dflts.base.GetPreReorderInfo(
            dflts, task, task.pSurfIn, &task.ctrl, m_lastKeyFrame, m_frameOrder);
        MFX_CHECK_STS(sts);

        task.DisplayOrder = m_frameOrder;

        SetIf(m_lastKeyFrame, IsI(task.FrameType), m_frameOrder);

        ++m_frameOrder;

        return MFX_ERR_NONE;
    });
}

void General::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        if (global.Contains(Glob::AllocRaw::Key))
        {
            task.Raw = Glob::AllocRaw::Get(global).Acquire();
            MFX_CHECK(task.Raw.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        auto& recPool = Glob::AllocRec::Get(global);
        task.Rec = recPool.Acquire();
        task.BS = Glob::AllocBS::Get(global).Acquire();
        MFX_CHECK(task.BS.Idx != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(task.Rec.Idx != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(task.Rec.Mid && task.BS.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& sh = Glob::SH::Get(global);
        auto& fh = Glob::FH::Get(global);
        auto  def = GetRTDefaults(global);

        ConfigureTask(task, def, sh, recPool);

        auto sts = GetCurrentFrameHeader(task, def, sh, fh, Task::FH::Get(s_task));
        MFX_CHECK_STS(sts);

        return sts;
    });
}

void General::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_GetRawHDL
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(global);
        auto& par = Glob::VideoParam::Get(global);
        const mfxExtOpaqueSurfaceAlloc& opaq = ExtBuffer::Get(par);
        auto& task = Task::Common::Get(s_task);

        bool bInternalFrame =
            par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY
            || (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY
                && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            || task.bSkip;

        MFX_CHECK(!bInternalFrame, core.GetFrameHDL(task.Raw.Mid, &task.HDLRaw.first));

        MFX_CHECK(par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY
            , core.GetExternalFrameHDL(task.pSurfReal->Data.MemId, &task.HDLRaw.first));

        MFX_CHECK(par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY
            , MFX_ERR_UNDEFINED_BEHAVIOR);

        return core.GetFrameHDL(task.pSurfReal->Data.MemId, &task.HDLRaw.first);
    });

    Push(BLK_CopySysToRaw
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task)->mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        const mfxExtOpaqueSurfaceAlloc& opaq = ExtBuffer::Get(par);
        auto& task = Task::Common::Get(s_task);

        MFX_CHECK(
            !(task.bSkip
            || par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY
            || (    par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY
                 && !(opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
            , MFX_ERR_NONE);

        auto& core = Glob::VideoCore::Get(global);

        mfxFrameSurface1 surfSrc = { {}, par.mfx.FrameInfo, task.pSurfReal->Data };
        mfxFrameSurface1 surfDst = { {}, par.mfx.FrameInfo, {} };
        surfDst.Data.MemId = task.Raw.Mid;

        surfDst.Info.Shift =
            surfDst.Info.FourCC == MFX_FOURCC_P010
            || surfDst.Info.FourCC == MFX_FOURCC_Y210; // convert to native shift in core.CopyFrame() if required

        return core.DoFastCopyWrapper(
            &surfDst
            , MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_ENCODE
            , &surfSrc
            , MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
    });
}

void General::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_CopyBS
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        MFX_CHECK(task.BsDataLength, MFX_ERR_NONE);

        mfxStatus sts = MFX_ERR_NONE;

        if (!task.pBsData)
        {
            auto& bs              = *task.pBsOut;
            task.pBsData          = bs.Data + bs.DataOffset + bs.DataLength;
            task.pBsDataLength    = &bs.DataLength;
            task.BsBytesAvailable = bs.MaxLength - bs.DataOffset - bs.DataLength;
        }

        MFX_CHECK(task.BsBytesAvailable >= task.BsDataLength, MFX_ERR_NOT_ENOUGH_BUFFER);

        FrameLocker codedFrame(Glob::VideoCore::Get(global), task.BS.Mid);
        MFX_CHECK(codedFrame.Y, MFX_ERR_LOCK_MEMORY);

        sts = FastCopy::Copy(
            task.pBsData
            , task.BsDataLength
            , codedFrame.Y
            , codedFrame.Pitch
            , { int(task.BsDataLength), 1 }
            , COPY_VIDEO_TO_SYS);
        MFX_CHECK_STS(sts);

        task.BsBytesAvailable -= task.BsDataLength;

        return MFX_ERR_NONE;
    });

    Push(BLK_UpdateBsInfo
        , [this](StorageW& /*global*/, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        auto& bs = *task.pBsOut;

        bs.TimeStamp = task.pSurfIn->Data.TimeStamp;
        bs.DecodeTimeStamp = bs.TimeStamp; // no support of reordering so far

        bs.PicStruct = task.pSurfIn->Info.PicStruct;
        bs.FrameType = task.FrameType;

        *task.pBsDataLength += task.BsDataLength;

        return MFX_ERR_NONE;
    });
}

inline bool ReleaseResource(IAllocation& a, Resource& r)
{
    if (r.Mid)
    {
        a.Release(r.Idx);
        r = Resource();
        return true;
    }

    return r.Idx == IDX_INVALID;
}

void General::FreeTask(const FeatureBlocks& /*blocks*/, TPushFT Push)
{
    Push(BLK_FreeTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        auto& core = Glob::VideoCore::Get(global);

        ThrowAssert(
            !ReleaseResource(Glob::AllocBS::Get(global), task.BS)
            , "task.BS resource is invalid");
        ThrowAssert(
            global.Contains(Glob::AllocRaw::Key)
            && !ReleaseResource(Glob::AllocRaw::Get(global), task.Raw)
            , "task.Raw resource is invalid");

        SetIf(task.pSurfIn, task.pSurfIn && !core.DecreaseReference(&task.pSurfIn->Data), nullptr);
        ThrowAssert(!!task.pSurfIn, "failed in core.DecreaseReference");

        auto& atrRec = Glob::AllocRec::Get(global);

        // TODO: release recon based of refresh_frame_flags
        ThrowAssert(
            !IsRef(task.FrameType)
            && !ReleaseResource(atrRec, task.Rec)
            , "task.Rec resource is invalid");

        task.DPB.clear();

        return MFX_ERR_NONE;
    });
}

void General::GetVideoParam(const FeatureBlocks& blocks, TPushGVP Push)
{
    Push(BLK_CopyConfigurable
        , [this, &blocks](mfxVideoParam& out, StorageR& global) -> mfxStatus
    {
        return CopyConfigurable(blocks, Glob::VideoParam::Get(global), out);
    });
}

void General::Close(const FeatureBlocks& blocks, TPushCLS Push)
{
    Push(BLK_Close
        , [this, &blocks](
            StorageW& /*global*/) -> mfxStatus
        {
            m_prevTask.DPB.clear();
            return MFX_ERR_NONE;
        });
}

using DpbIndexes = std::vector<mfxU8>;

static void FillSortedFwdBwd(
    const TaskCommonPar& task
    , DpbIndexes* fwd
    , DpbIndexes* bwd)
{
    if (!fwd && !bwd)
        return;

    using DisplayOrderToDPBIndex = std::map<mfxI32, mfxU8>;
    using Ref = DisplayOrderToDPBIndex::const_reference;
    auto GetIdx = [](Ref ref) {return ref.second; };
    auto IsBwd = [=](Ref ref) {return ref.first > task.DisplayOrderInGOP; };

    DisplayOrderToDPBIndex uniqueRefs;
    for (mfxU8 refIdx = 0; refIdx < task.DPB.size(); refIdx++)
    {
        auto& refFrm = task.DPB.at(refIdx);
        if (refFrm)
            uniqueRefs.insert({ refFrm->DisplayOrderInGOP, refIdx });
    }
    uniqueRefs.erase(task.DisplayOrderInGOP);

    auto firstBwd = find_if(uniqueRefs.begin(), uniqueRefs.end(), IsBwd);

    if (fwd)
        std::transform(uniqueRefs.begin(), firstBwd, std::back_inserter(*fwd), GetIdx);

    if (bwd)
        std::transform(firstBwd, uniqueRefs.end(), std::back_inserter(*bwd), GetIdx);
}

namespace RefListRules
{
    template <typename Iter>
    using Rules = std::list<std::pair<size_t, Iter>>;

    template<typename Iter>
    class SafeIncrement
    {
        const Iter curr;
        const Iter end;
    public:
        SafeIncrement(const Iter& _curr, const Iter& _end) : curr(_curr), end(_end)
        {
            // the class is not intended to work with reference types
            assert(!std::is_reference<Iter>::value);
        };

        Iter operator+(size_t inc) const
        {
            const size_t remaining = std::distance(curr, end);
            const size_t safeInc = std::min(inc, remaining);
            Iter iter = curr;
            std::advance(iter, safeInc);
            return iter;
        }

        Iter operator()() const { return curr; }
    };

    template<typename Iter>
    static void CleanRules(Rules<Iter>& rules, const Iter toRemove, mfxU8 maxRefs)
    {
        auto NeedRemove = [&toRemove](const auto& rule) { return rule.second == toRemove; };
        rules.remove_if(NeedRemove);

        if (rules.size() > maxRefs)
            rules.resize(maxRefs);
    }

    template<typename Iter>
    static void ApplyRules(const Rules<Iter>& rules, RefListType& refList)
    {
        std::array<mfxU8, NUM_REF_FRAMES> usedDpbSlots = {};

        auto ApplyRule = [&](const auto& rule)
        {
            const mfxU8 dpbIdx = *rule.second;
            if (!usedDpbSlots.at(dpbIdx))
            {
                refList.at(rule.first - LAST_FRAME) = dpbIdx;
                usedDpbSlots.at(dpbIdx) = 1;
            }
        };

        for_each(rules.begin(), rules.end(), ApplyRule);
    }
}

#define APPLY_HW_REF_LIST_RESTRICTIONS
static void FillFwdPart(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList, bool isLdbFrame= false)
{
    // logic below is same as in original GetRefList implementation (for compatibility reasons)
    // TODO: consider improvement of this logic
    //       e.g make it closer to logic from "7.8. Set frame refs process"

    using Iter = DpbIndexes::const_reverse_iterator;
    const RefListRules::SafeIncrement<Iter> closestRef{ fwd.crbegin() , fwd.crend() };
    RefListRules::Rules<Iter> constructionRules = {};

#ifdef APPLY_HW_REF_LIST_RESTRICTIONS
    if (isLdbFrame)
    {
        constructionRules = {
            {LAST_FRAME,   closestRef()},
            {LAST2_FRAME,  closestRef + 1},
            {LAST3_FRAME,  closestRef + 2},
        };
    }
    else
    {
        constructionRules = {
            {LAST_FRAME,   closestRef()},
            {GOLDEN_FRAME, closestRef + 1},
            {ALTREF_FRAME, closestRef + 2}
        };
    }

#else
    RefListRules::Rules<Iter> constructionRules = {
        {LAST_FRAME,   closestRef()},
        {LAST2_FRAME,  closestRef + 1},
        {LAST3_FRAME,  closestRef + 2},
        {GOLDEN_FRAME, closestRef + 3}
    };
#endif

    const mfxU8 maxRefs = std::max(maxFwdRefs, static_cast<mfxU8>(fwd.size()));
    RefListRules::CleanRules(constructionRules, fwd.crend(), maxRefs);
    RefListRules::ApplyRules(constructionRules, refList);
}

static void FillRefListP(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList)
{
    assert(!fwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList);
}

static void FillBwdPart(const DpbIndexes& bwd, mfxU8 maxBwdrefs, RefListType& refList)
{
    using Iter = DpbIndexes::const_iterator;

#ifdef APPLY_HW_REF_LIST_RESTRICTIONS
    RefListRules::Rules<Iter> constructionRules = {
        {BWDREF_FRAME,  bwd.cbegin()}
    };
#else
    // for ALTREF_FRAME, BWDREF_FRAME and ALTREF2_FRAME
    // use same logic as in AV1 spec "7.8. Set frame refs process"

    const RefListRules::SafeIncrement<Iter> closestRef{ bwd.cbegin() , bwd.cend() };

    RefListRules::Rules<Iter> constructionRules = {
        {ALTREF_FRAME,  bwd.cend() - 1},
        {BWDREF_FRAME,  closestRef()},
        {ALTREF2_FRAME, closestRef + 1}
    };
#endif

    const mfxU8 maxRefs = std::min(maxBwdrefs, static_cast<mfxU8>(bwd.size()));
    RefListRules::CleanRules(constructionRules, bwd.cend(), maxRefs);
    RefListRules::ApplyRules(constructionRules, refList);
}

static void FillRefListRAB(
    const DpbIndexes& fwd
    , mfxU8 maxFwdRefs
    , const DpbIndexes& bwd
    , mfxU8 maxBwdRefs
    , RefListType& refList)
{
    assert(!fwd.empty());
    assert(!bwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList);
    FillBwdPart(bwd, maxBwdRefs, refList);
}

static void FillRefListLDB(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList)
{
    assert(!fwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList, true);

#ifdef APPLY_HW_REF_LIST_RESTRICTIONS
    refList.at(BWDREF_FRAME - LAST_FRAME) = refList.at(LAST_FRAME - LAST_FRAME);
    refList.at(ALTREF2_FRAME - LAST_FRAME) = refList.at(LAST2_FRAME - LAST_FRAME);
    refList.at(ALTREF_FRAME - LAST_FRAME) = refList.at(LAST3_FRAME - LAST_FRAME);
#endif
}

inline std::tuple<mfxU8, mfxU8> GetMaxRefs(
    const TaskCommonPar& task
    , const mfxVideoParam& par)
{
    const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);

    const mfxU8 maxFwdRefs = static_cast<mfxU8>(IsB(task.FrameType) ?
        CO3.NumRefActiveBL0[0] : CO3.NumRefActiveP[0]);
    const mfxU8 maxBwdRefs = static_cast<mfxU8>(CO3.NumRefActiveBL1[0]);

    return std::make_tuple(maxFwdRefs, maxBwdRefs);
}

inline void SetTaskQp(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    if (IsB(task.FrameType))
        task.QpY = static_cast<mfxU8>(par.mfx.QPB);
    else if (IsP(task.FrameType))
        task.QpY = static_cast<mfxU8>(par.mfx.QPP);
    else
    {
        assert(IsI(task.FrameType));
        task.QpY = static_cast<mfxU8>(par.mfx.QPI);
    }

    SetIf(task.QpY, !!task.ctrl.QP, static_cast<mfxU8>(task.ctrl.QP));
}

inline void SetTaskBRCParams(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (!pAuxPar || par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        return;

    task.MinBaseQIndex = pAuxPar->QP.MinBaseQIndex;
    task.MaxBaseQIndex = pAuxPar->QP.MaxBaseQIndex;
}

inline void SetTaskEncodeOrders(
    TaskCommonPar& task
    , const TaskCommonPar& prevTask)
{
    task.EncodedOrder = prevTask.EncodedOrder + 1;

    if (IsI(task.FrameType))
    {
        task.EncodedOrderInGOP = 0;
        task.RefOrderInGOP     = 0;
    }
    else
    {
        task.EncodedOrderInGOP = prevTask.EncodedOrderInGOP + 1;
        task.RefOrderInGOP = IsRef(task.FrameType) ?
            prevTask.RefOrderInGOP + 1 :
            prevTask.RefOrderInGOP;
    }
}

inline void InitTaskDPB(
    TaskCommonPar& task,
    TaskCommonPar& prevTask)
{
    assert(task.DPB.size() >= prevTask.DPB.size());
    std::move(prevTask.DPB.begin(), prevTask.DPB.end(), task.DPB.begin());
}

// task - [in/out] Current task object, RefList field will be set in place
// par - [in] mfxVideoParam
// Return - N/A
inline void SetTaskRefList(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    auto& refList = task.RefList;
    std::fill_n(refList.begin(), REFS_PER_FRAME, IDX_INVALID);

    if (IsI(task.FrameType))
        return;

    DpbIndexes fwd;
    DpbIndexes bwd;
    FillSortedFwdBwd(task, &fwd, &bwd);

    mfxU8 maxFwdRefs, maxBwdRefs;
    std::tie(maxFwdRefs, maxBwdRefs) = GetMaxRefs(task, par);

    if (IsP(task.FrameType))
    {
        if (task.isLDB)
            FillRefListLDB(fwd, maxFwdRefs, refList);
        else
            FillRefListP(fwd, maxFwdRefs, refList);
    }
    else
        FillRefListRAB(fwd, maxFwdRefs, bwd, maxBwdRefs, refList);
}

// task - [in/out] Current task object, RefreshFrameFlags field will be set in place
// Return - N/A
inline void SetTaskDPBRefresh(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    auto& refreshRefFrames = task.RefreshFrameFlags;

    if (IsI(task.FrameType))
        std::fill(refreshRefFrames.begin(), refreshRefFrames.end(), static_cast<mfxU8>(1));
    else if (IsRef(task.FrameType))
    {
        // round robin DPB update
        refreshRefFrames = {};
        const mfxU16 slotToRefresh = task.RefOrderInGOP % par.mfx.NumRefFrame;
        refreshRefFrames.at(slotToRefresh) = 1;
    }
}

namespace CModelRefLogic
{
    const mfxU8 maxBGOPSize   = 8;
    const mfxU8 maxRefsListed = 8;

    struct RefStructureDef
    {
        const mfxU8 BGOPSize;
        const mfxU8 refsListed;
        const mfxI8 PerBFrameRefPics[maxBGOPSize][maxRefsListed];
    };

    const RefStructureDef IPPPP =
    {
        4, // BGOPSize
        4, // refsListed
        {
            {-1, -5, -9, -13},
            {-1, -2, -6, -10},
            {-1, -3, -7, -11},
            {-1, -4, -8, -12}
        }
    };

    const RefStructureDef IbbbP =
    {
        4, // BGOPSize
        2, // refsListed
        {
            {-4, -8},
            {-1,  3},
            {-2,  2},
            {-3,  1}
        }
    };

    using DisplayOrders = std::vector<mfxI32>;

    class UseRef
    {
        const DisplayOrders& usedRefs;
        const DpbType& dpb;
    public:
        UseRef(const DpbType& _dpb, const DisplayOrders& orders)
            : usedRefs(orders), dpb(_dpb) {}

        bool operator()(mfxI32 dpbIdx)
        {
            const mfxI32 dispOrder = dpb.at(dpbIdx)->DisplayOrderInGOP;
            return std::find(usedRefs.begin(), usedRefs.end(), dispOrder) != usedRefs.end();
        }
    };

    static void FillBwdPart(const DpbIndexes& bwd, mfxU8 maxBwdRefs, RefListType& refList)
    {
        auto ref = bwd.cbegin();
        mfxU8 refListIdx = BWDREF_FRAME - LAST_FRAME;

        while (ref != bwd.cend() && refListIdx < (maxBwdRefs + BWDREF_FRAME))
            refList.at(refListIdx++) = *ref++;
    }

    static RefListType GetRefList(
        const TaskCommonPar & task
        , const mfxVideoParam& par
        , const DpbIndexes& fwd
        , const DpbIndexes& bwd
        , const DisplayOrders& refDisplayOrders)
    {
        RefListType refList = {};
        std::fill_n(refList.begin(), REFS_PER_FRAME, IDX_INVALID);

        if (IsI(task.FrameType))
            return refList;

        mfxU8 maxFwdRefs, maxBwdRefs;
        std::tie(maxFwdRefs, maxBwdRefs) = GetMaxRefs(task, par);

        DpbIndexes fwdToUse;
        DpbIndexes bwdToUse;
        const DpbType& dpb = task.DPB;

        std::copy_if(fwd.begin(), fwd.end(), std::back_inserter(fwdToUse), UseRef(dpb, refDisplayOrders));
        std::copy_if(bwd.begin(), bwd.end(), std::back_inserter(bwdToUse), UseRef(dpb, refDisplayOrders));

        FillFwdPart(fwdToUse, maxFwdRefs, refList);
        FillBwdPart(bwdToUse, maxBwdRefs, refList);

        return refList;
    }

    static DpbRefreshType GetDPBRefresh(
        const DpbFrame& task
        , const DpbType& dpb
        , const ExtBuffer::Param<mfxVideoParam>& par
        , const DpbIndexes& fwd
        , const DpbIndexes& bwd
        , const DisplayOrders& refDisplayOrders)
    {
        std::array<mfxU8, NUM_REF_FRAMES> refreshRefFrames = {};
        if (IsI(task.FrameType))
            refreshRefFrames = { 1, 1, 1, 1, 1, 1, 1, 1 };
        else if (IsRef(task.FrameType))
        {
            if (fwd.size() + bwd.size() < NUM_REF_FRAMES)
            {
                // round robin DPB update until DPB is filled with unique refs
                const mfxU16 slotToOverwrite = task.RefOrderInGOP % par.mfx.NumRefFrame;
                refreshRefFrames.at(slotToOverwrite) = 1;
            }
            else
            {
                // if DPB is already filled with unique refs
                // overwrite oldest unused ref in DPB
                DpbIndexes unusedFwd;
                DpbIndexes unusedBwd;

                std::remove_copy_if(fwd.begin(), fwd.end(), std::back_inserter(unusedFwd), UseRef(dpb, refDisplayOrders));
                std::remove_copy_if(bwd.begin(), bwd.end(), std::back_inserter(unusedBwd), UseRef(dpb, refDisplayOrders));

                if (!unusedFwd.empty())
                    refreshRefFrames.at(unusedFwd.front()) = 1;
                else if (!unusedBwd.empty())
                    refreshRefFrames.at(unusedBwd.front()) = 1;
            }
        }

        return refreshRefFrames;
    }

    inline DisplayOrders GetRefDisplayOrders(const mfxVideoParam& par, mfxU32 currEncodedOrderInGOP, mfxI32 currDisplayOrderInGOP)
    {
        assert(currDisplayOrderInGOP);
        assert(par.mfx.GopRefDist == 1 || par.mfx.GopRefDist == 4);

        const RefStructureDef& refStructure = par.mfx.GopRefDist == 1 ? IPPPP : IbbbP;

        mfxU8 orderInBGOP = (currEncodedOrderInGOP - 1) % refStructure.BGOPSize;

        DisplayOrders orders = {};

        auto GetOrderFromDelta = [currDisplayOrderInGOP](auto delta) { return currDisplayOrderInGOP + delta; };

        auto deltas = &(refStructure.PerBFrameRefPics[orderInBGOP][0]);

        std::transform(deltas, deltas + refStructure.refsListed, std::back_inserter(orders), GetOrderFromDelta);

        return orders;
    }

    static void SetRefListAndDPBRefresh(
        TaskCommonPar& task
        , const mfxVideoParam& par)
    {
        DpbIndexes fwd;
        DpbIndexes bwd;
        DisplayOrders refDisplayOrders;

        RefListType& refList = task.RefList;
        DpbRefreshType& refreshRefFrames = task.RefreshFrameFlags;
        if (!IsI(task.FrameType))
        {
            FillSortedFwdBwd(task, &fwd, &bwd);
            refDisplayOrders = GetRefDisplayOrders(par, task.EncodedOrderInGOP, task.DisplayOrderInGOP);
        }

        refList = GetRefList(task, par, fwd, bwd, refDisplayOrders);
        refreshRefFrames = GetDPBRefresh(task, task.DPB, par, fwd, bwd, refDisplayOrders);
    }
}

class DpbFrameReleaser
{
    IAllocation& pool;
public:
    DpbFrameReleaser(IAllocation& _pool) : pool(_pool) {};
    void operator()(DpbFrame* pFrm)
    {
        ReleaseResource(pool, pFrm->Rec);
        delete pFrm;
    }
};

inline bool IsHidden(const FrameBaseInfo& frame)
{
    return frame.NextBufferedDisplayOrder != -1 &&
        frame.DisplayOrderInGOP > frame.NextBufferedDisplayOrder;
}

inline void SetTaskRepeatedFrames(
    TaskCommonPar& task)
{
    task.FramesToShow.clear();
    if (task.NextBufferedDisplayOrder == -1)
        return;

    DpbIndexes bwd;
    FillSortedFwdBwd(task, nullptr, &bwd);

    for (auto refIdx : bwd)
    {
        auto& refFrm = task.DPB.at(refIdx);
        if (refFrm->DisplayOrderInGOP < task.NextBufferedDisplayOrder && !refFrm->wasShown)
        {
            task.FramesToShow.push_back({ refIdx, refFrm->DisplayOrder });
            refFrm->wasShown = true;
        }
    }
}

inline void SetTaskIVFHeaderInsert(
    TaskCommonPar& task
    , const ExtBuffer::Param<mfxVideoParam>& par)
{
    const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    //WriteIVFHeaders should be enabled if pAV1Par is nullptr
    if (pAV1Par && !IsOn(pAV1Par->WriteIVFHeaders))
        return;

    if (task.EncodedOrder == 0)
        task.InsertHeaders |= INSERT_IVF_SEQ;

    task.InsertHeaders |= INSERT_IVF_FRM;
}

inline void SetTaskInsertHeaders(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    SetTaskIVFHeaderInsert(task, par);

    if (IsI(task.FrameType))
        task.InsertHeaders |= INSERT_SPS;

    task.InsertHeaders |= INSERT_PPS;

    if (!task.FramesToShow.empty())
        task.InsertHeaders |= INSERT_REPEATED;

    const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    if (pAV1Par && IsOn(pAV1Par->PackOBUFrame))
        task.InsertHeaders |= INSERT_FRM_OBU;
}

void General::ConfigureTask(
    TaskCommonPar& task
    , const Defaults::Param& dflts
    , const SH& /*sh*/
    , IAllocation& recPool)
{
    task.StatusReportId = std::max<mfxU32>(1, m_prevTask.StatusReportId + 1);

    const auto& par = dflts.mvp;
    SetTaskQp(task, par);
    SetTaskBRCParams(task, par);
    SetTaskEncodeOrders(task, m_prevTask);

    InitTaskDPB(task, m_prevTask);
    if (IsCModelMode(par))
        CModelRefLogic::SetRefListAndDPBRefresh(task, par);
    else
    {
        SetTaskRefList(task, par);
        SetTaskDPBRefresh(task, par);
    }
    SetTaskRepeatedFrames(task);

    SetTaskInsertHeaders(task, par);

    if (!IsHidden(task))
        task.wasShown = true;

    m_prevTask = task;

    UpdateDPB(m_prevTask.DPB, reinterpret_cast<DpbFrame&>(task), task.RefreshFrameFlags, DpbFrameReleaser(recPool));
}

static bool HaveL1(DpbType const & dpb, mfxI32 displayOrderInGOP)
{
    return std::any_of(dpb.begin(), dpb.end(),
        [displayOrderInGOP](DpbType::const_reference frm)
        {
            if (frm)
                return frm->DisplayOrderInGOP > displayOrderInGOP;
            else
                return false;
        });
}

template<class T>
static T GetFirstFrameToDisplay(
    T begin
    , T end
    , T cur)
{
    // TODO: consider implementation of this logic in post-reordering stage
    //       in this case removal of current frame from reorder list will not be required

    const size_t framesInBuffer = std::distance(begin, end);

    if (framesInBuffer < 2)
        return end;

    std::list<T> exceptCur;
    T top = begin;

    std::generate_n(
        std::back_inserter(exceptCur)
        , framesInBuffer
        , [&]() { return top++; });

    exceptCur.remove(cur);

    const auto firstToDisplay = std::min_element(
        exceptCur.begin(),
        exceptCur.end(),
        [](T& a, T& b) { return a->DisplayOrderInGOP < b->DisplayOrderInGOP; });

    return *firstToDisplay;
}

template<class T>
static T Reorder(
    ExtBuffer::Param<mfxVideoParam> const & /*par*/
    , DpbType const & dpb
    , T begin
    , T end
    , bool flush)
{
    typedef typename std::iterator_traits<T>::reference TRef;

    T top = begin;
    T reorderOut = top;
    std::list<T> brefs;
    auto IsB = [](TRef f) { return AV1EHW::IsB(f.FrameType); };
    auto NoL1 = [&](T& f) { return !HaveL1(dpb, f->DisplayOrderInGOP); };

    std::generate_n(
        std::back_inserter(brefs)
        , std::distance(begin, std::find_if_not(begin, end, IsB))
        , [&]() { return top++; });

    brefs.remove_if(NoL1);

    if (!brefs.empty())
    {
        // TODO: implement proper behavior for reference B-frames
        reorderOut = brefs.front();
    }
    else
    {
        // optimize end of GOP or end of sequence
        bool bForcePRef = flush && top == end && begin != end;
        if (bForcePRef)
        {
            --top;
            top->FrameType = mfxU16(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
        }

        reorderOut = top;
    }

    T firstToDisplay = GetFirstFrameToDisplay(begin, end, reorderOut);
    if (firstToDisplay != end)
        reorderOut->NextBufferedDisplayOrder = firstToDisplay->DisplayOrderInGOP;
    else if (flush)
    {
        // need to show all hidden frames before end of GOP or end of sequence
        reorderOut->NextBufferedDisplayOrder = std::numeric_limits<mfxI32>::max();
    }

    return reorderOut;
}

TTaskIt General::ReorderWrap(
    ExtBuffer::Param<mfxVideoParam> const & par
    , TTaskIt begin
    , TTaskIt end
    , bool flush)
{
    typedef TaskItWrap<FrameBaseInfo, Task::Common::Key> TItWrap;
    TItWrap newEnd(begin);

    while (newEnd != end)
    {
        if (newEnd != begin && IsIdr(newEnd->FrameType))
        {
            flush = true;
            break;
        }
        newEnd++;
    }

    return Reorder(par, m_prevTask.DPB, TItWrap(begin), newEnd, flush).it;
}

mfxU32 General::GetMinBsSize(
    const mfxVideoParam & par
    , const mfxExtAV1Param& AV1Param
    , const mfxExtCodingOption3& CO3)
{
    // TODO: it's more correct to use sizes aligned to Mi block size
    mfxU32 size = AV1Param.FrameWidth * AV1Param.FrameHeight;

    SetDefault(size, par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height);

    bool b10bit = (CO3.TargetBitDepthLuma == 10);
    bool b422   = (CO3.TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV422 + 1));
    bool b444   = (CO3.TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV444 + 1));

    mfxF64 k = 2.0
        + (b10bit * 0.3)
        + (b422   * 0.5)
        + (b444   * 1.5);

    size = mfxU32(k * size);

    return size;
}

bool General::GetRecInfo(
    const mfxVideoParam& par
    , const mfxExtCodingOption3* pCO3
    , eMFXHWType hw
    , mfxFrameInfo& rec)
{
    static const std::map<mfxU16, std::function<void(mfxFrameInfo&, eMFXHWType)>> ModRec[2] =
    {
        { //8b
            {
                mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                , [](mfxFrameInfo& rec, eMFXHWType)
                {
                    rec.FourCC = MFX_FOURCC_NV12;
                }
            }
        }
        , { //10b
            {
                mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                , [](mfxFrameInfo& rec, eMFXHWType)
                {
                    //P010
                    rec.FourCC = MFX_FOURCC_NV12;
                    rec.Width = mfx::align2_value(rec.Width, 32) * 2; //This is require by HW and MMC, which is same as TGL.
                }
            }
        }
    };
    rec = par.mfx.FrameInfo;

    if (!pCO3)
        return true;

    auto& rModRec  = ModRec[pCO3->TargetBitDepthLuma == 10];
    auto  itModRec = rModRec.find(pCO3->TargetChromaFormatPlus1);
    bool bUndef =
        (pCO3->TargetBitDepthLuma != 8 && pCO3->TargetBitDepthLuma != 10)
        || (itModRec == rModRec.end());

    if (bUndef)
    {
        assert(!"undefined target format");
        return false;
    }

    itModRec->second(rec, hw);

    rec.ChromaFormat   = pCO3->TargetChromaFormatPlus1 - 1;
    rec.BitDepthLuma   = pCO3->TargetBitDepthLuma;
    rec.BitDepthChroma = pCO3->TargetBitDepthChroma;

    return true;
}

inline mfxU16 MapMfxProfileToSpec(mfxU16 profile)
{
    switch (profile)
    {
    case MFX_PROFILE_AV1_MAIN:
        return 0;
    case MFX_PROFILE_AV1_HIGH:
        return 1;
    case MFX_PROFILE_AV1_PRO:
        return 2;
    default:
        return 0;
    }
}

void General::SetSH(
    const ExtBuffer::Param<mfxVideoParam>& par
    , eMFXHWType /*hw*/
    , const EncodeCapsAv1& caps
    , SH& sh)
{

    const mfxExtAV1Param* pAV1Par   = ExtBuffer::Get(par);
    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);

    assert(pAV1Par); // all ext buffers must be fpresent at this stage
    assert(pAuxPar); // all ext buffers must be fpresent at this stage

    sh = {};

    sh.seq_profile =   MapMfxProfileToSpec(par.mfx.CodecProfile);
    sh.still_picture = CO2Flag(pAV1Par->StillPictureMode);

    const int maxFrameResolutionBits = 15;
    sh.frame_width_bits       = maxFrameResolutionBits;
    sh.frame_height_bits      = maxFrameResolutionBits;
    sh.sbSize                 = SB_SIZE;
    sh.enable_order_hint      = CO2Flag(pAuxPar->EnableOrderHint);
    sh.order_hint_bits_minus1 = pAuxPar->OrderHintBits - 1;
    sh.enable_superres        = CO2Flag(pAV1Par->EnableSuperres);
    sh.enable_cdef            = CO2Flag(pAV1Par->EnableCdef);
    sh.enable_restoration     = CO2Flag(pAV1Par->EnableRestoration);

    // Below fields will directly use setting from caps.
    sh.enable_dual_filter         = caps.AV1ToolSupportFlags.fields.enable_dual_filter;
    sh.enable_filter_intra        = caps.AV1ToolSupportFlags.fields.enable_filter_intra;
    sh.enable_interintra_compound = caps.AV1ToolSupportFlags.fields.enable_interintra_compound;
    sh.enable_intra_edge_filter   = caps.AV1ToolSupportFlags.fields.enable_intra_edge_filter;
    sh.enable_jnt_comp            = caps.AV1ToolSupportFlags.fields.enable_jnt_comp;
    sh.enable_masked_compound     = caps.AV1ToolSupportFlags.fields.enable_masked_compound;

    const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
    assert(pCO3); // all ext buffers must be fpresent at this stage

    sh.color_config.BitDepth            = pCO3->TargetBitDepthLuma;
    sh.color_config.color_range         = 1; // full swing representation
    sh.color_config.separate_uv_delta_q = 1; // NB: currently driver not work if it's '0'
}

inline INTERP_FILTER MapMfxInterpFilter(mfxU16 filter)
{
    assert(filter); // default value must be set

    switch (filter)
    {
    case MFX_AV1_INTERP_EIGHTTAP_SMOOTH:
        return EIGHTTAP_SMOOTH;
    case MFX_AV1_INTERP_EIGHTTAP_SHARP:
        return EIGHTTAP_SHARP;
    case MFX_AV1_INTERP_BILINEAR:
        return BILINEAR;
    case MFX_AV1_INTERP_SWITCHABLE:
        return SWITCHABLE;
    case MFX_AV1_INTERP_EIGHTTAP:
    default:
        return EIGHTTAP_REGULAR;
    }
}

void General::SetFH(
    const ExtBuffer::Param<mfxVideoParam>& par
    , eMFXHWType /*hw*/
    , const SH& sh
    , FH& fh)
{
    // this functions sets "static" parameters which can be changed via Reset

    fh = {};

    const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);

    assert(pAV1Par); // all ext buffers must be present at this stage
    assert(pAuxPar); // all ext buffers must be present at this stage

    fh.FrameWidth                   = pAV1Par->FrameWidth;
    fh.FrameHeight                  = pAV1Par->FrameHeight;
    fh.error_resilient_mode         = CO2Flag(pAuxPar->ErrorResilientMode);
    fh.disable_cdf_update           = CO2Flag(pAV1Par->DisableCdfUpdate);
    fh.interpolation_filter         = MapMfxInterpFilter(pAV1Par->InterpFilter);
    fh.RenderWidth                  = pAV1Par->FrameWidth;
    fh.RenderHeight                 = pAV1Par->FrameHeight;
    fh.disable_frame_end_update_cdf = CO2Flag(pAV1Par->DisableFrameEndUpdateCdf);
    fh.allow_high_precision_mv      = 1;

    fh.quantization_params.DeltaQYDc = pAuxPar->QP.YDcDeltaQ;
    fh.quantization_params.DeltaQUDc = pAuxPar->QP.UDcDeltaQ;
    fh.quantization_params.DeltaQUAc = pAuxPar->QP.UAcDeltaQ;
    fh.quantization_params.DeltaQVDc = pAuxPar->QP.VDcDeltaQ;
    fh.quantization_params.DeltaQVAc = pAuxPar->QP.VAcDeltaQ;

    //TODO: programming default loop filter levels
    fh.loop_filter_params.loop_filter_level[0]      = pAuxPar->LoopFilter.LFLevelYVert;
    fh.loop_filter_params.loop_filter_level[1]      = pAuxPar->LoopFilter.LFLevelYHorz;
    fh.loop_filter_params.loop_filter_level[2]      = pAuxPar->LoopFilter.LFLevelU;
    fh.loop_filter_params.loop_filter_level[3]      = pAuxPar->LoopFilter.LFLevelV;
    fh.loop_filter_params.loop_filter_sharpness     = pAV1Par->LoopFilterSharpness;
    fh.loop_filter_params.loop_filter_delta_enabled = pAuxPar->LoopFilter.ModeRefDeltaEnabled;
    fh.loop_filter_params.loop_filter_delta_update  = pAuxPar->LoopFilter.ModeRefDeltaUpdate;

    std::copy_n(pAuxPar->LoopFilter.RefDeltas,
        TOTAL_REFS_PER_FRAME,
        fh.loop_filter_params.loop_filter_ref_deltas);

    std::copy_n(pAuxPar->LoopFilter.ModeDeltas,
        2,
        fh.loop_filter_params.loop_filter_mode_deltas);

    if (sh.enable_restoration)
    {
        // TODO: copy loop restoration params here
    }

    fh.TxMode = TX_MODE_SELECT;
    fh.reduced_tx_set = 1;
    fh.delta_lf_present = 1;
    fh.delta_lf_multi = 1;
}

void SetDefaultFormat(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption3* pCO3)
{
    auto& fi = par.mfx.FrameInfo;

    assert(fi.FourCC);

    SetDefault(fi.BitDepthLuma, [&]() { return defPar.base.GetMaxBitDepth(defPar); });
    SetDefault(fi.BitDepthChroma, fi.BitDepthLuma);

    if (pCO3)
    {
        pCO3->TargetChromaFormatPlus1 = defPar.base.GetTargetChromaFormat(defPar);
        pCO3->TargetBitDepthLuma = defPar.base.GetTargetBitDepthLuma(defPar);
        SetDefault(pCO3->TargetBitDepthChroma, pCO3->TargetBitDepthLuma);
    }
}

void SetDefaultGOP(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption2* pCO2
    , mfxExtCodingOption3* pCO3)
{
    par.mfx.GopPicSize = defPar.base.GetGopPicSize(defPar);
    par.mfx.GopRefDist = defPar.base.GetGopRefDist(defPar);

    SetIf(pCO2->BRefType, pCO2 && !pCO2->BRefType, [&]() { return defPar.base.GetBRefType(defPar); });
    SetIf(pCO3->PRefType, pCO3 && !pCO3->PRefType, [&]() { return defPar.base.GetPRefType(defPar); });

    SetDefault(par.mfx.NumRefFrame, defPar.base.GetNumRefFrames(defPar));

    if (pCO3)
    {
        SetDefault<mfxU16>(pCO3->GPB, MFX_CODINGOPTION_OFF);

        defPar.base.GetNumRefActive(
            defPar
            , &pCO3->NumRefActiveP
            , &pCO3->NumRefActiveBL0
            , &pCO3->NumRefActiveBL1);
    }
}

void SetDefaultBRC(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption2* /*pCO2*/
    , mfxExtCodingOption3* /*pCO3*/)
{
    par.mfx.RateControlMethod = defPar.base.GetRateControlMethod(defPar);
    BufferSizeInKB(par.mfx) = defPar.base.GetBufferSizeInKB(defPar);

    bool bSetQP = par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && !(par.mfx.QPI && par.mfx.QPP && par.mfx.QPB);

    if (bSetQP)
    {
        std::tie(par.mfx.QPI, par.mfx.QPP, par.mfx.QPB) = defPar.base.GetQPMFX(defPar);
    }

    SetDefault(par.mfx.BRCParamMultiplier, 1);
}

inline void SetDefaultOrderHint(mfxExtAV1AuxData* par)
{
    if (!par)
        return;

    SetDefault(par->EnableOrderHint, MFX_CODINGOPTION_ON);
    if (IsOn(par->EnableOrderHint))
    {
        SetDefault(par->OrderHintBits, 8);
    }
}

void General::SetDefaults(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , bool bExternalFrameAllocator)
{
    mfxU16 IOPByAlctr[2] = { MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_IOPATTERN_IN_VIDEO_MEMORY };
    SetDefault(par.AsyncDepth, defPar.base.GetAsyncDepth(defPar));
    SetDefault(par.IOPattern, IOPByAlctr[!!bExternalFrameAllocator]);

    mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

    SetDefault(par.mfx.CodecProfile, defPar.base.GetProfile(defPar));
    SetDefault(par.mfx.CodecLevel, 0);
    SetDefault(par.mfx.TargetUsage, MFX_TARGETUSAGE_BALANCED);
    SetDefaultGOP(par, defPar, nullptr, pCO3);
    SetDefault(par.mfx.LowPower, MFX_CODINGOPTION_ON);
    SetDefault(par.mfx.NumThread, 1);

    mfxFrameInfo &fi = par.mfx.FrameInfo;

    // fi.Width, fi.Height MUST be set when entering General::SetDefaults
    // TODO: implement respective checks
    SetDefault(fi.CropW, fi.Width);
    SetDefault(fi.CropH, fi.Height);

    mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    if (pAV1Par != nullptr)
    {
        SetDefaultFrameInfo(pAV1Par->FrameWidth, pAV1Par->FrameHeight, fi);
    }


    SetDefault(fi.AspectRatioW, mfxU16(1));
    SetDefault(fi.AspectRatioH, mfxU16(1));

    std::tie(fi.FrameRateExtN, fi.FrameRateExtD) = defPar.base.GetFrameRate(defPar);

    SetDefaultBRC(par, defPar, nullptr, nullptr);

    SetDefault(fi.PicStruct, MFX_PICSTRUCT_PROGRESSIVE);

    SetDefaultFormat(par, defPar, pCO3);

    if (pAV1Par)
    {
        SetDefault(pAV1Par->WriteIVFHeaders, MFX_CODINGOPTION_ON);
        SetDefault(pAV1Par->UseAnnexB, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->PackOBUFrame, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->InsertTemporalDelimiter, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->DisableCdfUpdate, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->DisableFrameEndUpdateCdf, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->EnableCdef, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->EnableRestoration, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->EnableSuperres, MFX_CODINGOPTION_OFF);
        SetDefault(pAV1Par->StillPictureMode, MFX_CODINGOPTION_OFF);

        SetDefault(pAV1Par->InterpFilter, MFX_AV1_INTERP_EIGHTTAP);
    }

    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (pAuxPar)
    {
        SetDefault(pAuxPar->LoopFilter.ModeRefDeltaEnabled, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->LoopFilter.ModeRefDeltaUpdate, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->DisplayFormatSwizzle, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->ErrorResilientMode, MFX_CODINGOPTION_OFF);
    }

    SetDefaultOrderHint(pAuxPar);
}

mfxStatus General::CheckNumRefFrame(
    mfxVideoParam & par
    , const Defaults::Param& defPar)
{
    mfxU32 changed = 0;

    changed += CheckMaxOrClip(par.mfx.NumRefFrame, NUM_REF_FRAMES);
    changed += SetIf(
        par.mfx.NumRefFrame
        , (par.mfx.GopRefDist > 1
            && par.mfx.NumRefFrame == 1
            && !defPar.base.GetNonStdReordering(defPar))
        , defPar.base.GetMinRefForBNoPyramid(defPar));

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckFrameRate(mfxVideoParam & par)
{
    auto& fi = par.mfx.FrameInfo;

    if (fi.FrameRateExtN && fi.FrameRateExtD) // FR <= 300
    {
        if (fi.FrameRateExtN > mfxU32(300 * fi.FrameRateExtD))
        {
            fi.FrameRateExtN = fi.FrameRateExtD = 0;
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if ((fi.FrameRateExtN == 0) != (fi.FrameRateExtD == 0))
    {
        fi.FrameRateExtN = 0;
        fi.FrameRateExtD = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

bool General::IsInVideoMem(const mfxVideoParam & par, const mfxExtOpaqueSurfaceAlloc* pOSA)
{
    if (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        return true;

    if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        return (!pOSA || (pOSA->In.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)));

    return false;
}

mfxStatus General::CheckShift(mfxVideoParam & par, mfxExtOpaqueSurfaceAlloc* pOSA)
{
    auto& fi = par.mfx.FrameInfo;
    bool bVideoMem = IsInVideoMem(par, pOSA);

    if (bVideoMem && !fi.Shift)
    {
        if (fi.FourCC == MFX_FOURCC_P010 || fi.FourCC == MFX_FOURCC_P210)
        {
            fi.Shift = 1;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus General::CheckCrops(
    mfxVideoParam & par
    , const Defaults::Param& defPar)
{
    mfxU32 changed = 0;

    auto W = defPar.base.GetCodedPicWidth(defPar);
    auto H = defPar.base.GetCodedPicHeight(defPar);
    auto& fi = par.mfx.FrameInfo;

    changed += CheckMaxOrClip(fi.CropX, W);
    changed += CheckMaxOrClip(fi.CropW, W - fi.CropX);
    changed += CheckMaxOrClip(fi.CropY, H);
    changed += CheckMaxOrClip(fi.CropH, H - fi.CropY);

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

bool CheckBufferSizeInKB(
    mfxVideoParam & par
    , const Defaults::Param& defPar
    , mfxU16 bd)
{
    mfxU32 changed = 0;
    auto W = defPar.base.GetCodedPicWidth(defPar);
    auto H = defPar.base.GetCodedPicHeight(defPar);
    mfxU16 cf = defPar.base.GetTargetChromaFormat(defPar) - 1;

    mfxU32 rawBytes = General::GetRawBytes(W, H, cf, bd) / 1000;

    bool bCqpOrIcq =
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        || par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ;

    bool bSetToRaw = bCqpOrIcq && BufferSizeInKB(par.mfx) < rawBytes;

    if (bSetToRaw)
    {
        BufferSizeInKB(par.mfx) = rawBytes;
        changed++;
    }
    else if (!bCqpOrIcq)
    {
        mfxU32 frN, frD;

        std::tie(frN, frD) = defPar.base.GetFrameRate(defPar);
        mfxU32 avgFS = Ceil((mfxF64)TargetKbps(par.mfx) * frD / frN / 8);

        if (BufferSizeInKB(par.mfx) < avgFS * 2 + 1)
        {
            BufferSizeInKB(par.mfx) = avgFS * 2 + 1;
            changed++;
        }

        if (par.mfx.CodecLevel)
        {
            mfxU32 maxCPB = GetMaxCpbInKBByLevel(par);

            if (BufferSizeInKB(par.mfx) > maxCPB)
            {
                BufferSizeInKB(par.mfx) = maxCPB;
                changed++;
            }
        }
    }

    return !!changed;
}

mfxStatus General::CheckBRC(
    mfxVideoParam & par
    , const Defaults::Param& defPar)
{
    mfxU32 changed = 0;
    mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
    mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        par.mfx.Accuracy = 0;
        par.mfx.Convergence = 0;
        changed++;
    }

    MFX_CHECK(par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ
        || !CheckMaxOrZero(par.mfx.ICQQuality, 51), MFX_ERR_UNSUPPORTED);

    changed += ((par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
        && par.mfx.MaxKbps != 0
        && par.mfx.TargetKbps != 0)
        && CheckMinOrClip(par.mfx.MaxKbps, par.mfx.TargetKbps);

    auto bd = defPar.base.GetTargetBitDepthLuma(defPar);
    auto minQP = defPar.base.GetMinQPMFX(defPar);
    auto maxQP = defPar.base.GetMaxQPMFX(defPar);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        changed += CheckMinOrClip<mfxU16>(par.mfx.QPI, minQP);
        changed += CheckMaxOrClip<mfxU16>(par.mfx.QPI, maxQP);
        changed += CheckMinOrClip<mfxU16>(par.mfx.QPP, minQP);
        changed += CheckMaxOrClip<mfxU16>(par.mfx.QPP, maxQP);
        changed += CheckMinOrClip<mfxU16>(par.mfx.QPB, minQP);
        changed += CheckMaxOrClip<mfxU16>(par.mfx.QPB, maxQP);

        changed += !par.mfx.QPI && (par.mfx.QPP || par.mfx.QPB);
        par.mfx.QPP *= !!par.mfx.QPI;
        par.mfx.QPB *= !!par.mfx.QPI;
    }

    changed += par.mfx.BufferSizeInKB && CheckBufferSizeInKB(par, defPar, bd);

    if (pCO3)
    {
        MFX_CHECK(par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR
            || !CheckMaxOrZero(pCO3->QVBRQuality, 51), MFX_ERR_UNSUPPORTED);
    }

    if (pCO2)
    {
        changed += CheckOrZero<mfxU8>(pCO2->MinQPI, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPI, 0, maxQP);
        changed += CheckOrZero<mfxU8>(pCO2->MinQPP, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPP, 0, maxQP);
        changed += CheckOrZero<mfxU8>(pCO2->MinQPB, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPB, 0, maxQP);
    }

    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (pAuxPar)
    {
        changed += CheckMaxOrClip(pAuxPar->QP.MinBaseQIndex, pAuxPar->QP.MaxBaseQIndex);
        changed += CheckMaxOrClip(pAuxPar->QP.MinBaseQIndex, AV1_MAX_Q_INDEX);
        changed += CheckMaxOrClip(pAuxPar->QP.MaxBaseQIndex, AV1_MAX_Q_INDEX);

        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            changed += CheckOrZero<mfxU8>(pAuxPar->QP.MinBaseQIndex, 0);
            changed += CheckOrZero<mfxU8>(pAuxPar->QP.MaxBaseQIndex, 0);
        }
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxU32 General::GetRawBytes(mfxU16 w, mfxU16 h, mfxU16 ChromaFormat, mfxU16 BitDepth)
{
    mfxU32 s = w * h;

    if (ChromaFormat == MFX_CHROMAFORMAT_YUV420)
        s = s * 3 / 2;
    else if (ChromaFormat == MFX_CHROMAFORMAT_YUV422)
        s *= 2;
    else if (ChromaFormat == MFX_CHROMAFORMAT_YUV444)
        s *= 3;

    assert(BitDepth >= 8);
    if (BitDepth != 8)
        s = (s * BitDepth + 7) / 8;

    return s;
}
mfxStatus General::CheckIOPattern(mfxVideoParam & par)
{
    if (Check<mfxU16
        , MFX_IOPATTERN_IN_VIDEO_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY
        , MFX_IOPATTERN_IN_OPAQUE_MEMORY
        , 0>
        (par.IOPattern))
    {
        par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus General::CheckGopRefDist(mfxVideoParam & par, const ENCODE_CAPS_AV1& /*caps*/)
{
    MFX_CHECK(par.mfx.GopRefDist, MFX_ERR_NONE);

    const mfxU16 maxRefDist = std::max<mfxU16>(1, par.mfx.GopPicSize - 1);
    MFX_CHECK(!CheckMaxOrClip(par.mfx.GopRefDist, maxRefDist), MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckGPB(mfxVideoParam & par)
{
    mfxU32 changed = 0;
    mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

    if (pCO3)
    {
        changed += CheckOrZero<mfxU16>(
            pCO3->GPB
            , mfxU16(MFX_CODINGOPTION_ON)
            , mfxU16(MFX_CODINGOPTION_OFF)
            , mfxU16(MFX_CODINGOPTION_UNKNOWN));
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckTU(mfxVideoParam & par, const ENCODE_CAPS_AV1& /* caps */)
{
    auto& tu = par.mfx.TargetUsage;

    if (CheckMaxOrZero(tu, 7u))
        return MFX_ERR_UNSUPPORTED;

    if (CheckMinOrZero(tu, 0u))
        return MFX_ERR_UNSUPPORTED;

    return MFX_ERR_NONE;

}

mfxStatus General::CheckDeltaQ(mfxVideoParam& par)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);

    mfxU32 changed = 0;

    changed += CheckMinOrClip(pAuxPar->QP.YDcDeltaQ, -15);
    changed += CheckMaxOrClip(pAuxPar->QP.YDcDeltaQ, 15);
    changed += CheckMinOrClip(pAuxPar->QP.UDcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.UDcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.UAcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.UAcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.VDcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.VDcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.VAcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.VAcDeltaQ, 63);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckFrameOBU(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    MFX_CHECK(pAV1Par, MFX_ERR_NONE);
    mfxU32 invalid = 0;

    if (IsOn(pAV1Par->PackOBUFrame) && caps.FrameOBUSupport == false)
    {
        pAV1Par->PackOBUFrame = MFX_CODINGOPTION_OFF;
        invalid = 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckOrderHint(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);
    mfxU32 invalid = 0;

    if (IsOn(pAuxPar->EnableOrderHint) && caps.AV1ToolSupportFlags.fields.enable_order_hint == false)
    {
        pAuxPar->EnableOrderHint = MFX_CODINGOPTION_OFF;
        invalid = 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckOrderHintBits(mfxVideoParam& par)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);

    //OrderHintBits valid range is 1-8, 0 is default value and will be reset in SetDefaults
    mfxU32 changed = 0;
    changed += CheckMaxOrClip(pAuxPar->OrderHintBits, 8);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckCDEF(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
    MFX_CHECK(pAV1Par, MFX_ERR_NONE);
    mfxU32 invalid = 0;

    if (IsOn(pAV1Par->EnableCdef) && caps.AV1ToolSupportFlags.fields.enable_cdef == false)
    {
        pAV1Par->EnableCdef = MFX_CODINGOPTION_OFF;
        invalid = 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

inline void CheckExtParam(
    const ParamSupport& sprt
    , mfxVideoParam& par
    , std::vector<mfxU8>& onesBuf)
{
    if (!par.ExtParam)
        return;

    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (!par.ExtParam[i])
            continue;

        mfxExtBuffer header = *par.ExtParam[i];

        memset(par.ExtParam[i], 0, header.BufferSz);
        *par.ExtParam[i] = header;

        auto it = sprt.m_ebCopySupported.find(header.BufferId);

        if (it != sprt.m_ebCopySupported.end())
        {
            if (onesBuf.size() < header.BufferSz)
                onesBuf.insert(onesBuf.end(), header.BufferSz - mfxU32(onesBuf.size()), 1);

            auto pSrc = (mfxExtBuffer*)onesBuf.data();
            *pSrc = header;

            for (auto& copy : it->second)
                copy(pSrc, par.ExtParam[i]);
        }
    }
}

void General::CheckQuery0(const ParamSupport& sprt, mfxVideoParam& par)
{
    std::vector<mfxU8> onesBuf(sizeof(par), 1);

    auto ExtParam    = par.ExtParam;
    auto NumExtParam = par.NumExtParam;

    par = mfxVideoParam{};

    for (auto& copy : sprt.m_mvpCopySupported)
        copy((mfxVideoParam*)onesBuf.data(), &par);

    par.ExtParam    = ExtParam;
    par.NumExtParam = NumExtParam;

    CheckExtParam(sprt, par, onesBuf);
}

mfxStatus General::CheckBuffers(const ParamSupport& sprt, const mfxVideoParam& in, const mfxVideoParam* out)
{
    MFX_CHECK(!(!in.NumExtParam && (!out || !out->NumExtParam)), MFX_ERR_NONE);
    MFX_CHECK(in.ExtParam, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(!(out && (!out->ExtParam || out->NumExtParam != in.NumExtParam))
        , MFX_ERR_UNDEFINED_BEHAVIOR);

    std::map<mfxU32, mfxU32> detected[2];
    mfxU32 dId = 0;

    for (auto pPar : { &in, out })
    {
        if (!pPar)
            continue;

        for (mfxU32 i = 0; i < pPar->NumExtParam; i++)
        {
            if (!pPar->ExtParam[i])
                continue;

            auto id = pPar->ExtParam[i]->BufferId;

            MFX_CHECK(sprt.m_ebCopySupported.find(id) != sprt.m_ebCopySupported.end(), MFX_ERR_UNSUPPORTED);
            MFX_CHECK(!(detected[dId][id]++), MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        dId++;
    }

    MFX_CHECK(!(out && detected[0] != detected[1]), MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}

mfxStatus General::CopyConfigurable(const ParamSupport& sprt, const mfxVideoParam& in, mfxVideoParam& out)
{
    auto CopyMVP = [&](const mfxVideoParam& src, mfxVideoParam& dst)
    {
        std::for_each(
            sprt.m_mvpCopySupported.begin()
            , sprt.m_mvpCopySupported.end()
            , [&](const std::function<void(const mfxVideoParam*, mfxVideoParam*)>& copy)
        {
            copy(&src, &dst);
        });
    };

    if (&in == &out) //"special" case
    {
        mfxVideoParam tmp = {};

        CopyMVP(in, tmp);

        tmp.NumExtParam = out.NumExtParam;
        tmp.ExtParam = out.ExtParam;

        out = tmp;

        std::list<mfxExtBuffer*> outBufs(out.ExtParam, out.ExtParam + out.NumExtParam);
        outBufs.sort();
        outBufs.remove(nullptr);

        std::for_each(outBufs.begin(), outBufs.end()
            , [&](mfxExtBuffer* pEB)
        {
            mfxExtBuffer header = *pEB;
            auto copyIt = sprt.m_ebCopySupported.find(header.BufferId);

            if (copyIt == sprt.m_ebCopySupported.end())
            {
                memset(pEB, 0, header.BufferSz);
                *pEB = header;
                return;
            }

            std::vector<mfxU8> zeroBuf(header.BufferSz, 0);
            auto pSrc = (mfxExtBuffer*)zeroBuf.data();
            *pSrc = header;

            std::for_each(
                copyIt->second.begin()
                , copyIt->second.end()
                , [&](const std::function<void(const mfxExtBuffer*, mfxExtBuffer*)>& copy)
            {
                copy(pEB, pSrc);
            });

            std::copy((mfxU8*)pSrc, (mfxU8*)pSrc + pSrc->BufferSz, (mfxU8*)pEB);

        });
    }
    else
    {
        auto ExtParam = out.ExtParam;
        auto NumExtParam = out.NumExtParam;

        out = mfxVideoParam{};

        CopyMVP(in, out);

        out.ExtParam = ExtParam;
        out.NumExtParam = NumExtParam;

        std::list<mfxExtBuffer*> outBufs(out.ExtParam, out.ExtParam + out.NumExtParam);
        outBufs.sort();
        outBufs.remove(nullptr);

        std::for_each(outBufs.begin(), outBufs.end()
            , [&](mfxExtBuffer* pEbOut)
        {
            mfxExtBuffer header = *pEbOut;

            memset(pEbOut, 0, header.BufferSz);
            *pEbOut = header;

            auto pEbIn = ExtBuffer::Get(in, header.BufferId);
            auto copyIt = sprt.m_ebCopySupported.find(header.BufferId);

            bool bSkip = (copyIt == sprt.m_ebCopySupported.end()) || !pEbIn;
            if (bSkip)
                return;

            std::for_each(
                copyIt->second.begin()
                , copyIt->second.end()
                , [&](const std::function<void(const mfxExtBuffer*, mfxExtBuffer*)>& copy)
            {
                copy(pEbIn, pEbOut);
            });
        });
    }

    return MFX_ERR_NONE;
}

mfxStatus General::CheckCodedPicSize(
    mfxVideoParam& par
    , const Defaults::Param& defPar)
{
    mfxExtAV1Param* pAV1 = ExtBuffer::Get(par);
    MFX_CHECK(pAV1, MFX_ERR_NONE);

    auto alignment = defPar.base.GetCodedPicAlignment(defPar);
    auto& W = pAV1->FrameWidth;
    auto& H = pAV1->FrameHeight;
    auto AW = mfx::align2_value(W, alignment);
    auto AH = mfx::align2_value(H, alignment);

    MFX_CHECK(!CheckMaxOrZero(W, par.mfx.FrameInfo.Width), MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!CheckMaxOrZero(H, par.mfx.FrameInfo.Height), MFX_ERR_UNSUPPORTED);

    if ((W != AW) || (H != AH))
    {
        W = AW;
        H = AH;
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxU16 FrameType2SliceType(mfxU32 ft)
{
    bool bB = IsB(ft);
    bool bP = !bB && IsP(ft);
    return 1 * bP + 2 * !(bB || bP);
}

template<class T> inline T Lsb(T val, mfxU32 maxLSB)
{
    if (val >= 0)
        return val % maxLSB;
    return (maxLSB - ((-val) % maxLSB)) % maxLSB;
}

inline FRAME_TYPE MapMfxFrameTypeToSpec(mfxU16 ft)
{
    if (IsB(ft) || IsP(ft))
        return INTER_FRAME;
    else
    {
        assert(IsI(ft));
        return KEY_FRAME;
    }
}

inline void SetCDEFByAuxData(
    const mfxExtAV1AuxData* const pAuxPar
    , FH& bs_fh)
{
    if (!pAuxPar)
        return;

    auto& cdef = bs_fh.cdef_params;
    if (pAuxPar->Cdef.CdefDampingMinus3)
    {
        cdef.cdef_damping = pAuxPar->Cdef.CdefDampingMinus3 + 3;
    }

    if (pAuxPar->Cdef.CdefBits)
    {
        cdef.cdef_bits = pAuxPar->Cdef.CdefBits;
    }

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        if (pAuxPar->Cdef.CdefYStrengths[i])
        {
            cdef.cdef_y_pri_strength[i] = pAuxPar->Cdef.CdefYStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_y_sec_strength[i] = pAuxPar->Cdef.CdefYStrengths[i] % CDEF_STRENGTH_DIVISOR;
        }

        if (pAuxPar->Cdef.CdefUVStrengths[i])
        {
            cdef.cdef_uv_pri_strength[i] = pAuxPar->Cdef.CdefUVStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_uv_sec_strength[i] = pAuxPar->Cdef.CdefUVStrengths[i] % CDEF_STRENGTH_DIVISOR;
        }
    }
}

inline bool FrameIsIntra(FRAME_TYPE ft)
{
    return ft == INTRA_ONLY_FRAME || ft == KEY_FRAME;
}

inline void SetCDEF(
    const Defaults::Param& dflts
    , const SH& sh
    , FH& fh)
{
    if (!sh.enable_cdef)
        return;

    //Get default CDEF settings
    dflts.base.GetCDEF(fh);

    //Set CDEF through command option
    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(dflts.mvp);
    SetCDEFByAuxData(pAuxPar, fh);
}

inline void SetRefFrameFlags(
    const TaskCommonPar& task
    , FH& fh
    , const bool frameIsIntra)
{
    if (frameIsIntra)
        fh.primary_ref_frame = PRIMARY_REF_NONE;

    const int allFrames = (1 << NUM_REF_FRAMES) - 1;
    if (fh.frame_type == SWITCH_FRAME ||
        (fh.frame_type == KEY_FRAME && fh.show_frame))
        fh.refresh_frame_flags = allFrames;
    else
    {
        for (mfxU8 i = 0; i < NUM_REF_FRAMES; i++)
            fh.refresh_frame_flags |= task.RefreshFrameFlags[i] << i;
    }
}

inline void SetRefFrameIndex(
    const TaskCommonPar& task
    , FH& fh
    , const bool frameIsIntra)
{
    if (frameIsIntra)
        return;

    const auto& refList = task.RefList;
    mfxU8 defaultRef = refList[LAST_FRAME - LAST_FRAME];

    auto SetRef = [&defaultRef](mfxU8 ref) -> int32_t
    { return IsValid(ref) ? ref : defaultRef;};

    const mfxU8 bwdStartIdx = BWDREF_FRAME - LAST_FRAME;
    auto bwdStartIt = refList.begin() + bwdStartIdx;

    std::transform(refList.begin(), bwdStartIt, fh.ref_frame_idx, SetRef);

    auto validBwd = IsP(task.FrameType) ? refList.end() : std::find_if(bwdStartIt, refList.end(), IsValid);
    if (validBwd != refList.end())
        defaultRef = *validBwd;

    std::transform(bwdStartIt, refList.end(), fh.ref_frame_idx + bwdStartIdx, SetRef);
}

inline mfxU32 GetReferenceMode(const TaskCommonPar& task)
{
    return IsB(task.FrameType) || task.isLDB ? 1 : 0;
}

inline int av1_get_relative_dist(const SH& sh, const uint32_t a, const uint32_t b)
{
    // the logic is from AV1 spec 5.9.3

    if (!sh.enable_order_hint)
        return 0;

    const uint32_t OrderHintBits = sh.order_hint_bits_minus1 + 1;

    int32_t diff = a - b;
    uint32_t m = 1 << (OrderHintBits - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
}

inline void GetFwdBwdIdx( const DpbType& dpb, const SH& sh, const FH& fh, const int orderHint, int& forwardIdx, int& forwardHint, int& backwardIdx)
{
    int i = 0;
    int refHint = 0;
    int backwardHint = -1;
    for (i = 0; i < REFS_PER_FRAME; i++)
    {
        auto& refFrm = dpb.at(fh.ref_frame_idx[i]);
        refHint = refFrm->DisplayOrderInGOP;
        if (av1_get_relative_dist(sh, refHint, orderHint) < 0)
        {
            if (forwardIdx < 0 ||
                av1_get_relative_dist(sh, refHint, forwardHint) > 0)
            {
                forwardIdx = i;
                forwardHint = refHint;
            }
        }
        else if (av1_get_relative_dist(sh, refHint, orderHint) > 0)
        {
            if (backwardIdx < 0 ||
                av1_get_relative_dist(sh, refHint, backwardHint) < 0)
            {
                backwardIdx = i;
                backwardHint = refHint;
            }
        }
    }
}

inline void SetSkipModeAllowed(const SH& sh, FH& fh, const DpbType& dpb)
{
    int forwardIdx = -1;
    int forwardHint = -1;
    int backwardIdx = -1;
    GetFwdBwdIdx(dpb, sh, fh, fh.order_hint, forwardIdx, forwardHint, backwardIdx);
    if (forwardIdx < 0)
    {
        fh.skipModeAllowed = 0;
    }
    else if (backwardIdx >= 0)
    {
        fh.skipModeAllowed = 1;
    }
    else
    {
        int secondForwardHint = -1;
        int secondForwardIdx = -1;
        GetFwdBwdIdx(dpb, sh, fh, forwardHint, secondForwardIdx, secondForwardHint, backwardIdx);
        if (secondForwardIdx < 0)
        {
            fh.skipModeAllowed = 0;
        }
        else
        {
            fh.skipModeAllowed = 1;
        }
    }
}

inline void SetSkipModeParams(const SH& sh, FH& fh, const bool frameIsIntra, const DpbType& dpb)
{
    // the logic is from AV1 spec 5.9.22

    fh.skipModeAllowed = 1;

    if (frameIsIntra || !fh.reference_select || !sh.enable_order_hint)
        fh.skipModeAllowed = 0;
    else
    {
        SetSkipModeAllowed(sh, fh, dpb);
    }

    fh.skip_mode_present = fh.skipModeAllowed;
}

mfxStatus General::GetCurrentFrameHeader(
    const TaskCommonPar& task
    , const Defaults::Param& dflts
    , const SH& sh
    , const FH& fh
    , FH & currFH) const
{
    currFH = fh;

    currFH.frame_type     = MapMfxFrameTypeToSpec(task.FrameType);
    currFH.show_frame     = IsHidden(task) ? 0 : 1;
    if (currFH.show_frame)
    {
        currFH.showable_frame = currFH.frame_type != KEY_FRAME;
    }
    else
    {
         currFH.showable_frame = 1; // for now, all hiden frame will be show in later.
    }

    if (currFH.frame_type == SWITCH_FRAME ||
        currFH.frame_type == KEY_FRAME && currFH.show_frame)
    {
        currFH.error_resilient_mode = 1;
    }

    currFH.order_hint = task.DisplayOrderInGOP;

    const bool frameIsIntra = FrameIsIntra(currFH.frame_type);
    SetRefFrameFlags(task, currFH, frameIsIntra);
    SetRefFrameIndex(task, currFH, frameIsIntra);

    currFH.quantization_params.base_q_idx = task.QpY;

    dflts.base.GetLoopFilters(dflts, currFH);

    SetCDEF(dflts, sh, currFH);

    currFH.reference_select = GetReferenceMode(task);

    SetSkipModeParams(sh, currFH, frameIsIntra, task.DPB);

    return MFX_ERR_NONE;
}

}

#endif
