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
}

void H265TrQuant::SetQPforQuant(Ipp32s QP, EnumTextType TxtType, Ipp32s qpBdOffset, Ipp32s chromaQPOffset)
{
    Ipp32s qpScaled;

    if (TxtType == TEXT_LUMA)
    {
        qpScaled = QP + qpBdOffset;
    }
    else
    {
        qpScaled = Clip3(-qpBdOffset, 57, QP + chromaQPOffset);

        if (qpScaled < 0)
        {
            qpScaled = qpScaled + qpBdOffset;
        }
        else
        {
            qpScaled = g_ChromaScale[qpScaled] + qpBdOffset;
        }
    }
    m_QPParam.SetQPParam(qpScaled);
}

// give identical results
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s shift >
void FastInverseDst(H265CoeffsPtrCommon tmp, H265CoeffsPtrCommon block, Ipp32s dstStride)  // input tmp, output block
#else
void FastInverseDst(H265CoeffsPtrCommon tmp, H265CoeffsPtrCommon block, Ipp32s dstStride, Ipp32s shift)  // input tmp, output block
#endif
{
  Ipp32s i, c[4];
  Ipp32s rnd_factor = 1<<(shift-1);

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: vectorize with constant shift
#pragma unroll
#pragma vector always
//#pragma ivdep
#endif
  for (i = 0; i < 4; i++)
  {
    // Intermediate Variables
    c[0] = tmp[  i] + tmp[ 8+i];
    c[1] = tmp[8+i] + tmp[12+i];
    c[2] = tmp[  i] - tmp[12+i];
    c[3] = 74* tmp[4+i];

// ML: OPT: TODO: make sure compiler vectorizes and optimizes Clip3 as PACKS
    block[dstStride*i+0] = (H265CoeffsCommon)Clip3( -32768, 32767, ( 29 * c[0] + 55 * c[1]     + c[3]      + rnd_factor ) >> shift );
    block[dstStride*i+1] = (H265CoeffsCommon)Clip3( -32768, 32767, ( 55 * c[2] - 29 * c[1]     + c[3]      + rnd_factor ) >> shift );
    block[dstStride*i+2] = (H265CoeffsCommon)Clip3( -32768, 32767, ( 74 * (tmp[i] - tmp[8+i]  + tmp[12+i]) + rnd_factor ) >> shift );
    block[dstStride*i+3] = (H265CoeffsCommon)Clip3( -32768, 32767, ( 55 * c[0] + 29 * c[2]     - c[3]      + rnd_factor ) >> shift );
  }
}

