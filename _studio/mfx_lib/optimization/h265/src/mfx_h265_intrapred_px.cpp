//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "assert.h"
#include "mfx_h265_optimization.h"
#include "ipps.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 


#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

typedef Ipp8u PixType;

namespace MFX_HEVC_PP
{

    static const Ipp8u h265_log2table[] =
    {
        2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
    };

    static const Ipp32s intraPredAngleTable[] = {
        0,   0,  32,  26,  21,  17, 13,  9,  5,  2,  0, -2, -5, -9, -13, -17, -21,
        -26, -32, -26, -21, -17, -13, -9, -5, -2,  0,  2,  5,  9, 13,  17,  21,  26, 32
    };

    static const Ipp32s invAngleTable[] = {
        0,    0,  256,  315,  390,  482,  630,  910,
        1638, 4096,    0, 4096, 1638,  910,  630,  482,
        390,  315,  256,  315,  390,  482,  630,  910,
        1638, 4096,    0, 4096, 1638,  910,  630,  482,
        390,  315,  256
    };

    /* ((1 << 15) + i/2) / i) */
    static const Ipp32s ImDiv[] = {
        32768, 16384, 10923,  8192,  6554,  5461,  4681,  4096,
        3641,  3277,  2979,  2731,  2521,  2341,  2185,  2048,
        1928,  1820,  1725,  1638,  1560,  1489,  1425,  1365,
        1311,  1260,  1214,  1170,  1130,  1092,  1057,  1024,
        993,   964,   936,   910,   886,   862,   840,   819,
        799,   780,   762,   745,   728,   712,   697,   683,
        669,   655,   643,   630,   618,   607,   596,   585,
        575,   565,   555,   546,   537,   529,   520,   512
    };

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX)
    void MAKE_NAME(h265_FilterPredictPels_8u)(
        Ipp8u* PredPel,
        Ipp32s width)
    {
        Ipp8u *tmpPtr;
        Ipp8u x0, x1, x2, xSaved;
        Ipp32s i, j;

        xSaved = (PredPel[1] + 2 * PredPel[0] + PredPel[2*width+1] + 2) >> 2;

        tmpPtr = PredPel+1;

        for (j = 0; j < 2; j++)
        {
            x0 = PredPel[0];
            x1 = tmpPtr[0];

            for (i = 0; i < 2*width-1; i++)
            {
                x2 = tmpPtr[1];
                tmpPtr[0] = (x0 + 2*x1 + x2 + 2) >> 2;
                x0 = x1; x1 = x2;
                tmpPtr++;
            }

            tmpPtr = PredPel+2*width+1;
        }

        PredPel[0] = xSaved;

    } // void h265_FilterPredictPels_8u(...)


    void MAKE_NAME(h265_FilterPredictPels_Bilinear_8u)(
        Ipp8u* pSrcDst,
        int width,
        int topLeft,
        int bottomLeft,
        int topRight)
    {
        for(int y = 0; y <= 62; y++)
        {
            pSrcDst[2*width + 1 + y] = (Ipp8u)(((63-y)*topLeft + (y+1)*bottomLeft + 32) >> 6);
        }
        for(int x = 0; x <=62; x++)
        {
            pSrcDst[1+x] = (Ipp8u)(((63-x)*topLeft + (x+1)*topRight + 32) >> 6);
        }

    } // void h265_FilterPredictPels_Bilinear_8u(...)

    void MAKE_NAME(h265_FilterPredictPels_16s)(
        Ipp16s* PredPel,
        Ipp32s width)
    {
        Ipp16s *tmpPtr;
        Ipp16s x0, x1, x2, xSaved;
        Ipp32s i, j;

        xSaved = (PredPel[1] + 2 * PredPel[0] + PredPel[2*width+1] + 2) >> 2;

        tmpPtr = PredPel+1;

        for (j = 0; j < 2; j++)
        {
            x0 = PredPel[0];
            x1 = tmpPtr[0];

            for (i = 0; i < 2*width-1; i++)
            {
                x2 = tmpPtr[1];
                tmpPtr[0] = (x0 + 2*x1 + x2 + 2) >> 2;
                x0 = x1; x1 = x2;
                tmpPtr++;
            }

            tmpPtr = PredPel+2*width+1;
        }

        PredPel[0] = xSaved;

    }

    void MAKE_NAME(h265_FilterPredictPels_Bilinear_16s)(
        Ipp16s* pSrcDst,
        int width,
        int topLeft,
        int bottomLeft,
        int topRight)
    {
        for(int y = 0; y <= 62; y++)
        {
            pSrcDst[2*width + 1 + y] = (Ipp16s)(((63-y)*topLeft + (y+1)*bottomLeft + 32) >> 6);
        }
        for(int x = 0; x <=62; x++)
        {
            pSrcDst[1+x] = (Ipp16s)(((63-x)*topLeft + (x+1)*topRight + 32) >> 6);
        }

    }

