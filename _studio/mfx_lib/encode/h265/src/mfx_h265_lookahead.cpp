//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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
                   Ipp32s srcPitch, PixType *dst, Ipp32s dstPitch, Ipp32s isFast, Ipp32s pelX, Ipp32s pelY)
{
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
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32, bitDepth, isFast);
    } else if (dx == 0) {
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32, bitDepth, isFast);
    } else {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

        if (isFast)
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 1 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 3, bitDepth - 8, 0, bitDepth, isFast);
        else
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 7, bitDepth - 8, 0, bitDepth, isFast);

        Ipp32s shift  = 20 - bitDepth;
        Ipp16s offset = 1 << (shift - 1);

        if (isFast)
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 1 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
        else
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
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

    Ipp32s isFast = 1;

    if ((mv->mvx | mv->mvy) & 3) {
        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, isFast, pelX, pelY);
        rec = predBuf;
        pitchRec = MAX_CU_SIZE;

        if(useHadamard)
            return (tuHad_Kernel(src, srcPitch, rec, pitchRec, 8, 8));
        else
            return h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, 8, 8);

    } else {
        if(useHadamard)
            return (tuHad_Kernel(src, srcPitch, rec, pitchRec, 8, 8));
        else
            return h265_SAD_MxN_general_1(src, srcPitch, rec, pitchRec, 8, 8);
    }
}

template 
    Ipp32s MatchingMetricPu<Ipp8u>(const Ipp8u *src, Ipp32s srcPitch, FrameData *refPic, const H265MEInfo *meInfo, const H265MV *mv, Ipp32s useHadamard, Ipp32s pelX, Ipp32s pelY);


template <typename PixType>
Ipp32s IntraPredSATD(PixType *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma)
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
    Ipp32s diff = MIN(abs(angmode - INTRA_HOR), abs(angmode - INTRA_VER));
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

template Ipp32s IntraPredSATD<Ipp8u>(Ipp8u *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma);

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
void DoIntraAnalysis_OneRow(FrameData* frame, Ipp32s* intraCost, Ipp32s row)
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

    //for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++)
    Ipp32s blk_row = row;
    {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            intraCost[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, bitDepthLuma);
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

#if 1
    if(cost_satd) {
        // recalc hadamar
        H265MEInfo meInfo;
        meInfo.posx = (Ipp8u)posx;
        meInfo.posy = (Ipp8u)posy;
        useHadamard = 1;
        *cost_satd = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, mv, useHadamard, pelX, pelY);
    }
#endif
} //


template <typename PixType>
void MeIntPelLog_Refine(FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost)
{
    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    PixType *src = m_ySrc + posx + posy * curr->pitch_luma_pix;
    Ipp32s refOffset = pelX + posx + (mv->mvx >> 2) + (pelY + posy + (mv->mvy >> 2)) * ref->pitch_luma_pix;
    const PixType *rec = (PixType*)ref->y + refOffset;
    Ipp32s pitchRec = ref->pitch_luma_pix;
    Ipp32s costR = h265_SAD_MxN_general_1(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, 8, 8);
    *cost = costR;
    *mvCost = 0;
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
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth)
{
    Interpolate<LUMA>(INTERP_HOR, src, pitchSrc, dst, pitchDst, dx, width, height, shift, offset, bitDepth, 1);//isFast
}

