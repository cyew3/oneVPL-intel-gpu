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

#include "umc_h265_frame_info.h"
#include "h265_tr_quant.h"

namespace UMC_HEVC_DECODER
{

void H265SegmentDecoder::InitNeighbourPatternChroma(H265CodingUnit* pCU, Ipp32u ZorderIdxInPart, Ipp32u PartDepth, H265PlanePtrUVCommon pAdiBuf,
                                                    Ipp32u OrgBufStride, Ipp32u OrgBufHeight, bool *neighborAvailable, Ipp32s numIntraNeighbors)
{
    H265PlanePtrUVCommon pRoiOrigin;
    H265PlanePtrUVCommon pAdiTemp;
    Ipp32u CUSize = pCU->GetWidth(ZorderIdxInPart) >> PartDepth;
    Ipp32u Size;

    Ipp32s UnitSize = (m_pSeqParamSet->MaxCUSize >> m_pSeqParamSet->MaxCUDepth) >> 1; // for chroma
    Ipp32s NumUnitsInCU = (CUSize / UnitSize) >> 1;            // for chroma
    Ipp32s TotalUnits = (NumUnitsInCU << 2) + 1;

    CUSize >>= 1;  // for chroma
    Size = CUSize * 2 + 1;

    if ((4 * Size > OrgBufStride) || (4 * Size > OrgBufHeight))
    {
        return;
    }

    Ipp32s PicStride = pCU->m_Frame->pitch_chroma();

    // get CbCb pattern
    pRoiOrigin = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, ZorderIdxInPart);
    pAdiTemp = pAdiBuf;

    FillReferenceSamplesChroma(g_bitDepthC, pRoiOrigin, pAdiTemp, neighborAvailable, numIntraNeighbors, UnitSize, NumUnitsInCU, TotalUnits, CUSize, Size, PicStride);
}

