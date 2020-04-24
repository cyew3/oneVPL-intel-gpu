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

#include "av1ehw_g12_general.h"
#include "av1ehw_g12_data.h"
#include "av1ehw_g12_constraints.h"
#include <numeric>
#include <set>

using namespace AV1EHW::Gen12;

namespace AV1EHW
{

static const uint8_t LoopFilterLevelsLuma[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,
     2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
     4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,
     6,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10,
    11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15,
    15, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20,
    21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26,
    27, 27, 27, 28, 28, 29, 29, 29, 30, 30, 31, 31, 31, 32, 32, 33,
    33, 34, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 41,
    41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 52, 53, 55,
    56, 58, 59, 61, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
};

static const uint8_t LoopFilterLevelsChroma[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9, 10,
    10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
    16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 30, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31
};

class GetDefault
{
private:
    inline static void TuneCDEFLowQP(uint32_t* strength, int32_t qp)
    {
        if (!(qp < 90))
            assert(false && "Only called if qp < 90");

        strength[0] = 5;
        strength[1] = 41;
        strength[3] = 6;
        strength[5] = 16;
    }

    inline static void TuneCDEFHighQP(
        CdefParams& cdef
        , uint32_t* strength
        , int32_t qp)
    {
        if (!(qp > 140))
            assert(false && "Only called if qp > 140");

        cdef.cdef_bits = 2;
        strength[1] = 63;
        if (qp > 210)
        {
            cdef.cdef_bits = 1;
            strength[0] = 0;
        }
    }

    inline static void TuneCDEFMediumQP(
        const FH& bs_fh
        , CdefParams& cdef
        , uint32_t* strength
        , int32_t qp)
    {
        if (!(qp > 130 && qp <= 140))
            assert(false && "Only called if qp > 130 && qp <= 140");

        cdef.cdef_bits = 2;
        strength[1] = 63;

        if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
            strength[3] = 1;
        else
            strength[3] = 32;
    }

public:
    static mfxU16 CodedPicAlignment(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        return mfxU16((2 - (par.hw >= MFX_HW_CNL)) * 8);
    }

    static mfxU16 CodedPicWidth(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtHEVCParam* pHEVC = ExtBuffer::Get(par.mvp);

        if (pHEVC && pHEVC->PicWidthInLumaSamples)
        {
            return pHEVC->PicWidthInLumaSamples;
        }

        auto&   fi          = par.mvp.mfx.FrameInfo;
        bool    bCropsValid = fi.CropW > 0;
        mfxU16  W           = mfxU16(bCropsValid * (fi.CropW + fi.CropX) + !bCropsValid * fi.Width);

        return mfx::align2_value(W, par.base.GetCodedPicAlignment(par));
    }

    static mfxU16 CodedPicHeight(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtHEVCParam* pHEVC = ExtBuffer::Get(par.mvp);

        if (pHEVC && pHEVC->PicHeightInLumaSamples)
        {
            return pHEVC->PicHeightInLumaSamples;
        }

        auto&   fi          = par.mvp.mfx.FrameInfo;
        bool    bCropsValid = fi.CropH > 0;
        mfxU16  H           = mfxU16(bCropsValid * (fi.CropH + fi.CropY) + !bCropsValid * fi.Height);

        return mfx::align2_value(H, par.base.GetCodedPicAlignment(par));
    }

    static mfxU16 GopPicSize(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.GopPicSize)
        {
            return par.mvp.mfx.GopPicSize;
        }

        return mfxU16(GOP_INFINITE);
    }

    static mfxU16 GopRefDist(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.GopRefDist)
        {
            return par.mvp.mfx.GopRefDist;
        }