#endif

    void h265_GetPredPelsLuma_8u(Ipp8u* pSrc, Ipp8u* pPredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        Ipp8u* tmpSrcPtr;
        Ipp32s num4x4InCU   = blkSize >> 2;
        Ipp32u blkSize2     = blkSize << 1;
        Ipp32s TotalPixels  = (blkSize << 2) + 1;
        Ipp32s i, j;

        Ipp32u avlMask  = (((Ipp32u)(1<<((num4x4InCU)<<1)))-1);

        tpIf &= avlMask; lfIf &= avlMask;

        Ipp32u isAnyNBR = (tpIf | lfIf | tlIf);
        Ipp32u isAllNBR = (tpIf & lfIf & (tlIf | 0xFFFFFFFE));

        if (!(isAnyNBR)) {
            // No neighbors available
            memset(pPredPel, 128, TotalPixels);
        } else if (isAllNBR == avlMask) {
            // All neighbors available
            ippsCopy_8u(pSrc - srcPitch - 1, pPredPel, blkSize2 + 1);

            tmpSrcPtr = pSrc - 1;
            for (i = blkSize2 + 1; i < TotalPixels; i++) {
                pPredPel[i] = tmpSrcPtr[0];
                tmpSrcPtr += srcPitch;
            }
        } else {
            // Some of the neighbors available, padding required
            Ipp32u  tIF, itIF;
            PixType availableRef = 0;
            Ipp8u*  tPredPel;

            if (tlIf & 0x1) {
                // top-left value is available
                availableRef = *(pSrc - srcPitch - 1);
            } else {
                // search for top-left padded value
                tIF = lfIf;
                if(tIF) {
                    // padding from the last of left values
                    tmpSrcPtr = pSrc - 1;
                    while((tIF & 0x1)==0) { tIF >>= 1; tmpSrcPtr += (srcPitch << 2); }
                    availableRef = *tmpSrcPtr;
                } else {
                    // padding from the first of top values
                    VM_ASSERT(tpIf != 0);
                    tmpSrcPtr = pSrc - srcPitch; tIF = tpIf;
                    while((tIF & 0x1)==0) { tIF >>= 1; tmpSrcPtr += 4; }
                    availableRef = *tmpSrcPtr;
                }
            }

            *pPredPel = availableRef; tIF = tpIf;
            // Fill top line with correct padding
            if(tIF) {
                tPredPel = pPredPel + 1; tmpSrcPtr = pSrc - srcPitch;
                for (j = 0; j < (num4x4InCU<<1); j++)
                {
                    if (tIF & 0x1) {
                        *tPredPel++ = *tmpSrcPtr++;
                        *tPredPel++ = *tmpSrcPtr++;
                        *tPredPel++ = *tmpSrcPtr++;
                        availableRef = *tmpSrcPtr++;
                        *tPredPel++ = availableRef;
                    } else {
                        *tPredPel++ = availableRef;
                        *tPredPel++ = availableRef;
                        *tPredPel++ = availableRef;
                        *tPredPel++ = availableRef;
                        tmpSrcPtr += 4;
                    }
                    tIF >>= 1;
                }
                availableRef = *pPredPel;
            } else {
                memset(pPredPel + 1, availableRef, blkSize2);
            }
            tIF = lfIf;
            // Fill left line with correct padding
            if(tIF) {
                tPredPel  = pPredPel + blkSize2 + 1;
                tmpSrcPtr = pSrc - 1;
                itIF = 0;
                // Copy pixel values with holes, find last valid value, invert avialability mask
                for (j = 0; j < (num4x4InCU<<1); j++)
                {
                    itIF <<= 1;
                    if (tIF & 0x1) {
                        *tPredPel++ = *tmpSrcPtr; tmpSrcPtr += srcPitch;
                        *tPredPel++ = *tmpSrcPtr; tmpSrcPtr += srcPitch;
                        *tPredPel++ = *tmpSrcPtr; tmpSrcPtr += srcPitch;
                        availableRef = *tmpSrcPtr; tmpSrcPtr += srcPitch;
                        *tPredPel++ = availableRef;
                        itIF |= 0x1;
                    } else {
                        tmpSrcPtr += (srcPitch << 2);
                        tPredPel += 4;
                    }
                    tIF >>= 1;
                }
                // Back-sweep filling holes
                tPredPel  = pPredPel + (blkSize2 << 1);
                for (j = 0; j < (num4x4InCU<<1); j++)
                {
                    if (itIF & 0x1) {
                        tPredPel -= 4; availableRef = tPredPel[1];
                    } else {
                        *tPredPel-- = availableRef;
                        *tPredPel-- = availableRef;
                        *tPredPel-- = availableRef;
                        *tPredPel-- = availableRef;
                    }
                    itIF >>= 1;
                }
            } else {
                memset(pPredPel + blkSize2 + 1, availableRef, blkSize2);
            }
        }
    }

    void h265_GetPredPelsChromaNV12_8u(Ipp8u* pSrc, Ipp8u* pPredPel, Ipp8u isChroma422, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        Ipp8u* tmpSrcPtr;
        Ipp32s num4x4InCU   = blkSize >> 2;
        Ipp32u blkSize2     = blkSize << 1;
        Ipp32s TotalPixels  = (blkSize << 2) + 2;
        Ipp32s i, j;

        Ipp32u avlMask  = (((Ipp32u)(1<<((num4x4InCU)<<1)))-1);
        Ipp8u  leftFlagShift = isChroma422 ? 1 : 2;

        tpIf &= avlMask; lfIf &= avlMask;

        Ipp32u isAnyNBR = (tpIf | lfIf | tlIf);
        Ipp32u isAllNBR = (tpIf & lfIf & (tlIf | 0xFFFFFFFE));

        if (!(isAnyNBR)) {
            // No neighbors available
            memset(pPredPel, 128, TotalPixels);
        } else if (isAllNBR == avlMask) {
            // All neighbors available
            ippsCopy_8u(pSrc - srcPitch - 2, pPredPel, blkSize2 + 2);

            tmpSrcPtr = pSrc - 2;
            for (i = blkSize2 + 2; i < TotalPixels; i+=2) {
                pPredPel[i]   = tmpSrcPtr[0];
                pPredPel[i+1] = tmpSrcPtr[1];
                tmpSrcPtr += srcPitch;
            }
        } else {
            // Some of the neighbors available, padding required
            Ipp32u  tIF, itIF;
            PixType RefU = 0, RefV = 0;
            Ipp8u*  tPredPel;

            if (tlIf & 0x1) {
                // top-left value is available
                RefU = *(pSrc - srcPitch - 1);
                RefV = *(pSrc - srcPitch - 2);
            } else {
                // search for top-left padded value
                tIF = lfIf;
                if(tIF) {
                    // padding from the last of left values
                    tmpSrcPtr = pSrc - 2;
                    while((tIF & 0x1)==0) { tIF >>= leftFlagShift; tmpSrcPtr += (srcPitch << 2); }
                    RefV = *(tmpSrcPtr);
                    RefU = *(tmpSrcPtr+1);
                } else {
                    // padding from the first of top values
                    VM_ASSERT(tpIf != 0);
                    tmpSrcPtr = pSrc - srcPitch; tIF = tpIf;
                    while((tIF & 0x1)==0) { tIF >>= 2; tmpSrcPtr += 8; }
                    RefV = *(tmpSrcPtr);
                    RefU = *(tmpSrcPtr+1);
                }
            }

            *pPredPel = RefV; *(pPredPel+1) = RefU; tIF = tpIf;
            
            // Fill top line with correct padding
            if(tIF) {
                tPredPel = pPredPel + 2; tmpSrcPtr = pSrc - srcPitch;
                for (j = 0; j < num4x4InCU; j++)
                {
                    if (tIF & 0x1) {
                        *tPredPel++ = *tmpSrcPtr++; *tPredPel++ = *tmpSrcPtr++;
                        *tPredPel++ = *tmpSrcPtr++; *tPredPel++ = *tmpSrcPtr++;
                        *tPredPel++ = *tmpSrcPtr++; *tPredPel++ = *tmpSrcPtr++;
                        RefV = *tmpSrcPtr++; *tPredPel++ = RefV;
                        RefU = *tmpSrcPtr++; *tPredPel++ = RefU;
                    } else {
                        *tPredPel++ = RefV; *tPredPel++ = RefU;
                        *tPredPel++ = RefV; *tPredPel++ = RefU;
                        *tPredPel++ = RefV; *tPredPel++ = RefU;
                        *tPredPel++ = RefV; *tPredPel++ = RefU;
                        tmpSrcPtr += 8;
                    }
                    tIF >>= 2;
                }
                RefV = *pPredPel; RefU = *(pPredPel+1);
            } else {
                Ipp16u *pDst16 = (Ipp16u*)pPredPel + 1;
                Ipp16u  refVal = (RefU<<8)+RefV;
                for(j = 0; j < (Ipp32s)(blkSize2>>1); j++)
                    *pDst16++ = refVal;
            }
            
            tIF = lfIf;
            // Fill left line with correct padding
            if(tIF) {
                tPredPel  = pPredPel + blkSize2 + 2;
                tmpSrcPtr = pSrc - 2;
                itIF = 0;
                // Copy pixel values with holes, find last valid value, invert avialability mask
                for (j = 0; j < num4x4InCU; j++)
                {
                    itIF <<= 1;
                    if (tIF & 0x1) {
                        *tPredPel++ = *tmpSrcPtr; *tPredPel++ = *(tmpSrcPtr+1); tmpSrcPtr += srcPitch;
                        *tPredPel++ = *tmpSrcPtr; *tPredPel++ = *(tmpSrcPtr+1); tmpSrcPtr += srcPitch;
                        *tPredPel++ = *tmpSrcPtr; *tPredPel++ = *(tmpSrcPtr+1); tmpSrcPtr += srcPitch;
                        RefV = *tmpSrcPtr; *tPredPel++ = RefV;
                        RefU = *(tmpSrcPtr+1); *tPredPel++ = RefU;
                        tmpSrcPtr += srcPitch;
                        itIF |= 0x1;
                    } else {
                        tmpSrcPtr += (srcPitch << 2);
                        tPredPel += 8;
                    }
                    tIF >>= leftFlagShift;
                }
                // Back-sweep filling holes
                tPredPel--;
                for (j = 0; j < num4x4InCU; j++)
                {
                    if (itIF & 0x1) {
                        tPredPel -= 8; 
                        RefV = tPredPel[1];
                        RefU = tPredPel[2];
                    } else {
                        *tPredPel-- = RefU; *tPredPel-- = RefV;
                        *tPredPel-- = RefU; *tPredPel-- = RefV;
                        *tPredPel-- = RefU; *tPredPel-- = RefV;
                        *tPredPel-- = RefU; *tPredPel-- = RefV;
                    }
                    itIF >>= 1;
                }
            } else {
                Ipp16u *pDst16 = (Ipp16u*)(pPredPel + blkSize2 + 2);
                Ipp16u  refVal = (RefU<<8)+RefV;
                for(j = 0; j < (Ipp32s)(blkSize2>>1); j++)
                    *pDst16++ = refVal;
            }
        }
    }

    void h265_PredictIntra_Ver_8u(
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s isLuma)
    {
        Ipp32s i, j;

        for (j =0; j < width; j++)
        {
            for (i = 0; i < width; i++)
            {
                pels[j*pitch+i] = PredPel[1+i];
            }
        }

        if (isLuma && width <= 16)
        {
            for (j = 0; j < width; j++)
            {
                pels[j*pitch] = (PixType)Saturate(0, (1 << bit_depth) - 1,
                    pels[j*pitch] + ((PredPel[2*width+1+j] - PredPel[0]) >> 1));
            }
        }

    } // void h265_PredictIntra_Ver_8u(...)


    void h265_PredictIntra_Hor_8u(
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s /*isLuma*/)
    {
        assert(width == 4 || width == 8 || width == 16 || width == 32);
        Ipp8u *predPel = PredPel+2*width+1;
        switch (width) {
        case 4:
            for (Ipp32s i = 0; i < 4; i++)
                *((Ipp32u *)(pels+i*pitch)) = predPel[i] * 0x01010101;
            break;
        case 8:
            for (Ipp32s i = 0; i < 8; i++)
                *((Ipp64u *)(pels+i*pitch)) = predPel[i] * 0x0101010101010101;
            break;
        case 16:
            for (Ipp32s y = 0; y < 16; y++)
                _mm_store_si128((__m128i *)(pels+y*pitch), _mm_set1_epi8(predPel[y]));
            break;
        case 32:
            for (Ipp32s y = 0; y < 32; y++) {
                __m128i val = _mm_set1_epi8(predPel[y]);
                _mm_store_si128((__m128i *)(pels+y*pitch+0),  val);
                _mm_store_si128((__m128i *)(pels+y*pitch+16), val);
            }
            break;
        }

        //for (Ipp32s j =0; j < width; j++)
        //    for (Ipp32s i = 0; i < width; i++)
        //        pels[i*pitch+j] = PredPel[2*width+1+i];

        if (width <= 16)
            for (Ipp32s i = 0; i < width; i++)
                pels[i] = (PixType)Saturate(0, (1 << bit_depth) - 1,
                    pels[i] + ((PredPel[1+i] - PredPel[0]) >> 1));

    } // void h265_PredictIntra_Hor_8u(


    void h265_PredictIntra_DC_8u(
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s /*isLuma*/)
    {
        Ipp32s dcval = 0;
        for (Ipp32s i = 0; i < width; i++) {
            dcval += PredPel[1+i];
            dcval += PredPel[2*width+1+i];
        }

        Ipp64u dcval64;
        switch (width) {
        case 4:
            dcval = (dcval + 4) >> 3;
            dcval64 = dcval * 0x0101010101010101;
            *(Ipp32u *)(pels+1*pitch) = (Ipp32u)dcval64;
            *(Ipp32u *)(pels+2*pitch) = (Ipp32u)dcval64;
            *(Ipp32u *)(pels+3*pitch) = (Ipp32u)dcval64;
            break;
        case 8:
            dcval = (dcval + 8) >> 4;
            dcval64 = dcval * 0x0101010101010101;
            *(Ipp64u *)(pels+1*pitch) = dcval64;
            *(Ipp64u *)(pels+2*pitch) = dcval64;
            *(Ipp64u *)(pels+3*pitch) = dcval64;
            *(Ipp64u *)(pels+4*pitch) = dcval64;
            *(Ipp64u *)(pels+5*pitch) = dcval64;
            *(Ipp64u *)(pels+6*pitch) = dcval64;
            *(Ipp64u *)(pels+7*pitch) = dcval64;
            break;
        case 16:
            dcval = (dcval + 16) >> 5;
            dcval64 = dcval * 0x0101010101010101;
            for (Ipp32s j = 1; j < 16; j++) {
                *(Ipp64u *)(pels+j*pitch+0) = dcval64;
                *(Ipp64u *)(pels+j*pitch+8) = dcval64;
            }
            break;
        case 32:
            dcval = (dcval + 32) >> 6;
            dcval64 = dcval * 0x0101010101010101;
            for (Ipp32s j = 0; j < 32; j++) {
                *(Ipp64u *)(pels+j*pitch+0) = dcval64;
                *(Ipp64u *)(pels+j*pitch+8) = dcval64;
                *(Ipp64u *)(pels+j*pitch+16) = dcval64;
                *(Ipp64u *)(pels+j*pitch+24) = dcval64;
            }
            break;
        }

        if (/*isLuma &&*/ width <= 16)
        {
            pels[0] = (PixType)((PredPel[2*width+1] + 2 * dcval + PredPel[1] + 2) >> 2);

            for (Ipp32s i = 1; i < width; i++)
            {
                pels[i] = (PixType)((PredPel[1+i] + 3 * dcval + 2) >> 2);
            }

            for (Ipp32s j = 1; j < width; j++)
            {
                pels[j*pitch] = (PixType)((PredPel[2*width+1+j] + 3 * dcval + 2) >> 2);
            }
        }
    } // void h265_PredictIntra_DC_8u(

    static void h265_PredictIntra_Ang_8u_px_no_transp(
        Ipp32s mode,
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width)
    {
        Ipp32s intraPredAngle = intraPredAngleTable[mode];
        PixType refMainBuf[4*64+1];
        PixType *refMain = refMainBuf + 128;
        PixType *PredPel1, *PredPel2;
        Ipp32s invAngle = invAngleTable[mode];
        Ipp32s invAngleSum;
        Ipp32s pos;
        Ipp32s i, j;

        if (mode >= 18)
        {
            PredPel1 = PredPel;
            PredPel2 = PredPel + 2 * width + 1;
        }
        else
        {
            PredPel1 = PredPel + 2 * width;
            PredPel2 = PredPel + 1;
        }

        refMain[0] = PredPel[0];

        if (intraPredAngle < 0)
        {
            for (i = 1; i <= width; i++)
            {
                refMain[i] = PredPel1[i];
            }

            invAngleSum = 128;
            for (i = -1; i > ((width * intraPredAngle) >> 5); i--)
            {
                invAngleSum += invAngle;
                refMain[i] = PredPel2[(invAngleSum >> 8) - 1];
            }
        }
        else
        {
            for (i = 1; i <= 2*width; i++)
            {
                refMain[i] = PredPel1[i];
            }
        }

        pos = 0;

        for (j = 0; j < width; j++)
        {
            Ipp32s iIdx;
            Ipp32s iFact;

            pos += intraPredAngle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            if (iFact)
            {
                for (i = 0; i < width; i++)
                {
                    pels[j*pitch+i] = (PixType)(((32 - iFact) * refMain[i+iIdx+1] +
                        iFact * refMain[i+iIdx+2] + 16) >> 5);
                }
            }
            else
            {
                for (i = 0; i < width; i++)
                {
                    pels[j*pitch+i] = refMain[i+iIdx+1];
                }
            }
        }
    } // void h265_PredictIntra_Ang_8u_px_no_transp(...)

