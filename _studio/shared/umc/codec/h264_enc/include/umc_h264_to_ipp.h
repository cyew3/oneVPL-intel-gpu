//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef UMC_H264_TO_IPP_H
#define UMC_H264_TO_IPP_H

#include "ippdefs.h"
#include "ipps.h"
#include "ippvc.h"
#include "umc_h264_core_enc.h"

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif

inline void ippiTransformDequantLumaDC_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp32s QP,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantInvLumaDC4x4_H264_16s_C1I(pSrcDst, QP, pScaleLevels);
}

inline void ippiTransformDequantChromaDC_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp32s QP,
    const Ipp16s* pLevelScale/* = NULL*/)
{
    ippiTransformQuantInvChromaDC2x2_H264_16s_C1I (pSrcDst, QP, pLevelScale);
}

inline void ippiDequantTransformResidualAndAdd_H264_8u16s(
    const Ipp8u*  pPred,
    Ipp16s* pSrcDst,
    const Ipp16s* pDC, // should be const ?
    Ipp8u*  pDst,
    Ipp32s  PredStep,
    Ipp32s  DstStep,
    Ipp32s  QP,
    Ipp32s  AC,
    Ipp32s  /*bit_depth*/,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantInvAddPred4x4_H264_16s_C1IR (pPred, PredStep, pSrcDst, pDC, pDst, DstStep, QP, AC, pScaleLevels);
}

inline void ippiTransformLuma8x8Fwd_H264_8u16s(
    const Ipp16s* pDiffBuf,
    Ipp16s* pTransformResult)
{
    ippiTransformFwdLuma8x8_H264_16s_C1(pDiffBuf, pTransformResult);
}

void QuantOptLuma8x8_H264_16s_C1_8u16s(
    const Ipp16s* pSrc,
    Ipp16s* pDst,
    Ipp32s  Qp6,
    Ipp32s  intra,
    const Ipp16s* pScanMatrix,
    const Ipp16s* pScaleLevels,
    Ipp32s* pNumLevels,
    Ipp32s* pLastCoeff,
    const H264Slice_8u16s* curr_slice,
    TrellisCabacState* cbSt,
    const Ipp16s* pInvLevelScale);

inline void ippiQuantLuma8x8_H264_8u16s(
    const Ipp16s* pSrc,
    Ipp16s* pDst,
    Ipp32s  Qp6,
    Ipp32s  intra,
    const Ipp16s* pScanMatrix,
    const Ipp16s* pScaleLevels,
    Ipp32s* pNumLevels,
    Ipp32s* pLastCoeff,
    const H264Slice_8u16s* curr_slice/* = NULL*/,
    TrellisCabacState* cbSt/* = NULL*/,
    const Ipp16s* pInvLevelScale/* = NULL*/)
{
    if( curr_slice == NULL ){
        ippiQuantLuma8x8_H264_16s_C1(pSrc, pDst,Qp6,intra,pScanMatrix,pScaleLevels,pNumLevels,pLastCoeff);
    }else{
        QuantOptLuma8x8_H264_16s_C1_8u16s(pSrc, pDst,Qp6,intra,pScanMatrix,pScaleLevels,pNumLevels,pLastCoeff,curr_slice,cbSt,pInvLevelScale);
    }
}

inline void ippiQuantLuma8x8Inv_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp32s Qp6,
    const Ipp16s* pInvLevelScale)
{
    ippiQuantLuma8x8Inv_H264_16s_C1I(pSrcDst, Qp6, pInvLevelScale);
}

inline void ippiTransformLuma8x8InvAddPred_H264_8u16s(
    const Ipp8u* pPred,
    Ipp32s PredStepPixels,
    Ipp16s* pSrcDst,
    Ipp8u* pDst,
    Ipp32s DstStepPixels,
    Ipp32s bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    ippiTransformLuma8x8InvAddPred_H264_16s8u_C1R(pPred, PredStepPixels*sizeof(Ipp8u), pSrcDst,pDst, DstStepPixels*sizeof(Ipp8u));
}

inline void ippiEncodeChromaDcCoeffsCAVLC_H264_8u16s(
    Ipp16s* pSrc,
    Ipp8u*  pTrailingOnes,
    Ipp8u*  pTrailingOneSigns,
    Ipp8u*  pNumOutCoeffs,
    Ipp8u*  pTotalZeroes,
    Ipp16s* pLevels,
    Ipp8u*  pRuns)
{
    ippiEncodeChromaDcCoeffsCAVLC_H264_16s(pSrc,pTrailingOnes, pTrailingOneSigns, pNumOutCoeffs, pTotalZeroes, pLevels, pRuns);
    //ippiEncodeChromaDC2x2CoeffsCAVLC_H264_16s(pSrc,pTrailingOnes, pTrailingOneSigns, pNumOutCoeffs, pTotalZeroes, pLevels, pRuns);
}

inline void ippiEncodeChroma422DC_CoeffsCAVLC_H264_8u16s(
    const Ipp16s *pSrc,
    Ipp8u *Trailing_Ones,
    Ipp8u *Trailing_One_Signs,
    Ipp8u *NumOutCoeffs,
    Ipp8u *TotalZeros,
    Ipp16s *Levels,
    Ipp8u *Runs)
{
    ippiEncodeCoeffsCAVLCChromaDC2x4_H264_16s (pSrc, Trailing_Ones, Trailing_One_Signs, NumOutCoeffs, TotalZeros, Levels, Runs);
}

inline void ippiTransformDequantChromaDC422_H264_8u16s(
    Ipp16s *pSrcDst,
    Ipp32s QPChroma,
    Ipp16s* pScaleLevels)
{
    ippiTransformQuantInvChromaDC2x4_H264_16s_C1I(pSrcDst, QPChroma, pScaleLevels);
}

inline void ippiTransformQuantChroma422DC_H264_8u16s(
    Ipp16s *pDCBuf,
    Ipp16s *pTBuf,
    Ipp32s  QPChroma,
    Ipp32s* NumCoeffs,
    Ipp32s  intra,
    Ipp32s  NeedTransform,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantFwdChromaDC2x4_H264_16s_C1I(pDCBuf, pTBuf, QPChroma, NumCoeffs, intra, NeedTransform, pScaleLevels);
}

inline void ippiSumsDiff8x8Blocks4x4_8u16s(
    Ipp8u* pSrc,
    Ipp32s srcStepPixels,
    Ipp8u* pPred,
    Ipp32s predStepPixels,
    Ipp16s* pDC,
    Ipp16s* pDiff)
{
    ippiSumsDiff8x8Blocks4x4_8u16s_C1(pSrc, srcStepPixels*sizeof(Ipp8u), pPred, predStepPixels*sizeof(Ipp8u), pDC, pDiff);
}

inline void ippiTransformQuantChromaDC_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp16s* pTBlock,
    Ipp32s  QPChroma,
    Ipp32s*    pNumLevels,
    Ipp32s  intra,
    Ipp32s  needTransform,
    const Ipp16s *pScaleLevels/* = NULL*/)
{
    ippiTransformQuantFwdChromaDC2x2_H264_16s_C1I(pSrcDst, pTBlock, QPChroma, pNumLevels, intra, needTransform,pScaleLevels);
}

IppStatus TransformQuantOptFwd4x4_H264_16s_C1(
    Ipp16s* pSrcDst,
    Ipp16s* pPred,
    Ipp16s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  Intra,
    Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    Ipp16s* pScaleLevels,
    const H264Slice_8u16s * curr_slice,
    Ipp32s sCoeff,
    TrellisCabacState* states);

inline void ippiTransformQuantResidual_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp16s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  Intra,
    Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    Ipp16s* pScaleLevels/* = NULL*/,
    const H264Slice_8u16s* curr_slice/* = NULL*/,
    Ipp32s sCoeff/* = 0*/,
    TrellisCabacState* states/* = NULL*/)
{
    if( curr_slice == NULL ){
        ippiTransformQuantFwd4x4_H264_16s_C1(pSrcDst,pDst, Qp, pNumLevels, (Ipp8u)Intra, pScanMatrix,pLastCoeff, pScaleLevels);
    }else{
        TransformQuantOptFwd4x4_H264_16s_C1(pSrcDst, NULL, pDst, Qp, pNumLevels, (Ipp8u)Intra, pScanMatrix,pLastCoeff, pScaleLevels, curr_slice, sCoeff, states);
    }
}