        return 1;
    }

    static mfxU16 NumRefFrames(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.NumRefFrame)
        {
            return par.mvp.mfx.NumRefFrame;
        }

        // TODO: add NumRefFrame calculation for B-pyramid

        return (par.base.GetGopRefDist(par) > 1) ? 2 : 1;
    }

    static mfxU16 MinRefForBNoPyramid(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return 2;
    }

    static bool NumRefActive(
        Defaults::TGetNumRefActive::TExt
        , const Defaults::Param& par
        , mfxU16(*pP)[8]
        , mfxU16(*pBL0)[8]
        , mfxU16(*pBL1)[8])
    {
        bool bExternal = false;
        const mfxU16 defaultFwd = 2; // TODO: change later based on TU settings
        const mfxU16 defaultBwd = 1; // TODO: change later based on TU settings

        auto SetDefaultNRef =
            [](const mfxU16(*extRef)[8], mfxU16 defaultRef, mfxU16(*NumRefActive)[8])
        {
            bool bExternal = false;
            bool bDone = false;

            bDone |= !NumRefActive;
            bDone |= !bDone && !extRef && std::fill_n(*NumRefActive, 8, defaultRef);
            bDone |= !bDone && std::transform(
                *extRef
                , std::end(*extRef)
                , *NumRefActive
                , [&](mfxU16 ext)
                {
                    bExternal |= !!ext;
                    defaultRef *= !bExternal;
                    SetDefault(defaultRef, ext);
                    return defaultRef;
                });

            return bExternal;
        };

        const mfxU16(*extRefP)[8] = nullptr;
        const mfxU16(*extRefBL0)[8] = nullptr;
        const mfxU16(*extRefBL1)[8] = nullptr;

        if (const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp))
        {
            extRefP = &pCO3->NumRefActiveP;
            extRefBL0 = &pCO3->NumRefActiveBL0;
            extRefBL1 = &pCO3->NumRefActiveBL1;
        }

        bExternal |= SetDefaultNRef(extRefP, defaultFwd, pP);
        bExternal |= SetDefaultNRef(extRefBL0, defaultFwd, pBL0);
        bExternal |= SetDefaultNRef(extRefBL1, defaultBwd, pBL1);

        return bExternal;
    }

    static mfxU16 BRefType(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return MFX_B_REF_OFF;
    }

    static mfxU16 PRefType(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param&)
    {
        return MFX_P_REF_SIMPLE;
    }

    static std::tuple<mfxU32, mfxU32> FrameRate(
        Defaults::TChain<std::tuple<mfxU32, mfxU32>>::TExt
        , const Defaults::Param& par)
    {
        auto& fi = par.mvp.mfx.FrameInfo;

        if (fi.FrameRateExtN && fi.FrameRateExtD)
        {
            return std::make_tuple(fi.FrameRateExtN, fi.FrameRateExtD);
        }

        mfxU32 frN = 30, frD = 1;

        return std::make_tuple(frN, frD);
    }

    static mfxU16 MaxBitDepthByFourCC(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        par;
        return mfxU16(BITDEPTH_8);
    }

    static mfxU16 MaxBitDepth(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        par;
        return mfxU16(BITDEPTH_8);
    }

    static mfxU16 MaxChroma(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        par;
        return mfxU16(MFX_CHROMAFORMAT_YUV420);
    }

    static mfxU16 TargetBitDepthLuma(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp);

        if (pCO3 && pCO3->TargetBitDepthLuma)
        {
            return pCO3->TargetBitDepthLuma;
        }

        return par.mvp.mfx.FrameInfo.BitDepthLuma;
    }

    static mfxU16 TargetChromaFormat(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp);

        if (pCO3 && pCO3->TargetChromaFormatPlus1)
        {
            return pCO3->TargetChromaFormatPlus1;
        }

        return par.mvp.mfx.FrameInfo.ChromaFormat + 1;
    }

    static mfxU32 TargetKbps(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& par)
    {
        auto& mfx = par.mvp.mfx;

        if (mfx.TargetKbps)
        {
            return mfx.TargetKbps * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
        }

        mfxU32 frN = 0, frD = 0, maxBR = 0xffffffff;

        SetIf(maxBR, !!mfx.CodecLevel, [&]() { return GetMaxKbpsByLevel(par.mvp); });

        mfxU16 W = par.base.GetCodedPicWidth(par);
        mfxU16 H = par.base.GetCodedPicHeight(par);
        std::tie(frN, frD) = par.base.GetFrameRate(par);

        mfxU16 bd = par.base.GetTargetBitDepthLuma(par);
        mfxU16 cf = par.base.GetTargetChromaFormat(par) - 1;
        mfxU32 rawBits = (General::GetRawBytes(W, H, cf, bd) << 3);

        return std::min<mfxU32>(maxBR, rawBits * frN / frD / 150000);
    }

    static mfxU32 MaxKbps(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& par)
    {
        auto& mfx = par.mvp.mfx;

        if (mfx.MaxKbps)
        {
            return mfx.MaxKbps * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
        }
        return par.base.GetTargetKbps(par);
    }

    static mfxU32 BufferSizeInKB(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& par)
    {
        auto& mfx = par.mvp.mfx;

        // this code is relevant for CQP only
        const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par.mvp);
        const mfxU32 width = pAV1Par ? pAV1Par->FrameWidth : mfx.FrameInfo.Width;
        const mfxU32 height = pAV1Par ? pAV1Par->FrameHeight : mfx.FrameInfo.Height;
        if (mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
            return (width * height * 3) / 1000; // size of two uncompressed 420 8bit frames in KB
        else
            return (width * height * 3) / 2 / 1000;  // size of uncompressed 420 8bit frame in KB
    }

    static std::tuple<mfxU16, mfxU16> MaxNumRef(
        Defaults::TChain<std::tuple<mfxU16, mfxU16>>::TExt
        , const Defaults::Param& par)
    {
        mfxU16 tu = par.mvp.mfx.TargetUsage;
        CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, 4);
        --tu;

        static const mfxU16 nRefs[1][2][7] =
        {
            {// VDENC
                  { 3, 3, 2, 2, 2, 1, 1 }
                , { 3, 3, 2, 2, 2, 1, 1 }
            }
        };
       //TODO: need to be re-implemented for AV1

        mfxU16 nRefL0 = mfxU16(nRefs[0][0][tu]);
        mfxU16 nRefL1 = mfxU16(nRefs[0][1][tu]);

        return std::make_tuple(nRefL0, nRefL1);
    }

    static mfxU16 RateControlMethod(
        Defaults::TChain< mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.RateControlMethod)
        {
            return par.mvp.mfx.RateControlMethod;
        }
        return mfxU16(MFX_RATECONTROL_CQP);
    }

    static mfxU8 MinQPMFX(
        Defaults::TChain< mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return mfxU8(0);
    }

    static mfxU8 MaxQPMFX(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return mfxU8(AV1_MAX_Q_INDEX);
    }

    static std::tuple<mfxU16, mfxU16, mfxU16> QPMFX(
        Defaults::TChain<std::tuple<mfxU16, mfxU16, mfxU16>>::TExt
        , const Defaults::Param& par)
    {
        bool bCQP = (par.base.GetRateControlMethod(par) == MFX_RATECONTROL_CQP);

        mfxU16 QPI = bCQP * par.mvp.mfx.QPI;
        mfxU16 QPP = bCQP * par.mvp.mfx.QPP;
        mfxU16 QPB = bCQP * par.mvp.mfx.QPB;

        bool bValid = (QPI && QPP && QPB);

        if (bValid)
            return std::make_tuple(QPI, QPP, QPB);

        auto maxQP = par.base.GetMaxQPMFX(par);

        SetDefault(QPI, std::max<mfxU16>(par.base.GetMinQPMFX(par), (maxQP + 1) / 2));
        SetDefault(QPP, std::min<mfxU16>(QPI + 5, maxQP));
        SetDefault(QPB, std::min<mfxU16>(QPP + 5, maxQP));

        return std::make_tuple(QPI, QPP, QPB);
    }

    static mfxU16 Profile(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        bool bPassThrough = !!par.mvp.mfx.CodecProfile;

        mfxU16 bd = 0, cf = 0;
        SetIf(bd, !bPassThrough, [&]() { return par.base.GetTargetBitDepthLuma(par); });
        SetIf(cf, !bPassThrough, [&]() { return mfxU16(par.base.GetTargetChromaFormat(par) - 1); });

        bool bMain10 = !bPassThrough && bd == 10;
        bool bMain = !bPassThrough && !bMain10;

        return
            bPassThrough * par.mvp.mfx.CodecProfile
            + bMain10 * AV1_SEQ_PROFILE_0_420_8or10bit
            + bMain * AV1_SEQ_PROFILE_0_420_8or10bit;
    }

    static bool GUID(
        Defaults::TGetGUID::TExt
        , const Defaults::Param& par
        , ::GUID& guid)
    {
        const std::map<mfxU16, std::map<mfxU16, ::GUID>> GUIDSupported =
        {
            {
                mfxU16(BITDEPTH_8),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), DXVA2_Intel_LowpowerEncode_AV1_420_8b}
                }
            }
            , {
                mfxU16(BITDEPTH_10),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), DXVA2_Intel_LowpowerEncode_AV1_420_10b}
                }
            }
        };

        // (1) If target bit depth, chroma sampling are specified explicitly - check that they are correct.
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp);
        bool bBDInvalid = pCO3 && Check<mfxU16, BITDEPTH_10, BITDEPTH_8, 0>(pCO3->TargetBitDepthLuma);
        bool bCFInvalid = pCO3 && Check<mfxU16
            , MFX_CHROMAFORMAT_YUV420 + 1>
            (pCO3->TargetChromaFormatPlus1);

        // (2) Try to deduce bit depth and chroma sampling from profile and platform.
        //     For Gen12 only Main profile is supported (8 and 10 bits, 4:2:0 sampling).
        mfxU16 BitDepth = 0;                           // can't derive BitDepth from profile
        mfxU16 ChromaFormat = MFX_CHROMAFORMAT_YUV420; // Main profile supports only 4:2:0 sampling

        // (3) Prepare mfxVideoParam for getting target bit depth and chroma sampling.
        //     - If incorrect depth and sampling were provided explicitly - remove mfxExtCodingOption3 from parameters
        //     - If not - do nothing
        auto mvpCopy = par.mvp;
        mvpCopy.NumExtParam = 0;

        Defaults::Param parCopy(mvpCopy, par.caps, par.hw, par.base);
        auto pParForBD = &par;
        auto pParForCF = &par;

        SetIf(pParForBD, bBDInvalid, &parCopy);
        SetIf(pParForCF, bCFInvalid, &parCopy);

        // (4) If BitDepth and/or ChromaFormat are zero, get them from mfxVideoParam
        SetIf(BitDepth, !BitDepth, [&]() { return pParForBD->base.GetTargetBitDepthLuma(*pParForBD); });
        SetIf(ChromaFormat, !ChromaFormat, [&]() { return mfxU16(pParForCF->base.GetTargetChromaFormat(*pParForCF) - 1); });

        // (5) Check that list of GUIDs contains GUID for resulting BitDepth, ChromaFormat
        bool bSupported =
            GUIDSupported.count(BitDepth)
            && GUIDSupported.at(BitDepth).count(ChromaFormat);

        // (6) Choose and return GUID
        SetIf(guid, bSupported, [&]() { return GUIDSupported.at(BitDepth).at(ChromaFormat); });

        return bSupported;
    }

    static mfxU16 AsyncDepth(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        return par.mvp.AsyncDepth + !par.mvp.AsyncDepth * 2;
    }

    static mfxU16 PicTimingSEI(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption* pCO = ExtBuffer::Get(par.mvp);
        bool bForceON = pCO && IsOn(pCO->PicTimingSEI);
        return Bool2CO(bForceON);
    }

    static mfxU16 NumTemporalLayers(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param&)
    {
        return 1;
    }

    static mfxU8 NumReorderFrames(
        Defaults::TChain<mfxU8>::TExt
        , const Defaults::Param& par)
    {
        mfxU8 BFrameRate = mfxU8(par.base.GetGopRefDist(par) - 1);
        return !!BFrameRate;
    }

    static bool NonStdReordering(
        Defaults::TChain<mfxU8>::TExt
        , const Defaults::Param& par)
    {
        return
            par.mvp.mfx.EncodedOrder
            && par.mvp.mfx.NumRefFrame > 2;
    }

    static mfxU16 FrameType(
        Defaults::TGetFrameType::TExt
        , const Defaults::Param& par
        , mfxU32 displayOrder
        , mfxU32 lastKeyFrame)
    {
        mfxU32 gopPicSize = par.mvp.mfx.GopPicSize;
        mfxU32 gopRefDist = par.mvp.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (par.mvp.mfx.IdrInterval);

        //infinite GOP
        SetIf(idrPicDist, gopPicSize == 0xffff, 0xffffffff);
        SetIf(gopPicSize, gopPicSize == 0xffff, 0xffffffff);

        mfxU32 fo = displayOrder - lastKeyFrame;
        bool bIdr = (fo % gopPicSize == 0);
        bool bPRef =
            !bIdr
            && (   (fo % gopPicSize % gopRefDist == 0)
                || ((fo + 1) % gopPicSize == 0));
        bool bB = !(bPRef || bIdr);
        bool bBRef = false;
        if (IsCModelMode(par.mvp) && bB)
        {
            // CModel doesn't encode non-ref frames
            bB = false;
            bBRef = true;
        }

        mfxU16 ft =
            bIdr * (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)
            + bPRef * (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF)
            + bBRef * (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF)
            + bB * (MFX_FRAMETYPE_B);

        return ft;
    }

    static mfxStatus PreReorderInfo(
        Defaults::TGetPreReorderInfo::TExt
        , const Defaults::Param& par
        , FrameBaseInfo& fi
        , const mfxFrameSurface1* pSurfIn
        , const mfxEncodeCtrl*    pCtrl
        , mfxU32 prevKeyFrameOrder
        , mfxU32 frameOrder)
    {
        mfxU16 ftype = 0;
        auto SetFrameTypeFromGOP   = [&]() { return Res2Bool(ftype, par.base.GetFrameType(par, frameOrder, prevKeyFrameOrder)); };
        auto SetFrameTypeFromCTRL  = [&]() { return Res2Bool(ftype, pCtrl->FrameType); };
        auto ForceIdr              = [&]() { return Res2Bool(ftype, mfxU16(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)); };
        auto SetFrameOrderFromSurf = [&]() { frameOrder = pSurfIn->Data.FrameOrder;  return true; };

        bool bFrameInfoValid =
            (par.mvp.mfx.EncodedOrder && SetFrameOrderFromSurf() && SetFrameTypeFromCTRL() )
            || (pCtrl && IsI(pCtrl->FrameType) && ForceIdr())
            || SetFrameTypeFromGOP();
        MFX_CHECK(bFrameInfoValid, MFX_ERR_UNDEFINED_BEHAVIOR);

        fi.DisplayOrderInGOP = !IsI(ftype) * (frameOrder - prevKeyFrameOrder);
        fi.FrameType         = ftype;

        if (IsP(ftype))
        {
            const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par.mvp);

            fi.isLDB        = IsOn(CO3.GPB);
        }

        return MFX_ERR_NONE;
    }

    static void GetTileUniformSpacingParam(
        std::vector<mfxU32>& colWidth
        , std::vector<mfxU32>& rowHeight
        , std::vector<mfxU32>& TsToRs
        , mfxU32 nCol
        , mfxU32 nRow
        , mfxU32 nTCol
        , mfxU32 nTRow
        , mfxU32 nLCU)
    {
        std::vector<mfxU32> colBd (nTCol + 1, 0);
        std::vector<mfxU32> rowBd (nTRow + 1, 0);

        colWidth.resize(nTCol, 0);
        rowHeight.resize(nTRow, 0);
        TsToRs.resize(nLCU);

        auto pColBd     = colBd.data();
        auto pRowBd     = rowBd.data();
        auto pColWidth  = colWidth.data();
        auto pRowHeight = rowHeight.data();

        mfxI32 i;
        auto NextCW = [nCol, nTCol, &i]() { ++i; return ((i + 1) * nCol) / nTCol - (i * nCol) / nTCol; };
        auto NextRH = [nRow, nTRow, &i]() { ++i; return ((i + 1) * nRow) / nTRow - (i * nRow) / nTRow; };
        auto NextBd  = [&i](mfxU32 wh) { return (i += wh); };

        i = -1;
        std::generate(pColWidth, pColWidth + nTCol, NextCW);
        i = -1;
        std::generate(pRowHeight, pRowHeight + nTRow, NextRH);
        i = 0;
        std::transform(pColWidth, pColWidth + nTCol, pColBd + 1, NextBd);
        i = 0;
        std::transform(pRowHeight, pRowHeight + nTRow, pRowBd + 1, NextBd);

        for (mfxU32 rso = 0; rso < nLCU; ++rso)
        {
            mfxU32 tbX   = rso % nCol;
            mfxU32 tbY   = rso / nCol;
            mfxU32 tso   = 0;
            auto   LTX   = [tbX](mfxU32 x) { return tbX >= x; };
            auto   LTY   = [tbY](mfxU32 y) { return tbY >= y; };
            auto   tileX = std::count_if(pColBd + 1, pColBd + nTCol, LTX);
            auto   tileY = std::count_if(pRowBd + 1, pRowBd + nTRow, LTY);
            
            tso += rowHeight[tileY] * std::accumulate(pColWidth, pColWidth + tileX, 0u);
            tso += nCol             * std::accumulate(pRowHeight, pRowHeight + tileY, 0u);
            tso += (tbY - rowBd[tileY]) * colWidth[tileX] + tbX - colBd[tileX];

            assert(tso < nLCU);

            TsToRs[tso] = rso;
        }
    }

    static void LoopFilters(
        Defaults::TGetLoopFilters::TExt
        , const Defaults::Param& par
        , FH& bs_fh)
    {
        int32_t levelY = 0xff;
        int32_t levelUV = 0xff;
        //for BRC cases place holder values are set to have correct PPS-header,
        //because when [0] and [1] are zeroes header is of different size
        if (par.mvp.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            const int32_t qp = bs_fh.quantization_params.base_q_idx;
            int32_t loopFilterLevelFormulaType = 2; // 0 for old formula, 2 for new formula
            if (loopFilterLevelFormulaType == 2)
            {
                levelY = LoopFilterLevelsLuma[qp];
                levelUV = LoopFilterLevelsChroma[qp];
            }
            else
            {
                if (qp < 150)
                    levelY = qp / 6;
                else
                    levelY = (int32_t)(qp * 0.38 - 32);

                levelY = std::min(levelY, 63);
                levelUV = levelY;
            }
        }

        auto& lp = bs_fh.loop_filter_params;

        lp.loop_filter_level[0] = levelY;
        lp.loop_filter_level[1] = levelY;
        lp.loop_filter_level[2] = levelUV;
        lp.loop_filter_level[3] = levelUV;
    }

    static void CDEF(
        Defaults::TGetCDEF::TExt
        , FH& bs_fh)
    {
        const int32_t qp = bs_fh.quantization_params.base_q_idx;

        uint32_t YStrengths[CDEF_MAX_STRENGTHS];
        // These default values are from CModel
        YStrengths[0] = 36;
        YStrengths[1] = 50;
        YStrengths[2] = 0;
        YStrengths[3] = 24;
        YStrengths[4] = 8;
        YStrengths[5] = 17;
        YStrengths[6] = 4;
        YStrengths[7] = 9;

        auto& cdef = bs_fh.cdef_params;
        cdef.cdef_bits = 3;

        if (qp < 90)
            TuneCDEFLowQP(YStrengths, qp);
        else if (qp > 140)
            TuneCDEFHighQP(cdef, YStrengths, qp);
        else if (qp > 130)
            TuneCDEFMediumQP(bs_fh, cdef, YStrengths, qp);

        if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
            YStrengths[3] = 5;

        for (int i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            cdef.cdef_y_pri_strength[i] = YStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_y_sec_strength[i] = YStrengths[i] % CDEF_STRENGTH_DIVISOR;
            cdef.cdef_uv_pri_strength[i] = YStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_uv_sec_strength[i] = YStrengths[i] % CDEF_STRENGTH_DIVISOR;
        }

        cdef.cdef_damping = (qp >> 6) + 3;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.Get##X.Push(X);

        PUSH_DEFAULT(CodedPicAlignment);
        PUSH_DEFAULT(CodedPicWidth);
        PUSH_DEFAULT(CodedPicHeight);
        PUSH_DEFAULT(GopPicSize);
        PUSH_DEFAULT(GopRefDist);
        PUSH_DEFAULT(NumRefFrames);
        PUSH_DEFAULT(MinRefForBNoPyramid);
        PUSH_DEFAULT(NumRefActive);
        PUSH_DEFAULT(BRefType);
        PUSH_DEFAULT(PRefType);
        PUSH_DEFAULT(FrameRate);
        PUSH_DEFAULT(MaxBitDepthByFourCC);
        PUSH_DEFAULT(MaxBitDepth);
        PUSH_DEFAULT(MaxChroma);
        PUSH_DEFAULT(TargetBitDepthLuma);
        PUSH_DEFAULT(TargetChromaFormat);
        PUSH_DEFAULT(TargetKbps);
        PUSH_DEFAULT(MaxKbps);
        PUSH_DEFAULT(BufferSizeInKB);
        PUSH_DEFAULT(MaxNumRef);
        PUSH_DEFAULT(RateControlMethod);
        PUSH_DEFAULT(MinQPMFX);
        PUSH_DEFAULT(MaxQPMFX);
        PUSH_DEFAULT(QPMFX);
        PUSH_DEFAULT(Profile);
        PUSH_DEFAULT(GUID);
        PUSH_DEFAULT(AsyncDepth);
        PUSH_DEFAULT(PicTimingSEI);
        PUSH_DEFAULT(FrameType);
        PUSH_DEFAULT(NumTemporalLayers);
        PUSH_DEFAULT(PreReorderInfo);
        PUSH_DEFAULT(NumReorderFrames);
        PUSH_DEFAULT(NonStdReordering);
        PUSH_DEFAULT(LoopFilters);
        PUSH_DEFAULT(CDEF);

#undef PUSH_DEFAULT
    }
};