template <typename TSrc, typename TDst>
void InterpVer(const TSrc *src, Ipp32s pitchSrc, TDst *dst, Ipp32s pitchDst, Ipp32s dy,
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth)
{
    Interpolate<LUMA>(INTERP_VER, src, pitchSrc, dst, pitchDst, dy, width, height, shift, offset, bitDepth, 1);//isFast
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

    // intermediate halfpels
    InterpHor(refY - 1 - 4 * pitchRef, pitchRef, tmpPels, pitchTmp2, 2, (w + 8) & ~7, h + 8, bitDepth - 8, 0, bitDepth); // (intermediate)
    Ipp16s *tmpHor2 = tmpPels + 3 * pitchTmp2;

    // interpolate horizontal halfpels
    h265_InterpLumaPack(tmpHor2 + pitchTmp2, pitchTmp2, subpel, pitchHpel, w + 1, h, bitDepth);
    costs[3] = costFunc(src, pitchSrc, subpel + 1, pitchHpel, w, h) >> costShift;
    costs[7] = costFunc(src, pitchSrc, subpel,     pitchHpel, w, h) >> costShift;

    // interpolate diagonal halfpels
    InterpVer(tmpHor2, pitchTmp2, subpel, pitchHpel, 2, (w + 8) & ~7, h + 2, shift, offset, bitDepth);
    costs[0] = costFunc(src, pitchSrc, subpel,                 pitchHpel, w, h) >> costShift;
    costs[2] = costFunc(src, pitchSrc, subpel + 1,             pitchHpel, w, h) >> costShift;
    costs[4] = costFunc(src, pitchSrc, subpel + 1 + pitchHpel, pitchHpel, w, h) >> costShift;
    costs[6] = costFunc(src, pitchSrc, subpel     + pitchHpel, pitchHpel, w, h) >> costShift;

    // interpolate vertical halfpels
    pitchHpel = (w + 15) & ~15;
    InterpVer(refY - pitchRef, pitchRef, subpel, pitchHpel, 2, w, h + 2, 6, 32, bitDepth);
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
#ifdef AMT_DQP_FIX
    if(bQPel)
#endif
    {
        Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
        Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
        Ipp32s dx = hpelx & 3; // can be 0 or 2
        Ipp32s dy = hpely & 3; // can be 0 or 2

        Ipp32s pitchQpel = (w + 15) & ~15;

        // interpolate vertical quater-pels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, 3 - dy, w, h, 6, 32, bitDepth);                             // hpx+0 hpy-1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely - 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx+0 hpy-1/4
        costs[1] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate vertical qpels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, dy + 1, w, h, 6, 32, bitDepth);                             // hpx+0 hpy+1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely + 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx+0 hpy+1/4
        costs[5] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // intermediate horizontal quater-pels (left of best half-pel)
        Ipp32s pitchTmp1 = (w + 15) & ~15;
        Ipp16s *tmpHor1 = tmpPels + 3 * pitchTmp1;
        InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, tmpPels, pitchTmp1, 3 - dx, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx-1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (left of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
            h265_InterpLumaPack(tmpHor1 + pitchTmp1, pitchTmp1, subpel, pitchHpel, w, h, bitDepth); // hpx-1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor1 + (hpely >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth); // hpx-1/4 hpy+0
        costs[7] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel)
        InterpVer(tmpHor1 + (hpely - 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx-1/4 hpy-1/4
        costs[0] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
        InterpVer(tmpHor1 + (hpely + 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx-1/4 hpy+1/4
        costs[6] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // intermediate horizontal quater-pels (right of best half-pel)
        Ipp32s pitchTmp3 = (w + 15) & ~15;
        Ipp16s *tmpHor3 = tmpPels + 3 * pitchTmp3;
        InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, tmpPels, pitchTmp3, dx + 1, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx+1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (right of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
            h265_InterpLumaPack(tmpHor3 + pitchTmp3, pitchTmp3, subpel, pitchHpel, w, h, bitDepth); // hpx+1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor3 + (hpely >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth); // hpx+1/4 hpy+0
        costs[3] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

        // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel)
        InterpVer(tmpHor3 + (hpely - 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx+1/4 hpy-1/4
        costs[2] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
        InterpVer(tmpHor3 + (hpely + 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx+1/4 hpy+1/4
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
    void MeIntPelLog_Refine<Ipp8u>(FrameData *curr, FrameData *ref, Ipp32s pelX, Ipp32s pelY, Ipp32s posx, Ipp32s posy, H265MV *mv, Ipp32s *cost, Ipp32s *cost_satd, Ipp32s *mvCost);

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
                    const Ipp32s blkSize = 8;
                    Ipp32s HorMax = (width + offset - pelX - 1) << MvShift;
                    Ipp32s HorMin = ( - (Ipp32s)blkSize - offset - (Ipp32s) pelX + 1) << MvShift;
                    Ipp32s VerMax = (curr->height + offset - pelY - 1) << MvShift;
                    Ipp32s VerMin = ( - (Ipp32s) blkSize - offset - (Ipp32s) pelY + 1) << MvShift;

                    ClipMV(mv, HorMin, HorMax, VerMin, VerMax);
                }


                MeIntPelLog_Refine<Ipp8u>(curr, ref, x, y, 0, 0, &mv, &cost, &cost_satd, &mvCost);

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


void DoInterAnalysis_OneRow(FrameData* curr, Statistics* currStat, FrameData* ref, Ipp32s* sad, Ipp32s* satd, H265MV* out_mv, Ipp8u isRefine, Ipp8u mode, Ipp32s lowresFactor, Ipp8u bitDepthLuma, Ipp32s row) 
{
    // analysis for blk 8x8.
    Ipp32s width  = curr->width;
    Ipp32s height = curr->height;
    const Ipp32s sizeBlk = SIZE_BLK_LA;
    const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    Ipp32s bitDepth8 = (bitDepthLuma == 8) ? 1 : 0;

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
                const Ipp32s blkSize = 8;
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
                cost = h265_SAD_MxN_general_1(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, 8, 8);
                mvCost = 0;
            }
            else
                MeIntPelLog_Refine<Ipp16u>(curr, ref, x, y, 0, 0, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost);

        } else {

            if (bitDepth8)
                MeIntPelLog<Ipp8u>(curr, ref, x, y, 0, 0, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost);
            else
                MeIntPelLog<Ipp16u>(curr, ref, x, y, 0, 0, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost);
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
Ipp64s Scale(FrameData* in, FrameData* out, Ipp32s planeIdx)
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
            diff += abs(pixPrev[x] - pixCurr[x]);
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


template <class PixType, class ResultType>
void h265_ComputeRsCs1(const PixType *ySrc, Ipp32s pitchSrc, ResultType *lcuRs, ResultType *lcuCs, Ipp32s widthCu)
{
    Ipp32s bP = MAX_CU_SIZE>>2;
    for (Ipp32s i=0; i<widthCu; i+=4) {
        for(Ipp32s j=0; j<widthCu; j+=4) {
            ResultType Rs=0;
            ResultType Cs=0;
            for(Ipp32s k=0; k<4; k++) {
                for(Ipp32s l=0; l<4; l++) {
                    ResultType temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k)*pitchSrc+(j+l-1)];
                    Cs += temp*temp;

                    temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k-1)*pitchSrc+(j+l)];
                    Rs += temp*temp;
                }
            }
            lcuCs[(i>>2)*bP+(j>>2)] = Cs / 16;
            lcuRs[(i>>2)*bP+(j>>2)] = Rs / 16;
        }
    }
}


template <class PixType>
void CalcFrameRsCs(FrameData* data, Statistics* stat)
{
    Ipp32s        i,j;
    Ipp32s        locx, locy;
    Ipp32s        hblocks, wblocks;

    const Ipp32s BLOCK_SIZE = 4;
    const Ipp32s RsCsSIZE = BLOCK_SIZE*BLOCK_SIZE;

    //FrameData* data = frame->m_origin; // important!!! calq on original resolution only!!!
    hblocks  = (Ipp32s)data->height / BLOCK_SIZE;
    wblocks  = (Ipp32s)data->width / BLOCK_SIZE;    
    //Statistics* stat = frame->m_stats[0];
    stat->m_frameCs                =    0.0;
    stat->m_frameRs                =    0.0;

    Ipp32s pitch = data->pitch_luma_pix;

    Ipp32s rs[(MAX_CU_SIZE/BLOCK_SIZE)*(MAX_CU_SIZE/BLOCK_SIZE)]={0}, cs[(MAX_CU_SIZE/BLOCK_SIZE)*(MAX_CU_SIZE/BLOCK_SIZE)]={0};
    for (i=0;i<hblocks;i+=2) {
        locy = i * wblocks;
        for (j=0;j<wblocks;j+=2) {
            PixType* ySrc = (PixType*)data->y + (i*BLOCK_SIZE)*pitch + (j*BLOCK_SIZE);
            h265_ComputeRsCs(ySrc, pitch, rs, cs, BLOCK_SIZE*2, BLOCK_SIZE*2);
            for(int k=0; k<2; k++) {
                if(i+k>=hblocks) continue;
                for(int l=0; l<2; l++) {
                    if(j+l>=wblocks) continue;
                    locx = locy+k*wblocks+j+l;
                    stat->m_rs[locx] = rs[k*(MAX_CU_SIZE/BLOCK_SIZE)+l];
                    stat->m_cs[locx] = cs[k*(MAX_CU_SIZE/BLOCK_SIZE)+l];
                    stat->m_frameCs += stat->m_cs[locx];
                    stat->m_frameRs += stat->m_rs[locx];
                }
            }
        }
    }

    stat->m_frameCs                /=    hblocks * wblocks;
    stat->m_frameRs                /=    hblocks * wblocks;
    stat->m_frameCs                =    sqrt(stat->m_frameCs);
    stat->m_frameRs                =    sqrt(stat->m_frameRs);
}

enum {
    BLOCK_SIZE = 4
};

// CalcRsCs_OneRow()  works with blk_8x8. (SIZE_BLK_LA)
template <class PixType>
void CalcRsCs_OneRow(FrameData* data, Statistics* stat, H265VideoParam& par, Ipp32s row)
{
    Ipp32s        i,j;
    Ipp32s        locx, locy;
    Ipp32s        hblocks, wblocks;

    //const Ipp32s BLOCK_SIZE = 4;
    const Ipp32s RsCsSIZE = BLOCK_SIZE*BLOCK_SIZE;
    
    hblocks  = (Ipp32s)data->height / BLOCK_SIZE;
    wblocks  = (Ipp32s)data->width / BLOCK_SIZE;
    
    stat->m_frameCs                =    0.0;
    stat->m_frameRs                =    0.0;

    Ipp32s pitch = data->pitch_luma_pix;

    Ipp32s rs[(MAX_CU_SIZE/BLOCK_SIZE)*(MAX_CU_SIZE/BLOCK_SIZE)]={0}, cs[(MAX_CU_SIZE/BLOCK_SIZE)*(MAX_CU_SIZE/BLOCK_SIZE)]={0};

    //for (Ipp32s row = 0; row < par.PicHeightInCtbs; row++) 
    {
        
        Ipp32s iStart = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * row;
        Ipp32s iEnd  = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * (row + 1);
        iEnd = IPP_MIN(iEnd, hblocks);

        //for (i=0;i<hblocks;i+=2) {
        for (i=iStart;i<iEnd;i+=2) {
            locy = i * wblocks;

            for (j=0;j<wblocks;j+=2) {
                PixType* ySrc = (PixType*)data->y + (i*BLOCK_SIZE)*pitch + (j*BLOCK_SIZE);
                h265_ComputeRsCs(ySrc, pitch, rs, cs, BLOCK_SIZE*2, BLOCK_SIZE*2);

                for(int k=0; k<2; k++) {
                    if(i+k>=hblocks) {
                        continue;
                    }

                    for(int l=0; l<2; l++) {
                        if(j+l>=wblocks) {
                            continue;
                        }

                        locx = locy+k*wblocks+j+l;
                        stat->m_rs[locx] = rs[k*(MAX_CU_SIZE/BLOCK_SIZE)+l];
                        stat->m_cs[locx] = cs[k*(MAX_CU_SIZE/BLOCK_SIZE)+l];
                    }
                }
            }
        }
    }
}

template <class PixType>
void CalcFrameRsCsByRow(FrameData* data, Statistics* stat, H265VideoParam& par)
{
    for (Ipp32s row = 0; row < par.PicHeightInCtbs; row++) {
        CalcRsCs_OneRow<PixType>(data, stat, par, row);
    }

    stat->m_frameCs = std::accumulate(stat->m_cs.begin(), stat->m_cs.end(), 0);
    stat->m_frameRs = std::accumulate(stat->m_rs.begin(), stat->m_rs.end(), 0);

    Ipp32s hblocks  = (Ipp32s)data->height / BLOCK_SIZE;
    Ipp32s wblocks  = (Ipp32s)data->width / BLOCK_SIZE;
    stat->m_frameCs                /=    hblocks * wblocks;
    stat->m_frameRs                /=    hblocks * wblocks;
    stat->m_frameCs                =    sqrt(stat->m_frameCs);
    stat->m_frameRs                =    sqrt(stat->m_frameRs);
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
    Frame* inFrame    = *curr;

    int widthInRegionGrid = inFrame->m_origin->width / 16;
    int wBlock = inFrame->m_origin->width/8;
    int w4 = inFrame->m_origin->width/4;

    const int CU_SIZE = videoParam.MaxCUSize;
    int heightInTiles = videoParam.PicHeightInCtbs;
    int widthInTiles  = videoParam.PicWidthInCtbs;

    const int MAX_TILE_SIZE = CU_SIZE;//64;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;

    Ipp64f tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    static const Ipp64f lmt_sc2[10] = {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    H265MV candMVs[(64/8)*(64/8)];

    Ipp64f futr_qp = MAX_DQP;

    Ipp32s c8_height = (inFrame->m_origin->height);
    Ipp32s c8_width  = (inFrame->m_origin->width);

    const Ipp32s statIdx = 0;// we use original stats here

    for (Ipp32s row=0;row<heightInTiles;row++) {
        for (Ipp32s col=0;col<widthInTiles;col++) {
            int i,j;

            N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            
            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;

            Ipp64f Rs=0.0, Cs=0.0;
            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += inFrame->m_stats[statIdx]->m_rs[(row*r4h+i)*w4+(col*r4w+j)];
                    Cs += inFrame->m_stats[statIdx]->m_cs[(row*r4h+i)*w4+(col*r4w+j)];
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);

            Ipp64f SC = sqrt(Rs + Cs);
            Ipp64f tsc_RTML = 0.6*sqrt(SC);
            Ipp64f tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414);
            Ipp64f tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414);

            int scVal = 0;
            inFrame->m_stats[statIdx]->rscs_ctb[row*widthInTiles+col] = SC;
            for (i = 0; i < 10; i++) {
                if (SC < lmt_sc2[i]*(Ipp64f)(1<<(inFrame->m_bitDepthLuma-8))) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->m_stats[statIdx]->sc_mask[row*widthInTiles+col] = scVal;
            

            FrameIter itStart = ++FrameIter(curr);
            int coloc=0;
            Ipp64f pdsad_futr=0.0;
            for (FrameIter it = itStart; it != end; it++) {
                Statistics* stat = (*it)->m_stats[statIdx];

                int uMVnum = 0;
                Ipp64f psad_futr = 0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_futr += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_futr/=tdiv;

                if (psad_futr < tsc_RTML && uMVnum<(mbT*nbT)/2) 
                    coloc++;
                else 
                    break;
            } 

            pdsad_futr=0.0;
            for(i=0;i<mbT;i++)
                for(j=0;j<nbT;j++)
                    pdsad_futr += inFrame->m_stats[statIdx]->m_interSad_pdist_future[(row*rbh+i)*(wBlock)+(col*rbw+j)];
            pdsad_futr/=tdiv;

            if (itStart == end) {
                pdsad_futr = tsc_RTS;
                coloc = 0;
            }
            
            inFrame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] = coloc;

            if(scVal<1 && pdsad_futr<tsc_RTML) {
                // Visual Quality (Flat area first, because coloc count doesn't work in flat areas)
                coloc = 1;
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*coloc;
            } else if(coloc>=8 && pdsad_futr<tsc_RTML) {
                // Stable Motion, Propagation & Motion reuse (long term stable hypothesis, 8+=>inf)
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*futr_qp));
            } else if(coloc>=8 && pdsad_futr<tsc_RTMG) {
                // Stable Motion, Propagation possible & Motion reuse 
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*4.0));
            } else if(coloc>1 && pdsad_futr<tsc_RTMG) {
                // Stable Motion & Motion reuse 
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((float)coloc/8.0)*4.0));
            } else if(scVal>=6 && pdsad_futr>tsc_RTS) {
                // Reduce disproportional cost on high texture and bad motion
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 1;
            } else {
                // Default
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0;
            }
        }
    }
}


