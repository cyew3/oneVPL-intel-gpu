//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "math.h"
#include "numeric"
#include "algorithm"

#include "ippi.h"

#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_lookahead.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"

#ifdef AMT_HROI_PSY_AQ
#include "FSapi.h"
#include <vector>
#endif


using namespace H265Enc;

//#define PAQ_LOGGING
template <typename PixType>
static
    void GetPredPelsLuma(PixType* src, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, PixType* PredPel, Ipp32s width, Ipp8u bitDepthLuma)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s i;

    dcval = (1 << (bitDepthLuma - 1));

    if ((x | y) == 0) {
        // Fill border with DC value
        for (i = 0; i <= 4*width; i++)
            PredPel[i] = dcval;
    } else if (x == 0) {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        Ipp32s hlim = IPP_MIN(roi.width - x, 2*width);
        for (i = 0; i < hlim; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[i];
        }
        for (i = hlim; i < 2*width; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];
        }
    } else if (y == 0) {
        // Fill top and top right border with rec. samples
        tmpSrcPtr = src - 1;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        Ipp32s vlim = IPP_MIN(roi.height - y, 2*width);
        for (i = 0; i < vlim; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
        }
    } else {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch - 1;
        PredPel[0] = tmpSrcPtr[0];

        // Fill top and top right border with rec. samples
        Ipp32s hlim = IPP_MIN(roi.width - x, 2*width);
        tmpSrcPtr = src - srcPitch;
        for (i = 0; i < hlim; i++)
            PredPel[1 + i] = tmpSrcPtr[i];
        for (i = hlim; i < 2*width; i++)
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];

        // Fill left and below left border with rec. samples
        Ipp32s vlim = IPP_MIN(roi.height - y, 2*width);
        tmpSrcPtr = src - 1;
        for (i = 0; i < vlim; i++) {
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++)
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
    }
}

static inline Ipp32s H265_Calc_SATD(Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pRef, Ipp32s width)
{
    Ipp32s numblks = width >> 3;
    Ipp32s satd = 0;
    for (Ipp32s i = 0; i < numblks; i++) {
        for (Ipp32s j = 0; j < numblks; j++) {
            Ipp8u *pS = pSrc + i*8*srcStep + j*8;
            Ipp8u *pR = pRef + i*8*width + j*8;
            satd +=  MFX_HEVC_PP::NAME(h265_SATD_8x8_8u)((const Ipp8u *)pS, srcStep, (const Ipp8u *)pR, width);
        }
    }
    satd = (satd + 2) >> 2; // to allign with hevce satd
    return satd;
}

static inline Ipp32s H265_Calc_SATD(Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pRef, Ipp32s width)
{
    Ipp32s numblks = width >> 3;
    Ipp32s satd = 0;
    for (Ipp32s i = 0; i < numblks; i++) {
        for (Ipp32s j = 0; j < numblks; j++) {
            Ipp16u *pS = pSrc + i*8*srcStep + j*8;
            Ipp16u *pR = pRef + i*8*width + j*8;
            satd +=  MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)pS, srcStep, (const Ipp16u *)pR, width);
        }
    }
    satd = (satd + 2) >> 2;// to allign with hevce satd
    return satd;
}


template <class PixType>
Ipp32s h265_SAD_MxN_general_1(const PixType *image, Ipp32s stride_img, const PixType *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY);

