// Copyright (c) 2012-2018 Intel Corporation
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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#include "umc_h265_frame_info.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_tr_quant.h"

namespace UMC_HEVC_DECODER
{
    // Reconstruct intra quad tree including handling IPCM
    void H265SegmentDecoder::ReconIntraQT(uint32_t AbsPartIdx, uint32_t Depth)
    {
        uint32_t InitTrDepth = (m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N ? 0 : 1);
        uint32_t NumPart = m_cu->getNumPartInter(AbsPartIdx);
        uint32_t NumQParts = m_cu->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
        NumQParts >>= 2;

        if (m_cu->GetIPCMFlag(AbsPartIdx))
        {
            ReconPCM(AbsPartIdx, Depth);

            int32_t Size = m_pSeqParamSet->MaxCUSize >> Depth;
            int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
            return;
        }

        uint32_t ChromaPredMode = m_cu->GetChromaIntra(AbsPartIdx);

        for (uint32_t PU = 0; PU < NumPart; PU++)
        {
            IntraRecQT(InitTrDepth, AbsPartIdx + PU * NumQParts, ChromaPredMode);
        }
    }

    // Reconstruct intra (no IPCM) quad tree recursively
    void H265SegmentDecoder::IntraRecQT(
        uint32_t TrDepth,
        uint32_t AbsPartIdx,
        uint32_t ChromaPredMode)
    {
        uint32_t FullDepth = m_cu->GetDepth(AbsPartIdx) + TrDepth;
        uint32_t TrMode = m_cu->GetTrIndex(AbsPartIdx);
        if (TrMode == TrDepth)
        {
            int32_t Size = m_pSeqParamSet->MaxCUSize >> FullDepth;
            int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            int32_t diagId = XInc + (m_pSeqParamSet->MaxCUSize >> 2) - YInc - 1;

            uint32_t lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            uint32_t tlIf = (*m_context->m_RecTLIntraFlags >> diagId) & 0x1;
            uint32_t tpIf = m_context->m_RecTpIntraFlags[YInc] >> (XInc);

            IntraRecLumaBlk(TrDepth, AbsPartIdx, tpIf, lfIf, tlIf);

            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);

            uint32_t num4x4InCU =
                (m_cu->GetWidth(AbsPartIdx) >> TrDepth) / 4;
            if (Size == 4)
                //scesial case - virtually increase count for 4x4 since we gather chroma from 8x8
                //and should calc pred. pels accord. to correct neigbors
                num4x4InCU *= 2;

            uint32_t const scale = m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 ? 1 : 2;
            uint32_t const avlMask  = (1 << (num4x4InCU * scale)) - 1;

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
            uint32_t NumQPart = m_pCurrentFrame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
            for (uint32_t Part = 0; Part < 4; Part++)
            {
                IntraRecQT(TrDepth + 1, AbsPartIdx + Part * NumQPart, ChromaPredMode);
            }
        }
    }

    extern const int32_t FilteredModes[5] = {10, 7, 1, 0, 10};
    static const uint8_t h265_log2table[] =
    {
        2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
    };

    // Reconstruct intra luma block
    void H265SegmentDecoder::IntraRecLumaBlk(
        uint32_t TrDepth,
        uint32_t AbsPartIdx,
        uint32_t tpIf, uint32_t lfIf, uint32_t tlIf)
    {
        uint32_t width = m_cu->GetWidth(AbsPartIdx) >> TrDepth;

        uint8_t PredPel[(4*64+1) * 2];

        // Get prediction from neighbours
        m_reconstructor->GetPredPelsLuma(m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx),
            PredPel, width, m_pCurrentFrame->pitch_luma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_luma);

        bool isFilterNeeded = true;
        uint32_t LumaPredMode = m_cu->GetLumaIntra(AbsPartIdx);

        if (LumaPredMode == INTRA_LUMA_DC_IDX)
        {
            isFilterNeeded = false;
        }
        else
        {
            int32_t diff = MFX_MIN(abs((int)LumaPredMode - (int)INTRA_LUMA_HOR_IDX), abs((int)LumaPredMode - (int)INTRA_LUMA_VER_IDX));

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
        uint32_t pitch = m_context->m_frame->pitch_luma();

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

    // Reconstruct intra NV12 chroma block
    void H265SegmentDecoder::IntraRecChromaBlk(uint32_t TrDepth,
        uint32_t AbsPartIdx,
        uint32_t ChromaPredMode,
        uint32_t tpIf, uint32_t lfIf, uint32_t tlIf)
    {
        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_400)
            return;

        uint8_t PredPel[(4*64+2)*2];

        m_reconstructor->GetPredPelsChromaNV12(m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx),
            PredPel, m_cu->GetWidth(AbsPartIdx) >> TrDepth, m_pCurrentFrame->pitch_chroma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_chroma);

        // Get prediction from neighbours
        uint32_t Size = m_cu->GetWidth(AbsPartIdx) >> (TrDepth + 1);
        PlanePtrUV pRecIPred = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx);
        uint32_t RecIPredStride = m_pCurrentFrame->pitch_chroma();

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
            uint32_t residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

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
                    SumOfResidAndPred<uint16_t>(p_ResiU, p_ResiV, residualPitch, (uint16_t*)pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
                else
                    SumOfResidAndPred<uint8_t>(p_ResiU, p_ResiV, residualPitch, pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
            }
        }

        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
        {
            uint32_t num4x4InCU = m_cu->GetWidth(AbsPartIdx) >> TrDepth >> 2;
            uint32_t const avlMask = (((uint32_t)(1<<((num4x4InCU))))-1);

            //we always have top from U/V
            tpIf = avlMask;

            //re-use Left flags from U/V to calculate U1/V1 TopLeft flag:
            //peek first half (U/V) and get lowest position - this is MSB bit
            uint32_t const TL_idx = (num4x4InCU / 2) - 1;
            uint32_t const TL_mask = 1 << TL_idx;
            tlIf = !!(lfIf & TL_mask);

            //update Left flags
            int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> 2;
            int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> 2;
            YInc += num4x4InCU >> 1; //shift flags for U1/V1
            lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            lfIf &= avlMask;

            uint32_t const bottomOffset =
                (Size*RecIPredStride) << m_pSeqParamSet->need16bitOutput;

            m_reconstructor->GetPredPelsChromaNV12(m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx) + bottomOffset,
                PredPel, 2*Size, m_pCurrentFrame->pitch_chroma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_chroma);

            m_reconstructor->PredictIntraChroma(ChromaPredMode, PredPel, pRecIPred + bottomOffset, RecIPredStride, Size);

            if (chromaUPresent1 || chromaVPresent1)
            {
                uint32_t residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;
                uint32_t bottomResOffset = Size*residualPitch;
                CoeffsPtr p_ResiU = (CoeffsPtr)m_ppcYUVResi->m_pUPlane + bottomResOffset;
                CoeffsPtr p_ResiV = (CoeffsPtr)m_ppcYUVResi->m_pVPlane + bottomResOffset;

                if (m_pSeqParamSet->need16bitOutput)
                    SumOfResidAndPred<uint16_t>(p_ResiU, p_ResiV, residualPitch, (uint16_t*)(pRecIPred + bottomOffset), RecIPredStride, Size, chromaUPresent1, chromaVPresent1, m_pSeqParamSet->bit_depth_chroma);
                else
                    SumOfResidAndPred<uint8_t>(p_ResiU, p_ResiV, residualPitch, pRecIPred + bottomOffset, RecIPredStride, Size, chromaUPresent1, chromaVPresent1, m_pSeqParamSet->bit_depth_chroma);
            }
        }
    }
} // end namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
