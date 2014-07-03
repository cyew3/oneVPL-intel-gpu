/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_dec_ipplevel.h"
#include "ippvc.h"

/*
THIS FILE IS A TEMPORAL SOLUTION AND IT WILL BE REMOVED AS SOON AS THE NEW FUNCTIONS ARE ADDED
*/
namespace UMC_H264_DECODER
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

/* clamping table(s) */
const Ipp8u ClampTbl[768] =
{
     0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x01 ,0x02 ,0x03 ,0x04 ,0x05 ,0x06 ,0x07
    ,0x08 ,0x09 ,0x0a ,0x0b ,0x0c ,0x0d ,0x0e ,0x0f
    ,0x10 ,0x11 ,0x12 ,0x13 ,0x14 ,0x15 ,0x16 ,0x17
    ,0x18 ,0x19 ,0x1a ,0x1b ,0x1c ,0x1d ,0x1e ,0x1f
    ,0x20 ,0x21 ,0x22 ,0x23 ,0x24 ,0x25 ,0x26 ,0x27
    ,0x28 ,0x29 ,0x2a ,0x2b ,0x2c ,0x2d ,0x2e ,0x2f
    ,0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37
    ,0x38 ,0x39 ,0x3a ,0x3b ,0x3c ,0x3d ,0x3e ,0x3f
    ,0x40 ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47
    ,0x48 ,0x49 ,0x4a ,0x4b ,0x4c ,0x4d ,0x4e ,0x4f
    ,0x50 ,0x51 ,0x52 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57
    ,0x58 ,0x59 ,0x5a ,0x5b ,0x5c ,0x5d ,0x5e ,0x5f
    ,0x60 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67
    ,0x68 ,0x69 ,0x6a ,0x6b ,0x6c ,0x6d ,0x6e ,0x6f
    ,0x70 ,0x71 ,0x72 ,0x73 ,0x74 ,0x75 ,0x76 ,0x77
    ,0x78 ,0x79 ,0x7a ,0x7b ,0x7c ,0x7d ,0x7e ,0x7f
    ,0x80 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87
    ,0x88 ,0x89 ,0x8a ,0x8b ,0x8c ,0x8d ,0x8e ,0x8f
    ,0x90 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96 ,0x97
    ,0x98 ,0x99 ,0x9a ,0x9b ,0x9c ,0x9d ,0x9e ,0x9f
    ,0xa0 ,0xa1 ,0xa2 ,0xa3 ,0xa4 ,0xa5 ,0xa6 ,0xa7
    ,0xa8 ,0xa9 ,0xaa ,0xab ,0xac ,0xad ,0xae ,0xaf
    ,0xb0 ,0xb1 ,0xb2 ,0xb3 ,0xb4 ,0xb5 ,0xb6 ,0xb7
    ,0xb8 ,0xb9 ,0xba ,0xbb ,0xbc ,0xbd ,0xbe ,0xbf
    ,0xc0 ,0xc1 ,0xc2 ,0xc3 ,0xc4 ,0xc5 ,0xc6 ,0xc7
    ,0xc8 ,0xc9 ,0xca ,0xcb ,0xcc ,0xcd ,0xce ,0xcf
    ,0xd0 ,0xd1 ,0xd2 ,0xd3 ,0xd4 ,0xd5 ,0xd6 ,0xd7
    ,0xd8 ,0xd9 ,0xda ,0xdb ,0xdc ,0xdd ,0xde ,0xdf
    ,0xe0 ,0xe1 ,0xe2 ,0xe3 ,0xe4 ,0xe5 ,0xe6 ,0xe7
    ,0xe8 ,0xe9 ,0xea ,0xeb ,0xec ,0xed ,0xee ,0xef
    ,0xf0 ,0xf1 ,0xf2 ,0xf3 ,0xf4 ,0xf5 ,0xf6 ,0xf7
    ,0xf8 ,0xf9 ,0xfa ,0xfb ,0xfc ,0xfd ,0xfe ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
};

static Ipp16s zeroArray[16] = {0};


/* Define NULL pointer value */
#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

extern void ConvertNV12ToYV12(const Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp8u *pSrcDstUPlane, Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi)
{
    Ipp32s i, j;

    for (j = 0; j < roi.height; j++)
    {
        for (i = 0; i < roi.width; i++)
        {
            pSrcDstUPlane[i] = pSrcDstUVPlane[2*i];
            pSrcDstVPlane[i] = pSrcDstUVPlane[2*i + 1];
        }

        pSrcDstUPlane += _srcdstUStep;
        pSrcDstVPlane += _srcdstUStep;

        pSrcDstUVPlane += _srcdstUVStep;
    }
}

extern void ConvertYV12ToNV12(const Ipp8u *pSrcDstUPlane, const Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi)
{
    Ipp32s i, j;

    for (j = 0; j < roi.height; j++)
    {
        for (i = 0; i < roi.width; i++)
        {
            pSrcDstUVPlane[2*i] = pSrcDstUPlane[i];
            pSrcDstUVPlane[2*i + 1] = pSrcDstVPlane[i];
        }

        pSrcDstUPlane += _srcdstUStep;
        pSrcDstVPlane += _srcdstUStep;

        pSrcDstUVPlane += _srcdstUVStep;
    }
}

void ConvertNV12ToYV12(const Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp16u *pSrcDstUPlane, Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi)
{
    Ipp32s i, j;

    for (j = 0; j < roi.height; j++)
    {
        for (i = 0; i < roi.width; i++)
        {
            pSrcDstUPlane[i] = pSrcDstUVPlane[2*i];
            pSrcDstVPlane[i] = pSrcDstUVPlane[2*i + 1];
        }

        pSrcDstUPlane += _srcdstUStep;
        pSrcDstVPlane += _srcdstUStep;

        pSrcDstUVPlane += _srcdstUVStep;
    }
}

void ConvertYV12ToNV12(const Ipp16u *pSrcDstUPlane, const Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi)
{
    Ipp32s i, j;

    for (j = 0; j < roi.height; j++)
    {
        for (i = 0; i < roi.width; i++)
        {
            pSrcDstUVPlane[2*i] = pSrcDstUPlane[i];
            pSrcDstUVPlane[2*i + 1] = pSrcDstVPlane[i];
        }

        pSrcDstUPlane += _srcdstUStep;
        pSrcDstVPlane += _srcdstUStep;

        pSrcDstUVPlane += _srcdstUVStep;
    }
}

IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s8u_C1I_NV12,(const Ipp8u *pPred,
                                                   Ipp16s *pCoeffs,
                                                   Ipp8u *pRec,
                                                   Ipp32s predPitch,
                                                   Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = ClampTblLookup(pPred[0], tmpBuf[i * 4 + 0]);
        pRec[2] = ClampTblLookup(pPred[2], tmpBuf[i * 4 + 1]);
        pRec[4] = ClampTblLookup(pPred[4], tmpBuf[i * 4 + 2]);
        pRec[6] = ClampTblLookup(pPred[6], tmpBuf[i * 4 + 3]);
        pRec += recPitch;
        pPred += predPitch;
    }

    return ippStsNoErr;

}

IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s16s_C1I_NV12, (const Ipp16s *pPred,
                                                      Ipp16s *pCoeffs,
                                                      Ipp16s *pRec,
                                                      Ipp32s predPitch,
                                                      Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = pPred[0] + tmpBuf[i * 4 + 0];
        pRec[2] = pPred[2] + tmpBuf[i * 4 + 1];
        pRec[4] = pPred[4] + tmpBuf[i * 4 + 2];
        pRec[6] = pPred[6] + tmpBuf[i * 4 + 3];
        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
        pPred = (Ipp16s*)((Ipp8u*)pPred + predPitch);
    }

    return ippStsNoErr;
}