void H265Enc::DetermineQpMap_PFrame(FrameIter begin, FrameIter curr, FrameIter end, H265VideoParam & videoParam)
{
    FrameIter itCurr = curr;
    Frame* inFrame    = *curr;
    Frame* past_frame = *begin;

    int row, col;
    int c8_width, c8_height;
    int widthInRegionGrid = inFrame->m_origin->width / 16;
    int wBlock = inFrame->m_origin->width/8;
    int w4 = inFrame->m_origin->width/4;

    const int CU_SIZE = videoParam.MaxCUSize;
    int heightInTiles = videoParam.PicHeightInCtbs;
    int widthInTiles  = videoParam.PicWidthInCtbs;

    const int MAX_TILE_SIZE = CU_SIZE;//64;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;

    Ipp64f tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    static Ipp64f lmt_sc2[10]   =   {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    H265MV candMVs[(64/8)*(64/8)];

    Ipp64f tsc_RTF, tsc_RTML, tsc_RTMG, tsc_RTS;
    const Ipp64f futr_qp = MAX_DQP;
    const Ipp32s REF_DIST = IPP_MIN(8, (inFrame->m_frameOrder - past_frame->m_frameOrder));

    const Ipp32s statIdx = 0;// we use original stats here

    c8_height = (inFrame->m_origin->height);
    c8_width  = (inFrame->m_origin->width);

    bool futr_key = false;
    for(row=0;row<heightInTiles;row++) {
        for(col=0;col<widthInTiles;col++) {
            int i, j;
            
            N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            
            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;

            Ipp64f Rs=0.0, Cs=0.0;
            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += inFrame->m_stats[statIdx]->m_rs[(row*r4h+i)*w4+(col*r4w+j)];
                    Cs += inFrame->m_stats[statIdx]->m_cs[(row*r4h+i)*w4+(col*r4w+j)];
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);
            Ipp64f SC = sqrt(Rs + Cs);
            inFrame->m_stats[statIdx]->rscs_ctb[row*widthInTiles+col] = SC;

            int scVal = 0;
            for(i = 0;i<10;i++) {
                if(SC < lmt_sc2[i]*(Ipp64f)(1<<(inFrame->m_bitDepthLuma-8))) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->m_stats[statIdx]->sc_mask[row*widthInTiles+col] = scVal;
            tsc_RTML = 0.6*sqrt(SC);

            tsc_RTF = tsc_RTML/2;
            tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414);
            tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414);


            // RTF RTS logic for P Frame is based on MC Error
            Ipp64f pdsad_past=0.0;
            Ipp64f pdsad_futr=0.0;
            for(i=0;i<mbT;i++) {
                for(j=0;j<nbT;j++) {
                    pdsad_past += inFrame->m_stats[statIdx]->m_interSad_pdist_past[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                    pdsad_futr += inFrame->m_stats[statIdx]->m_interSad_pdist_future[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                }
            }
            pdsad_past/=tdiv;
            pdsad_futr/=tdiv;


            // future (forward propagate)
            int coloc_futr=0;
            FrameIter itStart = ++FrameIter(itCurr);
            for (FrameIter it = itStart; it != end; it++) {
                int uMVnum = 0;
                Statistics* stat = (*it)->m_stats[statIdx];
                Ipp64f psad_futr=0.0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_futr += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_futr/=tdiv;
                if (psad_futr < tsc_RTML && uMVnum < (mbT*nbT)/2) 
                    coloc_futr++;
                else break;
            }


            // past (backward propagate)
            Ipp32s coloc_past=0;
            for (FrameIter it = itCurr; it != begin; it--) {
                Statistics* stat = (*it)->m_stats[statIdx];
                int uMVnum = 0; 
                Ipp64f psad_past=0.0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_past += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_past/=tdiv;
                if(psad_past<tsc_RTML && uMVnum<(mbT*nbT)/2) 
                    coloc_past++;
                else break;
            };


            inFrame->m_stats[statIdx]->coloc_past[row*widthInTiles+col] = coloc_past;
            inFrame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] = coloc_futr;
            Ipp32s coloc = IPP_MAX(coloc_past, coloc_futr);

            if(futr_key) coloc = coloc_past;

            if (coloc_past >= REF_DIST && past_frame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] >= REF_DIST) {
                // Stable Motion & P Skip (when GOP is small repeated P QP lowering can change RD Point)
                // Avoid quantization noise recoding
                //inFrame->qp_mask[row*widthInTiles+col] = IPP_MIN(0, past_frame->qp_mask[row*widthInTiles+col]+1);
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0; // Propagate
            } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTML && !futr_key) {
                // Stable Motion & Motion reuse 
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((Ipp32f)coloc/8.0)*futr_qp));
            } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                // Stable Motion & Motion reuse 
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((Ipp32f)coloc/8.0)*4.0));
            } else if(coloc>1 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                // Stable Motion & Motion Reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((Ipp32f)coloc/8.0)*4.0));
                // Possibly propagation
            } else if(coloc>1 && coloc==coloc_past && pdsad_past<tsc_RTMG) {
                // Stable Motion & Motion Reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((Ipp32f)coloc/8.0)*4.0));
                // Past Boost probably no propagation since coloc_futr is less than coloc_past
            }  else if(scVal>=6 && pdsad_past>tsc_RTS && coloc==0) {
                // reduce disproportional cost on high texture and bad motion
                // use pdsad_past since its coded a in order p frame
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 1;
            } else {
                // Default
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0;
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
            int val = inFrame->m_lcuQps[row*widthInTiles+col];
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


void H265Enc::DoPDistInterAnalysis_OneRow(Frame* curr, Frame* prevP, Frame* nextP, Ipp32s region_row, Ipp32s lowresRowsInRegion, Ipp32s originRowsInRegion, Ipp8u LowresFactor)
{
    FrameData* currFrame  = LowresFactor ? curr->m_lowres : curr->m_origin;
    Statistics* stat = curr->m_stats[LowresFactor ? 1 : 0];

    // compute (PDist = miniGopSize) Past ME
    if (prevP) {
        FrameData * prevFrame = LowresFactor ? prevP->m_lowres : prevP->m_origin;
        Ipp32s rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
        for (Ipp32s i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, prevFrame, &stat->m_interSad_pdist_past[0], NULL, &stat->m_mv_pdist_past[0], 0, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i);
        }
        if (LowresFactor) {
            FrameData* currFrame  = curr->m_origin;
            FrameData * prevFrame = prevP->m_origin;

            Ipp32s rowsInRegion = originRowsInRegion;
            Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
            for(Ipp32s i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, prevFrame, &curr->m_stats[0]->m_interSad_pdist_past[0], NULL, &curr->m_stats[0]->m_mv_pdist_past[0], 1, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion)+i); // refine
            }

        }
    }

    // compute (PDist = miniGopSize) Future ME
    if (nextP) {
        FrameData* nextFrame = LowresFactor ? nextP->m_lowres : nextP->m_origin;
        Ipp32s rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
        for (Ipp32s i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, nextFrame, &stat->m_interSad_pdist_future[0], NULL, &stat->m_mv_pdist_future[0], 0, 1, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i);
        }
        if (LowresFactor) {
            FrameData* currFrame = curr->m_origin;
            FrameData* nextFrame = nextP->m_origin;

            Ipp32s rowsInRegion = originRowsInRegion;
            Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
            for(Ipp32s i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, nextFrame, &curr->m_stats[0]->m_interSad_pdist_future[0], NULL, &curr->m_stats[0]->m_mv_pdist_future[0], 1, 1,LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i);//refine
            }
        }
    }
}


