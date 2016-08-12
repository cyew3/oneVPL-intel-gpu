/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_tr_quant.h"
#include "umc_h265_segment_decoder.h"
#include "mfx_h265_dispatcher.h"

namespace UMC_HEVC_DECODER
{

// Allocate inverse transform and dequantization buffers
H265TrQuant::H265TrQuant()
{
    // it is important to align this memory because intrinsic uses movdqa.
    m_pointerToMemory = h265_new_array_throw<Coeffs>(MAX_CU_SIZE * MAX_CU_SIZE * 2 + 16); // 16 extra bytes for align
    m_residualsBuffer = UMC::align_pointer<CoeffsPtr>(m_pointerToMemory, 16);
    m_residualsBuffer1 = m_residualsBuffer + MAX_CU_SIZE * MAX_CU_SIZE;
    m_context = 0;
}

// Deallocate buffers
H265TrQuant::~H265TrQuant()
{
    delete[] m_pointerToMemory;
}

// Do inverse transform of specified size using optimized functions
// ML: OPT: Parameterized to allow const 'shift' propogation
template <Ipp32s bitDepth, typename DstCoeffsType>
void InverseTransform(CoeffsPtr coeff, DstCoeffsType* dst, size_t dstPitch, Ipp32s Size, Ipp32u Mode, Ipp32u bit_depth)
{
    DstCoeffsType* pred = dst; // predicted pels are already in dst buffer
    Ipp32s predPitch = (Ipp32s)dstPitch;

    Ipp32s inplace = (sizeof(DstCoeffsType) == 1) ? 1 : 0;
    if (inplace)
        inplace = bitDepth > 8 ? 2 : 1;

    if (bit_depth > 10)
    {
        if (Size == 4)
        {
            if (Mode != REG_DCT)
            {
                MFX_HEVC_PP::h265_DST4x4Inv_16sT_px(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
            }
            else
            {
                MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
            }
        }
        else if (Size == 8)
        {
            MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
        else if (Size == 16)
        {
            MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
        else if (Size == 32)
        {
            MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
    }
    else
    {
        if (Size == 4)
        {
            if (Mode != REG_DCT)
            {
                MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
            }
            else
            {
                MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
            }
        }
        else if (Size == 8)
        {
            MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
        else if (Size == 16)
        {
            MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
        else if (Size == 32)
        {
            MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(pred, predPitch, dst, coeff, (Ipp32s)dstPitch, inplace, bit_depth);
        }
    }
}

// Process coefficients with transquant bypass flag
template <typename DstCoeffsType>
void H265TrQuant::InvTransformByPass(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, Ipp32u Size, Ipp32u bitDepth, bool inplace)
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

// Do inverse transform of specified size
template <typename DstCoeffsType>
void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual,
                                  size_t Stride, CoeffsPtr pCoeff, Ipp32u Size,
                                  bool transformSkip)
{
    Ipp32s bitDepth = TxtType == TEXT_LUMA ? m_context->m_sps->bit_depth_luma : m_context->m_sps->bit_depth_chroma;

    bool inplace = sizeof(DstCoeffsType) == 1;
    if (!m_context->m_sps->need16bitOutput)
    {
        if(transQuantBypass)
        {
            InvTransformByPass(pCoeff, pResidual, Stride, Size, bitDepth, inplace);
            return;
        }

        if (transformSkip)
            InvTransformSkip< 8 >(pCoeff, pResidual, Stride, Size, inplace, 8);
        else
        {
            InverseTransform< 8 >(pCoeff, pResidual, Stride, Size, Mode, 8);
        }
    }
    else
    {
        if(transQuantBypass)
        {
            if (inplace)
                InvTransformByPass<Ipp16u>(pCoeff, (Ipp16u*)pResidual, Stride, Size, bitDepth, inplace);
            else
                InvTransformByPass(pCoeff, pResidual, Stride, Size, bitDepth, inplace);
            return;
        }

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
template void H265TrQuant::InvTransformNxN<Ipp8u>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp8u* pResidual, size_t Stride,
    CoeffsPtr pCoeff,Ipp32u Size, bool transformSkip);

template void H265TrQuant::InvTransformNxN<Ipp16s>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp16s* pResidual, size_t Stride,
    CoeffsPtr pCoeff,Ipp32u Size, bool transformSkip);

/* ----------------------------------------------------------------------------*/

// Add residual and prediction needed for chroma reconstruct because transforms
// are done separately while chroma values reside together in NV12
template <typename PixType>
void SumOfResidAndPred(CoeffsPtr p_ResiU, CoeffsPtr p_ResiV, size_t residualPitch, PixType *pRecIPred, size_t RecIPredStride, Ipp32u Size,
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

// Recursively descend to basic transform blocks, inverse transform coefficients in them and/
// add the result to prediction for complete reconstruct
void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode)
{
    bool lumaPresent = pCU->GetCbf(COMPONENT_LUMA, AbsPartIdx, TrMode) != 0;
    bool chromaUPresent = pCU->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrMode) != 0;
    bool chromaVPresent = pCU->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrMode) != 0;

    bool chromaUPresent1 = 0;
    bool chromaVPresent1 = 0;

    if (m_context->m_sps->ChromaArrayType == CHROMA_FORMAT_422)
    {
        chromaUPresent1 = pCU->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx, TrMode) != 0;
        chromaVPresent1 = pCU->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx, TrMode) != 0;
    }

    if (!lumaPresent && !chromaUPresent && !chromaVPresent && !chromaUPresent1 && !chromaVPresent1)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->GetTrIndex(AbsPartIdx);

    if(TrMode == StopTrMode)
    {
        if (lumaPresent)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            PlanePtrY ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);

            CoeffsPtr pCoeff = m_context->m_coeffsRead;
            m_context->m_coeffsRead += Size*Size;

            InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_LUMA, REG_DCT, ptrLuma, DstStride, pCoeff, Size,
                pCU->GetTransformSkip(COMPONENT_LUMA, AbsPartIdx) != 0);
        }