IPPFUN(IppStatus, ippiTransformResidual_H264_16s16s_C1I_NV12,(const Ipp16s *pCoeffs,
                                                Ipp16s *pRec,
                                                Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR2_RET(pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = tmpBuf[i * 4 + 0];
        pRec[2] = tmpBuf[i * 4 + 1];
        pRec[4] = tmpBuf[i * 4 + 2];
        pRec[6] = tmpBuf[i * 4 + 3];
        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
    }

    return ippStsNoErr;
}

IPPFUN(IppStatus, ippiUniDirWeightBlock_NV12_H264_8u_C1IR, (Ipp8u *pSrcDst,
                                                      Ipp32u srcDstStep,
                                                      Ipp32u ulog2wd,
                                                      Ipp32s iWeightU,
                                                      Ipp32s iOffsetU,
                                                      Ipp32s iWeightV,
                                                      Ipp32s iOffsetV,
                                                      IppiSize roi))
{
    Ipp32u uRound;
    Ipp32u xpos, ypos;
    Ipp32s weighted_sample;
    Ipp32u uWidth=roi.width;
    Ipp32u uHeight=roi.height;
    IPP_BAD_PTR1_RET( pSrcDst)
    IPP_BADARG_RET( (srcDstStep < (unsigned)roi.width),  ippStsStepErr)
    IPP_BADARG_RET( ( roi.height != 2 && roi.height != 4    && roi.height != 8 && roi.height != 16 ) ||
                    ( roi.width  != 2 && roi.width  != 4    && roi.width  != 8 && roi.width  != 16 ),
                    ippStsSizeErr)

    if (ulog2wd > 0)
        uRound = 1<<(ulog2wd - 1);
    else
        uRound = 0;

    for (ypos=0; ypos<uHeight; ypos++)
    {
        for (xpos=0; xpos<uWidth; xpos++)
        {
            weighted_sample = (((Ipp32s)pSrcDst[2*xpos]*iWeightU + (Ipp32s)uRound)>>ulog2wd) + iOffsetU;
            if (weighted_sample > 255) weighted_sample = 255;
            if (weighted_sample < 0) weighted_sample = 0;
            pSrcDst[2*xpos] = (Ipp8u)weighted_sample;

            weighted_sample = (((Ipp32s)pSrcDst[2*xpos + 1]*iWeightV + (Ipp32s)uRound)>>ulog2wd) + iOffsetV;
            /*  clamp to 0..255. May be able to use ClampVal table for this,
                if range of unclamped weighted_sample can be guaranteed not
                to exceed table bounds. */
            if (weighted_sample > 255) weighted_sample = 255;
            if (weighted_sample < 0) weighted_sample = 0;
            pSrcDst[2*xpos + 1] = (Ipp8u)weighted_sample;
        }
        pSrcDst += srcDstStep;
    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, IppiUniDirWeightBlock_H264_8u_C1IR, (Ipp8u *pSrcDst, */


IPPFUN(IppStatus, ippiBiDirWeightBlock_NV12_H264_8u_P3P1R,( const Ipp8u *pSrc1,
    const Ipp8u *pSrc2,
    Ipp8u *pDst,
    Ipp32u nSrcPitch1,
    Ipp32u nSrcPitch2,
    Ipp32u nDstPitch,
    Ipp32u ulog2wd,    /* log2 weight denominator */
    Ipp32s iWeightU1,
    Ipp32s iOffsetU1,
    Ipp32s iWeightU2,
    Ipp32s iOffsetU2,
    Ipp32s iWeightV1,
    Ipp32s iOffsetV1,
    Ipp32s iWeightV2,
    Ipp32s iOffsetV2,
    IppiSize roi
    ))
{
    /*IPP_BAD_PTR3_RET(pSrc1,pSrc2,pDst)
    IPP_BADARG_RET( (nSrcPitch1 < (unsigned)roi.width),  ippStsStepErr)
    IPP_BADARG_RET( (nSrcPitch2 < (unsigned)roi.width),  ippStsStepErr)
    IPP_BADARG_RET( (nDstPitch < (unsigned)roi.width),  ippStsStepErr)
    IPP_BADARG_RET( ( roi.height != 2 && roi.height != 4    && roi.height != 8 && roi.height != 16 ) ||
                    ( roi.width  != 2 && roi.width  != 4    && roi.width  != 8 && roi.width  != 16 ),
                    ippStsSizeErr)*/

#if (_IPP >= _IPP_W7) || (_IPP32E >= _IPP32E_M7)
    {
        int block, offs;
        block = (tblIndex[roi.width] << 2) + tblIndex[roi.height];
        offs  = (iOffset1 + iOffset2 + 1) >> 1;
        bidir_weight_block_H264_8u_P3P1[block]((Ipp8u *) pSrc1, (Ipp8u *)pSrc2, pDst,
            nSrcPitch1, nSrcPitch2, nDstPitch, ulog2wd,
            iWeight1, iWeight2, offs, roi);
    }
#else
    {
    Ipp32u uRound;
    Ipp32s xpos, ypos;
    Ipp32s weighted_sample;

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
            if (weighted_sample > 255) weighted_sample = 255;
            if (weighted_sample < 0) weighted_sample = 0;
            pDst[2*xpos] = (Ipp8u)weighted_sample;

            weighted_sample = ((((Ipp32s)pSrc1[2*xpos + 1]*iWeightV1 +
                (Ipp32s)pSrc2[2*xpos + 1]*iWeightV2 + (Ipp32s)uRound)>>(ulog2wd+1))) +
                ((iOffsetV1 + iOffsetV2 + 1)>>1);
            /*  clamp to 0..255. May be able to use ClampVal table for this,
                if range of unclamped weighted_sample can be guaranteed not
                to exceed table bounds. */
            if (weighted_sample > 255) weighted_sample = 255;
            if (weighted_sample < 0) weighted_sample = 0;
            pDst[2*xpos + 1] = (Ipp8u)weighted_sample;
        }
        pSrc1 += nSrcPitch1;
        pSrc2 += nSrcPitch2;
        pDst += nDstPitch;
    }
    }
#endif
    return ippStsNoErr;

} /* ippiBiDirWeightBlock_H264_8u_P3P1R */


IPPFUN(IppStatus, ippiReconstructChromaIntraHalfsMB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcCoeff,
       Ipp8u *pSrcDstUVPlane,
       Ipp32u srcdstUVStep,
       IppIntraChromaPredMode_H264 intra_chroma_mode,
       Ipp32u cbp4x4,
       Ipp32u ChromaQP,
       Ipp8u edge_type_top,
       Ipp8u edge_type_bottom))
{
    IppStatus sts;
    Ipp8u  *pUV = pSrcDstUVPlane;

//#if ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T) || (_IPP_ARCH  == _IPP_ARCH_LP64 ))
//    Ipp64s j,i;
//#else /* !((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T))) */
    Ipp32s j,i;
//#endif /* ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T)) */

    /* check error(s) */
    IPP_BAD_PTR3_RET(ppSrcCoeff, pSrcDstUVPlane, *ppSrcCoeff)
    IPP_BAD_RANGE_RET((Ipp32s)ChromaQP, 0, 51)

    if (intra_chroma_mode == IPP_CHROMA_DC)
    {
        Ipp32s Su0, Su1, Su2;
        Ipp32s Sv0, Sv1, Sv2;
        Ipp8u PredictU[2];
        Ipp8u PredictV[2];
        Ipp8u* pu0 = pUV,     *pu1 = pUV + 8;
        Ipp8u* pv0 = pUV + 1, *pv1 = pUV + 8 + 1;
        Ipp32u edge_type = edge_type_top;

        /* first cicle iteration for upper halfblock, */
        {
            edge_type = edge_type_top;

            if (!(edge_type_top & IPPVC_TOP_EDGE) && !(edge_type & IPPVC_LEFT_EDGE))
            {
                Su0 = Su1 = Su2 = 0;
                Sv0 = Sv1 = Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i - (int)srcdstUVStep];
                    Su1 += pUV[2*i + 8 - (int)srcdstUVStep];
                    Su2 += pu0[i * (int)srcdstUVStep - 2];
                    Sv0 += pUV[1 + 2*i - (int)srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - (int)srcdstUVStep];
                    Sv2 += pv0[i * (int)srcdstUVStep - 2];
                }
                PredictU[0] = (Ipp8u)((Su0 + Su2 + 4)>>3);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + Sv2 + 4)>>3);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type_top & IPPVC_TOP_EDGE))
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i - (int)srcdstUVStep];
                    Su1 += pUV[2*i + 8 - (int)srcdstUVStep];
                    Sv0 += pUV[1 + 2*i - (int)srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - (int)srcdstUVStep];
                }
                PredictU[0] = (Ipp8u)((Su0 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type & IPPVC_LEFT_EDGE))
            {
                Su2 = 0;
                Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su2 += pu0[i * (int)srcdstUVStep - 2];
                    Sv2 += pv0[i * (int)srcdstUVStep - 2];
                }
                PredictU[0] = PredictU[1] = (Ipp8u)((Su2 + 2)>>2);
                PredictV[0] = PredictV[1] = (Ipp8u)((Sv2 + 2)>>2);
            }
            else
            {
                PredictU[0] = PredictU[1] = 128;
                PredictV[0] = PredictV[1] = 128;
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];
                }
                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;
            }
        }
        /* second cicle iteration for lower halfblock */
        {
            edge_type = edge_type_bottom;

            if (!(edge_type_top & IPPVC_TOP_EDGE) && !(edge_type & IPPVC_LEFT_EDGE))
            {
                Su0 = Su1 = Su2 = 0;
                Sv0 = Sv1 = Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su1 += pUV[2*i + 8 - (int)srcdstUVStep];
                    Su2 += pu0[i * (int)srcdstUVStep - 2];
                    Sv1 += pUV[1 + 2*i + 8 - (int)srcdstUVStep];
                    Sv2 += pv0[i * (int)srcdstUVStep - 2];
                }
                PredictU[0] = (Ipp8u)((Su2 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + Su2 + 4)>>3);
                PredictV[0] = (Ipp8u)((Sv2 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + Sv2 + 4)>>3);
            }
            else if (!(edge_type_top & IPPVC_TOP_EDGE))
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i - (int)srcdstUVStep];
                    Su1 += pUV[2*i + 8 - (int)srcdstUVStep];
                    Sv0 += pUV[1 + 2*i - (int)srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - (int)srcdstUVStep];
                }
                PredictU[0] = (Ipp8u)((Su0 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type & IPPVC_LEFT_EDGE))
            {
                Su2 = 0;
                Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su2 += pu0[i * (int)srcdstUVStep - 2];
                    Sv2 += pv0[i * (int)srcdstUVStep - 2];
                }
                PredictU[0] = PredictU[1] = (Ipp8u)((Su2 + 2)>>2);
                PredictV[0] = PredictV[1] = (Ipp8u)((Sv2 + 2)>>2);
            }
            else
            {
                PredictU[0] = PredictU[1] = 128;
                PredictV[0] = PredictV[1] = 128;
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];
                }
                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;
            }
        }
    }
    else if (IPP_CHROMA_VERT == intra_chroma_mode)
    {
        if (0 == (edge_type_top & IPPVC_TOP_EDGE))
        {
            Ipp8u* puv = pUV - srcdstUVStep;

#if ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T) || (_IPP_ARCH  == _IPP_ARCH_LP64 ) || (_IPP_ARCH == _IPP_ARCH_XSC))
            for (i = 0; i < 8; i++)
            {
                for (j = 0;  j < 8; j++)
                {
                    pU[j] = pu[j];
                    pV[j] = pv[j];
                }
                pU += srcdstUVStep;
                pV += srcdstUVStep;
            }
#else
            for (i = 0; i < 8; i++)
            {
                *(Ipp32s *)&pUV[0] = *(Ipp32s *)&puv[0];
                *(Ipp32s *)&pUV[4] = *(Ipp32s *)&puv[4];
                *(Ipp32s *)&pUV[8] = *(Ipp32s *)&puv[8];
                *(Ipp32s *)&pUV[12] = *(Ipp32s *)&puv[12];
                pUV += srcdstUVStep;
           }
#endif
        }
        else
            return ippStsLPCCalcErr;
    }
    else
        return ippStsLPCCalcErr;

    sts = ippiReconstructChromaInterMB_H264_16s8u_C2R(
            ppSrcCoeff,pSrcDstUVPlane,srcdstUVStep,cbp4x4,ChromaQP);

    return sts;
} /* ippiReconstructChromaIntraHalfsMB_H264_16s8u_P2R() */


