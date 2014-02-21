/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_tr_quant.h"
#include "mfx_h265_optimization.h"
#include "umc_h265_segment_decoder.h"

namespace UMC_HEVC_DECODER
{

#define RDOQ_CHROMA 1  // use of RDOQ in chroma

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// H265TrQuant class member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265TrQuant::H265TrQuant()
{
    m_residualsBuffer = (H265CoeffsCommon*)ippMalloc(sizeof(H265CoeffsCommon) * MAX_CU_SIZE * MAX_CU_SIZE * 2);//aligned 64 bytes
    m_residualsBuffer1 = m_residualsBuffer + MAX_CU_SIZE * MAX_CU_SIZE;
    m_context = 0;
}

H265TrQuant::~H265TrQuant()
{
    ippFree(m_residualsBuffer);
}


/** MxN inverse transform (2D)
*  \param coeff input data (transform coefficients)
*  \param block output data (residual)
*  \param iWidth input data (width of transform)
*  \param iHeight input data (height of transform)
*/
// ML: OPT: Parameterized to allow const 'shift' propogation
template <Ipp32s bitDepth, typename DstCoeffsType>
void InverseTransform(H265CoeffsPtrCommon coeff, DstCoeffsType* dst, Ipp32s dstPitch, Ipp32s Size, Ipp32u Mode, Ipp32u bit_depth)
{
    bool inplace = sizeof(DstCoeffsType) == 1;
    if (Size == 4)
    {
        if (Mode != REG_DCT)
        {
            MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(dst, coeff, dstPitch, inplace, bit_depth);
        }
        else
        {
            MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(dst, coeff, dstPitch, inplace, bit_depth);
        }
    }
    else if (Size == 8)
    {
        MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(dst, coeff, dstPitch, inplace, bit_depth);
    }
    else if (Size == 16)
    {
        MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(dst, coeff, dstPitch, inplace, bit_depth);
    }
    else if (Size == 32)
    {
        MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(dst, coeff, dstPitch, inplace, bit_depth);
    }
}

template <typename DstCoeffsType>
void H265TrQuant::InvTransformByPass(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Size, Ipp32u bitDepth, bool inplace)
{
    Ipp32s max_value = (1 << bitDepth) - 1;

    for (Ipp32u k = 0; k < Size; k++)
    {
        // ML: OPT: TODO: verify vectorization
        for (Ipp32u j = 0; j < Size; j++)
        {
            if (inplace)
            {
                pResidual[k * Stride + j] = (DstCoeffsType)Clip3(0, max_value, pResidual[k * Stride + j] + pCoeff[k * Size + j]);
            }
            else
            {
                pResidual[k * Stride + j] = (DstCoeffsType)pCoeff[k * Size + j];
            }
        }
    }
}

template <typename DstCoeffsType>
void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual,
                                  Ipp32u Stride, H265CoeffsPtrCommon pCoeff, Ipp32u Size,
                                  bool transformSkip)
{
    Ipp32s bitDepth = TxtType == TEXT_LUMA ? m_context->m_sps->bit_depth_luma : m_context->m_sps->bit_depth_chroma;

    bool inplace = sizeof(DstCoeffsType) == 1;

    if(transQuantBypass)
    {
        if (bitDepth == 8)
            InvTransformByPass(pCoeff, pResidual, Stride, Size, bitDepth, inplace);
        else
        {
            if (inplace)
                InvTransformByPass<Ipp16u>(pCoeff, (Ipp16u*)pResidual, Stride, Size, bitDepth, inplace);
            else
                InvTransformByPass(pCoeff, pResidual, Stride, Size, bitDepth, inplace);
        }
        return;
    }

    if (bitDepth == 8)
    {
        if (transformSkip)
            InvTransformSkip< 8 >(pCoeff, pResidual, Stride, Size, inplace, 8);
        else
        {
            InverseTransform< 8 >(pCoeff, pResidual, Stride, Size, Mode, 8);
        }
    }
    else
    {
        if (transformSkip)
        {
            if (inplace)
                InvTransformSkip<10, Ipp16u>(pCoeff, (Ipp16u*)pResidual, Stride, Size, inplace, bitDepth);
            else
                InvTransformSkip<10>(pCoeff, pResidual, Stride, Size, inplace, bitDepth);
        }
        else
            InverseTransform<10>(pCoeff, pResidual, Stride, Size, Mode, bitDepth);
    }
}

/* ----------------------------------------------------------------------------*/

/* NOTE: InvTransformNxN can be used from other files. The following declarations
 * make sure that functions are defined and linker will find them. Without
 * them Media SDK for Android failed to build.
 */
#if BITS_PER_PLANE == 8
template void H265TrQuant::InvTransformNxN<Ipp8u>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp8u* pResidual, Ipp32u Stride,
    H265CoeffsPtrCommon pCoeff,Ipp32u Size, bool transformSkip);
#endif

template void H265TrQuant::InvTransformNxN<Ipp16s>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp16s* pResidual, Ipp32u Stride,
    H265CoeffsPtrCommon pCoeff,Ipp32u Size, bool transformSkip);

/* ----------------------------------------------------------------------------*/