/** 4x4 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s shift >
void PartialButterflyInverse4x4(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride)
#else
void PartialButterflyInverse4x4(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride, Ipp32s shift)
#endif
{
  Ipp32s j;
  Ipp32s E[2],O[2];
  Ipp32s add = 1<<(shift-1);
  const Ipp32s line = 4;

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: vectorize with constant shift
//#pragma ivdep
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
    dst[0] = (H265CoeffsCommon)Clip3( -32768, 32767, (E[0] + O[0] + add)>>shift );
    dst[1] = (H265CoeffsCommon)Clip3( -32768, 32767, (E[1] + O[1] + add)>>shift );
    dst[2] = (H265CoeffsCommon)Clip3( -32768, 32767, (E[1] - O[1] + add)>>shift );
    dst[3] = (H265CoeffsCommon)Clip3( -32768, 32767, (E[0] - O[0] + add)>>shift );

    src++;
    dst += dstStride;
  }
}

/** 8x8 inverse transform implemented using partial butterfly structure (1D)
 *  \param coeff input data (transform coefficients)
 *  \param block output data (residual)
 *  \param shift specifies right shift after 1D transform
 */
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s shift >
void PartialButterflyInverse8x8(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride)
#else
void PartialButterflyInverse8x8(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride, Ipp32s shift)
#endif
{
    Ipp32s j;
    Ipp32s k;
    Ipp32s E[4];
    Ipp32s O[4];
    Ipp32s EE[2];
    Ipp32s EO[2];
    Ipp32s add = 1 << (shift - 1);
    const Ipp32s line = 8;

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: TODO: vectorize manually, ICC does poor auto-vec
//#pragma ivdep
#endif
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
        #pragma unroll(4)
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
            dst[k]     = (H265CoeffsCommon) Clip3( -32768, 32767, (E[k]     + O[k]     + add) >> shift);
            dst[k + 4] = (H265CoeffsCommon) Clip3( -32768, 32767, (E[3 - k] - O[3 - k] + add) >> shift);
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
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s shift >
void PartialButterflyInverse16x16(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride)
#else
void PartialButterflyInverse16x16(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride, Ipp32s shift)
#endif
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

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: TODO: vectorize manually, ICC does not auto-vec
//#pragma ivdep
#endif
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
        #pragma unroll(8)
        for (k = 0; k < 8; k++)
        {
            O[k] = g_T16[ 1][k] * src[line]     + g_T16[ 3][k] * src[ 3 * line] + g_T16[ 5][k] * src[ 5 * line] + g_T16[ 7][k] * src[ 7 * line] +
                   g_T16[ 9][k] * src[9 * line] + g_T16[11][k] * src[11 * line] + g_T16[13][k] * src[13 * line] + g_T16[15][k] * src[15 * line];
        }
        #pragma unroll(4)
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
        #pragma unroll(8)
        for (k = 0; k < 8; k++)
        {
            dst[k]   = (H265CoeffsCommon) Clip3( -32768, 32767, (E[k]     + O[k]     + add) >> shift);
            dst[k+8] = (H265CoeffsCommon) Clip3( -32768, 32767, (E[7 - k] - O[7 - k] + add) >> shift);
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
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s shift >
void PartialButterflyInverse32x32(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride)
#else
void PartialButterflyInverse32x32(H265CoeffsPtrCommon src, H265CoeffsPtrCommon dst, Ipp32s dstStride, Ipp32s shift)
#endif
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

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: TODO: vectorize manually, ICC does not auto-vec
//#pragma ivdep
#endif
    for (j = 0; j < line; j++)
    {
        /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
        #pragma unroll(16)
        for (k = 0; k < 16; k++)
        {
            O[k] = g_T32[ 1][k]*src[line]      + g_T32[ 3][k] * src[ 3 * line] + g_T32[ 5][k] * src[ 5 * line] + g_T32[ 7][k] * src[ 7 * line] +
                   g_T32[ 9][k]*src[ 9 * line] + g_T32[11][k] * src[11 * line] + g_T32[13][k] * src[13 * line] + g_T32[15][k] * src[15 * line] +
                   g_T32[17][k]*src[17 * line] + g_T32[19][k] * src[19 * line] + g_T32[21][k] * src[21 * line] + g_T32[23][k] * src[23 * line] +
                   g_T32[25][k]*src[25 * line] + g_T32[27][k] * src[27 * line] + g_T32[29][k] * src[29 * line] + g_T32[31][k] * src[31 * line];
        }
        #pragma unroll(8)
        for (k = 0; k < 8; k++)
        {
            EO[k] = g_T32[ 2][k] * src[ 2 * line] + g_T32[ 6][k] * src[ 6 * line] + g_T32[10][k] * src[10 * line] + g_T32[14][k] * src[14 * line] +
                    g_T32[18][k] * src[18 * line] + g_T32[22][k] * src[22 * line] + g_T32[26][k] * src[26 * line] + g_T32[30][k] * src[30 * line];
        }
        #pragma unroll(4)
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
        #pragma unroll(4)
        for (k = 0; k < 4; k++)
        {
            EE[k]     = EEE[k]     + EEO[k];
            EE[k + 4] = EEE[3 - k] - EEO[3 - k];
        }
        #pragma unroll(8)
        for (k = 0; k < 8; k++)
        {
            E[k]     = EE[k]     + EO[k];
            E[k + 8] = EE[7 - k] - EO[7 - k];
        }
        #pragma unroll(16)
        for (k = 0; k < 16; k++)
        {
            dst[k]      = (H265CoeffsCommon) Clip3( -32768, 32767, (E[k]      + O[k]      + add) >> shift);
            dst[k + 16] = (H265CoeffsCommon) Clip3( -32768, 32767, (E[15 - k] - O[15 - k] + add) >> shift);
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
#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'shift' propogation
template < Ipp32s bitDepth >
void InverseTransformMxN(H265CoeffsPtrCommon coeff, H265CoeffsPtrCommon block, Ipp32s dstStride, Ipp32s Size, Ipp32u Mode)
{
    const Ipp32s shift_1st = SHIFT_INV_1ST;
    const Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);
    H265CoeffsCommon tmp[64 * 64];

    if (Size == 4)
    {
        if (Mode != REG_DCT)
        {
            FastInverseDst< shift_1st >(coeff, tmp, 4 ); // Inverse DST by FAST Algorithm, coeff input, tmp output
            FastInverseDst< shift_2nd >(tmp, block, dstStride ); // Inverse DST by FAST Algorithm, tmp input, coeff output
        }
        else
        {
            PartialButterflyInverse4x4< shift_1st >(coeff, tmp, 4);
            PartialButterflyInverse4x4< shift_2nd >(tmp, block, dstStride);
        }
    }
    else if (Size == 8)
    {
        PartialButterflyInverse8x8< shift_1st >(coeff, tmp, 8);
        PartialButterflyInverse8x8< shift_2nd >(tmp, block, dstStride);
    }
    else if (Size == 16)
    {
        PartialButterflyInverse16x16< shift_1st >(coeff, tmp, 16);
        PartialButterflyInverse16x16< shift_2nd >(tmp, block, dstStride);
    }
    else if (Size == 32)
    {
        PartialButterflyInverse32x32< shift_1st >(coeff, tmp, 32);
        PartialButterflyInverse32x32< shift_2nd >(tmp, block, dstStride);
    }
}

#else // HEVC_OPT_CHANGES

void InverseTransformMxN(Ipp32s bitDepth, H265CoeffsPtrCommon coeff, H265CoeffsPtrCommon block, Ipp32s dstStride, Ipp32s Size, Ipp32u Mode)
{
    Ipp32s shift_1st = SHIFT_INV_1ST;
    Ipp32s shift_2nd = SHIFT_INV_2ND - (bitDepth - 8);
    H265CoeffsCommon tmp[64 * 64];

    if (Size == 4)
    {
        if (Mode != REG_DCT)
        {
            FastInverseDst(coeff, tmp, 4, shift_1st);    // Inverse DST by FAST Algorithm, coeff input, tmp output
            FastInverseDst(tmp, block, dstStride, shift_2nd); // Inverse DST by FAST Algorithm, tmp input, coeff output
        }
        else
        {
            PartialButterflyInverse4x4(coeff, tmp, 4, shift_1st);
            PartialButterflyInverse4x4(tmp, block, dstStride, shift_2nd);
        }
    }
    else if (Size == 8)
    {
        PartialButterflyInverse8x8(coeff, tmp, 8, shift_1st);
        PartialButterflyInverse8x8(tmp, block, dstStride, shift_2nd);
    }
    else if (Size == 16)
    {
        PartialButterflyInverse16x16(coeff, tmp, 16, shift_1st);
        PartialButterflyInverse16x16(tmp, block, dstStride, shift_2nd);
    }
    else if (Size == 32)
    {
        PartialButterflyInverse32x32(coeff, tmp, 32, shift_1st);
        PartialButterflyInverse32x32(tmp, block, dstStride, shift_2nd);
    }
}
#endif // HEVC_OPT_CHANGES

#if (HEVC_OPT_CHANGES & 512)
// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template< Ipp32s c_Log2TrSize, Ipp32s bitDepth >
void H265TrQuant::DeQuant_inner(const H265CoeffsPtrCommon pQCoef, Ipp32u Length, Ipp32s scalingListType)
{
    Ipp32s Shift;
    Ipp32s Add;
    Ipp32s CoeffQ;

    Ipp32u TransformShift = MAX_TR_DYNAMIC_RANGE - bitDepth - c_Log2TrSize;
    Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift;
    Ipp32s clipQCoef;

    if (m_UseScalingList)
    {
        Shift += 4;
        Ipp32s *pDequantCoef = getDequantCoeff(scalingListType, m_QPParam.m_Rem, c_Log2TrSize - 2);

        if (Shift > m_QPParam.m_Per)
        {
            Add = 1 << (Shift - m_QPParam.m_Per - 1);

            #pragma ivdep
            #pragma vector always
            for (Ipp32u n = 0; n < Length; n++)
            {
                // clipped when decoded
                CoeffQ = ((pQCoef[n] * pDequantCoef[n]) + Add) >> (Shift -  m_QPParam.m_Per);
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
            }
        }
        else
        {
            #pragma ivdep
            #pragma vector always
            for (Ipp32u n = 0; n < Length; n++)
            {
                // clipped when decoded
                CoeffQ   = Clip3(-32768, 32767, pQCoef[n] * pDequantCoef[n]); 
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ << ( m_QPParam.m_Per - Shift ) );
            }
        }
    }
    else
    {
        Add = 1 << (Shift - 1);
        Ipp32s scale = g_invQuantScales[m_QPParam.m_Rem] << m_QPParam.m_Per;

        #pragma ivdep
        #pragma vector always
        for (Ipp32u n = 0; n < Length; n++)
        {
            // clipped when decoded
            CoeffQ = (pQCoef[n] * scale + Add) >> Shift;
            pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
        }
    }
}
#endif


#if (HEVC_OPT_CHANGES & 512)
// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template < Ipp32s bitDepth >
void H265TrQuant::DeQuant(H265CoeffsPtrCommon pSrc, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType)
#else
void H265TrQuant::DeQuant(Ipp32s bitDepth, H265CoeffsPtrCommon pSrc, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType)
#endif
{
    const H265CoeffsPtrCommon pQCoef = pSrc;

    if (Width > m_MaxTrSize)
    {
        Width = m_MaxTrSize;
        Height = m_MaxTrSize;
    }

    Ipp32s Shift;
    Ipp32s Add;
    Ipp32s CoeffQ;
    Ipp32u Log2TrSize = g_ConvertToBit[Width] + 2;

#if (HEVC_OPT_CHANGES & 512)
// ML: OPT: g_ConvertToBit[Width] == -1 for most sizes, making special test check for Log2TrSize == '1' const to allow getting const 'Shift'
//          transforming the rest of the function into inlined function so that const shift propogated 
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
#else
    Ipp32u TransformShift = MAX_TR_DYNAMIC_RANGE - bitDepth - Log2TrSize;
    Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift;

    Ipp32s clipQCoef;

    if (m_UseScalingList)
    {
        Shift += 4;
        Ipp32s *pDequantCoef = getDequantCoeff(scalingListType, m_QPParam.m_Rem, Log2TrSize - 2);

        if (Shift > m_QPParam.m_Per)
        {
            Add = 1 << (Shift - m_QPParam.m_Per - 1);

            for (Ipp32u n = 0; n < Width * Height; n++)
            {
                CoeffQ = ((pQCoef[n] * pDequantCoef[n]) + Add) >> (Shift -  m_QPParam.m_Per);
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
            }
        }
        else
        {
            for (Ipp32u n = 0; n < Width * Height; n++)
            {
                CoeffQ   = Clip3(-32768, 32767, pQCoef[n] * pDequantCoef[n]); // Clip to avoid possible overflow in following shift left operation
                pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ << ( m_QPParam.m_Per - Shift ) );
            }
        }
    }
    else
    {
        Add = 1 << (Shift - 1);
        Ipp32s scale = g_invQuantScales[m_QPParam.m_Rem] << m_QPParam.m_Per;

        for (Ipp32u n = 0; n < Width * Height; n++)
        {
            CoeffQ = (pQCoef[n] * scale + Add) >> Shift;
            pQCoef[n] = (H265CoeffsCommon)Clip3(-32768, 32767, CoeffQ);
        }
    }
#endif // HEVC_OPT_CHANGES
}

void H265TrQuant::Init(Ipp32u MaxWidth, Ipp32u MaxHeight, Ipp32u MaxTrSize, Ipp32s SymbolMode, Ipp32u *Table4, Ipp32u *Table8, Ipp32u *TableLastPosVlcIndex,
                       bool UseRDOQ, bool EncFlag)
{
    assert(sizeof(MaxWidth) != 0 || sizeof(MaxHeight) != 0 || sizeof(TableLastPosVlcIndex) || sizeof(Table4) || sizeof(Table8) || sizeof(SymbolMode)); //wtf unreferenced parameter
    m_MaxTrSize = MaxTrSize;
    m_EncFlag = EncFlag;
    m_UseRDOQ = UseRDOQ;
}


void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, H265CoeffsPtrCommon pResidual,
                                  Ipp32u Stride, H265CoeffsPtrCommon pCoeff, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType,
                                  bool transformSkip)
{
    VM_ASSERT(scalingListType < 6);
    if(transQuantBypass)
    {
        for (Ipp32u k = 0; k < Height; k++)
            for (Ipp32u j = 0; j < Width; j++)
                pResidual[k * Stride + j] = (H265CoeffsCommon)pCoeff[k * Width + j];

        return;
    }

    Ipp32s bitDepth = TxtType == TEXT_LUMA ? g_bitDepthY : g_bitDepthC;

#if (HEVC_OPT_CHANGES & 512)
// ML: OPT: allows inlining and constant propogation for the most common case
    if (bitDepth == 8 )
        DeQuant< 8 >(pCoeff, Width, Height, scalingListType);
    else
        VM_ASSERT( 0 ); // no 10-bit support yet
#else
    DeQuant(bitDepth, pCoeff, Width, Height, scalingListType);
#endif 

#if (HEVC_OPT_CHANGES & 128)
    if (bitDepth == 8 )
    {
        if (transformSkip)
            InvTransformSkip< 8 >(pCoeff, pResidual, Stride, Width, Height);
        else
        {
            VM_ASSERT(Width == Height);
            InverseTransformMxN< 8 >(pCoeff, pResidual, Stride, Width, Mode);
        }
    } 
    else
        VM_ASSERT( 0 ); // no 10-bit support yet
#else
    if (transformSkip)
        InvTransformSkip(bitDepth, pCoeff, pResidual, Stride, Width, Height);
    else
    {
        VM_ASSERT(Width == Height);
        InverseTransformMxN(bitDepth, pCoeff, pResidual, Stride, Width, Mode);
    }
#endif
}

