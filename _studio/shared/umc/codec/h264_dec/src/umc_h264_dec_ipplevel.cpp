//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

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
#define clipd1(x, limit) min(limit, max(x,-limit))

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

extern void ConvertNV12ToYV12(const uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint8_t *pSrcDstUPlane, uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi)
{
    int32_t i, j;

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

extern void ConvertYV12ToNV12(const uint8_t *pSrcDstUPlane, const uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi)
{
    int32_t i, j;

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

void ConvertNV12ToYV12(const uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint16_t *pSrcDstUPlane, uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi)
{
    int32_t i, j;

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

void ConvertYV12ToNV12(const uint16_t *pSrcDstUPlane, const uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi)
{
    int32_t i, j;

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
    uint32_t x; \
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
    (data) = x & ((((uint32_t) 0x01) << (nbits)) - 1); \
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
static int32_t H264TotalCoeffTrailingOnesTab0[] = {
    0,  5, 10, 15,  4,  9, 19, 14, 23,  8, 13, 18, 27, 12, 17, 22,
    31, 16, 21, 26, 35, 20, 25, 30, 39, 24, 29, 34, 43, 28, 33, 38,
    32, 36, 37, 42, 47, 40, 41, 46, 51, 44, 45, 50, 55, 48, 49, 54,
    59, 53, 52, 57, 58, 63, 56, 61, 62, 67, 60, 65, 66, 64
};

static int32_t H264TotalCoeffTrailingOnesTab1[] = {
    0,  5, 10, 15, 19,  9, 23,  4, 13, 14, 27,  8, 17, 18, 31, 12,
    21, 22, 35, 16, 25, 26, 20, 24, 29, 30, 39, 28, 33, 34, 43, 32,
    37, 38, 47, 36, 41, 42, 51, 40, 45, 46, 44, 48, 49, 50, 55, 52,
    53, 54, 59, 56, 58, 63, 57, 60, 61, 62, 67, 64, 65, 66
};

static int32_t H264TotalCoeffTrailingOnesTab2[] = {
    0,  5, 10, 15, 19, 23, 27, 31,  9, 14, 35, 13, 18, 17, 22, 21,
    4, 25, 26, 39,  8, 29, 30, 12, 16, 33, 34, 43, 20, 38, 24, 28,
    32, 37, 42, 47, 36, 41, 46, 51, 40, 45, 50, 55, 44, 49, 54, 48,
    53, 52, 57, 58, 59, 56, 61, 62, 63, 60, 65, 66, 67, 64
};

static int32_t H264CoeffTokenIdxTab0[] = {
    0,  0,  0,  0,  4,  1,  0,  0,  9,  5,  2,  0, 13, 10,  7,  3,
    17, 14, 11,  6, 21, 18, 15,  8, 25, 22, 19, 12, 29, 26, 23, 16,
    32, 30, 27, 20, 33, 34, 31, 24, 37, 38, 35, 28, 41, 42, 39, 36,
    45, 46, 43, 40, 50, 49, 47, 44, 54, 51, 52, 48, 58, 55, 56, 53,
    61, 59, 60, 57
};

static int32_t H264CoeffTokenIdxTab1[] = {
    0,  0,  0,  0,  7,  1,  0,  0, 11,  5,  2,  0, 15,  8,  9,  3,
    19, 12, 13,  4, 22, 16, 17,  6, 23, 20, 21, 10, 27, 24, 25, 14,
    31, 28, 29, 18, 35, 32, 33, 26, 39, 36, 37, 30, 42, 40, 41, 34,
    43, 44, 45, 38, 47, 48, 49, 46, 51, 54, 52, 50, 55, 56, 57, 53,
    59, 60, 61, 58
};

static int32_t H264CoeffTokenIdxTab2[] = {
    0,  0,  0,  0, 16,  1,  0,  0, 20,  8,  2,  0, 23, 11,  9,  3,
    24, 13, 12,  4, 28, 15, 14,  5, 30, 17, 18,  6, 31, 21, 22,  7,
    32, 25, 26, 10, 36, 33, 29, 19, 40, 37, 34, 27, 44, 41, 38, 35,
    47, 45, 42, 39, 49, 48, 46, 43, 53, 50, 51, 52, 57, 54, 55, 56,
    61, 58, 59, 60
};

static int32_t* H264TotalCoeffTrailingOnesTab[] = {
    H264TotalCoeffTrailingOnesTab0,
    H264TotalCoeffTrailingOnesTab0,
    H264TotalCoeffTrailingOnesTab1,
    H264TotalCoeffTrailingOnesTab1,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2,
    H264TotalCoeffTrailingOnesTab2
};

static int32_t* H264CoeffTokenIdxTab[] = {
    H264CoeffTokenIdxTab0,
    H264CoeffTokenIdxTab0,
    H264CoeffTokenIdxTab1,
    H264CoeffTokenIdxTab1,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2,
    H264CoeffTokenIdxTab2
};

static const uint8_t vlc_inc[] = {0,3,6,12,24,48};

static int32_t bitsToGetTbl16s[7][16] = /*[level][numZeros]*/
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
static int32_t addOffsetTbl16s[7][16] = /*[level][numZeros]*/
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
static int32_t sadd[7]={15,0,0,0,0,0,0};

#ifndef imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s
#define imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s

IPPFUN(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (uint32_t **ppBitStream,
                                                     int32_t *pOffset,
                                                     int16_t *pNumCoeff,
                                                     int16_t **ppPosCoefbuf,
                                                     uint32_t uVLCSelect,
                                                     int16_t uMaxNumCoeff,
                                                     const int32_t **ppTblCoeffToken,
                                                     const int32_t **ppTblTotalZeros,
                                                     const int32_t **ppTblRunBefore,
                                                     const int32_t *pScanMatrix,
                                                     int32_t scanIdxStart,
                                                     int32_t scanIdxEnd)) /* buffer to return up to 16 */
{
    int16_t        CoeffBuf[16];    /* Temp buffer to hold coeffs read from bitstream*/
    uint32_t        uVLCIndex        = 2;
    uint32_t        uCoeffIndex      = 0;
    int32_t        sTotalZeros      = 0;
    int32_t        sFirstPos        = scanIdxStart;
    int32_t        coeffLimit = scanIdxEnd - scanIdxStart + 1;
    uint32_t        TrOneSigns = 0;        /* return sign bits (1==neg) in low 3 bits*/
    uint32_t        uTR1Mask;
    int32_t        pos;
    int32_t        sRunBefore;
    int16_t        sNumTrailingOnes;
    int32_t        sNumCoeff = 0;
    uint32_t        table_bits;
    uint8_t         code_len;
    int32_t        i;
    uint32_t  table_pos;
    int32_t  val;

    /* check error(s) */
    IPP_BAD_PTR4_RET(ppBitStream,pOffset,ppPosCoefbuf,pNumCoeff)
    IPP_BAD_PTR4_RET(ppTblCoeffToken,ppTblTotalZeros,ppTblRunBefore,pScanMatrix)
    IPP_BAD_PTR2_RET(*ppBitStream, *ppPosCoefbuf)
    IPP_BADARG_RET(((int32_t)uVLCSelect < 0), ippStsOutOfRangeErr)

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        h264GetNBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (int16_t) (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;

        uVLCSelect = 7;
    }
    else
    {
        uint32_t*  tmp_pbs = *ppBitStream;
        int32_t   tmp_offs = *pOffset;
        const int32_t *pDecTable;
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
        code_len = (uint8_t) (val);

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (uint8_t) (val & 0xff);
        }

        h264UngetNBits((*ppBitStream), (*pOffset), code_len);

        if ((val>>8) == IPPVC_VLC_FORBIDDEN)
        {
             *ppBitStream = tmp_pbs;
             *pOffset = tmp_offs;

             return ippStsH263VLCCodeErr;
        }

        sNumTrailingOnes  = (int16_t) ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    if (coeffLimit < 16)
    {
        int32_t *tmpTab = H264TotalCoeffTrailingOnesTab[uVLCSelect];
        int32_t targetCoeffTokenIdx =
            H264CoeffTokenIdxTab[uVLCSelect][sNumCoeff * 4 + sNumTrailingOnes];
        int32_t minNumCoeff = coeffLimit+1;
        int32_t minNumTrailingOnes = coeffLimit+1;
        int32_t j;

        if (minNumTrailingOnes > 4)
            minNumTrailingOnes = 4;

        for (j = 0, i = 0; i <= targetCoeffTokenIdx; j++)
        {
            sNumCoeff = tmpTab[j] >> 2;
            sNumTrailingOnes = (int16_t)(tmpTab[j] & 3);

            if ((sNumCoeff < minNumCoeff) && (sNumTrailingOnes < minNumTrailingOnes))
                i++;
        }

        sNumCoeff = tmpTab[j-1] >> 2;
        sNumTrailingOnes = (int16_t)(tmpTab[j-1] & 3);
    }

    *pNumCoeff = (int16_t) sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (int16_t) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
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
            uint16_t suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
            int16_t lCoeffIndex;
            uint16_t uCoeffLevel = 0;
            int32_t NumZeros;
            uint16_t uBitsToGet;
            uint16_t uFirstAdjust;
            uint16_t uLevelOffset;
            int32_t w;
            int16_t    *lCoeffBuf = &CoeffBuf[uCoeffIndex];

            if ((sNumCoeff > 10) && (sNumTrailingOnes < 3))
                suffixLength = 1;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            uFirstAdjust = (uint16_t) ((sNumTrailingOnes < 3) ? 1 : 0);

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
                    uBitsToGet = (int16_t) (bitsToGetTbl16s[suffixLength][NumZeros]);
                    uLevelOffset = (uint16_t) (addOffsetTbl16s[suffixLength][NumZeros]);

                    if (uBitsToGet)
                    {
                        h264GetNBits((*ppBitStream), (*pOffset), uBitsToGet, NumZeros);
                    }

                    uCoeffLevel = (uint16_t) ((NumZeros>>1) + uLevelOffset + uFirstAdjust);

                    lCoeffBuf[lCoeffIndex] = (int16_t) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    uint32_t level_suffix;
                    uint32_t levelSuffixSize = NumZeros - 3;
                    int32_t levelCode;

                    h264GetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = (uint16_t) ((min(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                    levelCode = (uint16_t) (levelCode + (1 << (NumZeros - 3)) - 4096);

                    lCoeffBuf[lCoeffIndex] = (int16_t) ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = (uint16_t) ABS(lCoeffBuf[lCoeffIndex]);
                }

                uFirstAdjust = 0;

            }    /* for uCoeffIndex */

        }
        /* Get TotalZeros if any */
        if (sNumCoeff < uMaxNumCoeff)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZeros[sNumCoeff]); */
            int32_t tmpNumCoeff;
            const int32_t *pDecTable;

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
            code_len = (uint8_t) (val & 0xff);
            val = val >> 8;

            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val = pDecTable[table_pos + val + 1];
                code_len = (uint8_t) (val & 0xff);
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
                const int32_t *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros]);

                table_bits = pDecTable[0];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (uint8_t) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (uint8_t) (val & 0xff);
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


} /* IPPFUN(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (uint32_t **ppBitStream, */

#endif /* imp_own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s */

}; // UMC_H264_DECODER

///////////////////////////////////////////////////////////////

#endif // UMC_ENABLE_H264_VIDEO_DECODER
