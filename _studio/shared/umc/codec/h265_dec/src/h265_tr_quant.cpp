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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// H265TrQuant class member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265TrQuant::H265TrQuant()
{
    m_residualsBuffer = (H265CoeffsCommon*)ippMalloc(sizeof(H265CoeffsCommon) * MAX_CU_SIZE * MAX_CU_SIZE * 2);//aligned 64 bytes
    m_residualsBuffer1 = m_residualsBuffer + MAX_CU_SIZE * MAX_CU_SIZE;
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
void InverseTransform(H265CoeffsPtrCommon coeff, DstCoeffsType* dst, Ipp32s dstPitch, Ipp32s Size, Ipp32u Mode)
{
    if (Size == 4)
    {
        if (Mode != REG_DCT)
        {
            MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(dst, coeff, dstPitch, sizeof(DstCoeffsType));
        }
        else
        {
            MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(dst, coeff, dstPitch, sizeof(DstCoeffsType));
        }
    }
    else if (Size == 8)
    {
        MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(dst, coeff, dstPitch, sizeof(DstCoeffsType));
    }
    else if (Size == 16)
    {
        MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(dst, coeff, dstPitch, sizeof(DstCoeffsType));
    }
    else if (Size == 32)
    {
        MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(dst, coeff, dstPitch, sizeof(DstCoeffsType));
    }
}

template <typename DstCoeffsType>
void H265TrQuant::InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual,
                                  Ipp32u Stride, H265CoeffsPtrCommon pCoeff, Ipp32u Size, 
                                  bool transformSkip)
{
    if(transQuantBypass)
    {
        for (Ipp32u k = 0; k < Size; k++)
            // ML: OPT: TODO: verify vectorization
            for (Ipp32u j = 0; j < Size; j++)
            {
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[k * Stride + j] = (DstCoeffsType)Clip3(0, 255, pResidual[k * Stride + j] + pCoeff[k * Size + j]);
                }
                else
                {
                    pResidual[k * Stride + j] = (DstCoeffsType)pCoeff[k * Size + j];
                }
            }

        return;
    }

    Ipp32s bitDepth = TxtType == TEXT_LUMA ? g_bitDepthY : g_bitDepthC;

    if (bitDepth == 8 )
    {
        if (transformSkip)
            InvTransformSkip< 8 >(pCoeff, pResidual, Stride, Size);
        else
        {
            InverseTransform< 8 >(pCoeff, pResidual, Stride, Size, Mode);
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
    H265CoeffsPtrCommon pCoeff,Ipp32u Size, bool transformSkip);
#endif

template void H265TrQuant::InvTransformNxN<Ipp16s>(
    bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, Ipp16s* pResidual, Ipp32u Stride,
    H265CoeffsPtrCommon pCoeff,Ipp32u Size, bool transformSkip);

/* ----------------------------------------------------------------------------*/

void H265TrQuant::InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    bool lumaPresent = pCU->GetCbf(AbsPartIdx, COMPONENT_LUMA, TrMode) != 0;
    bool chromaUPresent = pCU->GetCbf(AbsPartIdx, COMPONENT_CHROMA_U, TrMode) != 0;
    bool chromaVPresent = pCU->GetCbf(AbsPartIdx, COMPONENT_CHROMA_V, TrMode) != 0;

    if (!lumaPresent && !chromaUPresent && !chromaVPresent)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->m_TrIdxArray[AbsPartIdx];

    if(TrMode == StopTrMode)
    {
        H265CoeffsPtrCommon pCoeff;

        Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize * pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1);
        size_t coeffsOffset = NumCoeffInc * AbsPartIdx;

        if (lumaPresent)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);

            pCoeff = pCU->m_TrCoeffY + coeffsOffset;
            InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, REG_DCT, ptrLuma, DstStride, pCoeff, Size,
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
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_U, AbsPartIdx) != 0);
            }

            if (chromaVPresent)
            {
                pCoeff = pCU->m_TrCoeffCr + (coeffsOffset >> 2);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Size,
                    pCU->GetTransformSkip(COMPONENT_CHROMA_V, AbsPartIdx) != 0);
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
void H265TrQuant::InvTransformSkip(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Size)
{
    Ipp32u Log2TrSize = g_ConvertToBit[Size] + 2;
    Ipp32s shift = MAX_TR_DYNAMIC_RANGE - bitDepth - Log2TrSize;
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
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType) ClipY(((pCoeff[j * Size + k] + offset) >> transformSkipShift) + pResidual[j * Stride + k]);
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
                if (sizeof(DstCoeffsType) == 1)
                {
                    pResidual[j * Stride + k] =  (DstCoeffsType)ClipY((pCoeff[j * Size + k] << transformSkipShift) + pResidual[j * Stride + k]);
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