IppStatus ippiDequantResidual4x4_H264_16s(const Ipp16s *pCoeffs,
                                          Ipp16s *pDst,
                                          Ipp32s qp);

IppStatus ippiDequantResidual8x8_H264_16s(const Ipp16s *pCoeffs,
                                          Ipp16s *pDst,
                                          Ipp32s qp,
                                          const Ipp16s *pScaleCoeffs);


IppStatus ippiTransformResidualAndAdd_H264_16s8u_C1I(const Ipp8u *pPred,
                                                     Ipp32s predPitch,
                                                     Ipp16s *pCoeffs,
                                                     Ipp8u *pRec,
                                                     Ipp32s recPitch);
IppStatus ippiTransformResidualAndAdd8x8_H264_16s8u_C1I(const Ipp8u *pPred,
                                                        Ipp16s *pCoeffs,
                                                        Ipp8u *pRec,
                                                        Ipp32s predPitch,
                                                        Ipp32s recPitch);
IppStatus TransformSubQuantFwd4x4_H264_16s_C1(
    Ipp16s* pSrcDst,
    Ipp16s* pPred,
    Ipp16s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  Intra,
    Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    Ipp16s* pScaleLevels);

inline void ippiTransformSubQuantResidual_H264_8u16s(
    Ipp16s* pSrcDst,
    Ipp16s* pPred,
    Ipp16s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  Intra,
    Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    Ipp16s* pScaleLevels/* = NULL*/,
    const H264Slice_8u16s* curr_slice/* = NULL*/,
    Ipp32s sCoeff/* = 0*/,
    TrellisCabacState* states/* = NULL*/)
{
    if( curr_slice == NULL ){
        TransformSubQuantFwd4x4_H264_16s_C1(pSrcDst, pPred, pDst, Qp, pNumLevels, (Ipp8u)Intra, pScanMatrix,pLastCoeff, pScaleLevels);
    }else{
        TransformQuantOptFwd4x4_H264_16s_C1(pSrcDst, pPred, pDst, Qp, pNumLevels, (Ipp8u)Intra, pScanMatrix,pLastCoeff, pScaleLevels, curr_slice, sCoeff, states);
    }
}


inline void ippiInterpolateLuma_H264_8u16s(
    const Ipp8u*   src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    try{
    ippiInterpolateLuma_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, sz);
    }
    catch(...){
        assert(0);
    }
}

// can be defined in different files
//#ifdef NO_PADDING
inline void ippiInterpolateLumaBlock_H264_8u16s(
    IppVCInterpolateBlock_8u * interpolateInfo
    /*const Ipp8u*   src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   mvx,
    Ipp32s   mvy,
    Ipp32s   blkx,
    Ipp32s   blky,
    IppiSize frmsz,
    IppiSize blksz*/)
{
    ippiInterpolateLumaBlock_H264_8u_P1R(interpolateInfo);
}
//#endif //NO_PADDING

inline void ippiInterpolateLumaTop_H264_8u16s(
    const Ipp8u* src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    Ipp32s   outPixels,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    ippiInterpolateLumaTop_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, outPixels, sz);
}

/* Good interpation */
#if defined(NO_PADDING) && defined( USE_NV12 )
#ifndef NV12_INTERPOLATION
typedef struct H264InterpolationParams_8u
{
    const Ipp8u *pSrc;                                          /* (const Ipp8u *) pointer to the source memory */
    SIZE_T srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp8u *pDst;                                                /* (Ipp8u *) pointer to the destination memory */
    SIZE_T dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    const Ipp8u *pSrcComplementary;                             /* (const Ipp8u *) pointer to the complementary source memory */
    Ipp8u *pDstComplementary;                                   /* (Ipp8u *) pointer to the complementary destination memory */

} H264InterpolationParams_8u;
typedef void (*pH264Interpolation_8u) (H264InterpolationParams_8u *pParams);

#define read_data_through_boundary_table_8u read_data_through_boundary_table_8u_pxmx
#define h264_interpolate_chroma_type_table_8u h264_interpolate_chroma_type_table_8u_pxmx

static void read_data_through_boundary_none_8u(H264InterpolationParams_8u *pParams)
{
    /* touch unreferenced parameter(s) */
    UNREFERENCED_PARAMETER(pParams);

    /* there is something wrong */

} /* void read_data_through_boundary_none_8u(H264InterpolationParams_8u *) */

static void read_data_through_boundary_left_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            memset(pDst, pSrc[0], iIndent);
            for(l=0; l<pParams->dataWidth - iIndent;l++) pDst[l+iIndent] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_left_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_right_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            for(l=0; l<iIndent;l++) pDst[l] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            memset(pDst + iIndent, pSrc[2*(iIndent - 1)], pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_left_right_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_left_8u_px(pParams);
    else
        read_data_through_boundary_right_8u_px(pParams);

} /* void read_data_through_boundary_left_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            for(l=0; l<pParams->dataWidth;l++) pDst[l] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            for(l=0; l<pParams->dataWidth;l++) pDst[l] = pSrc[2*l];
//            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_left_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc;
        Ipp8u *pDst = pParams->pDst;
        Ipp8u *pTmp = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        memset(pDst, pSrc[0], iIndent);
        for(l=0; l<pParams->dataWidth-iIndent;l++) pDst[l+iIndent] = pSrc[2*l];
        //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, pParams->dataWidth);

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            memset(pDst, pSrc[0], iIndent);
            for(l=0; l<pParams->dataWidth-iIndent;l++) pDst[l+iIndent] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_left_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_right_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp8u *pTmp = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        for(l=0; l<iIndent;l++) pDst[l] = pSrc[2*l];
        //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
        memset(pDst + iIndent, pSrc[2*(iIndent - 1)], pParams->dataWidth - iIndent);

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, pParams->dataWidth);

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            for(l=0; l<iIndent;l++) pDst[l] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            memset(pDst + iIndent, pSrc[2*(iIndent - 1)], pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_left_right_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_top_left_8u_px(pParams);
    else
        read_data_through_boundary_top_right_8u_px(pParams);

} /* void read_data_through_boundary_top_left_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;

        /* copy existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            for(l=0; l<pParams->dataWidth;l++) pDst[l] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_left_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            memset(pDst, pSrc[0], iIndent);
            for(l=0; l<pParams->dataWidth-iIndent;l++) pDst[l+iIndent] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_left_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_right_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i,l;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            for(l=0; l<iIndent;l++) pDst[l] = pSrc[2*l];
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            memset(pDst + iIndent, pSrc[2*(iIndent - 1)], pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_left_right_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_bottom_left_8u_px(pParams);
    else
        read_data_through_boundary_bottom_right_8u_px(pParams);

} /* void read_data_through_boundary_bottom_left_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_8u_px(pParams);
    else
        read_data_through_boundary_bottom_8u_px(pParams);

} /* void read_data_through_boundary_top_top_bottom_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_left_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_left_8u_px(pParams);
    else
        read_data_through_boundary_bottom_left_8u_px(pParams);

} /* void read_data_through_boundary_top_bottom_left_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_right_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_right_8u_px(pParams);
    else
        read_data_through_boundary_bottom_right_8u_px(pParams);

} /* void read_data_through_boundary_top_bottom_right_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_left_right_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_left_8u_px(pParams);
        else
            read_data_through_boundary_bottom_left_8u_px(pParams);
    }
    else
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_right_8u_px(pParams);
        else
            read_data_through_boundary_bottom_right_8u_px(pParams);
    }

} /* void read_data_through_boundary_top_bottom_left_right_8u_px(H264InterpolationParams_8u *pParams) */

static pH264Interpolation_8u read_data_through_boundary_table_8u_pxmx[16] =
{
    &read_data_through_boundary_none_8u,
    &read_data_through_boundary_left_8u_px,
    &read_data_through_boundary_right_8u_px,
    &read_data_through_boundary_left_right_8u_px,

    &read_data_through_boundary_top_8u_px,
    &read_data_through_boundary_top_left_8u_px,
    &read_data_through_boundary_top_right_8u_px,
    &read_data_through_boundary_top_left_right_8u_px,

    &read_data_through_boundary_bottom_8u_px,
    &read_data_through_boundary_bottom_left_8u_px,
    &read_data_through_boundary_bottom_right_8u_px,
    &read_data_through_boundary_bottom_left_right_8u_px,

    &read_data_through_boundary_top_bottom_8u_px,
    &read_data_through_boundary_top_bottom_left_8u_px,
    &read_data_through_boundary_top_bottom_right_8u_px,
    &read_data_through_boundary_top_bottom_left_right_8u_px

};