void H265Enc::BackPropagateAvgTsc(FrameIter prevRef, FrameIter currRef)
{
    Frame* input = *currRef;

    Ipp32u frameType = input->m_picCodeType;
    FrameData* origFrame  =  input->m_origin;
    Statistics* origStats = input->m_stats[0];

    origStats->avgTSC = 0.0;
    origStats->avgsqrSCpp = 0.0;
    Ipp32s gop_size = 1;
    Ipp64f avgTSC     = 0.0;
    Ipp64f avgsqrSCpp = 0.0;

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
    avgsqrSCpp/=sqrt((Ipp64f)(frameSize));
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
        tt[startIdx].Init(TT_PREENC_START, 0, 0, poc, 0, 0);
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
        for (FrameIter it = m_inputQueue.begin(); it != itEnd; it++) {
            AverageComplexity(*it, m_videoParam);
        }
    }

    m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin(), itEnd);
}

bool MetricIsGreater(const StatItem &l, const StatItem &r) { return (l.met > r.met); }


void Lookahead::DetectSceneCut(FrameIter begin, FrameIter end, /*FrameIter input*/ Frame* in, Ipp32s updateGop, Ipp32s updateState)
{
    enum {
        ALG_PIX_DIFF = 0,
        ALG_HIST_DIFF
    };

    struct ScdConfig {
        Ipp32s M; // window size = 2*M+1
        Ipp32s N; // if (peak1 / peak2 > N) => SC Detected!
        Ipp32s algorithm; // ALG_PIX_DIFF, ALG_HIST_DIFF
        Ipp32s scaleFactor; // analysis will be done on (origW >> scaleFactor, origH >> scaleFactor) resolution
    } m_scdConfig;

    m_scdConfig.M = 10;
    m_scdConfig.N = 3;
    m_scdConfig.algorithm = ALG_HIST_DIFF;

    // accept new frame
    if (in) {
        bool isLowres = true; // always on lowres
        Ipp32s lastpos = m_slideWindowStat.size() - 1;
        m_slideWindowStat[ lastpos ].frameOrder = in->m_frameOrder;
        Ipp64s metric = 0;
        if (in->m_stats[0])
            in->m_stats[0]->m_metric = metric; // to prevent any issue we sync both lowres and original stats
        if (in->m_stats[1])
            in->m_stats[1]->m_metric  = metric;


        if (in->m_frameOrder > 0) {
            FrameIter it = std::find_if(begin, end, isEqual(in->m_frameOrder-1));
            if (it != end) {
                Frame* prev = (*it);

                FrameData* currFrame = isLowres ? in->m_lowres : in->m_origin;
                FrameData* prevFrame = isLowres ? prev->m_lowres : prev->m_origin;

                Ipp32s bitDepth8 = (in->m_bitDepthLuma == 8) ? 1 : 0;

                if (m_scdConfig.algorithm == ALG_HIST_DIFF) {
                    metric = bitDepth8 
                        ? CalcHistDiff<Ipp8u>(prevFrame, currFrame, 0)
                        : CalcHistDiff<Ipp16u>(prevFrame, currFrame, 0);
                } else {
                    metric = bitDepth8 
                        ? CalcPixDiff<Ipp8u>(prevFrame, currFrame, 0)
                        : CalcPixDiff<Ipp16u>(prevFrame, currFrame, 0);
                }

                // store metric in frame
                if (in->m_stats[0])
                    in->m_stats[0]->m_metric =  metric;
                if (in->m_stats[1])
                    in->m_stats[1]->m_metric =  metric;
            }
        }
        // store statistics
        m_slideWindowStat[ lastpos ].met = metric;
    }


    // make decision
    Ipp32s centralPos = (m_slideWindowStat.size() - 1) >> 1;
    Ipp32s frameOrderCentral = m_slideWindowStat[centralPos].frameOrder;
    Ipp8u isBufferingEnough = (frameOrderCentral > 0); // skip first frame
    FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));
    
    if (isBufferingEnough && curr != end) {

        if ((*curr)->m_stats[0])
            (*curr)->m_stats[0]->m_sceneCut = 0;
        if ((*curr)->m_stats[1])
            (*curr)->m_stats[1]->m_sceneCut = 0;

        StatItem peaks[2];
        std::partial_sort_copy(m_slideWindowStat.begin(), m_slideWindowStat.end(), peaks, peaks+2, MetricIsGreater);
        Ipp64s metric = ( (*curr)->m_stats[1] ) ? (*curr)->m_stats[1]->m_metric : (*curr)->m_stats[0]->m_metric;
        Ipp8u scd = (metric == peaks[0].met) && (peaks[0].met > m_scdConfig.N*peaks[1].met);        

        if (scd) {
            if ((*curr)->m_stats[0])
                (*curr)->m_stats[0]->m_sceneCut = 1;
            if ((*curr)->m_stats[1])
                (*curr)->m_stats[1]->m_sceneCut = 1;
        }


        if (scd && !(*curr)->m_isIdrPic && updateGop) {
            Frame* frame = (*curr);

            // restore global state
            m_enc.RestoreGopCountersFromFrame(frame, !!m_videoParam.encodedOrder);

            // light configure
            frame->m_isIdrPic = true;
            frame->m_picCodeType = MFX_FRAMETYPE_I;
            frame->m_poc = 0;

            // special case: scenecut detected in second Field
            if (frame->m_secondFieldFlag) {
                FrameIter tmp = curr;
                Frame* firstField = *(--tmp);
                firstField->m_picCodeType = MFX_FRAMETYPE_P;
            }

            if (m_videoParam.PGopPicSize > 1) {
                //const Ipp8u PGOP_LAYERS[PGOP_PIC_SIZE] = { 0, 2, 1, 2 };
                frame->m_RPSIndex = 0;
                frame->m_pyramidLayer = 0;//PGOP_LAYERS[frame->m_RPSIndex];
            }

            m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);

            if (frame->m_stats[1])
                frame->m_stats[1]->m_sceneCut = 1;
            if (frame->m_stats[0])
                frame->m_stats[0]->m_sceneCut = 1;

            // we need update all frames in inputQueue after Idr insertion to propogate SceneCut (frame type change) effect
            for (FrameIter it = ++FrameIter(curr); it != end; it++) {
                frame = (*it);
                m_enc.ConfigureInputFrame(frame, !!m_videoParam.encodedOrder);
                m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);
            }
        }
    }


    if (updateState) {
        // Release resource counters && Update algorithm state
        if (isBufferingEnough && curr != end) {
            if (curr != begin) {
                (*(--FrameIter(curr)))->m_lookaheadRefCounter--;
            }
            Ipp8u isEos = (m_slideWindowStat[centralPos + 1].frameOrder == -1);
            if (isEos) {
                (*curr)->m_lookaheadRefCounter--;
            }
        }
        // update statictics
        std::copy(m_slideWindowStat.begin()+1, m_slideWindowStat.end(), m_slideWindowStat.begin());
        // force last element to _zero_ to prevent issue with EndOfStream
        m_slideWindowStat[m_slideWindowStat.size()-1].met = 0;
        m_slideWindowStat[m_slideWindowStat.size()-1].frameOrder = -1;//invalid
    }

} // 


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

