/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_h265_dec_ipplevel.h"
#include "ippvc.h"
#include "ipps.h"
#include <memory.h>
#include <algorithm>

/*
THIS FILE IS A TEMPORAL SOLUTION AND IT WILL BE REMOVED AS SOON AS THE NEW FUNCTIONS ARE ADDED
*/
namespace UMC_HEVC_DECODER
{


#define ABS(a)          (((a) < 0) ? (-(a)) : (a))
#define max(a, b)       (((a) > (b)) ? (a) : (b))
#define min(a, b)       (((a) < (b)) ? (a) : (b))
#define ClampVal(x)  (ClampTbl[256 + (x)])
#define clipd1(x, limit) min(limit, max(x,-limit))

#define ClampTblLookup(x, y) ClampVal((x) + clipd1((y),256))
#define ClampTblLookupNew(x) ClampVal((x))

#define _IPP_ARCH_IA32    1
#define _IPP_ARCH_IA64    2
#define _IPP_ARCH_EM64T   4
#define _IPP_ARCH_XSC     8
#define _IPP_ARCH_LRB     16
#define _IPP_ARCH_LP32    32
#define _IPP_ARCH_LP64    64

#define _IPP_ARCH    _IPP_ARCH_IA32

#define _IPP_PX 0
#define _IPP_M6 1
#define _IPP_A6 2
#define _IPP_W7 4
#define _IPP_T7 8
#define _IPP_V8 16
#define _IPP_P8 32
#define _IPP_G9 64

#define _IPP _IPP_PX

#define _IPP32E_PX _IPP_PX
#define _IPP32E_M7 32
#define _IPP32E_U8 64
#define _IPP32E_Y8 128
#define _IPP32E_E9 256

#define _IPP32E _IPP32E_PX

#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#define IPP_BADARG_RET( expr, ErrCode )\
            {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#define IPP_BAD_SIZE_RET( n )\
            IPP_BADARG_RET( (n)<=0, ippStsSizeErr )

#define IPP_BAD_STEP_RET( n )\
            IPP_BADARG_RET( (n)<=0, ippStsStepErr )

#define IPP_BAD_PTR1_RET( ptr )\
            IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

#define IPP_BAD_PTR2_RET( ptr1, ptr2 )\
            IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))), ippStsNullPtrErr)

#define IPP_BAD_PTR3_RET( ptr1, ptr2, ptr3 )\
            IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))||(NULL==(ptr3))),\
                                                     ippStsNullPtrErr)

#define IPP_BAD_PTR4_RET( ptr1, ptr2, ptr3, ptr4 )\
                {IPP_BAD_PTR2_RET( ptr1, ptr2 ); IPP_BAD_PTR2_RET( ptr3, ptr4 )}

#define IPP_BAD_RANGE_RET( val, low, high )\
     IPP_BADARG_RET( ((val)<(low) || (val)>(high)), ippStsOutOfRangeErr)

#define __ALIGN16 __declspec (align(16))


/* Define NULL pointer value */
#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#define read_data_through_boundary_table_8u read_data_through_boundary_table_8u_pxmx
typedef void (*pH264Interpolation_8u) (H264InterpolationParams_8u *pParams);
extern pH264Interpolation_8u read_data_through_boundary_table_8u_pxmx[16];

#define read_data_through_boundary_table_16u read_data_through_boundary_table_16u_pxmx
typedef void (*pH264Interpolation_16u) (H264InterpolationParams_16u *pParams);
extern pH264Interpolation_16u read_data_through_boundary_table_16u_pxmx[16];

#define read_data_through_boundary_table_nv12_8u read_data_through_boundary_table_nv12_8u_pxmx
extern pH264Interpolation_8u read_data_through_boundary_table_nv12_8u_pxmx[16];