void H265SegmentDecoder::FillReferenceSamplesChroma(Ipp32s bitDepth,
                                                    H265PlanePtrUVCommon pRoiOrigin,
                                                    H265PlanePtrUVCommon pAdiTemp,
                                                    bool* NeighborFlags,
                                                    Ipp32s NumIntraNeighbor,
                                                    Ipp32s UnitSize,
                                                    Ipp32s NumUnitsInCU,
                                                    Ipp32s TotalUnits,
                                                    Ipp32u CUSize,
                                                    Ipp32u Size,
                                                    Ipp32s PicStride)
{
    H265PlanePtrUVCommon pRoiTemp;
    Ipp32s i, j;
    H265PlaneUVCommon DCValue = 1 << (bitDepth - 1);
    Ipp32s Width = Size << 1;
    Ipp32s Height = Size;
    Ipp32s CUWidth = CUSize << 1;
    Ipp32s CUHeight = CUSize;

    if (NumIntraNeighbor == 0)
    {
        // Fill border with DC value
        for (i = 0; i < Width; i++)
        {
            pAdiTemp[i] = DCValue;
        }
        for (i = 1; i < Height; i++)
        {
            pAdiTemp[i * Width] = DCValue;
            pAdiTemp[i * Width + 1] = DCValue;
        }
    }
    else if (NumIntraNeighbor == TotalUnits)
    {
        // Fill top-left border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride - 2;
        pAdiTemp[0] = pRoiTemp[0];
        pAdiTemp[1] = pRoiTemp[1];

        // Fill left border with rec. samples
        pRoiTemp = pRoiOrigin - 2;

        for (i = 0; i < CUHeight; i++)
        {
            pAdiTemp[(1 + i) * Width] = pRoiTemp[0];
            pAdiTemp[(1 + i) * Width + 1] = pRoiTemp[1];
            pRoiTemp += PicStride;
        }

        // Fill below left border with rec. samples
        for (i = 0; i < CUHeight; i++)
        {
            pAdiTemp[(1 + CUHeight + i) * Width] = pRoiTemp[0];
            pAdiTemp[(1 + CUHeight + i) * Width + 1] = pRoiTemp[1];
            pRoiTemp += PicStride;
        }

        // Fill top border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride;
        for (i = 0; i < CUWidth; i++)
        {
            pAdiTemp[2 + i] = pRoiTemp[i];
        }

        // Fill top right border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride + CUWidth;
        for (i = 0; i < CUWidth; i++)
        {
            pAdiTemp[2 + CUWidth + i] = pRoiTemp[i];
        }
    }
    else // reference samples are partially available
    {
        Ipp32s NumUnits2 = NumUnitsInCU << 1;
        Ipp32s TotalSamples = TotalUnits * UnitSize;
        H265PlaneUVCommon pAdiLine[5 * MAX_CU_SIZE];
        H265PlanePtrUVCommon pAdiLineTemp;
        bool *pNeighborFlags;
        Ipp32s Prev;
        Ipp32s Next;
        Ipp32s Curr;
        H265PlaneUVCommon pRef1 = 0, pRef2 = 0;

        // Initialize
        for (i = 0; i < TotalSamples * 2; i++)
        {
            pAdiLine[i] = DCValue;
        }

        // Fill top-left sample
        pRoiTemp = pRoiOrigin - PicStride - 2;
        pAdiLineTemp = pAdiLine + (NumUnits2 * UnitSize) * 2;
        pNeighborFlags = NeighborFlags + NumUnits2;
        if (*pNeighborFlags)
        {
            pAdiLineTemp[0] = pRoiTemp[0];
            pAdiLineTemp[1] = pRoiTemp[1];
            for (i = 2; i < UnitSize * 2; i += 2)
            {
                pAdiLineTemp[i] = pAdiLineTemp[0];
                pAdiLineTemp[i + 1] = pAdiLineTemp[1];
            }
        }

        // Fill left & below-left samples
        pRoiTemp += PicStride;

        pAdiLineTemp -= 2;
        pNeighborFlags--;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize; i++)
                {
                    pAdiLineTemp[-(Ipp32s)i * 2] = pRoiTemp[i * PicStride];
                    pAdiLineTemp[-(Ipp32s)i * 2 + 1] = pRoiTemp[i * PicStride + 1];
                }
            }
            pRoiTemp += UnitSize * PicStride;
            pAdiLineTemp -= UnitSize * 2;
            pNeighborFlags--;
        }

        // Fill above & above-right samples
        pRoiTemp = pRoiOrigin - PicStride;
        pAdiLineTemp = pAdiLine + ((NumUnits2 + 1) * UnitSize) * 2;
        pNeighborFlags = NeighborFlags + NumUnits2 + 1;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize * 2; i++)
                {
                    pAdiLineTemp[i] = pRoiTemp[i];
                }
            }
            pRoiTemp += UnitSize * 2;
            pAdiLineTemp += UnitSize * 2;
            pNeighborFlags++;
        }
        // Pad reference samples when necessary
        Prev = -1;
        Curr = 0;
        Next = 1;
        pAdiLineTemp = pAdiLine;
        while (Curr < TotalUnits)
        {
            if (!NeighborFlags[Curr])
            {
                if (Curr == 0)
                {
                    while (Next < TotalUnits && !NeighborFlags[Next])
                    {
                        Next++;
                    }
                    pRef1 = pAdiLine[Next * UnitSize * 2];
                    pRef2 = pAdiLine[Next * UnitSize * 2 + 1];
                    // Pad unavailable samples with new value
                    while (Curr < Next)
                    {
                        for (i = 0; i < UnitSize; i++)
                        {
                            pAdiLineTemp[i * 2] = pRef1;
                            pAdiLineTemp[i * 2 + 1] = pRef2;
                        }
                        pAdiLineTemp += UnitSize * 2;
                        Curr++;
                    }
                }
                else
                {
                    pRef1 = pAdiLine[(Curr * UnitSize - 1) * 2];
                    pRef2 = pAdiLine[(Curr * UnitSize - 1) * 2 + 1];
                    for (i = 0; i < UnitSize; i++)
                    {
                        pAdiLineTemp[i * 2] = pRef1;
                        pAdiLineTemp[i * 2 + 1] = pRef2;
                    }
                    pAdiLineTemp += UnitSize * 2;
                    Curr++;
                }
            }
            else
            {
                pAdiLineTemp += UnitSize * 2;
                Curr++;
            }
        }

        // Copy processed samples
        pAdiLineTemp = pAdiLine + (Height + UnitSize - 2) * 2;
        for (i = 0; i < Width; i++)
        {
            pAdiTemp[i] = pAdiLineTemp[i];
        }
        pAdiLineTemp = pAdiLine + (Height - 1) * 2;
        for (i = 1; i < Height; i++)
        {
            pAdiTemp[i * Width] = pAdiLineTemp[-(Ipp32s)i * 2];
            pAdiTemp[i * Width + 1] = pAdiLineTemp[-(Ipp32s)i * 2 + 1];
        }
    }
}

