//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

#include <vector>

namespace MFX_PP
{

void ippiInterpolateChromaBlock(H264InterpolationParams_8u *interpolateInfo, Ipp8u *temporary_buffer);
void ippiInterpolateChromaBlock(H264InterpolationParams_16u *interpolateInfo, Ipp16u *temporary_buffer);

typedef void (*pH264Interpolation_8u) (H264InterpolationParams_8u *pParams);
typedef void (*pH264Interpolation_16u) (H264InterpolationParams_16u *pParams);

template <typename T, typename InterpolateStruct>
void h264_interpolate_chroma_type_nv12_00_16u_px(InterpolateStruct *pParams)
{
    const T *pSrc = (const T *)pParams->pSrc;
    size_t srcStep = pParams->srcStep;
    T *pDst = (T *)pParams->pDst;
    size_t dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            pDst[2*x    ] = pSrc[2*x    ];
            pDst[2*x + 1] = pSrc[2*x + 1];
        }

        pSrc += srcStep;
        pDst += dstStep;
    }

} /* void h264_interpolate_chroma_type_nv12_00_8u_px(H264InterpolationParams_8u *pParams) */

template <typename T, typename InterpolateStruct>
void h264_interpolate_chroma_type_nv12_0x_16u_px(InterpolateStruct *pParams)
{
    const T *pSrc = (const T *)pParams->pSrc;
    size_t srcStep = pParams->srcStep;
    T *pDst = (T *)pParams->pDst;
    size_t dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iResU, iResV;

            iResU = pSrc[2*x] * (8 - pParams->hFraction) +
                    pSrc[2*(x + 1)] * (pParams->hFraction) + 4;

            pDst[2*x] = (T) (iResU >> 3);
            //
            iResV = pSrc[2*x + 1      ] * (8 - pParams->hFraction) +
                    pSrc[2*(x + 1) + 1] * (pParams->hFraction) + 4;

            pDst[2*x + 1] = (T) (iResV >> 3);

        }

        pSrc += srcStep;
        pDst += dstStep;
    }


}/*void h264_interpolate_chroma_type_nv12_0x_8u_px(H264InterpolationParams_8u *pParams)*/

template <typename T, typename InterpolateStruct>
void h264_interpolate_chroma_type_nv12_y0_16u_px(InterpolateStruct *pParams)
{
    const T *pSrc = (const T *)pParams->pSrc;
    size_t srcStep = pParams->srcStep;
    T *pDst = (T *)pParams->pDst;
    size_t dstStep = pParams->dstStep;
    Ipp32s x, y;

    for (y = 0; y < pParams->blockHeight; y += 1)
    {
        for (x = 0; x < pParams->blockWidth; x += 1)
        {
            Ipp32s iResU, iResV;

            iResU = pSrc[2*x          ] * (8 - pParams->vFraction) +
                    pSrc[2*x + srcStep] * (pParams->vFraction) + 4;

            pDst[2*x] = (T) (iResU >> 3);

            iResV = pSrc[2*x + 1          ] * (8 - pParams->vFraction) +
                    pSrc[2*x + 1 + srcStep] * (pParams->vFraction) + 4;

            pDst[2*x + 1] = (T) (iResV >> 3);

        }

        pSrc += srcStep;
        pDst += dstStep;
    }
}/*void h264_interpolate_chroma_type_nv12_y0_8u_px(H264InterpolationParams_8u *pParams)*/

template <typename T, typename InterpolateStruct>
void h264_interpolate_chroma_type_nv12_yx_16u_px(InterpolateStruct *pParams)
{
    const T *pSrc = (const T *)pParams->pSrc;
    size_t srcStep = pParams->srcStep;
    T *pDst = (T *)pParams->pDst;
    size_t dstStep = pParams->dstStep;
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

            pDst[2*x] = (T) (iResU >> 6);

            // for V
            iResV = (pSrc[2*x + 1] * (8 - pParams->hFraction) +
                    pSrc[2*(x + 1) + 1] * (pParams->hFraction)) * (8 - pParams->vFraction) +
                   (pSrc[2*x + 1 + srcStep] * (8 - pParams->hFraction) +
                    pSrc[2*(x + 1) + srcStep + 1] * (pParams->hFraction)) * (pParams->vFraction) + 32;

            pDst[2*x + 1] = (T) (iResV >> 6);


        }

        pSrc += srcStep;
        pDst += dstStep;
    }


}/*void h264_interpolate_chroma_type_nv12_xy_8u_px(H264InterpolationParams_8u *pParams)*/


