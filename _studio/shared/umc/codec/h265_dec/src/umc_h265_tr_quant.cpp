// Copyright (c) 2012-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#ifdef MFX_ENABLE_H265_VIDEO_DECODE
#ifndef MFX_VA

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
template <int32_t bitDepth, typename DstCoeffsType>
void InverseTransform(CoeffsPtr coeff, DstCoeffsType* dst, size_t dstPitch, int32_t Size, uint32_t Mode, uint32_t bit_depth)
{
    DstCoeffsType* pred = dst; // predicted pels are already in dst buffer
    int32_t predPitch = (int32_t)dstPitch;

    int32_t inplace = (sizeof(DstCoeffsType) == 1) ? 1 : 0;
    if (inplace)
        inplace = bitDepth > 8 ? 2 : 1;

#if !defined(ANDROID)
    if (bit_depth > 10)
    {
        if (Size == 4)
        {
            if (Mode != REG_DCT)
            {
                MFX_HEVC_PP::h265_DST4x4Inv_16sT_px(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
            }
            else
            {
                MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
            }
        }
        else if (Size == 8)
        {
            MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
        else if (Size == 16)
        {
            MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
        else if (Size == 32)
        {
            MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
    }
    else
#endif
    {
        if (Size == 4)
        {
            if (Mode != REG_DCT)
            {
                MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
            }
            else
            {
                MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
            }
        }
        else if (Size == 8)
        {
            MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
        else if (Size == 16)
        {
            MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
        else if (Size == 32)
        {
            MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(pred, predPitch, dst, coeff, (int32_t)dstPitch, inplace, bit_depth);
        }
    }
}

// Process coefficients with transquant bypass flag
template <typename DstCoeffsType>
void H265TrQuant::InvTransformByPass(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, uint32_t Size, uint32_t bitDepth, bool inplace)
{
    int32_t max_value = (1 << bitDepth) - 1;

    for (uint32_t k = 0; k < Size; k++)
    {
        // ML: OPT: TODO: verify vectorization
        for (uint32_t j = 0; j < Size; j++)
        {
            if (inplace)
            {
                pResidual[k * Stride + j] = (DstCoeffsType)mfx::clamp<int32_t>(pResidual[k * Stride + j] + pCoeff[k * Size + j], 0, max_value);
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
void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, uint32_t Mode, DstCoeffsType* pResidual,
                                  size_t Stride, CoeffsPtr pCoeff, uint32_t Size,
                                  bool transformSkip)
{
    int32_t bitDepth = TxtType == TEXT_LUMA ? m_context->m_sps->bit_depth_luma : m_context->m_sps->bit_depth_chroma;

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
                InvTransformByPass<uint16_t>(pCoeff, (uint16_t*)pResidual, Stride, Size, bitDepth, inplace);
            else
                InvTransformByPass(pCoeff, pResidual, Stride, Size, bitDepth, inplace);
            return;
        }

        if (transformSkip)
        {
            if (inplace)
                InvTransformSkip<10, uint16_t>(pCoeff, (uint16_t*)pResidual, Stride, Size, inplace, bitDepth);
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
template void H265TrQuant::InvTransformNxN<uint8_t>(
    bool transQuantBypass, EnumTextType TxtType, uint32_t Mode, uint8_t* pResidual, size_t Stride,
    CoeffsPtr pCoeff,uint32_t Size, bool transformSkip);

template void H265TrQuant::InvTransformNxN<int16_t>(
    bool transQuantBypass, EnumTextType TxtType, uint32_t Mode, int16_t* pResidual, size_t Stride,
    CoeffsPtr pCoeff,uint32_t Size, bool transformSkip);

/* ----------------------------------------------------------------------------*/

// Add residual and prediction needed for chroma reconstruct because transforms
// are done separately while chroma values reside together in NV12
template <typename PixType>
void SumOfResidAndPred(CoeffsPtr p_ResiU, CoeffsPtr p_ResiV, size_t residualPitch, PixType *pRecIPred, size_t RecIPredStride, uint32_t Size,
    bool chromaUPresent, bool chromaVPresent, uint32_t bit_depth)
{
    if (sizeof(PixType) == 1)
        bit_depth = 8;

    for (uint32_t y = 0; y < Size; y++)
    {
        for (uint32_t x = 0; x < Size; x++)
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
void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Size, uint32_t TrMode)
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

    const uint32_t StopTrMode = pCU->GetTrIndex(AbsPartIdx);

    if(TrMode == StopTrMode)
    {
        if (lumaPresent)
        {
            uint32_t DstStride = pCU->m_Frame->pitch_luma();
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
                    SumOfResidAndPred<uint16_t>(residualsTempBuffer, residualsTempBuffer1, res_pitch, (uint16_t*)ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);
                else
                    SumOfResidAndPred<uint8_t>(residualsTempBuffer, residualsTempBuffer1, res_pitch, ptrChroma, DstStride, Size, chromaUPresent, chromaVPresent, m_context->m_sps->bit_depth_chroma);
            }

            if (chromaUPresent1 || chromaVPresent1)
            {
                residualsTempBuffer += bottomChromaOffset;
                residualsTempBuffer1 += bottomChromaOffset;
                ptrChroma += (Size*DstStride) << m_context->m_sps->need16bitOutput;
                if (m_context->m_sps->need16bitOutput)
                    SumOfResidAndPred<uint16_t>(residualsTempBuffer, residualsTempBuffer1, res_pitch, (uint16_t*)ptrChroma, DstStride, Size, chromaUPresent1, chromaVPresent1, m_context->m_sps->bit_depth_chroma);
                else
                    SumOfResidAndPred<uint8_t>(residualsTempBuffer, residualsTempBuffer1, res_pitch, ptrChroma, DstStride, Size, chromaUPresent1, chromaVPresent1, m_context->m_sps->bit_depth_chroma);
            }
        }
    }
    else
    {
        TrMode++;
        Size >>= 1;

        uint32_t Depth = pCU->GetDepth(AbsPartIdx) + TrMode;
        uint32_t PartOffset = pCU->m_NumPartition >> (Depth << 1);

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
void H265TrQuant::InvTransformSkip(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, uint32_t Size, bool inplace, uint32_t bit_depth)
{
    if (bitDepth == 8)
        bit_depth = 8;

    uint32_t Log2TrSize = g_ConvertToBit[Size] + 2;
    int32_t shift = MAX_TR_DYNAMIC_RANGE - bit_depth - Log2TrSize;
    uint32_t transformSkipShift;
    uint32_t j, k;

    if(shift > 0)
    {
        int32_t offset;
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

#endif // #ifndef MFX_VA
#endif // MFX_ENABLE_H265_VIDEO_DECODE