static void h264_interpolate_chroma_type_00_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
            pDst[x] = pSrc[2*x];

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_00_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_0x_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = pSrc[2*x] * (8 - pParams->hFraction) +
                   pSrc[2*x + 2] * (pParams->hFraction) + 4;

            pDst[x] = (Ipp8u) (iRes >> 3);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_0x_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_y0_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = pSrc[2*x] * (8 - pParams->vFraction) +
                   pSrc[2*x + srcStep] * (pParams->vFraction) + 4;

            pDst[x] = (Ipp8u) (iRes >> 3);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_y0_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_yx_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = (pSrc[2*x] * (8 - pParams->hFraction) +
                    pSrc[2*x + 2] * (pParams->hFraction)) * (8 - pParams->vFraction) +
                   (pSrc[2*x + srcStep] * (8 - pParams->hFraction) +
                    pSrc[2*x + srcStep + 2] * (pParams->hFraction)) * (pParams->vFraction) + 32;

            pDst[x] = (Ipp8u) (iRes >> 6);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_yx_8u_px(H264InterpolationParams_8u *pParams) */

static pH264Interpolation_8u h264_interpolate_chroma_type_table_8u_pxmx[4] =
{
    &h264_interpolate_chroma_type_00_8u_px,
    &h264_interpolate_chroma_type_0x_8u_px,
    &h264_interpolate_chroma_type_y0_8u_px,
    &h264_interpolate_chroma_type_yx_8u_px

};

static void h264_interpolate_chroma_type_00_8u_px_b(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
            pDst[x] = pSrc[x];

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_00_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_0x_8u_px_b(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = pSrc[x] * (8 - pParams->hFraction) +
                   pSrc[x + 1] * (pParams->hFraction) + 4;

            pDst[x] = (Ipp8u) (iRes >> 3);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_0x_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_y0_8u_px_b(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = pSrc[x] * (8 - pParams->vFraction) +
                   pSrc[x + srcStep] * (pParams->vFraction) + 4;

            pDst[x] = (Ipp8u) (iRes >> 3);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_y0_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_yx_8u_px_b(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDst = pParams->pDst;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iRes;

            iRes = (pSrc[x] * (8 - pParams->hFraction) +
                    pSrc[x + 1] * (pParams->hFraction)) * (8 - pParams->vFraction) +
                   (pSrc[x + srcStep] * (8 - pParams->hFraction) +
                    pSrc[x + srcStep + 1] * (pParams->hFraction)) * (pParams->vFraction) + 32;

            pDst[x] = (Ipp8u) (iRes >> 6);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_yx_8u_px(H264InterpolationParams_8u *pParams) */

static pH264Interpolation_8u h264_interpolate_chroma_type_table_8u_b[4] =
{
    &h264_interpolate_chroma_type_00_8u_px_b,
    &h264_interpolate_chroma_type_0x_8u_px_b,
    &h264_interpolate_chroma_type_y0_8u_px_b,
    &h264_interpolate_chroma_type_yx_8u_px_b

};


static IppStatus ippiInterpolateBoundaryChromaBlock_H264_8u_NV12(Ipp32s iOverlappingType, H264InterpolationParams_8u *pParams);

static IppStatus ippiInterpolateChromaBlock_H264_8u_P2R_NV12 (const IppVCInterpolateBlock_8u *interpolateInfo)
{
    H264InterpolationParams_8u params;

    /* check error(s) */
#if 0
    IPP_BAD_PTR1_RET(interpolateInfo);
    IPP_BAD_PTR4_RET(interpolateInfo->pSrc[0],
                     interpolateInfo->pSrc[1],
                     interpolateInfo->pDst[0],
                     interpolateInfo->pDst[1]);
    IPP_BADARG_RET( ( interpolateInfo->sizeBlock.height & 1 ) |
                    ( interpolateInfo->sizeBlock.width & ~0x0e ),  ippStsSizeErr);
#endif
    /* prepare pointers */
    params.dstStep = interpolateInfo->dstStep;
    params.srcStep = interpolateInfo->srcStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        SIZE_T iRefOffset;

        /* set the current reference offset */
        iRefOffset = interpolateInfo->pointBlockPos.y * params.srcStep +
                     interpolateInfo->pointBlockPos.x*2;

        /* set the current source pointer */
        params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
        params.pDst = interpolateInfo->pDst[0];
        /* call optimized function from the table */
//#if ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7))
//        h264_interpolate_chroma_type_table_8u[params.blockWidth & 0x0c](&params);
//#else /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
        h264_interpolate_chroma_type_table_8u[0](&params);
//#endif /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */

        /* set the current source pointer */
        params.pSrc = interpolateInfo->pSrc[1] + iRefOffset;
        params.pDst = interpolateInfo->pDst[1];
        /* call optimized function from the table */
//#if ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7))
//        h264_interpolate_chroma_type_table_8u[params.blockWidth & 0x0c](&params);
//#else /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
        h264_interpolate_chroma_type_table_8u[0](&params);
//#endif /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */

    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = interpolateInfo->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            params.hFraction =
            hFraction = interpolateInfo->pointVector.x & 7;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                params.xPos = interpolateInfo->pointBlockPos.x +
                              (pointVector.x >>= 3);
                params.dataWidth = params.blockWidth + iAddition;
            }

            /* prepare vertical vector values */
            params.vFraction =
            vFraction = pointVector.y & 7;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                params.yPos = interpolateInfo->pointBlockPos.y +
                              (pointVector.y >>= 3);
                params.dataHeight = params.blockHeight + iAddition;
            }

//#if ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7))
//            params.iType = (params.blockWidth & 0x0c) |
//                           (((vFraction) ? (1) : (0)) << 1) |
//                           ((hFraction) ? (1) : (0));
//#else /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
            params.iType = (((vFraction) ? (1) : (0)) << 1) |
                           ((hFraction) ? (1) : (0));
//#endif /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
        }


        /*  call the appropriate function
            we select the function using the following rules:
            zero bit means x position of left data boundary lie out of frame,
            first bit means x position of right data boundary lie out of frame,
            second bit means y position of top data boundary lie out of frame,
            third bit means y position of bottom data boundary lie out of frame */
        iOverlappingType = 0;
        iOverlappingType |= ((0 > params.xPos) ? (1) : (0)) << 0;
        iOverlappingType |= ((interpolateInfo->sizeFrame.width < params.xPos + params.dataWidth) ? (1) : (0)) << 1;
        iOverlappingType |= ((0 > params.yPos) ? (1) : (0)) << 2;
        iOverlappingType |= ((interpolateInfo->sizeFrame.height < params.yPos + params.dataHeight) ? (1) : (0)) << 3;

        /* call appropriate function */
        if (0 == iOverlappingType)
        {
            SIZE_T iRefOffset;

            /* set the current reference offset */
            iRefOffset = (interpolateInfo->pointBlockPos.y + pointVector.y) * params.srcStep +
                         (interpolateInfo->pointBlockPos.x + pointVector.x)*2;

            /* set the current source pointer */
            params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
            params.pDst = interpolateInfo->pDst[0];
            /* call optimized function from the table */
            h264_interpolate_chroma_type_table_8u[params.iType](&params);

            /* set the current source pointer */
            params.pSrc = interpolateInfo->pSrc[1] + iRefOffset;
            params.pDst = interpolateInfo->pDst[1];
            /* call optimized function from the table */
            h264_interpolate_chroma_type_table_8u[params.iType](&params);

            return ippStsNoErr;
        }
        else
        {
            /* save additional parameters */
            params.frameSize.width = interpolateInfo->sizeFrame.width;
            params.frameSize.height = interpolateInfo->sizeFrame.height;

            params.pSrc = interpolateInfo->pSrc[0];
            params.pSrcComplementary = interpolateInfo->pSrc[1];
            params.pDst = interpolateInfo->pDst[0];
            params.pDstComplementary = interpolateInfo->pDst[1];

            /* there is something wrong, try to work through a temporal buffer */
            return ippiInterpolateBoundaryChromaBlock_H264_8u_NV12(iOverlappingType,
                                                              &params);
        }
    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiInterpolateChromaBlock_H264_8u_P2R, (const IppVCInterpolateBlock_8u *interpolateInfo)) */

#define PREPARE_TEMPORAL_BUFFER_8U(ptr, step, buf) \
    /* align pointer */ \
    ptr = ((Ipp8u *) buf) + ((16 - (((Ipp8u *) buf) - (Ipp8u *) 0)) & 15); \
    /* set step */ \
    step = (pParams->dataWidth + 15) & -16;

static IppStatus ippiReadDataBlockThroughBoundary_8u(Ipp32s iOverlappingType, H264InterpolationParams_8u *pParams)
{
    /*  this is an internal function to read data,
        which is overlapping with frame boundary.
        Bits of iOverlappingType mean following:
        zero bit means x position of left data boundary lie out of frame,
        first bit means x position of right data boundary lie out of frame,
        second bit means y position of top data boundary lie out of frame,
        third bit means y position of bottom data boundary lie out of frame */

    /* call appropriate function */
    read_data_through_boundary_table_8u[iOverlappingType](pParams);

    return ippStsNoErr;

} /* IppStatus ippiReadDataBlockThroughBoundary_8u(Ipp32s iOverlappingType, */

static IppStatus ippiInterpolateBoundaryChromaBlock_H264_8u_NV12(Ipp32s iOverlappingType, H264InterpolationParams_8u *pParams)
{
    Ipp8u bTmp[16 * 17 + 16];
    Ipp8u *pTmp;
    Ipp32s tmpStep;
    SIZE_T srcStep;
    Ipp8u *pDst;
    SIZE_T dstStep;

    /* prepare temporal buffer */
    PREPARE_TEMPORAL_BUFFER_8U(pTmp, tmpStep, bTmp);

    /* read data into temporal buffer */
    {
        pDst = pParams->pDst;
        dstStep = pParams->dstStep;
        srcStep = pParams->srcStep;

        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        ippiReadDataBlockThroughBoundary_8u(iOverlappingType,
                                            pParams);

        pParams->pSrc = pTmp;
        pParams->srcStep = tmpStep;
        pParams->pDst = pDst;
        pParams->dstStep = dstStep;
    }

    /* call appropriate function */
    h264_interpolate_chroma_type_table_8u_b[pParams->iType](pParams);

    /*
        PROCESS COMPLEMENTARY PLANE
    */

    /* read data into temporal buffer */
    {
        pParams->pSrc = pParams->pSrcComplementary;
        pParams->srcStep = srcStep;
        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        ippiReadDataBlockThroughBoundary_8u(iOverlappingType,
                                            pParams);

        pParams->pSrc = pTmp;
        pParams->srcStep = tmpStep;
        pParams->pDst = pParams->pDstComplementary;
        pParams->dstStep = dstStep;
    }

    /* call appropriate function */
    h264_interpolate_chroma_type_table_8u_b[pParams->iType](pParams);

    return ippStsNoErr;

} /* IppStatus ippiInterpolateBoundaryChromaBlock_H264_8u(Ipp32s iOverlappingType, */
#else // !NV12_INTERPOLATION

typedef struct H264InterpolationParams_8u
{
    const Ipp8u *pSrc;                                          /* (const Ipp8u *) pointer to the source memory */
    SIZE_T srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp8u *pDst;                                                /* (Ipp8u *) pointer to the destination memory */
    SIZE_T dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    const Ipp8u *pSrcComplementary;                             /* (const Ipp8u *) pointer to the complementary source memory */
    Ipp8u *pDstComplementary;                                   /* (Ipp8u *) pointer to the complementary destination memory */

} H264InterpolationParams_8u;

typedef void (*pH264Interpolation_8u) (H264InterpolationParams_8u *pParams);

#define h264_interpolate_chroma_type_table_nv12touv_8u h264_interpolate_chroma_type_table_nv12touv_8u_pxmx
#define read_data_through_boundary_table_nv12_8u read_data_through_boundary_table_nv12_8u_pxmx

static void h264_interpolate_chroma_type_nv12touv_00_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDstU = pParams->pDst;
    Ipp8u *pDstV = pParams->pDstComplementary;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            pDstU[x] = pSrc[2*x    ];
            pDstV[x] = pSrc[2*x + 1];
        }

        pSrc += srcStep;
        pDstU += dstStep;
        pDstV += dstStep;
    }

} /* void h264_interpolate_chroma_type_nv12touv_00_8u_px(H264InterpolationParams_8u *pParams) */

