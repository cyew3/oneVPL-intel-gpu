//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#include "umc_h265_frame_info.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_tr_quant.h"

namespace UMC_HEVC_DECODER
{
    // Reconstruct intra quad tree including handling IPCM
    void H265SegmentDecoder::ReconIntraQT(Ipp32u AbsPartIdx, Ipp32u Depth)
    {
        Ipp32u InitTrDepth = (m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N ? 0 : 1);
        Ipp32u NumPart = m_cu->getNumPartInter(AbsPartIdx);
        Ipp32u NumQParts = m_cu->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
        NumQParts >>= 2;

        if (m_cu->GetIPCMFlag(AbsPartIdx))
        {
            ReconPCM(AbsPartIdx, Depth);

            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> Depth;
            Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
            return;
        }

        Ipp32u ChromaPredMode = m_cu->GetChromaIntra(AbsPartIdx);

        for (Ipp32u PU = 0; PU < NumPart; PU++)
        {
            IntraRecQT(InitTrDepth, AbsPartIdx + PU * NumQParts, ChromaPredMode);
        }
    }

    // Reconstruct intra (no IPCM) quad tree recursively
    void H265SegmentDecoder::IntraRecQT(
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u ChromaPredMode)
    {
        Ipp32u FullDepth = m_cu->GetDepth(AbsPartIdx) + TrDepth;
        Ipp32u TrMode = m_cu->GetTrIndex(AbsPartIdx);
        if (TrMode == TrDepth)
        {
            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> FullDepth;
            Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            Ipp32s diagId = XInc + (m_pSeqParamSet->MaxCUSize >> 2) - YInc - 1;

            Ipp32u lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            Ipp32u tlIf = (*m_context->m_RecTLIntraFlags >> diagId) & 0x1;
            Ipp32u tpIf = m_context->m_RecTpIntraFlags[YInc] >> (XInc);

            IntraRecLumaBlk(TrDepth, AbsPartIdx, tpIf, lfIf, tlIf);

            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);

            Ipp32u num4x4InCU =
                (m_cu->GetWidth(AbsPartIdx) >> TrDepth) / 4;
            if (Size == 4)
                //scesial case - virtually increase count for 4x4 since we gather chroma from 8x8
                //and should calc pred. pels accord. to correct neigbors
                num4x4InCU *= 2;

            Ipp32u const scale = m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 ? 1 : 2;
            Ipp32u const avlMask  = (1 << (num4x4InCU * scale)) - 1;

            if (Size != 4)
            {
                IntraRecChromaBlk(TrDepth, AbsPartIdx, ChromaPredMode, tpIf, lfIf & avlMask, tlIf);
            }
            else
            {
                if ((AbsPartIdx & 0x03) == 0)
                {
                    // save flags because UpdateRecNeighboursBuffersN will change it
                    save_lfIf = lfIf;
                    save_tlIf = tlIf;
                    save_tpIf = tpIf;
                }

                if ((AbsPartIdx & 0x03) == 3)
                {
                    IntraRecChromaBlk(TrDepth-1, AbsPartIdx-3, ChromaPredMode, save_tpIf, save_lfIf & avlMask, save_tlIf);
                }
            }
        }
        else
        {
            Ipp32u NumQPart = m_pCurrentFrame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
            for (Ipp32u Part = 0; Part < 4; Part++)
            {
                IntraRecQT(TrDepth + 1, AbsPartIdx + Part * NumQPart, ChromaPredMode);
            }
        }
    }

    extern const Ipp32s FilteredModes[5] = {10, 7, 1, 0, 10};
    static const Ipp8u h265_log2table[] =
    {
        2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
    };

    // Reconstruct intra luma block
    void H265SegmentDecoder::IntraRecLumaBlk(
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        Ipp32u width = m_cu->GetWidth(AbsPartIdx) >> TrDepth;

        Ipp8u PredPel[(4*64+1) * 2];

        // Get prediction from neighbours
        m_reconstructor->GetPredPelsLuma(m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx),
            PredPel, width, m_pCurrentFrame->pitch_luma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_luma);

        bool isFilterNeeded = true;
        Ipp32u LumaPredMode = m_cu->GetLumaIntra(AbsPartIdx);

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

        if (isFilterNeeded)
        {
            m_reconstructor->FilterPredictPels(m_context, m_cu, PredPel, width, TrDepth, AbsPartIdx);
        }

        // Do prediction from neighbours
        PlanePtrY pRec = m_context->m_frame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx);
        Ipp32u pitch = m_context->m_frame->pitch_luma();

        m_reconstructor->PredictIntra(LumaPredMode, PredPel, pRec, pitch, width, m_pSeqParamSet->bit_depth_luma);

        // Inverse transform and addition of residual and prediction
        if (!m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx, TrDepth))
            return;

        CoeffsPtr pCoeff = m_context->m_coeffsRead;
        m_context->m_coeffsRead += width*width;
        bool useTransformSkip = m_cu->GetTransformSkip(COMPONENT_LUMA, AbsPartIdx) != 0;

        m_TrQuant->InvTransformNxN(m_cu->GetCUTransquantBypass(AbsPartIdx), TEXT_LUMA, m_cu->GetLumaIntra(AbsPartIdx),
            pRec, pitch, pCoeff, width, useTransformSkip);
    } // void H265SegmentDecoder::IntraRecLumaBlk(...)

    // Add residual to prediction for NV12 chroma
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

    // Reconstruct intra NV12 chroma block
    void H265SegmentDecoder::IntraRecChromaBlk(Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u ChromaPredMode,
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_400)
            return;

        Ipp8u PredPel[(4*64+2)*2];

        m_reconstructor->GetPredPelsChromaNV12(m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx),
            PredPel, m_cu->GetWidth(AbsPartIdx) >> TrDepth, m_pCurrentFrame->pitch_chroma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_chroma);

        // Get prediction from neighbours
        Ipp32u Size = m_cu->GetWidth(AbsPartIdx) >> (TrDepth + 1);
        PlanePtrUV pRecIPred = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx);
        Ipp32u RecIPredStride = m_pCurrentFrame->pitch_chroma();

        // Do prediction from neighbours
        m_reconstructor->PredictIntraChroma(ChromaPredMode, PredPel, pRecIPred, RecIPredStride, Size);

        bool chromaUPresent = m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrDepth) != 0;
        bool chromaVPresent = m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrDepth) != 0;

        bool chromaUPresent1 = 0;
        bool chromaVPresent1 = 0;

        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
        {
            chromaUPresent1 = m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx, TrDepth) != 0;
            chromaVPresent1 = m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx, TrDepth) != 0;
        }

        if (chromaUPresent || chromaVPresent || chromaUPresent1 || chromaVPresent1)
        {
            // Inverse transform and addition of residual and prediction
            Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

            // Cb
            if (chromaUPresent)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;

                CoeffsPtr pResi = (CoeffsPtr)m_ppcYUVResi->m_pUPlane;
                bool useTransformSkip = m_cu->GetTransformSkip(COMPONENT_CHROMA_U, AbsPartIdx) != 0;
                m_TrQuant->InvTransformNxN(m_cu->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT,
                    pResi, residualPitch, pCoeff, Size, useTransformSkip);
            }

            if (chromaUPresent1)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;

                CoeffsPtr pResi = (CoeffsPtr)m_ppcYUVResi->m_pUPlane + Size*residualPitch;
                bool useTransformSkip = m_cu->GetTransformSkip(COMPONENT_CHROMA_U1, AbsPartIdx) != 0;
                m_TrQuant->InvTransformNxN(m_cu->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT,
                    pResi, residualPitch, pCoeff, Size, useTransformSkip);
            }

            // Cr
            if (chromaVPresent)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;

                CoeffsPtr pResi = (CoeffsPtr)m_ppcYUVResi->m_pVPlane;
                bool useTransformSkip = m_cu->GetTransformSkip(COMPONENT_CHROMA_V, AbsPartIdx) != 0;
                m_TrQuant->InvTransformNxN(m_cu->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT,
                    pResi, residualPitch, pCoeff, Size, useTransformSkip);
            }

            if (chromaVPresent1)
            {
                CoeffsPtr pCoeff = m_context->m_coeffsRead;
                m_context->m_coeffsRead += Size*Size;

                CoeffsPtr pResi = (CoeffsPtr)m_ppcYUVResi->m_pVPlane + Size*residualPitch;
                bool useTransformSkip = m_cu->GetTransformSkip(COMPONENT_CHROMA_V1, AbsPartIdx) != 0;
                m_TrQuant->InvTransformNxN(m_cu->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT,
                    pResi, residualPitch, pCoeff, Size, useTransformSkip);
            }

            CoeffsPtr p_ResiU = (CoeffsPtr)m_ppcYUVResi->m_pUPlane;
            CoeffsPtr p_ResiV = (CoeffsPtr)m_ppcYUVResi->m_pVPlane;

            if (chromaUPresent || chromaVPresent)
            {
                if (m_pSeqParamSet->need16bitOutput)
                    SumOfResidAndPred<Ipp16u>(p_ResiU, p_ResiV, residualPitch, (Ipp16u*)pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
                else
                    SumOfResidAndPred<Ipp8u>(p_ResiU, p_ResiV, residualPitch, pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
            }
        }

        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
        {
            Ipp32u num4x4InCU = m_cu->GetWidth(AbsPartIdx) >> TrDepth >> 2;
            Ipp32u const avlMask = (((Ipp32u)(1<<((num4x4InCU))))-1);

            //we always have top from U/V
            tpIf = avlMask;

            //re-use Left flags from U/V to calculate U1/V1 TopLeft flag:
            //peek first half (U/V) and get lowest position - this is MSB bit
            Ipp32u const TL_idx = (num4x4InCU / 2) - 1;
            Ipp32u const TL_mask = 1 << TL_idx;
            tlIf = !!(lfIf & TL_mask);

            //update Left flags
            Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            YInc += num4x4InCU >> 1; //shift flags for U1/V1
            lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            lfIf &= avlMask;

            Ipp32u const bottomOffset =
                (Size*RecIPredStride) << m_pSeqParamSet->need16bitOutput;

            m_reconstructor->GetPredPelsChromaNV12(m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx) + bottomOffset,
                PredPel, 2*Size, m_pCurrentFrame->pitch_chroma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_chroma);

            m_reconstructor->PredictIntraChroma(ChromaPredMode, PredPel, pRecIPred + bottomOffset, RecIPredStride, Size);

            if (chromaUPresent1 || chromaVPresent1)
            {
                Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;
                Ipp32u bottomResOffset = Size*residualPitch;
                CoeffsPtr p_ResiU = (CoeffsPtr)m_ppcYUVResi->m_pUPlane + bottomResOffset;
                CoeffsPtr p_ResiV = (CoeffsPtr)m_ppcYUVResi->m_pVPlane + bottomResOffset;

                if (m_pSeqParamSet->need16bitOutput)
                    SumOfResidAndPred<Ipp16u>(p_ResiU, p_ResiV, residualPitch, (Ipp16u*)(pRecIPred + bottomOffset), RecIPredStride, Size, chromaUPresent1, chromaVPresent1, m_pSeqParamSet->bit_depth_chroma);
                else
                    SumOfResidAndPred<Ipp8u>(p_ResiU, p_ResiV, residualPitch, pRecIPred + bottomOffset, RecIPredStride, Size, chromaUPresent1, chromaVPresent1, m_pSeqParamSet->bit_depth_chroma);
            }
        }
    }
} // end namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