#define read_data_through_boundary_table_nv12_16u read_data_through_boundary_table_nv12_16u_pxmx
extern pH264Interpolation_16u read_data_through_boundary_table_nv12_16u_pxmx[16];

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateBoundaryLumaBlock_H264(Ipp32s iOverlappingType, InterpolationStruct *pParams, PixType *temporary_buffer)
{
    PixType *pTmp = temporary_buffer;
    Ipp32s tmpStep = 128; /* !!! pitch is fixed*/

    /* read data into temporal buffer */
    {
        PixType *pDst = pParams->pDst;
        /*SIZE_T*/size_t dstStep = pParams->dstStep;

        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        if (sizeof(PixType) == 1)
        {
            read_data_through_boundary_table_8u[iOverlappingType]((H264InterpolationParams_8u*)pParams);
        }
        else
        {
            read_data_through_boundary_table_16u[iOverlappingType]((H264InterpolationParams_16u*)pParams);
        }

        pParams->pSrc = pTmp +
                        ((pParams->iType & 0x0c) ? (1) : (0)) * 3 * tmpStep +
                        ((pParams->iType & 0x03) ? (1) : (0)) * 3;
        pParams->srcStep = tmpStep;
        pParams->pDst = pDst;
        pParams->dstStep = dstStep;
    }

    return ippStsNoErr;
}