void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u curX, Ipp32u curY,
                                       Ipp32u Width, Ipp32u Height, Ipp32u TrMode, size_t coeffsOffset)
{
    bool lumaPresent = pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrMode);
    bool chromaUPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrMode);
    bool chromaVPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrMode);

    if (!lumaPresent && !chromaUPresent && !chromaVPresent)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->m_TrIdxArray[AbsPartIdx];

    if(TrMode == StopTrMode)
    {
        H265CoeffsPtrCommon residualsTempBuffer = m_residualsBuffer;
        H265CoeffsPtrCommon residualsTempBuffer1 = m_residualsBuffer1;
        size_t res_pitch = 64;

        Ipp32s scalingListType;
        H265CoeffsPtrCommon pCoeff;

        if (lumaPresent)
        {
            scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_LUMA];
            pCoeff = pCU->m_TrCoeffY + coeffsOffset;
            SetQPforQuant(pCU->m_QPArray[0], TEXT_LUMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetY(), 0);
            InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Width, Height,
                scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_LUMA]][AbsPartIdx]);

            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + curX*Width + curY*Height * DstStride;
            
            for (Ipp32u y = 0; y < Height; y++)
            {
                for (Ipp32u x = 0; x < Width; x++)
                {
                    ptrLuma[x] = (H265PlaneYCommon)ClipY(residualsTempBuffer[x] + ptrLuma[x]);
                }
                residualsTempBuffer += res_pitch;
                ptrLuma += DstStride;
            }
        }

        if (chromaUPresent || chromaVPresent)
        {
            // Chroma
            Width >>= 1;
            Height >>= 1;

            Ipp32u DstStride = pCU->m_Frame->pitch_chroma();
            H265PlanePtrUVCommon ptrChroma = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + curX*Width*2 + curY*Height * DstStride;

            Ipp32u Depth = pCU->m_DepthArray[AbsPartIdx] + TrMode;
            Ipp32u Log2TrSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->getMaxCUWidth() >> Depth ] + 2;
            if (Log2TrSize == 2)
            {
                Ipp32u QPDiv = pCU->m_Frame->getNumPartInCU() >> ((Depth - 1) << 1);
                if((AbsPartIdx % QPDiv) != 0)
                {
                    return;
                }
                Width <<= 1;
                Height <<= 1;
            }

            residualsTempBuffer = m_residualsBuffer;

            if (chromaUPresent)
            {
                scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_CHROMA_U];
                pCoeff = pCU->m_TrCoeffCb + (coeffsOffset >> 2);
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCbQpOffset() + pCU->m_SliceHeader->m_SliceQpDeltaCb;
                SetQPforQuant(pCU->m_QPArray[0], TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Width, Height,
                    scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_U]][AbsPartIdx]);
            }

            if (chromaVPresent)
            {
                scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_CHROMA_V];
                pCoeff = pCU->m_TrCoeffCr + (coeffsOffset >> 2);
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCrQpOffset() + pCU->m_SliceHeader->m_SliceQpDeltaCr;
                SetQPforQuant(pCU->m_QPArray[0], TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Width, Height,
                    scalingListType, pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_V]][AbsPartIdx]);
            }
  
            for (Ipp32u y = 0; y < Height; y++)
            {
                for (Ipp32u x = 0; x < Width; x++)
                {
                    if (chromaUPresent)
                        ptrChroma[2*x] = (H265PlaneUVCommon)ClipY(residualsTempBuffer[x] + ptrChroma[2*x]);
                    if (chromaVPresent)
                        ptrChroma[2*x+1] = (H265PlaneUVCommon)ClipY(residualsTempBuffer1[x] + ptrChroma[2*x+1]);
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
        Width >>= 1;
        Height >>= 1;

        curX <<= 1;
        curY <<= 1;

        Ipp32u PartOffset = pCU->m_NumPartition >> (TrMode << 1);

        InvRecurTransformNxN(pCU, AbsPartIdx, curX, curY,    Width, Height, TrMode, coeffsOffset);
        coeffsOffset += Width*Height;
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, curX + 1, curY, Width, Height, TrMode, coeffsOffset);
        coeffsOffset += Width*Height;
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, curX, curY + 1, Width, Height, TrMode, coeffsOffset);
        coeffsOffset += Width*Height;
        AbsPartIdx += PartOffset;
        InvRecurTransformNxN(pCU, AbsPartIdx, curX + 1, curY + 1, Width, Height, TrMode, coeffsOffset);
    }
}

