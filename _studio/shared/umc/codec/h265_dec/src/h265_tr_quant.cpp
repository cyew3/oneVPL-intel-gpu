/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_tr_quant.h"
#include "mfx_h265_optimization.h"

namespace UMC_HEVC_DECODER
{

#define RDOQ_CHROMA 1  // use of RDOQ in chroma

QPParam::QPParam()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// H265TrQuant class member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265TrQuant::H265TrQuant()
{
    m_QPParam.Clear();

    // allocate bit estimation class  (for RDOQ)
    m_EstBitsSbac = new EstBitsSbacStruct;

    m_UseScalingList = false;
    initScalingList();

    m_residualsBuffer = new H265CoeffsCommon[MAX_CU_SIZE * MAX_CU_SIZE];
    m_residualsBuffer1 = new H265CoeffsCommon[MAX_CU_SIZE * MAX_CU_SIZE];
    m_tempTransformBuffer = new H265CoeffsCommon[MAX_CU_SIZE * MAX_CU_SIZE];
}

H265TrQuant::~H265TrQuant()
{
    // delete bit estimation class
    if (m_EstBitsSbac)
    {
        delete m_EstBitsSbac;
        m_EstBitsSbac = NULL;
    }
    destroyScalingList();

    delete[] m_residualsBuffer;
    delete[] m_residualsBuffer1;
    delete[] m_tempTransformBuffer;
}

// ML: OPT: Parameterized to allow const 'shift' propogation
template <Ipp32s shift, typename DstCoeffsType>
void FastInverseDst(H265CoeffsPtrCommon tmp, DstCoeffsType* block, Ipp32s dstStride)  // input tmp, output block
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
    if (sizeof(DstCoeffsType) == 1)
    {
        block[dstStride*i+0] = (DstCoeffsType)Clip3(0, 255, (( 29 * c[0] + 55 * c[1]     + c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+0]);
        block[dstStride*i+1] = (DstCoeffsType)Clip3(0, 255, (( 55 * c[2] - 29 * c[1]     + c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+1]);
        block[dstStride*i+2] = (DstCoeffsType)Clip3(0, 255, (( 74 * (tmp[i] - tmp[8+i]  + tmp[12+i]) + rnd_factor ) >> shift ) + block[dstStride*i+2]);
        block[dstStride*i+3] = (DstCoeffsType)Clip3(0, 255, (( 55 * c[0] + 29 * c[2]     - c[3]      + rnd_factor ) >> shift ) + block[dstStride*i+3]);
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
template <Ipp32s shift, typename DstCoeffsType>
void PartialButterflyInverse4x4(H265CoeffsPtrCommon src, DstCoeffsType*dst, Ipp32s dstStride)
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
    if (sizeof(DstCoeffsType) == 1)
    {
        dst[0] = (DstCoeffsType)Clip3(0, 255, ((E[0] + O[0] + add)>>shift ) + dst[0]);
        dst[1] = (DstCoeffsType)Clip3(0, 255, ((E[1] + O[1] + add)>>shift ) + dst[1]);
        dst[2] = (DstCoeffsType)Clip3(0, 255, ((E[1] - O[1] + add)>>shift ) + dst[2]);
        dst[3] = (DstCoeffsType)Clip3(0, 255, ((E[0] - O[0] + add)>>shift ) + dst[3]);
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
template <Ipp32s shift, typename DstCoeffsType>
void PartialButterflyInverse8x8(H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride)
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
            if (sizeof(DstCoeffsType) == 1)
            {
                dst[k]     = (DstCoeffsType) Clip3(0, 255, ((E[k]     + O[k]     + add) >> shift) + dst[k]);
                dst[k + 4] = (DstCoeffsType) Clip3(0, 255, ((E[3 - k] - O[3 - k] + add) >> shift) + dst[k + 4]);
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
template <Ipp32s shift, typename DstCoeffsType>
void PartialButterflyInverse16x16(H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride)
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
            if (sizeof(DstCoeffsType) == 1)
            {
                dst[k]   = (DstCoeffsType) Clip3(0, 255, ((E[k]     + O[k]     + add) >> shift) + dst[k]);
                dst[k+8] = (DstCoeffsType) Clip3(0, 255, ((E[7 - k] - O[7 - k] + add) >> shift) + dst[k+8]);
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
template <Ipp32s shift, typename DstCoeffsType>
void PartialButterflyInverse32x32(H265CoeffsPtrCommon src, DstCoeffsType* dst, Ipp32s dstStride)
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
            if (sizeof(DstCoeffsType) == 1)
            {
                dst[k]      = (DstCoeffsType)Clip3(0, 255, ((E[k]      + O[k]      + add) >> shift) + dst[k]);
                dst[k + 16] = (DstCoeffsType)Clip3(0, 255, ((E[15 - k] - O[15 - k] + add) >> shift) + dst[k + 16]);
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

/** MxN inverse transform (2D)
*  \param coeff input data (transform coefficients)
*  \param block output data (residual)
*  \param iWidth input data (width of transform)
*  \param iHeight input data (height of transform)
*/
#define OPT_IDCT

// ML: OPT: Parameterized to allow const 'shift' propogation
template <Ipp32s bitDepth, typename DstCoeffsType>
void InverseTransform(H265CoeffsPtrCommon coeff, DstCoeffsType* dst, Ipp32s dstPitch, Ipp32s Size, Ipp32u Mode
#if !defined(OPT_IDCT)
                      , H265CoeffsCommon * tempBuffer
#endif
)
{
#if !defined(OPT_IDCT)
    const Ipp32s shift_1st = SHIFT_INV_1ST;
    const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);
    tempBuffer[0] = 0; /* TMP - avoid compiler warning (tempBuffer unusued with optimized IDCT kernels) */
#endif

    if (Size == 4)
    {
        if (Mode != REG_DCT)
        {
#ifdef OPT_IDCT
            MFX_HEVC_COMMON::IDST_4x4_SSE4(dst, coeff, dstPitch, sizeof(DstCoeffsType));
#else
            FastInverseDst<shift_1st, H265CoeffsCommon>(coeff, tempBuffer, 4 ); // Inverse DST by FAST Algorithm, coeff input, tmp output
            FastInverseDst<shift_2nd, DstCoeffsType>(tempBuffer, dst, dstPitch ); // Inverse DST by FAST Algorithm, tmp input, coeff output
#endif
        }
        else
        {
#ifdef OPT_IDCT
            MFX_HEVC_COMMON::IDCT_4x4_SSE4(dst, coeff, dstPitch, sizeof(DstCoeffsType));
#else
            PartialButterflyInverse4x4<shift_1st, H265CoeffsCommon>(coeff, tempBuffer, 4);
            PartialButterflyInverse4x4<shift_2nd, DstCoeffsType>(tempBuffer, dst, dstPitch);
#endif
        }
    }
    else if (Size == 8)
    {
#ifdef OPT_IDCT
        MFX_HEVC_COMMON::IDCT_8x8_SSE4(dst, coeff, dstPitch, sizeof(DstCoeffsType));
#else
        PartialButterflyInverse8x8<shift_1st, H265CoeffsCommon>(coeff, tempBuffer, 8);
        PartialButterflyInverse8x8<shift_2nd, DstCoeffsType>(tempBuffer, dst, dstPitch);
#endif
    }
    else if (Size == 16)
    {
#ifdef OPT_IDCT
        MFX_HEVC_COMMON::IDCT_16x16_SSE4(dst, coeff, dstPitch, sizeof(DstCoeffsType));
#else
        PartialButterflyInverse16x16<shift_1st, H265CoeffsCommon>(coeff, tempBuffer, 16);
        PartialButterflyInverse16x16<shift_2nd, DstCoeffsType>(tempBuffer, dst, dstPitch);
#endif
    }
    else if (Size == 32)
    {
#ifdef OPT_IDCT
        MFX_HEVC_COMMON::IDCT_32x32_SSE4(dst, coeff, dstPitch, sizeof(DstCoeffsType));
#else
        PartialButterflyInverse32x32<shift_1st, H265CoeffsCommon>(coeff, tempBuffer, 32);
        PartialButterflyInverse32x32<shift_2nd, DstCoeffsType>(tempBuffer, dst, dstPitch);
#endif
    }
}

// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template< Ipp32u c_Log2TrSize, Ipp32s bitDepth >
void H265TrQuant::DeQuant_inner(const H265CoeffsPtrCommon pQCoef, Ipp32u Length, Ipp32s scalingListType)
{
    Ipp32u TransformShift = MAX_TR_DYNAMIC_RANGE - bitDepth - c_Log2TrSize;
    Ipp32s Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift;

    if (m_UseScalingList)
    {
        Shift += 4;
        Ipp16s *pDequantCoef = getDequantCoeff(scalingListType, m_QPParam.m_Rem, c_Log2TrSize - 2);

        if (Shift > m_QPParam.m_Per)
        {
            Ipp32s Add = 1 << (Shift - m_QPParam.m_Per - 1);

#ifdef __INTEL_COMPILER
            #pragma ivdep
            #pragma vector always
#endif
            for (Ipp32u n = 0; n < Length; n++)
            {
                // clipped when decoded
                Ipp32s CoeffQ = ((pQCoef[n] * pDequantCoef[n]) + Add) >> (Shift -  m_QPParam.m_Per);
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
            }
        }
        else
        {
#ifdef __INTEL_COMPILER
            #pragma ivdep
            #pragma vector always
#endif
            for (Ipp32u n = 0; n < Length; n++)
            {
                // clipped when decoded
                Ipp32s CoeffQ   = Clip3(-32768, 32767, pQCoef[n] * pDequantCoef[n]); 
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ << ( m_QPParam.m_Per - Shift ) );
            }
        }
    }
    else
    {
        Ipp16s Add = 1 << (Shift - 1);
        Ipp16s scale = g_invQuantScales[m_QPParam.m_Rem] << m_QPParam.m_Per;

        // ML: OPT: verify vectorization
#ifdef __INTEL_COMPILER
        #pragma ivdep
        #pragma vector always
#endif
        for (Ipp32u n = 0; n < Length; n++)
        {
            // clipped when decoded
            Ipp32s CoeffQ = (pQCoef[n] * scale + Add) >> Shift;
            pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
        }
    }
}

template < Ipp32s bitDepth >
void H265TrQuant::DeQuant(H265CoeffsPtrCommon pSrc, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType)
{
    const H265CoeffsPtrCommon pQCoef = pSrc;

    if (Width > m_MaxTrSize)
    {
        Width = m_MaxTrSize;
        Height = m_MaxTrSize;
    }

    Ipp32u Log2TrSize = g_ConvertToBit[Width] + 2;

    Ipp32u Length = Width * Height;
    if (Log2TrSize == 1)
        DeQuant_inner< 1, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 2)
        DeQuant_inner< 2, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 3)
        DeQuant_inner< 3, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 4)
        DeQuant_inner< 4, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 5)
        DeQuant_inner< 5, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 6)
        DeQuant_inner< 6, bitDepth >( pQCoef, Length, scalingListType );
    else if (Log2TrSize == 7)
        DeQuant_inner< 7, bitDepth >( pQCoef, Length, scalingListType );
    else
        VM_ASSERT(0); // should never happen
}

void H265TrQuant::Init(Ipp32u MaxWidth, Ipp32u MaxHeight, Ipp32u MaxTrSize)
{
    MaxWidth;
    MaxHeight;
    m_MaxTrSize = MaxTrSize;
    m_EncFlag = false;
    m_UseRDOQ = false;
}

template <typename DstCoeffsType>
void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual,
                                  Ipp32u Stride, H265CoeffsPtrCommon pCoeff, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType,
                                  bool transformSkip)
{
    VM_ASSERT(scalingListType < 6);
    if(transQuantBypass)
    {
        for (Ipp32u k = 0; k < Height; k++)
            // ML: OPT: TODO: verify vectorization
            for (Ipp32u j = 0; j < Width; j++)
            {
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[k * Stride + j] = (DstCoeffsType)Clip3(0, 255, pResidual[k * Stride + j] + pCoeff[k * Width + j]);
                }
                else
                {
                    pResidual[k * Stride + j] = (DstCoeffsType)pCoeff[k * Width + j];
                }
            }

        return;
    }

    Ipp32s bitDepth = TxtType == TEXT_LUMA ? g_bitDepthY : g_bitDepthC;

