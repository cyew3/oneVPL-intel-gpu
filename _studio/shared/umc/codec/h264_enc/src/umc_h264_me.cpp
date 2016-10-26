//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#include <string.h>
#include <limits.h>
#include <math.h>

#include "umc_h264_tables.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_bme.h"
#include "umc_h264_video_encoder.h"
#include "ippi.h"

namespace UMC_H264_ENCODER
{

#define BS_16x16  20
#define BS_16x8   18
#define BS_8x16   12
#define BS_8x8    10
#define BS_8x4     9
#define BS_4x8     6
#define BS_4x4     5
#define BS_4x2     4
#define BS_2x4     3
#define BS_2x2     2

#define CHECK_CBP_EMPTY_THRESH(sad, mask, factor) \
    if ((sad) < (core_enc->m_EmptyThreshold[iQP] >> (factor))) \
        cur_mb.LocalMacroblockInfo->cbp_luma &= (mask)

#define VertInWind(V) (V.mvx >= xL && V.mvx <= xR && V.mvy >= yT && V.mvy <= yB)
#define VertOutWind(V) (V.mvx < xL || V.mvx > xR || V.mvy < yT || V.mvy > yB)
#ifdef NO_PADDING
#define SAD_M(V, Val) \
    meInfo->interpolateInfo.pointVector.x = V.mvx << SUB_PEL_SHIFT; \
    meInfo->interpolateInfo.pointVector.y = V.mvy << SUB_PEL_SHIFT; \
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo)); \
    Val = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize); \
    Val += MVConstraint((V.mvx << SUB_PEL_SHIFT) - predictedMV.mvx, (V.mvy << SUB_PEL_SHIFT) - predictedMV.mvy, pRDQM);
#else // NO_PADDING
#define SAD_M(V, Val) Val = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, V.mvx, V.mvy), pitchPixels, blockSize); \
                      Val += MVConstraint((V.mvx << SUB_PEL_SHIFT) - predictedMV.mvx, (V.mvy << SUB_PEL_SHIFT) - predictedMV.mvy, pRDQM)
#endif // NO_PADDING

#define SHIFT_16x8          1
#define SHIFT_8x16          3
#define SHIFT_8x8          5