void H265SegmentDecoder::ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u InitTrDepth = (pCU->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N ? 0 : 1);
    Ipp32u NumPart = pCU->getNumPartInter(AbsPartIdx);
    Ipp32u NumQParts = pCU->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
    NumQParts >>= 2;

    if (pCU->GetIPCMFlag(AbsPartIdx))
    {
        ReconPCM(pCU, AbsPartIdx, Depth);
        return;
    }

    Ipp32u ChromaPredMode = pCU->GetChromaIntra(AbsPartIdx);
    if (ChromaPredMode == INTRA_DM_CHROMA_IDX)
    {
        ChromaPredMode = pCU->GetLumaIntra(AbsPartIdx);
    }

    for (Ipp32u PU = 0; PU < NumPart; PU++)
    {
        IntraRecQT(pCU, InitTrDepth, AbsPartIdx + PU * NumQParts, ChromaPredMode);
    }
}

void H265SegmentDecoder::IntraRecQT(H265CodingUnit* pCU,
                     Ipp32u TrDepth,
                     Ipp32u AbsPartIdx,
                     Ipp32u ChromaPredMode)
{
    Ipp32u FullDepth = pCU->GetDepth(AbsPartIdx) + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        bool neighborAvailable[65];
        Ipp32s numIntraNeighbors;

        numIntraNeighbors = CountIntraNeighbors(pCU, AbsPartIdx, TrDepth, neighborAvailable);
        IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx, numIntraNeighbors, neighborAvailable);

        Ipp32u FullDepth  = pCU->GetDepth(AbsPartIdx) + TrDepth;
        if ((pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize >> FullDepth) == 4)
        {
            TrDepth--;
            if ((AbsPartIdx & 0x03) == 0)
            {
                numIntraNeighbors = CountIntraNeighbors(pCU, AbsPartIdx, TrDepth, neighborAvailable);
                IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx, ChromaPredMode, numIntraNeighbors, neighborAvailable);
            }
        } else {
            IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx, ChromaPredMode, numIntraNeighbors, neighborAvailable);
        }
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart, ChromaPredMode);
        }
    }
}

