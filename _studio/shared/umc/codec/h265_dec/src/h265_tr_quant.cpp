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

    m_UseScalingList = false;
    initScalingList();

    m_residualsBuffer = (H265CoeffsCommon*)ippMalloc(sizeof(H265CoeffsCommon) * MAX_CU_SIZE * MAX_CU_SIZE * 3);//aligned 64 bytes
    m_residualsBuffer1 = m_residualsBuffer + MAX_CU_SIZE * MAX_CU_SIZE;
    m_tempTransformBuffer = m_residualsBuffer + 2 * MAX_CU_SIZE * MAX_CU_SIZE;

}

H265TrQuant::~H265TrQuant()
{
    destroyScalingList();

    ippFree(m_residualsBuffer);    
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

// ML: OPT: Parameterized to allow const 'bitDepth' propogation
template< Ipp32u c_Log2TrSize, Ipp32s bitDepth >
void H265TrQuant::DeQuant_inner(const H265CoeffsPtrCommon pQCoef, Ipp32u Length, Ipp32s scalingListType)
{
    Ipp32u TransformShift = MAX_TR_DYNAMIC_RANGE - bitDepth - c_Log2TrSize;
    Ipp32s Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift;
    Ipp32s totalShift;

    if (m_UseScalingList)
    {
        Shift += 4;
        Ipp16s *pDequantCoef = getDequantCoeff(scalingListType, m_QPParam.m_Rem, c_Log2TrSize - 2);

        if (Shift > m_QPParam.m_Per)
        {
            Ipp32s Add = 1 << (Shift - m_QPParam.m_Per - 1);
            totalShift = Shift -  m_QPParam.m_Per;

            MFX_HEVC_PP::h265_QuantInv_ScaleList_RShift_16s(pQCoef, pDequantCoef, pQCoef, Length, Add, totalShift);
        }
        else
        {
            totalShift = m_QPParam.m_Per - Shift;

            MFX_HEVC_PP::h265_QuantInv_ScaleList_LShift_16s(pQCoef, pDequantCoef, pQCoef, Length, totalShift);
        }
    }
    else
    {
        Ipp16s Add = 1 << (Shift - 1);
        Ipp16s scale = g_invQuantScales[m_QPParam.m_Rem] << m_QPParam.m_Per;

        MFX_HEVC_PP::h265_QuantInv_16s(pQCoef, pQCoef, Length, scale, Add, Shift);
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

void H265TrQuant::Init(Ipp32u MaxTrSize)
{
    m_MaxTrSize = MaxTrSize;
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

    bool lumaPresent = pCU->GetCbf(AbsPartIdx, TEXT_LUMA, TrMode) != 0;
    bool chromaUPresent = pCU->GetCbf(AbsPartIdx, TEXT_CHROMA_U, TrMode) != 0;
    bool chromaVPresent = pCU->GetCbf(AbsPartIdx, TEXT_CHROMA_V, TrMode) != 0;

    if (!lumaPresent && !chromaUPresent && !chromaVPresent)
    {
        return;
    }

    const Ipp32u StopTrMode = pCU->m_TrIdxArray[AbsPartIdx];

    if(TrMode == StopTrMode)
    {
        Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) ? 0 : 3);
        H265CoeffsPtrCommon pCoeff;

        Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize * pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1);
        size_t coeffsOffset = NumCoeffInc * AbsPartIdx;

        if (lumaPresent)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon ptrLuma = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);

            pCoeff = pCU->m_TrCoeffY + coeffsOffset;
            SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_LUMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetY(), 0);
            InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, REG_DCT, ptrLuma, DstStride, pCoeff, Size, Size,
                scalingListType, pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_LUMA], AbsPartIdx) != 0);
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
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->pps_cb_qp_offset + pCU->m_SliceHeader->slice_cb_qp_offset;
                SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT, residualsTempBuffer, res_pitch, pCoeff, Size, Size,
                    scalingListType+1, pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_CHROMA_U], AbsPartIdx) != 0);
            }

            if (chromaVPresent)
            {
                pCoeff = pCU->m_TrCoeffCr + (coeffsOffset >> 2);
                Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->pps_cr_qp_offset + pCU->m_SliceHeader->slice_cr_qp_offset;
                SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_CHROMA, pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetC(), curChromaQpOffset);
                InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT, residualsTempBuffer1, res_pitch, pCoeff, Size, Size,
                    scalingListType+2, pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_CHROMA_V], AbsPartIdx) != 0);
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