static void h264_interpolate_chroma_type_nv12touv_0x_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDstU = pParams->pDst;
    Ipp8u *pDstV = pParams->pDstComplementary;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iResU, iResV;

            iResU = pSrc[2*x] * (8 - pParams->hFraction) +
                pSrc[2*(x + 1)] * (pParams->hFraction) + 4;

            pDstU[x] = (Ipp8u) (iResU >> 3);
            //
            iResV = pSrc[2*x + 1      ] * (8 - pParams->hFraction) +
                pSrc[2*(x + 1) + 1] * (pParams->hFraction) + 4;

            pDstV[x] = (Ipp8u) (iResV >> 3);

        }

        pSrc += srcStep;
        pDstU += dstStep;
        pDstV += dstStep;
    }


}/*void h264_interpolate_chroma_type_nv12touv_0x_8u_px(H264InterpolationParams_8u *pParams)*/

static void h264_interpolate_chroma_type_nv12touv_y0_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDstU = pParams->pDst;
    Ipp8u *pDstV = pParams->pDstComplementary;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iResU, iResV;

            iResU = pSrc[2*x          ] * (8 - pParams->vFraction) +
                pSrc[2*x + srcStep] * (pParams->vFraction) + 4;

            pDstU[x] = (Ipp8u) (iResU >> 3);

            iResV = pSrc[2*x + 1          ] * (8 - pParams->vFraction) +
                pSrc[2*x + 1 + srcStep] * (pParams->vFraction) + 4;

            pDstV[x] = (Ipp8u) (iResV >> 3);

        }

        pSrc += srcStep;
        pDstU += dstStep;
        pDstV += dstStep;
    }
}/*void h264_interpolate_chroma_type_nv12touv_y0_8u_px(H264InterpolationParams_8u *pParams)*/

static void h264_interpolate_chroma_type_nv12touv_yx_8u_px(H264InterpolationParams_8u *pParams)
{
    const Ipp8u *pSrc = pParams->pSrc;
    SIZE_T srcStep = pParams->srcStep;
    Ipp8u *pDstU = pParams->pDst;
    Ipp8u *pDstV = pParams->pDstComplementary;
    SIZE_T dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iResU, iResV;

            iResU = (pSrc[2*x] * (8 - pParams->hFraction) +
                pSrc[2*x + 2] * (pParams->hFraction)) * (8 - pParams->vFraction) +
                (pSrc[2*x + srcStep] * (8 - pParams->hFraction) +
                pSrc[2*x + srcStep + 2] * (pParams->hFraction)) * (pParams->vFraction) + 32;

            pDstU[x] = (Ipp8u) (iResU >> 6);

            // for V
            iResV = (pSrc[2*x + 1] * (8 - pParams->hFraction) +
                pSrc[2*(x + 1) + 1] * (pParams->hFraction)) * (8 - pParams->vFraction) +
                (pSrc[2*x + 1 + srcStep] * (8 - pParams->hFraction) +
                pSrc[2*(x + 1) + srcStep + 1] * (pParams->hFraction)) * (pParams->vFraction) + 32;

            pDstV[x] = (Ipp8u) (iResV >> 6);
        }

        pSrc += srcStep;
        pDstU += dstStep;
        pDstV += dstStep;
    }


}/*void h264_interpolate_chroma_type_nv12touv_xy_8u_px(H264InterpolationParams_8u *pParams)*/

static pH264Interpolation_8u h264_interpolate_chroma_type_table_nv12touv_8u_pxmx[4] =
{
    &h264_interpolate_chroma_type_nv12touv_00_8u_px,
    &h264_interpolate_chroma_type_nv12touv_0x_8u_px,
    &h264_interpolate_chroma_type_nv12touv_y0_8u_px,
    &h264_interpolate_chroma_type_nv12touv_yx_8u_px

};

static void memset_nv12_8u(Ipp8u *pDst, Ipp8u* nVal, Ipp32s nNum)
{
    Ipp32s i;

    for (i = 0; i < nNum; i ++ )
    {
        pDst[2*i] = nVal[0];
        pDst[2*i + 1] = nVal[1];
    }

} /* void memset_nv12_8u(Ipp8u *pDst, Ipp8u* nVal, Ipp32s nNum) */