////////////////////////

pH264Interpolation_8u h264_interpolate_chroma_type_table_8u_pxmx[4] =
{
    &h264_interpolate_chroma_type_nv12_00_16u_px<Ipp8u, H264InterpolationParams_8u>,
    &h264_interpolate_chroma_type_nv12_0x_16u_px<Ipp8u, H264InterpolationParams_8u>,
    &h264_interpolate_chroma_type_nv12_y0_16u_px<Ipp8u, H264InterpolationParams_8u>,
    &h264_interpolate_chroma_type_nv12_yx_16u_px<Ipp8u, H264InterpolationParams_8u>
};

pH264Interpolation_16u h264_interpolate_chroma_type_table_16u_pxmx[4] =
{
    &h264_interpolate_chroma_type_nv12_00_16u_px<Ipp16u, H264InterpolationParams_16u>,
    &h264_interpolate_chroma_type_nv12_0x_16u_px<Ipp16u, H264InterpolationParams_16u>,
    &h264_interpolate_chroma_type_nv12_y0_16u_px<Ipp16u, H264InterpolationParams_16u>,
    &h264_interpolate_chroma_type_nv12_yx_16u_px<Ipp16u, H264InterpolationParams_16u>
};

template<typename Plane>
void ippiInterpolateChromaBlock_H264_kernel (H264InterpolationParams_16u *params)
{
    /* the most probably case - just copying */
    if (0 == (params->pointVector.x | params->pointVector.y))
    {
        Ipp32u iRefOffset;

        /* set the current reference offset */
        iRefOffset = params->pointBlockPos.y * params->srcStep +
                     params->pointBlockPos.x*2;

        /* set the current source pointer */
        params->pSrc += iRefOffset;
        /* call optimized function from the table */
        h264_interpolate_chroma_type_table_16u_pxmx[0](params);
    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = params->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            params->hFraction = hFraction = params->pointVector.x & 7;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                params->xPos = params->pointBlockPos.x +
                              (pointVector.x >>= 3);
                params->dataWidth = params->blockWidth + iAddition;
            }

            /* prepare vertical vector values */
            params->vFraction = vFraction = pointVector.y & 7;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                params->yPos = params->pointBlockPos.y +
                              (pointVector.y >>= 3);
                params->dataHeight = params->blockHeight + iAddition;
            }

            params->iType = (((vFraction) ? (1) : (0)) << 1) |
                           ((hFraction) ? (1) : (0));
        }

        /*  call the appropriate function
            we select the function using the following rules:
            zero bit means x position of left data boundary lie out of frame,
            first bit means x position of right data boundary lie out of frame,
            second bit means y position of top data boundary lie out of frame,
            third bit means y position of bottom data boundary lie out of frame */
        iOverlappingType = 0;
        iOverlappingType |= ((0 > params->xPos) ? (1) : (0)) << 0;
        iOverlappingType |= ((params->frameSize.width < params->xPos + params->dataWidth) ? (1) : (0)) << 1;
        iOverlappingType |= ((0 > params->yPos) ? (1) : (0)) << 2;
        iOverlappingType |= ((params->frameSize.height < params->yPos + params->dataHeight) ? (1) : (0)) << 3;

        /* call appropriate function */
        if (0 == iOverlappingType)
        {
            Ipp32u iRefOffset;

            /* set the current reference offset */
            iRefOffset = (params->pointBlockPos.y + pointVector.y) * params->srcStep +
                         (params->pointBlockPos.x*2 + pointVector.x*2);

            /* set the current source pointer */
            params->pSrc += iRefOffset;
            /* call optimized function from the table */
            h264_interpolate_chroma_type_table_16u_pxmx[params->iType](params);
        }
        else
        {
          
        }
    }
}