template <>
Ipp32s h265_SAD_MxN_general_1<Ipp8u>(const Ipp8u *image, Ipp32s stride_img, const Ipp8u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
{
    return MFX_HEVC_PP::h265_SAD_MxN_general_8u(ref, stride_ref, image, stride_img, SizeX, SizeY);
}

template <>
Ipp32s h265_SAD_MxN_general_1<Ipp16u>(const Ipp16u *image, Ipp32s stride_img, const Ipp16u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
{
    return MFX_HEVC_PP::NAME(h265_SAD_MxN_general_16s)((Ipp16s*)ref, stride_ref, (Ipp16s*)image, stride_img, SizeX, SizeY);
}

template <typename PixType>
Ipp32s tuHad_Kernel(const PixType *src, Ipp32s pitchSrc, const PixType *rec, Ipp32s pitchRec,
                    Ipp32s width, Ipp32s height)
{
    Ipp32u satdTotal = 0;
    Ipp32s satd[2] = {0, 0};

    /* assume height and width are multiple of 4 */
    VM_ASSERT(!(width & 0x03) && !(height & 0x03));

    /* test with optimized SATD source code */
    if (width == 4 && height == 4) {
        /* single 4x4 block */
        if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
        else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
        satdTotal += (satd[0] + 1) >> 1;
    } else if ( (height | width) & 0x07 ) {
        /* multiple 4x4 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x07;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 4, src += pitchSrc * 4, rec += pitchRec * 4) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 4*2) {
                if (sizeof(PixType) == 2)   MFX_HEVC_PP::NAME(h265_SATD_4x4_Pair_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec, satd);
                else                        MFX_HEVC_PP::NAME(h265_SATD_4x4_Pair_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
            }

            if (widthRem) {
                if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec);
                else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec);
                satdTotal += (satd[0] + 1) >> 1;
            }
        }
    } else if (width == 8 && height == 8) {
        /* single 8x8 block */
        if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
        else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
        satdTotal += (satd[0] + 2) >> 2;
    } else {
        /* multiple 8x8 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x0f;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 8*2) {
                if (sizeof(PixType) == 2)   MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec, satd);
                else                        MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
            }

            if (widthRem) {
                if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec);
                else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec);
                satdTotal += (satd[0] + 2) >> 2;
            }
        }
    }

    return satdTotal;
}

template
    Ipp32s tuHad_Kernel<Ipp8u>(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *rec, Ipp32s pitchRec, Ipp32s width, Ipp32s height);
template
    Ipp32s tuHad_Kernel<Ipp16u>(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *rec, Ipp32s pitchRec, Ipp32s width, Ipp32s height);


template <typename PixType>
void MeInterpolate(const H265MEInfo* me_info, const H265MV *MV, PixType *src,
                   Ipp32s srcPitch, PixType *dst, Ipp32s dstPitch, Ipp32s pelX, Ipp32s pelY)
{
    __ALIGN64 Ipp16s preAvgTmpBuf[72*64];
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx = MV->mvx & 3;
    Ipp32s dy = MV->mvy & 3;
    Ipp32s bitDepth = sizeof(PixType) == 1 ? 8 : 10;

    Ipp32s m_ctbPelX = pelX;
    Ipp32s m_ctbPelY = pelY;

    Ipp32s refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 2) + (m_ctbPelY + me_info->posy + (MV->mvy >> 2)) * srcPitch;
    src += refOffset;

    VM_ASSERT (!(dx == 0 && dy == 0));
    if (dy == 0) {
        InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, PixType>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32, bitDepth, preAvgTmpBuf);
    } else if (dx == 0) {
        InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, PixType>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32, bitDepth, preAvgTmpBuf);
    } else {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

        InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, Ipp16s>( INTERP_HOR, src - 1 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 3, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);

        Ipp32s shift  = 20 - bitDepth;
        Ipp16s offset = 1 << (shift - 1);

        InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, Ipp16s, PixType>( INTERP_VER,  tmp + 1 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf);
    }

    return;
} // 


template <typename PixType>
Ipp32s MatchingMetricPu(const PixType *src, Ipp32s srcPitch, FrameData *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY)
{
    Ipp32s refOffset = pelX + meInfo->posx + (mv->mvx >> 2) + (pelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    if ((mv->mvx | mv->mvy) & 3) {
        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, pelX, pelY);
        rec = predBuf;
        pitchRec = MAX_CU_SIZE;

        if(useHadamard)
            return (tuHad_Kernel(src, srcPitch, rec, pitchRec, SIZE_BLK_LA, SIZE_BLK_LA));
        else
            return h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, SIZE_BLK_LA, SIZE_BLK_LA);

    } else {
        if(useHadamard)
            return (tuHad_Kernel(src, srcPitch, rec, pitchRec, SIZE_BLK_LA, SIZE_BLK_LA));
        else
            return h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, SIZE_BLK_LA, SIZE_BLK_LA);
    }
}

template 
    Ipp32s MatchingMetricPu<Ipp8u>(const Ipp8u *src, Ipp32s srcPitch, FrameData *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY);

template <typename PixType>
Ipp32s IntraPredSATD(PixType *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma, Ipp32s modes)
{
    typedef typename GetHistogramType<PixType>::type HistType;
    PixType PredPel[4*2*64+1];
    __ALIGN32 PixType recPel[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 HistType hist8[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];

    pSrc += y*srcPitch + x;
    Ipp32s numblks = width >> 3;

    GetPredPelsLuma(pSrc, srcPitch, x, y, roi, PredPel, width, bitDepthLuma);

    h265_PredictIntra_DC(PredPel, recPel, width, width, 1);
    Ipp32s bestSATD = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if(!modes) return bestSATD;

    h265_PredictIntra_Ver(PredPel, recPel, width, width, bitDepthLuma, 1);
    Ipp32s satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    h265_PredictIntra_Hor(PredPel, recPel, width, width, bitDepthLuma, 1);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    h265_AnalyzeGradient(pSrc, srcPitch, hist8, hist8, width, width);
    if (numblks > 1) {
        Ipp32s pitch = 40 * numblks;
        const HistType *histBlock = hist8;
        for (Ipp32s py = 0; py < numblks; py++, histBlock += pitch)
            for (Ipp32s px = 0; px < numblks; px++)
                if (px | py) {
                    for (Ipp32s i = 0; i < 35; i++)
                        hist8[i] += histBlock[40 * px + i];
                }
    }
    Ipp32s angmode = (Ipp32s)(std::max_element(hist8 + 2, hist8 + 35) - hist8);
    Ipp32s diff = MIN(IPP_ABS(angmode - INTRA_HOR), IPP_ABS(angmode - INTRA_VER));
    bool isFilterNeeded = (diff > h265_filteredModes[h265_log2m2[width]]);
    if (!isFilterNeeded && diff > 0) {
        h265_PredictIntra_Ang(angmode, PredPel, recPel, width, width);
        satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
        if (satd < bestSATD)
            bestSATD = satd;
    }

    h265_FilterPredictPels(PredPel, width);

    if (isFilterNeeded && diff > 0) {
        h265_PredictIntra_Ang(angmode, PredPel, recPel, width, width);
        satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
        if (satd < bestSATD)
            bestSATD = satd;
    }

    h265_PredictIntra_Planar(PredPel, recPel, width, width);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    return bestSATD;
}

template Ipp32s IntraPredSATD<Ipp8u>(Ipp8u *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma, Ipp32s modes);

template <typename PixType>
void DoIntraAnalysis(FrameData* frame, Ipp32s* intraCost)
{
    Ipp32s width  = frame->width;
    Ipp32s height = frame->height;
    PixType* luma   = (PixType*)frame->y;
    Ipp8u bitDepth = sizeof(PixType) == 1 ? 8 : 10;;
    Ipp32s pitch_luma_pix = frame->pitch_luma_pix;
    Ipp8u  bitDepthLuma = bitDepth;

    // analysis for blk 8x8
    const Ipp32s sizeBlk = SIZE_BLK_LA;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {width, height};

    for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            intraCost[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, bitDepthLuma);
        }
    }
}

template <typename PixType>
void DoIntraAnalysis_OneRow(FrameData* frame, Ipp32s* intraCost, Ipp32s row, Ipp32s sizeBlk, Ipp32s modes)
{
    Ipp32s width  = frame->width;
    Ipp32s height = frame->height;
    PixType* luma   = (PixType*)frame->y;
    Ipp8u bitDepth = sizeof(PixType) == 1 ? 8 : 10;;
    Ipp32s pitch_luma_pix = frame->pitch_luma_pix;
    Ipp8u  bitDepthLuma = bitDepth;

    // analysis for blk 8x8
    //const Ipp32s sizeBlk = SIZE_BLK_LA;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {width, height};

    //for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++)
    Ipp32s blk_row = row;
    {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            intraCost[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, bitDepthLuma, modes);
        }
    }
}

Ipp32s ClipMV(H265MV& rcMv, Ipp32s HorMin, Ipp32s HorMax, Ipp32s VerMin, Ipp32s VerMax)
{
    Ipp32s change = 0;
    if (rcMv.mvx < HorMin) {
        change = 1;
        rcMv.mvx = (Ipp16s)HorMin;
    } else if (rcMv.mvx > HorMax) {
        change = 1;
        rcMv.mvx = (Ipp16s)HorMax;
    }
    if (rcMv.mvy < VerMin) {
        change = 1;
        rcMv.mvy = (Ipp16s)VerMin;
    } else if (rcMv.mvy > VerMax) {
        change = 1;
        rcMv.mvy = (Ipp16s)VerMax;
    }

    return change;
}

const Ipp16s tab_mePatternSelector[2][12][2] = {
    {{0,0}, {0,-1},  {1,0},  {0,1},  {-1,0}, {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}}, //diamond
    {{0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0},  {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}} //box
};

// Cyclic pattern to avoid trying already checked positions
const Ipp16s tab_mePattern[1 + 8 + 3][2] = {
    {0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
};

const struct TransitionTable {
    Ipp8u start;
    Ipp8u len;
} tab_meTransition[1 + 8 + 3] = {
    {1,8}, {7,5}, {1,3}, {1,5}, {3,3}, {3,5}, {5,3}, {5,5}, {7,3}, {7,5}, {1,3}, {1,5}
};

template <typename PixType, typename Frame>
void MeIntPelLog(Frame *curr, Frame *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost)
{
    Ipp16s meStepBest = 2; 
    Ipp16s meStepMax = MAX(MIN(8, 8), 16) * 4;
    // regulation from outside
    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    PixType *src = m_ySrc + posx + posy * curr->pitch_luma_pix;

    Ipp32s useHadamard = 0;
    Ipp16s mePosBest = 0;

    // limits to clip MV allover the CU
    const Ipp32s MvShift = 2;
    const Ipp32s offset = 8;
    const Ipp32s blkSize = SIZE_BLK_LA;
    Ipp32s HorMax = (curr->width + offset - pelX - 1) << MvShift;
    Ipp32s HorMin = ( - (Ipp32s)blkSize - offset - (Ipp32s) pelX + 1) << MvShift;
    Ipp32s VerMax = (curr->height + offset - pelY - 1) << MvShift;
    Ipp32s VerMin = ( - (Ipp32s) blkSize - offset - (Ipp32s) pelY + 1) << MvShift;

    // expanding search
    H265MV mvCenter = mvBest;
    for (Ipp16s meStep = meStepBest; (1<<meStep) <= meStepMax; meStep ++) {
        for (Ipp16s mePos = 0; mePos < 9; mePos++) {
            H265MV mv = {
                Ipp16s(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                Ipp16s(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };

            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            //Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
            H265MEInfo meInfo;
            meInfo.posx = (Ipp8u)posx;
            meInfo.posy = (Ipp8u)posy;
            Ipp32s cost = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, &mv, useHadamard, pelX, pelY);

            if (costBest > cost) {
                Ipp32s mvCost = 0;//MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    meStepBest = meStep;
                    mePosBest = mePos;
                }
            }
        }
    }

    // then logarithm from best
    // for Cpattern
    Ipp32s transPos = mePosBest;
    Ipp8u start = 0, len = 1;
    Ipp16s meStep = meStepBest;

    mvCenter = mvBest;
    Ipp32s iterbest = costBest;
    while (meStep >= 2) {
        Ipp32s refine = 1;
        Ipp32s bestPos = 0;
        for (Ipp16s mePos = start; mePos < start + len; mePos++) {
            H265MV mv = {
                Ipp16s(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                Ipp16s(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };
            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            //Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
            H265MEInfo meInfo;
            meInfo.posx = (Ipp8u)posx;
            meInfo.posy = (Ipp8u)posy;
            Ipp32s cost = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, &mv, useHadamard, pelX, pelY);

            if (costBest > cost) {
                Ipp32s mvCost = 0;//MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    refine = 0;
                    bestPos = mePos;
                }
            }
        }

        if (refine) {
            meStep --;
        } else {
            mvCenter = mvBest;
        }
        start = tab_meTransition[bestPos].start;
        len   = tab_meTransition[bestPos].len;
        iterbest = costBest;
        transPos = bestPos;
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;

    if(cost_satd) {
        // recalc hadamar
        H265MEInfo meInfo;
        meInfo.posx = (Ipp8u)posx;
        meInfo.posy = (Ipp8u)posy;
        useHadamard = 1;
        *cost_satd = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, mv, useHadamard, pelX, pelY);
    }
} //


template <typename PixType>
void MeIntPelLog_Refine(FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost, Ipp32s blkSize)
{
    Ipp16s meStepBest = 2; 
    Ipp16s meStepMax = 8;

    // regulation from outside
    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    PixType *src = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    Ipp32s srcPitch = curr->pitch_luma_pix;
    Ipp32s useHadamard = 0;
    Ipp16s mePosBest = 0;

    // limits to clip MV allover the CU
    const Ipp32s MvShift = 2;
    const Ipp32s offset = 8;
    
    Ipp32s HorMax = (curr->width + offset - pelX - 1) << MvShift;
    Ipp32s HorMin = ( - (Ipp32s)blkSize - offset - (Ipp32s) pelX + 1) << MvShift;
    Ipp32s VerMax = (curr->height + offset - pelY - 1) << MvShift;
    Ipp32s VerMin = ( - (Ipp32s) blkSize - offset - (Ipp32s) pelY + 1) << MvShift;

    // expanding search
    H265MV mvCenter = mvBest;
    for (Ipp16s meStep = meStepBest; (1<<meStep) <= meStepMax; meStep ++) {
        for (Ipp16s mePos = 0; mePos < 9; mePos++) {
            H265MV mv = {
                Ipp16s(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                Ipp16s(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };

            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            Ipp32s refOffset = pelX + (mv.mvx >> 2) + (pelY + (mv.mvy >> 2)) * ref->pitch_luma_pix;
            const PixType *rec = (PixType*)ref->y + refOffset;
            Ipp32s pitchRec = ref->pitch_luma_pix;
            Ipp32s cost = h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, blkSize, blkSize);

            if (costBest > cost) {
                costBest = cost;
                mvCostBest = 0;
                mvBest = mv;
                meStepBest = meStep;
                mePosBest = mePos;
            }
        }
    }
    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
    if (cost_satd) {
        // recalc hadamar
        Ipp32s refOffset = pelX + (mv->mvx >> 2) + (pelY + (mv->mvy >> 2)) * ref->pitch_luma_pix;
        const PixType *rec = (PixType*)ref->y + refOffset;
        *cost_satd = tuHad_Kernel(src, srcPitch, rec, ref->pitch_luma_pix, blkSize, blkSize);
    }
    return;
} //


void AddMvCost(Ipp32s log2Step, const Ipp32s *dists,
               H265MV *mv, Ipp32s *costBest, Ipp32s *mvCostBest, Ipp32s patternSubPel) 
{
    const Ipp16s (*pattern)[2] = tab_mePatternSelector[Ipp32s(patternSubPel == SUBPEL_BOX)] + 1;
    Ipp32s numMvs = (patternSubPel == SUBPEL_BOX) ? 8 : 4; // BOX or DIA

    H265MV centerMv = *mv;
    for (Ipp32s i = 0; i < numMvs; i++) {
        Ipp32s cost = dists[i];
        if (*costBest > cost) {
            Ipp16s mvx = centerMv.mvx + (pattern[i][0] << log2Step);
            Ipp16s mvy = centerMv.mvy + (pattern[i][1] << log2Step);
            Ipp32s mvCost = 0;//MvCost1RefLog(mvx, mvy, predInfo);
            cost += mvCost;
            if (*costBest > cost) {
                *costBest = cost;
                *mvCostBest = mvCost;
                mv->mvx = mvx;
                mv->mvy = mvy;
            }
        }
    }
}

const UMC_HEVC_DECODER::EnumTextType LUMA = (UMC_HEVC_DECODER::EnumTextType)TEXT_LUMA;

template <typename TSrc, typename TDst>
void InterpHor(const TSrc *src, Ipp32s pitchSrc, TDst *dst, Ipp32s pitchDst, Ipp32s dx,
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth, Ipp16s *tmpBuf)
{
    InterpolateEncFast<LUMA, TSrc, TDst>(INTERP_HOR, src, pitchSrc, dst, pitchDst, dx, width, height, shift, offset, bitDepth, tmpBuf);//isFast
}

template <typename TSrc, typename TDst>
void InterpVer(const TSrc *src, Ipp32s pitchSrc, TDst *dst, Ipp32s pitchDst, Ipp32s dy,
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth, Ipp16s *tmpBuf)
{
    InterpolateEncFast<LUMA, TSrc, TDst>(INTERP_VER, src, pitchSrc, dst, pitchDst, dy, width, height, shift, offset, bitDepth, tmpBuf);//isFast
}

template <typename PixType>
void MeSubPelBatchedBox(const H265MEInfo *meInfo, 
                        FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s *cost_satd, bool bQPel)
{
    Ipp32s m_ctbPelX = pelX;
    Ipp32s m_ctbPelY = pelY;
    Ipp32s m_pitchSrcLuma = curr->pitch_luma_pix;
    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;

    Ipp32s patternSubPel = SUBPEL_BOX;
    Ipp32s bitDepthLuma = sizeof(PixType) == 1 ? 8 : 10;
    Ipp32s bitDepthLumaShift = bitDepthLuma - 8;
    Ipp32s hadamardMe = 0;

    VM_ASSERT(patternSubPel == SUBPEL_BOX);
    using MFX_HEVC_PP::h265_InterpLumaPack;

    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s bitDepth = bitDepthLuma;
    Ipp32s costShift = bitDepthLumaShift;
    Ipp32s shift = 20 - bitDepth;
    Ipp32s offset = 1 << (19 - bitDepth);
    Ipp32s useHadamard = (hadamardMe >= 2);

    Ipp32s pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;

    Ipp32s pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (Ipp32s)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    Ipp32s (*costFunc)(const PixType*, Ipp32s, const PixType*, Ipp32s, Ipp32s, Ipp32s);
    costFunc = (useHadamard ? tuHad_Kernel<PixType> : h265_SAD_MxN_general_1<PixType>);
    Ipp32s costs[8];

    Ipp32s pitchTmp2 = (w + 1 + 15) & ~15;
    Ipp32s pitchHpel = (w + 1 + 15) & ~15;
    ALIGN_DECL(32) Ipp16s tmpPels[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 8)];
    ALIGN_DECL(32) PixType subpel[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 2)];
    __ALIGN64 Ipp16s preAvgTmpBuf[72*64];

    // intermediate halfpels
    InterpHor(refY - 1 - 4 * pitchRef, pitchRef, tmpPels, pitchTmp2, 2, (w + 8) & ~7, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf); // (intermediate)
    Ipp16s *tmpHor2 = tmpPels + 3 * pitchTmp2;

    // interpolate horizontal halfpels
    h265_InterpLumaPack(tmpHor2 + pitchTmp2, pitchTmp2, subpel, pitchHpel, w + 1, h, bitDepth);
    costs[3] = costFunc(src, pitchSrc, subpel + 1, pitchHpel, w, h) >> costShift;
    costs[7] = costFunc(src, pitchSrc, subpel,     pitchHpel, w, h) >> costShift;

    // interpolate diagonal halfpels
    InterpVer(tmpHor2, pitchTmp2, subpel, pitchHpel, 2, (w + 8) & ~7, h + 2, shift, offset, bitDepth, preAvgTmpBuf);
    costs[0] = costFunc(src, pitchSrc, subpel,                 pitchHpel, w, h) >> costShift;
    costs[2] = costFunc(src, pitchSrc, subpel + 1,             pitchHpel, w, h) >> costShift;
    costs[4] = costFunc(src, pitchSrc, subpel + 1 + pitchHpel, pitchHpel, w, h) >> costShift;
    costs[6] = costFunc(src, pitchSrc, subpel     + pitchHpel, pitchHpel, w, h) >> costShift;

    // interpolate vertical halfpels
    pitchHpel = (w + 15) & ~15;
    InterpVer(refY - pitchRef, pitchRef, subpel, pitchHpel, 2, w, h + 2, 6, 32, bitDepth, preAvgTmpBuf);
    costs[1] = costFunc(src, pitchSrc, subpel,             pitchHpel, w, h) >> costShift;
    costs[5] = costFunc(src, pitchSrc, subpel + pitchHpel, pitchHpel, w, h) >> costShift;

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    if (hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = tuHad(src, pitchSrc, refY, pitchRef, w, h) >> costShift;
        costBest += mvCostBest;
    }

    AddMvCost(/*predInfo,*/ 1, costs, &mvBest, &costBest, &mvCostBest, patternSubPel);

    if (bQPel) {
        Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
        Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
        Ipp32s dx = hpelx & 3; // can be 0 or 2
        Ipp32s dy = hpely & 3; // can be 0 or 2

        Ipp32s pitchQpel = (w + 15) & ~15;

        // interpolate vertical quater-pels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, 3 - dy, w, h, 6, 32, bitDepth, preAvgTmpBuf);                             // hpx+0 hpy-1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely - 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+0 hpy-1/4
        costs[1] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate vertical qpels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, dy + 1, w, h, 6, 32, bitDepth, preAvgTmpBuf);                             // hpx+0 hpy+1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely + 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+0 hpy+1/4
        costs[5] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // intermediate horizontal quater-pels (left of best half-pel)
        Ipp32s pitchTmp1 = (w + 15) & ~15;
        Ipp16s *tmpHor1 = tmpPels + 3 * pitchTmp1;
        InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, tmpPels, pitchTmp1, 3 - dx, w, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);  // hpx-1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (left of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
            h265_InterpLumaPack(tmpHor1 + pitchTmp1, pitchTmp1, subpel, pitchHpel, w, h, bitDepth); // hpx-1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor1 + (hpely >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy+0
        costs[7] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel)
        InterpVer(tmpHor1 + (hpely - 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy-1/4
        costs[0] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
        InterpVer(tmpHor1 + (hpely + 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy+1/4
        costs[6] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // intermediate horizontal quater-pels (right of best half-pel)
        Ipp32s pitchTmp3 = (w + 15) & ~15;
        Ipp16s *tmpHor3 = tmpPels + 3 * pitchTmp3;
        InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, tmpPels, pitchTmp3, dx + 1, w, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);  // hpx+1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (right of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
            h265_InterpLumaPack(tmpHor3 + pitchTmp3, pitchTmp3, subpel, pitchHpel, w, h, bitDepth); // hpx+1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor3 + (hpely >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy+0
        costs[3] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel)
        InterpVer(tmpHor3 + (hpely - 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy-1/4
        costs[2] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
        InterpVer(tmpHor3 + (hpely + 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy+1/4
        costs[4] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        AddMvCost(/*predInfo,*/ 0, costs, &mvBest, &costBest, &mvCostBest, patternSubPel);
    }
    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;

    // second metric
    if (cost_satd) {
        Ipp32s useHadamard = 1;
        *cost_satd = MatchingMetricPu(src, pitchSrc, ref, meInfo, &mvBest, useHadamard, pelX, pelY);
    }
}


template
    void MeIntPelLog<Ipp8u>(FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost);
template 
    void MeIntPelLog_Refine<Ipp8u>(FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost, Ipp32s blkSize);

// mode: 0 - means regular ME{ F(i), F(i+1) }, 1 - means pdist Future ME { P, Pnext }, 2 - means pdist Past ME { P, Pprev }
void DoInterAnalysis(FrameData* curr, Statistics* currStat, FrameData* ref, Ipp32s* sad, Ipp32s* satd, H265MV* out_mv, Ipp8u isRefine, Ipp8u mode, Ipp32s lowresFactor, Ipp8u bitDepthLuma) 
{
    // analysis for blk 8x8.
    Ipp32s width  = curr->width;
    Ipp32s height = curr->height;
    const Ipp32s sizeBlk = SIZE_BLK_LA;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    Ipp32s bitDepth8 = (bitDepthLuma == 8) ? 1 : 0;

    for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;

            H265MV mv = {0, 0};
            Ipp32s cost = INT_MAX;
            Ipp32s cost_satd = 0;
            Ipp32s mvCost = 0;//INT_MAX;

            if (isRefine) {
                Ipp32s widthLowres  = width  >> lowresFactor;
                Ipp32s heightLowres = height >> lowresFactor;
                const Ipp32s sizeBlkLowres = SIZE_BLK_LA;
                const Ipp32s picWidthInBlksLowres  = (widthLowres  + sizeBlkLowres - 1) / sizeBlkLowres;
                const Ipp32s picHeightInBlksLowres = (heightLowres + sizeBlkLowres - 1) / sizeBlkLowres;
                Ipp32s fPosLowres = (blk_row >> lowresFactor) * picWidthInBlksLowres + (blk_col >> lowresFactor);

                if (mode == 0) {
                    mv   = currStat->m_mv[fPosLowres];
                    cost = currStat->m_interSad[fPosLowres];
                } else if (mode == 1) {
                    mv   = currStat->m_mv_pdist_future[fPosLowres];
                    cost = currStat->m_interSad_pdist_future[fPosLowres];
                } else {
                    mv   = currStat->m_mv_pdist_past[fPosLowres];
                    cost = currStat->m_interSad_pdist_past[fPosLowres];
                }

                mv.mvx <<= lowresFactor;
                mv.mvy <<= lowresFactor;

                // recalculate to nearest int-pel
                mv.mvx = (mv.mvx + 1) & ~3;
                mv.mvy = (mv.mvy + 1) & ~3;
                {
                    // limits to clip MV allover the CU
                    Ipp32s pelX = x;
                    Ipp32s pelY = y;
                    const Ipp32s MvShift = 2;
                    const Ipp32s offset = 8;
                    const Ipp32s blkSize = SIZE_BLK_LA;
                    Ipp32s HorMax = (width + offset - pelX - 1) << MvShift;
                    Ipp32s HorMin = ( - (Ipp32s)blkSize - offset - (Ipp32s) pelX + 1) << MvShift;
                    Ipp32s VerMax = (curr->height + offset - pelY - 1) << MvShift;
                    Ipp32s VerMin = ( - (Ipp32s) blkSize - offset - (Ipp32s) pelY + 1) << MvShift;

                    ClipMV(mv, HorMin, HorMax, VerMin, VerMax);
                }


                MeIntPelLog_Refine<Ipp8u>(curr, ref, x, y, &mv, &cost, &cost_satd, &mvCost, SIZE_BLK_LA);

            } else {

                if (bitDepth8)
                    MeIntPelLog<Ipp8u>(curr, ref, x, y, 0, 0, &mv, &cost, &cost_satd, &mvCost);
                else
                    MeIntPelLog<Ipp16u>(curr, ref, x, y, 0, 0, &mv, &cost, &cost_satd, &mvCost);
            }

            {
                H265MV mv_sad = mv;
                H265MEInfo meInfo;
                meInfo.posx = 0;
                meInfo.posy = 0;
                meInfo.width  = SIZE_BLK_LA;
                meInfo.height = SIZE_BLK_LA;

                if (bitDepth8)
                    MeSubPelBatchedBox<Ipp8u>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */satd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);
                else
                    MeSubPelBatchedBox<Ipp16u>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */satd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);

                sad[fPos] = cost;
                out_mv[fPos] = mv_sad;
            }
        }
    }
}

void DoInterAnalysis_OneRow(FrameData* curr, Statistics* currStat, FrameData* ref, Ipp32s* sad, Ipp32s* satd, H265MV* out_mv, Ipp8u isRefine, Ipp8u mode, Ipp32s lowresFactor, Ipp8u bitDepthLuma, Ipp32s row, FrameData* frame=NULL, FrameData* framePrev=NULL) 
{
    // analysis for blk 8x8.
    Ipp32s width  = curr->width;
    Ipp32s height = curr->height;
    const Ipp32s sizeBlk = SIZE_BLK_LA;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    Ipp32s bitDepth8 = (bitDepthLuma == 8) ? 1 : 0;
    const IppiSize frmSize = {width, height};
    //for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
    Ipp32s blk_row = row;
    for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
        Ipp32s x = blk_col*sizeBlk;
        Ipp32s y = blk_row*sizeBlk;
        Ipp32s fPos = blk_row * picWidthInBlks + blk_col;

        H265MV mv = {0, 0};
        Ipp32s cost = INT_MAX;
        Ipp32s cost_satd = 0;
        Ipp32s mvCost = 0;//INT_MAX;

        if (isRefine) {
            Ipp32s widthLowres  = width  >> lowresFactor;
            Ipp32s heightLowres = height >> lowresFactor;
            const Ipp32s sizeBlkLowres = SIZE_BLK_LA;
            const Ipp32s picWidthInBlksLowres  = (widthLowres  + sizeBlkLowres - 1) / sizeBlkLowres;
            const Ipp32s picHeightInBlksLowres = (heightLowres + sizeBlkLowres - 1) / sizeBlkLowres;
            Ipp32s fPosLowres = (blk_row >> lowresFactor) * picWidthInBlksLowres + (blk_col >> lowresFactor);

            if (mode == 0) {
                mv   = currStat->m_mv[fPosLowres];
                cost = currStat->m_interSad[fPosLowres];
            } else if (mode == 1) {
                mv   = currStat->m_mv_pdist_future[fPosLowres];
                cost = currStat->m_interSad_pdist_future[fPosLowres];
            } else {
                mv   = currStat->m_mv_pdist_past[fPosLowres];
                cost = currStat->m_interSad_pdist_past[fPosLowres];
            }

            mv.mvx <<= lowresFactor;
            mv.mvy <<= lowresFactor;
            // recalculate to nearest int-pel
            mv.mvx = (mv.mvx + 1) & ~3;
            mv.mvy = (mv.mvy + 1) & ~3;
            //---------
            {
                // limits to clip MV allover the CU
                Ipp32s pelX = x;
                Ipp32s pelY = y;
                const Ipp32s MvShift = 2;
                const Ipp32s offset = 8;
                const Ipp32s blkSize = SIZE_BLK_LA;
                Ipp32s HorMax = (width + offset - pelX - 1) << MvShift;
                Ipp32s HorMin = ( - (Ipp32s)blkSize - offset - (Ipp32s) pelX + 1) << MvShift;
                Ipp32s VerMax = (curr->height + offset - pelY - 1) << MvShift;
                Ipp32s VerMin = ( - (Ipp32s) blkSize - offset - (Ipp32s) pelY + 1) << MvShift;

                ClipMV(mv, HorMin, HorMax, VerMin, VerMax);
            }
            //---------
            if (bitDepth8) {
                //MeIntPelLog_Refine<Ipp8u>(curr, ref, x, y, 0, 0, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost);
                Ipp8u *src = curr->y + x + y * curr->pitch_luma_pix;
                Ipp32s refOffset = x + (mv.mvx >> 2) + (y + (mv.mvy >> 2)) * ref->pitch_luma_pix;
                const Ipp8u *rec = ref->y + refOffset;
                Ipp32s pitchRec = ref->pitch_luma_pix;
                cost = h265_SAD_MxN_general_1(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, SIZE_BLK_LA, SIZE_BLK_LA);
                mvCost = 0;
            }
            else
            {
                Ipp16u *src = (Ipp16u *)curr->y + x + y * curr->pitch_luma_pix;
                Ipp32s refOffset = x + (mv.mvx >> 2) + (y + (mv.mvy >> 2)) * ref->pitch_luma_pix;
                const Ipp16u *rec = (Ipp16u *)ref->y + refOffset;
                Ipp32s pitchRec = ref->pitch_luma_pix;
                cost = h265_SAD_MxN_general_1(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, SIZE_BLK_LA, SIZE_BLK_LA);
                mvCost = 0;
            }

        } else {

            if (bitDepth8)
                MeIntPelLog<Ipp8u>(curr, ref, x, y, 0, 0, &mv, &cost, /*satd ? (&cost_satd) :*/ NULL, &mvCost);
            else
                MeIntPelLog<Ipp16u>(curr, ref, x, y, 0, 0, &mv, &cost, /*satd ? (&cost_satd) :*/ NULL, &mvCost);

            sad[fPos] = cost;
            out_mv[fPos] = mv;

        }

        {
            H265MV mv_sad = mv;
            H265MEInfo meInfo;
            meInfo.posx = 0;
            meInfo.posy = 0;
            meInfo.width  = SIZE_BLK_LA;
            meInfo.height = SIZE_BLK_LA;
            Ipp32s subSatd = (satd) ? 1: 0;
            if(lowresFactor && !isRefine && frame && framePrev) subSatd = 0;

            if (bitDepth8)
                MeSubPelBatchedBox<Ipp8u>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */subSatd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);
            else
                MeSubPelBatchedBox<Ipp16u>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */subSatd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);

            sad[fPos] = cost;
            out_mv[fPos] = mv_sad;

            if (lowresFactor && !isRefine && frame && framePrev) {
                // LowresFactor FullresMetrics for GopCqBrc
                Ipp32s widthFullres  = width  << lowresFactor;
                Ipp32s heightFullres = height << lowresFactor;
                const Ipp32s sizeBlkFullres = SIZE_BLK_LA << lowresFactor;
                Ipp32s xFullres = blk_col * sizeBlkFullres;
                Ipp32s yFullres = blk_row * sizeBlkFullres;
                const IppiSize frmSizeFullres = {widthFullres, heightFullres};
                H265MV mvFullres = {0, 0};
                mvFullres.mvx = mv_sad.mvx << lowresFactor;
                mvFullres.mvy = mv_sad.mvy << lowresFactor;
                if (bitDepth8) {
                    Ipp8u *src = frame->y + xFullres + yFullres * frame->pitch_luma_pix;
                    Ipp32s refOffset = xFullres + (mvFullres.mvx >> 2) + (yFullres + (mvFullres.mvy >> 2)) * framePrev->pitch_luma_pix;
                    const Ipp8u *rec = framePrev->y + refOffset;
                    cost = h265_SAD_MxN_general_1(src, frame->pitch_luma_pix, rec, framePrev->pitch_luma_pix, sizeBlkFullres, sizeBlkFullres);
                    MeIntPelLog_Refine<Ipp8u>(frame, framePrev, xFullres, yFullres, &mvFullres, &cost, satd ? (satd + fPos) : NULL, &mvCost, sizeBlkFullres);
                } else {
                    Ipp16u *src = (Ipp16u *)frame->y + xFullres + yFullres * frame->pitch_luma_pix;
                    Ipp32s refOffset = xFullres + (mvFullres.mvx >> 2) + (yFullres + (mvFullres.mvy >> 2)) * framePrev->pitch_luma_pix;
                    const Ipp16u *rec = (Ipp16u *)framePrev->y + refOffset;
                    cost = h265_SAD_MxN_general_1(src, frame->pitch_luma_pix, rec, framePrev->pitch_luma_pix, sizeBlkFullres, sizeBlkFullres);
                    MeIntPelLog_Refine<Ipp16u>(frame, framePrev, xFullres, yFullres, &mvFullres, &cost, satd ? (satd + fPos) : NULL, &mvCost, sizeBlkFullres);
                }

                mv_sad.mvx = mvFullres.mvx >> lowresFactor;
                mv_sad.mvy = mvFullres.mvy >> lowresFactor;
                out_mv[fPos] = mv_sad;
                sad[fPos] = cost;
            }
        }
    }
    //}
}


template <class PixType>
PixType AvgPixBlk(PixType* in, Ipp32s pitch, Ipp32s posx, Ipp32s posy, Ipp32s step)
{
    Ipp32s posx_orig = posx * step;
    Ipp32s posy_orig = posy * step;

    PixType* src = in + posy_orig*pitch + posx_orig;
    Ipp64s sum = 0;

    for (Ipp32s y = 0; y < step; y++) {
        for (Ipp32s x = 0; x < step; x++) {
            sum += src[ y*pitch + x ];
        }
    }

    return (PixType) ((sum + (step * step / 2)) / (step * step));
}

template <class PixType>
PixType NearestPixBlk(PixType* in, Ipp32s pitch, Ipp32s posx, Ipp32s posy, Ipp32s step)
{
    Ipp32s posx_orig = posx * step;
    Ipp32s posy_orig = posy * step;

    PixType* src = in + posy_orig*pitch + posx_orig;
    //return src[0];

    // the same as IppNearest
    Ipp32s pos_x = (step >> 1) - 1;
    Ipp32s pos_y = (step >> 1) - 1;
    return src[pos_y*pitch + pos_x];
}

template <class PixType>
Ipp64s Scale(FrameData* in, FrameData* out)
{
    const PixType *pixIn = (const PixType *)in->y;
    PixType *pixOut = (PixType *)out->y;
    Ipp32s widthOrig  = in->width;
    Ipp32s heightOrig = in->height;

    Ipp32s widthOut  = out->width;
    Ipp32s heightOut = out->height;

    Ipp32s pitchIn = in->pitch_luma_pix;
    Ipp32s pitchOut = out->pitch_luma_pix;
    Ipp32s step = widthOrig / widthOut;

    for ( Ipp32s y = 0; y < heightOut; y++ ) {
        for (Ipp32s x = 0; x < widthOut; x++) {
            //pixOut[ y*pitchOut + x] = AvgPixBlk( pixIn, pitchIn, x, y, step);
            pixOut[ y*pitchOut + x] = NearestPixBlk( pixIn, pitchIn, x, y, step);
        }
    }
    return 0;
}