IPPFUN(IppStatus, ippiFilterDeblockingChroma_NV12_VerEdge_MBAFF_H264_8u_C1IR, (const IppiFilterDeblock_8u * pDeblockInfo))
{
    Ipp32s i;
    Ipp32u Aedge, Ap1, Aq1; /* pixel activity measures */
    Ipp8u p0, p1;
    Ipp8u q0, q1;
    Ipp8u uStrong;
    Ipp8u uClip1;
    Ipp32s iDelta;
    Ipp32s iFiltPel;

    /* check error(s) */
    IPP_BAD_PTR4_RET(pDeblockInfo, pDeblockInfo->pSrcDstPlane, pDeblockInfo->pThresholds, pDeblockInfo->pBs);

#if (_IPP_ARCH == _IPP_ARCH_IA64) || (_IPP_ARCH == _IPP_ARCH_XSC)
    if ((0 == pBs[0]) &&
        (0 == pBs[1]) &&
        (0 == pBs[2]) &&
        (0 == pBs[3]))
        return ippStsNoErr;
#else
    if (0 == *((Ipp32u *) pDeblockInfo->pBs))
        return ippStsNoErr;
#endif

    Ipp32s plane;
    for (plane = 0; plane <= 1; plane += 1)
    {
        Ipp8u *pSrcDst = pDeblockInfo->pSrcDstPlane + plane;
        const Ipp8u *pThresholds = pDeblockInfo->pThresholds + 8*plane;
        Ipp32u nAlpha = pDeblockInfo->pAlpha[2*plane];
        Ipp32u nBeta = pDeblockInfo->pBeta[2*plane];

        if (nAlpha == 0) continue;

        for (i = 0; i < 4; i += 1, pSrcDst += pDeblockInfo->srcDstStep)
        {
            /* Filter this bit position? */
            uStrong = pDeblockInfo->pBs[i];
            if (uStrong)
            {
                /* filter used is dependent upon pixel activity on each side of edge */
                p0 = *(pSrcDst - 1 * 2);
                q0 = *(pSrcDst);
                Aedge = ABS(p0 - q0);
                /* filter edge only when abs(p0-q0) < nAlpha */
                if (Aedge >= nAlpha)
                    continue;

                p1  = *(pSrcDst - 2 * 2);
                Ap1 = ABS(p1 - p0);
                /* filter edge only when abs(p1-p0) < nBeta */
                if (Ap1 >= nBeta)
                    continue;

                q1  = *(pSrcDst + 1 * 2);
                Aq1 = ABS(q1 - q0);
                /* filter edge only when abs(q1-q0) < nBeta */
                if (Aq1 >= nBeta)
                    continue;

                /* strong filtering */
                if (4 == uStrong)
                {
                    *(pSrcDst - 1 * 2) = (Ipp8u)((Ipp32s)(2 * p1 + p0 + q1 + 2) >> 2);
                    *(pSrcDst) = (Ipp8u)((Ipp32s)(2 * q1 + q0 + p1 + 2) >> 2);
                }
                /* weak filtering */
                else
                {
                    uClip1 = (Ipp8u) (pThresholds[i] + 1);
                    /* p0, q0 */
                    iDelta = (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3;
                    if (iDelta)
                    {
                        iDelta = clipd1(iDelta, uClip1);
                        iFiltPel = p0 + iDelta;
                        *(pSrcDst - 1 * 2) = ClampVal(iFiltPel);
                        iFiltPel = q0 - iDelta;
                        *(pSrcDst) = ClampVal(iFiltPel);
                    }
                }
            }
        }
    }
    return ippStsNoErr;

} /* ippiFilterDeblockingChroma_VerEdge_MBAFF_H264_8u_C1IR */


IPPFUN(IppStatus, ippiReconstructChromaIntraHalfs4x4MB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcDstCoeff,
                                                                        Ipp8u *pSrcDstUVPlane,
                                                                        Ipp32u _srcdstUVStep,
                                                                        IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                                        Ipp32u cbp4x4,
                                                                        Ipp32u chromaQPU,
                                                                        Ipp32u chromaQPV,
                                                                        Ipp8u edge_type_top,
                                                                        Ipp8u edge_type_bottom,
                                                                        const Ipp16s *pQuantTableU,
                                                                        const Ipp16s *pQuantTableV,
                                                                        Ipp8u bypass_flag))
{
    int srcdstUVStep = (int)_srcdstUVStep;
    IppStatus sts;
    Ipp8u* pUV = pSrcDstUVPlane;

//#if ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T))
//    Ipp64s j,i;
//#else /* !((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T))) */
    Ipp32s j,i;
//#endif /* ((_IPP_ARCH  == _IPP_ARCH_IA64) || (_IPP_ARCH  == _IPP_ARCH_EM64T)) */

    /* check error(s) */
    IPP_BAD_PTR2_RET(ppSrcDstCoeff, pSrcDstUVPlane);
    IPP_BAD_PTR3_RET(pQuantTableU,pQuantTableV,*ppSrcDstCoeff);
    IPP_BADARG_RET(39 < ((unsigned) chromaQPU), ippStsOutOfRangeErr);
    IPP_BADARG_RET(39 < ((unsigned) chromaQPV), ippStsOutOfRangeErr);

    if (intra_chroma_mode == IPP_CHROMA_DC)
    {
        Ipp32s Su0, Su1, Su2;
        Ipp32s Sv0, Sv1, Sv2;
        Ipp8u PredictU[2];
        Ipp8u PredictV[2];
        Ipp8u* pu0 = pUV,     *pu1 = pUV + 8;
        Ipp8u* pv0 = pUV + 1, *pv1 = pUV + 8 + 1;
        Ipp32u edge_type = edge_type_top;

        /* first cicle iteration for upper halfblock, */
        {
            edge_type = edge_type_top;

            if (!(edge_type_top & IPPVC_TOP_EDGE) && !(edge_type & IPPVC_LEFT_EDGE))
            {
                Su0 = Su1 = Su2 = 0;
                Sv0 = Sv1 = Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i     - srcdstUVStep];
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Su2 += pu0[i * srcdstUVStep - 2];
                    Sv0 += pUV[1 + 2*i     - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                    Sv2 += pv0[i * srcdstUVStep - 2];
                }
                PredictU[0] = (Ipp8u)((Su0 + Su2 + 4)>>3);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + Sv2 + 4)>>3);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type_top & IPPVC_TOP_EDGE))
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i     - srcdstUVStep];
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Sv0 += pUV[1 + 2*i     - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                }
                PredictU[0] = (Ipp8u)((Su0 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type & IPPVC_LEFT_EDGE))
            {
                Su2 = 0;
                Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su2 += pu0[i * srcdstUVStep - 2];
                    Sv2 += pv0[i * srcdstUVStep - 2];
                }
                PredictU[0] = PredictU[1] = (Ipp8u)((Su2 + 2)>>2);
                PredictV[0] = PredictV[1] = (Ipp8u)((Sv2 + 2)>>2);
            }
            else
            {
                PredictU[0] = PredictU[1] = 128;
                PredictV[0] = PredictV[1] = 128;
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];
                }
                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;
            }
        }
        /* second cicle iteration for lower halfblock */
        {
            edge_type = edge_type_bottom;

            if (!(edge_type_top & IPPVC_TOP_EDGE) && !(edge_type & IPPVC_LEFT_EDGE))
            {
                Su0 = Su1 = Su2 = 0;
                Sv0 = Sv1 = Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Su2 += pu0[i * srcdstUVStep - 2];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                    Sv2 += pv0[i * srcdstUVStep - 2];
                }
                PredictU[0] = (Ipp8u)((Su2 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + Su2 + 4)>>3);
                PredictV[0] = (Ipp8u)((Sv2 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + Sv2 + 4)>>3);
            }
            else if (!(edge_type_top & IPPVC_TOP_EDGE))
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i     - srcdstUVStep];
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Sv0 += pUV[1 + 2*i     - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                }
                PredictU[0] = (Ipp8u)((Su0 + 2)>>2);
                PredictU[1] = (Ipp8u)((Su1 + 2)>>2);
                PredictV[0] = (Ipp8u)((Sv0 + 2)>>2);
                PredictV[1] = (Ipp8u)((Sv1 + 2)>>2);
            }
            else if (!(edge_type & IPPVC_LEFT_EDGE))
            {
                Su2 = 0;
                Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su2 += pu0[i * srcdstUVStep - 2];
                    Sv2 += pv0[i * srcdstUVStep - 2];
                }
                PredictU[0] = PredictU[1] = (Ipp8u)((Su2 + 2)>>2);
                PredictV[0] = PredictV[1] = (Ipp8u)((Sv2 + 2)>>2);
            }
            else
            {
                PredictU[0] = PredictU[1] = 128;
                PredictV[0] = PredictV[1] = 128;
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];
                }
                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;
            }
        }
    }
    else if (IPP_CHROMA_VERT == intra_chroma_mode)
    {
        if (0 == (edge_type_top & IPPVC_TOP_EDGE))
        {
            Ipp8u* puv = pUV - srcdstUVStep;

#if ((_IPP_ARCH  == _IPP_ARCH_IA64)||(_IPP_ARCH  == _IPP_ARCH_EM64T)||(_IPP_ARCH == _IPP_ARCH_XSC))
            for ( i = 0; i < 8; i++)
            {
                for (j = 0;  j < 8; j++)
                {
                    pU[j] = pu[j];
                    pV[j] = pv[j];
                }
                pU += srcdstUVStep;
                pV += srcdstUVStep;
            }
#else
            for ( i = 0; i < 8; i++)
            {
                *(Ipp32s*)&pUV[0] = *(Ipp32s*)&puv[0];
                *(Ipp32s*)&pUV[4] = *(Ipp32s*)&puv[4];
                *(Ipp32s*)&pUV[8] = *(Ipp32s*)&puv[8];
                *(Ipp32s*)&pUV[12] = *(Ipp32s*)&puv[12];
                pUV += srcdstUVStep;
            }
#endif
        }
        else
            return ippStsLPCCalcErr;
    }
    else
        return ippStsLPCCalcErr;

    sts = ippiReconstructChromaInter4x4MB_H264_16s8u_C2R(
            ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
              cbp4x4,chromaQPU,chromaQPV,pQuantTableU,pQuantTableV,bypass_flag);

    return sts;
} /* ippiReconstructChromaIntraHalfs4x4MB_H264_16s8u_P2R() */


//////////// pvch264dequant.c ///////////////////////////////////////////
/* dequantization table(s) */
/* dequantization table(s) */
static const Ipp32s QuantIndex[16] =
{
    0,2,0,2,2,1,2,1,0,2,0,2,2,1,2,1
};