template<typename Plane>
void ippiUniDirWeightBlock_NV12_H264_8u_C1IR(Plane *pSrcDst,
                                                Ipp32u srcDstStep,
                                                Ipp32u ulog2wd,
                                                Ipp32s iWeightU, Ipp32s iOffsetU,
                                                Ipp32s iWeightV, Ipp32s iOffsetV,
                                                IppiSize roi, Ipp32u bitDepth)
{
    Ipp32u uRound;
    Ipp32u xpos, ypos;
    Ipp32s weighted_sample;
    Ipp32u uWidth=roi.width;
    Ipp32u uHeight=roi.height;

    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);

    if (ulog2wd > 0)
        uRound = 1<<(ulog2wd - 1);
    else
        uRound = 0;

    for (ypos=0; ypos<uHeight; ypos++)
    {
        for (xpos=0; xpos<uWidth; xpos++)
        {
            weighted_sample = (((Ipp32s)pSrcDst[2*xpos]*iWeightU + (Ipp32s)uRound)>>ulog2wd) + iOffsetU;
            if (weighted_sample > maxValue) weighted_sample = maxValue;
            if (weighted_sample < 0) weighted_sample = 0;
            pSrcDst[2*xpos] = (Plane)weighted_sample;

            weighted_sample = (((Ipp32s)pSrcDst[2*xpos + 1]*iWeightV + (Ipp32s)uRound)>>ulog2wd) + iOffsetV;
            /*  clamp to 0..255. May be able to use ClampVal table for this,
                if range of unclamped weighted_sample can be guaranteed not
                to exceed table bounds. */
            if (weighted_sample > maxValue) weighted_sample = maxValue;
            if (weighted_sample < 0) weighted_sample = 0;
            pSrcDst[2*xpos + 1] = (Plane)weighted_sample;
        }
        pSrcDst += srcDstStep;
    }
}

template<typename Plane>
void ippiBiDirWeightBlock_NV12_H264_8u_P3P1R(const Plane *pSrc1,
    const Plane *pSrc2,
    Plane *pDst,
    Ipp32u nSrcPitch1,
    Ipp32u nSrcPitch2,
    Ipp32u nDstPitch,
    Ipp32u ulog2wd,    /* log2 weight denominator */
    Ipp32s iWeightU1, Ipp32s iOffsetU1,
    Ipp32s iWeightU2, Ipp32s iOffsetU2,
    Ipp32s iWeightV1, Ipp32s iOffsetV1,
    Ipp32s iWeightV2, Ipp32s iOffsetV2,
    IppiSize roi, Ipp32u bitDepth)
{
    Ipp32u uRound;
    Ipp32s xpos, ypos;
    Ipp32s weighted_sample;

    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);

    uRound = 1<<ulog2wd;

    for (ypos=0; ypos<roi.height; ypos++)
    {
        for (xpos=0; xpos<roi.width; xpos++)
        {
            weighted_sample = ((((Ipp32s)pSrc1[2*xpos]*iWeightU1 +
                (Ipp32s)pSrc2[2*xpos]*iWeightU2 + (Ipp32s)uRound)>>(ulog2wd+1))) +
                ((iOffsetU1 + iOffsetU2 + 1)>>1);
            /*  clamp to 0..255. May be able to use ClampVal table for this,
                if range of unclamped weighted_sample can be guaranteed not
                to exceed table bounds. */
            if (weighted_sample > maxValue) weighted_sample = maxValue;
            if (weighted_sample < 0) weighted_sample = 0;
            pDst[2*xpos] = (Plane)weighted_sample;

            weighted_sample = ((((Ipp32s)pSrc1[2*xpos + 1]*iWeightV1 +
                (Ipp32s)pSrc2[2*xpos + 1]*iWeightV2 + (Ipp32s)uRound)>>(ulog2wd+1))) +
                ((iOffsetV1 + iOffsetV2 + 1)>>1);
            /*  clamp to 0..255. May be able to use ClampVal table for this,
                if range of unclamped weighted_sample can be guaranteed not
                to exceed table bounds. */
            if (weighted_sample > maxValue) weighted_sample = maxValue;
            if (weighted_sample < 0) weighted_sample = 0;
            pDst[2*xpos + 1] = (Plane)weighted_sample;
        }
        pSrc1 += nSrcPitch1;
        pSrc2 += nSrcPitch2;
        pDst += nDstPitch;
    }
}


void H264_FASTCALL MFX_Dispatcher::InterpolateChromaBlock_16u(const IppVCInterpolateBlock_16u *interpolateInfo)
{
    H264InterpolationParams_16u params;
    params.pSrc = interpolateInfo->pSrc[0];
    params.srcStep = interpolateInfo->srcStep;
    params.pDst = interpolateInfo->pDst[0];
    params.dstStep = interpolateInfo->dstStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    params.pointVector = interpolateInfo->pointVector;
    params.pointBlockPos = interpolateInfo->pointBlockPos;
    params.frameSize = interpolateInfo->sizeFrame;

    Ipp16u temporary_buffer[2*16*17 + 16];

    ippiInterpolateChromaBlock(&params, temporary_buffer); // boundaries

    h264_interpolate_chroma_type_table_16u_pxmx[params.iType](&params);
    //ippiInterpolateChromaBlock_H264_kernel<mfxU16>(&params);
}