template <class PixType, typename Frame>
Ipp64s CalcPixDiff(Frame *prev, Frame *curr, Ipp32s planeIdx)
{
    const PixType *pixPrev = (const PixType *)prev->y;
    const PixType *pixCurr = (const PixType *)curr->y;
    Ipp32s width = prev->width;
    Ipp32s height = prev->height;
    Ipp32s pitchPrev = prev->pitch_luma_pix;
    Ipp32s pitchCurr = curr->pitch_luma_pix;

    if (planeIdx > 0) {
        pixPrev = (const PixType *)prev->uv;
        pixCurr = (const PixType *)curr->uv;
        pitchPrev = prev->pitch_chroma_pix;
        pitchCurr = curr->pitch_chroma_pix;
        height /= 2; // TODO: YUV420
    }

    Ipp64s diff = 0;
    for (Ipp32s y = 0; y < height; y++, pixPrev += pitchPrev, pixCurr += pitchCurr) {
        for (Ipp32s x = 0; x < width; x++) {
            diff += IPP_ABS(pixPrev[x] - pixCurr[x]);
        }
    }

    return diff;
}

template <class PixType>
void BuildHistLuma(const PixType *pix, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s bitDepth, Ipp64s *hist)
{
    const Ipp32s binMask = (1 << bitDepth) - 1;
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (Ipp32s y = 0; y < height; y++, pix += pitch)
        for (Ipp32s x = 0; x < width; x++)
            hist[pix[x] & binMask]++;
}

template <class PixType>
void BuildHistChroma(const PixType *pix, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s bitDepth, Ipp64s *hist)
{
    const Ipp32s binMask = (1 << bitDepth) - 1;
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (Ipp32s y = 0; y < height; y++, pix += pitch)
        for (Ipp32s x = 0; x < width * 2; x += 2)
            hist[pix[x] & binMask]++;
}

template <class PixType, class Frame>
Ipp64s CalcHistDiff(Frame *prev, Frame *curr, Ipp32s planeIdx)
{
    const Ipp32s maxNumBins = 1 << (sizeof(PixType) == 1 ? 8 : 10);
    Ipp64s histPrev[maxNumBins];
    Ipp64s histCurr[maxNumBins];
    Ipp32s bitDepth = sizeof(PixType) == 1 ? 8 : 10;

    if (planeIdx == 0) {
        const PixType *pixPrev = (const PixType *)prev->y;
        const PixType *pixCurr = (const PixType *)curr->y;
        Ipp32s width = prev->width;
        Ipp32s height = prev->height;
        Ipp32s pitchPrev = prev->pitch_luma_pix;
        Ipp32s pitchCurr = curr->pitch_luma_pix;

        BuildHistLuma<PixType>(pixPrev, pitchPrev, width, height, bitDepth, histPrev);
        BuildHistLuma<PixType>(pixCurr, pitchCurr, width, height, bitDepth, histCurr);
    }
    //else {
    //    const PixType *pixPrev = (const PixType *)prev->uv + (planeIdx - 1);
    //    const PixType *pixCurr = (const PixType *)curr->uv + (planeIdx - 1);
    //    Ipp32s width = prev->width / 2; // TODO: YUV422, YUV444
    //    Ipp32s height = prev->height / 2; // TODO: YUV422, YUV444
    //    Ipp32s pitchPrev = prev->pitch_chroma_pix;
    //    Ipp32s pitchCurr = curr->pitch_chroma_pix;
    //    bitDepth = prev->m_bitDepthChroma;

    //    BuildHistChroma<PixType>(pixPrev, pitchPrev, width, height, bitDepth, histPrev);
    //    BuildHistChroma<PixType>(pixCurr, pitchCurr, width, height, bitDepth, histCurr);
    //}

    Ipp32s numBins = 1 << bitDepth;
    Ipp64s diff = 0;
    for (Ipp32s i = 0; i < numBins; i++)
        diff += abs(histPrev[i] - histCurr[i]);

    return diff;
}


enum {
    BLOCK_SIZE = 4
};

// CalcRsCs_OneRow()  works with blk_8x8. (SIZE_BLK_LA)
template <class PixType>
void CalcRsCs_OneRow(FrameData* data, Statistics* stat, H265VideoParam& par, Ipp32s row)
{
    Ipp32s locx, locy;

    const Ipp32s RsCsSIZE = BLOCK_SIZE*BLOCK_SIZE;
    Ipp32s hblocks = (Ipp32s)data->height / BLOCK_SIZE;
    Ipp32s wblocks = (Ipp32s)data->width / BLOCK_SIZE;
    
    stat->m_frameCs = 0.0;
    stat->m_frameRs = 0.0;

    Ipp32s pitch = data->pitch_luma_pix;
    Ipp32s pitchRsCs4 = stat->m_pitchRsCs4;

    Ipp32s iStart = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * row;
    Ipp32s iEnd  = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * (row + 1);
    iEnd = IPP_MIN(iEnd, hblocks);

    for (Ipp32s j = 0; j < wblocks; j += 16) {
        PixType *ySrc = (PixType *)data->y + (iStart*BLOCK_SIZE)*pitch + BLOCK_SIZE*j;
        Ipp32s *rs = stat->m_rs[0] + iStart * pitchRsCs4 + j;
        Ipp32s *cs = stat->m_cs[0] + iStart * pitchRsCs4 + j;
        h265_ComputeRsCs(ySrc, pitch, rs, cs, pitchRsCs4, BLOCK_SIZE*16, BLOCK_SIZE*(iEnd-iStart));
    }
}


// 8x8 blk
Ipp32s GetSpatioTemporalComplexity_own(Ipp32s sad, Ipp32f scpp)
{
    const Ipp32s width = SIZE_BLK_LA;
    const Ipp32s height = SIZE_BLK_LA;
    float sadpp=(float)sad/(float)(width*height);
    sadpp=(sadpp*sadpp);
    if     (sadpp < 0.09*scpp )  return 0; // Very Low
    else if(sadpp < 0.36*scpp )  return 1; // Low
    else if(sadpp < 1.44*scpp )  return 2; // Medium
    else if(sadpp < 3.24*scpp )  return 3; // High
    else                         return 4; // VeryHigh
}

#define MAXPDIST 5
#define REFMOD_MAX      15

static void AddCandidate(H265MV *pMV, int *uMVnum, H265MV *MVCandidate) 
{
    int i;
    MVCandidate[*uMVnum] = *pMV;
    for (i = 0; i < *uMVnum; i ++)
    {        
        if (MVCandidate[*uMVnum].mvx == MVCandidate[i].mvx &&
            MVCandidate[*uMVnum].mvy == MVCandidate[i].mvy)
            break;
    }
    /* Test if Last Predictor vector is valid */
    if (i == *uMVnum)
    {
        (*uMVnum)++;
    }
}

struct isEqual
{
    isEqual(Ipp32s frameOrder): m_frameOrder(frameOrder)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_frameOrder == m_frameOrder;
    }

    Ipp32s m_frameOrder;
};


struct isEqualFrameType
{
    isEqualFrameType(Ipp32u frameType): m_frameType(frameType)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_origin->m_picCodeType == m_frameType;
    }

    Ipp32u m_frameType;
};

struct isEqualRefFrame
{
    isEqualRefFrame(){}

    template<typename T>
    bool operator()(const T& l)
    {
        return ((*l).m_picCodeType == MFX_FRAMETYPE_I) || ((*l).m_picCodeType == MFX_FRAMETYPE_P);
    }
};