static void read_data_through_boundary_none_nv12_8u(H264InterpolationParams_8u *pParams)
{
    /* touch unreferenced parameter(s) */
    UNREFERENCED_PARAMETER(pParams);

    /* there is something wrong */

} /* void read_data_through_boundary_none_nv12_8u(H264InterpolationParams_8u *) */

static void read_data_through_boundary_left_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            //memset(pDst, pSrc[0], iIndent);
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);
            memset_nv12_8u(pDst, (Ipp8u *) pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_left_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            //memset(pDst + iIndent, pSrc[iIndent - 1], pParams->dataWidth - iIndent);
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent);
            memset_nv12_8u(pDst + 2*iIndent, (Ipp8u *)(&pSrc[2*iIndent - 2]), 2*(pParams->dataWidth - iIndent) );

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_left_nv12_8u_px(pParams);
    else
        read_data_through_boundary_right_nv12_8u_px(pParams);

} /* void read_data_through_boundary_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_left_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc;
        Ipp8u *pDst = pParams->pDst;
        Ipp8u *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        //memset(pDst, pSrc[0], iIndent);
        //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);
        memset_nv12_8u(pDst, (Ipp8u*)pSrc, iIndent);
        MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            //memset(pDst, pSrc[0], iIndent);
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);
            memset_nv12_8u(pDst, (Ipp8u*)pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_left_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp8u *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
        //memset(pDst + iIndent, pSrc[iIndent - 1], pParams->dataWidth - iIndent);
        MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent);
        memset_nv12_8u(pDst + 2*iIndent, (Ipp8u*)(&pSrc[2*iIndent - 2]), 2*(pParams->dataWidth - iIndent) );

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            //memset(pDst + iIndent, pSrc[iIndent - 1], pParams->dataWidth - iIndent);
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent);
            memset_nv12_8u(pDst + 2*iIndent, (Ipp8u*)(&pSrc[2*iIndent - 2]), 2*(pParams->dataWidth - iIndent) );

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

} /* void read_data_through_boundary_top_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_top_left_nv12_8u_px(pParams);
    else
        read_data_through_boundary_top_right_nv12_8u_px(pParams);

} /* void read_data_through_boundary_top_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;

        /* copy existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_left_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            //memset(pDst, pSrc[0], iIndent);
            //MFX_INTERNAL_CPY(pDst + iIndent, pSrc, pParams->dataWidth - iIndent);
            memset_nv12_8u(pDst, (Ipp8u*)pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_left_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const Ipp8u *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        Ipp8u *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            //MFX_INTERNAL_CPY(pDst, pSrc, iIndent);
            //memset(pDst + iIndent, pSrc[iIndent - 1], pParams->dataWidth - iIndent);
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent);
            memset_nv12_8u(pDst + 2*iIndent, (Ipp8u*)(&pSrc[2*iIndent - 2]), 2*(pParams->dataWidth - iIndent) );

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth);

            pDst += pParams->dstStep;
        }
    }

} /* void read_data_through_boundary_bottom_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_bottom_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_bottom_left_nv12_8u_px(pParams);
    else
        read_data_through_boundary_bottom_right_nv12_8u_px(pParams);

} /* void read_data_through_boundary_bottom_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_nv12_8u_px(pParams);
    else
        read_data_through_boundary_bottom_nv12_8u_px(pParams);

} /* void read_data_through_boundary_top_top_bottom_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_left_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_left_nv12_8u_px(pParams);
    else
        read_data_through_boundary_bottom_left_nv12_8u_px(pParams);

} /* void read_data_through_boundary_top_bottom_left_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_right_nv12_8u_px(pParams);
    else
        read_data_through_boundary_bottom_right_nv12_8u_px(pParams);

} /* void read_data_through_boundary_top_bottom_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static void read_data_through_boundary_top_bottom_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams)
{
    if (0 > pParams->xPos)
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_left_nv12_8u_px(pParams);
        else
            read_data_through_boundary_bottom_left_nv12_8u_px(pParams);
    }
    else
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_right_nv12_8u_px(pParams);
        else
            read_data_through_boundary_bottom_right_nv12_8u_px(pParams);
    }

} /* void read_data_through_boundary_top_bottom_left_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

static pH264Interpolation_8u read_data_through_boundary_table_nv12_8u_pxmx[16] =
{
    &read_data_through_boundary_none_nv12_8u,
    &read_data_through_boundary_left_nv12_8u_px,
    &read_data_through_boundary_right_nv12_8u_px,
    &read_data_through_boundary_left_right_nv12_8u_px,

    &read_data_through_boundary_top_nv12_8u_px,
    &read_data_through_boundary_top_left_nv12_8u_px,
    &read_data_through_boundary_top_right_nv12_8u_px,
    &read_data_through_boundary_top_left_right_nv12_8u_px,

    &read_data_through_boundary_bottom_nv12_8u_px,
    &read_data_through_boundary_bottom_left_nv12_8u_px,
    &read_data_through_boundary_bottom_right_nv12_8u_px,
    &read_data_through_boundary_bottom_left_right_nv12_8u_px,

    &read_data_through_boundary_top_bottom_nv12_8u_px,
    &read_data_through_boundary_top_bottom_left_nv12_8u_px,
    &read_data_through_boundary_top_bottom_right_nv12_8u_px,
    &read_data_through_boundary_top_bottom_left_right_nv12_8u_px

};

static IppStatus ippiInterpolateBoundaryChromaBlock_NV12_H264_C2P2_8u(Ipp32s iOverlappingType,
                                                               H264InterpolationParams_8u *pParams);

static IppStatus ippiInterpolateChromaBlock_NV12_H264_8u_C2P2R(const IppVCInterpolateBlock_8u *interpolateInfo)
{
    H264InterpolationParams_8u params;

#if 0
    /* check error(s) */
    IPP_BAD_PTR1_RET(interpolateInfo);
    IPP_BAD_PTR2_RET(interpolateInfo->pSrc[0],
        interpolateInfo->pDst[0]);
    IPP_BADARG_RET( ( interpolateInfo->sizeBlock.height & 1 ) |
        ( interpolateInfo->sizeBlock.width & ~0x0e ),  ippStsSizeErr);
#endif

    /* prepare pointers */
    params.dstStep = interpolateInfo->dstStep;
    params.srcStep = interpolateInfo->srcStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        SIZE_T iRefOffset;

        /* set the current reference offset */
        iRefOffset = interpolateInfo->pointBlockPos.y * params.srcStep +
            interpolateInfo->pointBlockPos.x*2;

        /* set the current source pointer */
        params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
        params.pDst = interpolateInfo->pDst[0];
        params.pDstComplementary = interpolateInfo->pDst[1];
        /* call optimized function from the table */
        //#if ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7))
        //        h264_interpolate_chroma_type_table_8u[params.blockWidth & 0x0c](&params);
        //#else /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
        h264_interpolate_chroma_type_table_nv12touv_8u[0](&params);
        //#endif /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = interpolateInfo->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            params.hFraction =
                hFraction = interpolateInfo->pointVector.x & 7;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                params.xPos = interpolateInfo->pointBlockPos.x +
                    (pointVector.x >>= 3);
                params.dataWidth = params.blockWidth + iAddition;
            }

            /* prepare vertical vector values */
            params.vFraction =
                vFraction = pointVector.y & 7;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                params.yPos = interpolateInfo->pointBlockPos.y +
                    (pointVector.y >>= 3);
                params.dataHeight = params.blockHeight + iAddition;
            }

            //#if ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7))
            //            params.iType = (params.blockWidth & 0x0c) |
            //                           (((vFraction) ? (1) : (0)) << 1) |
            //                           ((hFraction) ? (1) : (0));
            //#else /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
            params.iType = (((vFraction) ? (1) : (0)) << 1) |
                ((hFraction) ? (1) : (0));
            //#endif /* ((_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)) */
        }


        /*  call the appropriate function
        we select the function using the following rules:
        zero bit means x position of left data boundary lie out of frame,
        first bit means x position of right data boundary lie out of frame,
        second bit means y position of top data boundary lie out of frame,
        third bit means y position of bottom data boundary lie out of frame */
        iOverlappingType = 0;
        iOverlappingType |= ((0 > params.xPos) ? (1) : (0)) << 0;
        iOverlappingType |= ((interpolateInfo->sizeFrame.width < params.xPos + params.dataWidth) ? (1) : (0)) << 1;
        iOverlappingType |= ((0 > params.yPos) ? (1) : (0)) << 2;
        iOverlappingType |= ((interpolateInfo->sizeFrame.height < params.yPos + params.dataHeight) ? (1) : (0)) << 3;

        /* call appropriate function */
        if (0 == iOverlappingType)
        {
            SIZE_T iRefOffset;

            /* set the current reference offset */
            iRefOffset = (interpolateInfo->pointBlockPos.y + pointVector.y) * params.srcStep +
                (interpolateInfo->pointBlockPos.x*2 + pointVector.x*2);

            /* set the current source pointer */
            params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
            params.pDst = interpolateInfo->pDst[0];
            params.pDstComplementary = interpolateInfo->pDst[1];
            /* call optimized function from the table */
            h264_interpolate_chroma_type_table_nv12touv_8u[params.iType](&params);

            return ippStsNoErr;
        }
        else
        {
            /* save additional parameters */
            params.frameSize.width = interpolateInfo->sizeFrame.width;
            params.frameSize.height = interpolateInfo->sizeFrame.height;

            params.pSrc = interpolateInfo->pSrc[0];
            params.pDst = interpolateInfo->pDst[0];
            params.pDstComplementary = interpolateInfo->pDst[1];

            /* there is something wrong, try to work through a temporal buffer */
            return ippiInterpolateBoundaryChromaBlock_NV12_H264_C2P2_8u(iOverlappingType,
                &params);
        }
    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiInterpolateChromaBlock_NV12_H264_8u_C2P2R, (const IppVCInterpolateBlock_8u *interpolateInfo)) */

