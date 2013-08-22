/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM)


#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

typedef Ipp8u PixType;

namespace MFX_HEVC_COMMON
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
    static Ipp32s ImDiv[] = {
        32768, 16384, 10923,  8192,  6554,  5461,  4681,  4096,
        3641,  3277,  2979,  2731,  2521,  2341,  2185,  2048,
        1928,  1820,  1725,  1638,  1560,  1489,  1425,  1365,
        1311,  1260,  1214,  1170,  1130,  1092,  1057,  1024,
        993,   964,   936,   910,   886,   862,   840,   819,
        799,   780,   762,   745,   728,   712,   697,   683,
        669,   655,   643,   630,   618,   607,   596,   585,
        575,   565,   555,   546,   537,   529,   520,   512
    };
    

    void h265_FilterPredictPels_8u(
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


    void h265_FilterPredictPels_Bilinear_8u(
        Ipp8u* pSrcDst,
        int width,
        int topLeft,
        int bottomLeft,
        int topRight)
    {
        for(int y = 0; y <= 62; y++)
        {
            pSrcDst[2*width + 1 + y] = ((63-y)*topLeft + (y+1)*bottomLeft + 32) >> 6;
        }
        for(int x = 0; x <=62; x++)
        {
            pSrcDst[1+x] = ((63-x)*topLeft + (x+1)*topRight + 32) >> 6;
        }

    } // void h265_FilterPredictPels_Bilinear_8u(...)


    void h265_GetPredictPels_8u(
        Ipp32u log2_min_transform_block_size,
        Ipp8u* src,
        Ipp8u* PredPel,
        bool* neighborFlags,
        Ipp32s numIntraNeighbor,

        Ipp32s width,
        Ipp32s srcPitch,
        Ipp32s isLuma,

        Ipp32s UnitSize)
    {
        Ipp8u* tmpSrcPtr;
        Ipp8u dcval;
        Ipp32s minTUSize = 1 << log2_min_transform_block_size;
        Ipp32s numUnitsInCU = width >> log2_min_transform_block_size;
        Ipp32s TotalUnits = (numUnitsInCU << 2) + 1;    
        Ipp32s i, j;
        const int BIT_DEPTH_LUMA = 8;
        const int BIT_DEPTH_CHROMA = 8;

        if (isLuma)
        {
            dcval = (1 << (BIT_DEPTH_LUMA - 1));
        }
        else
        {
            dcval = (1 << (BIT_DEPTH_CHROMA - 1));
            minTUSize >>= 1;
            numUnitsInCU = width >> (log2_min_transform_block_size - 1);
        }

        if (numIntraNeighbor == 0)
        {
            // Fill border with DC value

            for (i = 0; i <= 4*width; i++)
            {
                PredPel[i] = dcval;
            }
        }
        else if (numIntraNeighbor == (4*numUnitsInCU + 1))
        {
            // Fill top-left border with rec. samples
            tmpSrcPtr = src - srcPitch - 1;
            PredPel[0] = tmpSrcPtr[0];

            // Fill top and top right border with rec. samples
            tmpSrcPtr = src - srcPitch;
            for (i = 0; i < 2*width; i++)
            {
                PredPel[1+i] = tmpSrcPtr[i];
            }

            // Fill left and below left border with rec. samples
            tmpSrcPtr = src - 1;

            for (i = 0; i < 2*width; i++)
            {
                PredPel[2*width+1+i] = tmpSrcPtr[0];
                tmpSrcPtr += srcPitch;
            }
        }
        else // reference samples are partially available
        {
            int Size = (width << 1) + 1;
            Ipp8u pAdiTemp[65*65];
            Ipp32u NumUnits2 = numUnitsInCU << 1;
            Ipp32u TotalSamples = TotalUnits * UnitSize;
            const int MAX_CU_SIZE = 64;
            Ipp8u pAdiLine[5 * MAX_CU_SIZE];
            Ipp8u* pAdiLineTemp;
            bool *pNeighborFlags;
            Ipp32s Prev;
            Ipp32s Next;
            Ipp32s Curr;
            Ipp16s pRef = 0;
            Ipp8u* pRoiTemp;

            // Initialize
            for (i = 0; i < TotalSamples; i++)
            {
                pAdiLine[i] = dcval;
            }

            // Fill top-left sample
            pRoiTemp = src - srcPitch - 1;
            pAdiLineTemp = pAdiLine + (NumUnits2 * UnitSize);
            pNeighborFlags = neighborFlags + NumUnits2;
            if (*pNeighborFlags)
            {
                pAdiLineTemp[0] = pRoiTemp[0];
                for (i = 1; i < UnitSize; i++)
                {
                    pAdiLineTemp[i] = pAdiLineTemp[0];
                }
            }

            // Fill left & below-left samples
            pRoiTemp += srcPitch;

            int PicStride = srcPitch;
            Ipp8u* pRoiOrigin = src;

            pAdiLineTemp--;
            pNeighborFlags--;
            for (j = 0; j < NumUnits2; j++)
            {
                if (*pNeighborFlags)
                {
                    for (i = 0; i < UnitSize; i++)
                    {
                        pAdiLineTemp[-(Ipp32s)i] = pRoiTemp[i * PicStride];
                    }
                }
                pRoiTemp += UnitSize * PicStride;
                pAdiLineTemp -= UnitSize;
                pNeighborFlags--;
            }

            // Fill above & above-right samples
            pRoiTemp = pRoiOrigin - PicStride;
            pAdiLineTemp = pAdiLine + ((NumUnits2 + 1) * UnitSize);
            pNeighborFlags = neighborFlags + NumUnits2 + 1;
            for (j = 0; j < NumUnits2; j++)
            {
                if (*pNeighborFlags)
                {
                    for (i = 0; i < UnitSize; i++)
                    {
                        pAdiLineTemp[i] = pRoiTemp[i];
                    }
                }
                pRoiTemp += UnitSize;
                pAdiLineTemp += UnitSize;
                pNeighborFlags++;
            }
            // Pad reference samples when necessary
            Prev = -1;
            Curr = 0;
            Next = 1;
            pAdiLineTemp = pAdiLine;
            while (Curr < TotalUnits)
            {
                if (!neighborFlags[Curr])
                {
                    if (Curr == 0)
                    {
                        while (Next < TotalUnits && !neighborFlags[Next])
                        {
                            Next++;
                        }
                        pRef = pAdiLine[Next * UnitSize];
                        // Pad unavailable samples with new value
                        while (Curr < Next)
                        {
                            for (i = 0; i < UnitSize; i++)
                            {
                                pAdiLineTemp[i] = pRef;
                            }
                            pAdiLineTemp += UnitSize;
                            Curr++;
                        }
                    }
                    else
                    {
                        pRef = pAdiLine[Curr * UnitSize - 1];
                        for (i = 0; i < UnitSize; i++)
                        {
                            pAdiLineTemp[i] = pRef;
                        }
                        pAdiLineTemp += UnitSize;
                        Curr++;
                    }
                }
                else
                {
                    pAdiLineTemp += UnitSize;
                    Curr++;
                }
            }

            // Copy processed samples
            pAdiLineTemp = pAdiLine + Size + UnitSize - 2;
            for (i = 0; i < Size; i++)
            {
                PredPel[i] = pAdiLineTemp[i];
            }
            pAdiLineTemp = pAdiLine + Size - 1;
            for (i = 1; i < Size; i++)
            {         
                PredPel[Size + i - 1] = pAdiLineTemp[-(Ipp32s)i];
            }
        }

    } // void h265_GetPredictPels_8u(...)


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
        Ipp32s isLuma)
    {
        Ipp32s i, j;

        for (j =0; j < width; j++)
        {
            for (i = 0; i < width; i++)
            {
                pels[i*pitch+j] = PredPel[2*width+1+i];
            }
        }

        if (isLuma && width <= 16)
        {
            for (i = 0; i < width; i++)
            {
                pels[i] = (PixType)Saturate(0, (1 << bit_depth) - 1,
                    pels[i] + ((PredPel[1+i] - PredPel[0]) >> 1));
            }
        }

    } // void h265_PredictIntra_Hor_8u(


    void h265_PredictIntra_DC_8u(
        PixType* PredPel,
        PixType* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s isLuma)
    {
        Ipp32s i, j, dcval;

        dcval = 0;

        for (i = 0; i < width; i++)
        {
            dcval += PredPel[1+i];
        }

        for (i = 0; i < width; i++)
        {
            dcval += PredPel[2*width+1+i];
        }

        dcval = (dcval + width) / (2*width);

        if (isLuma && width <= 16)
        {
            pels[0] = (PixType)((PredPel[2*width+1] + 2 * dcval + PredPel[1] + 2) >> 2);

            for (i = 1; i < width; i++)
            {
                pels[i] = (PixType)((PredPel[1+i] + 3 * dcval + 2) >> 2);
            }

            for (j = 1; j < width; j++)
            {
                pels[j*pitch] = (PixType)((PredPel[2*width+1+j] + 3 * dcval + 2) >> 2);
            }

            for (j = 1; j < width; j++)
            {
                for (i = 1; i < width; i++)
                {
                    pels[j*pitch+i] = (PixType)dcval;
                }
            }
        }
        else
        {
            for (j = 0; j < width; j++)
            {
                for (i = 0; i < width; i++)
                {
                    pels[j*pitch+i] = (PixType)dcval;
                }
            }
        }

    } // void h265_PredictIntra_DC_8u(