#if (HEVC_OPT_CHANGES & 128)
// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template <int bitDepth>
void H265TrQuant::InvTransformSkip(H265CoeffsPtrCommon pCoeff, H265CoeffsPtrCommon pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height)
#else
void H265TrQuant::InvTransformSkip(Ipp32s bitDepth, H265CoeffsPtrCommon pCoeff, H265CoeffsPtrCommon pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height)
#endif
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
            for (k = 0; k < Width; k ++)
            {
                pResidual[j * Stride + k] =  (H265CoeffsCommon)((pCoeff[j * Width + k] + offset) >> transformSkipShift);
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
                pResidual[j * Stride + k] =  (H265CoeffsCommon)(pCoeff[j * Width + k] << transformSkipShift);
            }
        }
    }
}

/** Context derivation process of coeff_abs_significant_flag
 * \param uiSigCoeffGroupFlag significance map of L1
 * \param uiBlkX column of current scan position
 * \param uiBlkY row of current scan position
 * \param uiLog2BlkSize log2 value of block size
 * \returns ctxInc for current scan position
 */
Ipp32u H265TrQuant::getSigCoeffGroupCtxInc(const Ipp32u* SigCoeffGroupFlag,
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
        Lower = (SigCoeffGroupFlag[(CGPosY  + 1 ) * width + CGPosX] != 0);
    }

    return (Right || Lower);
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