#define PREPARE_TEMPORAL_BUFFER_NV12_8U(ptr, step, buf) \
    /* align pointer */ \
    ptr = ((Ipp8u *) buf) + ((16 - (((Ipp8u *) buf) - (Ipp8u *) 0)) & 15); \
    /* set step */ \
    step = (pParams->dataWidth + 15) & -16;

static IppStatus ippiReadDataBlockThroughBoundary_NV12_8u(Ipp32s iOverlappingType,
                                                   H264InterpolationParams_8u *pParams)
{
    /*  this is an internal function to read data,
    which is overlapping with frame boundary.
    Bits of iOverlappingType mean following:
    zero bit means x position of left data boundary lie out of frame,
    first bit means x position of right data boundary lie out of frame,
    second bit means y position of top data boundary lie out of frame,
    third bit means y position of bottom data boundary lie out of frame */

    /* call appropriate function */
    read_data_through_boundary_table_nv12_8u[iOverlappingType](pParams);

    return ippStsNoErr;

} /* IppStatus ippiReadDataBlockThroughBoundary_8u(Ipp32s iOverlappingType, */

static IppStatus ippiInterpolateBoundaryChromaBlock_NV12_H264_C2P2_8u(Ipp32s iOverlappingType,
                                                               H264InterpolationParams_8u *pParams)
{
    Ipp8u bTmp[2*16 * 17 + 16];
    Ipp8u *pTmp;
    Ipp32s tmpStep;
    SIZE_T srcStep;
    Ipp8u *pDst;
    SIZE_T dstStep;

    /* prepare temporal buffer */
    PREPARE_TEMPORAL_BUFFER_NV12_8U(pTmp, tmpStep, bTmp);

    /* read data into temporal buffer */
    {
        pDst = pParams->pDst;
        dstStep = pParams->dstStep;
        srcStep = pParams->srcStep;

        pParams->pDst = pTmp;
        //pParams->dstStep = tmpStep;
        // pitch is const and always 32 = 0x20
        pParams->dstStep = 32;

        ippiReadDataBlockThroughBoundary_NV12_8u(iOverlappingType,
            pParams);

        pParams->pSrc = pTmp;
        //pParams->srcStep = tmpStep;
        // pitch is const and always 32 = 0x20
        pParams->srcStep = 32;
        pParams->pDst = pDst;
        pParams->dstStep = dstStep;
        // Plane pParams->pDstComplementary without changes
    }

    /* call appropriate function */
    h264_interpolate_chroma_type_table_nv12touv_8u[pParams->iType](pParams);

    return ippStsNoErr;

} /* IppStatus ippiInterpolateBoundaryChromaBlock_H264_8u(Ipp32s iOverlappingType, */
#endif // !NV12_INTERPOLATION
#endif // NO_PADIING && USE_NV12


#ifdef NO_PADDING
inline void ippiInterpolateChromaBlock_H264_8u16s(IppVCInterpolateBlock_8u * interpolateInfo)
{
#ifdef USE_NV12
#ifdef NV12_INTERPOLATION
    ippiInterpolateChromaBlock_NV12_H264_8u_C2P2R(interpolateInfo);
#else // NV12_INTERPOLATION
    ippiInterpolateChromaBlock_H264_8u_P2R_NV12(interpolateInfo);
#endif // NV12_INTERPOLATION
#else
    ippiInterpolateChromaBlock_H264_8u_P2R(interpolateInfo);
#endif
}
#endif // NO_PADDING

inline void ippiInterpolateLumaBottom_H264_8u16s(
    const Ipp8u* src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    Ipp32s   outPixels,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    ippiInterpolateLumaBottom_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, outPixels, sz);
}

inline void ippiInterpolateChroma_H264_8u16s(
    const Ipp8u* src,
    Ipp32s src_pitch,
#ifdef USE_NV12
    Ipp8u *pDstU,
    Ipp8u *pDstV,
#else // USE_NV12
    Ipp8u* dst,
#endif // USE_NV12
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    IppiSize sz,
    Ipp32s bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    H264ENC_UNREFERENCED_PARAMETER(sz);
    H264ENC_UNREFERENCED_PARAMETER(xh);
    H264ENC_UNREFERENCED_PARAMETER(yh);
    H264ENC_UNREFERENCED_PARAMETER(dst_pitch);
    H264ENC_UNREFERENCED_PARAMETER(src);
    H264ENC_UNREFERENCED_PARAMETER(src_pitch);

#if defined(USE_NV12) && !defined(NO_PADDING)
    ippiInterpolateChroma_H264_8u_C2P2R(src, src_pitch, pDstU, pDstV, dst_pitch, xh, yh, sz);
#else // USE_NV12 && !NO_PADDING
#ifndef USE_NV12
    ippiInterpolateChroma_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, sz);
//#else // !USE_NV12
//    ippiInterpolateChroma_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, sz);
#endif // !USE_NV12
#endif // USE_NV12 && !NO_PADDING
}

inline void ippiInterpolateChromaTop_H264_8u16s(
    const Ipp8u* src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    Ipp32s   outPixels,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    ippiInterpolateChromaTop_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, outPixels, sz);
}

inline void ippiInterpolateChromaBottom_H264_8u16s(
    const Ipp8u* src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    Ipp32s   outPixels,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    H264ENC_UNREFERENCED_PARAMETER(bit_depth);
    ippiInterpolateChromaBottom_H264_8u_C1R(src, src_pitch, dst, dst_pitch, xh, yh, outPixels, sz);
}

inline void ippiInterpolateBlock_H264_8u16s(
    const Ipp8u* pSrc1,
    const Ipp8u* pSrc2,
    Ipp8u* pDst,
    Ipp32s width,
    Ipp32s height,
    Ipp32s pitchPixels)
{
    ippiInterpolateBlock_H264_8u_P2P1R(const_cast<Ipp8u*>(pSrc1), const_cast<Ipp8u*>(pSrc2), pDst, width, height, pitchPixels);
}

inline void ippiInterpolateBlock_H264_A_8u16s(
    const Ipp8u* pSrc1,
    const Ipp8u* pSrc2,
    Ipp8u* pDst,
    Ipp32s width,
    Ipp32s height,
    Ipp32s pitchPix1,
    Ipp32s pitchPix2,
    Ipp32s pitchPix3)
{
    ippiInterpolateBlock_H264_8u_P3P1R(pSrc1, pSrc2, pDst, width, height, pitchPix1, pitchPix2, pitchPix3);
}

inline void ippiEncodeCoeffsCAVLC_H264_8u16s(
    Ipp16s* pSrc,
    Ipp8u   AC,
    const Ipp32s* pScanMatrix,
    Ipp8u   Count,
    Ipp8u*  Trailing_Ones,
    Ipp8u*  Trailing_One_Signs,
    Ipp8u*  NumOutCoeffs,
    Ipp8u*  TotalZeroes,
    Ipp16s* Levels,
    Ipp8u*  Runs)
{
    ippiEncodeCoeffsCAVLC_H264_16s(pSrc, AC, pScanMatrix, Count, Trailing_Ones, Trailing_One_Signs, NumOutCoeffs, TotalZeroes, Levels, Runs);
}