void Lookahead::ResetState()
{
    if (m_videoParam.SceneCut)
        std::fill(m_slideWindowStat.begin(), m_slideWindowStat.end(), StatItem());
    
    //if (m_videoParam.DeltaQpMode) 
    //    std::fill(m_slideWindowPaq.begin(), m_slideWindowPaq.end(), -1);

    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;
}

void H265Enc::AverageComplexity(Frame *in, H265VideoParam& videoParam)
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

    if (in->m_frameOrder > 0 && in->m_picCodeType != MFX_FRAMETYPE_I) {
        Ipp32s blkCount = (Ipp32s)stat->m_intraSatd.size();

        for (Ipp32s blk = 0; blk < blkCount; blk++) {
            sumSatd += IPP_MIN(stat->m_intraSatd[blk], stat->m_interSatd[blk]);
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
        useLowres--;
        useLowres = Saturate(0, 1, useLowres);
        Ipp32f tabCorrFactor[] = {1.5f, 2.f};
        if (in->m_stats[0]) {
            in->m_stats[0]->m_avgBestSatd /= tabCorrFactor[useLowres];
            in->m_stats[0]->m_avgIntraSatd /= tabCorrFactor[useLowres];
            in->m_stats[0]->m_avgInterSatd /= tabCorrFactor[useLowres];
        } 
        if (in->m_stats[1]) {
            in->m_stats[1]->m_avgBestSatd /= tabCorrFactor[useLowres];
            in->m_stats[1]->m_avgIntraSatd /= tabCorrFactor[useLowres];
            in->m_stats[1]->m_avgInterSatd /= tabCorrFactor[useLowres];
        }
    }
}