void H265Enc::DetermineQpMap_IFrame(FrameIter curr, FrameIter end, H265VideoParam& videoParam)
{
    static const Ipp32f lmt_sc2[10] = { // lower limit of SFM(Rs,Cs) range for spatial classification
        4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, static_cast<Ipp32f>(INT_MAX)};
    const Ipp32s futr_qp = MAX_DQP;
    VM_ASSERT(videoParam.Log2MaxCUSize==6);
    Frame* inFrame = *curr;
    H265MV candMVs[(64/8)*(64/8)];
    Ipp32s LowresFactor = 0;
#ifdef LOW_COMPLX_PAQ
    LowresFactor = videoParam.LowresFactor;
#endif
    Ipp32s lowRes = LowresFactor ? 1 : 0;
    Ipp32s log2SubBlk = 3 + LowresFactor;
    Ipp32s widthInSubBlks = videoParam.Width>>3;
    if (lowRes) {
        widthInSubBlks = (inFrame->m_lowres->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
    }
    Ipp32s heightInCtbs = (Ipp32s)videoParam.PicHeightInCtbs;
    Ipp32s widthInCtbs = (Ipp32s)videoParam.PicWidthInCtbs;

    bool futr_key = false;
    Ipp32s REF_DIST = 8;

    FrameIter it1 = ++FrameIter(curr);
    Frame* futr_frame = inFrame;
    for (FrameIter it = it1; it != end; it++) {
        if(*it) {
            futr_frame = *it;
            if (futr_frame->m_picCodeType == MFX_FRAMETYPE_I) {
                futr_key = true;
            }
        }
    }
    REF_DIST = IPP_MIN(8, (inFrame->m_frameOrder - futr_frame->m_frameOrder));
    
    for (Ipp32s row=0; row<heightInCtbs; row++) {
        for (Ipp32s col=0; col<widthInCtbs; col++) {
            Ipp32s pelX = col * videoParam.MaxCUSize;
            Ipp32s pelY = row * videoParam.MaxCUSize;
            Ipp32s MaxDepth = IPP_MIN((Ipp32s) videoParam.Log2MaxCUSize - log2SubBlk, videoParam.MaxCuDQPDepth);
            
            for (Ipp32s depth = 0; depth <= MaxDepth; depth++) { // 64x64 -> 8x8
                Ipp32s log2BlkSize = videoParam.Log2MaxCUSize - depth;
                Ipp32s pitchRsCs = inFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
                Ipp32s blkSize = 1<<log2BlkSize;
                Ipp32s width = IPP_MIN(64, videoParam.Width - pelX);
                Ipp32s height = IPP_MIN(64, videoParam.Height - pelY);
                for (Ipp32s y = 0; y < height; y += blkSize) {
                    for (Ipp32s x = 0; x < width; x += blkSize) {
                        Ipp32s idx = ((pelY+y)>>log2BlkSize)*pitchRsCs + ((pelX+x)>>log2BlkSize);
                        Ipp32f Rs2 = inFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
                        Ipp32f Cs2 = inFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
                        Ipp32s blkW4 = IPP_MIN(blkSize, width-x) >> 2;
                        Ipp32s blkH4 = IPP_MIN(blkSize, height-y) >> 2;
                        Ipp32s numSubBlkw = blkW4 >> (log2SubBlk - 2);
                        Ipp32s numSubBlkh = blkH4 >> (log2SubBlk - 2);
#ifdef LOW_COMPLX_PAQ
                        if (numSubBlkh*(1 << log2SubBlk) < height || numSubBlkw*(1 << log2SubBlk) < width) {
                            Ipp32s widthInTiles = widthInCtbs << depth;
                            Ipp32s off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                            inFrame->m_stats[0]->coloc_futr[depth][off] = 0;
                            inFrame->m_stats[0]->qp_mask[depth][off] = 0;
                            continue;
                        }
#endif
                        Rs2/=(blkW4*blkH4);
                        Cs2/=(blkW4*blkH4);
                        Ipp32f SC = sqrt(Rs2 + Cs2);
                        Ipp32f tsc_RTML = 0.6f*sqrt(SC);
#ifdef LOW_COMPLX_PAQ
                        if (lowRes) tsc_RTML = 0.66f*sqrt(SC);
#endif
                        Ipp32f tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414f);
                        Ipp32f tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414f);
                        int scVal = 0;
                        for (Ipp32s i = 0; i < 10; i++) {
                            if (SC < lmt_sc2[i]*(Ipp32f)(1<<(inFrame->m_bitDepthLuma-8))) {
                                scVal = i;
                                break;
                            }
                        }

                        FrameIter itStart = ++FrameIter(curr);
                        int coloc=0;
                        Ipp32f pdsad_futr=0.0f;
                        for (FrameIter it = itStart; it != end; it++) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            int uMVnum = 0;
                            Ipp32f psad_futr = 0.0f;
                            for (Ipp32s i=0;i<numSubBlkh;i++) {
                                for (Ipp32s j=0;j<numSubBlkw;j++) {
                                    Ipp32s off = (((pelY+y)>>(log2SubBlk))+i) * widthInSubBlks + ((pelX+x)>>(log2SubBlk))+j;
                                    psad_futr += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_futr/=(blkW4*blkH4)<<4;

                            if (psad_futr < tsc_RTML && uMVnum < 1 + IPP_MAX(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc++;
                            else 
                                break;
                        } 
                        pdsad_futr=0.0f;
#ifdef LOW_COMPLX_PAQ
                        if (itStart != end) {
                            Ipp32f sadFrm[17] = { 0 };
                            Ipp32f sadSum = 0;
                            Ipp32f sadMax = 0;
                            Ipp32s sadCnt = 0;
                            for (FrameIter it = itStart; it != end; it++) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                Ipp32f psad_futr = 0.0f;
                                for (Ipp32s i = 0; i < numSubBlkh; i++) {
                                    for (Ipp32s j = 0; j < numSubBlkw; j++) {
                                        Ipp32s off = (((pelY + y) >> (log2SubBlk)) + i) * widthInSubBlks + ((pelX + x) >> (log2SubBlk)) + j;
                                        psad_futr += stat->m_interSad[off];
                                    }
                                }
                                psad_futr /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_futr;
                                sadCnt++;
                                sadSum += psad_futr;
                            }
                            Ipp32f sadAvg = sadSum / sadCnt;
                            Ipp32s numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_futr = sadMax;
                            }
                            else {
                                pdsad_futr = sadAvg * 2;
                            }
                        }
#else
                        for (Ipp32s i=0; i<numSubBlkh; i++) {
                            for (Ipp32s j=0; j<numSubBlkw; j++) {
                                Ipp32s off = (((pelY+y)>>log2SubBlk)+i) * widthInSubBlks + ((pelX+x)>>log2SubBlk)+j;
                                pdsad_futr += inFrame->m_stats[lowRes]->m_interSad_pdist_future[off];
                            }
                        }
                        pdsad_futr/=(blkW4*blkH4)<<4;
#endif

                        if (itStart == end) {
                            pdsad_futr = tsc_RTS;
                            coloc = 0;
                        }
            
                        Ipp32s widthInTiles = videoParam.PicWidthInCtbs << depth;
                        Ipp32s off = ((pelY+y)*widthInTiles+(pelX+x)) >> log2BlkSize;
                        inFrame->m_stats[0]->coloc_futr[depth][off] = coloc;

                        Ipp32s &qp_mask = inFrame->m_stats[0]->qp_mask[depth][off];
                        if(scVal<1 && pdsad_futr<tsc_RTML) {
                            // Visual Quality (Flat area first, because coloc count doesn't work in flat areas)
                            coloc = 1;
                            qp_mask = -1*coloc;
                        } else {
                            if(futr_key) {
                                coloc = IPP_MIN(REF_DIST/2, coloc);
                            }
                            if(coloc>=8 && pdsad_futr<tsc_RTML) {
                                // Stable Motion, Propagation & Motion reuse (long term stable hypothesis, 8+=>inf)
                                qp_mask = -1*IPP_MIN(futr_qp, (Ipp32s)(((Ipp32f)coloc/8.f)*futr_qp));
                            } else if(coloc>=8 && pdsad_futr<tsc_RTMG) {
                                // Stable Motion, Propagation possible & Motion reuse 
                                qp_mask = -1*IPP_MIN(futr_qp, (Ipp32s)(((Ipp32f)coloc/8.f)*4.f));
                            } else if(coloc>1 && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion reuse 
                                qp_mask = -1*IPP_MIN(4, (Ipp32s)(((Ipp32f)coloc/8.f)*4.f));
                            } else if(scVal>=6 && pdsad_futr>tsc_RTS) {
                                // Reduce disproportional cost on high texture and bad motion
                                qp_mask = 1;
                            } else {
                                // Default
                                qp_mask = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}


void H265Enc::DetermineQpMap_PFrame(FrameIter begin, FrameIter curr, FrameIter end, H265VideoParam & videoParam)
{
    static const Ipp32f lmt_sc2[10] = { // lower limit of SFM(Rs,Cs) range for spatial classification
        4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, static_cast<Ipp32f>(INT_MAX)};
    FrameIter itCurr = curr;
    Frame* inFrame = *curr;
    Frame* past_frame = *begin;
    const Ipp32f futr_qp = MAX_DQP;
    const Ipp32s REF_DIST = IPP_MIN(8, (inFrame->m_frameOrder - past_frame->m_frameOrder));
    H265MV candMVs[(64/8)*(64/8)];
    Ipp32s LowresFactor = 0;
#ifdef LOW_COMPLX_PAQ
    LowresFactor = videoParam.LowresFactor;
#endif
    Ipp32s lowRes = LowresFactor ? 1 : 0;
    Ipp32s log2SubBlk = 3 + LowresFactor;
    Ipp32s widthInSubBlks = videoParam.Width>>3;
    if (lowRes) {
        widthInSubBlks = (inFrame->m_lowres->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
    }
    Ipp32s heightInCtbs = (Ipp32s)videoParam.PicHeightInCtbs;
    Ipp32s widthInCtbs = (Ipp32s)videoParam.PicWidthInCtbs;

    bool futr_key = false;
    FrameIter it1 = ++FrameIter(curr);
    for (FrameIter it = it1; it != end; it++) {
        if (*it && (*it)->m_picCodeType == MFX_FRAMETYPE_I) {
            futr_key = true;
            break;
        }
    }
    
    for (Ipp32s row=0; row<heightInCtbs; row++) {
        for (Ipp32s col=0; col<widthInCtbs; col++) {
            Ipp32s pelX = col * videoParam.MaxCUSize;
            Ipp32s pelY = row * videoParam.MaxCUSize;
            Ipp32s MaxDepth = IPP_MIN((Ipp32s) videoParam.Log2MaxCUSize - log2SubBlk, videoParam.MaxCuDQPDepth);
            for (Ipp32s depth = 0; depth <= MaxDepth; depth++) { // 64x64 -> 8x8
                Ipp32s log2BlkSize = videoParam.Log2MaxCUSize - depth;
                Ipp32s pitchRsCs = inFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
                Ipp32s blkSize = 1<<log2BlkSize;
                Ipp32s width = IPP_MIN(64, videoParam.Width - pelX);
                Ipp32s height = IPP_MIN(64, videoParam.Height - pelY);
                for (Ipp32s y = 0; y < height; y += blkSize) {
                    for (Ipp32s x = 0; x < width; x += blkSize) {
                        Ipp32s idx = ((pelY+y)>>log2BlkSize)*pitchRsCs + ((pelX+x)>>log2BlkSize);
                        Ipp32f Rs2 = inFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
                        Ipp32f Cs2 = inFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
                        Ipp32s blkW4 = IPP_MIN(blkSize, width-x) >> 2;
                        Ipp32s blkH4 = IPP_MIN(blkSize, height-y) >> 2;

                        Ipp32s numSubBlkw = blkW4 >> (log2SubBlk - 2);
                        Ipp32s numSubBlkh = blkH4 >> (log2SubBlk - 2);
#ifdef LOW_COMPLX_PAQ
                        if (numSubBlkh*(1 << log2SubBlk) < height || numSubBlkw*(1 << log2SubBlk) < width) {
                            Ipp32s widthInTiles = widthInCtbs << depth;
                            Ipp32s off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                            inFrame->m_stats[0]->coloc_futr[depth][off] = 0;
                            inFrame->m_stats[0]->qp_mask[depth][off] = 0;
                            continue;
                        }
#endif

                        Rs2/=(blkW4*blkH4);
                        Cs2/=(blkW4*blkH4);
                        Ipp32f SC = sqrt(Rs2 + Cs2);
                        Ipp32f tsc_RTML = 0.6f*sqrt(SC);
#ifdef LOW_COMPLX_PAQ
                        if(lowRes) tsc_RTML = 0.66f*sqrt(SC);
#endif
                        Ipp32f tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414f);
                        Ipp32f tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414f);
                        int scVal = 0;
                        for (Ipp32s i = 0; i < 10; i++) {
                            if (SC < lmt_sc2[i]*(Ipp32f)(1<<(inFrame->m_bitDepthLuma-8))) {
                                scVal = i;
                                break;
                            }
                        }

                        // RTF RTS logic for P Frame is based on MC Error

                        Ipp32f pdsad_past=0.0f;
                        Ipp32f pdsad_futr=0.0f;
                        FrameIter itStart = ++FrameIter(itCurr);
#ifdef LOW_COMPLX_PAQ
                        if (itStart != end) {
                            Ipp32f sadFrm[17] = { 0 };
                            Ipp32f sadSum = 0;
                            Ipp32f sadMax = 0;
                            Ipp32s sadCnt = 0;
                            for (FrameIter it = itStart; it != end; it++) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                Ipp32f psad_futr = 0.0f;
                                for (Ipp32s i = 0; i < numSubBlkh; i++) {
                                    for (Ipp32s j = 0; j < numSubBlkw; j++) {
                                        Ipp32s off = (((pelY + y) >> (log2SubBlk)) + i) * widthInSubBlks + ((pelX + x) >> (log2SubBlk)) + j;
                                        psad_futr += stat->m_interSad[off];
                                    }
                                }
                                psad_futr /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_futr;
                                sadCnt++;
                                sadSum += psad_futr;
                            }
                            Ipp32f sadAvg = sadSum / sadCnt;
                            Ipp32s numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_futr = sadMax;
                            }
                            else {
                                pdsad_futr = sadAvg * 2;
                            }
                        }
                        if (itCurr != begin) {
                            Ipp32f sadFrm[17] = { 0 };
                            Ipp32f sadSum = 0;
                            Ipp32f sadMax = 0;
                            Ipp32s sadCnt = 0;
                            for (FrameIter it = itCurr; it != begin; it--) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                Ipp32f psad_past = 0.0;
                                for (Ipp32s i = 0; i < numSubBlkh; i++) {
                                    for (Ipp32s j = 0; j < numSubBlkw; j++) {
                                        Ipp32s off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                        psad_past += stat->m_interSad[off];
                                    }
                                }
                                psad_past /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_past;
                                sadCnt++;
                                sadSum += psad_past;
                            }
                            Ipp32f sadAvg = sadSum / sadCnt;
                            Ipp32s numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_past = sadMax;
                            }
                            else {
                                pdsad_past = sadAvg * 2;
                            }
                        }
#else
                        for(Ipp32s i=0; i<numSubBlkh; i++) {
                            for(Ipp32s j=0; j<numSubBlkw; j++) {
                                Ipp32s off = (((pelY+y)>> log2SubBlk)+i) * widthInSubBlks + ((pelX+x)>> log2SubBlk)+j;
                                pdsad_past += inFrame->m_stats[lowRes]->m_interSad_pdist_past[off];
                                pdsad_futr += inFrame->m_stats[lowRes]->m_interSad_pdist_future[off];
                            }
                        }
                        pdsad_past/=(blkW4*blkH4)<<4;
                        pdsad_futr/=(blkW4*blkH4)<<4;
#endif
                        // future (forward propagate)
                        Ipp32s coloc_futr=0;
                        for (FrameIter it = itStart; it != end; it++) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            Ipp32s uMVnum = 0;
                            Ipp32f psad_futr = 0.0f;
                            for (Ipp32s i=0; i<numSubBlkh; i++) {
                                for (Ipp32s j=0; j<numSubBlkw; j++) {
                                    Ipp32s off = (((pelY+y)>>log2SubBlk)+i) * widthInSubBlks + ((pelX+x)>>log2SubBlk)+j;
                                    psad_futr += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_futr/=(blkW4*blkH4)<<4;
                            if (psad_futr < tsc_RTML && uMVnum < 1 + IPP_MAX(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc_futr++;
                            else break;
                        }


                        // past (backward propagate)
                        Ipp32s coloc_past=0;
                        for (FrameIter it = itCurr; it != begin; it--) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            int uMVnum = 0; 
                            Ipp32f psad_past=0.0;
                            for (Ipp32s i=0; i<numSubBlkh; i++) {
                                for (Ipp32s j=0; j<numSubBlkw; j++) {
                                    Ipp32s off = (((pelY+y)>> log2SubBlk)+i) * widthInSubBlks + ((pelX+x)>> log2SubBlk)+j;
                                    psad_past += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_past/=(blkW4*blkH4)<<4;
                            if (psad_past < tsc_RTML && uMVnum < 1 + IPP_MAX(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc_past++;
                            else
                                break;
                        }

                        if (itStart == end) {
                            pdsad_futr = tsc_RTS;
                            coloc_futr = 0;
                        }
                        if (itCurr == begin) {
                            pdsad_past = tsc_RTS;
                            coloc_past = 0;
                        }

                        Ipp32s widthInTiles = widthInCtbs << depth;
                        Ipp32s off = ((pelY+y)*widthInTiles+(pelX+x)) >> log2BlkSize;
                        inFrame->m_stats[0]->coloc_futr[depth][off] = coloc_futr;
                        Ipp32s coloc = IPP_MAX(coloc_past, coloc_futr);
                        if (futr_key) coloc = coloc_past;

                        Ipp32s &qp_mask = inFrame->m_stats[0]->qp_mask[depth][off];
                        if (coloc_past >= REF_DIST && past_frame->m_stats[0]->coloc_futr[depth][off] >= REF_DIST) {
                            // Stable Motion & P Skip (when GOP is small repeated P QP lowering can change RD Point)
                            // Avoid quantization noise recoding
                            //inFrame->qp_mask[off] = IPP_MIN(0, past_frame->qp_mask[off]+1);
                            qp_mask = 0; // Propagate
                        } else {
                            if(futr_key) {
                                coloc = IPP_MIN(REF_DIST/2, coloc);
                            }
                            if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTML) {
                                // Stable Motion & Motion reuse 
                                qp_mask = -1*IPP_MIN((int)futr_qp, (int)(((Ipp32f)coloc/8.0)*futr_qp));
                            } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion reuse 
                                qp_mask = -1*IPP_MIN((int)futr_qp, (int)(((Ipp32f)coloc/8.0)*4.0));
                            } else if(coloc>1 && coloc==coloc_futr && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion Reuse
                                qp_mask = -1*IPP_MIN(4, (int)(((Ipp32f)coloc/8.0)*4.0));
                                // Possibly propagation
                            } else if(coloc>1 && coloc==coloc_past && pdsad_past<tsc_RTMG) {
                                // Stable Motion & Motion Reuse
                                qp_mask = -1*IPP_MIN(4, (int)(((Ipp32f)coloc/8.0)*4.0));
                                // Past Boost probably no propagation since coloc_futr is less than coloc_past
                            }  else if(scVal>=6 && pdsad_past>tsc_RTS && coloc==0) {
                                // reduce disproportional cost on high texture and bad motion
                                // use pdsad_past since its coded a in order p frame
                                qp_mask = 1;
                            } else {
                                // Default
                                qp_mask = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}

void WriteQpMap(Frame *inFrame, const H265VideoParam& param) {

#ifdef PAQ_LOGGING
    int heightInTiles = param.PicHeightInCtbs;
    int widthInTiles = param.PicWidthInCtbs;
    int row, col;
    FILE *fp = fopen("qpmap.txt", "a+");
    fprintf(fp, "%d ", inFrame->m_frameOrder);
    fprintf(fp, "%d %d \n", 1, 1);
    for(row = 0; row<heightInTiles; row++) {
        for(col=0; col<widthInTiles; col++) {
            int val = inFrame->m_stats[0]->qp_mask[row*widthInTiles+col];
            fprintf(fp, "%+d ", val);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
#else
    inFrame, param;
    return;
#endif
}

void WriteDQpMap(Frame *inFrame, const H265VideoParam& param) {

#ifdef PAQ_LOGGING

    int heightInTiles = param.PicHeightInCtbs;
    int widthInTiles = param.PicWidthInCtbs;
    int row, col;
    FILE *fp = fopen("qpmap.txt", "a+");
    fprintf(fp, "%d ", inFrame->m_frameOrder);
    fprintf(fp, "%d %d \n", 1, inFrame->m_sliceQpY);
    for(row = 0; row<heightInTiles; row++) {
        for(col=0; col<widthInTiles; col++) {
            int val = inFrame->m_lcuQps[0][row*widthInTiles+col];
            fprintf(fp, "%+d ", val);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
#else
    inFrame, param;
    return;
#endif
}

// based on blk_8x8. picHeight maybe original or lowres
Ipp32s GetNumActiveRows(Ipp32s region_row, Ipp32s rowsInRegion, Ipp32s picHeight)
{
    Ipp32s numRows = rowsInRegion;
    if(((region_row * rowsInRegion) + numRows) * SIZE_BLK_LA > picHeight)
        numRows = (picHeight - (region_row * rowsInRegion)*SIZE_BLK_LA) / SIZE_BLK_LA;

    return numRows;
}


Frame* H265Enc::GetPrevAnchor(FrameIter begin, FrameIter end, const Frame* curr)
{
    FrameIter itCurr = std::find_if(begin, end, isEqual(curr->m_frameOrder));

    Frame* prevP = NULL;
    for (FrameIter it = begin; it != itCurr; it++) {
        if ((*it)->m_picCodeType == MFX_FRAMETYPE_P || 
            (*it)->m_picCodeType == MFX_FRAMETYPE_I) {
                prevP = (*it);
        }
    }

    return prevP;
}

Frame* H265Enc::GetNextAnchor(FrameIter curr, FrameIter end)
{
    Frame *nextP = NULL;
    FrameIter it = std::find_if(++curr, end, isEqualRefFrame());
    if (it != end)
        nextP = (*it);
    
    return nextP;
}


void H265Enc::DoPDistInterAnalysis_OneRow(Frame* curr, Frame* prevP, Frame* nextP, Ipp32s region_row, Ipp32s lowresRowsInRegion, Ipp32s originRowsInRegion, Ipp8u LowresFactor, Ipp8u FullresMetrics)
{
    FrameData* currFrame  = LowresFactor ? curr->m_lowres : curr->m_origin;
    Statistics* stat = curr->m_stats[LowresFactor ? 1 : 0];

    // compute (PDist = miniGopSize) Past ME
    if (prevP) {
        FrameData * prevFrame = LowresFactor ? prevP->m_lowres : prevP->m_origin;
        Ipp32s rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
        for (Ipp32s i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, prevFrame, &stat->m_interSad_pdist_past[0], NULL, &stat->m_mv_pdist_past[0], 0, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i,
                FullresMetrics ? curr->m_origin : NULL,
                FullresMetrics ? prevP->m_origin : NULL);
        }
#ifndef LOW_COMPLX_PAQ
        if (LowresFactor) {
            FrameData* currFrame  = curr->m_origin;
            FrameData * prevFrame = prevP->m_origin;

            Ipp32s rowsInRegion = originRowsInRegion;
            Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
            for(Ipp32s i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, prevFrame, &curr->m_stats[0]->m_interSad_pdist_past[0], NULL, &curr->m_stats[0]->m_mv_pdist_past[0], 1, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion)+i); // refine
            }
        }
#endif
    }
    // compute (PDist = miniGopSize) Future ME
    if (nextP) {
        FrameData* nextFrame = LowresFactor ? nextP->m_lowres : nextP->m_origin;
        Ipp32s rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
        for (Ipp32s i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, nextFrame, &stat->m_interSad_pdist_future[0], NULL, &stat->m_mv_pdist_future[0], 0, 1, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i,
                FullresMetrics ? curr->m_origin : NULL,
                FullresMetrics ? nextP->m_origin : NULL);
        }
#ifndef LOW_COMPLX_PAQ
        if (LowresFactor) {
            FrameData* currFrame = curr->m_origin;
            FrameData* nextFrame = nextP->m_origin;

            Ipp32s rowsInRegion = originRowsInRegion;
            Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
            for(Ipp32s i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, nextFrame, &curr->m_stats[0]->m_interSad_pdist_future[0], NULL, &curr->m_stats[0]->m_mv_pdist_future[0], 1, 1,LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i);//refine
            }
        }
#endif
    }
}


void H265Enc::BackPropagateAvgTsc(FrameIter prevRef, FrameIter currRef)
{
    Frame* input = *currRef;

    //Ipp32u frameType = input->m_picCodeType;
    FrameData* origFrame  =  input->m_origin;
    Statistics* origStats = input->m_stats[0];

    origStats->avgTSC = 0.0;
    origStats->avgsqrSCpp = 0.0;
    Ipp32s gop_size = 1;
    Ipp32f avgTSC     = 0.0;
    Ipp32f avgsqrSCpp = 0.0;

    for (FrameIter it = currRef; it != prevRef; it--) {
        Statistics* stat = (*it)->m_stats[0];
        avgTSC     += stat->TSC;
        avgsqrSCpp += stat->SC;
        gop_size++;
    }

    Ipp32s frameSize = origFrame->width * origFrame->height;

    avgTSC /= gop_size;
    avgTSC /= frameSize;

    avgsqrSCpp /= gop_size;
    avgsqrSCpp/=sqrt((Ipp32f)(frameSize));
    avgsqrSCpp = sqrt(avgsqrSCpp);

    for (FrameIter it = currRef; it != prevRef; it--) {
        Statistics* stats = (*it)->m_stats[0];
        stats->avgTSC = avgTSC;
        stats->avgsqrSCpp = avgsqrSCpp;
    }
}

void WritePSAD(Frame *inFrame)
{
#ifdef PAQ_LOGGING

    int i,j,M,N,P;
    int *p;

    M = inFrame->height / 8;
    N = inFrame->width / 8;
    {
        FILE *fp = fopen("psad_map.txt", "a+");
        fprintf(fp, "PSAD for Frame %d\n", inFrame->m_frameOrder);
        for(i=0;i<M;i++) 
        {
            for(j=0;j<N;j++) 
            {
                //if(fp) 
                fprintf(fp, "%4d ", inFrame->m_interSad[i*N+j]);
            }
            //if(fp) 
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
#else
    inFrame;
    return;
#endif
}

void WriteMV(Frame *inFrame)
{
#ifdef PAQ_LOGGING

    FILE *fp=NULL;
    int i,j;
    char fname[64];
    int *p;

    int height = inFrame->height / 8;
    int width  = inFrame->width / 8;
    {
        sprintf_s(fname, sizeof(fname), "%s_%d_%dx%d.txt", "psmv_me", inFrame->m_frameOrder, inFrame->width, inFrame->height);

        fp = fopen(fname, "a+");
        if(!fp) return;
        for(j=0;j<height;j++) {
            for(i=0;i<width;i++) {
                fprintf(fp, "(%7d,%7d) ", inFrame->m_mv[i+j*width].mvx, inFrame->m_mv[i+j*width].mvy);
            }
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
        fclose(fp);
    }
#else
    inFrame;
    return;
#endif
}


int Lookahead::ConfigureLookaheadFrame(Frame* in, Ipp32s fieldNum)
{
    if (in == NULL && m_inputQueue.empty()) {
        return 0;
    }

    if (fieldNum == 0 && in) {
        std::vector<ThreadingTask> &tt = in->m_ttLookahead;
        Ipp32s poc = in->m_frameOrder;

        size_t startIdx = 0;
        tt[startIdx].Init(TT_PREENC_START, 0, 0, poc, 1, 0);
        tt[startIdx].la = this;

        size_t endIdx   = tt.size() - 1;
        tt[endIdx].Init(TT_PREENC_END, 0, 0, poc, 0, 0);
        tt[endIdx].la = this;


        for (size_t idx = 1; idx < endIdx; idx++) {
            Ipp32s row = (Ipp32s)idx - 1;
            tt[idx].Init(TT_PREENC_ROUTINE, row, 0, poc, 0, 0);
            tt[idx].la = this;

            // dependency LA_ROUTINE <- LA_START
            AddTaskDependency(&tt[idx], &tt[startIdx], &m_ttHubPool);

            // dependency LA_END     <- LA_ROUTINE
            AddTaskDependency(&tt[endIdx], &tt[idx], &m_ttHubPool);
        }
    }

     return 1;
}


void H265Encoder::OnLookaheadCompletion()
{
    if (m_la.get() == 0)
        return;

    // update 
    FrameIter itEnd = m_inputQueue.begin();
    for (itEnd; itEnd != m_inputQueue.end(); itEnd++) {
        if ((*itEnd)->m_lookaheadRefCounter > 0) 
            break;
    }

    if (m_videoParam.AnalyzeCmplx) {
        for (FrameIter it = m_inputQueue.begin(); it != m_inputQueue.end() && it != itEnd; it++) {
            FrameIter nt = it;
            if(nt != m_inputQueue.end()) nt++;
            AverageComplexity((it != m_inputQueue.end())?*it:NULL, m_videoParam, (nt != m_inputQueue.end())?*nt:NULL);
        }
    }

    m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin(), itEnd);
}

bool MetricIsGreater(const StatItem &l, const StatItem &r) { return (l.met > r.met); }

//void WriteCmplx(Frame *frames[], Ipp32s numFrames)
//{
//    {
//        FILE *fp = fopen("BRC_complexity.txt", "a+");
//        fprintf(fp, "complexity for Frame %d type %d\n", frames[0]->m_frameOrder, frames[0]->m_picCodeType);
//        for(int i=0;i<numFrames;i++) {
//            fprintf(fp, "%11.3f \n", frames[i]->m_avgBestSatd);
//
//        }
//        //fprintf(fp, "\n");
//        fclose(fp);
//    }
//}
// ========================================================
void Lookahead::AnalyzeSceneCut_AndUpdateState_Atul(Frame* in)
{
    if (in == NULL)
        return;

    FrameIter curr = std::find_if(m_inputQueue.begin(), m_inputQueue.end(), isEqual(in->m_frameOrder));
    Frame* prev = curr == m_inputQueue.begin() ? NULL : *(--FrameIter(curr));
    Ipp32s sceneCut = DetectSceneCut_AMT(in, prev);
    if (sceneCut && in->m_frameOrder < 2) sceneCut = 0;

#if 0
    if (sceneCut) {
        FILE *fp = fopen("sceneReport.txt", "a+");
        fprintf(fp, "%i\t", in->m_frameOrder);
        fclose(fp);
    }
#endif
    
    if (sceneCut) {
        m_enc.m_sceneOrder++;
        if (in->m_stats[1]) 
            in->m_stats[1]->m_sceneCut = -1;
        if (in->m_stats[0]) 
            in->m_stats[0]->m_sceneCut = -1;
    }
    if (in->m_stats[1]) in->m_stats[1]->m_sceneChange = sceneCut;
    if (in->m_stats[0]) in->m_stats[0]->m_sceneChange = sceneCut;

    in->m_sceneOrder = m_enc.m_sceneOrder;

    //special case for interlace mode to keep (P | I) pair instead of (I | P)
    Ipp32s rightAnchor = (m_videoParam.picStruct == MFX_PICSTRUCT_PROGRESSIVE || (m_videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE && in->m_secondFieldFlag) || m_videoParam.GopRefDist==1);

    Ipp8u insertKey = 0;
    if (sceneCut) {
        if (in->m_picCodeType == MFX_FRAMETYPE_B) {
            m_pendingSceneCut = 1;
        } else {
            if  (rightAnchor){
                m_pendingSceneCut = 0;
                insertKey = 1;
            }
            else {
                m_pendingSceneCut = 1;
            }
        }
    } else if (m_pendingSceneCut && (in->m_picCodeType != MFX_FRAMETYPE_B) && rightAnchor) {
        m_pendingSceneCut = 0;
        insertKey = 1;
    }

    // special marker for accurate INTRA encode for RASL
    if (m_pendingSceneCut) {
        in->m_forceTryIntra = 1;
    }

    Ipp32s isInsertI = insertKey && (in->m_picCodeType == MFX_FRAMETYPE_P);
    if (isInsertI) {
        Frame* frame = in;

        // restore global state
        m_enc.RestoreGopCountersFromFrame(frame, !!m_videoParam.encodedOrder);

        // light configure
        frame->m_isIdrPic = false;
        frame->m_picCodeType = MFX_FRAMETYPE_I;
        //frame->m_poc = 0;

        if (m_videoParam.PGopPicSize > 1) {
            //const Ipp8u PGOP_LAYERS[PGOP_PIC_SIZE] = { 0, 2, 1, 2 };
            frame->m_RPSIndex = 0;
            frame->m_pyramidLayer = 0;//PGOP_LAYERS[frame->m_RPSIndex];
        }

        m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);

        // we need update all frames in inputQueue after replacement to propogate SceneCut (frame type change) effect
        FrameIter it = curr;
        it++;
        for (it; it != m_inputQueue.end(); it++) {
            frame = (*it);
            m_enc.ConfigureInputFrame(frame, !!m_videoParam.encodedOrder);
            m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);
        }
    }

#if 0
    {
        FILE *fp = fopen("scene.txt", "a+");
        fprintf(fp, "FrameOrder %i SceneOrder %i SceneCutDetected %i InsertI %i\n", in->m_frameOrder, in->m_sceneOrder, sceneCut, isInsertI);
        fclose(fp);
    }
#endif

    if (insertKey && in->m_picCodeType == MFX_FRAMETYPE_I) {
        if (in->m_stats[1])
            in->m_stats[1]->m_sceneCut = 1;
        if (in->m_stats[0])
            in->m_stats[0]->m_sceneCut = 1;
    }

    if (prev) {
        prev->m_lookaheadRefCounter--;
    }

} // 
// ========================================================
void Lookahead::ResetState()
{
    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;
}

void H265Enc::AverageComplexity(Frame *in, H265VideoParam& videoParam, Frame *next)
{
    if (in == NULL)
        return;

    Ipp8u useLowres = videoParam.LowresFactor;
    FrameData* frame = useLowres ? in->m_lowres : in->m_origin;
    Statistics* stat = in->m_stats[useLowres ? 1 : 0];

    Ipp64s sumSatd = 0;
    Ipp64s sumSatdIntra = 0;
    Ipp64s sumSatdInter = 0;
    Ipp32s intraBlkCount = 0;

    if (in->m_frameOrder > 0 && (in->m_picCodeType != MFX_FRAMETYPE_I || stat->m_sceneCut > 0)) {
    //if (in->m_frameOrder > 0 && (in->m_picCodeType != MFX_FRAMETYPE_I)) {
        Ipp32s blkCount = (Ipp32s)stat->m_intraSatd.size();

        for (Ipp32s blk = 0; blk < blkCount; blk++) {
            if (in->m_picCodeType != MFX_FRAMETYPE_I)
                sumSatd += IPP_MIN(stat->m_intraSatd[blk], stat->m_interSatd[blk]);
            else
                sumSatd += stat->m_intraSatd[blk];

            if( stat->m_intraSatd[blk] <= stat->m_interSatd[blk] ) {
                intraBlkCount++;
            }

            sumSatdIntra += stat->m_intraSatd[blk];
            sumSatdInter += stat->m_interSatd[blk];
        }
        stat->m_intraRatio = (Ipp32f)intraBlkCount / blkCount;

    } else {
        sumSatd = std::accumulate(stat->m_intraSatd.begin(), stat->m_intraSatd.end(), 0);
        stat->m_intraRatio = 1.f;
        sumSatdIntra = sumSatd;
    }

    // store metric in frame
    Ipp32s resolution = frame->width * frame->height;
    if(useLowres && videoParam.FullresMetrics) {
        resolution = in->m_origin->width * in->m_origin->height;
    }

    stat->m_avgBestSatd = (Ipp32f)sumSatd / (resolution);
    stat->m_avgIntraSatd = (Ipp32f)sumSatdIntra / (resolution);
    stat->m_avgInterSatd = (Ipp32f)sumSatdInter / (resolution);

    // hack for BRC
    if (in->m_stats[0]) {
        in->m_stats[0]->m_avgBestSatd = stat->m_avgBestSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[0]->m_avgIntraSatd = stat->m_avgIntraSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[0]->m_avgInterSatd = stat->m_avgInterSatd / (1 << videoParam.bitDepthLumaShift);
    }
    if (in->m_stats[1]) {
        in->m_stats[1]->m_avgBestSatd = stat->m_avgBestSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[1]->m_avgIntraSatd = stat->m_avgIntraSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[1]->m_avgInterSatd = stat->m_avgInterSatd / (1 << videoParam.bitDepthLumaShift);
    }

    if (useLowres) {
        Ipp8u res = Saturate(0, 1, useLowres-1);
        Ipp32f scaleFactor = 1.29;
        if (!videoParam.FullresMetrics) {
            Ipp32f tabCorrFactor[] = {1.5f, 2.f};
            scaleFactor = tabCorrFactor[res];
        }
        //if(videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE) scaleFactor *= 1.29;
        if (in->m_stats[0]) {
            in->m_stats[0]->m_avgBestSatd /= scaleFactor;
            in->m_stats[0]->m_avgIntraSatd /= scaleFactor;
            in->m_stats[0]->m_avgInterSatd /= scaleFactor;
        } 
        if (in->m_stats[1]) {
            in->m_stats[1]->m_avgBestSatd /= scaleFactor;
            in->m_stats[1]->m_avgIntraSatd /= scaleFactor;
            in->m_stats[1]->m_avgInterSatd /= scaleFactor;
        }
    }

    stat->m_bestSatdHist[stat->m_bestSatdHist.size()-1] = stat->m_avgBestSatd;
    stat->m_intraSatdHist[stat->m_intraSatdHist.size()-1] = stat->m_avgIntraSatd;
    if(next) {
        for (Ipp32s k = 0;  k < (Ipp32s) stat->m_bestSatdHist.size()-1; k++) {
            next->m_stats[useLowres ? 1 : 0]->m_bestSatdHist[k] = stat->m_bestSatdHist[k+1];
        }
        for (Ipp32s k = 0;  k < (Ipp32s) stat->m_intraSatdHist.size()-1; k++) {
            next->m_stats[useLowres ? 1 : 0]->m_intraSatdHist[k] = stat->m_intraSatdHist[k+1];
        }
    }

}


void H265Enc::AverageRsCs(Frame *in, H265VideoParam& videoParam)
{
    if (in == NULL)
        return;

    Statistics* stat = in->m_stats[0];
    FrameData* data = in->m_origin;
    stat->m_frameCs = std::accumulate(stat->m_cs[4], stat->m_cs[4] + stat->m_rcscSize[4], 0);
    stat->m_frameRs = std::accumulate(stat->m_rs[4], stat->m_rs[4] + stat->m_rcscSize[4], 0);

    Ipp32s hblocks  = (Ipp32s)data->height / BLOCK_SIZE;
    Ipp32s wblocks  = (Ipp32s)data->width / BLOCK_SIZE;
    stat->m_frameCs /= hblocks * wblocks;
    stat->m_frameRs /= hblocks * wblocks;
    stat->m_frameCs  = sqrt(stat->m_frameCs);
    stat->m_frameRs  = sqrt(stat->m_frameRs);

    Ipp32f RsGlobal = in->m_stats[0]->m_frameRs;
    Ipp32f CsGlobal = in->m_stats[0]->m_frameCs;
    Ipp32s frameSize = in->m_origin->width * in->m_origin->height;

    in->m_stats[0]->SC = sqrt(((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal))*frameSize);
    if (videoParam.LowresFactor 
#ifndef LOW_COMPLX_PAQ
        && !(videoParam.DeltaQpMode&AMT_DQP_PAQ)
#endif
        ) 
    {
        // no refine
        in->m_stats[0]->TSC = std::accumulate(in->m_stats[1]->m_interSad.begin(), in->m_stats[1]->m_interSad.end(), 0);
        Ipp8u res = Saturate(0, 1, videoParam.LowresFactor - 1);
        Ipp32f frameFactor = (Ipp32f)(in->m_origin->width*in->m_origin->height)/(Ipp32f)(in->m_lowres->width*in->m_lowres->height);
        Ipp32f scaleFactor = 1.13;
        in->m_stats[0]->TSC = (in->m_stats[0]->TSC*frameFactor)/scaleFactor;
    }
    else {
        in->m_stats[0]->TSC = std::accumulate(in->m_stats[0]->m_interSad.begin(), in->m_stats[0]->m_interSad.end(), 0);
    }
}


template<class InputIterator, class UnaryPredicate>
InputIterator h265_findPastRef_OrSetBegin(InputIterator curr, InputIterator begin, UnaryPredicate pred)
{
    while (curr!=begin) {
        if (pred(*curr)) return curr;
        curr--;
    }
    return begin;
}

Ipp32s H265Enc::BuildQpMap(FrameIter begin, FrameIter end, Ipp32s frameOrderCentral, H265VideoParam& videoParam, Ipp32s doUpdateState)
{
    //FrameIter begin = m_inputQueue.begin();
    //FrameIter end   = m_inputQueue.end();
    
    //Ipp32s frameOrderCentral = in->m_frameOrder - m_bufferingPaq;
    Ipp8u isBufferingEnough = (frameOrderCentral >= 0);
    FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));


    // make decision
    if (isBufferingEnough && curr != end) {

        Ipp32u frameType = (*curr)->m_picCodeType;
        bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);

        if (isRef) {
            Frame* prevP = GetPrevAnchor(begin, end, *curr);
            Frame* nextP = GetNextAnchor(curr, end);
            FrameIter itNextRefPlus1 = nextP ? ++(std::find_if(begin, end, isEqual(nextP->m_frameOrder))) : end;

            Ipp32u frameType = (*curr)->m_picCodeType;
            if (videoParam.DeltaQpMode & AMT_DQP_PAQ) {

                if (nextP == NULL) {
                    std::fill((*curr)->m_stats[0]->m_interSad_pdist_future.begin(), (*curr)->m_stats[0]->m_interSad_pdist_future.end(), IPP_MAX_32S);
                    std::fill((*curr)->m_stats[1]->m_interSad_pdist_future.begin(), (*curr)->m_stats[1]->m_interSad_pdist_future.end(), IPP_MAX_32S);
                }
                if (frameType == MFX_FRAMETYPE_I) 
                    DetermineQpMap_IFrame(curr, itNextRefPlus1, videoParam);
                else if (frameType == MFX_FRAMETYPE_P) {
                    FrameIter itPrevRef  = std::find_if(begin, end, isEqual(prevP->m_frameOrder));
                    DetermineQpMap_PFrame(itPrevRef, curr, itNextRefPlus1, videoParam);
                }
            }
            if ((videoParam.DeltaQpMode & AMT_DQP_CAL) && (frameType == MFX_FRAMETYPE_P)) {
                FrameIter itPrevRef = std::find_if(begin, end, isEqual(prevP->m_frameOrder));
                BackPropagateAvgTsc(itPrevRef, curr);
            }
        }
        
        //WriteQpMap(*curr, m_videoParam);
    }


    // update state
    if (doUpdateState) {
        //Ipp32s frameOrderCentral = in->m_frameOrder - m_bufferingPaq;
        Ipp8u isBufferingEnough = (frameOrderCentral >= 0);
        FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));

        // Release resource counter
        if (isBufferingEnough && curr != end) {
            Ipp32u frameType = (*curr)->m_picCodeType;
            bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);
            //bool isEos = (m_slideWindowPaq[centralPos + 1] == -1);

            if (/*isEos || */isRef) {
                FrameIter itEnd = /*isEos ? end : */curr; // isEos has more priority
                FrameIter itStart = begin;
                if (curr != begin) {
                    curr--;
                    itStart = h265_findPastRef_OrSetBegin(curr, begin, isEqualRefFrame());
                }
                for (FrameIter it = itStart; it != itEnd; it++) {
                    (*it)->m_lookaheadRefCounter--;
                }
            }
        }
    }

    return 0;
} // 

void H265Enc::GetLookaheadGranularity(const H265VideoParam& videoParam, Ipp32s& regionCount, Ipp32s& lowRowsInRegion, Ipp32s& originRowsInRegion, Ipp32s& numTasks)
{
    Ipp32s lowresHeightInBlk = ((videoParam.Height >> videoParam.LowresFactor) + (SIZE_BLK_LA-1)) >> 3;
    //Ipp32s originHeightInBlk = ((videoParam.Height) + (SIZE_BLK_LA-1)) >> 3;

    regionCount = videoParam.num_threads;// should be based on blk_8x8
    regionCount = IPP_MIN(lowresHeightInBlk, regionCount);
    // adjust
    {
        lowRowsInRegion  = (lowresHeightInBlk + (regionCount - 1)) / regionCount;
        regionCount      = (lowresHeightInBlk + (lowRowsInRegion - 1)) / lowRowsInRegion;
    }

    lowRowsInRegion    = (lowresHeightInBlk + (regionCount - 1)) / regionCount;
    originRowsInRegion = lowRowsInRegion << videoParam.LowresFactor;

    /*Ipp32s*/ numTasks = 1 /*LA_START*/  + regionCount + 1 /*LA_END*/;
    
}

Lookahead::Lookahead(H265Encoder & enc)
    : m_inputQueue(enc.m_inputQueue)
    , m_videoParam(enc.m_videoParam)
    , m_enc(enc)
    , m_pendingSceneCut(0)
{
    m_bufferingPaq = 0;

    if (m_videoParam.DeltaQpMode & AMT_DQP_PAQ)
        m_bufferingPaq = m_videoParam.GopRefDist;

    // to prevent multiple PREENC_START per frame
    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;

    Ipp32s numTasks;
    GetLookaheadGranularity(m_videoParam, m_regionCount, m_lowresRowsInRegion, m_originRowsInRegion, numTasks);
}

Lookahead::~Lookahead()
{
}


const Ipp32f CU_RSCS_TH[5][4][8] = {
    {{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0}},
    {{  5.0,  6.0,  8.0, 11.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  7.0,  9.0, 11.0, 14.0, 18.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 14.0, 18.0, 25.0,65025.0}},
    {{  5.0,  7.0, 10.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 20.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0}},
    {{  6.0,  8.0, 10.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 19.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 15.0, 19.0, 24.0,65025.0}},
    {{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}}
};


const Ipp32f LQ_M[5][8]   = {
    {4.2415, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.0684},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.2005, 4.2005, 4.2005},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},    // B3
};
const Ipp32f LQ_K[5][8] = {
    {12.8114, 13.8536, 13.8536, 13.8536, 13.8395, 13.8395, 13.8395, 13.8395},   // I
    {12.3857, 12.3857, 12.3857, 12.3857, 12.3857, 13.7122, 13.7122, 13.7122},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};
const Ipp32f LQ_M16[5][8]   = {
    {4.3281, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.3281},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.3281, 4.3281, 4.3281},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B3
};
const Ipp32f LQ_K16[5][8] = {
    {14.4329, 14.8983, 14.8983, 14.8983, 14.9069, 14.9069, 14.9069, 14.4329},   // I
    {12.4456, 12.4456, 12.4456, 12.4456, 12.4456, 13.5336, 13.5336, 13.5336},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};

int GetCalqDeltaQpLCU(Frame* frame, const H265VideoParam & par, Ipp32s ctb_addr, Ipp32f sliceLambda, Ipp32f sliceQpY)
{
    Ipp32s picClass = 0;
    if(frame->m_picCodeType == MFX_FRAMETYPE_I) {
        picClass = 0;   // I
    } else {
        picClass = frame->m_pyramidLayer+1;
    }

    static int pQPi[5][4] = {{22,27,32,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41}};
    Ipp32s qpClass = 0;
    if(sliceQpY < pQPi[picClass][0])
        qpClass = 0;
    else if(sliceQpY > pQPi[picClass][3])
        qpClass = 3;
    else
        qpClass = (Ipp32s)((sliceQpY - 22 - picClass) / 5);

    Ipp32s col =  (ctb_addr % par.PicWidthInCtbs);
    Ipp32s row =  (ctb_addr / par.PicWidthInCtbs);
    Ipp32s pelX = col * par.MaxCUSize;
    Ipp32s pelY = row * par.MaxCUSize;

    //TODO: replace by template call here
    // complex ops in Enqueue frame can cause severe threading eff loss -NG
    Ipp32f rscs = 0;
    if (picClass < 2) {
        // calulate from 4x4 Rs/Cs
        Ipp32s N = (col==par.PicWidthInCtbs-1)?(frame->m_origin->width-(par.PicWidthInCtbs-1)*par.MaxCUSize):par.MaxCUSize;
        Ipp32s M = (row==par.PicHeightInCtbs-1)?(frame->m_origin->height-(par.PicHeightInCtbs-1)*par.MaxCUSize):par.MaxCUSize;
        Ipp32s m4T = M/4;
        Ipp32s n4T = N/4;
        Ipp32s X4 = pelX/4;
        Ipp32s Y4 = pelY/4;
        Ipp32s w4 = frame->m_origin->width/4;
        Ipp32s Rs=0,Cs=0;
        for(Ipp32s i=0;i<m4T;i++) {
            for(Ipp32s j=0;j<n4T;j++) {
                Rs += frame->m_stats[0]->m_rs[0][(Y4+i)*w4+(X4+j)];
                Cs += frame->m_stats[0]->m_cs[0][(Y4+i)*w4+(X4+j)];
            }
        }
        Rs/=(m4T*n4T);
        Cs/=(m4T*n4T);
        rscs = sqrt(Rs + Cs);
    }

    Ipp32s rscsClass = 0;
    {
        Ipp32s k = 7;
        for (Ipp32s i = 0; i < 8; i++) {
            if (rscs < CU_RSCS_TH[picClass][qpClass][i]*(Ipp32f)(1<<(par.bitDepthLumaShift))) {
                k = i;
                break;
            }
        }
        rscsClass = k;
    }

    Ipp32f dLambda = sliceLambda * 256.f;
    Ipp32s gopSize = frame->m_biFramesInMiniGop + 1;
    Ipp32f QP = 0.f;
    if(16 == gopSize) {
        QP = LQ_M16[picClass][rscsClass]*log( dLambda ) + LQ_K16[picClass][rscsClass];
    } else if(8 == gopSize){
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    } else { // default case could be modified !!!
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    }
    QP -= sliceQpY;
    return (QP>=0.0)?int(QP+0.5):int(QP-0.5);
}

void GetCalqDeltaQp(Frame* frame, const H265VideoParam & par, Ipp32s ctb_addr, Ipp32f sliceLambda, Ipp32f sliceQpY)
{
    Ipp32s picClass = 0;
    if(frame->m_picCodeType == MFX_FRAMETYPE_I) {
        picClass = 0;   // I
    } else {
        picClass = frame->m_pyramidLayer+1;
    }
    if (picClass > 4)
        picClass = 4;

    static int pQPi[5][4] = {{22,27,32,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41}};
    Ipp32s qpClass = 0;
    if(sliceQpY < pQPi[picClass][0])
        qpClass = 0;
    else if(sliceQpY > pQPi[picClass][3])
        qpClass = 3;
    else
        qpClass = (Ipp32s)((sliceQpY - 22 - picClass) / 5);

    Ipp32s col =  (ctb_addr % par.PicWidthInCtbs);
    Ipp32s row =  (ctb_addr / par.PicWidthInCtbs);
    Ipp32s pelX = col * par.MaxCUSize;
    Ipp32s pelY = row * par.MaxCUSize;

    //TODO: replace by template call here
    // complex ops in Enqueue frame can cause severe threading eff loss -NG
    for (Ipp32s depth = 0; depth < 4; depth++) {
        Ipp32s log2BlkSize = par.Log2MaxCUSize - depth;
        Ipp32s pitchRsCs = frame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
        Ipp32s width = IPP_MIN(64, frame->m_origin->width-pelX);
        Ipp32s height = IPP_MIN(64, frame->m_origin->height-pelY);
        for (Ipp32s y = 0; y < height; y += (1<<log2BlkSize)) {
            for (Ipp32s x = 0; x < width; x += (1<<log2BlkSize)) {
                Ipp32s rscsClass = 0;
                if (picClass < 2) {
                    Ipp32s idx = ((pelY+y)>>log2BlkSize)*pitchRsCs + ((pelX+x)>>log2BlkSize);
                    Ipp32s Rs2 = frame->m_stats[0]->m_rs[log2BlkSize-2][idx];
                    Ipp32s Cs2 = frame->m_stats[0]->m_cs[log2BlkSize-2][idx];
                    Ipp32s blkW = IPP_MIN(1<<log2BlkSize, width-x) >> 2;
                    Ipp32s blkH = IPP_MIN(1<<log2BlkSize, height-y) >> 2;
                    Rs2/=(blkW*blkH);
                    Cs2/=(blkW*blkH);
                    Ipp32f rscs = sqrt(Rs2 + Cs2);

                    rscsClass = 7;
                    for (Ipp32s i = 0; i < 8; i++) {
                        if (rscs < CU_RSCS_TH[picClass][qpClass][i]*(Ipp32f)(1<<(par.bitDepthLumaShift))) {
                            rscsClass = i;
                            break;
                        }
                    }
                }

                Ipp32f dLambda = sliceLambda * 256.f;
                Ipp32s gopSize = frame->m_biFramesInMiniGop + 1; // in fact m_biFramesInMiniGop==0 at this point
                Ipp32f QP = 0.f;
                if (16 == gopSize)
                    QP = LQ_M16[picClass][rscsClass]*log( dLambda ) + LQ_K16[picClass][rscsClass];
                else if(8 == gopSize)
                    QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
                else // default case could be modified !!!
                    QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
                QP -= sliceQpY;

                Ipp32s calq = (QP>=0.0)?int(QP+0.5):int(QP-0.5);
                Ipp32s totalDQP = calq;

                Ipp32s qpBlkIdx = ((pelY+y)>>log2BlkSize)*(par.PicWidthInCtbs<<(6-log2BlkSize)) + ((pelX+x)>>log2BlkSize);
                if (par.DeltaQpMode&AMT_DQP_PAQ)
                    totalDQP += frame->m_stats[0]->qp_mask[depth][qpBlkIdx];

                Ipp32s lcuQp = frame->m_sliceQpY + totalDQP;
                lcuQp = Saturate(0, 51, lcuQp);
                frame->m_lcuQps[depth][qpBlkIdx] = (Ipp8s)lcuQp;
                frame->m_lcuQpOffs[depth][qpBlkIdx] = calq;
            }
        }
    }
}


void UpdateAllLambda(Frame* frame, const H265VideoParam& param)
{
    Statistics* stats = frame->m_stats[0];

    bool IsHiCplxGOP = false;
    bool IsMedCplxGOP = false;
    GetGopComplexity(param, frame, IsHiCplxGOP, IsMedCplxGOP);

    CostType rd_lamba_multiplier;
    bool extraMult = SliceLambdaMultiplier(rd_lamba_multiplier, param,  frame->m_slices[0].slice_type, frame, IsHiCplxGOP, IsMedCplxGOP, true);

    Ipp32s  origQP = frame->m_sliceQpY;
    for(Ipp32s iDQpIdx = 0; iDQpIdx < 2*(MAX_DQP)+1; iDQpIdx++)  {
        Ipp32s deltaQP = ((iDQpIdx+1)>>1)*(iDQpIdx%2 ? -1 : 1);
        Ipp32s curQp = origQP + deltaQP;

        frame->m_dqpSlice[iDQpIdx].slice_type = frame->m_slices[0].slice_type;
        SetSliceLambda(param, &(frame->m_dqpSlice[iDQpIdx]), curQp, frame, rd_lamba_multiplier, extraMult);
    }
}

void H265Enc::ApplyDeltaQp(Frame* frame, const H265VideoParam & par, Ipp8u useBrc)
{
    Ipp32s numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;

    // assign PAQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_PAQ) {// no pure CALQ 
        for (Ipp32s depth = 0; depth < 4; depth++) {
            for (size_t blk = 0; blk < frame->m_stats[0]->qp_mask[depth].size(); blk++) {
                Ipp32s deltaQp = frame->m_stats[0]->qp_mask[depth][blk];
                Ipp32s lcuQp = frame->m_lcuQps[depth][blk] + deltaQp;
                lcuQp = Saturate(0, 51, lcuQp);
                frame->m_lcuQps[depth][blk] = (Ipp8s)lcuQp;
            }
        }
#ifdef LOW_COMPLX_PAQ
        if (par.DeltaQpMode&AMT_DQP_PAQ) {
            Ipp32f avgDQp = std::accumulate(frame->m_stats[0]->qp_mask[0].begin(), frame->m_stats[0]->qp_mask[0].end(), 0);
            avgDQp /= frame->m_stats[0]->qp_mask[0].size();
            frame->m_stats[0]->m_avgDPAQ = avgDQp;
        }
#endif
    }


    // recalc (align) CTB lambdas with CTB Qp
    for (Ipp8u i = 0; i < par.NumSlices; i++)
        UpdateAllLambda(frame, par);

    // assign CALQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_CAQ) {
        Ipp32f baseQP = frame->m_sliceQpY;
        Ipp32f sliceLambda =  frame->m_dqpSlice[0].rd_lambda_slice;
        for (Ipp32s ctb_addr = 0; ctb_addr < numCtb; ctb_addr++) {
            GetCalqDeltaQp(frame, par, ctb_addr, sliceLambda, baseQP);
        }
    }

    // if (BRC) => need correct deltaQp (via QStep) to hack BRC logic
    /*
    if (useBrc) {
    Ipp32s totalDeltaQp = 0;
    Ipp32s corr = 0;
    Ipp32f corr0 = pow(2.0, (frame->m_sliceQpY-4)/6.0);
    Ipp32f denum = 0.0;
    for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
    denum += pow(2.0, (frame->m_lcuQps[0][ctb] -4) / 6.0);
    }
    corr = 6.0 * log10( (numCtb * corr0) / denum) / log10(2.0);
    // final correction 
    for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
    Ipp32s lcuQp = frame->m_lcuQps[0][ctb] + corr;
    lcuQp = Saturate(0, 51, lcuQp);
    frame->m_lcuQps[0][ctb] = lcuQp;
    }
    }
    */
    //WriteDQpMap(frame, par);
}


void H265Enc::ApplyDeltaQpOnRoi(Frame* frame, const H265VideoParam & par, Ipp8u useBrc)
{
    Ipp32s numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;

    // assign CALQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_CAQ) {
        Ipp32f baseQP = frame->m_sliceQpY;
        Ipp32f sliceLambda =  frame->m_roiSlice[baseQP].rd_lambda_slice;
        for (Ipp32s ctb_addr = 0; ctb_addr < numCtb; ctb_addr++) {

            int calq = GetCalqDeltaQpLCU(frame, par, ctb_addr, sliceLambda, baseQP);
            
            // Save CAQ dQp for ModeDecision Thresholds
            frame->m_lcuQpOffs[0][ctb_addr] =  calq;
            
            Ipp32s lcuQp = frame->m_lcuQps[0][ctb_addr] + calq;
            lcuQp = Saturate(0, 51, lcuQp);

            frame->m_lcuQps[0][ctb_addr] = (Ipp8s)lcuQp;

            Ipp32s col =  (ctb_addr % par.PicWidthInCtbs);
            Ipp32s row =  (ctb_addr / par.PicWidthInCtbs);
            Ipp32s pelX = col * par.MaxCUSize;
            Ipp32s pelY = row * par.MaxCUSize;

            for (Ipp32s depth = 1; depth < 4; depth++) {
                Ipp32s log2BlkSize = par.Log2MaxCUSize - depth;
                Ipp32s width = IPP_MIN(64, frame->m_origin->width-pelX);
                Ipp32s height = IPP_MIN(64, frame->m_origin->height-pelY);
                for (Ipp32s y = 0; y < height; y += (1<<log2BlkSize)) {
                    for (Ipp32s x = 0; x < width; x += (1<<log2BlkSize)) {
                        Ipp32s qpBlkIdx = ((pelY+y)>>log2BlkSize)*(par.PicWidthInCtbs<<(6-log2BlkSize)) + ((pelX+x)>>log2BlkSize);
                        frame->m_lcuQpOffs[depth][qpBlkIdx] = calq;
                        frame->m_lcuQps[depth][qpBlkIdx] = lcuQp;
                    }
                }
            }
        }
    }

}