inline void ippiSumsDiff16x16Blocks4x4_8u16s(
    Ipp8u* pSrc,
    Ipp32s srcStepPixels,
    Ipp8u* pPred,
    Ipp32s predStepPixels,
    Ipp16s* pDC,
    Ipp16s* pDiff)
{
    ippiSumsDiff16x16Blocks4x4_8u16s_C1(pSrc, srcStepPixels*sizeof(Ipp8u), pPred, predStepPixels*sizeof(Ipp8u), pDC, pDiff);
}

inline void ippiTransformQuantLumaDC_H264_8u16s(
    Ipp16s* pDCBuf,
    Ipp16s* pQBuf,
    Ipp32s QP,
    Ipp32s* iNumCoeffs,
    Ipp32s intra,
    const Ipp16s* scan,
    Ipp32s* iLastCoeff,
    const Ipp16s* pLevelScale/* = NULL*/)
{
    ippiTransformQuantFwdLumaDC4x4_H264_16s_C1I(pDCBuf,pQBuf,QP,iNumCoeffs,intra, scan,iLastCoeff, pLevelScale);
}

inline void ippiEdgesDetect16x16_8u16s(
    const Ipp8u *pSrc,
    Ipp32s srcStepPixels,
    Ipp32s EdgePelDifference,
    Ipp32s EdgePelCount,
    Ipp32s *pRes)
{
    Ipp8u uRes;
    ippiEdgesDetect16x16_8u_C1R(pSrc, srcStepPixels, (Ipp8u)EdgePelDifference,(Ipp8u)EdgePelCount,&uRes);
    *pRes = uRes;
}

#if defined BITDEPTH_9_12
inline void ippiTransformDequantLumaDC_H264_16u32s(
    Ipp32s* pSrcDst,
    Ipp32s QP,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantInvLumaDC4x4_H264_32s_C1I(pSrcDst, QP, pScaleLevels);
}

inline void ippiTransformDequantChromaDC_H264_16u32s(
    Ipp32s* pSrcDst,
    Ipp32s QP,
    const Ipp16s *pLevelScale/* = NULL*/)
{
    ippiTransformQuantInvChromaDC2x2_H264_32s_C1I (pSrcDst, QP, pLevelScale);
}

inline void ippiDequantTransformResidualAndAdd_H264_16u32s(
    const Ipp16u* pPred,
    Ipp32s* pSrcDst,
    const Ipp32s* pDC,
    Ipp16u* pDst,
    Ipp32s  PredStep,
    Ipp32s  DstStep,
    Ipp32s  QP,
    Ipp32s  AC,
    Ipp32s  bit_depth,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantInvAddPred4x4_H264_32s_C1IR (pPred, PredStep*sizeof(Ipp16s), pSrcDst, pDC, pDst, DstStep*sizeof(Ipp16u), QP, AC, bit_depth, pScaleLevels);
}

inline void ippiTransformLuma8x8Fwd_H264_16u32s(
    const Ipp16s* pDiffBuf,
    Ipp32s* pTransformResult)
{
    ippiTransformFwdLuma8x8_H264_16s32s_C1(pDiffBuf, pTransformResult);
}

inline void ippiQuantLuma8x8_H264_16u32s(
    const Ipp32s* pSrc,
    Ipp32s* pDst,
    Ipp32s  Qp6,
    Ipp32s  intra,
    const Ipp16s* pScanMatrix,
    const Ipp16s* pScaleLevels,
    Ipp32s* pNumLevels,
    Ipp32s* pLastCoeff,
    const H264Slice_16u32s * curr_slice/* = NULL*/,
    TrellisCabacState* cbSt/* = NULL*/,
    const Ipp16s* pInvLevelScale/* = NULL*/)
{
    curr_slice;
    cbSt;
    pInvLevelScale;
    ippiQuantLuma8x8_H264_32s_C1(pSrc, pDst,Qp6,intra,pScanMatrix,pScaleLevels,pNumLevels,pLastCoeff);
}

inline void ippiQuantLuma8x8Inv_H264_16u32s(
    Ipp32s* pSrcDst,
    Ipp32s Qp6,
    const Ipp16s* pInvLevelScale)
{
    ippiQuantInvLuma8x8_H264_32s_C1I(pSrcDst, Qp6, pInvLevelScale);
}

inline void ippiTransformLuma8x8InvAddPred_H264_16u32s(
    const Ipp16u* pPred,
    Ipp32s PredStepPixels,
    Ipp32s* pSrcDst,
    Ipp16u* pDst,
    Ipp32s DstStepPixels,
    Ipp32s bit_depth)

{
    ippiTransformInvAddPredLuma8x8_H264_32s16u_C1R(pPred, PredStepPixels*sizeof(Ipp16u), pSrcDst, pDst, DstStepPixels*sizeof(Ipp16u), bit_depth);
}

inline void ippiEncodeChromaDcCoeffsCAVLC_H264_16u32s(
    const Ipp32s* pSrc,
    Ipp8u*  pTrailingOnes,
    Ipp8u*  pTrailingOneSigns,
    Ipp8u*  pNumOutCoeffs,
    Ipp8u*  pTotalZeroes,
    Ipp32s* pLevels,
    Ipp8u*  pRuns)
{
    ippiEncodeCoeffsCAVLCChromaDC2x2_H264_32s(pSrc,pTrailingOnes, pTrailingOneSigns, pNumOutCoeffs, pTotalZeroes, pLevels, pRuns);
}

inline void ippiEncodeChroma422DC_CoeffsCAVLC_H264_16u32s(
    const Ipp32s *pSrc,
    Ipp8u *Trailing_Ones,
    Ipp8u *Trailing_One_Signs,
    Ipp8u *NumOutCoeffs,
    Ipp8u *TotalZeros,
    Ipp32s *Levels,
    Ipp8u *Runs)
{
    ippiEncodeCoeffsCAVLCChromaDC2x4_H264_32s (pSrc, Trailing_Ones, Trailing_One_Signs, NumOutCoeffs, TotalZeros, Levels, Runs);
}

inline void ippiTransformQuantChromaDC_H264_16u32s(
    Ipp32s* pSrcDst,
    Ipp32s* pTBlock,
    Ipp32s  QPChroma,
    Ipp32s* pNumLevels,
    Ipp32s  intra,
    Ipp32s  needTransform,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantFwdChromaDC2x2_H264_32s_C1I(pSrcDst, pTBlock, QPChroma, pNumLevels, intra, needTransform, pScaleLevels);
}

IppStatus TransformQuantOptFwd4x4_H264_16s32s_C1(
    Ipp16s* pSrc,
    Ipp32s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  intra,
    const Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    const Ipp16s* pLevelScales,
    const H264Slice_16u32s* curr_slice,
    Ipp32s sCoeff,
    TrellisCabacState* states);


inline void ippiTransformQuantResidual_H264_16u32s(
    Ipp16s* pSrc,
    Ipp32s* pDst,
    Ipp32s  Qp,
    Ipp32s* pNumLevels,
    Ipp32s  intra,
    const Ipp16s* pScanMatrix,
    Ipp32s* pLastCoeff,
    const Ipp16s* pLevelScales/* = NULL*/,
    const H264Slice_16u32s* curr_slice/* = NULL*/,
    Ipp32s sCoeff/* = 0*/,
    TrellisCabacState* states/* = NULL*/)
{
    curr_slice;
    sCoeff;
    states;
    ippiTransformQuantFwd4x4_H264_16s32s_C1(pSrc, pDst, Qp, pNumLevels, intra, pScanMatrix,pLastCoeff, pLevelScales);
}

inline void ippiInterpolateBlock_H264_16u32s(
    const Ipp16u* pSrc1,
    const Ipp16u* pSrc2,
    Ipp16u* pDst,
    Ipp32s  width,
    Ipp32s  height,
    Ipp32s  pitchPixels)
{
    IppVCBidir_16u info;
    info.pSrc1 = (Ipp16u*)pSrc1;
    info.pSrc2 = (Ipp16u*)pSrc2;
    info.pDst = pDst;
    info.dstStep = info.srcStep2 = info.srcStep1 = pitchPixels;
    info.roiSize.width = width;
    info.roiSize.height = height;
    ippiBidir_H264_16u_P2P1R( &info );
}