void H265Enc::AverageRsCs(Frame *in)
{
    if (in == NULL)
        return;

    Statistics* stat = in->m_stats[0];
    FrameData* data = in->m_origin;
    stat->m_frameCs = std::accumulate(stat->m_cs.begin(), stat->m_cs.end(), 0);
    stat->m_frameRs = std::accumulate(stat->m_rs.begin(), stat->m_rs.end(), 0);

    Ipp32s hblocks  = (Ipp32s)data->height / BLOCK_SIZE;
    Ipp32s wblocks  = (Ipp32s)data->width / BLOCK_SIZE;
    stat->m_frameCs                /=    hblocks * wblocks;
    stat->m_frameRs                /=    hblocks * wblocks;
    stat->m_frameCs                =    sqrt(stat->m_frameCs);
    stat->m_frameRs                =    sqrt(stat->m_frameRs);

    Ipp64f RsGlobal = in->m_stats[0]->m_frameRs;
    Ipp64f CsGlobal = in->m_stats[0]->m_frameCs;
    Ipp32s frameSize = in->m_origin->width * in->m_origin->height;

    in->m_stats[0]->SC = sqrt(((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal))*frameSize);
    in->m_stats[0]->TSC = std::accumulate(in->m_stats[0]->m_interSad.begin(), in->m_stats[0]->m_interSad.end(), 0);
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

    //return (in || isBufferingEnough) ? 1 : 0;
    return 0;
} // 


//Ipp32s Lookahead::GetDelay()
//{
//    Ipp32s delayScd   = m_videoParam.SceneCut ? m_scdConfig.M + 1 + 1: 0; // algorithm specific
//    Ipp32s delayCmplx = m_videoParam.AnalyzeCmplx ? m_videoParam.RateControlDepth : 0;
//    Ipp32s delayPaq   = (m_videoParam.DeltaQpMode&(AMT_DQP_CAL|AMT_DQP_PAQ)) ? 2*m_videoParam.GopRefDist + 1 : 0;
//    Ipp32s lookaheadBuffering = delayScd + delayCmplx + delayPaq + 1; // +1 due to arch issue
//
//    return lookaheadBuffering;
//}


void H265Enc::GetLookaheadGranularity(const H265VideoParam& videoParam, Ipp32s & regionCount, Ipp32s & lowRowsInRegion, Ipp32s & originRowsInRegion, Ipp32s & numTasks)
{
    Ipp32s lowresHeightInBlk = ((videoParam.Height >> videoParam.LowresFactor) + (SIZE_BLK_LA-1)) >> 3;
    Ipp32s originHeightInBlk = ((videoParam.Height) + (SIZE_BLK_LA-1)) >> 3;

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

    //m_threadingTasks.resize( numTasks );
}


Lookahead::Lookahead(H265Encoder & enc)
    : m_inputQueue(enc.m_inputQueue)
    , m_videoParam(enc.m_videoParam)
    , m_enc(enc)
{
    m_bufferingPaq = 0;

    // configuration of lookahead algorithm family
    // SceneCut
    Ipp32s bufferingSceneCut = 0;
    if (m_videoParam.SceneCut > 0) {
        bufferingSceneCut = 10;
        Ipp32s N = 3;
        //m_scdConfig.algorithm = ALG_HIST_DIFF;
        //m_scdConfig.scaleFactor = m_videoParam.LowresFactor ? m_videoParam.LowresFactor : 1;
        
        Ipp32s windowSize = 2*/*m_scdConfig.*/bufferingSceneCut + 1;
        m_slideWindowStat.resize( windowSize, StatItem());
    }

    // Content Analysis configuration
    if (m_videoParam.DeltaQpMode)
    {
        Ipp32s M = 0;
        if (m_videoParam.DeltaQpMode&(AMT_DQP_CAL|AMT_DQP_PAQ))
            M = m_videoParam.GopRefDist;
        if (m_videoParam.SceneCut) {
            M = IPP_MAX(M, m_videoParam.GopRefDist + bufferingSceneCut);
        }
        //Ipp32s windowSize = 2*M + 1;
       // m_slideWindowPaq.resize( windowSize, -1 );
        m_bufferingPaq = M;

        // reset lowres (tmp solution)
        //m_videoParam.LowresFactor = 0;
        if (m_videoParam.SceneCut) {
            //m_scdConfig.scaleFactor = 1;
        }
    }

    // to prevent multiple PREENC_START per frame
    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;

    Ipp32s numTasks;
    GetLookaheadGranularity(m_videoParam, m_regionCount, m_lowresRowsInRegion, m_originRowsInRegion, numTasks);

    //m_ttLookahead.resize( numTasks );
    
    //Build_ttGraph(0);
}


