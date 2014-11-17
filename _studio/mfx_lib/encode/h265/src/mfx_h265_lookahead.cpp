//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <numeric>
#include <limits.h>

//#include "mfx_h265_defs.h"
#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"

#include "algorithm" // ???
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"
#include "ippi.h"

namespace H265Enc {
// --------------------------------------------------------
// INTRA PREDICTION
// --------------------------------------------------------
template <typename PixType>
static
void GetPredPelsLuma(PixType* src, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, PixType* PredPel,
                      Ipp32s width, Ipp8u bitDepthLuma)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s i;
    
    dcval = (1 << (bitDepthLuma - 1));

    //src += y*srcPitch + x;

    if ((x | y) == 0) 
    {
        // Fill border with DC value
        for (i = 0; i <= 4*width; i++)
            PredPel[i] = dcval;
    }
    else if (x == 0)
    {
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
    }
    else if (y == 0)
    {
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
    }
    else
    {
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
    return satd;
}


template <typename PixType>
Ipp32s IntraPredSATD(PixType *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma)
{
    typedef typename GetHistogramType<PixType>::type HistType;
    PixType PredPel[4*2*64+1];
    __ALIGN32 PixType recPel[MAX_CU_SIZE * MAX_CU_SIZE];
    //__ALIGN32 HistType hist4[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];
    //__ALIGN32 HistType hist8[40 * MAX_CU_SIZE / 8 * MAX_CU_SIZE / 8];
    __ALIGN32 HistType hist8[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];

    pSrc += y*srcPitch + x;
    Ipp32s numblks = width >> 3;

    //if (!(x|y))
    //printf("\n %p %d %d %d %d %d \n", pSrc, srcPitch, x, y, roi.width, roi.height);


    GetPredPelsLuma(pSrc, srcPitch, x, y, roi, PredPel, width, bitDepthLuma);

    h265_PredictIntra_DC(PredPel, recPel, width, width, 1);
    Ipp32s bestSATD = H265_Calc_SATD(pSrc, srcPitch, recPel, width);

    h265_PredictIntra_Ver(PredPel, recPel, width, width, bitDepthLuma, 1);
    Ipp32s satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    h265_PredictIntra_Hor(PredPel, recPel, width, width, bitDepthLuma, 1);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    //h265_AnalyzeGradient(pSrc, srcPitch, hist4, hist8, width, width);
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

    Ipp32s diff = MIN(abs(angmode - INTRA_HOR), abs(angmode - INTRA_VER));

    bool isFilterNeeded = (diff > h265_filteredModes[h265_log2m2[width]]);
    if (!isFilterNeeded && diff > 0) {
        h265_PredictIntra_Ang(angmode, PredPel, recPel, width, width);
        satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
        if (satd < bestSATD)
            bestSATD = satd;
    }

    // planar - filtering required
    //if (video->csps->strong_intra_smoothing_enabled_flag && width == 32)
    //{
    //    Ipp32s threshold = 1 << (video->bitDepthLuma - 5);

    //    Ipp32s topLeft = PredPel[0];
    //    Ipp32s topRight = PredPel[2*width];
    //    Ipp32s midHor = PredPel[width];

    //    Ipp32s bottomLeft = PredPel[4*width];
    //    Ipp32s midVer = PredPel[3*width];

    //    bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold;
    //    bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

    //    if (bilinearLeft && bilinearAbove)
    //        h265_FilterPredictPels_Bilinear(PredPel, width, topLeft, bottomLeft, topRight);
    //    else
    //        h265_FilterPredictPels(PredPel, width);
    //} else
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
    //if (!(x|y))
        //printf("%d \n", bestSATD);


    return bestSATD;
}

template Ipp32s IntraPredSATD<Ipp8u>(Ipp8u *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma);

void DoIntraAnalysis(Task* task, bool useLowRes)
{
    Ipp32s width  = useLowRes ? task->m_frameOrigin->m_lowres.width  : task->m_frameOrigin->width;
    Ipp32s height = useLowRes ? task->m_frameOrigin->m_lowres.height : task->m_frameOrigin->height;
    Ipp8u* luma      = useLowRes ? task->m_frameOrigin->m_lowres.y : task->m_frameOrigin->y;
    Ipp32s pitch_luma_pix = useLowRes ? task->m_frameOrigin->m_lowres.pitch_luma_pix : task->m_frameOrigin->pitch_luma_pix;

    // analysis for blk 8x8
    const Ipp32s sizeBlk = 8;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {width, height};

    for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            task->m_frameOrigin->m_intraSatd[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, 8);
        }
    }
}