//#define OPTIM_PRED_ANG
#ifdef OPTIM_PRED_ANG

#include <immintrin.h>

    static inline void PredAngle_4x4_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1;

        for (int j = 0; j < 4; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            // process i = 0..3

            // r0 = (Ipp16s)pSrc[iIdx+1+i];
            // r1 = (Ipp16s)pSrc[iIdx+2+i];
            r0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+1]));
            r1 = _mm_srli_si128(r0, 2);

            // r0 += ((r1 - r0) * iFact + 16) >> 5;
            r1 = _mm_sub_epi16(r1, r0);
            r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
            r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
            r1 = _mm_srai_epi16(r1, 5);
            r0 = _mm_add_epi16(r0, r1);

            // pDst[j*pitch+i] = (Ipp8u)r0;
            r0 = _mm_packus_epi16(r0, r0);
            *(int *)&pDst[j*dstPitch] = _mm_cvtsi128_si32(r0);
        }
    }

    static inline void PredAngle_8x8_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1;

        for (int j = 0; j < 8; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            // process i = 0..7

            r0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+1]));
            r1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+2]));

            r1 = _mm_sub_epi16(r1, r0);
            r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
            r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
            r1 = _mm_srai_epi16(r1, 5);
            r0 = _mm_add_epi16(r0, r1);

            r0 = _mm_packus_epi16(r0, r0);
            _mm_storel_epi64((__m128i *)&pDst[j*dstPitch], r0);
        }
    }

    static inline void PredAngle_16x16_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1, r2, r3;

        for (int j = 0; j < 16; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            // process i = 0..15

            r0 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+1]);
            r1 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+2]);
            r2 = _mm_unpackhi_epi8(r0, _mm_setzero_si128());
            r3 = _mm_unpackhi_epi8(r1, _mm_setzero_si128());
            r0 = _mm_cvtepu8_epi16(r0);
            r1 = _mm_cvtepu8_epi16(r1);

            r1 = _mm_sub_epi16(r1, r0);
            r3 = _mm_sub_epi16(r3, r2);
            r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
            r3 = _mm_mullo_epi16(r3, _mm_set1_epi16(iFact));
            r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
            r3 = _mm_add_epi16(r3, _mm_set1_epi16(16));
            r1 = _mm_srai_epi16(r1, 5);
            r3 = _mm_srai_epi16(r3, 5);
            r0 = _mm_add_epi16(r0, r1);
            r2 = _mm_add_epi16(r2, r3);

            r0 = _mm_packus_epi16(r0, r2);
            _mm_storeu_si128((__m128i *)&pDst[j*dstPitch], r0);
        }
    }

    static inline void PredAngle_32x32_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1, r2, r3;

        for (int j = 0; j < 32; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            for (int i = 0; i < 32; i += 16)
            {
                r0 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+1+i]);
                r1 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+2+i]);
                r2 = _mm_unpackhi_epi8(r0, _mm_setzero_si128());
                r3 = _mm_unpackhi_epi8(r1, _mm_setzero_si128());
                r0 = _mm_cvtepu8_epi16(r0);
                r1 = _mm_cvtepu8_epi16(r1);

                r1 = _mm_sub_epi16(r1, r0);
                r3 = _mm_sub_epi16(r3, r2);
                r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
                r3 = _mm_mullo_epi16(r3, _mm_set1_epi16(iFact));
                r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
                r3 = _mm_add_epi16(r3, _mm_set1_epi16(16));
                r1 = _mm_srai_epi16(r1, 5);
                r3 = _mm_srai_epi16(r3, 5);
                r0 = _mm_add_epi16(r0, r1);
                r2 = _mm_add_epi16(r2, r3);

                r0 = _mm_packus_epi16(r0, r2);
                _mm_storeu_si128((__m128i *)&pDst[j*dstPitch+i], r0);
            }
        }
    }

    // Out-of-place transpose
    static inline void Transpose_4x4_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        __m128i r0, r1, r2, r3;
        r0 = _mm_cvtsi32_si128(*(int *)&pSrc[0*srcPitch]);  // 03,02,01,00
        r1 = _mm_cvtsi32_si128(*(int *)&pSrc[1*srcPitch]);  // 13,12,11,10
        r2 = _mm_cvtsi32_si128(*(int *)&pSrc[2*srcPitch]);  // 23,22,21,20
        r3 = _mm_cvtsi32_si128(*(int *)&pSrc[3*srcPitch]);  // 33,32,31,30

        r0 = _mm_unpacklo_epi8(r0, r1);
        r2 = _mm_unpacklo_epi8(r2, r3);
        r0 = _mm_unpacklo_epi16(r0, r2);    // 33,23,13,03, 32,22,12,02, 31,21,11,01, 30,20,10,00, 

        *(int *)&pDst[0*dstPitch] = _mm_cvtsi128_si32(r0);
        *(int *)&pDst[1*dstPitch] = _mm_extract_epi32(r0, 1);
        *(int *)&pDst[2*dstPitch] = _mm_extract_epi32(r0, 2);
        *(int *)&pDst[3*dstPitch] = _mm_extract_epi32(r0, 3);
    }

    static inline void Transpose_8x8_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        __m128i r0, r1, r2, r3, r4, r5, r6, r7;
        r0 = _mm_loadl_epi64((__m128i*)&pSrc[0*srcPitch]);  // 07,06,05,04,03,02,01,00
        r1 = _mm_loadl_epi64((__m128i*)&pSrc[1*srcPitch]);  // 17,16,15,14,13,12,11,10
        r2 = _mm_loadl_epi64((__m128i*)&pSrc[2*srcPitch]);  // 27,26,25,24,23,22,21,20
        r3 = _mm_loadl_epi64((__m128i*)&pSrc[3*srcPitch]);  // 37,36,35,34,33,32,31,30
        r4 = _mm_loadl_epi64((__m128i*)&pSrc[4*srcPitch]);  // 47,46,45,44,43,42,41,40
        r5 = _mm_loadl_epi64((__m128i*)&pSrc[5*srcPitch]);  // 57,56,55,54,53,52,51,50
        r6 = _mm_loadl_epi64((__m128i*)&pSrc[6*srcPitch]);  // 67,66,65,64,63,62,61,60
        r7 = _mm_loadl_epi64((__m128i*)&pSrc[7*srcPitch]);  // 77,76,75,74,73,72,71,70

        r0 = _mm_unpacklo_epi8(r0, r1);
        r2 = _mm_unpacklo_epi8(r2, r3);
        r4 = _mm_unpacklo_epi8(r4, r5);
        r6 = _mm_unpacklo_epi8(r6, r7);

        r1 = r0;
        r0 = _mm_unpacklo_epi16(r0, r2);
        r1 = _mm_unpackhi_epi16(r1, r2);
        r5 = r4;
        r4 = _mm_unpacklo_epi16(r4, r6);
        r5 = _mm_unpackhi_epi16(r5, r6);
        r2 = r0;
        r0 = _mm_unpacklo_epi32(r0, r4);
        r2 = _mm_unpackhi_epi32(r2, r4);
        r3 = r1;
        r1 = _mm_unpacklo_epi32(r1, r5);
        r3 = _mm_unpackhi_epi32(r3, r5);

        _mm_storel_pi((__m64 *)&pDst[0*dstPitch], _mm_castsi128_ps(r0));    // MOVLPS
        _mm_storeh_pi((__m64 *)&pDst[1*dstPitch], _mm_castsi128_ps(r0));    // MOVHPS
        _mm_storel_pi((__m64 *)&pDst[2*dstPitch], _mm_castsi128_ps(r2));
        _mm_storeh_pi((__m64 *)&pDst[3*dstPitch], _mm_castsi128_ps(r2));
        _mm_storel_pi((__m64 *)&pDst[4*dstPitch], _mm_castsi128_ps(r1));
        _mm_storeh_pi((__m64 *)&pDst[5*dstPitch], _mm_castsi128_ps(r1));
        _mm_storel_pi((__m64 *)&pDst[6*dstPitch], _mm_castsi128_ps(r3));
        _mm_storeh_pi((__m64 *)&pDst[7*dstPitch], _mm_castsi128_ps(r3));
    }

    static inline void Transpose_16x16_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        for (int j = 0; j < 16; j += 8)
        {
            for (int i = 0; i < 16; i += 8)
            {
                Transpose_8x8_8u_SSE4(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
            }
        }
    }

    static inline void Transpose_32x32_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        for (int j = 0; j < 32; j += 8)
        {
            for (int i = 0; i < 32; i += 8)
            {
                Transpose_8x8_8u_SSE4(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
            }
        }
    }


    void h265_PredictIntra_Ang_8u(
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
        Ipp32s i;

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

        switch (width)
        {
        case 4:
            if (mode >= 18)
                PredAngle_4x4_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[4*4];
                PredAngle_4x4_8u_SSE4(refMain, buf, 4, intraPredAngle);
                Transpose_4x4_8u_SSE4(buf, 4, pels, pitch);
            }
            break;
        case 8:
            if (mode >= 18)
                PredAngle_8x8_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[8*8];
                PredAngle_8x8_8u_SSE4(refMain, buf, 8, intraPredAngle);
                Transpose_8x8_8u_SSE4(buf, 8, pels, pitch);
            }
            break;
        case 16:
            if (mode >= 18)
                PredAngle_16x16_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[16*16];
                PredAngle_16x16_8u_SSE4(refMain, buf, 16, intraPredAngle);
                Transpose_16x16_8u_SSE4(buf, 16, pels, pitch);
            }
            break;
        case 32:
            if (mode >= 18)
                PredAngle_32x32_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[32*32];
                PredAngle_32x32_8u_SSE4(refMain, buf, 32, intraPredAngle);
                Transpose_32x32_8u_SSE4(buf, 32, pels, pitch);
            }
            break;
        }

    } // void h265_PredictIntra_Ang_8u(...)

#else

    void h265_PredictIntra_Ang_8u(
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

#endif

    void h265_PredictIntra_Planar_8u(
        PixType* PredPel,
        PixType* pels,
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

}; // namespace MFX_HEVC_COMMON

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