Lookahead::~Lookahead()
{
    m_slideWindowStat.resize(0);
    //m_slideWindowPaq.resize(0);
}


//  pure functions
template <typename PixType>
double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride)
{
    int        i, j;
    int        tmpVal, CsVal, RsVal;
    int        blkSizeC = (width-1) * height;
    int        blkSizeR = width * (height-1);
    double    RS, CS, RsCs;
    PixType    *pBuf;

    RS = CS = RsCs = 0.0;
    CsVal =    0;
    RsVal =    0;

    // CS
    CsVal =    0;
    pBuf = pSrcBuf;
    for(i = 0; i < height; i++) {
        for(j = 1; j < width; j++) {
            tmpVal = pBuf[j] - pBuf[j-1];
            CsVal += tmpVal * tmpVal;
        }
        pBuf += stride;
    }

    if(blkSizeC)
        CS = sqrt((double)CsVal / (double)blkSizeC);
    else {
        //fprintf(stderr, "ERROR: RSCS block %dx%d\n", width, height);
        VM_ASSERT(!"ERROR: RSCS block");
        CS = 0;
    }

    // RS
    pBuf = pSrcBuf;
    for(i = 1; i < height; i++) {
        for(j = 0; j < width; j++) {
            tmpVal = pBuf[j] - pBuf[j+stride];
            RsVal += tmpVal * tmpVal;
        }
        pBuf += stride;
    }

    if(blkSizeR)
        RS = sqrt((double)RsVal / (double)blkSizeR);
    else {
        VM_ASSERT(!"ERROR: RSCS block");
        RS = 0;
    }

    RsCs = sqrt(CS * CS + RS * RS);

    return RsCs;

} // 

const double CU_RSCS_TH[5][4][8] = {
    {{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0}},
    {{  5.0,  6.0,  8.0, 11.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  7.0,  9.0, 11.0, 14.0, 18.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 14.0, 18.0, 25.0,65025.0}},
    {{  5.0,  7.0, 10.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 20.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0}},
    {{  6.0,  8.0, 10.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 19.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 15.0, 19.0, 24.0,65025.0}},
    {{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}}
};


const double LQ_M[5][8]   = {
    {4.2415, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.0684},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.2005, 4.2005, 4.2005},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},    // B3
};
const double LQ_K[5][8] = {
    {12.8114, 13.8536, 13.8536, 13.8536, 13.8395, 13.8395, 13.8395, 13.8395},   // I
    {12.3857, 12.3857, 12.3857, 12.3857, 12.3857, 13.7122, 13.7122, 13.7122},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};
const double LQ_M16[5][8]   = {
    {4.3281, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.3281},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.3281, 4.3281, 4.3281},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B3
};
const double LQ_K16[5][8] = {
    {14.4329, 14.8983, 14.8983, 14.8983, 14.9069, 14.9069, 14.9069, 14.4329},   // I
    {12.4456, 12.4456, 12.4456, 12.4456, 12.4456, 13.5336, 13.5336, 13.5336},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};


int GetCalqDeltaQp(Frame* frame, const H265VideoParam & par, Ipp32s ctb_addr, Ipp64f sliceLambda, Ipp64f sliceQpY)
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
        qpClass = (sliceQpY - 22 - picClass) / 5;

    Ipp32s col =  (ctb_addr % par.PicWidthInCtbs);
    Ipp32s row =  (ctb_addr / par.PicWidthInCtbs);
    Ipp32s pelX = col * par.MaxCUSize;
    Ipp32s pelY = row * par.MaxCUSize;

    //TODO: replace by template call here
#ifndef AMT_DQP_FIX
    Ipp8u* ySrc = frame->m_origin->y + pelX + pelY * frame->m_origin->pitch_luma_pix;
    Ipp64f rscs = compute_block_rscs(ySrc, (Ipp32s)par.MaxCUSize, (Ipp32s)par.MaxCUSize, frame->m_origin->pitch_luma_pix);
#else
    // complex ops in Enqueue frame can cause severe threading eff loss -NG
    Ipp64f rscs = 0;
    //if(par.DeltaQpMode&AMT_DQP_PAQ) {
    //    // pre-calculated
    //    rscs = frame->m_stats[0]->rscs_ctb[ctb_addr];
    //} else {
    if(picClass<2)
    {
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
                Rs += frame->m_stats[0]->m_rs[(Y4+i)*w4+(X4+j)];
                Cs += frame->m_stats[0]->m_cs[(Y4+i)*w4+(X4+j)];
            }
        }
        Rs/=(m4T*n4T);
        Cs/=(m4T*n4T);
        rscs = sqrt(Rs + Cs);
    }
#endif
    Ipp32s rscsClass = 0;
    {
        Ipp32s k = 7;
        for (Ipp32s i = 0; i < 8; i++) {
            if (rscs < CU_RSCS_TH[picClass][qpClass][i]*(double)(1<<(par.bitDepthLumaShift))) {
                k = i;
                break;
            }
        }
        rscsClass = k;
    }

    Ipp64f dLambda = sliceLambda * 256.0;
    Ipp32s gopSize = frame->m_biFramesInMiniGop + 1;
    Ipp64f QP = 0;
    if(16 == gopSize) {
        QP = LQ_M16[picClass][rscsClass]*log( dLambda ) + LQ_K16[picClass][rscsClass];
    } else if(8 == gopSize){
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    } else { // default case could be modified !!!
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    }
    QP -= sliceQpY;
    return (QP>=0.0)?int(QP+0.5):int(QP-0.5);
} // 