#if 0
Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    Ipp32s numRows = rowsInRegion;
                    if(((Ipp32s)(region_row * rowsInRegion) + numRows)*SIZE_BLK_LA > frame->height)
                            numRows = (frame->height - (region_row * rowsInRegion)*SIZE_BLK_LA) / SIZE_BLK_LA;
#endif

namespace {
    void SumUp(Ipp32s *out, Ipp32s pitchOut, const Ipp32s *in, Ipp32s pitchIn, Ipp32u Width, Ipp32u Height, Ipp32s shift) {
        for (Ipp32u y = 0; y < ((Height+(1<<shift)-1)>>shift); y++) {
            for (Ipp32u x = 0; x < ((Width+(1<<shift)-1)>>shift); x++)
                out[y*pitchOut+x] = in[(2*y)*pitchIn+(2*x)]+in[(2*y)*pitchIn+(2*x+1)]+in[(2*y+1)*pitchIn+(2*x)]+in[(2*y+1)*pitchIn+(2*x+1)];
        }
    }
};

#ifdef AMT_HROI_PSY_AQ

static void setCtb_SegMap_Cmplx(Frame *frame, H265VideoParam *par)
{
    Ipp32s lumaSize = par->Width * par->Height;

    Ipp32s QP = par->QPI + frame->m_pyramidLayer + 1;  // frame->m_sliceQpY;

    Ipp32s numCtb = par->PicWidthInCtbs * par->PicHeightInCtbs;
    Ipp32s maxCuSize = (par->MaxCUSize * par->MaxCUSize);

    Statistics *stats = frame->m_stats[0];
    CtbRoiStats *ctbStats = stats->ctbStats;

    Ipp32s ctbWidthBnd = frame->m_origin->width - (par->PicWidthInCtbs-1) * par->MaxCUSize;
    Ipp32s ctbHeightBnd = frame->m_origin->height - (par->PicHeightInCtbs-1) * par->MaxCUSize;

    // seg info=====================================================================================
    Ipp8u  *segMap = &(stats->roi_map_8x8[0]);
    Ipp32s segBlkPitch = stats->m_RoiPitch;
    Ipp8u  *lum8x8 = &(stats->lum_avg_8x8[0]);
    Ipp32s lcuDimInSegBlks = par->MaxCUSize >> 3;
    Ipp32s lcuWBndDimInSegBlks = ctbWidthBnd >> 3;
    Ipp32s lcuHBndDimInSegBlks = ctbHeightBnd >> 3;

    // complexity info==============================================================================
    Statistics *statsCmplx = frame->m_stats[par->LowresFactor ? 1 : 0];
    Ipp32s cmplxBlkPitch = (frame->m_origin->width  + (SIZE_BLK_LA - 1)) / SIZE_BLK_LA; 
    Ipp32s cmplxBlkW = SIZE_BLK_LA;
    Ipp32s cmplxBlkH = SIZE_BLK_LA;
    if(par->LowresFactor) {
        cmplxBlkPitch = (frame->m_lowres->width + (SIZE_BLK_LA-1)) / SIZE_BLK_LA;
        cmplxBlkW = SIZE_BLK_LA<<par->LowresFactor;
        cmplxBlkH = SIZE_BLK_LA<<par->LowresFactor;
    }
    Ipp32s lcuDimInCmplxBlks = par->MaxCUSize/cmplxBlkW;
    Ipp32s lcuWBndDimInCmplxBlks = (ctbWidthBnd + (cmplxBlkW-1))/cmplxBlkW;
    Ipp32s lcuHBndDimInCmplxBlks = (ctbHeightBnd + (cmplxBlkW-1))/cmplxBlkW;

    Ipp64f picCmplx = 0.0;

    // initialize picture stats
    for(Ipp32s i = 0; i < NUM_HROI_CLASS; i++)
    {
        stats->roi_pic.numCtbRoi[i] = 0;
        stats->roi_pic.cmplx[i] = 0.0;
    }

    Ipp32s luminanceAvg = 0;
    Ipp32s numAct = 0;
    for(Ipp32s ctb = 0; ctb < numCtb; ctb++)
    {
        Ipp32s ctbCol =  (ctb % par->PicWidthInCtbs);
        Ipp32s ctbRow =  (ctb / par->PicWidthInCtbs);

        Ipp32s luminance = 0;

        // seg info==================================================================================
        Ipp32s hS = (ctbRow == par->PicHeightInCtbs-1) ? lcuHBndDimInSegBlks : lcuDimInSegBlks;
        Ipp32s wS = (ctbCol == par->PicWidthInCtbs-1) ? lcuWBndDimInSegBlks : lcuDimInSegBlks;
        Ipp32s segSum = 0;

        for(Ipp32s i = 0; i < hS; i++)
        {
            for(Ipp32s j = 0; j < wS; j++)
            {
                Ipp32s offset = (ctbRow * lcuDimInSegBlks + i) * segBlkPitch + ctbCol * lcuDimInSegBlks + j;
                if(segMap[offset])
                    segSum++;

                luminance += lum8x8[offset];
            }
        }

        ctbStats[ctb].segCount = segSum;
        Ipp32s isFullFlag = (segSum > 31) ? 1 : 0;

        // complexity================================================================================
        Ipp32s hC = (ctbRow == par->PicHeightInCtbs-1) ? lcuHBndDimInCmplxBlks : lcuDimInCmplxBlks;
        Ipp32s wC = (ctbCol == par->PicWidthInCtbs-1) ? lcuWBndDimInCmplxBlks : lcuDimInCmplxBlks;
        Ipp32f stc = 0.0;

        for(Ipp32s i = 0; i < hC; i++)
        {
            for(Ipp32s j = 0; j < wC; j++)
            {
                Ipp32s offset = (ctbRow * lcuDimInCmplxBlks + i) * cmplxBlkPitch + ctbCol * lcuDimInCmplxBlks + j;
                Ipp64f cmplx = (Ipp64f)IPP_MIN(statsCmplx->m_interSatd[offset], statsCmplx->m_intraSatd[offset]);
                stc += cmplx;
            }
        }

        //stc /= ctbSizeCmplx;
        stc /= (hC*wC);  // ?? not needed

        ctbStats[ctb].complexity = stc;

        ctbStats[ctb].luminance = 1 + luminance/(wS*hS);

        if(ctbStats[ctb].luminance>=32) {
            luminanceAvg += ctbStats[ctb].luminance;
            numAct++;
        }

        picCmplx += stc;
        Ipp32s ctbClass = 0;

        if(ctbStats[ctb].segCount) {
            ctbClass++;
            if(isFullFlag) {
                ctbClass++;
            }
        }

        ctbStats[ctb].roiLevel = ctbClass;

        stats->roi_pic.numCtbRoi[ctbClass]++;

        stats->roi_pic.cmplx[ctbClass] += stc;

    }

    if(numAct) luminanceAvg /= numAct;
    stats->roi_pic.luminanceAvg = luminanceAvg;

}