void H265SegmentDecoder::IntraRecLumaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         Ipp32s numIntraNeighbors,
                         bool* neighborAvailable)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    Ipp32u Size = pCU->GetWidth(AbsPartIdx) >> TrDepth;

    //===== get Predicted Pels =====
    Ipp8u PredPel[4*64+1];

    const Ipp32s FilteredModes[] = {10, 7, 1, 0, 10};
    const Ipp8u h265_log2table[] =
    {
        2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
    };

    Ipp32s UnitSize = m_pSeqParamSet->MaxCUSize >> m_pSeqParamSet->MaxCUDepth;
    Ipp32s width = Size;

    h265_GetPredictPels_8u(
        m_pSeqParamSet->log2_min_transform_block_size, 
        pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + AbsPartIdx),
        PredPel, 
        neighborAvailable,
        numIntraNeighbors,
        width, 
        pCU->m_Frame->pitch_luma(), 
        1,
        UnitSize);

    bool isFilterNeeded = true;
    Ipp32u LumaPredMode = pCU->GetLumaIntra(AbsPartIdx);

    if (LumaPredMode == INTRA_LUMA_DC_IDX)
    {
        isFilterNeeded = false;
    }
    else
    {
        Ipp32s diff = IPP_MIN(abs((int)LumaPredMode - (int)INTRA_LUMA_HOR_IDX), abs((int)LumaPredMode - (int)INTRA_LUMA_VER_IDX));

        if (diff <= FilteredModes[h265_log2table[width - 4] - 2])
        {
            isFilterNeeded = false;
        }
    }   

    if (m_pSeqParamSet->sps_strong_intra_smoothing_enabled_flag && isFilterNeeded)
    {
        Ipp32s CUSize = pCU->GetWidth(AbsPartIdx) >> TrDepth;

        unsigned blkSize = 32;
        int threshold = 1 << (g_bitDepthY - 5);        

        int topLeft = PredPel[0];
        int topRight = PredPel[2*width];
        int midHor = PredPel[width];

        int bottomLeft = PredPel[4*width];
        int midVer = PredPel[3*width];

        bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold; 
        bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

        if (CUSize >= blkSize && (bilinearLeft && bilinearAbove))
        {
            h265_FilterPredictPels_Bilinear_8u(PredPel, width, topLeft, bottomLeft, topRight);
        }
        else
        {
            h265_FilterPredictPels_8u(PredPel, width);
        }
    }
    else if(isFilterNeeded)
    {
        h265_FilterPredictPels_8u(PredPel, width);
    }

    //===== get prediction signal =====    
    H265PlanePtrYCommon pRec = m_context->m_frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u pitch = m_context->m_frame->pitch_luma();

    switch(LumaPredMode)
    {
    case INTRA_LUMA_PLANAR_IDX:
        MFX_HEVC_PP::h265_PredictIntra_Planar_8u(PredPel, pRec, pitch, width);
        break;
    case INTRA_LUMA_DC_IDX:
        MFX_HEVC_PP::h265_PredictIntra_DC_8u(PredPel, pRec, pitch, width, 1);
        break;
    case INTRA_LUMA_VER_IDX:
        MFX_HEVC_PP::h265_PredictIntra_Ver_8u(PredPel, pRec, pitch, width, 8, 1);
        break;
    case INTRA_LUMA_HOR_IDX:
        MFX_HEVC_PP::h265_PredictIntra_Hor_8u(PredPel, pRec, pitch, width, 8, 1);
        break;
    default:
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_8u)(LumaPredMode, PredPel, pRec, pitch, width);
    }    

    //===== inverse transform =====
    if (!pCU->GetCbf(AbsPartIdx, TEXT_LUMA, TrDepth))
        return;

    m_TrQuant->SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_LUMA, m_pSeqParamSet->m_QPBDOffsetY, 0);

    Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) ? 0 : 3) + g_Table[(Ipp32s)TEXT_LUMA];
    Ipp32u NumCoeffInc = (m_pSeqParamSet->MaxCUSize * m_pSeqParamSet->MaxCUSize) >> (m_pSeqParamSet->MaxCUDepth << 1);
    H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffY + (NumCoeffInc * AbsPartIdx);
    bool useTransformSkip = pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_LUMA], AbsPartIdx) != 0;

    m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, pCU->GetLumaIntra(AbsPartIdx),
        pRec, pitch, pCoeff, Size, Size, scalingListType, useTransformSkip);

} // void H265SegmentDecoder::IntraRecLumaBlk(...)