        if (chromaUPresent || chromaVPresent || chromaUPresent1 || chromaVPresent1)
        {
            // Chroma
            if (Size == 4) {
                if((AbsPartIdx & 0x3) != 3) {
                    return;
                }
                AbsPartIdx -= 3;
            } else {
                Size >>= 1;
            }

            size_t DstStride = pCU->m_Frame->pitch_chroma();
            PlanePtrUV ptrChroma = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
            CoeffsPtr residualsTempBuffer = m_residualsBuffer;
            CoeffsPtr residualsTempBuffer1 = m_residualsBuffer1;
            size_t res_pitch = MAX_CU_SIZE;

            if (chromaUPresent)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_U, AbsPartIdx) != 0);
            }

            size_t bottomChromaOffset = (Size*res_pitch) << m_context->m_sps->need16bitOutput;
            if (chromaUPresent1)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT, residualsTempBuffer + bottomChromaOffset, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_U1, AbsPartIdx) != 0);
            }

            if (chromaVPresent)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_V, AbsPartIdx) != 0);
            }

            if (chromaVPresent1)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;
                InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1 + bottomChromaOffset, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_V1, AbsPartIdx) != 0);
            }

            if (chromaUPresent || chromaVPresent)
            {
                if (m_context->m_sps->need16bitOutput)
                    SumOfResidAndPred<Ipp16u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, (Ipp16u*)ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);
                else
                    SumOfResidAndPred<Ipp8u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);
            }

            if (chromaUPresent1 || chromaVPresent1)
            {
                residualsTempBuffer += bottomChromaOffset;
                residualsTempBuffer1 += bottomChromaOffset;
                ptrChroma += (Size*DstStride) << m_context->m_sps->need16bitOutput;
                if (m_context->m_sps->need16bitOutput)
                    SumOfResidAndPred<Ipp16u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, (Ipp16u*)ptrChroma, DstStride, Size, chromaUPresent1, chromaVPresent1, m_context->m_sps->bit_depth_chroma);
                else
                    SumOfResidAndPred<Ipp8u>(residualsTempBuffer, residualsTempBuffer1, res_pitch, ptrChroma, DstStride, Size, chromaUPresent1, chromaVPresent1, m_context->m_sps->bit_depth_chroma);
            }
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

// Process coefficients with transform skip flag
// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template <int bitDepth, typename DstCoeffsType>
void H265TrQuant::InvTransformSkip(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, Ipp32u Size, bool inplace, Ipp32u bit_depth)
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

#endif // UMC_ENABLE_H265_VIDEO_DECODER