void CopyFramePtrInfoToFrameBuffElement(FrameBuffElement &tmp, Frame *frm)
{
    tmp.poc     = frm->m_frameOrder;
    tmp.frameY  = frm->m_origin->y;
    tmp.frameU  = frm->m_origin->uv;
    tmp.frameV  = frm->m_origin->uv+1;
    tmp.h       = (frm->m_origin->height+15)&~15;    // 16
    tmp.w       = (frm->m_origin->width+15)&~15;    // 16
    tmp.p       = frm->m_origin->pitch_luma_pix;
    tmp.pc      = frm->m_origin->pitch_chroma_pix;
    tmp.nv12    = 1;
}

#endif


mfxStatus Lookahead::Execute(ThreadingTask& task)
{
    Ipp32s frameOrder = task.poc;
    if (frameOrder < 0)
        return MFX_ERR_NONE;

    ThreadingTaskSpecifier action = task.action;
    Ipp32u region_row = task.row;
    //Ipp32u region_col = task.col;
    FrameIter begin = m_inputQueue.begin();
    FrameIter end   = m_inputQueue.end();

    // fix for interlace processing to keep old-style (pair)
    Frame* in[2] = {NULL, NULL};
    {
        FrameIter it = std::find_if(begin, end, isEqual(frameOrder));
        in[0] = (it == end) ? NULL : *it;

        if ((m_videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE) && in[0]) {
            it++;
            in[1] = (it == end) ? NULL : *it;
        }
    }

    Ipp32s fieldCount = (m_videoParam.picStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    Ipp8u useLowres = m_videoParam.LowresFactor;

    switch (action) {
    case TT_PREENC_START:
        {
            // do this stage only once per frame
            if ((m_lastAcceptedFrame[0] && in[0]) && (m_lastAcceptedFrame[0]->m_frameOrder == in[0]->m_frameOrder)) {
                break;
            }

            for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {

                if (in[fieldNum] && (useLowres || m_videoParam.SceneCut)) {
                    if (in[fieldNum]->m_bitDepthLuma == 8)
                        Scale<Ipp8u>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres);
                    else
                        Scale<Ipp16u>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres);

                    if (m_videoParam.DeltaQpMode || m_videoParam.AnalyzeCmplx)
                        PadRectLuma(*in[fieldNum]->m_lowres, m_videoParam.fourcc, 0, 0, in[fieldNum]->m_lowres->width, in[fieldNum]->m_lowres->height);
                }
                if (m_videoParam.SceneCut) {
                    //Ipp32s updateGop = 1;
                    //Ipp32s updateState = 1;
                    //DetectSceneCut(begin, end, in[fieldNum], updateGop, updateState);
                    AnalyzeSceneCut_AndUpdateState_Atul(in[fieldNum]);
                } 
            }

#ifdef AMT_HROI_PSY_AQ
            if(m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI) {
                FrameBuffElement tmp;
                CopyFramePtrInfoToFrameBuffElement(tmp, in[0]);
                // Init slicing
                if(frameOrder == 0) FS_set_Slice(m_enc.m_faceSkinDet, tmp.w, tmp.h, tmp.p, tmp.pc, IPP_MIN(8, m_regionCount));
                
                if(m_videoParam.DeltaQpMode & AMT_DQP_HROI) {
                    Frame *frm = in[0];
                    if(!m_videoParam.SceneCut) {
                        Ipp32s sceneCut = DetectSceneCut_AMT(in[0], (in[0]->m_frameOrder > 0) ? m_lastAcceptedFrame[0] : NULL);
                        if (frm->m_stats[1]) frm->m_stats[1]->m_sceneChange = sceneCut;
                        if (frm->m_stats[0]) frm->m_stats[0]->m_sceneChange = sceneCut;
                    }
                    
                    if(in[0]->m_frameOrder == 0) FS_set_SChg(m_enc.m_faceSkinDet, 1); 
                    else if (frm->m_stats[1])    FS_set_SChg(m_enc.m_faceSkinDet, frm->m_stats[1]->m_sceneChange);
                    else if (frm->m_stats[0])    FS_set_SChg(m_enc.m_faceSkinDet, frm->m_stats[0]->m_sceneChange);

                    FS_ProcessMode1_Slice_start(m_enc.m_faceSkinDet, &tmp, &(in[0]->m_stats[0]->roi_map_8x8[0]));
                } else {
                    FS_Luma_Slice_start(m_enc.m_faceSkinDet, &tmp);
                }
            }
#endif

            for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                m_lastAcceptedFrame[fieldNum] = in[fieldNum];
            }
            break;
        }


    case TT_PREENC_ROUTINE:
        {

#ifdef AMT_HROI_PSY_AQ
            if(m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI) {
                if(m_videoParam.DeltaQpMode & AMT_DQP_HROI) {
                    if(region_row < (Ipp32u) FS_get_NumSlice(m_enc.m_faceSkinDet))
                        FS_ProcessMode1_Slice_main (m_enc.m_faceSkinDet, region_row);
                } else if(m_videoParam.DeltaQpMode & AMT_DQP_PSY) {
                    if(region_row <  (Ipp32u) FS_get_NumSlice(m_enc.m_faceSkinDet))
                        FS_Luma_Slice_main (m_enc.m_faceSkinDet, region_row);
                }
            }
#endif

            for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) 
            {
                if (in[fieldNum] && in[fieldNum]->m_frameOrder > 0 &&
                    (m_videoParam.AnalyzeCmplx || (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL)))) // PAQ/CAL disable in encOrder
                {
                    Frame* curr = in[fieldNum];
                    FrameIter it = std::find_if(begin, end, isEqual(curr->m_frameOrder));
                    if (it == begin || it == end) {
                        it = end;
                    } else {
                        it--;
                        if (fieldCount > 1 && m_videoParam.MaxRefIdxP[0] > 1) {  // condition for field parity analysis
                            if (it != begin && (*it)->m_picCodeType != MFX_FRAMETYPE_I && curr->m_frameOrder > 1) { // check for I frame
                                it--; // maintain Interlace field parity
                            }
                        }
                    }

                    // ME (prev, curr)
                    if (it != end) {
                        Frame* prev = (*it);
                        FrameData* frame = useLowres ? curr->m_lowres : curr->m_origin;
                        Statistics* stat = curr->m_stats[useLowres ? 1 : 0];
                        FrameData* framePrev = useLowres ? prev->m_lowres : prev->m_origin;

                        Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                        for (Ipp32s i = 0; i < numActiveRows; i++) {
                            DoInterAnalysis_OneRow(frame, NULL, framePrev, &stat->m_interSad[0], &stat->m_interSatd[0],
                                &stat->m_mv[0], 0, 0, m_videoParam.LowresFactor, curr->m_bitDepthLuma, region_row * rowsInRegion + i,
                                m_videoParam.FullresMetrics ? curr->m_origin : NULL,
                                m_videoParam.FullresMetrics ? prev->m_origin : NULL);
                        }
#ifndef LOW_COMPLX_PAQ
                        if ((m_videoParam.DeltaQpMode & AMT_DQP_PAQ) && useLowres) { // need refine for _Original_ resolution
                            FrameData* frame = curr->m_origin;
                            Statistics* originStat = curr->m_stats[0];
                            Statistics* lowresStat = curr->m_stats[1];
                            FrameData* framePrev = prev->m_origin;

                            Ipp32s rowsInRegion = m_originRowsInRegion;
                            Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                            for (Ipp32s i=0; i<numActiveRows; i++) {
                                DoInterAnalysis_OneRow(frame, lowresStat, framePrev, &originStat->m_interSad[0], NULL, &originStat->m_mv[0], 1, 0, m_videoParam.LowresFactor, curr->m_bitDepthLuma, region_row * rowsInRegion + i);//fixed-NG
                            }
                        }
#endif
                    }
#ifndef LOW_COMPLX_PAQ
                    // ME (prevRef <-  currRef ->  nextRef)
                    if (m_videoParam.DeltaQpMode & AMT_DQP_PAQ) {
                        Ipp32s frameOrderCentral = curr->m_frameOrder - m_bufferingPaq;
                        bool doAnalysis = (frameOrderCentral >= 0);

                        if (doAnalysis) {
                            FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));
                            Ipp32u frameType = (*curr)->m_picCodeType;
                            bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);
                            if (isRef) {
                                Frame* prevP = GetPrevAnchor(begin, end, *curr);
                                Frame* nextP = GetNextAnchor(curr, end);
                                DoPDistInterAnalysis_OneRow(*curr, prevP, nextP, region_row, m_lowresRowsInRegion, m_originRowsInRegion, m_videoParam.LowresFactor, m_videoParam.FullresMetrics);
                            }
                        }
                    }