#if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#if defined(MFX_TARGET_OPTIMIZATION_PX)
    void h265_PredictIntra_Ang_8u(
#elif defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void h265_PredictIntra_Ang_8u_px(
#endif
        Ipp32s mode,
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width)
    {
        Ipp32s i, j;

        h265_PredictIntra_Ang_8u_px_no_transp(mode, PredPel, pels, pitch, width);

        if (mode < 18)
        {
            for (j = 0; j < width - 1; j++)
            {
                for (i = j + 1; i < width; i++)
                {
                    PixType tmp = pels[j*pitch+i];
                    pels[j*pitch+i]= pels[i*pitch+j];
                    pels[i*pitch+j] = tmp;
                }
            }
        }

    } // void h265_PredictIntra_Ang_8u(...)

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)


#if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#if defined(MFX_TARGET_OPTIMIZATION_PX)
    void h265_PredictIntra_Ang_NoTranspose_8u(
#elif defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void h265_PredictIntra_Ang_NoTranspose_8u_px(
#endif
        Ipp32s mode,
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width)
    {
        h265_PredictIntra_Ang_8u_px_no_transp(mode, PredPel, pels, pitch, width);
    } // void h265_PredictIntra_Ang_8u(...)

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX)
    void MAKE_NAME(h265_PredictIntra_Planar_8u)(
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width)
    {
        Ipp32s bottomLeft, topRight;
        Ipp32s horPred;
        Ipp32s leftColumn[64], topRow[64], bottomRow[64], rightColumn[64];
        Ipp32s shift = h265_log2table[width - 4];
        Ipp32s i, j;

        // Get left and above reference column and row
        for (i = 0; i < width; i++)
        {
            topRow[i] = PredPel[1+i];
            leftColumn[i] = PredPel[2*width+1+i];
        }

        // Prepare intermediate variables used in interpolation
        bottomLeft = PredPel[3*width+1];
        topRight   = PredPel[1+width];

        for (i = 0; i < width; i++)
        {
            bottomRow[i]   = bottomLeft - topRow[i];
            rightColumn[i] = topRight   - leftColumn[i];
            topRow[i]      <<= shift;
            leftColumn[i]  <<= shift;
        }

        // Generate prediction signal
        for (j = 0; j < width; j++)
        {
            horPred = leftColumn[j] + width;
            for (i = 0; i < width; i++)
            {
                horPred += rightColumn[j];
                topRow[i] += bottomRow[i];
                pels[j*pitch+i] = (PixType)((horPred + topRow[i]) >> (shift+1));
            }
        }
    } // void h265_PredictIntra_Planar_8u(...)

    void MAKE_NAME(h265_PredictIntra_Planar_16s)(
        Ipp16s* PredPel,
        Ipp16s* pels,
        Ipp32s pitch,
        Ipp32s width)
    {
        Ipp32s bottomLeft, topRight;
        Ipp32s horPred;
        Ipp32s leftColumn[64], topRow[64], bottomRow[64], rightColumn[64];
        Ipp32s shift = h265_log2table[width - 4];
        Ipp32s i, j;

        // Get left and above reference column and row
        for (i = 0; i < width; i++)
        {
            topRow[i] = PredPel[1+i];
            leftColumn[i] = PredPel[2*width+1+i];
        }

        // Prepare intermediate variables used in interpolation
        bottomLeft = PredPel[3*width+1];
        topRight   = PredPel[1+width];

        for (i = 0; i < width; i++)
        {
            bottomRow[i]   = bottomLeft - topRow[i];
            rightColumn[i] = topRight   - leftColumn[i];
            topRow[i]      <<= shift;
            leftColumn[i]  <<= shift;
        }

        // Generate prediction signal
        for (j = 0; j < width; j++)
        {
            horPred = leftColumn[j] + width;
            for (i = 0; i < width; i++)
            {
                horPred += rightColumn[j];
                topRow[i] += bottomRow[i];
                pels[j*pitch+i] = (Ipp16s)((horPred + topRow[i]) >> (shift+1));
            }
        }
    }
