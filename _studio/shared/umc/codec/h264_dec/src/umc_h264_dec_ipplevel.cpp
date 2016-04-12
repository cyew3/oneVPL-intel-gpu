/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
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

}; // UMC_H264_DECODER

///////////////////////////////////////////////////////////////

#endif // UMC_ENABLE_H264_VIDEO_DECODER