#endif
                } // foreach field
                else {
                    Frame* curr = in[fieldNum];
                    Statistics* stat = curr->m_stats[ useLowres ? 1 : 0 ];
                    FrameData* frame = useLowres ? curr->m_lowres : curr->m_origin;
                    Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);
                    Ipp32s picWidthInBlks = (frame->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
                    Ipp32s fPos = (region_row * rowsInRegion) * picWidthInBlks; // [fPos, lPos)
                    Ipp32s lPos = fPos + numActiveRows*picWidthInBlks;
                    std::fill(stat->m_interSad.begin()+fPos, stat->m_interSad.begin()+lPos, IPP_MAX_32S);
                    std::fill(stat->m_interSatd.begin()+fPos, stat->m_interSatd.begin()+lPos, IPP_MAX_32S);
                }
            }

            // INTRA:  optional ORIGINAL / LOWRES 
            if (in && m_videoParam.AnalyzeCmplx) {
                for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    FrameData* frame = useLowres ? in[fieldNum]->m_lowres : in[fieldNum]->m_origin;
                    Statistics* stat = in[fieldNum]->m_stats[useLowres ? 1 : 0];
                    Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);
                    Ipp32s size = SIZE_BLK_LA;
                    Ipp32s modes = 1;
                    if(useLowres && m_videoParam.FullresMetrics) {
                        frame = in[fieldNum]->m_origin;
                        size = (SIZE_BLK_LA)<<m_videoParam.LowresFactor;
                        modes = 0;
                    }
                    if (in[fieldNum]->m_bitDepthLuma == 8) {
                        for (Ipp32s i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<Ipp8u>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i, size, modes);
                    } else { 
                        for (Ipp32s i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<Ipp16u>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i, size, modes);
                    }
                }
            }


            // RsCs: only ORIGINAL
            if (in) {
                Ipp32s rowsInRegion = m_originRowsInRegion;
                Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, in[0]->m_origin->height);

                for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame* curr = in[fieldNum];
                    for(Ipp32s i=0;i<numActiveRows;i++) {
                        (curr->m_bitDepthLuma == 8)
                            ? CalcRsCs_OneRow<Ipp8u>(curr->m_origin, curr->m_stats[0], m_videoParam, region_row * rowsInRegion + i)
                            : CalcRsCs_OneRow<Ipp16u>(curr->m_origin, curr->m_stats[0], m_videoParam, region_row * rowsInRegion + i);
                    }
                }
            }
            break;
        }


    case TT_PREENC_END:
        {
            if (in) {
                for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame *curr = in[fieldNum];
                    Ipp32s width4  = (m_videoParam.Width+63)&~63;
                    Ipp32s height4 = (m_videoParam.Height+63)&~63;
                    Ipp32s pitchRsCs4  = curr->m_stats[0]->m_pitchRsCs4;
                    Ipp32s pitchRsCs8  = curr->m_stats[0]->m_pitchRsCs4>>1;
                    Ipp32s pitchRsCs16 = curr->m_stats[0]->m_pitchRsCs4>>2;
                    Ipp32s pitchRsCs32 = curr->m_stats[0]->m_pitchRsCs4>>3;
                    Ipp32s pitchRsCs64 = curr->m_stats[0]->m_pitchRsCs4>>4;
                    Ipp32s *rs4  = curr->m_stats[0]->m_rs[0];
                    Ipp32s *rs8  = curr->m_stats[0]->m_rs[1];
                    Ipp32s *rs16 = curr->m_stats[0]->m_rs[2];
                    Ipp32s *rs32 = curr->m_stats[0]->m_rs[3];
                    Ipp32s *rs64 = curr->m_stats[0]->m_rs[4];
                    Ipp32s *cs4  = curr->m_stats[0]->m_cs[0];
                    Ipp32s *cs8  = curr->m_stats[0]->m_cs[1];
                    Ipp32s *cs16 = curr->m_stats[0]->m_cs[2];
                    Ipp32s *cs32 = curr->m_stats[0]->m_cs[3];
                    Ipp32s *cs64 = curr->m_stats[0]->m_cs[4];

                    SumUp(rs8,  pitchRsCs8,  rs4,  pitchRsCs4,  m_videoParam.Width, m_videoParam.Height, 3);
                    SumUp(rs16, pitchRsCs16, rs8,  pitchRsCs8,  m_videoParam.Width, m_videoParam.Height, 4);
                    SumUp(rs32, pitchRsCs32, rs16, pitchRsCs16, m_videoParam.Width, m_videoParam.Height, 5);
                    SumUp(rs64, pitchRsCs64, rs32, pitchRsCs32, m_videoParam.Width, m_videoParam.Height, 6);
                    SumUp(cs8,  pitchRsCs8,  cs4,  pitchRsCs4,  m_videoParam.Width, m_videoParam.Height, 3);
                    SumUp(cs16, pitchRsCs16, cs8,  pitchRsCs8,  m_videoParam.Width, m_videoParam.Height, 4);
                    SumUp(cs32, pitchRsCs32, cs16, pitchRsCs16, m_videoParam.Width, m_videoParam.Height, 5);
                    SumUp(cs64, pitchRsCs64, cs32, pitchRsCs32, m_videoParam.Width, m_videoParam.Height, 6);
                }
            }
            // syncpoint only (like accumulation & refCounter--), no real hard working here!!!
            for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                if (m_videoParam.AnalyzeCmplx) {
                    FrameIter it = std::find_if(begin, end, isEqual(in[fieldNum]->m_frameOrder));
                    if (it == begin || it == end) {
                        it = end;
                    } else {
                        it--;
                        if(fieldCount > 1 && m_videoParam.MaxRefIdxP[0] > 1) {  // condition for field parity analysis
                            if (it == begin) {
                                it = end;   // don't release
                            } else {
                                it--;       // release same field parity
                            }
                        }
                    }
                    if (it != end) (*it)->m_lookaheadRefCounter--;
                }
                // PSY HROI tightly coupled to AnalyzeCmplx, not independent owner
                if (m_videoParam.DeltaQpMode & ~AMT_DQP_PSY_HROI) {
                    AverageRsCs(in[fieldNum], m_videoParam);
                    if ((m_videoParam.DeltaQpMode & ~AMT_DQP_PSY_HROI) == AMT_DQP_CAQ) {
                        // CAQ only Mode 
                        if (in[fieldNum]) in[fieldNum]->m_lookaheadRefCounter--;
                    } else { 
                        // & (PAQ | CAL) (and/or CAQ): PAQ, CAL have higher buffering requirement.
                        Ipp32s frameOrderCentral = in[fieldNum]->m_frameOrder - m_bufferingPaq;
                        Ipp32s updateState = 1;
                        BuildQpMap(begin, end, frameOrderCentral, m_videoParam, updateState);
                    }
                }
            }
#ifdef AMT_HROI_PSY_AQ
            if(m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI) {
                if((m_videoParam.DeltaQpMode & AMT_DQP_HROI) == 0) {
                    FS_Luma_Slice_end (m_enc.m_faceSkinDet, &(in[0]->m_stats[0]->lum_avg_8x8[0]), in[0]->m_stats[0]->lum_avg_8x8.size());
                } else {
                    FS_ProcessMode1_Slice_end (m_enc.m_faceSkinDet, &(in[0]->m_stats[0]->roi_map_8x8[0]), &(in[0]->m_stats[0]->lum_avg_8x8[0]), in[0]->m_stats[0]->lum_avg_8x8.size());
                }
                // set seg map and complexity of ctb
                setCtb_SegMap_Cmplx(in[0], &m_videoParam);
            }
#endif

            break;
        }
    case TT_HUB:
        {
            break;
        }
    default: 
        {
            assert(!"invalid lookahead action");
        }
    };

    //vm_interlocked_inc32(&m_task->m_numFinishedThreadingTasks);
    return MFX_ERR_NONE;

} // 

//
// AMT SceneCut algorithm
//
enum {    
        REF = 0,
        CUR = 1,
    
        PAD_V = 2,
        PAD_H = 2,
        LOWRES_W  = 112,
        LOWRES_H =  64,
        LOWRES_SIZE = LOWRES_W * LOWRES_H,
        LOWRES_PITCH = LOWRES_W + 2 *PAD_H,

        BLOCK_W = 8,
        BLOCK_H = 8,
        WIDTH_IN_BLOCKS  = LOWRES_W / BLOCK_W,
        HEIGHT_IN_BLOCKS = LOWRES_H / BLOCK_H,
        NUM_BLOCKS = (LOWRES_H / BLOCK_H) * (LOWRES_W / BLOCK_W),

        NUM_TSC = 10,
        NUM_SC  = 10
    };

#define NABS(a)           (((a)<0)?(-(a)):(a))

    Ipp32s SCDetectUF1(
        Ipp32f diffMVdiffVal, Ipp32f RsCsDiff,   Ipp32f MVDiff,   Ipp32f Rs,       Ipp32f AFD,
        Ipp32f CsDiff,        Ipp32f diffTSC,    Ipp32f TSC,      Ipp32f gchDC,    Ipp32f diffRsCsdiff,
        Ipp32f posBalance,    Ipp32f SC,         Ipp32f TSCindex, Ipp32f Scindex,  Ipp32f Cs,
        Ipp32f diffAFD,       Ipp32f negBalance, Ipp32f ssDCval,  Ipp32f refDCval, Ipp32f RsDiff);

    const H265MV ZERO_MV = {0,0};

    void MotionRangeDeliveryF(Ipp32s xLoc, Ipp32s yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup, Ipp32s *limitYdown) 
    {
        Ipp32s Extended_Height = LOWRES_H + 2 *PAD_V;
        Ipp32s locY = yLoc / ((3 * (16 / BLOCK_H)) / 2);
        Ipp32s locX = xLoc / ((3 * (16 / BLOCK_W)) / 2);
        *limitXleft = IPP_MAX(-16, -(xLoc * BLOCK_W) - PAD_H);
        *limitXright = IPP_MIN(15, LOWRES_PITCH - (((xLoc + 1) * BLOCK_W) + PAD_H));
        *limitYup = IPP_MAX(-12, -(yLoc * BLOCK_H) - PAD_V);
        *limitYdown = IPP_MIN(11, Extended_Height - (((yLoc + 1) * BLOCK_W) + PAD_V));
    }

    Ipp32s MVcalcSAD8x8(H265MV MV, Ipp8u* curY, Ipp8u* refY, Ipp32u *bestSAD, Ipp32s *distance) 
    {
        Ipp32s preDist = (MV.mvx * MV.mvx) + (MV.mvy * MV.mvy);
        Ipp8u* fRef = refY + MV.mvx + (MV.mvy * LOWRES_PITCH);
        Ipp32u SAD = MFX_HEVC_PP::NAME(h265_SAD_8x8_general_8u)(curY, fRef, LOWRES_PITCH, LOWRES_PITCH);
        if ((SAD < *bestSAD) || ((SAD == *(bestSAD)) && *distance > preDist)) {
            *distance = preDist;
            *(bestSAD) = SAD;
            return 1;
        }
        return 0;
    }

    void SearchLimitsCalc(Ipp32s xLoc, Ipp32s yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup, Ipp32s *limitYdown, Ipp32s range, H265MV mv) 
    {
        Ipp32s Extended_Height = LOWRES_H + 2 *PAD_V;
        Ipp32s locX  = (xLoc * BLOCK_W) + PAD_H + mv.mvx;
        Ipp32s locY  = (yLoc * BLOCK_H) + PAD_V + mv.mvy;
        *limitXleft  = IPP_MAX(-locX,-range);
        *limitXright = IPP_MIN(LOWRES_PITCH - ((xLoc + 1) * BLOCK_W) - PAD_H - mv.mvx, range);
        *limitYup    = IPP_MAX(-locY,-range);
        *limitYdown  = IPP_MIN(Extended_Height - ((yLoc + 1) * BLOCK_H) - PAD_V - mv.mvy, range);
    }

    Ipp32s DistInt(H265MV vector) 
    {
        return (vector.mvx*vector.mvx) + (vector.mvy*vector.mvy);
    }

    void MVpropagationCheck(Ipp32s xLoc, Ipp32s yLoc, H265MV *propagatedMV) 
    {
        Ipp32s Extended_Height = LOWRES_H + 2 *PAD_V;
        Ipp32s
            left = (xLoc * BLOCK_W) + PAD_H,
            right = (LOWRES_PITCH - left - BLOCK_W),
            up = (yLoc * BLOCK_H) + PAD_V,
            down = (Extended_Height - up - BLOCK_H);

        if(propagatedMV->mvx < 0) {
            if(left + propagatedMV->mvx < 0)
                propagatedMV->mvx = -left;
        } else {
            if(right - propagatedMV->mvx < 0)
                propagatedMV->mvx = right;
        }

        if(propagatedMV->mvy < 0) {
            if(up + propagatedMV->mvy < 0)
                propagatedMV->mvy = -up;
        } else {
            if(down - propagatedMV->mvy < 0)
                propagatedMV->mvy = down;
        }
    }

    struct Rsad
    {
        Ipp32u SAD;
        Ipp32u distance;
        H265MV BestMV;
    };

    #define EXTRANEIGHBORS

    Ipp32s HME_Low8x8fast(Ipp8u *src, Ipp8u *ref, H265MV *srcMv, Ipp32s fPos, Ipp32s first, Ipp32u accuracy, Ipp32u* SAD) 
    {
        H265MV
            lineMV[2],
            tMV,
            ttMV,
            preMV = ZERO_MV,
            Nmv;
        Ipp32s 
            limitXleft = 0,
            limitXright = 0,
            limitYup = 0,
            limitYdown = 0,
            direction[2],
            counter,
            lineSAD[2],
            distance = INT_MAX,
            plane = 0,
            bestPlane = 0,
            bestPlaneN = 0,
            zeroSAD = INT_MAX;
            
        Ipp32u
            tl,
            *outSAD,
            bestSAD = INT_MAX;
        Rsad
            range;
        Ipp32s
            foundBetter = 0,//1,
            truneighbor = 0;//1;

        Ipp32s xLoc = (fPos % WIDTH_IN_BLOCKS);
        Ipp32s yLoc = (fPos / WIDTH_IN_BLOCKS);
        Ipp32s offset = (yLoc * LOWRES_PITCH * BLOCK_H) + (xLoc * BLOCK_W);
        Ipp8u *objFrame = src + offset;
        Ipp8u *refFrame = ref + offset;
        H265MV *current = srcMv;
        outSAD = SAD;
        Ipp32s acc = (accuracy == 1) ? 1 : 4;

        if (first) {
            MVcalcSAD8x8(ZERO_MV, objFrame, refFrame, &bestSAD, &distance);
            zeroSAD = bestSAD;
            current[fPos] = ZERO_MV;
            outSAD[fPos] = bestSAD;
            if (zeroSAD == 0)
                return zeroSAD;
            MotionRangeDeliveryF(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown);
        }
        
        range.SAD = outSAD[fPos];
        range.BestMV = current[fPos];
        range.distance = INT_MAX;
        
        direction[0] = 0;
        lineMV[0].mvx = 0;
        lineMV[0].mvy = 0;
        {
            Ipp8u *ps = objFrame;
            Ipp8u *pr = refFrame + (limitYup * LOWRES_PITCH) + limitXleft;
            Ipp32s xrange = limitXright - limitXleft + 1,
                yrange = limitYdown - limitYup + 1,
                bX = 0,
                bY = 0;
            Ipp32u bSAD = INT_MAX;
            h265_SearchBestBlock8x8(ps, pr, LOWRES_PITCH, xrange, yrange, &bSAD, &bX, &bY);
            if (bSAD < range.SAD) {
                range.SAD = bSAD;
                range.BestMV.mvx = bX + limitXleft;
                range.BestMV.mvy = bY + limitYup;
                range.distance = DistInt(range.BestMV);
            }
        }
        outSAD[fPos] = range.SAD;
        current[fPos] = range.BestMV;

        /*ExtraNeighbors*/
#ifdef EXTRANEIGHBORS
        if (fPos > (WIDTH_IN_BLOCKS * 3)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 3)].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 3)].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos > (WIDTH_IN_BLOCKS * 2)) && (xLoc > 0)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2) - 1].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2) - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (fPos > (WIDTH_IN_BLOCKS * 2)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2)].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2)].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos > (WIDTH_IN_BLOCKS * 2)) && (xLoc < WIDTH_IN_BLOCKS - 1)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2) + 1].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2) + 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