#endif

    void h265_PredictIntra_Planar_ChromaNV12_8u_px(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        Ipp32s bottomLeft, topRight;
        Ipp32s bottomLeft1, topRight1;
        Ipp32s horPred;
        Ipp32s horPred1;
        Ipp32s leftColumn[64], topRow[64], bottomRow[64], rightColumn[64];
        Ipp32s shift = h265_log2table[blkSize - 4];
        Ipp32s i, j;

        // Get left and above reference column and row
        for (i = 0; i < 2*blkSize; i++)
        {
            topRow[i] = PredPel[2+i];
            leftColumn[i] = PredPel[4*blkSize+2+i];
        }

        // Prepare intermediate variables used in interpolation
        bottomLeft  = PredPel[2+6*blkSize];
        bottomLeft1 = PredPel[2+6*blkSize+1];
        topRight  = PredPel[2+2*blkSize];
        topRight1 = PredPel[2+2*blkSize+1];

        for (i = 0; i < 2*blkSize; i+=2)
        {
            bottomRow[i]   = bottomLeft - topRow[i];
            rightColumn[i] = topRight   - leftColumn[i];
            topRow[i]      <<= shift;
            leftColumn[i]  <<= shift;

            bottomRow[i+1]   = bottomLeft1 - topRow[i+1];
            rightColumn[i+1] = topRight1   - leftColumn[i+1];
            topRow[i+1]      <<= shift;
            leftColumn[i+1]  <<= shift;
        }

        // Generate blkSize signal
        for (j = 0; j < blkSize; j++)
        {
            horPred  = leftColumn[2*j] + blkSize;
            horPred1 = leftColumn[2*j+1] + blkSize;
            for (i = 0; i < 2*blkSize; i+=2)
            {
                horPred += rightColumn[2*j];
                topRow[i] += bottomRow[i];
                pDst[j*dstStride+i] = (PixType)((horPred + topRow[i]) >> (shift+1));

                horPred1 += rightColumn[2*j+1];
                topRow[i+1] += bottomRow[i+1];
                pDst[j*dstStride+i+1] = (PixType)((horPred1 + topRow[i+1]) >> (shift+1));
            }
        }
    }

    namespace PredictIntra_DC_ChromaNV12_8u_Details {
        inline void Sum128(__m128i sum128, Ipp32s &sumU, Ipp32s &sumV) {
            Ipp64u sum64 = _mm_extract_epi64(sum128, 0) + _mm_extract_epi64(sum128, 1);
            Ipp32u sum32 = (sum64 & 0xffffffff) + (sum64 >> 32);
            sumU = sum32 & 0xffff;
            sumV = sum32 >> 16;
        }
    };

    void h265_PredictIntra_DC_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        Ipp32s k;
        Ipp32s l;

        Ipp32s dc1, dc2;
        Ipp32s Sum1 = 0, Sum2 = 0;

        Ipp8u *predTop  = PredPel+2;
        Ipp8u *predLeft = PredPel+2+4*blkSize;
        __m128i acc = _mm_cvtepu8_epi16(_mm_cvtsi64_si128(*(Ipp64u*)(predTop)));
        __m128i add = _mm_cvtepu8_epi16(_mm_cvtsi64_si128(*(Ipp64u*)(predLeft)));
        acc = _mm_add_epi16(acc, add);
        for (Ipp32s i = 8; i < 2 * blkSize; i += 8) {
            add = _mm_cvtepu8_epi16(_mm_cvtsi64_si128(*(Ipp64u*)(predTop+i)));
            acc = _mm_add_epi16(acc, add);
            add = _mm_cvtepu8_epi16(_mm_cvtsi64_si128(*(Ipp64u*)(predLeft+i)));
            acc = _mm_add_epi16(acc, add);
        }
        PredictIntra_DC_ChromaNV12_8u_Details::Sum128(acc, Sum1, Sum2);

        dc1 = (Ipp32s)((Sum1 + blkSize) / (blkSize << 1));
        dc2 = (Ipp32s)((Sum2 + blkSize) / (blkSize << 1));

        Ipp64u dcval64 = (dc1 + dc2 * 0x100) * 0x0001000100010001;

        switch (blkSize) {
        case 4:
            *(Ipp64u *)(pDst+0*dstStride) = dcval64;
            *(Ipp64u *)(pDst+1*dstStride) = dcval64;
            *(Ipp64u *)(pDst+2*dstStride) = dcval64;
            *(Ipp64u *)(pDst+3*dstStride) = dcval64;
            break;
        case 8:
            for (Ipp32s j = 0; j < 8; j++) {
                *(Ipp64u *)(pDst+j*dstStride+0) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+8) = dcval64;
            }
            break;
        case 16:
            for (Ipp32s j = 0; j < 16; j++) {
                *(Ipp64u *)(pDst+j*dstStride+0)  = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+8)  = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+16) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+24) = dcval64;
            }
            break;
        case 32:
            for (Ipp32s j = 0; j < 32; j++) {
                *(Ipp64u *)(pDst+j*dstStride+0)  = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+8)  = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+16) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+24) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+32) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+40) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+48) = dcval64;
                *(Ipp64u *)(pDst+j*dstStride+56) = dcval64;
            }
            break;
        }
    }

    void h265_PredictIntra_Hor_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        assert(blkSize == 4 || blkSize == 8 || blkSize == 16 || blkSize == 32);
        Ipp16u *predPel16 = (Ipp16u *)PredPel + 2 * blkSize + 1;
        if (blkSize == 4) {
            *((Ipp64u *)(pDst+0*dstStride)) = predPel16[0] * 0x0001000100010001;
            *((Ipp64u *)(pDst+1*dstStride)) = predPel16[1] * 0x0001000100010001;
            *((Ipp64u *)(pDst+2*dstStride)) = predPel16[2] * 0x0001000100010001;
            *((Ipp64u *)(pDst+3*dstStride)) = predPel16[3] * 0x0001000100010001;
        } else {
            for (Ipp32s y = 0; y < blkSize; y++) {
                __m128i val = _mm_set1_epi16(predPel16[y]);
                for (Ipp32s x = 0; x < 2*blkSize; x += 16)
                    _mm_store_si128((__m128i *)(pDst+y*dstStride+x), val);
            }
        }
    }

    void h265_PredictIntra_Ver_ChromaNV12_8u(Ipp8u* predPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        assert(blkSize == 4 || blkSize == 8 || blkSize == 16 || blkSize == 32);
        if (blkSize == 4) {
            *((Ipp64u *)(pDst+0*dstStride)) = *((Ipp64u *)(predPel + 2));
            *((Ipp64u *)(pDst+1*dstStride)) = *((Ipp64u *)(predPel + 2));
            *((Ipp64u *)(pDst+2*dstStride)) = *((Ipp64u *)(predPel + 2));
            *((Ipp64u *)(pDst+3*dstStride)) = *((Ipp64u *)(predPel + 2));
        } else {
            for (Ipp32s x = 0; x < 2*blkSize; x += 16) {
                __m128i pred = _mm_loadu_si128((__m128i *)(predPel+2+x));
                for (Ipp32s y = 0; y < blkSize; y++)
                    _mm_store_si128((__m128i *)(pDst+y*dstStride+x), pred);
            }
        }
    }

    void h265_PredictIntra_Ang_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode)
    {
        Ipp32s intraPredAngle = intraPredAngleTable[dirMode];
        PixType refMainBuf[4*64+2];
        PixType *refMain = refMainBuf + 128;
        PixType *PredPel1, *PredPel2;
        Ipp32s invAngle = invAngleTable[dirMode];
        Ipp32s invAngleSum;
        Ipp32s pos;
        Ipp32s i, j;

        if (dirMode >= 18)
        {
            PredPel1 = PredPel;
            PredPel2 = PredPel + 4 * blkSize + 2;
        }
        else
        {
            PredPel1 = PredPel + 4 * blkSize;
            PredPel2 = PredPel + 2;
        }

        refMain[0] = PredPel[0];
        refMain[1] = PredPel[1];

        if (intraPredAngle < 0)
        {
            for (i = 1; i <= blkSize; i++)
            {
                refMain[2*i] = PredPel1[2*i];
                refMain[2*i+1] = PredPel1[2*i+1];
            }

            invAngleSum = 128;
            for (i = -1; i > ((blkSize * intraPredAngle) >> 5); i--)
            {
                invAngleSum += invAngle;
                refMain[2*i] = PredPel2[2*((invAngleSum >> 8) - 1)];
                refMain[2*i+1] = PredPel2[2*((invAngleSum >> 8) - 1)+1];
            }
        }
        else
        {
            for (i = 1; i <= 2*blkSize; i++)
            {
                refMain[2*i] = PredPel1[2*i];
                refMain[2*i+1] = PredPel1[2*i+1];
            }
        }

        pos = 0;

        for (j = 0; j < blkSize; j++)
        {
            Ipp32s iIdx;
            Ipp32s iFact;

            pos += intraPredAngle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            if (iFact)
            {
                for (i = 0; i < blkSize; i++)
                {
                    pDst[j*dstStride+2*i] = (PixType)(((32 - iFact) * refMain[2*(i+iIdx+1)] + iFact * refMain[2*(i+iIdx+2)] + 16) >> 5);
                    pDst[j*dstStride+2*i+1] = (PixType)(((32 - iFact) * refMain[2*(i+iIdx+1)+1] + iFact * refMain[2*(i+iIdx+2)+1] + 16) >> 5);
                }
            }
            else
            {
                for (i = 0; i < blkSize; i++)
                {
                    pDst[j*dstStride+2*i] = refMain[2*(i+iIdx+1)];
                    pDst[j*dstStride+2*i+1] = refMain[2*(i+iIdx+1)+1];
                }
            }
        }

        if (dirMode < 18)
        {
            PixType tmp;
            for (j = 0; j < blkSize - 1; j++)
            {
                for (i = j + 1; i < blkSize; i++)
                {
                    tmp = pDst[j*dstStride+2*i];
                    pDst[j*dstStride+2*i]= pDst[i*dstStride+2*j];
                    pDst[i*dstStride+2*j] = tmp;
                    tmp = pDst[j*dstStride+2*i+1];
                    pDst[j*dstStride+2*i+1]= pDst[i*dstStride+2*j+1];
                    pDst[i*dstStride+2*j+1] = tmp;
                }
            }
        }
    }

