//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_dispatcher.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined (MFX_TARGET_OPTIMIZATION_AUTO)

#include "ippdefs.h"

namespace MFX_HEVC_PP
{

#define CoeffsType Ipp16s
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

    /* Inverse transforms */

    void h265_dst_inv4x4(CoeffsType *srcdst,
        Ipp32s bit_depth);

    void h265_dct_inv4x4(CoeffsType *srcdst,
        Ipp32s bit_depth);

    void h265_dct_inv8x8(CoeffsType *srcdst,
        Ipp32s bit_depth);

    void h265_dct_inv16x16(CoeffsType *srcdst,
        Ipp32s bit_depth);

    void h265_dct_inv32x32(CoeffsType *srcdst,
        Ipp32s bit_depth);


    static const Ipp16s h265_t4[4][4] =
    {
        { 64, 64, 64, 64},
        { 83, 36,-36,-83},
        { 64,-64,-64, 64},
        { 36,-83, 83,-36}
    };

    static const Ipp16s h265_t8[8][8] =
    {
        { 64, 64, 64, 64, 64, 64, 64, 64},
        { 89, 75, 50, 18,-18,-50,-75,-89},
        { 83, 36,-36,-83,-83,-36, 36, 83},
        { 75,-18,-89,-50, 50, 89, 18,-75},
        { 64,-64,-64, 64, 64,-64,-64, 64},
        { 50,-89, 18, 75,-75,-18, 89,-50},
        { 36,-83, 83,-36,-36, 83,-83, 36},
        { 18,-50, 75,-89, 89,-75, 50,-18}
    };