static const Ipp32s InvQuantTable[52][3] =
{
    {10,16,13},
    {11,18,14},
    {13,20,16},
    {14,23,18},
    {16,25,20},
    {18,29,23},
    {20,32,26},
    {22,36,28},
    {26,40,32},
    {28,46,36},
    {32,50,40},
    {36,58,46},
    {40,64,52},
    {44,72,56},
    {52,80,64},
    {56,92,72},
    {64,100,80},
    {72,116,92},
    {80,128,104},
    {88,144,112},
    {104,160,128},
    {112,184,144},
    {128,200,160},
    {144,232,184},
    {160,256,208},
    {176,288,224},
    {208,320,256},
    {224,368,288},
    {256,400,320},
    {288,464,368},
    {320,512,416},
    {352,576,448},
    {416,640,512},
    {448,736,576},
    {512,800,640},
    {576,928,736},
    {640,1024,832},
    {704,1152,896},
    {832,1280,1024},
    {896,1472,1152},
    {1024,1600,1280},
    {1152,1856,1472},
    {1280,2048,1664},
    {1408,2304,1792},
    {1664,2560,2048},
    {1792,2944,2304},
    {2048,3200,2560},
    {2304,3712,2944},
    {2560,4096,3328},
    {2816,4608,3584},
    {3328,5120,4096},
    {3584,5888,4608}
};

#ifndef imp_ippiDequantChroma_H264_16s_C1
#define imp_ippiDequantChroma_H264_16s_C1

IPPFUN(IppStatus, ippiDequantChroma_H264_16s_C1, (Ipp16s **ppSrcCoeff,
       Ipp16s *pDst,
       const Ipp32u cbp4x4,
       const Ipp32u ChromaQP,
       Ipp8u iblFlag))
{
    Ipp32s uBlock, i, ChromaPlane;
    Ipp32u uCBPMaskDC, uCBPMaskAC;
    Ipp16s a[4], tmpDst[2][4];

    /* check error(s) */
    IPP_BAD_PTR3_RET(ppSrcCoeff, *ppSrcCoeff, pDst);
    IPP_BADARG_RET(39 < ((unsigned) ChromaQP), ippStsOutOfRangeErr);

    uCBPMaskAC = (1<<IPPVC_CBP_1ST_CHROMA_AC_BITPOS);
    uCBPMaskDC = (1<<IPPVC_CBP_1ST_CHROMA_DC_BITPOS);

    for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++, uCBPMaskDC <<= 1)
    {
        if ((cbp4x4 & uCBPMaskDC) != 0)
        {
            if (iblFlag) {
                a[0] = (Ipp16s)((*ppSrcCoeff)[0] * InvQuantTable[ChromaQP][0]);
                a[1] = (Ipp16s)((*ppSrcCoeff)[1] * InvQuantTable[ChromaQP][0]);
                a[2] = (Ipp16s)((*ppSrcCoeff)[2] * InvQuantTable[ChromaQP][0]);
                a[3] = (Ipp16s)((*ppSrcCoeff)[3] * InvQuantTable[ChromaQP][0]);
                *ppSrcCoeff += 4;
            } else {
                a[0] = (Ipp16s)((*ppSrcCoeff)[64*ChromaPlane+16*0] * InvQuantTable[ChromaQP][0]);
                a[1] = (Ipp16s)((*ppSrcCoeff)[64*ChromaPlane+16*1] * InvQuantTable[ChromaQP][0]);
                a[2] = (Ipp16s)((*ppSrcCoeff)[64*ChromaPlane+16*2] * InvQuantTable[ChromaQP][0]);
                a[3] = (Ipp16s)((*ppSrcCoeff)[64*ChromaPlane+16*3] * InvQuantTable[ChromaQP][0]);
            }

            tmpDst[ChromaPlane][0] = (Ipp16s) (((a[0] + a[2]) + (a[1] + a[3])) >> 1);
            tmpDst[ChromaPlane][1] = (Ipp16s) (((a[0] + a[2]) - (a[1] + a[3])) >> 1);
            tmpDst[ChromaPlane][2] = (Ipp16s) (((a[0] - a[2]) + (a[1] - a[3])) >> 1);
            tmpDst[ChromaPlane][3] = (Ipp16s) (((a[0] - a[2]) - (a[1] - a[3])) >> 1);
        }
        else
        {
            tmpDst[ChromaPlane][0] = 0;
            tmpDst[ChromaPlane][1] = 0;
            tmpDst[ChromaPlane][2] = 0;
            tmpDst[ChromaPlane][3] = 0;
        }
    }

    for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++)
    {
        for (uBlock = 0; uBlock < 4; uBlock++, uCBPMaskAC <<= 1)
        {
            if ((cbp4x4 & uCBPMaskAC) != 0)
            {
                for (i = 1; i < 16; i++)
                    pDst[i] = (Ipp16s) ((*ppSrcCoeff)[i] * InvQuantTable[ChromaQP][QuantIndex[i]]);
            }
            else
            {
                for (i = 1; i < 16; i++)
                    pDst[i] = 0;
            }

            if (!iblFlag || (cbp4x4 & uCBPMaskAC))
                *ppSrcCoeff += 16;

            pDst[0] = tmpDst[ChromaPlane][uBlock];

            pDst += 16;

        } /* for uBlock */
    } /* chroma planes */

    return ippStsNoErr;
}

#endif /* imp_ippiDequantChroma_H264_16s_C1 */

#ifndef imp_ippiDequantChromaHigh_H264_16s_C1
#define imp_ippiDequantChromaHigh_H264_16s_C1

IPPFUN(IppStatus, ippiDequantChromaHigh_H264_16s_C1, (Ipp16s **ppSrcCoeff,
       Ipp16s *pDst,
       const Ipp32u cbp4x4,
       const Ipp32s ChromaQPU,
       const Ipp32s ChromaQPV,
       const Ipp16s *pQuantTable0,
       const Ipp16s *pQuantTable1,
       Ipp8u iblFlag))
{
    Ipp32s uBlock, i, ChromaPlane;
    Ipp32u uCBPMaskDC, uCBPMaskAC;
    Ipp16s a[4], tmpDst[2][4];
    Ipp32s q, q1, q2, qp6;
    Ipp32s ChromaQP[2] = {ChromaQPU, ChromaQPV};
    const Ipp16s *pQuantTable[2] = {pQuantTable0, pQuantTable1};

    /* check error(s) */
    IPP_BAD_PTR3_RET(ppSrcCoeff, *ppSrcCoeff, pDst);
    IPP_BAD_PTR2_RET(pQuantTable0, pQuantTable1);
    IPP_BADARG_RET(39 < ((unsigned) ChromaQPU), ippStsOutOfRangeErr);
    IPP_BADARG_RET(39 < ((unsigned) ChromaQPV), ippStsOutOfRangeErr);

    uCBPMaskAC = (1<<IPPVC_CBP_1ST_CHROMA_AC_BITPOS);
    uCBPMaskDC = (1<<IPPVC_CBP_1ST_CHROMA_DC_BITPOS);

    for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++, uCBPMaskDC <<= 1)
    {
        if ((cbp4x4 & uCBPMaskDC) != 0)
        {
            q = pQuantTable[ChromaPlane][0];
            qp6 = ChromaQP[ChromaPlane]/6;

            if (iblFlag) {
                a[0] = (Ipp16s)((*ppSrcCoeff)[0]);
                a[1] = (Ipp16s)((*ppSrcCoeff)[1]);
                a[2] = (Ipp16s)((*ppSrcCoeff)[2]);
                a[3] = (Ipp16s)((*ppSrcCoeff)[3]);
                *ppSrcCoeff += 4;
            } else {
                a[0] = (Ipp16s)((*ppSrcCoeff)[ChromaPlane*64+16*0]);
                a[1] = (Ipp16s)((*ppSrcCoeff)[ChromaPlane*64+16*1]);
                a[2] = (Ipp16s)((*ppSrcCoeff)[ChromaPlane*64+16*2]);
                a[3] = (Ipp16s)((*ppSrcCoeff)[ChromaPlane*64+16*3]);
            }

            tmpDst[ChromaPlane][0] = (Ipp16s) (((a[0] + a[2]) + (a[1] + a[3])));
            tmpDst[ChromaPlane][1] = (Ipp16s) (((a[0] + a[2]) - (a[1] + a[3])));
            tmpDst[ChromaPlane][2] = (Ipp16s) (((a[0] - a[2]) + (a[1] - a[3])));
            tmpDst[ChromaPlane][3] = (Ipp16s) (((a[0] - a[2]) - (a[1] - a[3])));

            tmpDst[ChromaPlane][0] = (Ipp16s) (((tmpDst[ChromaPlane][0] * q) << qp6) >> 5);
            tmpDst[ChromaPlane][1] = (Ipp16s) (((tmpDst[ChromaPlane][1] * q) << qp6) >> 5);
            tmpDst[ChromaPlane][2] = (Ipp16s) (((tmpDst[ChromaPlane][2] * q) << qp6) >> 5);
            tmpDst[ChromaPlane][3] = (Ipp16s) (((tmpDst[ChromaPlane][3] * q) << qp6) >> 5);
        }
        else
        {
            tmpDst[ChromaPlane][0] = 0;
            tmpDst[ChromaPlane][1] = 0;
            tmpDst[ChromaPlane][2] = 0;
            tmpDst[ChromaPlane][3] = 0;
        }
    }

    for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++)
    {
        qp6 = ChromaQP[ChromaPlane]/6;

        for (uBlock = 0; uBlock < 4; uBlock++, uCBPMaskAC <<= 1)
        {
            if ((cbp4x4 & uCBPMaskAC) != 0)
            {
                if (/*!bypass_flag || */ChromaQP[ChromaPlane] >= 0)
                {
                    for (i = 1; i < 16; i++){
                        if(ChromaQP[ChromaPlane] < 24) {
                            q1 = 1 << (3 - qp6);
                            q2 = 4 - qp6;
                            pDst[i] = (Ipp16s) (((*ppSrcCoeff)[i] * pQuantTable[ChromaPlane][i] + q1) >> q2);
                        } else {
                            q2 = qp6 - 4;
                            pDst[i] = (Ipp16s) (((*ppSrcCoeff)[i] * pQuantTable[ChromaPlane][i]) << q2);
                        }
                    }
                }
            }
            else
            {
                for (i = 1; i < 16; i++)
                    pDst[i] = 0;
            }

            if (!iblFlag || (cbp4x4 & uCBPMaskAC)) {
                *ppSrcCoeff += 16;
            }

            pDst[0] = tmpDst[ChromaPlane][uBlock];
            pDst += 16;
        } /* for uBlock */
    } /* chroma planes */

    return ippStsNoErr;
}