#ifdef NO_PADDING
#define REFINE_BIDIR for (i = 0; i < 9; i ++) \
    {\
    curMVL0.mvx = bestMVL0.mvx + bdX[i];\
    curMVL0.mvy = bestMVL0.mvy + bdY[i];\
    curMVL1.mvx = bestMVL1.mvx + bdX[i];\
    curMVL1.mvy = bestMVL1.mvy + bdY[i];\
    \
    pInterpolateInfo->pSrc[0] = pRefL0; \
    pInterpolateInfo->pDst[0] = addTmpBufL0[i]; \
    pInterpolateInfo->pointVector.x = curMVL0.mvx; \
    pInterpolateInfo->pointVector.y = curMVL0.mvy; \
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo); \
    pInterpolateInfo->pSrc[0] = pRefL1; \
    pInterpolateInfo->pDst[0] = addTmpBufL1[i]; \
    pInterpolateInfo->pointVector.x = curMVL1.mvx; \
    pInterpolateInfo->pointVector.y = curMVL1.mvy; \
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo); \
}\
    \
    for (posL0 = 0; posL0 < 9; posL0 ++)\
    for (posL1 = 0; posL1 < 9; posL1 ++)\
    { \
    curMVL0.mvx = bestMVL0.mvx + bdX[posL0];\
    curMVL0.mvy = bestMVL0.mvy + bdY[posL0];\
    curMVL1.mvx = bestMVL1.mvx + bdX[posL1];\
    curMVL1.mvy = bestMVL1.mvy + bdY[posL1];\
    \
    H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidirBuff, addTmpBufL0[posL0], addTmpBufL1[posL1], 16, 1 ,0, 0, block); \
    if (core_enc->m_Analyse & ANALYSE_SAD)                    \
    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
                else                    \
                cSAD = H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
                cSAD += MVConstraint( curMVL0.mvx - predictedMVL0.mvx, curMVL0.mvy - predictedMVL0.mvy, glob_RDQM[cur_mb.lumaQP51])+\
                MVConstraint(curMVL1.mvx - predictedMVL1.mvx, curMVL1.mvy - predictedMVL1.mvy, glob_RDQM[cur_mb.lumaQP51]);\
                if (cSAD < bSAD)\
    {\
    bSAD = cSAD;\
    bMVL0 = curMVL0;\
    bMVL1 = curMVL1;\
    MFX_INTERNAL_CPY(bestRefinedBlockMC,bidirBuff,256*sizeof(PIXTYPE));\
    isRefined = 1;\
}\
}
#else // NO_PADDING
#define REFINE_BIDIR for (i = 0; i < 9; i ++) \
    {\
    curMVL0.mvx = bestMVL0.mvx + bdX[i];\
    curMVL0.mvy = bestMVL0.mvy + bdY[i];\
    curMVL1.mvx = bestMVL1.mvx + bdX[i];\
    curMVL1.mvy = bestMVL1.mvy + bdY[i];\
    \
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRefCurL0, pitchPixels, curMVL0.mvx >> SUB_PEL_SHIFT, curMVL0.mvy >> SUB_PEL_SHIFT), pitchPixels,\
    addTmpBufL0[i], 16, curMVL0.mvx&3, curMVL0.mvy&3, block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);\
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRefCurL1, pitchPixels, curMVL1.mvx >> SUB_PEL_SHIFT, curMVL1.mvy >> SUB_PEL_SHIFT), pitchPixels,\
    addTmpBufL1[i], 16, curMVL1.mvx&3, curMVL1.mvy&3, block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);\
}\
    \
    for (posL0 = 0; posL0 < 9; posL0 ++)\
    for (posL1 = 0; posL1 < 9; posL1 ++)\
    {\
    curMVL0.mvx = bestMVL0.mvx + bdX[posL0];\
    curMVL0.mvy = bestMVL0.mvy + bdY[posL0];\
    curMVL1.mvx = bestMVL1.mvx + bdX[posL1];\
    curMVL1.mvy = bestMVL1.mvy + bdY[posL1];\
    \
    H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidirBuff, addTmpBufL0[posL0], addTmpBufL1[posL1], 16, 1 ,0, 0, block);                \
    if (core_enc->m_Analyse & ANALYSE_SAD)                    \
    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
    else                    \
    cSAD = H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
    cSAD += MVConstraint( curMVL0.mvx - predictedMVL0.mvx, curMVL0.mvy - predictedMVL0.mvy, glob_RDQM[cur_mb.lumaQP51]);\
    cSAD += MVConstraint(curMVL1.mvx - predictedMVL1.mvx, curMVL1.mvy - predictedMVL1.mvy, glob_RDQM[cur_mb.lumaQP51]);\
    if (cSAD < bSAD)\
    {\
    bSAD = cSAD;\
    bMVL0 = curMVL0;\
    bMVL1 = curMVL1;\
    MFX_INTERNAL_CPY(bestRefinedBlockMC,bidirBuff,256*sizeof(PIXTYPE));\
    isRefined = 1;\
}\
}
#endif // NO_PADDING

    /*#define REFINE_BIDIR for (i = 0; i < 9; i ++)\
    {\
    curMVL0.mvx = bestMVL0.mvx + bdX[i];\
    curMVL0.mvy = bestMVL0.mvy + bdY[i];\
    curMVL1.mvx = bestMVL1.mvx + bdX[i];\
    curMVL1.mvy = bestMVL1.mvy + bdY[i];\
    \
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRefCurL0, pitchPixels, curMVL0.mvx >> SUB_PEL_SHIFT, curMVL0.mvy >> SUB_PEL_SHIFT), pitchPixels,\
    addTmpBufL0[i], 16, curMVL0.mvx&3, curMVL0.mvy&3, block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);\
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRefCurL1, pitchPixels, curMVL1.mvx >> SUB_PEL_SHIFT, curMVL1.mvy >> SUB_PEL_SHIFT), pitchPixels,\
    addTmpBufL1[i], 16, curMVL1.mvx&3, curMVL1.mvy&3, block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);\
    }\
    \
    for (posL0 = 0; posL0 < 9; posL0 ++)\
    {\
    curMVL0.mvx = bestMVL0.mvx + bdX[posL0];\
    curMVL0.mvy = bestMVL0.mvy + bdY[posL0];\
    n = bPC[posL0];\
    for (posL1 = 0; posL1 < n; posL1 ++)\
    {\
    curMVL1.mvx = bestMVL1.mvx + bdL1[posL0][0][posL1];\
    curMVL1.mvy = bestMVL1.mvy + bdL1[posL0][1][posL1];\
    \
    H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidirBuff, addTmpBufL0[posL0], addTmpBufL1[bdL1PosBuf[posL0][posL1]], 16, 1 ,0, 0, block);\
    if (core_enc->m_Analyse & ANALYSE_SAD)\
    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
    else\
    cSAD = H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, bidirBuff, 16, blockSize);\
    cSAD += MVConstraint( curMVL0.mvx - predictedMVL0.mvx, curMVL0.mvy - predictedMVL0.mvy, glob_RDQM[cur_mb.lumaQP51])+\
    MVConstraint(curMVL1.mvx - predictedMVL1.mvx, curMVL1.mvy - predictedMVL1.mvy, glob_RDQM[cur_mb.lumaQP51]);\
    if (cSAD < bSAD)\
    {\
    bSAD = cSAD;\
    bMVL0 = curMVL0;\
    bMVL1 = curMVL1;\
    MFX_INTERNAL_CPY(bestRefinedBlockMC,bidirBuff,256*sizeof(PIXTYPE));\
    isRefined = 1;\
    }\
    }\
    }*/