template <class PixType>
Ipp32s h265_SAD_MxN_general_1(const PixType *image, Ipp32s stride_img, const PixType *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY);

template <>
Ipp32s h265_SAD_MxN_general_1<Ipp8u>(const Ipp8u *image, Ipp32s stride_img, const Ipp8u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
{
    return MFX_HEVC_PP::h265_SAD_MxN_general_8u(ref, stride_ref, image, stride_img, SizeX, SizeY);
}

//template <>
//Ipp32s h265_SAD_MxN_general<Ipp16u>(const Ipp16u *image, Ipp32s stride_img, const Ipp16u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
//{
//    return MFX_HEVC_PP::NAME(h265_SAD_MxN_general_16s)((Ipp16s*)ref, stride_ref, (Ipp16s*)image, stride_img, SizeX, SizeY);
//}


template <typename PixType>
Ipp32s tuHad_Kernel_1(const PixType *src, Ipp32s pitchSrc, const PixType *rec, Ipp32s pitchRec,
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

Ipp32s tuHad_1(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *rec, Ipp32s pitchRec, Ipp32s width, Ipp32s height)
{
    return tuHad_Kernel_1<Ipp8u>(src, pitchSrc, rec, pitchRec, width, height);
}


template <typename PixType, typename Frame>
Ipp32s MatchingMetricPu(const PixType *src, Ipp32s srcPitch, Frame *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY)
{
    Ipp32s refOffset = pelX + meInfo->posx + (mv->mvx >> 2) + (pelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;

    if(useHadamard)
        return (tuHad_1(src, srcPitch, rec, pitchRec, 8, 8) << 2);
    else
        return h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, 8, 8);
}

template 
Ipp32s MatchingMetricPu<Ipp8u>(const Ipp8u *src, Ipp32s srcPitch, const H265Frame *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY);
//template 
//Ipp32s MatchingMetricPu<Ipp16s>(const Ipp16s *src, Ipp32s srcPitch, const H265Frame *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY);

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
void MeIntPelLog(/*const */Frame *curr, /*const*/ Frame *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, /*const MvPredInfo<2> *predInfo,*/ H265MV *mv, Ipp32s *cost, Ipp32s *mvCost)
{
    // Log2( step ) is used, except meStepMax
    Ipp16s meStepBest = 2;
    //Ipp16s meStepMax = MAX(MIN(meInfo->width, meInfo->height), 16) * 4;
    Ipp16s meStepMax = MAX(MIN(8, 8), 16) * 4;
    H265MV mvBest = {0, 0};//*mv;
    Ipp32s costBest = INT_MAX;//*cost;
    Ipp32s mvCostBest = INT_MAX;//*mvCost;
    
    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    PixType *src = m_ySrc + posx + posy * curr->pitch_luma_pix;

    Ipp32s useHadamard = 0;//(m_par->hadamardMe == 3);
    Ipp16s mePosBest = 0;

    // limits to clip MV allover the CU
    const Ipp32s MvShift = 2;
    const Ipp32s offset = 8;
    const Ipp32s blkSize = 8;
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

    // recalc hadamar
    H265MEInfo meInfo;
    meInfo.posx = (Ipp8u)posx;
    meInfo.posy = (Ipp8u)posy;
    useHadamard = 1;
    *cost = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, mv, useHadamard, pelX, pelY);
} //

template
void MeIntPelLog<Ipp8u>(/*const */H265Frame *curr, /*const*/ H265Frame *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, /*const MvPredInfo<2> *predInfo,*/ H265MV *mv, Ipp32s *cost, Ipp32s *mvCost);

void DoInterAnalysis(Task* curr, Task* ref, bool useLowRes)
{
    // analysis for blk 8x8.
    Ipp32s width  = useLowRes ? curr->m_frameOrigin->m_lowres.width  : curr->m_frameOrigin->width;
    Ipp32s height = useLowRes ? curr->m_frameOrigin->m_lowres.height : curr->m_frameOrigin->height;
    const Ipp32s sizeBlk = 8;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            
            H265MV mv;
            Ipp32s cost = 0;
            Ipp32s mvCost = 0;
            (useLowRes ) 
                ? MeIntPelLog<Ipp8u>(&(curr->m_frameOrigin->m_lowres), &(ref->m_frameOrigin->m_lowres), x, y, 0, 0, /*const MvPredInfo<2> *predInfo, */ &mv, &cost, &mvCost)
                : MeIntPelLog<Ipp8u>(curr->m_frameOrigin, ref->m_frameOrigin, x, y, 0, 0, /*const MvPredInfo<2> *predInfo, */ &mv, &cost, &mvCost);

            curr->m_frameOrigin->m_interSatd[fPos] = cost;
        }
    }
}

} // namespace