#endif /* imp_ippiDequantChromaHigh_H264_16s_C1 */

#ifndef imp_ippiDequantResidual_H264_16s_C1
#define imp_ippiDequantResidual_H264_16s_C1

IPPFUN(IppStatus, ippiDequantResidual_H264_16s_C1, (const Ipp16s *pCoeffs,
       Ipp16s *pDst,
       Ipp32s qp))
{
    Ipp32s i;

    /* bad arguments */
    if ((qp < 0) || (qp > 51))
        return ippStsOutOfRangeErr;

    IPP_BAD_PTR2_RET(pCoeffs, pDst)

        for (i = 0; i < 16; i++)
        {
            pDst[i] = (Ipp16s) (pCoeffs[i] * InvQuantTable[qp][QuantIndex[i]]);
        }

        return ippStsNoErr;

}

#endif /* imp_ippiDequantResidual_H264_16s_C1 */

#ifndef imp_ippiDequantResidual4x4_H264_16s_C1
#define imp_ippiDequantResidual4x4_H264_16s_C1

IPPFUN(IppStatus, ippiDequantResidual4x4_H264_16s_C1, (const Ipp16s *pCoeffs,
       Ipp16s *pDst,
       Ipp32s qp,
       const Ipp16s *pQuantTable,
       Ipp8u bypass_flag))
{
    Ipp32s i;
    Ipp32s q1 = 0, q2 = 0, qp6, start = 0;

    /* bad arguments */
    if ((qp < 0) || (qp > 51))
        return ippStsOutOfRangeErr;

    IPP_BAD_PTR3_RET(pCoeffs, pDst, pQuantTable)

        if (!bypass_flag || qp > 0)
        {
            qp6 = qp/6;
            for (i = start; i < 16; i++)
            {
                if (pCoeffs[i])
                {
                    if(qp < 24) {
                        q1 = 1 << (3 - qp6);
                        q2 = 4 - qp6;
                        pDst[i] = (Ipp16s) ((pCoeffs[i] * pQuantTable[i] + q1) >> q2);
                    } else {
                        q2 = qp6 - 4;
                        pDst[i] = (Ipp16s) ((pCoeffs[i] * pQuantTable[i]) << q2);
                    }
                } else {
                    pDst[i] = 0;
                }
            }
        }

        return ippStsNoErr;

}

#endif /* imp_ippiDequantResidual4x4_H264_16s_C1 */

#ifndef imp_ippiDequantResidual8x8_H264_16s_C1
#define imp_ippiDequantResidual8x8_H264_16s_C1

IPPFUN(IppStatus, ippiDequantResidual8x8_H264_16s_C1, (const Ipp16s *pCoeffs,
       Ipp16s *pDst,
       Ipp32s qp,
       const Ipp16s *pQuantTable,
       Ipp8u bypass_flag))
{
    Ipp32s i;
    Ipp32s q1 = 0, q2 = 0, qp6;

    //    if (1) return ippiQuantLuma8x8Inv_H264_16s_C1I(pDst, qp, pCoeffs);

    /* bad arguments */
    if ((qp < 0) || (qp > 51))
        return ippStsOutOfRangeErr;

    IPP_BAD_PTR3_RET(pCoeffs, pDst, pQuantTable)

        if (!bypass_flag || qp > 0)
        {
            qp6 = qp/6;

            if(qp < 36)
            {
                q1 = 1 << (5 - qp6);
                q2 = 6 - qp6;
                for ( i = 0; i < 64; i++ )
                {
                    pDst[i] = (Ipp16s) ((pCoeffs[i] * pQuantTable[i] + q1) >> q2);
                }
            } else {
                q2 = qp6 - 6;
                for ( i = 0; i < 64; i++ )
                {
                    pDst[i] = (Ipp16s) ((pCoeffs[i] * pQuantTable[i]) << q2);
                }
            }
        }

        return ippStsNoErr;
}

#endif /* imp_ippiDequantResidual8x8_H264_16s_C1 */

///////////////////////////////////////////////////////////////


////////////////// pvch264huffman.c ///////////////////////////
#define h264GetBits(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    /*removeSCEBP(current_data, offset);*/ \
    offset -= (nbits); \
    if (offset >= 0) \
{ \
    x = current_data[0] >> (offset + 1); \
} \
    else \
{ \
    offset += 32; \
    x = current_data[1] >> (offset); \
    x >>= 1; \
    x += current_data[0] << (31 - offset); \
    current_data++; \
} \
    (data) = x & ((((Ipp32u) 0x01) << (nbits)) - 1); \
}

#define h264GetBits1(current_data, offset, data) h264GetBits(current_data, offset,  1, data);
#define h264GetBits8(current_data, offset, data) h264GetBits(current_data, offset,  8, data);
#define h264GetNBits(current_data, offset, nbits, data) h264GetBits(current_data, offset, nbits, data);

#define h264UngetNBits(current_data, offset, nbits) \
{ \
    offset += (nbits); \
    if (offset > 31) \
{ \
    offset -= 32; \
    current_data--; \
} \
}
static Ipp32s H264TotalCoeffTrailingOnesTab0[] = {
    0,  5, 10, 15,  4,  9, 19, 14, 23,  8, 13, 18, 27, 12, 17, 22,
    31, 16, 21, 26, 35, 20, 25, 30, 39, 24, 29, 34, 43, 28, 33, 38,
    32, 36, 37, 42, 47, 40, 41, 46, 51, 44, 45, 50, 55, 48, 49, 54,
    59, 53, 52, 57, 58, 63, 56, 61, 62, 67, 60, 65, 66, 64
};

static Ipp32s H264TotalCoeffTrailingOnesTab1[] = {
    0,  5, 10, 15, 19,  9, 23,  4, 13, 14, 27,  8, 17, 18, 31, 12,
    21, 22, 35, 16, 25, 26, 20, 24, 29, 30, 39, 28, 33, 34, 43, 32,
    37, 38, 47, 36, 41, 42, 51, 40, 45, 46, 44, 48, 49, 50, 55, 52,
    53, 54, 59, 56, 58, 63, 57, 60, 61, 62, 67, 64, 65, 66
};

static Ipp32s H264TotalCoeffTrailingOnesTab2[] = {
    0,  5, 10, 15, 19, 23, 27, 31,  9, 14, 35, 13, 18, 17, 22, 21,
    4, 25, 26, 39,  8, 29, 30, 12, 16, 33, 34, 43, 20, 38, 24, 28,
    32, 37, 42, 47, 36, 41, 46, 51, 40, 45, 50, 55, 44, 49, 54, 48,
    53, 52, 57, 58, 59, 56, 61, 62, 63, 60, 65, 66, 67, 64
};

static Ipp32s H264CoeffTokenIdxTab0[] = {
    0,  0,  0,  0,  4,  1,  0,  0,  9,  5,  2,  0, 13, 10,  7,  3,
    17, 14, 11,  6, 21, 18, 15,  8, 25, 22, 19, 12, 29, 26, 23, 16,
    32, 30, 27, 20, 33, 34, 31, 24, 37, 38, 35, 28, 41, 42, 39, 36,
    45, 46, 43, 40, 50, 49, 47, 44, 54, 51, 52, 48, 58, 55, 56, 53,
    61, 59, 60, 57
};

static Ipp32s H264CoeffTokenIdxTab1[] = {
    0,  0,  0,  0,  7,  1,  0,  0, 11,  5,  2,  0, 15,  8,  9,  3,
    19, 12, 13,  4, 22, 16, 17,  6, 23, 20, 21, 10, 27, 24, 25, 14,
    31, 28, 29, 18, 35, 32, 33, 26, 39, 36, 37, 30, 42, 40, 41, 34,
    43, 44, 45, 38, 47, 48, 49, 46, 51, 54, 52, 50, 55, 56, 57, 53,
    59, 60, 61, 58
};

static Ipp32s H264CoeffTokenIdxTab2[] = {
    0,  0,  0,  0, 16,  1,  0,  0, 20,  8,  2,  0, 23, 11,  9,  3,
    24, 13, 12,  4, 28, 15, 14,  5, 30, 17, 18,  6, 31, 21, 22,  7,
    32, 25, 26, 10, 36, 33, 29, 19, 40, 37, 34, 27, 44, 41, 38, 35,
    47, 45, 42, 39, 49, 48, 46, 43, 53, 50, 51, 52, 57, 54, 55, 56,
    61, 58, 59, 60
};

static Ipp32s* H264TotalCoeffTrailingOnesTab[] = {
    H264TotalCoeffTrailingOnesTab0,
    H264TotalCoeffTrailingOnesTab0,
    H264TotalCoeffTrailingOnesTab1,
    H264TotalCoeffTrailingOnesTab1,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2
};

static Ipp32s* H264CoeffTokenIdxTab[] = {
    H264CoeffTokenIdxTab0,
    H264CoeffTokenIdxTab0,
    H264CoeffTokenIdxTab1,
    H264CoeffTokenIdxTab1,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2
};

static const Ipp8u vlc_inc[] = {0,3,6,12,24,48};

static Ipp32s bitsToGetTbl16s[7][16] = /*[level][numZeros]*/
{
    /*         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15        */
    /*0*/    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 12, },
    /*1*/    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 12, },
    /*2*/    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 12, },
    /*3*/    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 12, },
    /*4*/    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 12, },
    /*5*/    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 12, },
    /*6*/    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12  }
};
static Ipp32s addOffsetTbl16s[7][16] = /*[level][numZeros]*/
{
    /*         0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15    */
    /*0*/    {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  8,  16,},
    /*1*/    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,},
    /*2*/    {1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,},
    /*3*/    {1,  5,  9,  13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61,},
    /*4*/    {1,  9,  17, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97, 105,113,121,},
    /*5*/    {1,  17, 33, 49, 65, 81, 97, 113,129,145,161,177,193,209,225,241,},
    /*6*/    {1,  33, 65, 97, 129,161,193,225,257,289,321,353,385,417,449,481,}
};
static Ipp32s sadd[7]={15,0,0,0,0,0,0};

#ifndef imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s
#define imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s