/** Pattern decision for context derivation process of significant_coeff_flag
 * \param sigCoeffGroupFlag pointer to prior coded significant coeff group
 * \param posXCG column of current coefficient group
 * \param posYCG row of current coefficient group
 * \param width width of the block
 * \param height height of the block
 * \returns pattern for current coefficient group
 */
Ipp32s H265TrQuant::calcPatternSigCtx(const Ipp32u* sigCoeffGroupFlag, Ipp32u posXCG, Ipp32u posYCG, Ipp32s width, Ipp32s height)
{
    if( width == 4 && height == 4 )
        return -1;

    Ipp32u sigRight = 0;
    Ipp32u sigLower = 0;

    width >>= 2;
    height >>= 2;
    if(posXCG < (Ipp32u)width - 1)
    {
        sigRight = (sigCoeffGroupFlag[ posYCG * width + posXCG + 1 ] != 0);
    }
    if (posYCG < (Ipp32u)height - 1)
    {
        sigLower = (sigCoeffGroupFlag[ (posYCG  + 1 ) * width + posXCG ] != 0);
    }

    return sigRight + (sigLower<<1);
}

/** Context derivation process of coeff_abs_significant_flag
 * \param pcCoeff pointer to prior coded transform coefficients
 * \param uiPosX column of current scan position
 * \param uiPosY row of current scan position
 * \param uiLog2BlkSize log2 value of block size
 * \param uiStride stride of the block
 * \returns ctxInc for current scan position
 */