template<typename PixType, typename InterpolationStruct, typename InterpolationStructInternal>
IppStatus ippiInterpolateLumaBlock_H265Internal(InterpolationStruct *interpolateInfo, PixType *temporary_buffer)
{
    InterpolationStructInternal params;

    /* prepare pointers */
    params.pDst = interpolateInfo->pDst[0];
    params.dstStep = interpolateInfo->dstStep;
    params.pSrc = interpolateInfo->pSrc[0];
    params.srcStep = interpolateInfo->srcStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        /* set the current source pointer */
        params.pSrc += interpolateInfo->pointBlockPos.y * params.srcStep +
                       interpolateInfo->pointBlockPos.x;

        return ippStsNoOperation;
    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = interpolateInfo->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            hFraction = pointVector.x & 3;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                params.xPos = interpolateInfo->pointBlockPos.x + (pointVector.x >>= 2) - iAddition * 3;
                params.dataWidth = params.blockWidth + iAddition * 8;
            }

            /* prepare vertical vector values */
            vFraction = pointVector.y & 3;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                params.yPos = interpolateInfo->pointBlockPos.y + (pointVector.y >>= 2) - iAddition * 3;
                params.dataHeight = params.blockHeight + iAddition * 8;
            }

            params.iType = (vFraction << 2) | hFraction;
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
            /* set the current source pointer */
            params.pSrc += (interpolateInfo->pointBlockPos.y + pointVector.y) * params.srcStep +
                           (interpolateInfo->pointBlockPos.x + pointVector.x);

            /* call optimized function from the table */
            //h264_interpolate_luma_type_table_8u[params.iType](&params);

            //return ippStsNoErr;
            return ippStsNoOperation;
        }
        else
        {
            /* save additional parameters */
            params.frameSize.width = interpolateInfo->sizeFrame.width;
            params.frameSize.height = interpolateInfo->sizeFrame.height;

            /* there is something wrong, try to work through a temporal buffer */
            IppStatus sts = ippiInterpolateBoundaryLumaBlock_H264<PixType, InterpolationStructInternal> (iOverlappingType, &params, temporary_buffer);


            interpolateInfo->pSrc[0] = params.pSrc;
            interpolateInfo->srcStep = (Ipp32s)params.srcStep;
            return sts;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateBoundaryChromaBlock_NV12_H264(Ipp32s iOverlappingType, InterpolationStruct *pParams, PixType *temporary_buffer)
{
    PixType *pTmp = temporary_buffer;
    Ipp32s tmpStep = 128;

    /* read data into temporal buffer */
    {
        PixType *pDst = pParams->pDst;
        size_t dstStep = pParams->dstStep;

        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        if (sizeof(PixType) == 1)
        {
            read_data_through_boundary_table_nv12_8u[iOverlappingType]((H264InterpolationParams_8u*)pParams);
        }
        else
        {
            read_data_through_boundary_table_nv12_16u[iOverlappingType]((H264InterpolationParams_16u*)pParams);
        }

        pParams->pSrc = pTmp +
                        ((pParams->iType & 0x02) ? (1) : (0)) * 1 * tmpStep +
                        ((pParams->iType & 0x01) ? (1) : (0)) * 2;
        pParams->srcStep = tmpStep;
        pParams->pDst = pDst;
        pParams->dstStep = dstStep;
    }

    return ippStsNoErr;
}

template<typename PixType, typename InterpolationStruct, typename InterpolationStructInternal>
IppStatus ippiInterpolateChromaBlock_H264Internal(InterpolationStruct *interpolateInfo, PixType *temporary_buffer)
{
    InterpolationStructInternal params;

    /* check error(s) */
    IPP_BAD_PTR1_RET(interpolateInfo);
    IPP_BAD_PTR1_RET(interpolateInfo->pSrc[0]);

    /* prepare pointers */
    params.pDst = interpolateInfo->pDst[0];
    params.dstStep = interpolateInfo->dstStep;
    params.pSrc = interpolateInfo->pSrc[0];
    params.srcStep = interpolateInfo->srcStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        size_t iRefOffset;

        /* set the current reference offset */
        iRefOffset = interpolateInfo->pointBlockPos.y * params.srcStep +
                     interpolateInfo->pointBlockPos.x*2;

        /* set the current source pointer */
        params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
        params.pDst = interpolateInfo->pDst[0];
        params.pDstComplementary = interpolateInfo->pDst[1];
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
                params.xPos = interpolateInfo->pointBlockPos.x + (pointVector.x >>= 3) - iAddition;
                params.dataWidth = params.blockWidth + iAddition*4;
            }

            /* prepare vertical vector values */
            params.vFraction =
            vFraction = pointVector.y & 7;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                params.yPos = interpolateInfo->pointBlockPos.y + (pointVector.y >>= 3) - iAddition;
                params.dataHeight = params.blockHeight + iAddition*4;
            }

            params.iType = ((vFraction ? 1 : 0) << 1) |
                           (hFraction ? 1 : 0);
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
            size_t iRefOffset;

            /* set the current reference offset */
            iRefOffset = (interpolateInfo->pointBlockPos.y + pointVector.y) * params.srcStep +
                         (interpolateInfo->pointBlockPos.x*2 + pointVector.x*2);

            /* set the current source pointer */
            params.pSrc = interpolateInfo->pSrc[0] + iRefOffset;
            params.pDst = interpolateInfo->pDst[0];
            params.pDstComplementary = interpolateInfo->pDst[1];
            params.frameSize.height = interpolateInfo->sizeFrame.height;
            params.frameSize.width = interpolateInfo->sizeFrame.width;
            /* call optimized function from the table */
        }
        else
        {
            /* save additional parameters */
            params.frameSize.width = interpolateInfo->sizeFrame.width;
            params.frameSize.height = interpolateInfo->sizeFrame.height;

            /* there is something wrong, try to work through a temporal buffer */
            IppStatus sts =  ippiInterpolateBoundaryChromaBlock_NV12_H264<PixType, InterpolationStructInternal>(iOverlappingType, &params, temporary_buffer);
            interpolateInfo->pSrc[0] = params.pSrc;
            interpolateInfo->srcStep = (Ipp32s)params.srcStep;
            return sts;
        }
    }

    return ippStsNoOperation;
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_none(InterpolationStruct *pParams)
{
    /* touch unreferenced parameter(s) */
    IPP_UNREFERENCED_PARAMETER(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            std::fill(pDst, pDst + iIndent, pSrc[0]);
            MFX_INTERNAL_CPY(pDst + iIndent, pSrc, (pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, iIndent*sizeof(PixType));
            std::fill(pDst + iIndent, pDst + iIndent + pParams->dataWidth - iIndent, pSrc[iIndent - 1]);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_left_right_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_left_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_right_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }

}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        std::fill(pDst, pDst + iIndent, pSrc[0]);
        MFX_INTERNAL_CPY(pDst + iIndent, pSrc, (pParams->dataWidth - iIndent)*sizeof(PixType));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            std::fill(pDst, pDst + iIndent, pSrc[0]);
            MFX_INTERNAL_CPY(pDst + iIndent, pSrc, (pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->xPos;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        MFX_INTERNAL_CPY(pDst, pSrc, iIndent*sizeof(PixType));
        std::fill(pDst + iIndent, pDst + iIndent + pParams->dataWidth - iIndent, pSrc[iIndent - 1]);

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, iIndent*sizeof(PixType));
            std::fill(pDst + iIndent, pDst + iIndent + pParams->dataWidth - iIndent, pSrc[iIndent - 1]);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_left_right_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_top_left_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_top_right_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* copy existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            std::fill(pDst, pDst + iIndent, pSrc[0]);
            MFX_INTERNAL_CPY(pDst + iIndent, pSrc, (pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, iIndent*sizeof(PixType));
            std::fill(pDst + iIndent, pDst + iIndent + pParams->dataWidth - iIndent, pSrc[iIndent - 1]);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_left_right_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_bottom_left_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_right_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_left_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_left_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_left_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_right_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_right_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_right_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_left_right_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_left_px<PixType, InterpolationStruct>(pParams);
        else
            read_data_through_boundary_bottom_left_px<PixType, InterpolationStruct>(pParams);
    }
    else
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_right_px<PixType, InterpolationStruct>(pParams);
        else
            read_data_through_boundary_bottom_right_px<PixType, InterpolationStruct>(pParams);
    }
}