// ML: OPT: allows inlining and constant propogation for the most common case
    if (bitDepth == 8 )
        DeQuant< 8 >(pCoeff, Width, Height, scalingListType);
    else
        VM_ASSERT( 0 ); // no 10-bit support yet

    if (bitDepth == 8 )
    {
        if (transformSkip)
            InvTransformSkip< 8 >(pCoeff, pResidual, Stride, Width, Height);
        else
        {
            VM_ASSERT(Width == Height);
            InverseTransform< 8 >(pCoeff, pResidual, Stride, Width, Mode
#if !defined(OPT_IDCT)
                , m_tempTransformBuffer
#endif
            );
        }
    } 
    else
        VM_ASSERT( 0 ); // no 10-bit support yet
}

/* ----------------------------------------------------------------------------*/

/* NOTE: InvTransformNxN can be used from other files. The following declarations
 * make sure that functions are defined and linker will find them. Without
 * them Media SDK for Android failed to build.
 */
#if BITS_PER_PLANE == 8
template void H265TrQuant::InvTransformNxN<Ipp8u>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp8u* pResidual, Ipp32u Stride,
    H265CoeffsPtrCommon pCoeff,Ipp32u Width, Ipp32u Height, Ipp32s scalingListType, bool transformSkip);
#endif

template void H265TrQuant::InvTransformNxN<Ipp16s>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp16s* pResidual, Ipp32u Stride,
    H265CoeffsPtrCommon pCoeff,Ipp32u Width, Ipp32u Height, Ipp32s scalingListType, bool transformSkip);

