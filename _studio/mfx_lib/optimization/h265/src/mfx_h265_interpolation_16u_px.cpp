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

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))

//=========================================================
// AYA TMP SOLUTIONS
//#include <intrin.h> // aya for WriteP.B/N only
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

#undef MAKE_NAME

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define MAKE_NAME( func ) func ## _16u_px
#else
    #define MAKE_NAME( func ) func
#endif

    typedef Ipp16u PixType;

    void MAKE_NAME(h265_InterpLuma_s8_d16_H)(const PixType* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index,
        int width, int height, int shift, short offset)
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


    void MAKE_NAME(h265_InterpChroma_s8_d16_H)(const PixType* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index,
        int width, int height, int shift, short offset, int plane)
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


    void MAKE_NAME(h265_InterpLuma_s8_d16_V)(const PixType* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, \
        int width, int height, int shift, short offset)
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


    void MAKE_NAME(h265_InterpChroma_s8_d16_V)  (const PixType* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index,
        int width, int height, int shift, short offset)
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


    void MAKE_NAME(h265_InterpLuma_s16_d16_V)    (const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index,
        int width, int height, int shift, short offset)
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


    void MAKE_NAME(h265_InterpChroma_s16_d16_V)  (const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index,
        int width, int height, int shift, short offset)
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
    void MAKE_NAME(h265_AverageModeN)(short *pSrc, unsigned int srcPitch, PixType *pDst, unsigned int dstPitch, int width, int height)
    {
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                pDst[y*dstPitch + x] = (PixType)Saturate(0, 255, pSrc[y*srcPitch + x]);
            }
        }
    }

    /* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
    void MAKE_NAME(h265_AverageModeP)(short *pSrc, unsigned int srcPitch, PixType *pAvg, unsigned int avgPitch, PixType *pDst, unsigned int dstPitch, int width, int height)
    {
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                int add1 = pSrc[y*srcPitch + x];
                int add2 = (int)pAvg[y*avgPitch + x];
                add2 <<= 6;
                int add3 = 1 << 6;

                int sum_total = (add1 + add2 + add3) >> 7;
         
                pDst[y*dstPitch + x] = (Ipp8u)Saturate(0, 255, sum_total);
            }
        }
    }

    /* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
    void MAKE_NAME(h265_AverageModeB)(short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, PixType *pDst, unsigned int dstPitch, int width, int height)
    {
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                Ipp8u dst_tmp = (Ipp8u)Saturate(0, 255, (pSrc[y*srcPitch + x] + (pAvg[y*avgPitch + x] + (1<<6))) >> 7);
                pDst[y*dstPitch + x] = dst_tmp;//(Ipp8u)Saturate(0, 255, (pSrc[y*srcPitch + x] + (Ipp16s)pAvg[y*avgPitch + x] + 1) >> 7);
            }
        }
    }

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