#define INTRA_VER                26
#define INTRA_HOR                10

    static const Ipp32s FilteredModes[] = {10, 7, 1, 0, 10};

#if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#if defined(MFX_TARGET_OPTIMIZATION_PX)
    void h265_PredictIntra_Ang_All_8u(
#elif defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void h265_PredictIntra_Ang_All_8u_px(
#endif
        PixType* PredPel,
        PixType* FiltPel,
        PixType* pels,
        Ipp32s width)
    {
        PixType *pred_ptr = pels;
        const int BIT_DEPTH_LUMA = 8;

        for (int mode = 2; mode < 35; mode++)
        {
            Ipp32s diff = MIN(abs(mode - INTRA_HOR), abs(mode - INTRA_VER));

            if (diff <= FilteredModes[h265_log2table[width - 4] - 2])
            {
                h265_PredictIntra_Ang_8u_px_no_transp(mode, PredPel, pels, width, width);
            }
            else
            {
                h265_PredictIntra_Ang_8u_px_no_transp(mode, FiltPel, pels, width, width);
            }
            pels += width * width;  // next buffer
        }

        // hor and ver modes require additional filtering
        if (width <= 16)
        {
            pels = pred_ptr + (INTRA_HOR - 2) * width * width;
            for (Ipp32s i = 0; i < width; i++)
            {
                pels[i*width] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1,
                    pels[i*width] + ((PredPel[1+i] - PredPel[0]) >> 1));
            }
            pels = pred_ptr + (INTRA_VER - 2) * width * width;
            for (Ipp32s j = 0; j < width; j++)
            {
                pels[j*width] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1,
                    pels[j*width] + ((PredPel[2*width+1+j] - PredPel[0]) >> 1));
            }
        }
    }

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#if defined(MFX_TARGET_OPTIMIZATION_PX)
    void h265_PredictIntra_Ang_All_Even_8u(
#elif defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void h265_PredictIntra_Ang_All_Even_8u_px(
#endif
        PixType* PredPel,
        PixType* FiltPel,
        PixType* pels,
        Ipp32s width)
    {
        PixType *pred_ptr = pels;
        const int BIT_DEPTH_LUMA = 8;

        for (int mode = 2; mode < 35; mode += 2)
        {
            Ipp32s diff = MIN(abs(mode - INTRA_HOR), abs(mode - INTRA_VER));

            if (diff <= FilteredModes[h265_log2table[width - 4] - 2])
            {
                h265_PredictIntra_Ang_8u_px_no_transp(mode, PredPel, pels, width, width);
            }
            else
            {
                h265_PredictIntra_Ang_8u_px_no_transp(mode, FiltPel, pels, width, width);
            }
            pels += (width * width)*2;  // next buffer
        }

        // hor and ver modes require additional filtering
        if (width <= 16)
        {
            pels = pred_ptr + (INTRA_HOR - 2) * width * width;
            for (Ipp32s i = 0; i < width; i++)
            {
                pels[i*width] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1,
                    pels[i*width] + ((PredPel[1+i] - PredPel[0]) >> 1));
            }
            pels = pred_ptr + (INTRA_VER - 2) * width * width;
            for (Ipp32s j = 0; j < width; j++)
            {
                pels[j*width] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1,
                    pels[j*width] + ((PredPel[2*width+1+j] - PredPel[0]) >> 1));
            }
        }
    }

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)  || defined(MFX_TARGET_OPTIMIZATION_AUTO)

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