/* ----------------------------------------------------------------------------*/

void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    bool lumaPresent = pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrMode) != 0;
    bool chromaUPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrMode) != 0;
    bool chromaVPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrMode) != 0;

    if (!lumaPresent && !chromaUPresent && !chromaVPresent)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->m_TrIdxArray[AbsPartIdx];

    if(TrMode == StopTrMode)
    {
        Ipp32s scalingListType;
        H265CoeffsPtrCommon pCoeff;

        Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth * pCU->m_SliceHeader->m_SeqParamSet->MaxCUHeight) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1);
        size_t coeffsOffset = NumCoeffInc * AbsPartIdx;

        if (lumaPresent)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);

            scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_LUMA];
            pCoeff = pCU->m_TrCoeffY + coeffsOffset;
            SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_LUMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetY(), 0);
            InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, REG_DCT, ptrLuma, DstStride, pCoeff, Size, Size,
                scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_LUMA]][AbsPartIdx] != 0);
        }

        if (chromaUPresent || chromaVPresent)
        {
            // Chroma
            Size >>= 1;

            Ipp32u DstStride = pCU->m_Frame->pitch_chroma();
            H265PlanePtrUVCommon ptrChroma = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);

            Ipp32u Depth = pCU->m_DepthArray[AbsPartIdx] + TrMode;
            Ipp32u Log2TrSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->getMaxCUWidth() >> Depth ] + 2;
            if (Log2TrSize == 2)
            {
                Ipp32u QPDiv = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
                if((AbsPartIdx % QPDiv) != 0)
                {
                    return;
                }
                Size <<= 1;
            }

            H265CoeffsPtrCommon residualsTempBuffer = m_residualsBuffer;
            H265CoeffsPtrCommon residualsTempBuffer1 = m_residualsBuffer1;
            size_t res_pitch = MAX_CU_SIZE;

            if (chromaUPresent)
            {
                scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_CHROMA_U];
                pCoeff = pCU->m_TrCoeffCb + (coeffsOffset >> 2);
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCbQpOffset() + pCU->m_SliceHeader->slice_cb_qp_offset;
                SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Size, Size,
                    scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_U]][AbsPartIdx] != 0);
            }

            if (chromaVPresent)
            {
                scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_CHROMA_V];
                pCoeff = pCU->m_TrCoeffCr + (coeffsOffset >> 2);
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCrQpOffset() + pCU->m_SliceHeader->slice_cr_qp_offset;
                SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Size, Size,
                    scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_V]][AbsPartIdx] != 0);
            }
  
            // ML: OPT: TODO: Vectorize this
            for (Ipp32u y = 0; y < Size; y++)
            {
                for (Ipp32u x = 0; x < Size; x++)
                {
                    if (chromaUPresent)
                        ptrChroma[2*x] = (H265PlaneUVCommon)ClipC(residualsTempBuffer[x] + ptrChroma[2*x]);
                    if (chromaVPresent)
                        ptrChroma[2*x+1] = (H265PlaneUVCommon)ClipC(residualsTempBuffer1[x] + ptrChroma[2*x+1]);
                }
                residualsTempBuffer += res_pitch;
                residualsTempBuffer1 += res_pitch;
                ptrChroma += DstStride;
            }
        }
    }
    else
    {
        TrMode++;
        Size >>= 1;

        Ipp32u Depth = pCU->m_DepthArray[AbsPartIdx] + TrMode;
        Ipp32u PartOffset = pCU->m_NumPartition >> (Depth << 1);

        InvRecurTransformNxN(pCU, AbsPartIdx, Size, TrMode);
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, Size, TrMode);
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, Size, TrMode);
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, Size, TrMode);
    }
}

// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template <int bitDepth, typename DstCoeffsType>
void H265TrQuant::InvTransformSkip(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height)
{
    VM_ASSERT(Width == Height);
    Ipp32u Log2TrSize = g_ConvertToBit[Width] + 2;
    Ipp32s shift = MAX_TR_DYNAMIC_RANGE - bitDepth - Log2TrSize;
    Ipp32u transformSkipShift;
    Ipp32u j, k;

    if(shift > 0)
    {
        Ipp32s offset;
        transformSkipShift = shift;
        offset = (1 << (transformSkipShift -1));
        for (j = 0; j < Height; j++)
        {
            // ML: OPT: TODO: verify vectorization
            for (k = 0; k < Width; k ++)
            {
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType) ClipY(((pCoeff[j * Width + k] + offset) >> transformSkipShift) + pResidual[j * Stride + k]);
                }
                else
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)((pCoeff[j * Width + k] + offset) >> transformSkipShift);
                }
            }
        }
    }
    else
    {
        //The case when uiBitDepth >= 13
        transformSkipShift = - shift;
        for (j = 0; j < Height; j++)
        {
            for (k = 0; k < Width; k ++)
            {
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)ClipY((pCoeff[j * Width + k] << transformSkipShift) + pResidual[j * Stride + k]);
                }
                else
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)(pCoeff[j * Width + k] << transformSkipShift);
                }
            }
        }
    }
}



