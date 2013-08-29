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

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

//=========================================================
// AYA TMP SOLUTIONS
#include <intrin.h> // aya for WriteP.B/N only

/* workaround for compiler bug (see h265_tr_quant_opt.cpp) */
#ifdef NDEBUG 
#define MM_LOAD_EPI64(x) (*(__m128i*)(x))
#else
#define MM_LOAD_EPI64(x) _mm_loadl_epi64( (__m128i*)(x))
#endif
//=========================================================

namespace MFX_HEVC_PP
{

    enum EnumPlane
    {
        TEXT_LUMA = 0,
        TEXT_CHROMA,
        TEXT_CHROMA_U,
        TEXT_CHROMA_V,
    };

    __declspec(align(16)) static const short filtTabLumaSc[3][8] = {
        { -1, 4, -10, 58, 17,  -5, 1,  0 },
        { -1, 4, -11, 40, 40, -11, 4, -1 },
        {  0, 1,  -5, 17, 58, -10, 4, -1 },
    };

    __declspec(align(16)) static const short filtTabChromaSc[7][4] = {
        {  -2,  58,  10, -2,  },
        {  -4,  54,  16, -2,  },
        {  -6,  46,  28, -4,  },
        {  -4,  36,  36, -4,  },
        {  -4,  28,  46, -6,  },
        {  -2,  16,  54, -4,  },
        {  -2,  10,  58, -2,  },
    };

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
#define MAKE_NAME( func ) func ## _px
#else
    #define MAKE_NAME( func ) func
#endif

    void MAKE_NAME(h265_InterpLuma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        short acc;
        const short* coeffs16x = filtTabLumaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 8; k++)
                    acc += pSrc[j+k] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpChroma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST, int plane)
    {
        int i, j, k, srcOff;
        short acc;
        const short* coeffs8x = filtTabChromaSc[tab_index - 1];

        if (plane == TEXT_CHROMA)
            srcOff = 2;
        else
            srcOff = 1;

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j+k*srcOff] * coeffs8x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpLuma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        short acc;
        const short* coeffs16x = filtTabLumaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 8; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpChroma_s8_d16_V)  (INTERP_S8_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        short acc;
        const short* coeffs8x = filtTabChromaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs8x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpLuma_s16_d16_V)    (INTERP_S16_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        int acc;
        const short* coeffs16x = filtTabLumaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 8; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpChroma_s16_d16_V)  (INTERP_S16_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        int acc;
        const short* coeffs8x = filtTabChromaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs8x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)acc;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

    //======================
    // TEMP SOLUTIONS
    /* mode: AVERAGE_NO, just clip/pack 16-bit output to 8-bit 
    * NOTE: could be optimized more, but is not used very often in practice
    */
    void MAKE_NAME(h265_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST)
    {
        int col;
        short *pSrcRef = pSrc;
        unsigned char *pDstRef = pDst;
        __m128i xmm0;

        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels, clip to 8-bit */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    }

    /* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
    void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
    {
        int col;
        short *pSrcRef = pSrc;
        unsigned char *pDstRef = pDst, *pAvgRef = pAvg;
        __m128i xmm0, xmm1, xmm7;

        xmm7 = _mm_set1_epi16(1 << 6);
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            pAvg = pAvgRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels from source */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);

                /* load 8 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(pAvg));    
                xmm1 = _mm_slli_epi16(xmm1, 6);

                /* add, round, clip back to 8 bits */
                xmm1 = _mm_adds_epi16(xmm1, xmm7);
                xmm0 = _mm_adds_epi16(xmm0, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, 7);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    pAvg += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
            pAvgRef += avgPitch;
        } while (--height);
    }

    /* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
    void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
    {
        int col;
        short *pSrcRef = pSrc, *pAvgRef = pAvg;
        unsigned char *pDstRef = pDst;
        __m128i xmm0, xmm1, xmm7;

        xmm7 = _mm_set1_epi16(1 << 6);
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            pAvg = pAvgRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels from source and from avg */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm1 = _mm_loadu_si128((__m128i *)pAvg);

                /* add, round, clip back to 8 bits */
                xmm1 = _mm_adds_epi16(xmm1, xmm7);
                xmm0 = _mm_adds_epi16(xmm0, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, 7);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    pAvg += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
            pAvgRef += avgPitch;
        } while (--height);
    }

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