IPPFUN(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (Ipp32u **ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s **ppTblCoeffToken,
                                                     const Ipp32s **ppTblTotalZeros,
                                                     const Ipp32s **ppTblRunBefore,
                                                     const Ipp32s *pScanMatrix,
                                                     Ipp32s scanIdxStart,
                                                     Ipp32s scanIdxEnd)) /* buffer to return up to 16 */
{
    Ipp16s        CoeffBuf[16];    /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        uVLCIndex        = 2;
    Ipp32u        uCoeffIndex      = 0;
    Ipp32s        sTotalZeros      = 0;
    Ipp32s        sFirstPos        = scanIdxStart;
    Ipp32s        coeffLimit = scanIdxEnd - scanIdxStart + 1;
    Ipp32u        TrOneSigns = 0;        /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uTR1Mask;
    Ipp32s        pos;
    Ipp32s        sRunBefore;
    Ipp16s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;
    Ipp32u        table_bits;
    Ipp8u         code_len;
    Ipp32s        i;
    register Ipp32u  table_pos;
    register Ipp32s  val;

    /* check error(s) */
    IPP_BAD_PTR4_RET(ppBitStream,pOffset,ppPosCoefbuf,pNumCoeff)
    IPP_BAD_PTR4_RET(ppTblCoeffToken,ppTblTotalZeros,ppTblRunBefore,pScanMatrix)
    IPP_BAD_PTR2_RET(*ppBitStream, *ppPosCoefbuf)
    IPP_BADARG_RET(((Ipp32s)uVLCSelect < 0), ippStsOutOfRangeErr)

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        h264GetNBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (Ipp16s) (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;

        uVLCSelect = 7;
    }
    else
    {
        Ipp32u*  tmp_pbs = *ppBitStream;
        Ipp32s   tmp_offs = *pOffset;
        const Ipp32s *pDecTable;
        /* Use one of 3 luma tables */
        if (uVLCSelect < 4)
            uVLCIndex = uVLCSelect>>1;

        /* check for the only codeword of all zeros */
        /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, ppTblCoeffToken[uVLCIndex], */
        /*                                        &sNumTrailingOnes, &sNumCoeff); */

        pDecTable = ppTblCoeffToken[uVLCIndex];

        IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        table_bits = *pDecTable;
        h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val = pDecTable[table_pos + 1];
        code_len = (Ipp8u) (val);

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (Ipp8u) (val & 0xff);
        }

        h264UngetNBits((*ppBitStream), (*pOffset), code_len);

        if ((val>>8) == IPPVC_VLC_FORBIDDEN)
        {
             *ppBitStream = tmp_pbs;
             *pOffset = tmp_offs;

             return ippStsH263VLCCodeErr;
        }

        sNumTrailingOnes  = (Ipp16s) ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    if (coeffLimit < 16)
    {
        Ipp32s *tmpTab = H264TotalCoeffTrailingOnesTab[uVLCSelect];
        Ipp32s targetCoeffTokenIdx =
            H264CoeffTokenIdxTab[uVLCSelect][sNumCoeff * 4 + sNumTrailingOnes];
        Ipp32s minNumCoeff = coeffLimit+1;
        Ipp32s minNumTrailingOnes = coeffLimit+1;
        Ipp32s j;

        if (minNumTrailingOnes > 4)
            minNumTrailingOnes = 4;

        if (minNumCoeff > 17)
            minNumCoeff  = 17;

        for (j = 0, i = 0; i <= targetCoeffTokenIdx; j++)
        {
            sNumCoeff = tmpTab[j] >> 2;
            sNumTrailingOnes = (Ipp16s)(tmpTab[j] & 3);

            if ((sNumCoeff < minNumCoeff) && (sNumTrailingOnes < minNumTrailingOnes))
                i++;
        }

        sNumCoeff = tmpTab[j-1] >> 2;
        sNumTrailingOnes = (Ipp16s)(tmpTab[j-1] & 3);
    }

    *pNumCoeff = (Ipp16s) sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (Ipp16s) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
            uTR1Mask >>= 1;
        }
    }
    if (sNumCoeff)
    {
#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 16; i++)
            (*ppPosCoefbuf)[i] = 0;

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            /*_GetBlockCoeffs_CAVLC(ppBitStream, pOffset,sNumCoeff,*/
            /*                             sNumTrailingOnes, &CoeffBuf[uCoeffIndex]); */
            Ipp16u suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
            Ipp16s lCoeffIndex;
            Ipp16u uCoeffLevel = 0;
            Ipp32s NumZeros;
            Ipp16u uBitsToGet;
            Ipp16u uFirstAdjust;
            Ipp16u uLevelOffset;
            Ipp32s w;
            Ipp16s    *lCoeffBuf = &CoeffBuf[uCoeffIndex];

            if ((sNumCoeff > 10) && (sNumTrailingOnes < 3))
                suffixLength = 1;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            uFirstAdjust = (Ipp16u) ((sNumTrailingOnes < 3) ? 1 : 0);

            /* read coeffs */
            for (lCoeffIndex = 0; lCoeffIndex<(sNumCoeff - sNumTrailingOnes); lCoeffIndex++)
            {
                /* update suffixLength */
                if ((lCoeffIndex == 1) && (uCoeffLevel > 3))
                    suffixLength = 2;
                else if (suffixLength < 6)
                {
                    if (uCoeffLevel > vlc_inc[suffixLength])
                        suffixLength++;
                }

                /* Get the number of leading zeros to determine how many more */
                /* bits to read. */
                NumZeros = -1;
                for (w = 0; !w; NumZeros++)
                {
                    h264GetBits1((*ppBitStream), (*pOffset), w);
                }

                if (15 >= NumZeros)
                {
                    uBitsToGet = (Ipp16s) (bitsToGetTbl16s[suffixLength][NumZeros]);
                    uLevelOffset = (Ipp16u) (addOffsetTbl16s[suffixLength][NumZeros]);

                    if (uBitsToGet)
                    {
                        h264GetNBits((*ppBitStream), (*pOffset), uBitsToGet, NumZeros);
                    }

                    uCoeffLevel = (Ipp16u) ((NumZeros>>1) + uLevelOffset + uFirstAdjust);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    Ipp32u level_suffix;
                    Ipp32u levelSuffixSize = NumZeros - 3;
                    Ipp32s levelCode;

                    h264GetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = (Ipp16u) ((min(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                    levelCode = (Ipp16u) (levelCode + (1 << (NumZeros - 3)) - 4096);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = (Ipp16u) ABS(lCoeffBuf[lCoeffIndex]);
                }

                uFirstAdjust = 0;

            }    /* for uCoeffIndex */

        }
        /* Get TotalZeros if any */
        if (sNumCoeff < uMaxNumCoeff)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZeros[sNumCoeff]); */
            Ipp32s tmpNumCoeff;
            const Ipp32s *pDecTable;

            tmpNumCoeff = sNumCoeff;
            if (uMaxNumCoeff < 15)
            {
                if (uMaxNumCoeff <=4)
                {
                    tmpNumCoeff += 4 - uMaxNumCoeff;
                    if (tmpNumCoeff > 3)
                        tmpNumCoeff = 3;
                }
                else if (uMaxNumCoeff <= 8)
                {
                    tmpNumCoeff += 8 - uMaxNumCoeff;
                    if (tmpNumCoeff > 7)
                        tmpNumCoeff = 7;

                }
                else
                {
                    tmpNumCoeff += 16 - uMaxNumCoeff;
                    if (tmpNumCoeff > 15)
                        tmpNumCoeff = 15;
                }
            }

            pDecTable = ppTblTotalZeros[tmpNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZeros[sNumCoeff]);

            table_bits = pDecTable[0];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val = pDecTable[table_pos + 1];
            code_len = (Ipp8u) (val & 0xff);
            val = val >> 8;

            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val = pDecTable[table_pos + val + 1];
                code_len = (Ipp8u) (val & 0xff);
                val = val >> 8;
            }

            if (val == IPPVC_VLC_FORBIDDEN)
            {
                sTotalZeros = val;
                return ippStsH263VLCCodeErr;
            }

            h264UngetNBits((*ppBitStream), (*pOffset), code_len)

            sTotalZeros = val;
        }

        uCoeffIndex = 0;
        while (sNumCoeff)
        {
            /* Get RunBerore if any */
            if ((sNumCoeff > 1) && (sTotalZeros > 0))
            {
                /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sRunBefore, */
                /*                                                ppTblRunBefore[sTotalZeros]); */
                const Ipp32s *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros]);

                table_bits = pDecTable[0];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (Ipp8u) (val & 0xff);
                    val        = val >> 8;
                }

                if (val == IPPVC_VLC_FORBIDDEN)
                {
                    sRunBefore =  val;
                    return ippStsH263VLCCodeErr;
                }

                h264UngetNBits((*ppBitStream), (*pOffset),code_len)

                sRunBefore =  val;
            }
            else
                sRunBefore = sTotalZeros;

            /*Put coeff to the buffer */
            pos             = sNumCoeff - 1 + sTotalZeros + sFirstPos;

            sTotalZeros -= sRunBefore;
            if (sTotalZeros < 0)
                return ippStsH263VLCCodeErr;
            pos             = pScanMatrix[pos];

            (*ppPosCoefbuf)[pos] = CoeffBuf[uCoeffIndex++];
            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;


} /* IPPFUN(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (Ipp32u **ppBitStream, */

#endif /* imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s */

///////////////////////////////////////////////////////////////

/////////////// pvch264transformresidualsvc.c /////////////////


#define ClampTblLookup(x, y) ClampVal((x) + clipd1((y),256))
#define ClampTblLookupNew(x) ClampVal((x))

#ifndef imp_ippiTransformResidual_H264_16s16s_C1I
#define imp_ippiTransformResidual_H264_16s16s_C1I