void H265SegmentDecoder::IntraRecChromaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         Ipp32u ChromaPredMode,
                         Ipp32s numIntraNeighbors,
                         bool* neighborAvailable)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    Ipp32u Size = pCU->GetWidth(AbsPartIdx) >> TrDepth;

    InitNeighbourPatternChroma(pCU, AbsPartIdx, TrDepth,
        m_Prediction->GetPredicBuf(),
        m_Prediction->GetPredicBufWidth(),
        m_Prediction->GetPredicBufHeight(),
        neighborAvailable, numIntraNeighbors);

    //===== get prediction signal =====
    Size = pCU->GetWidth(AbsPartIdx) >> (TrDepth + 1);
    H265PlanePtrUVCommon pPatChroma = m_Prediction->GetPredicBuf();
    H265PlanePtrUVCommon pRecIPred = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u RecIPredStride = pCU->m_Frame->pitch_chroma();

    m_Prediction->h265_PredictIntraChroma(pPatChroma, ChromaPredMode, pRecIPred, RecIPredStride, Size);

    bool chromaUPresent = pCU->GetCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth) != 0;
    bool chromaVPresent = pCU->GetCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth) != 0;

    if (!chromaUPresent && !chromaVPresent)
        return;

    //===== inverse transform =====
    Ipp32u NumCoeffInc = ((pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize * pCU->m_SliceHeader->m_SeqParamSet->MaxCUSize) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1)) >> 2;
    Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

    // Cb
    if (chromaUPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->pps_cb_qp_offset + pCU->m_SliceHeader->slice_cb_qp_offset;
        m_TrQuant->SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_CHROMA_U, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_U];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCb + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
        bool useTransformSkip = pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_CHROMA_U], AbsPartIdx) != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT,
            pResi, residualPitch, pCoeff, Size, Size, scalingListType, useTransformSkip);
    }

    // Cr
    if (chromaVPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->pps_cr_qp_offset + pCU->m_SliceHeader->slice_cr_qp_offset;
        m_TrQuant->SetQPforQuant(pCU->GetQP(AbsPartIdx), TEXT_CHROMA_V, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_V];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCr + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;
        bool useTransformSkip = pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_CHROMA_V], AbsPartIdx) != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT,
            pResi, residualPitch, pCoeff, Size, Size, scalingListType, useTransformSkip);
    }

    //===== reconstruction =====
    {
        H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
        H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;
        for (Ipp32u y = 0; y < Size; y++)
        {
            for (Ipp32u x = 0; x < Size; x++)
            {
                if (chromaUPresent)
                    pRecIPred[2*x] = (H265PlaneUVCommon)ClipC(pRecIPred[2*x] + p_ResiU[x]);
                if (chromaVPresent)
                    pRecIPred[2*x+1] = (H265PlaneUVCommon)ClipC(pRecIPred[2*x + 1] + p_ResiV[x]);

            }
            p_ResiU     += residualPitch;
            p_ResiV     += residualPitch;
            pRecIPred += RecIPredStride;
        }
    }
}

