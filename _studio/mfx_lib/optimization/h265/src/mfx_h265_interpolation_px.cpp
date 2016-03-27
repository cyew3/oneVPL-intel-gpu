/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))
#define SaturateToShort(val) Saturate(-32768, 32767, (val))

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

    ALIGN_DECL(16) static const short filtTabLumaSc[3][8] = {
        { -1, 4, -10, 58, 17,  -5, 1,  0 },
        { -1, 4, -11, 40, 40, -11, 4, -1 },
        {  0, 1,  -5, 17, 58, -10, 4, -1 },
    };

//kolya
    ALIGN_DECL(16) static const short filtTabLumaScFast[3][4] = {
        { -4, 52, 20,  -4 },
        { -8, 40, 40,  -8 },
        { -4, 20, 52,  -4 },
    };

    ALIGN_DECL(16) static const short filtTabChromaSc[7][4] = {
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
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

//kolya
    void MAKE_NAME(h265_InterpLumaFast_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        short acc;
        const short* coeffs16x = filtTabLumaScFast[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j+k] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)SaturateToShort(acc);
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
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

    void MAKE_NAME(h265_InterpLuma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        int acc;
        const short* coeffs16x = filtTabLumaSc[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 8; k++)
                    acc += pSrc[j+k] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

    //kolya
    void MAKE_NAME(h265_InterpLumaFast_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        int acc;
        const short* coeffs16x = filtTabLumaScFast[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j+k] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }


    void MAKE_NAME(h265_InterpChroma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST, int plane)
    {
        int i, j, k, srcOff;
        int acc;
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
                pDst[j] = (short)SaturateToShort(acc);
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
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

    //kolya
    void MAKE_NAME(h265_InterpLumaFast_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        short acc;
        const short* coeffs16x = filtTabLumaScFast[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)SaturateToShort(acc);
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
                pDst[j] = (short)SaturateToShort(acc);
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
                pDst[j] = (short)SaturateToShort(acc);
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        }
    }

    //kolya
    void MAKE_NAME(h265_InterpLumaFast_s16_d16_V)    (INTERP_S16_D16_PARAMETERS_LIST)
    {
        int i, j, k;
        int acc;
        const short* coeffs16x = filtTabLumaScFast[tab_index - 1];

        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                acc = 0;
                for (k = 0; k < 4; k++)
                    acc += pSrc[j + k*srcPitch] * coeffs16x[k];
                acc += offset;
                acc >>= shift;
                pDst[j] = (short)SaturateToShort(acc);
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
                pDst[j] = (short)SaturateToShort(acc);
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
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                pDst[y*dstPitch + x] = (Ipp8u)Saturate(0, 255, pSrc[y*srcPitch + x]);
            }
        }
    }

    /* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
    void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
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
    void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
    {
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                Ipp8u dst_tmp = (Ipp8u)Saturate(0, 255, (pSrc[y*srcPitch + x] + (pAvg[y*avgPitch + x] + (1<<6))) >> 7);
                /*if( dst_tmp != pDst_own[y*dstPitch + x])
                {
                    printf("\n stop!!! \n");
                }*/
                pDst[y*dstPitch + x] = dst_tmp;//(Ipp8u)Saturate(0, 255, (pSrc[y*srcPitch + x] + (Ipp16s)pAvg[y*avgPitch + x] + 1) >> 7);
            }
        }
    }

    /* 16-bit versions */
    void MAKE_NAME(h265_AverageModeN_U16)(short *pSrc, unsigned int srcPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth)
    {
        Ipp32s max_value = (1 << bit_depth) - 1;
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                pDst[y*dstPitch + x] = (Ipp16u)Saturate(0, max_value, pSrc[y*srcPitch + x]);
            }
        }
    }

    void MAKE_NAME(h265_AverageModeP_U16)(short *pSrc, unsigned int srcPitch, Ipp16u *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth)
    {
        Ipp32s max_value = (1 << bit_depth) - 1;
        Ipp32s shift = 15 - bit_depth;
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                int add1 = pSrc[y*srcPitch + x];
                int add2 = (int)pAvg[y*avgPitch + x];
                add2 <<= shift - 1;
                int add3 = 1 << (shift - 1);

                int sum_total = (add1 + add2 + add3) >> shift;
         
                pDst[y*dstPitch + x] = (Ipp16u)Saturate(0, max_value, sum_total);
            }
        }
    }

    void MAKE_NAME(h265_AverageModeB_U16)(short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth)
    {
        Ipp32s max_value = (1 << bit_depth) - 1;
        Ipp32s shift = 15 - bit_depth;
        Ipp32s offset = 1 << (shift - 1);
        for( int y = 0; y < height; y++  )
        {
            for(int x = 0; x < width; x++)
            {
                Ipp16u dst_tmp = (Ipp16u)Saturate(0, max_value, (pSrc[y*srcPitch + x] + (pAvg[y*avgPitch + x] + offset)) >> shift);
                pDst[y*dstPitch + x] = dst_tmp;
            }
        }
    }

    // pack intermediate interpolation pels s16 to u8/u16 [ dst = Saturate( (src + 32) >> 6, 0, (1 << bitDepth) - 1 ) ]
    template <typename PixType>
    void MAKE_NAME(h265_InterpLumaPack)(const short *src, int pitchSrc, PixType *dst, int pitchDst, int width, int height, int bitDepth)
    {
        int shift = 14 - bitDepth;
        int offset = 1 << (shift - 1);
        int maxPel = (1 << bitDepth) - 1;

        for (; height > 0; height--, src += pitchSrc, dst += pitchDst)
            for (Ipp32s col = 0; col < width; col++)
                dst[col] = Saturate(0, maxPel, (src[col] + offset) >> shift);
    }
    template void MAKE_NAME(h265_InterpLumaPack)<Ipp8u >(const Ipp16s *src, Ipp32s pitchSrc, Ipp8u  *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);
    template void MAKE_NAME(h265_InterpLumaPack)<Ipp16u>(const Ipp16s *src, Ipp32s pitchSrc, Ipp16u *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);

    void MAKE_NAME(h265_ConvertShiftR)(const short *src, int pitchSrc, unsigned char *dst, int pitchDst, int width, int height, int rshift)
    {
        int offset = 1 << (rshift - 1);
        int maxPel = (1 << 8) - 1;

        for (; height > 0; height--, src += pitchSrc, dst += pitchDst)
            for (Ipp32s col = 0; col < width; col++)
                dst[col] = Saturate(0, maxPel, (src[col] + offset) >> rshift);
    }

    template <typename PixType> void h265_Average_px(const PixType *pSrc0, Ipp32s pitchSrc0, const PixType *pSrc1, Ipp32s pitchSrc1, PixType *H265_RESTRICT pDst, Ipp32s pitchDst, Ipp32s width, Ipp32s height)
    {
#ifdef __INTEL_COMPILER
        #pragma ivdep
#endif // __INTEL_COMPILER
        for (int j = 0; j < height; j++, pSrc0 += pitchSrc0, pSrc1 += pitchSrc1, pDst += pitchDst) {
#ifdef __INTEL_COMPILER
            #pragma ivdep
            #pragma vector always
#endif // __INTEL_COMPILER
            for (int i = 0; i < width; i++)
                 pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);
        }
    }
    template void h265_Average_px<Ipp8u> (const Ipp8u  *pSrc0, Ipp32s pitchSrc0, const Ipp8u  *pSrc1, Ipp32s pitchSrc1, Ipp8u  *H265_RESTRICT pDst, Ipp32s pitchDst, Ipp32s width, Ipp32s height);
    template void h265_Average_px<Ipp16u>(const Ipp16u *pSrc0, Ipp32s pitchSrc0, const Ipp16u *pSrc1, Ipp32s pitchSrc1, Ipp16u *H265_RESTRICT pDst, Ipp32s pitchDst, Ipp32s width, Ipp32s height);

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