void UpdateAllLambda(Frame* frame, const H265VideoParam& param)
{
    Statistics* stats = frame->m_stats[0];

    bool IsHiCplxGOP = false;
    bool IsMedCplxGOP = false;
    if (param.DeltaQpMode&AMT_DQP_CAL) {
        Ipp64f SADpp = stats->avgTSC; 
        Ipp64f SCpp  = stats->avgsqrSCpp;
        if (SCpp > 2.0) {
            Ipp64f minSADpp = 0;
            if (param.GopRefDist > 8) {
                minSADpp = 1.3*SCpp - 2.6;
                if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                if (!IsHiCplxGOP) {
                    Ipp64f minSADpp = 1.1*SCpp - 2.2;
                    if(minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                }
            } 
            else {
                minSADpp = 1.1*SCpp - 1.5;
                if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                if (!IsHiCplxGOP) {
                    Ipp64f minSADpp = 1.0*SCpp - 2.0;
                    if (minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                }
            }
        }
    }

    Ipp64f rd_lamba_multiplier;
    bool extraMult = SliceLambdaMultiplier(rd_lamba_multiplier, param,  frame->m_slices[0].slice_type, frame, IsHiCplxGOP, IsMedCplxGOP);

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
        for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
            Ipp32s deltaQp = frame->m_stats[0]->qp_mask[ctb];
            //deltaQp = Saturate(-MAX_DQP, MAX_DQP, deltaQp);

            Ipp32s lcuQp = frame->m_lcuQps[ctb] + deltaQp;
            lcuQp = Saturate(0, 51, lcuQp);
            frame->m_lcuQps[ctb] = lcuQp;
        }
    }
    // recalc (align) CTB lambdas with CTB Qp
    for (Ipp8u i = 0; i < par.NumSlices; i++)
        UpdateAllLambda(frame, par);

    // assign CALQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_CAQ) {
        Ipp64f baseQP = frame->m_sliceQpY;
        Ipp64f sliceLambda =  frame->m_dqpSlice[0].rd_lambda_slice;
        for (Ipp32s ctb_addr = 0; ctb_addr < numCtb; ctb_addr++) {
            int calq = GetCalqDeltaQp(frame, par, ctb_addr, sliceLambda, baseQP);

            Ipp32s totalDQP = calq;
            /*
            if(useBrc) {
            if(par.cbrFlag) totalDQP = Saturate(-1, 1, totalDQP);  // CBR
            else            totalDQP = Saturate(-2, 2, totalDQP);  // VBR
            }
            */
            if (par.DeltaQpMode&AMT_DQP_PAQ) {
                totalDQP += frame->m_stats[0]->qp_mask[ctb_addr];
            }
            //totalDQP = Saturate(-MAX_DQP, MAX_DQP, totalDQP);
            Ipp32s lcuQp = frame->m_sliceQpY + totalDQP;
            lcuQp = Saturate(0, 51, lcuQp);
            frame->m_lcuQps[ctb_addr] = lcuQp;
        }
    }

    // if (BRC) => need correct deltaQp (via QStep) to hack BRC logic
    /*
    if (useBrc) {
    Ipp32s totalDeltaQp = 0;
    Ipp32s corr = 0;
    Ipp64f corr0 = pow(2.0, (frame->m_sliceQpY-4)/6.0);
    Ipp64f denum = 0.0;
    for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
    denum += pow(2.0, (frame->m_lcuQps[ctb] -4) / 6.0);
    }
    corr = 6.0 * log10( (numCtb * corr0) / denum) / log10(2.0);
    // final correction 
    for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
    Ipp32s lcuQp = frame->m_lcuQps[ctb] + corr;
    lcuQp = Saturate(0, 51, lcuQp);
    frame->m_lcuQps[ctb] = lcuQp;
    }
    }
    */
    //WriteDQpMap(frame, par);
}

#if 0
Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    Ipp32s numRows = rowsInRegion;
                    if(((Ipp32s)(region_row * rowsInRegion) + numRows)*SIZE_BLK_LA > frame->height)
                            numRows = (frame->height - (region_row * rowsInRegion)*SIZE_BLK_LA) / SIZE_BLK_LA;
#endif


mfxStatus Lookahead::Execute(ThreadingTask& task)
{
    Ipp32s frameOrder = task.poc;
    if (frameOrder < 0)
        return MFX_ERR_NONE;

    ThreadingTaskSpecifier action = task.action;
    Ipp32u region_row = task.row;
    Ipp32u region_col = task.col;
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
                        Scale<Ipp8u>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres, 0);
                    else
                        Scale<Ipp16u>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres, 0);

                    if (m_videoParam.DeltaQpMode || m_videoParam.AnalyzeCmplx)
                        PadRectLuma(*in[fieldNum]->m_lowres, m_videoParam.fourcc, 0, 0, in[fieldNum]->m_lowres->width, in[fieldNum]->m_lowres->height);
                }
                if (m_videoParam.SceneCut) {
                    Ipp32s updateGop = 1;
                    Ipp32s updateState = 1;
                    DetectSceneCut(begin, end, in[fieldNum], updateGop, updateState);
                }
                m_lastAcceptedFrame[fieldNum] = in[fieldNum];
            }

            break;
        }


    case TT_PREENC_ROUTINE:
        {

            if (in[0] && in[0]->m_frameOrder > 0 && 
                (m_videoParam.AnalyzeCmplx || (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL)) )) 
            {
                for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame* curr = in[fieldNum];
                    FrameIter it = std::find_if(begin, end, isEqual(curr->m_frameOrder-1));

                    // ME (prev, curr)
                    if (it != m_inputQueue.end()) {
                        Frame* prev = (*it);
                        FrameData* frame = useLowres ? curr->m_lowres : curr->m_origin;
                        Statistics* stat = curr->m_stats[ useLowres ? 1 : 0 ];
                        FrameData* framePrev = useLowres ? prev->m_lowres : prev->m_origin;

                        Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                        Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                        for (Ipp32s i=0; i<numActiveRows; i++) {
                            DoInterAnalysis_OneRow(frame, NULL, framePrev, &stat->m_interSad[0], &stat->m_interSatd[0], &stat->m_mv[0], 0, 0, m_videoParam.LowresFactor, curr->m_bitDepthLuma, region_row * rowsInRegion + i);
                        }

                        if ((m_videoParam.DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL)) && useLowres) { // need refine for _Original_ resolution
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
                    }

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
                                DoPDistInterAnalysis_OneRow(*curr, prevP, nextP, region_row, m_lowresRowsInRegion, m_originRowsInRegion, m_videoParam.LowresFactor);
                            }
                        }
                    }
                } // foreach fieldNum
            }

            // INTRA:  optional ORIGINAL / LOWRES 
            if (in && m_videoParam.AnalyzeCmplx) {
                for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    FrameData* frame = useLowres ? in[fieldNum]->m_lowres : in[fieldNum]->m_origin;
                    Statistics* stat = in[fieldNum]->m_stats[useLowres ? 1 : 0];
                    Ipp32s rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    Ipp32s numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                    if (in[fieldNum]->m_bitDepthLuma == 8) {
                        for (Ipp32s i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<Ipp8u>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i);
                    } else { 
                        for (Ipp32s i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<Ipp16u>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i);
                    }
                }
            }


            // RsCs: only ORIGINAL
            if (in && m_videoParam.DeltaQpMode) {
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
            // syncpoint only (like accumulation & refCounter--), no real hard working here!!!
            for (Ipp32s fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                if (m_videoParam.AnalyzeCmplx) {
                     FrameIter it = std::find_if(begin, end, isEqual(in[fieldNum]->m_frameOrder-1));
                    if (it != end) (*it)->m_lookaheadRefCounter--;
                    
                }
                if (m_videoParam.DeltaQpMode) {
                    AverageRsCs(in[fieldNum]);
                    if (m_videoParam.DeltaQpMode == AMT_DQP_CAQ) {
                        if (in[fieldNum]) in[fieldNum]->m_lookaheadRefCounter--;
                    } else { // & (PAQ | CAL)
                        Ipp32s frameOrderCentral = in[fieldNum]->m_frameOrder - m_bufferingPaq;
                        Ipp32s updateState = 1;
                        BuildQpMap(begin, end, frameOrderCentral, m_videoParam, updateState);
                    }
                }
            }
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
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