inline void ippiInterpolateBlock_H264_A_16u32s(
    const Ipp16u* pSrc1,
    const Ipp16u* pSrc2,
    Ipp16u* pDst,
    Ipp32s  width,
    Ipp32s  height,
    Ipp32s pitchPix1,
    Ipp32s pitchPix2,
    Ipp32s pitchPix3)
{
    IppVCBidir_16u info;
    info.pSrc1 = (Ipp16u*)pSrc1;
    info.pSrc2 = (Ipp16u*)pSrc2;
    info.pDst = pDst;
    info.dstStep = pitchPix3;
    info.srcStep2 = pitchPix2;
    info.srcStep1 = pitchPix1;
    info.roiSize.width = width;
    info.roiSize.height = height;
    ippiBidir_H264_16u_P2P1R( &info );
}

inline void ippiEncodeCoeffsCAVLC_H264_16u32s(
    const Ipp32s* pSrc,
    Ipp8u AC,
    const Ipp32s* pScanMatrix,
    Ipp32s Count,
    Ipp8u* Trailing_Ones,
    Ipp8u* Trailing_One_Signs,
    Ipp8u* NumOutCoeffs,
    Ipp8u* TotalZeroes,
    Ipp32s* Levels,
    Ipp8u* Runs)
{
    ippiEncodeCoeffsCAVLC_H264_32s(pSrc, AC, pScanMatrix, Count, Trailing_Ones, Trailing_One_Signs, NumOutCoeffs, TotalZeroes, Levels, Runs);
}

inline void ippiSumsDiff16x16Blocks4x4_16u32s(
    const Ipp16u* pSrc,
    Ipp32s srcStepPixels,
    const Ipp16u* pPred,
    Ipp32s predStepPixels,
    Ipp32s* pDC,
    Ipp16s* pDiff)
{
    ippiSumsDiff16x16Blocks4x4_16u32s_C1R(pSrc, srcStepPixels*sizeof(Ipp16s), pPred, predStepPixels*sizeof(Ipp16s), pDC, pDiff);
}

inline void ippiTransformQuantLumaDC_H264_16u32s(
    Ipp32s* pDCBuf,
    Ipp32s* pQBuf,
    Ipp32s QP,
    Ipp32s* iNumCoeffs,
    Ipp32s intra,
    const Ipp16s* scan,
    Ipp32s* iLastCoeff,
    const Ipp16s* pLevelScale/* = NULL*/)
{
    ippiTransformQuantFwdLumaDC4x4_H264_32s_C1I (pDCBuf,pQBuf,QP,iNumCoeffs,intra, scan,iLastCoeff, pLevelScale);
}

inline void ippiInterpolateLuma_H264_16u32s(
    const Ipp16u* src,
    Ipp32s   src_pitch,
    Ipp16u*  dst,
    Ipp32s   dst_pitch,
    Ipp32s   xh,
    Ipp32s   yh,
    IppiSize sz,
    Ipp32s   bit_depth)
{
    //    IppVCInterpolate_16u info = { (Ipp16u*)src,src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth };
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateLuma_H264_16u_C1R(&info);
}

#ifdef NO_PADDING
inline void ippiInterpolateLumaBlock_H264_16u32s(
    IppVCInterpolateBlock_16u * interpolateInfo
    /*const Ipp8u*   src,
    Ipp32s   src_pitch,
    Ipp8u*   dst,
    Ipp32s   dst_pitch,
    Ipp32s   mvx,
    Ipp32s   mvy,
    Ipp32s   blkx,
    Ipp32s   blky,
    IppiSize frmsz,
    IppiSize blksz*/)
{
    ippiInterpolateLumaBlock_H264_16u_P1R(interpolateInfo);
}
#endif //NO_PADDING

inline void ippiInterpolateLumaTop_H264_16u32s(
    const Ipp16u* src,
    Ipp32s    src_pitch,
    Ipp16u*   dst,
    Ipp32s    dst_pitch,
    Ipp32s    xh,
    Ipp32s    yh,
    Ipp32s    outPixels,
    IppiSize  sz,
    Ipp32s    bit_depth)
{
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateLumaTop_H264_16u_C1R( &info, outPixels );
}

inline void ippiInterpolateLumaBottom_H264_16u32s(
    const Ipp16u* src,
    Ipp32s src_pitch,
    Ipp16u* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    Ipp32s outPixels,
    IppiSize sz,
    Ipp32s bit_depth)
{
    //    IppVCInterpolate_16u info = { (Ipp16u*)src, src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth };
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateLumaBottom_H264_16u_C1R( &info, outPixels );
}

inline void ippiTransformDequantChromaDC422_H264_16u32s(
    Ipp32s *pSrcDst,
    Ipp32s QPChroma,
    Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantInvChromaDC2x4_H264_32s_C1I(pSrcDst, QPChroma, pScaleLevels);
}

inline void ippiTransformQuantChroma422DC_H264_16u32s(
    Ipp32s *pDCBuf,
    Ipp32s *pTBuf,
    Ipp32s QPChroma,
    Ipp32s* NumCoeffs,
    Ipp32s Intra,
    Ipp32s NeedTransform,
    const Ipp16s* pScaleLevels/* = NULL*/)
{
    ippiTransformQuantFwdChromaDC2x4_H264_32s_C1I(pDCBuf, pTBuf, QPChroma, NumCoeffs, Intra, NeedTransform, pScaleLevels);
}

inline void ippiSumsDiff8x8Blocks4x4_16u32s(
    const Ipp16u* pSrc,
    Ipp32s  srcStepPixels,
    const Ipp16u* pPred,
    Ipp32s  predStepPixels,
    Ipp32s* pDC,
    Ipp16s* pDiff)
{
    ippiSumsDiff8x8Blocks4x4_16u32s_C1R(pSrc, srcStepPixels*sizeof(Ipp16u), pPred, predStepPixels*sizeof(Ipp16u), pDC, pDiff);
}

inline void ippiInterpolateChromaBottom_H264_16u32s(
    const Ipp16u* src,
    Ipp32s src_pitch,
    Ipp16u* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    Ipp32s outPixels,
    IppiSize sz,
    Ipp32s bit_depth)
{
    //    IppVCInterpolate_16u info = { (Ipp16u*)src, src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth };
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateChromaBottom_H264_16u_C1R( &info, outPixels );
}

inline void ippiInterpolateChromaTop_H264_16u32s(
    const Ipp16u* src,
    Ipp32s src_pitch,
    Ipp16u* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    Ipp32s outPixels,
    IppiSize sz,
    Ipp32s bit_depth)
{
    //    IppVCInterpolate_16u info = { (Ipp16u*)src, src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth };
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateChromaTop_H264_16u_C1R( &info, outPixels );
}

inline void ippiEdgesDetect16x16_16u32s(
    const Ipp16u* pSrc,
    Ipp32s srcStepPixels,
    Ipp32s EdgePelDifference,
    Ipp32s EdgePelCount,
    Ipp32s* pRes)
{
    ippiEdgesDetect16x16_16u_C1R(pSrc, srcStepPixels * sizeof(Ipp16u), EdgePelDifference, EdgePelCount, pRes);
}

inline void ippiInterpolateChroma_H264_16u32s(
    const Ipp16u* src,
    Ipp32s src_pitch,
    Ipp16u* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    IppiSize sz,
    Ipp32s bit_depth)
{
//    IppVCInterpolate_16u info = { (Ipp16u*)src,src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth };
    IppVCInterpolate_16u info;
    info.pSrc = (Ipp16u*)src;
    info.srcStep = src_pitch;
    info.pDst = dst;
    info.dstStep = dst_pitch;
    info.dx = xh;
    info.dy = yh;
    info.roiSize.width = sz.width;
    info.roiSize.height = sz.height;
    info.bitDepth = bit_depth;
    ippiInterpolateChroma_H264_16u_C1R( &info );
}

#ifdef NO_PADDING
inline void ippiInterpolateChromaBlock_H264_16u32s(
    IppVCInterpolateBlock_16u * interpolateInfo)
{
    ippiInterpolateChromaBlock_H264_16u_P2R(interpolateInfo);
}
#endif // NO_PADDING

#endif // BITDEPTH_9_12

#endif // UMC_H264_TO_IPP_H

#endif //UMC_ENABLE_H264_VIDEO_ENCODER