TaskIter  findOldestToLookAheadProcess(std::list<Task*> & queue)
{
    if ( queue.empty() ) 
        return queue.end();

    for (TaskIter it = queue.begin(); it != queue.end(); it++ ) {

        if ( !(*it)->m_frameOrigin->wasLAProcessed() ) {
            return it;
        }
    }

    return queue.end();
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


template <class PixType, typename Frame>
Ipp64s Scale(H265Frame *in, Frame *out, Ipp32s planeIdx)
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

    //if (planeIdx > 0) {
    //    pixPrev = (const PixType *)prev->uv;
    //    pixCurr = (const PixType *)curr->uv;
    //    pitchPrev = prev->pitch_chroma_pix;
    //    pitchCurr = curr->pitch_chroma_pix;
    //    height /= 2; // TODO: YUV420
    //}

    /*Ipp64s diff = 0;
    for (Ipp32s y = 0; y < height; y++, pixPrev += pitchPrev, pixCurr += pitchCurr) {
        for (Ipp32s x = 0; x < width; x++) {
            diff += abs(pixPrev[x] - pixCurr[x]);
        }
    }*/

    for ( Ipp32s y = 0; y < heightOut; y++ ) {
        for (Ipp32s x = 0; x < widthOut; x++) {
            pixOut[ y*pitchOut + x] = AvgPixBlk( pixIn, pitchIn, x, y, step);
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
            diff += abs(pixPrev[x] - pixCurr[x]);
        }
    }

    return diff;
}

template <class PixType>
void BuildHistLuma(const PixType *pix, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s bitDepth, Ipp64s *hist)
{
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (Ipp32s y = 0; y < height; y++, pix += pitch)
        for (Ipp32s x = 0; x < width; x++)
            hist[pix[x]]++;
}

template <class PixType>
void BuildHistChroma(const PixType *pix, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s bitDepth, Ipp64s *hist)
{
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (Ipp32s y = 0; y < height; y++, pix += pitch)
        for (Ipp32s x = 0; x < width * 2; x += 2)
            hist[pix[x]]++;
}

template <class PixType, class Frame>
Ipp32s CalcHistDiff(Frame *prev, Frame *curr, Ipp32s planeIdx)
{
    const Ipp32s maxNumBins = 1 << (sizeof(PixType) == 1 ? 8 : 10);
    Ipp64s histPrev[maxNumBins];
    Ipp64s histCurr[maxNumBins];
    Ipp32s bitDepth = prev->m_bitDepthLuma;

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
    else {
        const PixType *pixPrev = (const PixType *)prev->uv + (planeIdx - 1);
        const PixType *pixCurr = (const PixType *)curr->uv + (planeIdx - 1);
        Ipp32s width = prev->width / 2; // TODO: YUV422, YUV444
        Ipp32s height = prev->height / 2; // TODO: YUV422, YUV444
        Ipp32s pitchPrev = prev->pitch_chroma_pix;
        Ipp32s pitchCurr = curr->pitch_chroma_pix;
        bitDepth = prev->m_bitDepthChroma;

        BuildHistChroma<PixType>(pixPrev, pitchPrev, width, height, bitDepth, histPrev);
        BuildHistChroma<PixType>(pixCurr, pitchCurr, width, height, bitDepth, histCurr);
    }
    
    Ipp32s numBins = 1 << bitDepth;
    Ipp32s diff = 0;
    for (Ipp32s i = 0; i < numBins; i++)
        diff += abs(histPrev[i] - histCurr[i]);

    return diff;
}