Ipp32s H265SegmentDecoder::CountIntraNeighbors(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u TrDepth, bool *neighborAvailable)
{
    Ipp32u Size = pCU->GetWidth(AbsPartIdx) >> TrDepth;

    // compute the location of the current PU
    Ipp32s XInc = pCU->m_rasterToPelX[AbsPartIdx];
    Ipp32s YInc = pCU->m_rasterToPelY[AbsPartIdx];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;
    Ipp32s NumUnitsInCU = Size >> m_pSeqParamSet->log2_min_transform_block_size;

    Ipp32s numIntraNeighbors = 0;

    // below left
    if (XInc > 0 && pCU->m_rasterToZscan[pCU->m_zscanToRaster[AbsPartIdx] - 1 + NumUnitsInCU * m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            neighborAvailable[i] = false;
    }
    else
    {
        isRecIntraBelowLeftAvailable(TUPartNumberInCTB - 1 + NumUnitsInCU * m_context->m_CurrCTBStride, PartY, YInc, NumUnitsInCU, neighborAvailable + NumUnitsInCU - 1);
    }

    // left
    {
        bool *ValidFlags = neighborAvailable + NumUnitsInCU;
        Ipp32s flagIdx = TUPartNumberInCTB-1 + (m_context->m_CurrCTBStride)*(NumUnitsInCU-1);

        switch (NumUnitsInCU) {
        case 16:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
        case 8:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
        case 4:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
        case 2:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
        case 1:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx].members.IsAvailable; flagIdx-=m_context->m_CurrCTBStride;
        default:
            break;
        }

        flagIdx = TUPartNumberInCTB - 1 - m_context->m_CurrCTBStride;
        switch (NumUnitsInCU) {
        case 16:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
        case 8:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
        case 4:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
        case 2:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
        case 1:
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
            *ValidFlags++ = m_context->m_CurrRecFlags[flagIdx++].members.IsAvailable;
        default:
            break;
        }
    }
    
    // above right
    if (YInc > 0 && pCU->m_rasterToZscan[pCU->m_zscanToRaster[AbsPartIdx] + NumUnitsInCU - m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            neighborAvailable[NumUnitsInCU * 3 + 1 + i] = false;
    }
    else
    {
        if (YInc == 0)
            isRecIntraAboveRightAvailableOtherCTB(PartX, NumUnitsInCU, neighborAvailable + (NumUnitsInCU * 3) + 1);
        else
            isRecIntraAboveRightAvailable(TUPartNumberInCTB + NumUnitsInCU - m_context->m_CurrCTBStride, PartX, XInc, NumUnitsInCU, neighborAvailable + (NumUnitsInCU * 3) + 1);
    }

    numIntraNeighbors = 0;
    for(Ipp32s i=0; i<((NumUnitsInCU<<2)+1); i++)
        numIntraNeighbors += (Ipp32s)neighborAvailable[i];

    return numIntraNeighbors;
}

Ipp32s H265SegmentDecoder::isRecIntraAboveAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    for (Ipp32s i = 0; i < NumUnitsInCU; i++)
    {
        if (m_context->m_CurrRecFlags[TUPartNumberInCTB].members.IsAvailable)
        {
            NumIntra++;
            *ValidFlags = true;
        }
        else
        {
            *ValidFlags = false;
        }

        ValidFlags++; //opposite direction
        TUPartNumberInCTB++;
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isRecIntraLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    for (Ipp32s i = 0; i < NumUnitsInCU; i++)
    {
        if (m_context->m_CurrRecFlags[TUPartNumberInCTB].members.IsAvailable)
        {
            NumIntra++;
            *ValidFlags = true;
        }
        else
        {
            *ValidFlags = false;
        }

        ValidFlags--; //opposite direction
        TUPartNumberInCTB += m_context->m_CurrCTBStride;
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isRecIntraAboveRightAvailableOtherCTB(Ipp32s PartX, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartX += NumUnitsInCU;

    for (Ipp32s i = 0; i < NumUnitsInCU; i++)
    {
        if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
            m_context->m_RecTopNgbrs[PartX].members.IsAvailable)
        {
            NumIntra++;
            *ValidFlags = true;
        }
        else
        {
            *ValidFlags = false;
        }

        ValidFlags++; //opposite direction
        PartX++;
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isRecIntraAboveRightAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartX, Ipp32s XInc, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartX += NumUnitsInCU;
    XInc += NumUnitsInCU;

    for (Ipp32s i = 0; i < NumUnitsInCU; i++)
    {
        if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
            XInc < m_pCurrentFrame->getNumPartInCUSize() &&
            m_context->m_CurrRecFlags[TUPartNumberInCTB].members.IsAvailable)
        {
            NumIntra++;
            *ValidFlags = true;
        }
        else
        {
            *ValidFlags = false;
        }

        ValidFlags++; //opposite direction
        TUPartNumberInCTB++;
        XInc++;
        PartX++;
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isRecIntraBelowLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartY, Ipp32s YInc, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartY += NumUnitsInCU;
    YInc += NumUnitsInCU;

    for (Ipp32s i = 0; i < NumUnitsInCU; i++)
    {
        if ((PartY << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_height_in_luma_samples &&
            YInc < m_pCurrentFrame->getNumPartInCUSize() &&
            m_context->m_CurrRecFlags[TUPartNumberInCTB].members.IsAvailable)
        {
            NumIntra++;
            *ValidFlags = true;
        }
        else
        {
            *ValidFlags = false;
        }

        ValidFlags--; //opposite direction
        TUPartNumberInCTB += m_context->m_CurrCTBStride;
        YInc++;
        PartY++;
    }

    return NumIntra;
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