class PreCheck
{
public:
    static mfxStatus CodecId(
        Defaults::TPreCheck::TExt
        , const mfxVideoParam& in)
    {
        MFX_CHECK(in.mfx.CodecId == MFX_CODEC_AV1, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus ChromaFormat(
        Defaults::TPreCheck::TExt
        , const mfxVideoParam& in)
    {
        MFX_CHECK(in.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus LowPower(
        Defaults::TPreCheck::TExt
        , const mfxVideoParam& in)
    {
        MFX_CHECK(in.mfx.LowPower != MFX_CODINGOPTION_OFF, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.PreCheck##X.Push(X);

        PUSH_DEFAULT(CodecId);
        PUSH_DEFAULT(ChromaFormat);
        PUSH_DEFAULT(LowPower);

#undef PUSH_DEFAULT
    }
};

class CheckAndFix
{
public:
    static mfxStatus Level(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& /*dpar*/
        , mfxVideoParam& par)
    {
        // TODO: level is hardcoded as 0 in legacy path
        bool bInvalid = CheckOrZero<mfxU16
            , 0>
            (par.mfx.CodecLevel);

        MFX_CHECK(!bInvalid, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus SurfSize(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        auto&  W        = par.mfx.FrameInfo.Width;
        auto&  H        = par.mfx.FrameInfo.Height;
        auto   AW       = mfx::align2_value(W, HW_SURF_ALIGN_W);
        auto   AH       = mfx::align2_value(H, HW_SURF_ALIGN_H);
        mfxU32 changed  = 0;
        mfxU16 MaxW     = mfxU16(dpar.caps.MaxPicWidth);
        mfxU16 MaxH     = mfxU16(dpar.caps.MaxPicHeight);
        mfxU16 MinW     = mfxU16(SB_SIZE);
        mfxU16 MinH     = mfxU16(SB_SIZE);

        MFX_CHECK(W, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(H, MFX_ERR_INVALID_VIDEO_PARAM);

        changed += (W != AW) + (H != AH);
        W = AW;
        H = AH;

        MFX_CHECK(!CheckRangeOrSetDefault<mfxU16>(W, MinW, MaxW, 0), MFX_ERR_UNSUPPORTED);
        MFX_CHECK(!CheckRangeOrSetDefault<mfxU16>(H, MinH, MaxH, 0), MFX_ERR_UNSUPPORTED);

        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    }

    static mfxStatus Profile(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& defPar
        , mfxVideoParam& par)
    {
        defPar;

        bool bValid =
            !par.mfx.CodecProfile
            || (par.mfx.CodecProfile == AV1_SEQ_PROFILE_0_420_8or10bit);

        par.mfx.CodecProfile *= bValid;

        MFX_CHECK(bValid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus FourCC(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& /*dpar*/
        , mfxVideoParam& par)
    {
        bool bInvalid = CheckOrZero<mfxU32
            , MFX_FOURCC_NV12
            , MFX_FOURCC_P010>
            (par.mfx.FrameInfo.FourCC);

        MFX_CHECK(!bInvalid, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus InputFormatByFourCC(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& /*dpar*/
        , mfxVideoParam& par)
    {
        mfxU32 invalid = 0;
        static const std::map<mfxU32, std::array<mfxU16, 2>> FourCCPar=
        {
            {mfxU32(MFX_FOURCC_NV12),     {mfxU16(MFX_CHROMAFORMAT_YUV420), BITDEPTH_8}}
            , {mfxU32(MFX_FOURCC_P010),   {mfxU16(MFX_CHROMAFORMAT_YUV420), BITDEPTH_10}}
        };

        auto itFourCCPar = FourCCPar.find(par.mfx.FrameInfo.FourCC);
        invalid += (itFourCCPar == FourCCPar.end() && Res2Bool(par.mfx.FrameInfo.FourCC, mfxU32(MFX_FOURCC_NV12)));

        itFourCCPar = FourCCPar.find(par.mfx.FrameInfo.FourCC);
        assert(itFourCCPar != FourCCPar.end());

        invalid += CheckOrZero(par.mfx.FrameInfo.ChromaFormat, itFourCCPar->second[0]);
        invalid += CheckOrZero(par.mfx.FrameInfo.BitDepthLuma, itFourCCPar->second[1], 0);
        invalid += CheckOrZero(par.mfx.FrameInfo.BitDepthChroma, itFourCCPar->second[1], 0);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus TargetChromaFormat(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);

        bool b420 = (pCO3->TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV420 + 1));
        bool b422 = (pCO3->TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV422 + 1));
        bool b444 = (pCO3->TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV444 + 1));

        // range check
        mfxU32 invalid = (pCO3->TargetChromaFormatPlus1 > 0) && (!(b420 || b422 || b444));
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        // caps check
        invalid = (b420 && !dpar.caps.ChromeSupportFlags.fields.i420) ||
                  (b422 && !dpar.caps.ChromeSupportFlags.fields.i422) ||
                  (b444 && !dpar.caps.ChromeSupportFlags.fields.i444);
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus TargetBitDepth(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);

        bool b8bit = (pCO3->TargetBitDepthLuma == BITDEPTH_8);
        bool b10bit = (pCO3->TargetBitDepthLuma == BITDEPTH_10);

        // range check
        mfxU32 invalid = (pCO3->TargetBitDepthLuma > 0) && (!(b8bit || b10bit));
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        // caps check
        invalid = (b8bit && !dpar.caps.BitDepthSupportFlags.fields.eight_bits) ||
                  (b10bit && !dpar.caps.BitDepthSupportFlags.fields.ten_bits);
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus FourCCByTargetFormat(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        dpar;
        par;
        return MFX_ERR_NONE;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.Check##X.Push(X);

        PUSH_DEFAULT(Level);
        PUSH_DEFAULT(SurfSize);
        PUSH_DEFAULT(Profile);
        PUSH_DEFAULT(FourCC);
        PUSH_DEFAULT(InputFormatByFourCC);
        PUSH_DEFAULT(TargetChromaFormat);
        PUSH_DEFAULT(TargetBitDepth);
        PUSH_DEFAULT(FourCCByTargetFormat);
#undef PUSH_DEFAULT
    }

};

void General::PushDefaults(Defaults& df)
{
    GetDefault::Push(df);
    PreCheck::Push(df);
    CheckAndFix::Push(df);
}

}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