// return 1 if both right neighbour and lower neighour are 1's
bool H265TrQuant::bothCGNeighboursOne(const Ipp32u* SigCoeffGroupFlag,
                                      const Ipp32u  CGPosX,
                                      const Ipp32u  CGPosY,
                                      Ipp32u width, Ipp32u height)
{
    Ipp32u Right = 0;
    Ipp32u Lower = 0;

    width >>= 2;
    height >>= 2;
    if (CGPosX < width - 1)
    {
        Right = (SigCoeffGroupFlag[CGPosY * width + CGPosX + 1] != 0);
    }
    if (CGPosY < height - 1)
    {
        Lower = (SigCoeffGroupFlag[(CGPosY  + 1) * width + CGPosX] != 0);
    }

    if (Right && Lower)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void H265TrQuant::setScalingListDec(H265ScalingList *scalingList)
{
  for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
      for(Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
      {
          for(Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
          {
              Ipp32u width = g_scalingListSizeX[sizeId];
              Ipp32u height = g_scalingListSizeX[sizeId];
              Ipp32u ratio = g_scalingListSizeX[sizeId] / IPP_MIN(MAX_MATRIX_SIZE_NUM, (Ipp32s)g_scalingListSizeX[sizeId]);
              Ipp16s *dequantcoeff;
              Ipp32s *coeff = scalingList->getScalingListAddress(sizeId, listId);

              dequantcoeff = getDequantCoeff(listId, qp, sizeId);
              processScalingListDec(coeff, dequantcoeff, g_invQuantScales[qp], height, width, ratio,
                  IPP_MIN(MAX_MATRIX_SIZE_NUM, (Ipp32s)g_scalingListSizeX[sizeId]), scalingList->getScalingListDC(sizeId, listId));
          }
      }
  }
}

void H265TrQuant::setDefaultScalingList()
{
    H265ScalingList sl;

    for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
        {
            ::memcpy(sl.getScalingListAddress(sizeId, listId),
                sl.getScalingListDefaultAddress(sizeId, listId),
                sizeof(Ipp32s) * IPP_MIN(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]));
            sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
        }
    }

    setScalingListDec(&sl);
}


void H265TrQuant::processScalingListDec(Ipp32s *coeff, Ipp16s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc)
{
    for(Ipp32u j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
        #pragma vector always
#endif
        for(Ipp32u i = 0; i < width; i++)
        {
            dequantcoeff[j * width + i] = (Ipp16s)(invQuantScales * coeff[sizuNum * (j / ratio) + i / ratio]);
        }
    }
    if(ratio > 1)
    {
        dequantcoeff[0] = (Ipp16s)(invQuantScales * dc);
    }
}

/** initialization process of scaling list array
 */
void H265TrQuant::initScalingList()
{
    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        size_t scalingListNum = g_scalingListNum[sizeId];
        size_t scalingListSize = g_scalingListSize[sizeId];

//        Ipp32s* pScalingList = new Ipp32s[scalingListNum * scalingListSize * SCALING_LIST_REM_NUM ];
        Ipp16s* pScalingList = new Ipp16s[scalingListNum * scalingListSize * SCALING_LIST_REM_NUM ];

        for (Ipp32u listId = 0; listId < scalingListNum; listId++)
        {
            for (Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
            {
                m_dequantCoef[sizeId][listId][qp] = pScalingList + (qp * scalingListSize);
            }
            pScalingList += (SCALING_LIST_REM_NUM * scalingListSize);
        }
    }

    //alias list [1] as [3].
    for (Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
    {
        m_dequantCoef[SCALING_LIST_32x32][3][qp] = m_dequantCoef[SCALING_LIST_32x32][1][qp];
    }
}

/** destroy quantization matrix array
 */
void H265TrQuant::destroyScalingList()
{
    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
        delete [] m_dequantCoef[sizeId][0][0];
}
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