void H264_FASTCALL MFX_Dispatcher::InterpolateChromaBlock_8u(const IppVCInterpolateBlock_8u *interpolateInfo)
{
    H264InterpolationParams_8u params;
    params.pSrc = interpolateInfo->pSrc[0];
    params.srcStep = interpolateInfo->srcStep;
    params.pDst = interpolateInfo->pDst[0];
    params.dstStep = interpolateInfo->dstStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    params.pointVector = interpolateInfo->pointVector;
    params.pointBlockPos = interpolateInfo->pointBlockPos;
    params.frameSize = interpolateInfo->sizeFrame;

    Ipp8u temporary_buffer[2*16*17 + 16];

    ippiInterpolateChromaBlock(&params, temporary_buffer); // boundaries

    h264_interpolate_chroma_type_table_8u_pxmx[params.iType](&params);
    //ippiInterpolateChromaBlock_H264_kernel<mfxU16>(&params);
}
void MFX_Dispatcher::UniDirWeightBlock_NV12(Ipp8u *pSrcDst, Ipp32u srcDstStep,
                                Ipp32u ulog2wd,
                                Ipp32s iWeightU, Ipp32s iOffsetU,
                                Ipp32s iWeightV, Ipp32s iOffsetV,
                                IppiSize roi, Ipp32u bitDepth)
{
    ippiUniDirWeightBlock_NV12_H264_8u_C1IR(pSrcDst,
                                                srcDstStep,
                                                ulog2wd,
                                                iWeightU, iOffsetU,
                                                iWeightV, iOffsetV,
                                                roi, bitDepth);
}

void MFX_Dispatcher::UniDirWeightBlock_NV12(Ipp16u *pSrcDst, Ipp32u srcDstStep,
                                Ipp32u ulog2wd,
                                Ipp32s iWeightU, Ipp32s iOffsetU,
                                Ipp32s iWeightV, Ipp32s iOffsetV,
                                IppiSize roi, Ipp32u bitDepth)
{
    ippiUniDirWeightBlock_NV12_H264_8u_C1IR(pSrcDst,
                                                srcDstStep,
                                                ulog2wd,
                                                iWeightU, iOffsetU,
                                                iWeightV, iOffsetV,
                                                roi, bitDepth);
}

void MFX_Dispatcher::BiDirWeightBlock_NV12(const Ipp8u *pSrc0, Ipp32u src0Pitch,
                                            const Ipp8u *pSrc1, Ipp32u src1Pitch,
                                            Ipp8u *pDst, Ipp32u dstPitch,
                                            Ipp32u ulog2wd,
                                            Ipp32s iWeightU1, Ipp32s iOffsetU1,
                                            Ipp32s iWeightU2, Ipp32s iOffsetU2,
                                            Ipp32s iWeightV1, Ipp32s iOffsetV1,
                                            Ipp32s iWeightV2, Ipp32s iOffsetV2,
                                            IppiSize roi, Ipp32u bitDepth)
{
    ippiBiDirWeightBlock_NV12_H264_8u_P3P1R(pSrc0, pSrc1, pDst,
        src0Pitch, src1Pitch, dstPitch,
        ulog2wd,
        iWeightU1, iOffsetU1,
        iWeightU2, iOffsetU2,
        iWeightV1, iOffsetV1,
        iWeightV2, iOffsetV2,
        roi, bitDepth);
}

void MFX_Dispatcher::BiDirWeightBlock_NV12(const Ipp16u *pSrc0, Ipp32u src0Pitch,
                                            const Ipp16u *pSrc1, Ipp32u src1Pitch,
                                            Ipp16u *pDst, Ipp32u dstPitch,
                                            Ipp32u ulog2wd,
                                            Ipp32s iWeightU1, Ipp32s iOffsetU1,
                                            Ipp32s iWeightU2, Ipp32s iOffsetU2,
                                            Ipp32s iWeightV1, Ipp32s iOffsetV1,
                                            Ipp32s iWeightV2, Ipp32s iOffsetV2,
                                            IppiSize roi, Ipp32u bitDepth)
{
    ippiBiDirWeightBlock_NV12_H264_8u_P3P1R(pSrc0, pSrc1, pDst,
        src0Pitch, src1Pitch, dstPitch,
        ulog2wd,
        iWeightU1, iOffsetU1,
        iWeightU2, iOffsetU2,
        iWeightV1, iOffsetV1,
        iWeightV2, iOffsetV2,
        roi, bitDepth);
}

} // namespace MFX_PP