void MFXVideoENCODEH265::LookAheadAnalysis()
{
    TaskIter it = findOldestToLookAheadProcess(m_inputQueue);
    TaskIter prevIt = it;
    if (it != m_inputQueue.begin() ) {
        --prevIt;
    }

    if (it == m_inputQueue.end()) {
        if (!m_inputQueue.empty()) {
            DetectSceneChange_AndUpdateState();
        }

        return;
    }

    Task* curr = (*it);
    Task* prev = (it == m_inputQueue.begin()) ? NULL : (*prevIt);

    //-----------------------------------------------------
    // analysis #1: ME/Intra based
    //-----------------------------------------------------
    bool useLowResMe = false; // force analysis based on original resolution
    DoIntraAnalysis(curr, useLowResMe);
    if (prev) {
        DoInterAnalysis(curr, prev, useLowResMe);
    }

    Ipp32s resolution = (useLowResMe) 
        ? curr->m_frameOrigin->m_lowres.width * curr->m_frameOrigin->m_lowres.height 
        : curr->m_frameOrigin->width * curr->m_frameOrigin->height;

    Ipp64s sumSatd = 0;
    Ipp64s sumSatdIntra = 0;
    Ipp64s sumSatdInter = 0;
    if( prev ) {
        Ipp32s blkCount = (Ipp32s)curr->m_frameOrigin->m_intraSatd.size();
        Ipp32s intraBlkCount = 0;
        for (Ipp32s blk = 0; blk < blkCount; blk++) {
            sumSatd += IPP_MIN(curr->m_frameOrigin->m_intraSatd[blk], curr->m_frameOrigin->m_interSatd[blk]);
            if( curr->m_frameOrigin->m_intraSatd[blk] <= curr->m_frameOrigin->m_interSatd[blk] ) {
                intraBlkCount++;
            }

            sumSatdIntra += curr->m_frameOrigin->m_intraSatd[blk];
            sumSatdInter += curr->m_frameOrigin->m_interSatd[blk];
        }
        curr->m_frameOrigin->m_intraRatio = (Ipp32f)intraBlkCount / blkCount;

    } else {
        sumSatd =std::accumulate(curr->m_frameOrigin->m_intraSatd.begin(), curr->m_frameOrigin->m_intraSatd.end(), 0);
        curr->m_frameOrigin->m_intraRatio = 1.f;
        sumSatdIntra = sumSatd;
    }
    // store metric in frame
    curr->m_frameOrigin->m_avgBestSatd = (Ipp32f)sumSatd / (resolution);

    //-----------------------------------------------------
    // analysis #2: Pix Dif/Hist based
    //-----------------------------------------------------
    bool isLowRes = (m_videoParam.preEncMode > 1);
    const Ipp32s scaleFactor = m_scdConfig.scaleFactor;

    if (isLowRes) {
        Ipp32s widthOrig   = curr->m_frameOrigin->width;
        Ipp32s heightOrig  = curr->m_frameOrigin->height;

        curr->m_frameOrigin->m_lowres.width  = widthOrig >> (scaleFactor);
        curr->m_frameOrigin->m_lowres.height = heightOrig >> (scaleFactor);
        curr->m_frameOrigin->m_lowres.pitch_luma_pix = curr->m_frameOrigin->m_lowres.width;

        Scale<Ipp8u>(curr->m_frameOrigin, &curr->m_frameOrigin->m_lowres, 0);
    }

    if (prev) {
        Ipp64s lumaPixDiff;
        Ipp64s lumaHistDiff;
        
        lumaPixDiff = (isLowRes)
            ? CalcPixDiff<Ipp8u>(&prev->m_frameOrigin->m_lowres, &curr->m_frameOrigin->m_lowres, 0)
            : CalcPixDiff<Ipp8u>(prev->m_frameOrigin, curr->m_frameOrigin, 0);

        lumaHistDiff = (isLowRes)
            ? CalcHistDiff<Ipp8u>(&prev->m_frameOrigin->m_lowres, &curr->m_frameOrigin->m_lowres, 0)
            : CalcHistDiff<Ipp8u>(prev->m_frameOrigin, curr->m_frameOrigin, 0);

        // store metric in frame
        curr->m_frameOrigin->m_metric = (m_scdConfig.algorithm == ALG_HIST_DIFF) ? lumaHistDiff : lumaPixDiff;

        // store statistics 
        Ipp32s lastpos = m_slideWindowStat.size() - 1;
        m_slideWindowStat[ lastpos ].met = curr->m_frameOrigin->m_metric;
        m_slideWindowStat[ lastpos ].frameOrder = curr->m_frameOrigin->m_frameOrder;
    }

    // update
    curr->m_frameOrigin->setWasLAProcessed();

    DetectSceneChange_AndUpdateState();

} // 

