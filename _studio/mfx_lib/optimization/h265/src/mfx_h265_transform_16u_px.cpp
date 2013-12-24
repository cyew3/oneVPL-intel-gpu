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

// Template magic :) and MSC produce warning. INTEL_COMPILER IS OK
// warning C4127: conditional expression is constant
#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

namespace MFX_HEVC_PP
{

#define CoeffsType Ipp16s
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define Clip3 Saturate


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

#define g_T4 h265_t4
#define g_T8 h265_t8
#define g_T16 h265_t16
#define g_T32 h265_t32

typedef Ipp16s H265CoeffsCommon;
typedef H265CoeffsCommon *H265CoeffsPtrCommon;

// ML: OPT: Parameterized to allow const 'shift' propogation
template <typename DstCoeffsType, bool inplace>
void FastInverseDst(Ipp32s shift, H265CoeffsPtrCommon tmp, DstCoeffsType* block, Ipp32s dstStride, Ipp32u bitDepth)  // input tmp, output block
{
  Ipp32s i, c[4];
  Ipp32s rnd_factor = 1<<(shift-1);

#ifdef __INTEL_COMPILER
// ML: OPT: vectorize with constant shift
#pragma unroll
#pragma vector always
#endif
  for (i = 0; i < 4; i++)
  {
    // Intermediate Variables
    c[0] = tmp[  i] + tmp[ 8+i];
    c[1] = tmp[8+i] + tmp[12+i];
    c[2] = tmp[  i] - tmp[12+i];
    c[3] = 74* tmp[4+i];

// ML: OPT: TODO: make sure compiler vectorizes and optimizes Clip3 as PACKS
    if (inplace)
    {
        Ipp32s max_value = (1 << bitDepth) - 1;
        block[dstStride*i+0] = (DstCoeffsType)Clip3(0, max_value, (( 29 * c[0] + 55 * c[1]     + c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+0]);
        block[dstStride*i+1] = (DstCoeffsType)Clip3(0, max_value, (( 55 * c[2] - 29 * c[1]     + c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+1]);
        block[dstStride*i+2] = (DstCoeffsType)Clip3(0, max_value, (( 74 * (tmp[i] - tmp[8+i]  + tmp[12+i]) + rnd_factor ) >> shift ) + block[dstStride*i+2]);
        block[dstStride*i+3] = (DstCoeffsType)Clip3(0, max_value, (( 55 * c[0] + 29 * c[2]     - c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+3]);
    }
    else
    {
        block[dstStride*i+0] = (DstCoeffsType)Clip3( -32768, 32767, ( 29 * c[0] + 55 * c[1]     + c[3]      + rnd_factor ) >> shift );
        block[dstStride*i+1] = (DstCoeffsType)Clip3( -32768, 32767, ( 55 * c[2] - 29 * c[1]     + c[3]      + rnd_factor ) >> shift );
        block[dstStride*i+2] = (DstCoeffsType)Clip3( -32768, 32767, ( 74 * (tmp[i] - tmp[8+i]  + tmp[12+i]) + rnd_factor ) >> shift );
        block[dstStride*i+3] = (DstCoeffsType)Clip3( -32768, 32767, ( 55 * c[0] + 29 * c[2]     - c[3]      + rnd_factor ) >> shift );
    }
  }
}

/** 4x4 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
// ML: OPT: Parameterized to allow const 'shift' propogation
template <typename DstCoeffsType, bool inplace>
void PartialButterflyInverse4x4(Ipp32s shift, H265CoeffsPtrCommon src, DstCoeffsType*dst, Ipp32s dstStride, Ipp32u bitDepth)
{
  Ipp32s j;
  Ipp32s E[2],O[2];
  Ipp32s add = 1<<(shift-1);
  const Ipp32s line = 4;

#ifdef __INTEL_COMPILER
// ML: OPT: vectorize with constant shift
#pragma vector always
#endif
  for (j=0; j<line; j++)
  {
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    O[0] = g_T4[1][0]*src[line] + g_T4[3][0]*src[3*line];
    O[1] = g_T4[1][1]*src[line] + g_T4[3][1]*src[3*line];
    E[0] = g_T4[0][0]*src[0] + g_T4[2][0]*src[2*line];
    E[1] = g_T4[0][1]*src[0] + g_T4[2][1]*src[2*line];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    if (inplace)
    {
        Ipp32s max_value = (1 << bitDepth) - 1;
        dst[0] = (DstCoeffsType)Clip3(0, max_value, ((E[0] + O[0] + add)>>shift ) + dst[0]);
        dst[1] = (DstCoeffsType)Clip3(0, max_value, ((E[1] + O[1] + add)>>shift ) + dst[1]);
        dst[2] = (DstCoeffsType)Clip3(0, max_value, ((E[1] - O[1] + add)>>shift ) + dst[2]);
        dst[3] = (DstCoeffsType)Clip3(0, max_value, ((E[0] - O[0] + add)>>shift ) + dst[3]);
    }
    else
    {
        dst[0] = (DstCoeffsType)Clip3( -32768, 32767, (E[0] + O[0] + add)>>shift );
        dst[1] = (DstCoeffsType)Clip3( -32768, 32767, (E[1] + O[1] + add)>>shift );
        dst[2] = (DstCoeffsType)Clip3( -32768, 32767, (E[1] - O[1] + add)>>shift );
        dst[3] = (DstCoeffsType)Clip3( -32768, 32767, (E[0] - O[0] + add)>>shift );
    }

    src++;
    dst += dstStride;
  }
}

/** 8x8 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
// ML: OPT: Parameterized to allow const 'shift' propogation
template <typename DstCoeffsType, bool inplace>
void PartialButterflyInverse8x8(Ipp32s shift, H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride, Ipp32u bitDepth)
{
    Ipp32s j;
    Ipp32s k;
    Ipp32s E[4];
    Ipp32s O[4];
    Ipp32s EE[2];
    Ipp32s EO[2];
    Ipp32s add = 1 << (shift - 1);
    const Ipp32s line = 8;

    // ML: OPT: TODO: vectorize manually, ICC does poor auto-vec
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (k = 0; k < 4; k++)
        {
            O[k] = g_T8[1][k] * src[line] + g_T8[3][k] * src[3 * line] + g_T8[5][k] * src[5 * line] + g_T8[7][k] * src[7 * line];
        }

        EO[0] = g_T8[2][0] * src[2 * line] + g_T8[6][0] * src[6 * line];
        EO[1] = g_T8[2][1] * src[2 * line] + g_T8[6][1] * src[6 * line];
        EE[0] = g_T8[0][0] * src[0]        + g_T8[4][0] * src[4 * line];
        EE[1] = g_T8[0][1] * src[0]        + g_T8[4][1] * src[4 * line];

        /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];

        // ML: OPT: TODO: failed ST->LD forwarding because ICC generates 128-bit load below from small stores above
//        #pragma unroll(4)
        for (k = 0; k < 4; k++)
        {
            if (inplace)
            {
                Ipp32s max_value = (1 << bitDepth) - 1;
                dst[k]     = (DstCoeffsType) Clip3(0, max_value, ((E[k]     + O[k]     + add) >> shift) + dst[k]);
                dst[k + 4] = (DstCoeffsType) Clip3(0, max_value, ((E[3 - k] - O[3 - k] + add) >> shift) + dst[k + 4]);
            }
            else
            {
                dst[k]     = (DstCoeffsType) Clip3( -32768, 32767, (E[k]     + O[k]     + add) >> shift);
                dst[k + 4] = (DstCoeffsType) Clip3( -32768, 32767, (E[3 - k] - O[3 - k] + add) >> shift);
            }
        }
        src ++;
        dst += dstStride;
    }
}

/** 16x16 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
// ML: OPT: Parameterized to allow const 'shift' propogation
template <typename DstCoeffsType, bool inplace>
void PartialButterflyInverse16x16(Ipp32s shift, H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride, Ipp32u bitDepth)
{
    Ipp32s j;
    Ipp32s k;
    Ipp32s E[8];
    Ipp32s O[8];
    Ipp32s EE[4];
    Ipp32s EO[4];
    Ipp32s EEE[2];
    Ipp32s EEO[2];
    Ipp32s add = 1 << (shift - 1);
    const Ipp32s line = 16;

    // ML: OPT: TODO: vectorize manually, ICC does not auto-vec
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (k = 0; k < 8; k++)
        {
            O[k] = g_T16[ 1][k] * src[line]     + g_T16[ 3][k] * src[ 3 * line] + g_T16[ 5][k] * src[ 5 * line] + g_T16[ 7][k] * src[ 7 * line] +
                   g_T16[ 9][k] * src[9 * line] + g_T16[11][k] * src[11 * line] + g_T16[13][k] * src[13 * line] + g_T16[15][k] * src[15 * line];
        }

#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (k = 0; k < 4; k++)
        {
            EO[k] = g_T16[2][k] * src[2 * line] + g_T16[6][k] * src[6 * line] + g_T16[10][k]*src[10*line] + g_T16[14][k]*src[14*line];
        }
        EEO[0] = g_T16[4][0] * src[4 * line] + g_T16[12][0] * src[12 * line];
        EEE[0] = g_T16[0][0] * src[0       ] + g_T16[ 8][0] * src[8  * line];
        EEO[1] = g_T16[4][1] * src[4 * line] + g_T16[12][1] * src[12 * line];
        EEE[1] = g_T16[0][1] * src[0       ] + g_T16[ 8][1] * src[8  * line];

        /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
        for (k = 0; k < 2; k++)
        {
            EE[k]     = EEE[k]     + EEO[k];
            EE[k + 2] = EEE[1 - k] - EEO[1 - k];
        }
        for (k = 0; k < 4; k++)
        {
            E[k]     = EE[k]     + EO[k];
            E[k + 4] = EE[3 - k] - EO[3 - k];
        }

#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (k = 0; k < 8; k++)
        {
            if (inplace)
            {
                Ipp32s max_value = (1 << bitDepth) - 1;
                dst[k]   = (DstCoeffsType) Clip3(0, max_value, ((E[k]     + O[k]     + add) >> shift) + dst[k]);
                dst[k+8] = (DstCoeffsType) Clip3(0, max_value, ((E[7 - k] - O[7 - k] + add) >> shift) + dst[k+8]);
            }
            else
            {
                dst[k]   = (DstCoeffsType) Clip3( -32768, 32767, (E[k]     + O[k]     + add) >> shift);
                dst[k+8] = (DstCoeffsType) Clip3( -32768, 32767, (E[7 - k] - O[7 - k] + add) >> shift);
            }
        }
        src ++;
        dst += dstStride;
    }
}

/** 32x32 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
// ML: OPT: Parameterized to allow const 'shift' propogation
template <typename DstCoeffsType, bool inplace>
void PartialButterflyInverse32x32(Ipp32s shift, H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride, Ipp32u bitDepth)
{
    Ipp32s j;
    Ipp32s k;
    Ipp32s E[16];
    Ipp32s O[16];
    Ipp32s EE[8];
    Ipp32s EO[8];
    Ipp32s EEE[4];
    Ipp32s EEO[4];
    Ipp32s EEEE[2];
    Ipp32s EEEO[2];
    Ipp32s add = 1 << (shift - 1);
    const Ipp32s line = 32;

    // ML: OPT: TODO: vectorize manually, ICC does not auto-vec
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
#ifdef __INTEL_COMPILER
        #pragma unroll(16)
#endif
        for (k = 0; k < 16; k++)
        {
            O[k] = g_T32[ 1][k]*src[line]      + g_T32[ 3][k] * src[ 3 * line] + g_T32[ 5][k] * src[ 5 * line] + g_T32[ 7][k] * src[ 7 * line] +
                   g_T32[ 9][k]*src[ 9 * line] + g_T32[11][k] * src[11 * line] + g_T32[13][k] * src[13 * line] + g_T32[15][k] * src[15 * line] +
                   g_T32[17][k]*src[17 * line] + g_T32[19][k] * src[19 * line] + g_T32[21][k] * src[21 * line] + g_T32[23][k] * src[23 * line] +
                   g_T32[25][k]*src[25 * line] + g_T32[27][k] * src[27 * line] + g_T32[29][k] * src[29 * line] + g_T32[31][k] * src[31 * line];
        }
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (k = 0; k < 8; k++)
        {
            EO[k] = g_T32[ 2][k] * src[ 2 * line] + g_T32[ 6][k] * src[ 6 * line] + g_T32[10][k] * src[10 * line] + g_T32[14][k] * src[14 * line] +
                    g_T32[18][k] * src[18 * line] + g_T32[22][k] * src[22 * line] + g_T32[26][k] * src[26 * line] + g_T32[30][k] * src[30 * line];
        }
#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (k = 0; k < 4; k++)
        {
            EEO[k] = g_T32[4][k] * src[ 4 * line] + g_T32[12][k] * src[12 * line] + g_T32[20][k] * src[20 * line] + g_T32[28][k] * src[28 * line];
        }
        EEEO[0] = g_T32[8][0] * src[ 8 * line] + g_T32[24][0] * src[24 * line];
        EEEO[1] = g_T32[8][1] * src[ 8 * line] + g_T32[24][1] * src[24 * line];
        EEEE[0] = g_T32[0][0] * src[ 0       ] + g_T32[16][0] * src[16 * line];
        EEEE[1] = g_T32[0][1] * src[ 0       ] + g_T32[16][1] * src[16 * line];

        /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
        EEE[0] = EEEE[0] + EEEO[0];
        EEE[3] = EEEE[0] - EEEO[0];
        EEE[1] = EEEE[1] + EEEO[1];
        EEE[2] = EEEE[1] - EEEO[1];
#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (k = 0; k < 4; k++)
        {
            EE[k]     = EEE[k]     + EEO[k];
            EE[k + 4] = EEE[3 - k] - EEO[3 - k];
        }

#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (k = 0; k < 8; k++)
        {
            E[k]     = EE[k]     + EO[k];
            E[k + 8] = EE[7 - k] - EO[7 - k];
        }
#ifdef __INTEL_COMPILER
        #pragma unroll(16)
#endif
        for (k = 0; k < 16; k++)
        {
            if (inplace)
            {
                Ipp32s max_value = (1 << bitDepth) - 1;
                dst[k]      = (DstCoeffsType)Clip3(0, max_value, ((E[k]      + O[k]      + add) >> shift) + dst[k]);
                dst[k + 16] = (DstCoeffsType)Clip3(0, max_value, ((E[15 - k] - O[15 - k] + add) >> shift) + dst[k + 16]);
            }
            else
            {
                dst[k]      = (DstCoeffsType) Clip3( -32768, 32767, (E[k]      + O[k]      + add) >> shift);
                dst[k + 16] = (DstCoeffsType) Clip3( -32768, 32767, (E[15 - k] - O[15 - k] + add) >> shift);
            }
        }
        src ++;
        dst += dstStride;
    }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#undef MAKE_NAME
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
#define MAKE_NAME( func ) func ##_16u_px
#else
#define MAKE_NAME( func ) func ## _16u
#endif

    /* ************************************************** */
    /*              inverse transform                     */
    /* ************************************************** */
    #define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
    #define SHIFT_INV_2ND         12 // Shift after second inverse transform stage

    void MAKE_NAME(h265_DST4x4Inv_16sT)(void *destPtr, const short *H265_RESTRICT coeff, int destStride, bool inplace, Ipp32u bitDepth)
    {
        Ipp16s tmp[4*4];
        const Ipp32s shift_1st = SHIFT_INV_1ST;
        const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);
        
        FastInverseDst<Ipp16s, false>(shift_1st, (Ipp16s*)coeff, tmp, 4, bitDepth); // Inverse DST by FAST Algorithm, coeff input, tmp output
        if (inplace)
        {
            FastInverseDst<Ipp16u, true>(shift_2nd, tmp, (Ipp16u*)destPtr, destStride, bitDepth); // Inverse DST by FAST Algorithm, tmp input, coeff output
        }
        else
        {
            FastInverseDst<Ipp16s, false>(shift_2nd, tmp, (Ipp16s*)destPtr, destStride, bitDepth); // Inverse DST by FAST Algorithm, tmp input, coeff output
        }
    }


    void MAKE_NAME(h265_DCT4x4Inv_16sT)(void *destPtr, const short *H265_RESTRICT coeff, int destStride, bool inplace, Ipp32u bitDepth)
    {
        Ipp16s tmp[4*4];    
        const Ipp32s shift_1st = SHIFT_INV_1ST;
        const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);

        PartialButterflyInverse4x4<Ipp16s, false>(shift_1st, (Ipp16s*)coeff, tmp, 4, bitDepth);
        if (inplace)
        {
            PartialButterflyInverse4x4<Ipp16u, true>(shift_2nd, tmp, (Ipp16u*)destPtr, destStride, bitDepth);
        }
        else
        {
            PartialButterflyInverse4x4<Ipp16s, false>(shift_2nd, tmp, (Ipp16s*)destPtr, destStride, bitDepth);
        }
    }


    void MAKE_NAME(h265_DCT8x8Inv_16sT)(void *destPtr, const short *H265_RESTRICT coeff, int destStride, bool inplace, Ipp32u bitDepth)
    {
        Ipp16s tmp[8*8];
        const Ipp32s shift_1st = SHIFT_INV_1ST;
        const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);

        PartialButterflyInverse8x8<Ipp16s, false>(shift_1st, (Ipp16s*)coeff, tmp, 8, bitDepth);
        if (inplace)
        {
            PartialButterflyInverse8x8<Ipp16u, true>(shift_2nd, tmp, (Ipp16u*)destPtr, destStride, bitDepth);
        }
        else
        {
            PartialButterflyInverse8x8<Ipp16s, false>(shift_2nd, tmp, (Ipp16s*)destPtr, destStride, bitDepth);
        }
    }