Ipp32u H265TrQuant::getSigCtxInc(Ipp32s patternSigCtx,
                                 Ipp32u scanIdx,
                                 const Ipp32u PosX,
                                 const Ipp32u PosY,
                                 const Ipp32u log2BlockSize,
                                 EnumTextType Type)
{
    //LUMA map
    const Ipp32u ctxIndMap[16] =
    {
        0, 1, 4, 5,
        2, 3, 4, 5,
        6, 6, 8, 8,
        7, 7, 8, 8
    };

    if (PosX + PosY == 0)
        return 0;

    if (log2BlockSize == 2)
    {
        return ctxIndMap[4 * PosY + PosX];
    }

    Ipp32u Offset = log2BlockSize == 3 ? (scanIdx == SCAN_DIAG ? 9 : 15) : (Type == TEXT_LUMA ? 21 : 12);

    Ipp32s posXinSubset = PosX - ((PosX >> 2) << 2);
    Ipp32s posYinSubset = PosY - ((PosY >> 2) << 2);
    Ipp32s cnt = 0;
    if(patternSigCtx == 0)
    {
        cnt = posXinSubset + posYinSubset <= 2 ? (posXinSubset + posYinSubset == 0 ? 2 : 1) : 0;
    }
    else if(patternSigCtx == 1)
    {
        cnt = posYinSubset <= 1 ? (posYinSubset == 0 ? 2 : 1) : 0;
    }
    else if(patternSigCtx == 2)
    {
        cnt = posXinSubset <= 1 ? (posXinSubset == 0 ? 2 : 1) : 0;
    }
    else
    {
        cnt = 2;
    }

    return ((Type == TEXT_LUMA && ((PosX >> 2) + (PosY >> 2)) > 0 ) ? 3 : 0) + Offset + cnt;
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
              Ipp32s *dequantcoeff;
              Ipp32s *coeff = scalingList->getScalingListAddress(sizeId, listId);

              dequantcoeff = getDequantCoeff(listId, qp, sizeId);
              processScalingListDec(coeff, dequantcoeff, g_invQuantScales[qp], height, width, ratio,
                  IPP_MIN(MAX_MATRIX_SIZE_NUM, (Ipp32s)g_scalingListSizeX[sizeId]), scalingList->getScalingListDC(sizeId, listId));
          }
      }
  }
}

void H265TrQuant::setDefaultScalingList(bool use_ts)
{
    H265ScalingList sl;

    for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
        {
            ::memcpy(sl.getScalingListAddress(sizeId, listId),
                sl.getScalingListDefaultAddress(sizeId, listId, use_ts),
                sizeof(Ipp32s) * IPP_MIN(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]));
            sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
        }
    }

    setScalingListDec(&sl);
}


void H265TrQuant::processScalingListDec(Ipp32s *coeff, Ipp32s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc)
{
    for(Ipp32u j = 0; j < height; j++)
    {
        #pragma vector always
        for(Ipp32u i = 0; i < width; i++)
        {
            dequantcoeff[j * width + i] = invQuantScales * coeff[sizuNum * (j / ratio) + i / ratio];
        }
    }
    if(ratio > 1)
    {
        dequantcoeff[0] = invQuantScales * dc;
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

        Ipp32s* pScalingList = new Ipp32s[scalingListNum * scalingListSize * SCALING_LIST_REM_NUM ];

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