/* Functions for NV12*/

template<typename PixType>
void memset_nv12(PixType *pDst, const PixType* nVal, Ipp32s nNum)
{
    Ipp32s i;

    for (i = 0; i < nNum; i ++ )
    {
        pDst[2*i] = nVal[0];
        pDst[2*i + 1] = nVal[1];
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_none_nv12(InterpolationStruct *pParams)
{
    /* touch unreferenced parameter(s) */
    IPP_UNREFERENCED_PARAMETER(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_left_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            memset_nv12<PixType>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_right_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent*sizeof(PixType));
            memset_nv12<PixType>(pDst + 2*iIndent, &pSrc[2*iIndent - 2], 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_left_right_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_left_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_right_nv12_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + 2*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_left_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        memset_nv12<PixType>(pDst, pSrc, iIndent);
        MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent)*sizeof(PixType));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            memset_nv12<PixType>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_right_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + 2*pParams->xPos;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent*sizeof(PixType));
        memset_nv12<PixType>(pDst + 2*iIndent, &pSrc[2*iIndent - 2], 2*(pParams->dataWidth - iIndent));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent*sizeof(PixType));
            memset_nv12<PixType>(pDst + 2*iIndent, &pSrc[2*iIndent - 2], 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_left_right_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_top_left_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_top_right_nv12_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* copy existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_left_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            memset_nv12<PixType>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + 2*iIndent, pSrc, 2*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_right_nv12_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + 2*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*iIndent*sizeof(PixType));
            memset_nv12<PixType>(pDst + 2*iIndent, &pSrc[2*iIndent - 2], 2*(pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, 2*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_bottom_left_right_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
        read_data_through_boundary_bottom_left_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_right_nv12_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_nv12_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_left_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_left_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_left_nv12_px<PixType, InterpolationStruct>(pParams);
}

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_right_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->yPos)
        read_data_through_boundary_top_right_nv12_px<PixType, InterpolationStruct>(pParams);
    else
        read_data_through_boundary_bottom_right_nv12_px<PixType, InterpolationStruct>(pParams);
} /* void read_data_through_boundary_top_bottom_right_nv12_8u_px(H264InterpolationParams_8u *pParams) */