template <typename PixType>
void SumOfResidAndPred(H265CoeffsPtrCommon p_ResiU, H265CoeffsPtrCommon p_ResiV, Ipp32u residualPitch, PixType *pRecIPred, Ipp32u RecIPredStride, Ipp32u Size,
    bool chromaUPresent, bool chromaVPresent, Ipp32u bit_depth)
{
    if (sizeof(PixType) == 1)
        bit_depth = 8;

    for (Ipp32u y = 0; y < Size; y++)
    {
        for (Ipp32u x = 0; x < Size; x++)
        {
            if (chromaUPresent)
                pRecIPred[2*x] = (PixType)ClipC(pRecIPred[2*x] + p_ResiU[x], bit_depth);
            if (chromaVPresent)
                pRecIPred[2*x+1] = (PixType)ClipC(pRecIPred[2*x + 1] + p_ResiV[x], bit_depth);

        }
        p_ResiU     += residualPitch;
        p_ResiV     += residualPitch;
        pRecIPred += RecIPredStride;
    }
}

void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode)
{
    bool lumaPresent = pCU->GetCbf(COMPONENT_LUMA, AbsPartIdx, TrMode) != 0;
    bool chromaUPresent = pCU->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrMode) != 0;
    bool chromaVPresent = pCU->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrMode) != 0;

    if (!lumaPresent && !chromaUPresent && !chromaVPresent)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->GetTrIndex(AbsPartIdx);

    if(TrMode == StopTrMode)
    {
        H265CoeffsPtrCommon pCoeff;

        Ipp32u NumCoeffInc = pCU->m_SliceHeader->m_SeqParamSet->MinCUSize * pCU->m_SliceHeader->m_SeqParamSet->MinCUSize;
        size_t coeffsOffset = NumCoeffInc * AbsPartIdx;

        if (lumaPresent)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);

            pCoeff = pCU->m_TrCoeffY + coeffsOffset;
            InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_LUMA, REG_DCT, ptrLuma, DstStride, pCoeff, Size,
                pCU->GetTransformSkip(COMPONENT_LUMA, AbsPartIdx) != 0);
        }

        if (chromaUPresent || chromaVPresent)
        {
            // Chroma
            if (Size == 4) {
                if((AbsPartIdx & 0x3) != 0) {
                    return;
                }
            } else {
                Size >>= 1;
            }

            Ipp32u DstStride = pCU->m_Frame->pitch_chroma();
            H265PlanePtrUVCommon ptrChroma = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
            H265CoeffsPtrCommon residualsTempBuffer = m_residualsBuffer;
            H265CoeffsPtrCommon residualsTempBuffer1 = m_residualsBuffer1;
            size_t res_pitch = MAX_CU_SIZE;

            if (chromaUPresent)
            {
                pCoeff = pCU->m_TrCoeffCb + (coeffsOffset >> 2);
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_U, AbsPartIdx) != 0);
            }

            if (chromaVPresent)
            {
                pCoeff = pCU->m_TrCoeffCr + (coeffsOffset >> 2);
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_V, AbsPartIdx) != 0);
            }

            {
            if (m_context->m_sps->bit_depth_chroma > 8 || m_context->m_sps->bit_depth_luma > 8)
                SumOfResidAndPred<Ipp16u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, (Ipp16u*)ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);
            else
                SumOfResidAndPred<Ipp8u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);

            }

            // ML: OPT: TODO: Vectorize this
            /*for (Ipp32u y = 0; y < Size; y++)
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
            }*/
        }
    }
    else
    {
        TrMode++;
        Size >>= 1;

        Ipp32u Depth = pCU->GetDepth(AbsPartIdx) + TrMode;
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
void H265TrQuant::InvTransformSkip(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Size, bool inplace, Ipp32u bit_depth)
{
    if (bitDepth == 8)
        bit_depth = 8;

    Ipp32u Log2TrSize = g_ConvertToBit[Size] + 2;
    Ipp32s shift = MAX_TR_DYNAMIC_RANGE - bit_depth - Log2TrSize;
    Ipp32u transformSkipShift;
    Ipp32u j, k;

    if(shift > 0)
    {
        Ipp32s offset;
        transformSkipShift = shift;
        offset = (1 << (transformSkipShift -1));
        for (j = 0; j < Size; j++)
        {
            // ML: OPT: TODO: verify vectorization
            for (k = 0; k < Size; k ++)
            {
                if (inplace)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType) ClipY(((pCoeff[j * Size + k] + offset) >> transformSkipShift) + pResidual[j * Stride + k], bit_depth);
                }
                else
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)((pCoeff[j * Size + k] + offset) >> transformSkipShift);
                }
            }
        }
    }
    else
    {
        //The case when uiBitDepth >= 13
        transformSkipShift = - shift;
        for (j = 0; j < Size; j++)
        {
            for (k = 0; k < Size; k ++)
            {
                if (inplace)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)ClipY((pCoeff[j * Size + k] << transformSkipShift) + pResidual[j * Stride + k], bit_depth);
                }
                else
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)(pCoeff[j * Size + k] << transformSkipShift);
                }
            }
        }
    }
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