#define COPY_BLOCK pBestRefinedBlockMC = bestRefinedBlockMC;\
    for (i = 0; i < block.height; i++)\
    {\
    MFX_INTERNAL_CPY(mcCurBuff,pBestRefinedBlockMC,block.width * sizeof(PIXTYPE));\
    pBestRefinedBlockMC += 16;\
    mcCurBuff += 16;\
}

#define TR_SHIFT 8

#define MV_SEARCH_TYPE_FULL             0
#define MV_SEARCH_TYPE_CLASSIC_LOG      1
#define MV_SEARCH_TYPE_LOG              2
#define MV_SEARCH_TYPE_EPZS             3
#define MV_SEARCH_TYPE_FULL_ORTHOGONAL  4
#define MV_SEARCH_TYPE_LOG_ORTHOGONAL   5
#define MV_SEARCH_TYPE_TTS              6
#define MV_SEARCH_TYPE_NEW_EPZS         7
#define MV_SEARCH_TYPE_UMH              8
#define MV_SEARCH_TYPE_SQUARE           9
#define MV_SEARCH_TYPE_FTS             10
#define MV_SEARCH_TYPE_SMALL_DIAMOND   11

#define MV_SEARCH_TYPE_SUBPEL_FULL      0
#define MV_SEARCH_TYPE_SUBPEL_HALF      1
#define MV_SEARCH_TYPE_SUBPEL_SQUARE    2
#define MV_SEARCH_TYPE_SUBPEL_HQ        3
#define MV_SEARCH_TYPE_SUBPEL_DIAMOND   4

#define SB_THRESH_RD  269 >> 8

enum PredType
{
    MVPRED_MEDIAN,
    MVPRED_A,
    MVPRED_B,
    MVPRED_C
};

#define TR_RND (1 << (TR_SHIFT - 1))

#define TRUNCATE_LO(val, lim) \
{ \
    Ipp32s (tmp) = (lim); \
    if ((tmp) < (val)) \
        (val) = (Ipp16s) (tmp); \
}

#define TRUNCATE_HI(val, lim) \
{ \
    Ipp32s (tmp) = (lim); \
    if ((tmp) > (val)) \
        (val) = (Ipp16s) (tmp); \
}

static void AdjustIndex(
                 Ipp8u cur_mb_is_bottom,
                 Ipp8u cur_mb_is_field,
                 Ipp8u ref_mb_is_bottom,
                 Ipp8u ref_mb_is_field,
                 Ipp8s& RefIdx)
{
    if (RefIdx<0)
    {
        RefIdx=0;
    }
    if (ref_mb_is_field) //both are AFRM
    {
        if (cur_mb_is_field)
        {
            bool same_parity = (((RefIdx&1) ^ ref_mb_is_bottom) == cur_mb_is_bottom);
            if (same_parity)
                RefIdx&=-2;
            else
                RefIdx|=1;
        }
    }
}

#define PIXBITS 8
#include "umc_h264_me_tmpl.cpp.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_me_tmpl.cpp.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} //namespace UMC_H264_ENCODER

#endif //UMC_ENABLE_H264_VIDEO_ENCODER