struct isEqual
{
    isEqual(Ipp32s frameOrder): m_frameOrder(frameOrder)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_frameOrigin->m_frameOrder == m_frameOrder;
    }

    Ipp32s m_frameOrder;
};

bool MetricIsGreater(const StatItem &l, const StatItem &r) { return (l.met > r.met); }

void MFXVideoENCODEH265::DetectSceneChange_AndUpdateState( void )
{
    Ipp32s frameOrderCentral = m_slideWindowStat[m_scdConfig.M].frameOrder;
    bool doAnalysis = (frameOrderCentral >= 0);

    StatItem peaks[2] = {0};
    if (doAnalysis) {
        std::partial_sort_copy(m_slideWindowStat.begin(), m_slideWindowStat.end(), peaks, peaks+2, MetricIsGreater);

        TaskIter it = std::find_if(m_inputQueue.begin(), m_inputQueue.end(), isEqual(frameOrderCentral));

        bool scd = ((*it)->m_frameOrigin->m_metric == peaks[0].met) && (peaks[0].met > m_scdConfig.N*peaks[1].met);
        if (scd && !(*it)->m_frameOrigin->m_isIdrPic) {

            H265Frame* frame = (*it)->m_frameOrigin;

            // restore global state
            m_frameOrder = frame->m_frameOrder;
            m_frameOrderOfLastIdr = frame->m_frameOrderOfLastIdr;
            m_frameOrderOfLastIntra = frame->m_frameOrderOfLastIntra;
            m_frameOrderOfLastAnchor = frame->m_frameOrderOfLastAnchor;
            m_miniGopCount = frame->m_miniGopCount - !(frame->m_picCodeType == MFX_FRAMETYPE_B);

            // light configure
            frame->m_isIdrPic = true;
            frame->m_picCodeType = MFX_FRAMETYPE_I;
            frame->m_poc = 0;
            
            UpdateGopCounters(frame);

            frame->m_sceneCut = 1;

            // we need update all frames in inputQueue after the frame to propogate SCD (frame type change) effect
            it++;
            for (TaskIter frmIt = it; frmIt != m_inputQueue.end(); frmIt++) {
                frame = (*frmIt)->m_frameOrigin;
                ConfigureInputFrame(frame);
                UpdateGopCounters(frame);
            }
        }

        // move processed frame and frames before to lookaheadQueue 
        it = std::find_if(m_inputQueue.begin(), m_inputQueue.end(), isEqual(frameOrderCentral));
        it++;
        m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin(), it);
    }

    // update statictics
    std::copy(m_slideWindowStat.begin()+1, m_slideWindowStat.end(), m_slideWindowStat.begin());

    // force last element to _zero_ to prevent issue with EndOfStream
    m_slideWindowStat[m_slideWindowStat.size()-1].met = 0;
    m_slideWindowStat[m_slideWindowStat.size()-1].frameOrder = -1;//invalid

} // 

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