    static const Ipp16s h265_t16[16][16] =
    {
        { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
        { 90, 87, 80, 70, 57, 43, 25,  9, -9,-25,-43,-57,-70,-80,-87,-90},
        { 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89},
        { 87, 57,  9,-43,-80,-90,-70,-25, 25, 70, 90, 80, 43, -9,-57,-87},
        { 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83},
        { 80,  9,-70,-87,-25, 57, 90, 43,-43,-90,-57, 25, 87, 70, -9,-80},
        { 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75},
        { 70,-43,-87,  9, 90, 25,-80,-57, 57, 80,-25,-90, -9, 87, 43,-70},
        { 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64},
        { 57,-80,-25, 90, -9,-87, 43, 70,-70,-43, 87,  9,-90, 25, 80,-57},
        { 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50},
        { 43,-90, 57, 25,-87, 70,  9,-80, 80, -9,-70, 87,-25,-57, 90,-43},
        { 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36},
        { 25,-70, 90,-80, 43,  9,-57, 87,-87, 57, -9,-43, 80,-90, 70,-25},
        { 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18},
        {  9,-25, 43,-57, 70,-80, 87,-90, 90,-87, 80,-70, 57,-43, 25, -9}
    };

    static const Ipp16s h265_t32[32][32] =
    {
        { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
        { 90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13,  4, -4,-13,-22,-31,-38,-46,-54,-61,-67,-73,-78,-82,-85,-88,-90,-90},
        { 90, 87, 80, 70, 57, 43, 25,  9, -9,-25,-43,-57,-70,-80,-87,-90,-90,-87,-80,-70,-57,-43,-25, -9,  9, 25, 43, 57, 70, 80, 87, 90},
        { 90, 82, 67, 46, 22, -4,-31,-54,-73,-85,-90,-88,-78,-61,-38,-13, 13, 38, 61, 78, 88, 90, 85, 73, 54, 31,  4,-22,-46,-67,-82,-90},
        { 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89, 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89},
        { 88, 67, 31,-13,-54,-82,-90,-78,-46, -4, 38, 73, 90, 85, 61, 22,-22,-61,-85,-90,-73,-38,  4, 46, 78, 90, 82, 54, 13,-31,-67,-88},
        { 87, 57,  9,-43,-80,-90,-70,-25, 25, 70, 90, 80, 43, -9,-57,-87,-87,-57, -9, 43, 80, 90, 70, 25,-25,-70,-90,-80,-43,  9, 57, 87},
        { 85, 46,-13,-67,-90,-73,-22, 38, 82, 88, 54, -4,-61,-90,-78,-31, 31, 78, 90, 61,  4,-54,-88,-82,-38, 22, 73, 90, 67, 13,-46,-85},
        { 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83},
        { 82, 22,-54,-90,-61, 13, 78, 85, 31,-46,-90,-67,  4, 73, 88, 38,-38,-88,-73, -4, 67, 90, 46,-31,-85,-78,-13, 61, 90, 54,-22,-82},
        { 80,  9,-70,-87,-25, 57, 90, 43,-43,-90,-57, 25, 87, 70, -9,-80,-80, -9, 70, 87, 25,-57,-90,-43, 43, 90, 57,-25,-87,-70,  9, 80},
        { 78, -4,-82,-73, 13, 85, 67,-22,-88,-61, 31, 90, 54,-38,-90,-46, 46, 90, 38,-54,-90,-31, 61, 88, 22,-67,-85,-13, 73, 82,  4,-78},
        { 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75, 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75},
        { 73,-31,-90,-22, 78, 67,-38,-90,-13, 82, 61,-46,-88, -4, 85, 54,-54,-85,  4, 88, 46,-61,-82, 13, 90, 38,-67,-78, 22, 90, 31,-73},
        { 70,-43,-87,  9, 90, 25,-80,-57, 57, 80,-25,-90, -9, 87, 43,-70,-70, 43, 87, -9,-90,-25, 80, 57,-57,-80, 25, 90,  9,-87,-43, 70},
        { 67,-54,-78, 38, 85,-22,-90,  4, 90, 13,-88,-31, 82, 46,-73,-61, 61, 73,-46,-82, 31, 88,-13,-90, -4, 90, 22,-85,-38, 78, 54,-67},
        { 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64},
        { 61,-73,-46, 82, 31,-88,-13, 90, -4,-90, 22, 85,-38,-78, 54, 67,-67,-54, 78, 38,-85,-22, 90,  4,-90, 13, 88,-31,-82, 46, 73,-61},
        { 57,-80,-25, 90, -9,-87, 43, 70,-70,-43, 87,  9,-90, 25, 80,-57,-57, 80, 25,-90,  9, 87,-43,-70, 70, 43,-87, -9, 90,-25,-80, 57},
        { 54,-85, -4, 88,-46,-61, 82, 13,-90, 38, 67,-78,-22, 90,-31,-73, 73, 31,-90, 22, 78,-67,-38, 90,-13,-82, 61, 46,-88,  4, 85,-54},
        { 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50, 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50},
        { 46,-90, 38, 54,-90, 31, 61,-88, 22, 67,-85, 13, 73,-82,  4, 78,-78, -4, 82,-73,-13, 85,-67,-22, 88,-61,-31, 90,-54,-38, 90,-46},
        { 43,-90, 57, 25,-87, 70,  9,-80, 80, -9,-70, 87,-25,-57, 90,-43,-43, 90,-57,-25, 87,-70, -9, 80,-80,  9, 70,-87, 25, 57,-90, 43},
        { 38,-88, 73, -4,-67, 90,-46,-31, 85,-78, 13, 61,-90, 54, 22,-82, 82,-22,-54, 90,-61,-13, 78,-85, 31, 46,-90, 67,  4,-73, 88,-38},
        { 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36},
        { 31,-78, 90,-61,  4, 54,-88, 82,-38,-22, 73,-90, 67,-13,-46, 85,-85, 46, 13,-67, 90,-73, 22, 38,-82, 88,-54, -4, 61,-90, 78,-31},
        { 25,-70, 90,-80, 43,  9,-57, 87,-87, 57, -9,-43, 80,-90, 70,-25,-25, 70,-90, 80,-43, -9, 57,-87, 87,-57,  9, 43,-80, 90,-70, 25},
        { 22,-61, 85,-90, 73,-38, -4, 46,-78, 90,-82, 54,-13,-31, 67,-88, 88,-67, 31, 13,-54, 82,-90, 78,-46,  4, 38,-73, 90,-85, 61,-22},
        { 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18, 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18},
        { 13,-38, 61,-78, 88,-90, 85,-73, 54,-31,  4, 22,-46, 67,-82, 90,-90, 82,-67, 46,-22, -4, 31,-54, 73,-85, 90,-88, 78,-61, 38,-13},
        {  9,-25, 43,-57, 70,-80, 87,-90, 90,-87, 80,-70, 57,-43, 25, -9, -9, 25,-43, 57,-70, 80,-87, 90,-90, 87,-80, 70,-57, 43,-25,  9},
        {  4,-13, 22,-31, 38,-46, 54,-61, 67,-73, 78,-82, 85,-88, 90,-90, 90,-90, 88,-85, 82,-78, 73,-67, 61,-54, 46,-38, 31,-22, 13, -4}
    };

    static
        void h265_transform_partial_butterfly_inv4(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j;
        Ipp32s E[2], O[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 4;

        for (j = 0; j < line; j++)
        {
            /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
            O[0] = h265_t4[1][0]*src[line] + h265_t4[3][0]*src[3*line];
            O[1] = h265_t4[1][1]*src[line] + h265_t4[3][1]*src[3*line];
            E[0] = h265_t4[0][0]*src[0   ] + h265_t4[2][0]*src[2*line];
            E[1] = h265_t4[0][1]*src[0   ] + h265_t4[2][1]*src[2*line];

            /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
            dst[0] = (CoeffsType)Saturate(-32768, 32767, (E[0] + O[0] + add) >> shift);
            dst[1] = (CoeffsType)Saturate(-32768, 32767, (E[1] + O[1] + add) >> shift);
            dst[2] = (CoeffsType)Saturate(-32768, 32767, (E[1] - O[1] + add) >> shift);
            dst[3] = (CoeffsType)Saturate(-32768, 32767, (E[0] - O[0] + add) >> shift);

            src++;
            dst += 4;
        }
    }

    static
        void h265_transform_partial_butterfly_inv8(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j,k;
        Ipp32s E[4], O[4];
        Ipp32s EE[2], EO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 8;

        for (j = 0; j < line; j++)
        {
            /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
            for (k = 0; k < 4; k++)
            {
                O[k] = h265_t8[1][k]*src[line] + h265_t8[3][k]*src[3*line] + h265_t8[5][k]*src[5*line] + h265_t8[7][k]*src[7*line];
            }

            EO[0] = h265_t8[2][0]*src[2*line] + h265_t8[6][0]*src[6*line];
            EO[1] = h265_t8[2][1]*src[2*line] + h265_t8[6][1]*src[6*line];
            EE[0] = h265_t8[0][0]*src[0     ] + h265_t8[4][0]*src[4*line];
            EE[1] = h265_t8[0][1]*src[0     ] + h265_t8[4][1]*src[4*line];

            /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
            E[0] = EE[0] + EO[0];
            E[3] = EE[0] - EO[0];
            E[1] = EE[1] + EO[1];
            E[2] = EE[1] - EO[1];

            for (k = 0; k < 4;k++)
            {
                dst[k  ] = (CoeffsType)(Saturate((E[k] + O[k] + add) >> shift, -32768, 32767));
                dst[k+4] = (CoeffsType)(Saturate((E[3-k] - O[3-k] + add) >> shift, -32768, 32767));
            }
            src++;
            dst += 8;
        }
    }

    static
        void h265_transform_partial_butterfly_inv16(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j,k;
        Ipp32s E[8], O[8];
        Ipp32s EE[4], EO[4];
        Ipp32s EEE[2], EEO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 16;

        for (j = 0; j < line; j++)
        {
            /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
            for (k = 0; k < 8; k++)
            {
                O[k] = h265_t16[ 1][k]*src[ 1*line] + h265_t16[ 3][k]*src[ 3*line] +
                    h265_t16[ 5][k]*src[ 5*line] + h265_t16[ 7][k]*src[ 7*line] +
                    h265_t16[ 9][k]*src[ 9*line] + h265_t16[11][k]*src[11*line] +
                    h265_t16[13][k]*src[13*line] + h265_t16[15][k]*src[15*line];
            }

            for (k = 0; k < 4; k++)
            {
                EO[k] = h265_t16[ 2][k]*src[ 2*line] + h265_t16[ 6][k]*src[ 6*line] +
                    h265_t16[10][k]*src[10*line] + h265_t16[14][k]*src[14*line];
            }
            EEO[0] = h265_t16[4][0]*src[4*line] + h265_t16[12][0]*src[12*line];
            EEE[0] = h265_t16[0][0]*src[0     ] + h265_t16[ 8][0]*src[ 8*line];
            EEO[1] = h265_t16[4][1]*src[4*line] + h265_t16[12][1]*src[12*line];
            EEE[1] = h265_t16[0][1]*src[0     ] + h265_t16[ 8][1]*src[ 8*line];

            /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
            for (k = 0; k < 2; k++)
            {
                EE[k]   = EEE[k]   + EEO[k];
                EE[k+2] = EEE[1-k] - EEO[1-k];
            }

            for (k = 0; k < 4; k++)
            {
                E[k]   = EE[k]   + EO[k];
                E[k+4] = EE[3-k] - EO[3-k];
            }

            for (k = 0; k < 8 ;k++)
            {
                dst[k]   = (CoeffsType)Saturate((E[k]   + O[k]   + add) >> shift, -32768, 32767);
                dst[k+8] = (CoeffsType)Saturate((E[7-k] - O[7-k] + add) >> shift, -32768, 32767);
            }

            src++;
            dst += 16;
        }
    }

    static
        void h265_transform_partial_butterfly_inv32(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j, k;
        Ipp32s E[16], O[16];
        Ipp32s EE[8], EO[8];
        Ipp32s EEE[4], EEO[4];
        Ipp32s EEEE[2], EEEO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 32;

        for (j = 0; j < line; j++)
        {
            /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
            for (k = 0; k < 16; k++)
            {
                O[k] = h265_t32[ 1][k]*src[   line] + h265_t32[ 3][k]*src[ 3*line] +
                    h265_t32[ 5][k]*src[ 5*line] + h265_t32[ 7][k]*src[ 7*line] +
                    h265_t32[ 9][k]*src[ 9*line] + h265_t32[11][k]*src[11*line] +
                    h265_t32[13][k]*src[13*line] + h265_t32[15][k]*src[15*line] +
                    h265_t32[17][k]*src[17*line] + h265_t32[19][k]*src[19*line] +
                    h265_t32[21][k]*src[21*line] + h265_t32[23][k]*src[23*line] +
                    h265_t32[25][k]*src[25*line] + h265_t32[27][k]*src[27*line] +
                    h265_t32[29][k]*src[29*line] + h265_t32[31][k]*src[31*line];
            }
            for (k = 0; k < 8; k++)
            {
                EO[k] = h265_t32[ 2][k]*src[ 2*line] + h265_t32[ 6][k]*src[ 6*line] +
                    h265_t32[10][k]*src[10*line] + h265_t32[14][k]*src[14*line] +
                    h265_t32[18][k]*src[18*line] + h265_t32[22][k]*src[22*line] +
                    h265_t32[26][k]*src[26*line] + h265_t32[30][k]*src[30*line];
            }
            for (k = 0; k < 4; k++)
            {
                EEO[k] = h265_t32[4][k]*src[4*line] + h265_t32[12][k]*src[12*line] +
                    h265_t32[20][k]*src[20*line] + h265_t32[28][k]*src[28*line];
            }
            EEEO[0] = h265_t32[8][0]*src[8*line] + h265_t32[24][0]*src[24*line];
            EEEO[1] = h265_t32[8][1]*src[8*line] + h265_t32[24][1]*src[24*line];
            EEEE[0] = h265_t32[0][0]*src[0     ] + h265_t32[16][0]*src[16*line];
            EEEE[1] = h265_t32[0][1]*src[0     ] + h265_t32[16][1]*src[16*line];

            /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
            EEE[0] = EEEE[0] + EEEO[0];
            EEE[3] = EEEE[0] - EEEO[0];
            EEE[1] = EEEE[1] + EEEO[1];
            EEE[2] = EEEE[1] - EEEO[1];

            for (k = 0; k < 4; k++)
            {
                EE[k]   = EEE[k]   + EEO[k];
                EE[k+4] = EEE[3-k] - EEO[3-k];
            }

            for (k = 0; k < 8; k++)
            {
                E[k]   = EE[k]   + EO[k];
                E[k+8] = EE[7-k] - EO[7-k];
            }

            for (k = 0; k < 16; k++)
            {
                dst[k]    = (CoeffsType)Saturate((E[k]    + O[k]    + add) >> shift, -32768, 32767);
                dst[k+16] = (CoeffsType)Saturate((E[15-k] - O[15-k] + add) >> shift, -32768, 32767);
            }

            src++;
            dst += 32;
        }
    }

    static
        void h265_transform_fast_inverse_dst(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s i, c[4];
        Ipp32s add = 1<<(shift-1);

        for (i = 0; i < 4; i++)
        {
            c[0] = src[  i] + src[ 8+i];
            c[1] = src[8+i] + src[12+i];
            c[2] = src[  i] - src[12+i];
            c[3] = 74* src[4+i];

            dst[4*i+0] = (CoeffsType)Saturate((29*c[0] + 55*c[1]      + c[3]       + add) >> shift, -32768, 32767);
            dst[4*i+1] = (CoeffsType)Saturate((55*c[2] - 29*c[1]      + c[3]       + add) >> shift, -32768, 32767);
            dst[4*i+2] = (CoeffsType)Saturate((74*(src[i] - src[8+i]  + src[12+i]) + add) >> shift, -32768, 32767);
            dst[4*i+3] = (CoeffsType)Saturate((55*c[0] + 29*c[2]      - c[3]       + add) >> shift, -32768, 32767);
        }
    }

    static
        void h265_transform_partial_butterfly_fwd4(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j;
        Ipp32s E[2], O[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 4;

        for (j = 0; j < 4; j++)
        {
            /* E and O */
            E[0] = src[0] + src[3];
            O[0] = src[0] - src[3];
            E[1] = src[1] + src[2];
            O[1] = src[1] - src[2];

            dst[0*line] = (CoeffsType)((h265_t4[0][0]*E[0] + h265_t4[0][1]*E[1] + add) >> shift);
            dst[1*line] = (CoeffsType)((h265_t4[1][0]*O[0] + h265_t4[1][1]*O[1] + add) >> shift);
            dst[2*line] = (CoeffsType)((h265_t4[2][0]*E[0] + h265_t4[2][1]*E[1] + add) >> shift);
            dst[3*line] = (CoeffsType)((h265_t4[3][0]*O[0] + h265_t4[3][1]*O[1] + add) >> shift);

            src += 4;
            dst++;
        }
    }

    static
        void h265_transform_partial_butterfly_fwd8(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j,k;
        Ipp32s E[4], O[4];
        Ipp32s EE[2], EO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 8;

        for (j = 0; j < line; j++)
        {
            /* E and O*/
            for (k = 0; k < 4; k++)
            {
                E[k] = src[k] + src[7-k];
                O[k] = src[k] - src[7-k];
            }

            /* EE and EO */
            EE[0] = E[0] + E[3];
            EO[0] = E[0] - E[3];
            EE[1] = E[1] + E[2];
            EO[1] = E[1] - E[2];

            dst[0*line] = (CoeffsType)((h265_t8[0][0]*EE[0] + h265_t8[0][1]*EE[1] + add) >> shift);
            dst[2*line] = (CoeffsType)((h265_t8[2][0]*EO[0] + h265_t8[2][1]*EO[1] + add) >> shift);
            dst[4*line] = (CoeffsType)((h265_t8[4][0]*EE[0] + h265_t8[4][1]*EE[1] + add) >> shift);
            dst[6*line] = (CoeffsType)((h265_t8[6][0]*EO[0] + h265_t8[6][1]*EO[1] + add) >> shift);

            dst[1*line] = (CoeffsType)((h265_t8[1][0]*O[0] + h265_t8[1][1]*O[1] + h265_t8[1][2]*O[2] + h265_t8[1][3]*O[3] + add) >> shift);
            dst[3*line] = (CoeffsType)((h265_t8[3][0]*O[0] + h265_t8[3][1]*O[1] + h265_t8[3][2]*O[2] + h265_t8[3][3]*O[3] + add) >> shift);
            dst[5*line] = (CoeffsType)((h265_t8[5][0]*O[0] + h265_t8[5][1]*O[1] + h265_t8[5][2]*O[2] + h265_t8[5][3]*O[3] + add) >> shift);
            dst[7*line] = (CoeffsType)((h265_t8[7][0]*O[0] + h265_t8[7][1]*O[1] + h265_t8[7][2]*O[2] + h265_t8[7][3]*O[3] + add) >> shift);

            src += 8;
            dst++;
        }
    }

    static
        void h265_transform_partial_butterfly_fwd16(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j, k;
        Ipp32s E[8], O[8];
        Ipp32s EE[4], EO[4];
        Ipp32s EEE[2], EEO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 16;

        for (j = 0; j < line; j++)
        {
            /* E and O*/
            for (k = 0; k < 8; k++)
            {
                E[k] = src[k] + src[15-k];
                O[k] = src[k] - src[15-k];
            }

            /* EE and EO */
            for (k = 0; k < 4; k++)
            {
                EE[k] = E[k] + E[7-k];
                EO[k] = E[k] - E[7-k];
            }

            /* EEE and EEO */
            EEE[0] = EE[0] + EE[3];
            EEO[0] = EE[0] - EE[3];
            EEE[1] = EE[1] + EE[2];
            EEO[1] = EE[1] - EE[2];

            dst[ 0*line] = (CoeffsType)((h265_t16[ 0][0]*EEE[0] + h265_t16[ 0][1]*EEE[1] + add) >> shift);
            dst[ 4*line] = (CoeffsType)((h265_t16[ 4][0]*EEO[0] + h265_t16[ 4][1]*EEO[1] + add) >> shift);
            dst[ 8*line] = (CoeffsType)((h265_t16[ 8][0]*EEE[0] + h265_t16[ 8][1]*EEE[1] + add) >> shift);
            dst[12*line] = (CoeffsType)((h265_t16[12][0]*EEO[0] + h265_t16[12][1]*EEO[1] + add) >> shift);

            for (k = 2; k < 16; k += 4)
            {
                dst[k*line] = (CoeffsType)((h265_t16[k][0]*EO[0] + h265_t16[k][1]*EO[1] +
                    h265_t16[k][2]*EO[2] + h265_t16[k][3]*EO[3] + add) >> shift);
            }

            for (k = 1; k < 16;  k+= 2)
            {
                dst[k*line] = (CoeffsType)((h265_t16[k][0]*O[0] + h265_t16[k][1]*O[1] +
                    h265_t16[k][2]*O[2] + h265_t16[k][3]*O[3] +
                    h265_t16[k][4]*O[4] + h265_t16[k][5]*O[5] +
                    h265_t16[k][6]*O[6] + h265_t16[k][7]*O[7] + add) >> shift);
            }

            src += 16;
            dst++;
        }
    }

    static
        void h265_transform_partial_butterfly_fwd32(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s j,k;
        Ipp32s E[16], O[16];
        Ipp32s EE[8], EO[8];
        Ipp32s EEE[4], EEO[4];
        Ipp32s EEEE[2], EEEO[2];
        Ipp32s add = 1<<(shift-1);
        const Ipp32s line = 32;

        for (j = 0; j < line; j++)
        {
            /* E and O*/
            for (k = 0; k < 16; k++)
            {
                E[k] = src[k] + src[31-k];
                O[k] = src[k] - src[31-k];
            }

            /* EE and EO */
            for (k = 0; k < 8; k++)
            {
                EE[k] = E[k] + E[15-k];
                EO[k] = E[k] - E[15-k];
            }

            /* EEE and EEO */
            for (k = 0; k < 4; k++)
            {
                EEE[k] = EE[k] + EE[7-k];
                EEO[k] = EE[k] - EE[7-k];
            }

            /* EEEE and EEEO */
            EEEE[0] = EEE[0] + EEE[3];
            EEEO[0] = EEE[0] - EEE[3];
            EEEE[1] = EEE[1] + EEE[2];
            EEEO[1] = EEE[1] - EEE[2];

            dst[ 0*line] = (CoeffsType)((h265_t32[ 0][0]*EEEE[0] + h265_t32[ 0][1]*EEEE[1] + add) >> shift);
            dst[ 8*line] = (CoeffsType)((h265_t32[ 8][0]*EEEO[0] + h265_t32[ 8][1]*EEEO[1] + add) >> shift);
            dst[16*line] = (CoeffsType)((h265_t32[16][0]*EEEE[0] + h265_t32[16][1]*EEEE[1] + add) >> shift);
            dst[24*line] = (CoeffsType)((h265_t32[24][0]*EEEO[0] + h265_t32[24][1]*EEEO[1] + add) >> shift);

            for (k = 4; k < 32; k+=8)
            {
                dst[k*line] = (CoeffsType)((h265_t32[k][0]*EEO[0] + h265_t32[k][1]*EEO[1] +
                    h265_t32[k][2]*EEO[2] + h265_t32[k][3]*EEO[3] + add) >> shift);
            }

            for (k = 2; k < 32; k+=4)
            {
                dst[k*line] = (CoeffsType)((h265_t32[k][0]*EO[0] + h265_t32[k][1]*EO[1] +
                    h265_t32[k][2]*EO[2] + h265_t32[k][3]*EO[3] +
                    h265_t32[k][4]*EO[4] + h265_t32[k][5]*EO[5] +
                    h265_t32[k][6]*EO[6] + h265_t32[k][7]*EO[7] + add) >> shift);
            }

            for (k = 1; k < 32; k+=2)
            {
                dst[k*line] = (CoeffsType)((h265_t32[k][ 0]*O[ 0] + h265_t32[k][ 1]*O[ 1] +
                    h265_t32[k][ 2]*O[ 2] + h265_t32[k][ 3]*O[ 3] +
                    h265_t32[k][ 4]*O[ 4] + h265_t32[k][ 5]*O[ 5] +
                    h265_t32[k][ 6]*O[ 6] + h265_t32[k][ 7]*O[ 7] +
                    h265_t32[k][ 8]*O[ 8] + h265_t32[k][ 9]*O[ 9] +
                    h265_t32[k][10]*O[10] + h265_t32[k][11]*O[11] +
                    h265_t32[k][12]*O[12] + h265_t32[k][13]*O[13] +
                    h265_t32[k][14]*O[14] + h265_t32[k][15]*O[15] + add) >> shift);
            }

            src += 32;
            dst++;
        }
    }

    static
        void h265_transform_fast_forward_dst(CoeffsType *src,
        CoeffsType *dst,
        Ipp32s shift)
    {
        Ipp32s i, c[4];
        Ipp32s add = 1<<(shift-1);

        for (i = 0; i < 4; i++)
        {
            c[0] = src[4*i+0] + src[4*i+3];
            c[1] = src[4*i+1] + src[4*i+3];
            c[2] = src[4*i+0] - src[4*i+1];
            c[3] = 74*src[4*i+2];

            dst[   i] = (CoeffsType)((29*c[0] + 55*c[1]           + c[3]        + add) >> shift);
            dst[ 4+i] = (CoeffsType)((74*(src[4*i+0] + src[4*i+1] - src[4*i+3]) + add) >> shift);
            dst[ 8+i] = (CoeffsType)((29*c[2] + 55*c[0]           - c[3]        + add) >> shift);
            dst[12+i] = (CoeffsType)((55*c[2] - 29*c[1]           + c[3]        + add) >> shift);
        }
    }

    /* forward transform */
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void H265_FASTCALL h265_DST4x4Fwd_16s_px( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#else
    void H265_FASTCALL h265_DST4x4Fwd_16s( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#endif
    {
        const int bit_depth = 8;
        CoeffsType tmp[4*4];
        h265_transform_fast_forward_dst((Ipp16s*)src, tmp, bit_depth - 7);
        h265_transform_fast_forward_dst(tmp, dst, 8);
    }


#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void H265_FASTCALL h265_DCT4x4Fwd_16s_px( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#else
    void H265_FASTCALL h265_DCT4x4Fwd_16s( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#endif
    {
        const int bit_depth = 8;
        CoeffsType tmp[4*4];
        h265_transform_partial_butterfly_fwd4((Ipp16s*)src, tmp, bit_depth - 7);
        h265_transform_partial_butterfly_fwd4(tmp, dst, 8);
    }


#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void H265_FASTCALL h265_DCT8x8Fwd_16s_px( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#else
    void H265_FASTCALL h265_DCT8x8Fwd_16s( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#endif
    {
        const int bit_depth = 8;
        CoeffsType tmp[8*8];
        h265_transform_partial_butterfly_fwd8((Ipp16s*)src, tmp, bit_depth - 6);
        h265_transform_partial_butterfly_fwd8(tmp, dst, 9); 
    }


#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void H265_FASTCALL h265_DCT16x16Fwd_16s_px( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#else
    void H265_FASTCALL h265_DCT16x16Fwd_16s( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#endif
    {
        const int bit_depth = 8;
        CoeffsType tmp[16*16];
        h265_transform_partial_butterfly_fwd16((Ipp16s*)src, tmp, bit_depth - 5);
        h265_transform_partial_butterfly_fwd16(tmp, dst, 10);
    }
    
    
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    void H265_FASTCALL h265_DCT32x32Fwd_16s_px( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#else
    void H265_FASTCALL h265_DCT32x32Fwd_16s( const short *H265_RESTRICT src, short *H265_RESTRICT dst )
#endif
    {
        const int bit_depth = 8;
        CoeffsType tmp[32*32];
        h265_transform_partial_butterfly_fwd32((Ipp16s*)src, tmp, bit_depth - 4);
        h265_transform_partial_butterfly_fwd32(tmp, dst, 11);
    }


    /* inverse transform */
    void h265_dst_inv4x4(CoeffsType *srcdst,
        Ipp32s bit_depth)
    {
        CoeffsType tmp[4*4];
        h265_transform_fast_inverse_dst(srcdst, tmp, 7);
        h265_transform_fast_inverse_dst(tmp, srcdst, 20 - bit_depth);
    }

    void h265_dct_inv4x4(CoeffsType *srcdst,
        Ipp32s bit_depth)
    {
        CoeffsType tmp[4*4];
        h265_transform_partial_butterfly_inv4(srcdst, tmp, 7);
        h265_transform_partial_butterfly_inv4(tmp, srcdst, 20 - bit_depth);
    }

    void h265_dct_inv8x8(CoeffsType *srcdst,
        Ipp32s bit_depth)
    {
        CoeffsType tmp[8*8];
        h265_transform_partial_butterfly_inv8(srcdst, tmp, 7);
        h265_transform_partial_butterfly_inv8(tmp, srcdst, 20 - bit_depth); }

    void h265_dct_inv16x16(CoeffsType *srcdst,
        Ipp32s bit_depth)
    {
        CoeffsType tmp[16*16];
        h265_transform_partial_butterfly_inv16(srcdst, tmp, 7);
        h265_transform_partial_butterfly_inv16(tmp, srcdst, 20 - bit_depth);
    }

    void h265_dct_inv32x32(CoeffsType *srcdst,
        Ipp32s bit_depth)
    {
        CoeffsType tmp[32*32];
        h265_transform_partial_butterfly_inv32(srcdst, tmp, 7);
        h265_transform_partial_butterfly_inv32(tmp, srcdst, 20 - bit_depth);
    }

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