#endif
        if ((fPos > WIDTH_IN_BLOCKS) && (xLoc > 0)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS - 1].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (fPos > WIDTH_IN_BLOCKS) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos> WIDTH_IN_BLOCKS) && (xLoc < WIDTH_IN_BLOCKS - 1)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS + 1].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS + 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (xLoc > 0) {
            tl = range.SAD;
            distance = INT_MAX;
            Nmv.mvx = current[fPos - 1].mvx;
            Nmv.mvy = current[fPos - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (xLoc > 1) {
            tl = range.SAD;
            distance = INT_MAX;
            Nmv.mvx = current[fPos - 2].mvx;
            Nmv.mvy = current[fPos - 2].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (truneighbor) {
            ttMV = range.BestMV;
            bestSAD = range.SAD;
            SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 1, range.BestMV);
            for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy++) {
                for (tMV.mvx = limitXleft; tMV.mvx <= limitXright; tMV.mvx++) {
                    preMV.mvx = tMV.mvx + ttMV.mvx;
                    preMV.mvy = tMV.mvy + ttMV.mvy;
                    foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame, &bestSAD, &distance);
                    if (foundBetter) {
                        range.BestMV = preMV;
                        range.SAD = bestSAD;
                        bestPlaneN = plane;
                        foundBetter = 0;
                    }
                }
            }

            if (truneighbor) {
                outSAD[fPos] = range.SAD;
                current[fPos] = range.BestMV;
                truneighbor = 0;
                bestPlane = bestPlaneN;
            }
        }
        range.SAD = outSAD[fPos];
        /*Zero search*/
        if (!first) {
            SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 16/*32*/, ZERO_MV);
            counter = 0;
            for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy += 2) {
                lineSAD[0] = INT_MAX;
                for (tMV.mvx = limitXleft + ((counter % 2) * 2); tMV.mvx <= limitXright; tMV.mvx += 4) {
                    ttMV.mvx = tMV.mvx + ZERO_MV.mvx;
                    ttMV.mvy = tMV.mvy + ZERO_MV.mvy;
                    bestSAD = range.SAD;
                    distance = range.distance;
                    foundBetter = MVcalcSAD8x8(ttMV, objFrame, refFrame, &bestSAD, &distance);
                    if (foundBetter) {
                        range.BestMV = ttMV;
                        range.SAD = bestSAD;
                        range.distance = distance;
                        foundBetter = 0;
                        bestPlane = 0;
                    }
                }
                counter++;
            }
        }
        ttMV = range.BestMV;
        SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 1, range.BestMV);
        for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy++) {
            for (tMV.mvx = limitXleft; tMV.mvx <= limitXright; tMV.mvx++) {
                preMV.mvx = tMV.mvx + ttMV.mvx;
                preMV.mvy = tMV.mvy + ttMV.mvy;
                foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame, &bestSAD, &distance);
                if (foundBetter) {
                    range.BestMV = preMV;
                    range.SAD = bestSAD;
                    range.distance = distance;
                    bestPlane = 0;
                    foundBetter = 0;
                }
            }
        }
        outSAD[fPos] = range.SAD;
        current[fPos] = range.BestMV;

        return(zeroSAD);
    }

    static Ipp32s PDISTTbl2[/*NumTSC*NumSC*/] =
    {
        2, 3, 3, 4, 4, 5, 5, 5, 5, 5,
        2, 2, 3, 3, 4, 4, 5, 5, 5, 5,
        1, 2, 2, 3, 3, 3, 4, 4, 5, 5,
        1, 1, 2, 2, 3, 3, 3, 4, 4, 5,
        1, 1, 2, 2, 3, 3, 3, 3, 3, 4,
        1, 1, 1, 2, 2, 3, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 2, 3, 3, 3,
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };


    const Ipp32f FLOAT_MAX  =       2241178.0;

    static Ipp32f
        lmt_sc2[/*NumSC*/] = { 4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78.0, FLOAT_MAX },             // lower limit of SFM(Rs,Cs) range for spatial classification
        // 9 ranges of SC are: 0 0-4, 1 4-9, 2 9-15, 3 15-23, 4 23-32, 5 32-44, 6 42-53, 7 53-65, 8 65-78, 9 78->??
        lmt_tsc2[/*NumTSC*/] = { 0.75, 1.5, 2.25, 3.0, 4.0, 5.0, 6.0, 7.5, 9.25, FLOAT_MAX };               // lower limit of AFD
        // 8 ranges of TSC (based on FD) are:0 0-0.75 1 0.75-1.5, 2 1.5-2.25. 3 2.25-3, 4 3-4, 5 4-5, 6 5-6, 7 6-7.5, 8 7.5-9.25, 9 9.25->??

    void SceneStats::Create(const AllocInfo &allocInfo)
    {
        for (Ipp32s idx = 0; idx < 2; idx++) {
            memset(data, 0, sizeof(data)/sizeof(data[0]));
            Ipp32s initialPoint = (LOWRES_PITCH * PAD_V) + PAD_H;
            Y = data + initialPoint;
        }
    }

    template <class T> void SubSampleImage(const T *pSrc, Ipp32s srcWidth, Ipp32s srcHeight, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstWidth, Ipp32s dstHeight, Ipp32s dstPitch) 
    {
        Ipp32s shift = 2*(sizeof(T) - 1);

        Ipp32s dstSSHstep = srcWidth / dstWidth;
        Ipp32s dstSSVstep = srcHeight / dstHeight;

        //No need for exact subsampling, approximation is good enough for analysis.
        for(Ipp32s i = 0, j = 0 ; i < dstHeight; i++, j += dstSSVstep) {
            for(Ipp32s k = 0, h = 0 ; k < dstWidth; k++, h += dstSSHstep) {
                pDst[(i * dstPitch) + k] = (Ipp8u)(pSrc[(j * srcPitch) + h] >> shift);
            }
        }
    }

    Ipp32s TableLookUp(Ipp32s limit, Ipp32f *table, Ipp32f comparisonValue) 
    {
        for (Ipp32s pos = 0; pos < limit; pos++) {
            if (comparisonValue < table[pos])
                return pos;
        }
        return limit;
    }

    Ipp32s ShotDetect(SceneStats& input, SceneStats& ref, Ipp32s* histogram, Ipp64s ssDCint, Ipp64s refDCint) 
    {
        Ipp32f RsDiff, CsDiff;
        {
            Ipp32f* objRs = input.Rs;
            Ipp32f* objCs = input.Cs;
            Ipp32f* refRs = ref.Rs;
            Ipp32f* refCs = ref.Cs;
            Ipp32s len = (2*WIDTH_IN_BLOCKS) * (2*HEIGHT_IN_BLOCKS); // blocks in 8x8 but RsCs in 4x4
            h265_ComputeRsCsDiff(objRs, objCs, refRs, refCs, len, &RsDiff, &CsDiff);
            RsDiff /= NUM_BLOCKS * (2 / (8 / BLOCK_H)) * (2 / (8 / BLOCK_W));
            CsDiff /= NUM_BLOCKS * (2 / (8 / BLOCK_H)) * (2 / (8 / BLOCK_W));
            input.RsCsDiff = (Ipp32f)sqrt((RsDiff*RsDiff) + (CsDiff*CsDiff));
        }
        
        Ipp32f ssDCval  = ssDCint * (1.0 / LOWRES_SIZE);
        Ipp32f refDCval = refDCint * (1.0 / LOWRES_SIZE);
        Ipp32f gchDC = NABS(ssDCval - refDCval);
        Ipp32f posBalance = (Ipp32f)(histogram[3] + histogram[4]);
        Ipp32f negBalance = (Ipp32f)(histogram[0] + histogram[1]);
        posBalance -= negBalance;
        posBalance = NABS(posBalance);
        Ipp32f histTOT = (Ipp32f)(histogram[0] + histogram[1] + histogram[2] + histogram[3] + histogram[4]);
        posBalance /= histTOT;
        negBalance /= histTOT;

        Ipp32f diffAFD = input.AFD - ref.AFD;
        Ipp32f diffTSC = input.TSC - ref.TSC;
        Ipp32f diffRsCsDiff = input.RsCsDiff - ref.RsCsDiff;
        Ipp32f diffMVdiffVal = input.MVdiffVal - ref.MVdiffVal;
        Ipp32s TSCindex = TableLookUp(NUM_TSC, lmt_tsc2, input.TSC);

        Ipp32f SC = (Ipp32f)sqrt(( input.avgRs * input.avgRs) + (input.avgCs * input.avgCs));
        Ipp32s SCindex  = TableLookUp(NUM_SC, lmt_sc2,  SC);

        Ipp32s Schg = SCDetectUF1(diffMVdiffVal, input.RsCsDiff, input.MVdiffVal, input.avgRs, input.AFD, CsDiff, diffTSC, input.TSC, gchDC, diffRsCsDiff, posBalance, SC, TSCindex, SCindex, input.avgCs, diffAFD, negBalance, ssDCval, refDCval, RsDiff);

        return Schg;
    }

    void MotionAnalysis(Ipp8u *src, H265MV *srcMv, Ipp8u *ref, H265MV *refMv, Ipp32f *TSC, Ipp32f *AFD, Ipp32f *MVdiffVal)
    {
        Ipp32u valNoM = 0, valb = 0;
        Ipp32u SAD[NUM_BLOCKS];

        *MVdiffVal = 0;
        for (Ipp32s fPos = 0; fPos < NUM_BLOCKS; fPos++) {
            valNoM += HME_Low8x8fast(src, ref, srcMv, fPos, 1, 1, SAD);
            valb += SAD[fPos];
            *MVdiffVal += (srcMv[fPos].mvx - refMv[fPos].mvx) * (srcMv[fPos].mvx - refMv[fPos].mvx);
            *MVdiffVal += (srcMv[fPos].mvy - refMv[fPos].mvy) * (srcMv[fPos].mvy - refMv[fPos].mvy);
        }

        *TSC = (Ipp32f)valb / (Ipp32f)LOWRES_SIZE;
        *AFD = (Ipp32f)valNoM / (Ipp32f)LOWRES_SIZE;
        *MVdiffVal = (Ipp32f)((Ipp32f)*MVdiffVal / (Ipp32f)NUM_BLOCKS);
    }

    Ipp32s H265Enc::DetectSceneCut_AMT(Frame* input, Frame* prev)
    {
        {
            Ipp32s srcWidth  = input->m_origin->width;
            Ipp32s srcHeight = input->m_origin->height;
            Ipp32s srcPitch  = input->m_origin->pitch_luma_pix;

            Ipp8u* lowres = input->m_sceneStats->Y;
            Ipp32s dstWidth  = LOWRES_W;
            Ipp32s dstHeight = LOWRES_H;
            Ipp32s dstPitch  = LOWRES_PITCH;

            if (input->m_bitDepthLuma == 8) {
                Ipp8u *origin = input->m_origin->y;
                SubSampleImage(origin, srcWidth, srcHeight, srcPitch, lowres, dstWidth, dstHeight, dstPitch);
            } else {
                Ipp16u *origin = (Ipp16u*)input->m_origin->y;
                SubSampleImage(origin, srcWidth, srcHeight, srcPitch, lowres, dstWidth, dstHeight, dstPitch);
            }
        }
        
        SceneStats* stats[2] = {prev ? prev->m_sceneStats : input->m_sceneStats, input->m_sceneStats};
        Ipp8u  *src  = stats[CUR]->Y;
        H265MV *srcMv =  stats[CUR]->mv;
        Ipp8u  *ref  = stats[REF]->Y;
        H265MV *refMv =  stats[REF]->mv;
        {
            const Ipp32s BLOCK_SIZE = 4;
            const Ipp32s hblocks = LOWRES_H / BLOCK_SIZE;
            const Ipp32s wblocks = LOWRES_W / BLOCK_SIZE;
            const Ipp32s len = hblocks * wblocks;
            SceneStats* data = stats[CUR];

            h265_ComputeRsCs4x4(src, LOWRES_PITCH, wblocks, hblocks, data->Rs, data->Cs);
            
            ippsSum_32f(data->Rs, len, &data->avgRs, ippAlgHintFast);
            data->avgRs = (Ipp32f)sqrt(data->avgRs / len);
            ippsSqrt_32f_I(data->Rs, len);
            ippsSum_32f(data->Cs, len, &data->avgCs, ippAlgHintFast);
            data->avgCs = (Ipp32f)sqrt(data->avgCs / len);
            ippsSqrt_32f_I(data->Cs, len);
        }

        Ipp32s Schg = 0;
        if (prev == NULL) {
            stats[CUR]->TSC = 0;
            stats[CUR]->AFD = 0;
            stats[CUR]->MVdiffVal = 0;
            stats[CUR]->RsCsDiff = 0;
        } else {
            MotionAnalysis(src, srcMv, ref, refMv, &stats[CUR]->TSC, &stats[CUR]->AFD, &stats[CUR]->MVdiffVal);

            Ipp64s ssDCint, refDCint;
            Ipp32s histogram[5];
            h265_ImageDiffHistogram(src, ref, LOWRES_PITCH, LOWRES_W, LOWRES_H, histogram, &ssDCint, &refDCint);
            Schg = ShotDetect(*stats[CUR], *stats[REF], histogram, ssDCint, refDCint);
        }

        return Schg;
    }

    Ipp32s SCDetectUF1(
        Ipp32f diffMVdiffVal, Ipp32f RsCsDiff,   Ipp32f MVDiff,   Ipp32f Rs,       Ipp32f AFD,
        Ipp32f CsDiff,        Ipp32f diffTSC,    Ipp32f TSC,      Ipp32f gchDC,    Ipp32f diffRsCsdiff,
        Ipp32f posBalance,    Ipp32f SC,         Ipp32f TSCindex, Ipp32f Scindex,  Ipp32f Cs,
        Ipp32f diffAFD,       Ipp32f negBalance, Ipp32f ssDCval,  Ipp32f refDCval, Ipp32f RsDiff) 
    {
        if (diffMVdiffVal <= 100.429) {
            if (diffAFD <= 12.187) {
                if (diffRsCsdiff <= 160.697) {
                    if (diffTSC <= 2.631) {
                        if (MVDiff <= 262.598) {
                            return 0;
                        } else if (MVDiff > 262.598) {
                            if (Scindex <= 2) {
                                if (RsCsDiff <= 258.302) {
                                    return 0;
                                } else if (RsCsDiff > 258.302) {
                                    if (AFD <= 29.182) {
                                        return 1;
                                    } else if (AFD > 29.182) {
                                        return 0;
                                    }
                                }
                            } else if (Scindex > 2) {
                                return 0;
                            }
                        }
                    } else if (diffTSC > 2.631) {
                        if (negBalance <= 0.317) {
                            return 0;
                        } else if (negBalance > 0.317) {
                            if (Cs <= 18.718) {
                                if (AFD <= 10.086) {
                                    return 1;
                                } else if (AFD > 10.086) {
                                    if (Scindex <= 3) {
                                        if (SC <= 20.257) {
                                            return 0;
                                        } else if (SC > 20.257) {
                                            if (AFD <= 23.616) {
                                                return 0;
                                            } else if (AFD > 23.616) {
                                                return 1;
                                            }
                                        }
                                    } else if (Scindex > 3) {
                                        return 0;
                                    }
                                }
                            }
                            else if (Cs > 18.718)
                            {
                                return 0;
                            }
                        }
                    }
                }
                else if (diffRsCsdiff > 160.697) {
                    if (Scindex <= 3) {
                        if (Rs <= 11.603) {
                            return 1;
                        } else if (Rs > 11.603) {
                            if (MVDiff <= 239.661) {
                                return 0;
                            } else if (MVDiff > 239.661) {
                                if (Rs <= 16.197) {
                                    return 1;
                                } else if (Rs > 16.197) {
                                    return 0;
                                }
                            }
                        }
                    } else if (Scindex > 3) {
                        return 0;
                    }
                }
            } else if (diffAFD > 12.187) {
                if (RsCsDiff <= 574.733) {
                    if (negBalance <= 0.304) {
                        if (MVDiff <= 226.839) {
                            return 0;
                        } else if (MVDiff > 226.839) {
                            if (diffRsCsdiff <= 179.802) {
                                return 0;
                            } else if (diffRsCsdiff > 179.802) {
                                if (RsDiff <= 187.951) {
                                    return 1;
                                } else if (RsDiff > 187.951) {
                                    return 0;
                                }
                            }
                        }
                    } else if (negBalance > 0.304) {
                        if (refDCval <= 51.521) {
                            return 1;
                        } else if (refDCval > 51.521) {
                            if (diffTSC <= 9.661) {
                                if (MVDiff <= 207.214) {
                                    return 0;
                                } else if (MVDiff > 207.214) {
                                    if (ssDCval <= 41.36) {
                                        return 1;
                                    } else if (ssDCval > 41.36) {
                                        if (diffRsCsdiff <= 330.407) {
                                            return 0;
                                        } else if (diffRsCsdiff > 330.407) {
                                            return 1;
                                        }
                                    }
                                }
                            } else if (diffTSC > 9.661) {
                                if (Scindex <= 5) {
                                    if (posBalance <= 0.661) {
                                        if (diffRsCsdiff <= 45.975) {
                                            return 0;
                                        } else if (diffRsCsdiff > 45.975) {
                                            if (Scindex <= 4) {
                                                return 1;
                                            } else if (Scindex > 4) {
                                                if (ssDCval <= 125.414) {
                                                    return 1;
                                                } else if (ssDCval > 125.414) {
                                                    return 0;
                                                }
                                            }
                                        }
                                    } else if (posBalance > 0.661) {
                                        return 0;
                                    }
                                } else if (Scindex > 5) {
                                    return 0;
                                }
                            }
                        }
                    }
                } else if (RsCsDiff > 574.733) {
                    if (diffRsCsdiff <= 299.149) {
                        return 0;
                    } else if (diffRsCsdiff > 299.149) {
                        if (SC <= 39.857) {
                            if (MVDiff <= 325.304) {
                                if (Cs <= 11.102) {
                                    return 0;
                                } else if (Cs > 11.102) {
                                    return 1;
                                }
                            } else if (MVDiff > 325.304) {
                                return 0;
                            }
                        } else if (SC > 39.857) {
                            if (TSC <= 24.941) {
                                return 0;
                            } else if (TSC > 24.941) {
                                if (diffRsCsdiff <= 635.748) {
                                    return 0;
                                } else if (diffRsCsdiff > 635.748) {
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        } else if (diffMVdiffVal > 100.429) {
            if (diffAFD <= 9.929) {
                if (diffAFD <= 7.126) {
                    if (diffRsCsdiff <= 96.456) {
                        return 0;
                    } else if (diffRsCsdiff > 96.456) {
                        if (Rs <= 24.984) {
                            if (diffTSC <= -0.375) {
                                if (ssDCval <= 41.813) {
                                    return 1;
                                } else if (ssDCval > 41.813) {
                                    return 0;
                                }
                            } else if (diffTSC > -0.375) {
                                return 1;
                            }
                        } else if (Rs > 24.984) {
                            return 0;
                        }
                    }
                } else if (diffAFD > 7.126) {
                    if (refDCval <= 62.036) {
                        if (posBalance <= 0.545) {
                            return 1;
                        } else if (posBalance > 0.545) {
                            return 0;
                        }
                    } else if (refDCval > 62.036) {
                        return 0;
                    }
                }
            } else if (diffAFD > 9.929) {
                if (negBalance <= 0.114) {
                    if (diffAFD <= 60.112) {
                        if (SC <= 21.607) {
                            if (diffMVdiffVal <= 170.955) {
                                return 0;
                            } else if (diffMVdiffVal > 170.955) {
                                return 1;
                            }
                        } else if (SC > 21.607) {
                            if (CsDiff <= 394.574) {
                                return 0;
                            } else if (CsDiff > 394.574) {
                                if (MVDiff <= 311.679) {
                                    return 0;
                                } else if (MVDiff > 311.679) {
                                    return 1;
                                }
                            }
                        }
                    } else if (diffAFD > 60.112) {
                        if (negBalance <= 0.021) {
                            if (Cs <= 25.091) {
                                if (ssDCval <= 161.319) {
                                    return 1;
                                } else if (ssDCval > 161.319) {
                                    return 0;
                                }
                            } else if (Cs > 25.091) {
                                return 0;
                            }
                        } else if (negBalance > 0.021) {
                            if (CsDiff <= 118.864) {
                                return 0;
                            } else if (CsDiff > 118.864) {
                                return 1;
                            }
                        }
                    }
                } else if (negBalance > 0.114) {
                    if (diffRsCsdiff <= 73.957) {
                        if (refDCval <= 52.698) {
                            return 1;
                        } else if (refDCval > 52.698) {
                            if (CsDiff <= 292.448) {
                                if (gchDC <= 17.615) {
                                    if (diffRsCsdiff <= 31.82) {
                                        return 0;
                                    } else if (diffRsCsdiff > 31.82) {
                                        if (TSCindex <= 8) {
                                            return 1;
                                        } else if (TSCindex > 8) {
                                            if (diffTSC <= 9.277) {
                                                return 0;
                                            } else if (diffTSC > 9.277) {
                                                return 1;
                                            }
                                        }
                                    }
                                } else if (gchDC > 17.615) {
                                    return 0;
                                }
                            } else if (CsDiff > 292.448) {
                                return 1;
                            }
                        }
                    } else if (diffRsCsdiff > 73.957) {
                        if (MVDiff <= 327.545) {
                            if (TSCindex <= 8) {
                                if (Scindex <= 2) {
                                    return 1;
                                } else if (Scindex > 2) {
                                    if (Cs <= 15.328) {
                                        if (gchDC <= 11.785) {
                                            return 1;
                                        } else if (gchDC > 11.785) {
                                            return 0;
                                        }
                                    } else if (Cs > 15.328) {
                                        return 0;
                                    }
                                }
                            } else if (TSCindex > 8) {
                                if (Scindex <= 3) {
                                    if (posBalance <= 0.355) {
                                        return 1;
                                    } else if (posBalance > 0.355) {
                                        if (Scindex <= 2) {
                                            return 1;
                                        } else if (Scindex > 2) {
                                            if (CsDiff <= 190.806) {
                                                if (AFD <= 30.279) {
                                                    return 1;
                                                } else if (AFD > 30.279) {
                                                    if (Rs <= 11.233) {
                                                        return 0;
                                                    } else if (Rs > 11.233) {
                                                        if (RsDiff <= 135.624) {
                                                            if (Rs <= 13.737) {
                                                                return 1;
                                                            } else if (Rs > 13.737) {
                                                                return 0;
                                                            }
                                                        } else if (RsDiff > 135.624) {
                                                            return 1;
                                                        }
                                                    }
                                                }
                                            } else if (CsDiff > 190.806) {
                                                return 1;
                                            }
                                        }
                                    }
                                } else if (Scindex > 3) {
                                    if (diffMVdiffVal <= 132.973) {
                                        if (Scindex <= 4) {
                                            return 1;
                                        } else if (Scindex > 4) {
                                            if (TSC <= 18.753) {
                                                return 0;
                                            } else if (TSC > 18.753) {
                                                return 1;
                                            }
                                        }
                                    } else if (diffMVdiffVal > 132.973) {
                                        return 1;
                                    }
                                }
                            }
                        } else if (MVDiff > 327.545) {
                            if (CsDiff <= 369.087) {
                                if (Rs <= 18.379) {
                                    if (diffMVdiffVal <= 267.732) {
                                        if (Cs <= 10.464) {
                                            return 1;
                                        } else if (Cs > 10.464) {
                                            return 0;
                                        }
                                    } else if (diffMVdiffVal > 267.732) {
                                        return 1;
                                    }
                                } else if (Rs > 18.379) {
                                    if (diffAFD <= 35.587) {
                                        return 0;
                                    } else if (diffAFD > 35.587) {
                                        if (CsDiff <= 190.153) {
                                            return 0;
                                        } else if (CsDiff > 190.153) {
                                            return 1;
                                        }
                                    }
                                }
                            } else if (CsDiff > 369.087) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