template<typename PixType, typename InterpolationStruct>
void read_data_through_boundary_top_bottom_left_right_nv12_px(InterpolationStruct *pParams)
{
    if (0 > pParams->xPos)
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_left_nv12_px<PixType, InterpolationStruct>(pParams);
        else
            read_data_through_boundary_bottom_left_nv12_px<PixType, InterpolationStruct>(pParams);
    }
    else
    {
        if (0 > pParams->yPos)
            read_data_through_boundary_top_right_nv12_px<PixType, InterpolationStruct>(pParams);
        else
            read_data_through_boundary_bottom_right_nv12_px<PixType, InterpolationStruct>(pParams);
    }
}

/* declare functions for handle boundary cases */
pH264Interpolation_8u read_data_through_boundary_table_8u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_left_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_right_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_left_right_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_top_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_left_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_right_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_left_right_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_bottom_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_left_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_right_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_left_right_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_top_bottom_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_left_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_right_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp8u, H264InterpolationParams_8u>
};

pH264Interpolation_8u read_data_through_boundary_table_nv12_8u_pxmx[16] =
{
    &read_data_through_boundary_none_nv12<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_left_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_left_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_top_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_left_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_left_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_bottom_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_left_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_bottom_left_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,

    &read_data_through_boundary_top_bottom_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_left_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_right_nv12_px<Ipp8u, H264InterpolationParams_8u>,
    &read_data_through_boundary_top_bottom_left_right_nv12_px<Ipp8u, H264InterpolationParams_8u>
};

pH264Interpolation_16u read_data_through_boundary_table_16u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_left_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_right_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_left_right_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_top_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_left_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_right_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_left_right_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_bottom_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_left_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_right_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_left_right_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_top_bottom_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_left_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_right_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp16u, H264InterpolationParams_16u>
};

pH264Interpolation_16u read_data_through_boundary_table_nv12_16u_pxmx[16] =
{
    &read_data_through_boundary_none_nv12<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_left_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_left_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_top_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_left_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_left_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_bottom_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_left_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_bottom_left_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,

    &read_data_through_boundary_top_bottom_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_left_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_right_nv12_px<Ipp16u, H264InterpolationParams_16u>,
    &read_data_through_boundary_top_bottom_left_right_nv12_px<Ipp16u, H264InterpolationParams_16u>
};

IppStatus ippiInterpolateLumaBlock_H265(IppVCInterpolateBlock_8u *interpolateInfo, Ipp8u *temporary_buffer)
{
    return ippiInterpolateLumaBlock_H265Internal<Ipp8u, IppVCInterpolateBlock_8u, H264InterpolationParams_8u>(interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateChromaBlock_H264(IppVCInterpolateBlock_8u *interpolateInfo, Ipp8u *temporary_buffer)
{
    return ippiInterpolateChromaBlock_H264Internal<Ipp8u, IppVCInterpolateBlock_8u, H264InterpolationParams_8u>(interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateLumaBlock_H265(IppVCInterpolateBlock_8u *interpolateInfo, Ipp16u *temporary_buffer)
{
    return ippiInterpolateLumaBlock_H265Internal<Ipp16u, IppVCInterpolateBlock_16u, H264InterpolationParams_16u>((IppVCInterpolateBlock_16u*)interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateChromaBlock_H264(IppVCInterpolateBlock_8u *interpolateInfo, Ipp16u *temporary_buffer)
{
    return ippiInterpolateChromaBlock_H264Internal<Ipp16u, IppVCInterpolateBlock_16u, H264InterpolationParams_16u>((IppVCInterpolateBlock_16u*)interpolateInfo, temporary_buffer);
}


} /*namespace UMC_HEVC_DECODER*/

#endif // UMC_ENABLE_H265_VIDEO_DECODER