IPPFUN(IppStatus, ippiTransformResidual_H264_16s16s_C1I,(const Ipp16s *pCoeffs,
                                                Ipp16s *pRec,
                                                Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR2_RET(pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = tmpBuf[i * 4 + 0];
        pRec[1] = tmpBuf[i * 4 + 1];
        pRec[2] = tmpBuf[i * 4 + 2];
        pRec[3] = tmpBuf[i * 4 + 3];
        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
    }

    return ippStsNoErr;
}

#endif /* imp_ippiTransformResidual_H264_16s16s_C1I */

#ifndef imp_ippiTransformResidual8x8_H264_16s16s_C1I
#define imp_ippiTransformResidual8x8_H264_16s16s_C1I

IPPFUN(IppStatus, ippiTransformResidual8x8_H264_16s16s_C1I, (const Ipp16s *pCoeffs,
                                                   Ipp16s *pRec,
                                                   Ipp32s recPitch))
{
    Ipp16s tmpBuf[64];
    Ipp16s a[8], b[8];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR2_RET(pCoeffs, pRec)

    for (i = 0; i < 8; i ++)
    {
        a[0] = (pCoeffs[i * 8 + 0] + pCoeffs[i * 8 + 4]);
        a[1] = (-pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] - pCoeffs[i * 8 + 7] - (pCoeffs[i * 8 + 7] >> 1));
        a[2] = (pCoeffs[i * 8 + 0] - pCoeffs[i * 8 + 4]);
        a[3] = (pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] - pCoeffs[i * 8 + 3] - (pCoeffs[i * 8 + 3] >> 1));
        a[4] = ((pCoeffs[i * 8 + 2] >> 1) - pCoeffs[i * 8 + 6]);
        a[5] = (-pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] + pCoeffs[i * 8 + 5] + (pCoeffs[i * 8 + 5] >> 1));
        a[6] = (pCoeffs[i * 8 + 2] + (pCoeffs[i * 8 + 6] >> 1));
        a[7] = (pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] + pCoeffs[i * 8 + 1] + (pCoeffs[i * 8 + 1] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[i * 8 + 0] = (Ipp16s) (b[0] + b[7]);
        tmpBuf[i * 8 + 1] = (Ipp16s) (b[2] + b[5]);
        tmpBuf[i * 8 + 2] = (Ipp16s) (b[4] + b[3]);
        tmpBuf[i * 8 + 3] = (Ipp16s) (b[6] + b[1]);
        tmpBuf[i * 8 + 4] = (Ipp16s) (b[6] - b[1]);
        tmpBuf[i * 8 + 5] = (Ipp16s) (b[4] - b[3]);
        tmpBuf[i * 8 + 6] = (Ipp16s) (b[2] - b[5]);
        tmpBuf[i * 8 + 7] = (Ipp16s) (b[0] - b[7]);

    }

    /* vertical */

    for (i = 0; i < 8; i ++)
    {
        a[0] = (tmpBuf[0 * 8 + i] + tmpBuf[4 * 8 + i]);
        a[1] = (-tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] - tmpBuf[7 * 8 + i] - (tmpBuf[7 * 8 + i] >> 1));
        a[2] = (tmpBuf[0 * 8 + i] - tmpBuf[4 * 8 + i]);
        a[3] = (tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] - tmpBuf[3 * 8 + i] - (tmpBuf[3 * 8 + i] >> 1));
        a[4] = ((tmpBuf[2 * 8 + i] >> 1) - tmpBuf[6 * 8 + i]);
        a[5] = (-tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] + tmpBuf[5 * 8 + i] + (tmpBuf[5 * 8 + i] >> 1));
        a[6] = (tmpBuf[2 * 8 + i] + (tmpBuf[6 * 8 + i] >> 1));
        a[7] = (tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] + tmpBuf[1 * 8 + i] + (tmpBuf[1 * 8 + i] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[0 * 8 + i] = (Ipp16s) ((b[0] + b[7] + 32) >> 6);
        tmpBuf[1 * 8 + i] = (Ipp16s) ((b[2] + b[5] + 32) >> 6);
        tmpBuf[2 * 8 + i] = (Ipp16s) ((b[4] + b[3] + 32) >> 6);
        tmpBuf[3 * 8 + i] = (Ipp16s) ((b[6] + b[1] + 32) >> 6);
        tmpBuf[4 * 8 + i] = (Ipp16s) ((b[6] - b[1] + 32) >> 6);
        tmpBuf[5 * 8 + i] = (Ipp16s) ((b[4] - b[3] + 32) >> 6);
        tmpBuf[6 * 8 + i] = (Ipp16s) ((b[2] - b[5] + 32) >> 6);
        tmpBuf[7 * 8 + i] = (Ipp16s) ((b[0] - b[7] + 32) >> 6);
    }

    for (i = 0; i < 8; i++)
    {
        pRec[0] = tmpBuf[i * 8 + 0];
        pRec[1] = tmpBuf[i * 8 + 1];
        pRec[2] = tmpBuf[i * 8 + 2];
        pRec[3] = tmpBuf[i * 8 + 3];
        pRec[4] = tmpBuf[i * 8 + 4];
        pRec[5] = tmpBuf[i * 8 + 5];
        pRec[6] = tmpBuf[i * 8 + 6];
        pRec[7] = tmpBuf[i * 8 + 7];

        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
    }

    return ippStsNoErr;
}

#endif /* imp_ippiTransformResidual8x8_H264_16s16s_C1I */

#ifndef imp_ippiTransformResidualAndAdd_H264_16s_C1I
#define imp_ippiTransformResidualAndAdd_H264_16s_C1I

IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s8u_C1I,(const Ipp8u *pPred,
                                                   const Ipp16s *pCoeffs,
                                                   Ipp8u *pRec,
                                                   Ipp32s predPitch,
                                                   Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = ClampTblLookup(pPred[0], tmpBuf[i * 4 + 0]);
        pRec[1] = ClampTblLookup(pPred[1], tmpBuf[i * 4 + 1]);
        pRec[2] = ClampTblLookup(pPred[2], tmpBuf[i * 4 + 2]);
        pRec[3] = ClampTblLookup(pPred[3], tmpBuf[i * 4 + 3]);
        pRec += recPitch;
        pPred += predPitch;
    }

    return ippStsNoErr;

}

#endif /* imp_ippiTransformResidualAndAdd_H264_16s_C1I */

#ifndef imp_ippiTransformResidualAndAdd8x8_H264_16s_C1I
#define imp_ippiTransformResidualAndAdd8x8_H264_16s_C1I

IPPFUN(IppStatus, ippiTransformResidualAndAdd8x8_H264_16s8u_C1I, (const Ipp8u *pPred,
                                                      const Ipp16s *pCoeffs,
                                                      Ipp8u *pRec,
                                                      Ipp32s predPitch,
                                                      Ipp32s recPitch))
{
    Ipp16s tmpBuf[64];
    Ipp16s a[8], b[8];
    Ipp32s i;


//    if (1) return ippiTransformLuma8x8InvAddPred_H264_16s8u_C1R(pPred, predPitch, pCoeffs, pRec, recPitch);

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    for (i = 0; i < 8; i ++)
    {
        a[0] = (pCoeffs[i * 8 + 0] + pCoeffs[i * 8 + 4]);
        a[1] = (-pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] - pCoeffs[i * 8 + 7] - (pCoeffs[i * 8 + 7] >> 1));
        a[2] = (pCoeffs[i * 8 + 0] - pCoeffs[i * 8 + 4]);
        a[3] = (pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] - pCoeffs[i * 8 + 3] - (pCoeffs[i * 8 + 3] >> 1));
        a[4] = ((pCoeffs[i * 8 + 2] >> 1) - pCoeffs[i * 8 + 6]);
        a[5] = (-pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] + pCoeffs[i * 8 + 5] + (pCoeffs[i * 8 + 5] >> 1));
        a[6] = (pCoeffs[i * 8 + 2] + (pCoeffs[i * 8 + 6] >> 1));
        a[7] = (pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] + pCoeffs[i * 8 + 1] + (pCoeffs[i * 8 + 1] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[i * 8 + 0] = (Ipp16s) (b[0] + b[7]);
        tmpBuf[i * 8 + 1] = (Ipp16s) (b[2] + b[5]);
        tmpBuf[i * 8 + 2] = (Ipp16s) (b[4] + b[3]);
        tmpBuf[i * 8 + 3] = (Ipp16s) (b[6] + b[1]);
        tmpBuf[i * 8 + 4] = (Ipp16s) (b[6] - b[1]);
        tmpBuf[i * 8 + 5] = (Ipp16s) (b[4] - b[3]);
        tmpBuf[i * 8 + 6] = (Ipp16s) (b[2] - b[5]);
        tmpBuf[i * 8 + 7] = (Ipp16s) (b[0] - b[7]);

    }

    /* vertical */

    for (i = 0; i < 8; i ++)
    {
        a[0] = (tmpBuf[0 * 8 + i] + tmpBuf[4 * 8 + i]);
        a[1] = (-tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] - tmpBuf[7 * 8 + i] - (tmpBuf[7 * 8 + i] >> 1));
        a[2] = (tmpBuf[0 * 8 + i] - tmpBuf[4 * 8 + i]);
        a[3] = (tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] - tmpBuf[3 * 8 + i] - (tmpBuf[3 * 8 + i] >> 1));
        a[4] = ((tmpBuf[2 * 8 + i] >> 1) - tmpBuf[6 * 8 + i]);
        a[5] = (-tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] + tmpBuf[5 * 8 + i] + (tmpBuf[5 * 8 + i] >> 1));
        a[6] = (tmpBuf[2 * 8 + i] + (tmpBuf[6 * 8 + i] >> 1));
        a[7] = (tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] + tmpBuf[1 * 8 + i] + (tmpBuf[1 * 8 + i] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[0 * 8 + i] = (Ipp16s) ((b[0] + b[7] + 32) >> 6);
        tmpBuf[1 * 8 + i] = (Ipp16s) ((b[2] + b[5] + 32) >> 6);
        tmpBuf[2 * 8 + i] = (Ipp16s) ((b[4] + b[3] + 32) >> 6);
        tmpBuf[3 * 8 + i] = (Ipp16s) ((b[6] + b[1] + 32) >> 6);
        tmpBuf[4 * 8 + i] = (Ipp16s) ((b[6] - b[1] + 32) >> 6);
        tmpBuf[5 * 8 + i] = (Ipp16s) ((b[4] - b[3] + 32) >> 6);
        tmpBuf[6 * 8 + i] = (Ipp16s) ((b[2] - b[5] + 32) >> 6);
        tmpBuf[7 * 8 + i] = (Ipp16s) ((b[0] - b[7] + 32) >> 6);
    }
    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */

    for (i = 0; i < 8; i++)
    {
        pRec[0] = ClampTblLookup(pPred[0], tmpBuf[i * 8 + 0]);
        pRec[1] = ClampTblLookup(pPred[1], tmpBuf[i * 8 + 1]);
        pRec[2] = ClampTblLookup(pPred[2], tmpBuf[i * 8 + 2]);
        pRec[3] = ClampTblLookup(pPred[3], tmpBuf[i * 8 + 3]);
        pRec[4] = ClampTblLookup(pPred[4], tmpBuf[i * 8 + 4]);
        pRec[5] = ClampTblLookup(pPred[5], tmpBuf[i * 8 + 5]);
        pRec[6] = ClampTblLookup(pPred[6], tmpBuf[i * 8 + 6]);
        pRec[7] = ClampTblLookup(pPred[7], tmpBuf[i * 8 + 7]);

        pRec += recPitch;
        pPred += predPitch;

    }

    return ippStsNoErr;
}