    void MAKE_NAME(h265_DCT16x16Inv_16sT)(void *destPtr, const short *H265_RESTRICT coeff, int destStride, bool inplace, Ipp32u bitDepth)
    {
        Ipp16s tmp[16*16];
        const Ipp32s shift_1st = SHIFT_INV_1ST;
        const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);

        PartialButterflyInverse16x16<Ipp16s, false>(shift_1st, (Ipp16s*)coeff, tmp, 16, bitDepth);
        if (inplace)
        {
            PartialButterflyInverse16x16<Ipp16u, true>(shift_2nd, tmp, (Ipp16u*)destPtr, destStride, bitDepth);
        }
        else
        {
            PartialButterflyInverse16x16<Ipp16s, false>(shift_2nd, tmp, (Ipp16s*)destPtr, destStride, bitDepth);
        }
    }

    void MAKE_NAME(h265_DCT32x32Inv_16sT)(void *destPtr, const short *H265_RESTRICT coeff, int destStride, bool inplace, Ipp32u bitDepth)
    {
        Ipp16s tmp[32*32];
        const Ipp32s shift_1st = SHIFT_INV_1ST;
        const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);

        PartialButterflyInverse32x32<Ipp16s, false>(shift_1st, (Ipp16s*)coeff, tmp, 32, bitDepth);
        if (inplace)
        {
            PartialButterflyInverse32x32<Ipp16u, true>(shift_2nd, tmp, (Ipp16u*)destPtr, destStride, bitDepth);
        }
        else
        {
            PartialButterflyInverse32x32<Ipp16s, false>(shift_2nd, tmp, (Ipp16s*)destPtr, destStride, bitDepth);
        }
    }

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
