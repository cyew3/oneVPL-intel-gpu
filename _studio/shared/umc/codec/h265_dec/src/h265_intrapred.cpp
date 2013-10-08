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
    void H265SegmentDecoder::ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
    {
        Ipp32u InitTrDepth = (pCU->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N ? 0 : 1);
        Ipp32u NumPart = pCU->getNumPartInter(AbsPartIdx);
        Ipp32u NumQParts = pCU->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
        NumQParts >>= 2;

        if (pCU->GetIPCMFlag(AbsPartIdx))
        {
            ReconPCM(pCU, AbsPartIdx, Depth);

            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> Depth;
            Ipp32s XInc = pCU->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = pCU->m_rasterToPelY[AbsPartIdx] >> 2;
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
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
            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> FullDepth;
            Ipp32s XInc = pCU->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = pCU->m_rasterToPelY[AbsPartIdx] >> 2;
            Ipp32s diagId = XInc + (m_pSeqParamSet->MaxCUSize >> 2) - YInc - 1;

            Ipp32u lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            Ipp32u tlIf = (*m_context->m_RecTLIntraFlags >> diagId) & 0x1;
            Ipp32u tpIf = m_context->m_RecTpIntraFlags[YInc] >> (XInc);

            IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx, tpIf, lfIf, tlIf);
            if (Size == 4) {
                if ((AbsPartIdx & 0x03) == 0) {
                    IntraRecChromaBlk(pCU, TrDepth-1, AbsPartIdx, ChromaPredMode, tpIf, lfIf, tlIf);
                }
            } else {
                IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx, ChromaPredMode, tpIf, lfIf, tlIf);
            }
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
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
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

        Ipp32u width = pCU->GetWidth(AbsPartIdx) >> TrDepth;

        //===== get Predicted Pels =====
        Ipp8u PredPel[4*64+1];

        const Ipp32s FilteredModes[] = {10, 7, 1, 0, 10};
        const Ipp8u h265_log2table[] =
        {
            2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
        };

        h265_GetPredPelsLuma_8u(
            pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + AbsPartIdx),
            PredPel, 
            width, 
            pCU->m_Frame->pitch_luma(), 
            tpIf, lfIf, tlIf);

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

        Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) ? 0 : 3);
        Ipp32u NumCoeffInc = (m_pSeqParamSet->MaxCUSize * m_pSeqParamSet->MaxCUSize) >> (m_pSeqParamSet->MaxCUDepth << 1);
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffY + (NumCoeffInc * AbsPartIdx);
        bool useTransformSkip = pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_LUMA], AbsPartIdx) != 0;

        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, pCU->GetLumaIntra(AbsPartIdx),
            pRec, pitch, pCoeff, width, width, scalingListType, useTransformSkip);

    } // void H265SegmentDecoder::IntraRecLumaBlk(...)

    void H265SegmentDecoder::IntraRecChromaBlk(H265CodingUnit* pCU,
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u ChromaPredMode,
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

        Ipp8u PredPel[4*64+2];
        h265_GetPredPelsChromaNV12_8u(pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx), PredPel, pCU->GetWidth(AbsPartIdx) >> TrDepth, pCU->m_Frame->pitch_chroma(), tpIf, lfIf, tlIf);

        //===== get prediction signal =====
        Ipp32u Size = pCU->GetWidth(AbsPartIdx) >> (TrDepth + 1);
        H265PlanePtrUVCommon pRecIPred = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
        Ipp32u RecIPredStride = pCU->m_Frame->pitch_chroma();

        switch(ChromaPredMode)
        {
        case INTRA_LUMA_PLANAR_IDX:
            h265_PredictIntra_Planar_ChromaNV12_8u(PredPel, pRecIPred, RecIPredStride, Size);
            break;
        case INTRA_LUMA_DC_IDX:
            h265_PredictIntra_DC_ChromaNV12_8u(PredPel, pRecIPred, RecIPredStride, Size);
            break;
        case INTRA_LUMA_VER_IDX:
            h265_PredictIntra_Ver_ChromaNV12_8u(PredPel, pRecIPred, RecIPredStride, Size);
            break;
        case INTRA_LUMA_HOR_IDX:
            h265_PredictIntra_Hor_ChromaNV12_8u(PredPel, pRecIPred, RecIPredStride, Size);
            break;
        default:
            h265_PredictIntra_Ang_ChromaNV12_8u(PredPel, pRecIPred, RecIPredStride, Size, ChromaPredMode);
            break;
        }

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
            Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 1 : 4);
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
            Ipp32s scalingListType = (pCU->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 2 : 5);
            H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCr + (NumCoeffInc * AbsPartIdx);
            H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;
            bool useTransformSkip = pCU->GetTransformSkip(g_ConvertTxtTypeToIdx[TEXT_CHROMA_V], AbsPartIdx) != 0;
            m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT,
                pResi, residualPitch, pCoeff, Size, Size, scalingListType, useTransformSkip);
        }
/*
        //===== reconstruction =====
        {
            H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
            H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;

            switch ((chromaVPresent << 1) | chromaUPresent) {

            case ((0 << 1) | 1):    // U only

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_loadl_epi64((__m128i *)&p_ResiU[x]);
                        __m128i ResiV = _mm_setzero_si128();
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);        
                
                        IPred = _mm_cvtepu8_epi16(IPred);
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);
                        IPred = _mm_add_epi16(IPred, ResiU);
                        IPred = _mm_packus_epi16(IPred, IPred);

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;

            case ((1 << 1) | 0):    // V only

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_setzero_si128();
                        __m128i ResiV = _mm_loadl_epi64((__m128i *)&p_ResiV[x]);
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);
                
                        IPred = _mm_cvtepu8_epi16(IPred);
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);
                        IPred = _mm_add_epi16(IPred, ResiU);
                        IPred = _mm_packus_epi16(IPred, IPred);

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;

            case ((1 << 1) | 1):    // both U and V

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_loadl_epi64((__m128i *)&p_ResiU[x]);        // load 4x16
                        __m128i ResiV = _mm_loadl_epi64((__m128i *)&p_ResiV[x]);        // load 4x16
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);    // load 8x8           
                
                        IPred = _mm_cvtepu8_epi16(IPred);           // zero-extend
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);   // interleave residuals
                        IPred = _mm_add_epi16(IPred, ResiU);        // add residuals
                        IPred = _mm_packus_epi16(IPred, IPred);     // ClipC()

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;
            }
        }
*/
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
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