#endif /* imp_ippiTransformResidualAndAdd8x8_H264_16s_C1I */

#ifndef imp_ippiTransformResidualAndAdd_H264_16s16s_C1I
#define imp_ippiTransformResidualAndAdd_H264_16s16s_C1I

IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s16s_C1I, (const Ipp16s *pPred,
                                                      const Ipp16s *pCoeffs,
                                                      Ipp16s *pRec,
                                                      Ipp32s predPitch,
                                                      Ipp32s recPitch))
{
    Ipp16s tmpBuf[16];
    Ipp16s a[4];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    /* horizontal */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (pCoeffs[i * 4 + 0] + pCoeffs[i * 4 + 2]);          /* A' = A + C */
        a[1] = (Ipp16s) (pCoeffs[i * 4 + 0] - pCoeffs[i * 4 + 2]);          /* B' = A - C */
        a[2] = (Ipp16s) ((pCoeffs[i * 4 + 1] >> 1) - pCoeffs[i * 4 + 3]); /* C' = (B>>1) - D */
        a[3] = (Ipp16s) (pCoeffs[i * 4 + 1] + (pCoeffs[i * 4 + 3] >> 1)); /* D' = B + (D>>1) */

        tmpBuf[i * 4 + 0] = (Ipp16s) (a[0] + a[3]);       /* A = A' + D' */
        tmpBuf[i * 4 + 1] = (Ipp16s) (a[1] + a[2]);       /* B = B' + C' */
        tmpBuf[i * 4 + 2] = (Ipp16s) (a[1] - a[2]);       /* C = B' - C' */
        tmpBuf[i * 4 + 3] = (Ipp16s) (a[0] - a[3]);       /* A = A' - D' */
    }

    /* vertical */
    for (i = 0; i <= 3; i ++)
    {
        a[0] = (Ipp16s) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);         /*A' = A + C */
        a[1] = (Ipp16s) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);         /*B' = A - C */
        a[2] = (Ipp16s) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]);/*C' = (B>>1) - D */
        a[3] = (Ipp16s) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1));/*D' = B + (D>>1) */

        tmpBuf[0 * 4 + i] = (Ipp16s) ((a[0] + a[3] + 32) >> 6);     /*A = (A' + D' + 32) >> 6 */
        tmpBuf[1 * 4 + i] = (Ipp16s) ((a[1] + a[2] + 32) >> 6); /*B = (B' + C' + 32) >> 6 */
        tmpBuf[2 * 4 + i] = (Ipp16s) ((a[1] - a[2] + 32) >> 6); /*C = (B' - C' + 32) >> 6 */
        tmpBuf[3 * 4 + i] = (Ipp16s) ((a[0] - a[3] + 32) >> 6); /*D = (A' - D' + 32) >> 6 */
    }

    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */
    for (i = 0; i < 4; i++)
    {
        pRec[0] = pPred[0] + tmpBuf[i * 4 + 0];
        pRec[1] = pPred[1] + tmpBuf[i * 4 + 1];
        pRec[2] = pPred[2] + tmpBuf[i * 4 + 2];
        pRec[3] = pPred[3] + tmpBuf[i * 4 + 3];
        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
        pPred = (Ipp16s*)((Ipp8u*)pPred + predPitch);
    }

    return ippStsNoErr;
}

#endif /* imp_ippiTransformResidualAndAdd_H264_16s16s_C1I */

#ifndef imp_ippiTransformResidualAndAdd8x8_H264_16s16s_C1I
#define imp_ippiTransformResidualAndAdd8x8_H264_16s16s_C1I

IPPFUN(IppStatus, ippiTransformResidualAndAdd8x8_H264_16s16s_C1I,(const Ipp16s *pPred,
                                                         const Ipp16s *pCoeffs,
                                                         Ipp16s *pRec,
                                                         Ipp32s predPitch,
                                                         Ipp32s recPitch))
{
    Ipp16s tmpBuf[64];
    Ipp16s a[8], b[8];
    Ipp32s i;

    /* bad arguments */
    IPP_BAD_PTR3_RET(pPred, pCoeffs, pRec)

    for (i = 0; i < 8; i ++)
    {
        a[0] = (pCoeffs[i * 8 + 0] + pCoeffs[i * 8 + 4]);
        a[1] = (-pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] - pCoeffs[i * 8 + 7] - (pCoeffs[i * 8 + 7] >> 1));
        a[2] = (pCoeffs[i * 8 + 0] - pCoeffs[i * 8 + 4]);
        a[3] = (pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] - pCoeffs[i * 8 + 3] - (pCoeffs[i * 8 + 3] >> 1));
        a[4] = ((pCoeffs[i * 8 + 2] >> 1) - pCoeffs[i * 8 + 6]);
        a[5] = (-pCoeffs[i * 8 + 1] + pCoeffs[i * 8 + 7] + pCoeffs[i * 8 + 5] + (pCoeffs[i * 8 + 5] >> 1));
        a[6] = (pCoeffs[i * 8 + 2] + (pCoeffs[i * 8 + 6] >> 1));
        a[7] = (pCoeffs[i * 8 + 3] + pCoeffs[i * 8 + 5] + pCoeffs[i * 8 + 1] + (pCoeffs[i * 8 + 1] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[i * 8 + 0] = (Ipp16s) (b[0] + b[7]);
        tmpBuf[i * 8 + 1] = (Ipp16s) (b[2] + b[5]);
        tmpBuf[i * 8 + 2] = (Ipp16s) (b[4] + b[3]);
        tmpBuf[i * 8 + 3] = (Ipp16s) (b[6] + b[1]);
        tmpBuf[i * 8 + 4] = (Ipp16s) (b[6] - b[1]);
        tmpBuf[i * 8 + 5] = (Ipp16s) (b[4] - b[3]);
        tmpBuf[i * 8 + 6] = (Ipp16s) (b[2] - b[5]);
        tmpBuf[i * 8 + 7] = (Ipp16s) (b[0] - b[7]);

    }

    /* vertical */

    for (i = 0; i < 8; i ++)
    {
        a[0] = (tmpBuf[0 * 8 + i] + tmpBuf[4 * 8 + i]);
        a[1] = (-tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] - tmpBuf[7 * 8 + i] - (tmpBuf[7 * 8 + i] >> 1));
        a[2] = (tmpBuf[0 * 8 + i] - tmpBuf[4 * 8 + i]);
        a[3] = (tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] - tmpBuf[3 * 8 + i] - (tmpBuf[3 * 8 + i] >> 1));
        a[4] = ((tmpBuf[2 * 8 + i] >> 1) - tmpBuf[6 * 8 + i]);
        a[5] = (-tmpBuf[1 * 8 + i] + tmpBuf[7 * 8 + i] + tmpBuf[5 * 8 + i] + (tmpBuf[5 * 8 + i] >> 1));
        a[6] = (tmpBuf[2 * 8 + i] + (tmpBuf[6 * 8 + i] >> 1));
        a[7] = (tmpBuf[3 * 8 + i] + tmpBuf[5 * 8 + i] + tmpBuf[1 * 8 + i] + (tmpBuf[1 * 8 + i] >> 1));

        b[0] = (a[0] + a[6]);
        b[1] = (a[1] + (a[7] >> 2));
        b[2] = (a[2] + a[4]);
        b[3] = (a[3] + (a[5] >> 2));
        b[4] = (a[2] - a[4]);
        b[5] = ((a[3] >> 2) - a[5]);
        b[6] = (a[0] - a[6]);
        b[7] = (a[7] - (a[1] >> 2));

        tmpBuf[0 * 8 + i] = (Ipp16s) ((b[0] + b[7] + 32) >> 6);
        tmpBuf[1 * 8 + i] = (Ipp16s) ((b[2] + b[5] + 32) >> 6);
        tmpBuf[2 * 8 + i] = (Ipp16s) ((b[4] + b[3] + 32) >> 6);
        tmpBuf[3 * 8 + i] = (Ipp16s) ((b[6] + b[1] + 32) >> 6);
        tmpBuf[4 * 8 + i] = (Ipp16s) ((b[6] - b[1] + 32) >> 6);
        tmpBuf[5 * 8 + i] = (Ipp16s) ((b[4] - b[3] + 32) >> 6);
        tmpBuf[6 * 8 + i] = (Ipp16s) ((b[2] - b[5] + 32) >> 6);
        tmpBuf[7 * 8 + i] = (Ipp16s) ((b[0] - b[7] + 32) >> 6);
    }
    /* add to prediction */
    /* Note that JM39a scales and rounds after adding scaled */
    /* prediction, producing the same results. */

    for (i = 0; i < 8; i++)
    {
        pRec[0] = (pPred[0] + tmpBuf[i * 8 + 0]);
        pRec[1] = (pPred[1] + tmpBuf[i * 8 + 1]);
        pRec[2] = (pPred[2] + tmpBuf[i * 8 + 2]);
        pRec[3] = (pPred[3] + tmpBuf[i * 8 + 3]);
        pRec[4] = (pPred[4] + tmpBuf[i * 8 + 4]);
        pRec[5] = (pPred[5] + tmpBuf[i * 8 + 5]);
        pRec[6] = (pPred[6] + tmpBuf[i * 8 + 6]);
        pRec[7] = (pPred[7] + tmpBuf[i * 8 + 7]);

        pRec = (Ipp16s*)((Ipp8u*)pRec + recPitch);
        pPred = (Ipp16s*)((Ipp8u*)pPred + predPitch);
    }

    return ippStsNoErr;
}

#endif /* imp_ippiTransformResidualAndAdd8x8_H264_16s16s_C1I */

}; // UMC_H264_DECODER

///////////////////////////////////////////////////////////////

#endif // UMC_ENABLE_H264_VIDEO_DECODER
