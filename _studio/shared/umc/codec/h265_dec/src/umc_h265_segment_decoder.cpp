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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#include "umc_h265_segment_decoder.h"
#include "umc_h265_tr_quant.h"
#include "umc_h265_frame_info.h"
#include "mfx_h265_optimization.h"
#include "umc_h265_task_broker.h"

#define Saturate(min_val, max_val, val) MFX_MAX((min_val), MFX_MIN((max_val), (val)))

namespace UMC_HEVC_DECODER
{

DecodingContext::DecodingContext()
{
    m_sps = 0;
    m_pps = 0;
    m_frame = 0;
    m_TopNgbrs = 0;
    m_CurrCTBFlags = 0;
    m_CurrCTBStride = 0;
    m_LastValidQP = 0;
    m_LastValidOffsetIndex = -1;

    m_needToSplitDecAndRec = true;
    m_RecTpIntraFlags = 0;
    m_RecLfIntraFlags = 0;
    m_RecTLIntraFlags = 0;
    m_mvsDistortion = 0;
    m_sh = 0;
    m_coeffsRead = 0;
    m_mvsDistortionTemp = 0;
    m_RecTpIntraRowFlags = 0;
    m_weighted_prediction = 0;
    m_dequantCoef = 0;
    m_coeffsWrite = 0;
}

// Clear all flags in left and top buffers
void DecodingContext::Reset()
{
    if (m_needToSplitDecAndRec)
        ResetRowBuffer();
}

// Allocate context buffers
void DecodingContext::Init(H265Task *task)
{
    H265Slice *slice = task->m_pSlice;
    m_sps = slice->GetSeqParam();
    m_pps = slice->GetPicParam();
    m_sh = &slice->m_SliceHeader;
    m_frame = slice->GetCurrentFrame();

    if (m_TopNgbrsHolder.size() < m_sps->NumPartitionsInFrameWidth + 2)
    {
        // Zero element is left-top diagonal from zero CTB
        m_TopNgbrsHolder.resize(m_sps->NumPartitionsInFrameWidth + 2);
    }

    m_TopNgbrs = &m_TopNgbrsHolder[1];

    VM_ASSERT(m_sps->pic_width_in_luma_samples <= 11776); // Top Intra flags placeholder can store up to (11776 / 4 / 32) + 1 flags
    m_RecTpIntraFlags    = m_RecIntraFlagsHolder;
    m_RecLfIntraFlags    = m_RecIntraFlagsHolder + 17;
    m_RecTLIntraFlags    = m_RecIntraFlagsHolder + 34;
    m_RecTpIntraRowFlags = (uint16_t*)(m_RecIntraFlagsHolder + 35) + 1;

    // One element outside of CTB at each side for neighbour CTBs
    m_CurrCTBStride = (m_sps->NumPartitionsInCUSize + 2);
    if (m_CurrCTBFlagsHolder.size() < (uint32_t)(m_CurrCTBStride * m_CurrCTBStride))
    {
        m_CurrCTBFlagsHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
    }

    // Pointer to the beginning of current CTB, so negative offsets are allowed for top elements
    //
    // +--------
    // |*Array start ptr is here
    // | +------
    // | |*CTB start ptr is here
    // | |
    // | |
    m_CurrCTBFlags = &m_CurrCTBFlagsHolder[m_CurrCTBStride + 1];

    int32_t sliceNum = slice->GetSliceNum();
    m_refPicList[0] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_refPicList[1] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;

    if (m_needToSplitDecAndRec && !slice->GetSliceHeader()->dependent_slice_segment_flag)
    {
        ResetRowBuffer();
        m_LastValidQP = slice->m_SliceHeader.SliceQP ^ 1; // Force QP recalculation because QP offsets may be different in new slice
        m_LastValidOffsetIndex = -1;
        SetNewQP(slice->m_SliceHeader.SliceQP);
    }

    int32_t oneCUSize = sizeof(CoeffsPtr) * m_sps->MaxCUSize*m_sps->MaxCUSize;
    oneCUSize = oneCUSize*3 / 2; // add chroma coeffs

    int32_t rowCoeffSize;
    int32_t rowCount;

    int32_t widthInCU = m_sps->WidthInCU;

    if (task->m_threadingInfo)
    {
        widthInCU = task->m_threadingInfo->processInfo.m_width;
    }

    // decide number of buffers
    if (slice->m_iMaxMB - slice->m_iFirstMB > widthInCU)
    {
        rowCount = (slice->m_iMaxMB - slice->m_iFirstMB + widthInCU - 1) / widthInCU;
        rowCount = MFX_MIN(4, rowCount);
        rowCoeffSize =  widthInCU * oneCUSize;
    }
    else
    {
        rowCoeffSize = (slice->m_iMaxMB - slice->m_iFirstMB) * oneCUSize;
        rowCount = 1;
    }

    m_coeffBuffer.Init(rowCount, rowCoeffSize);

    m_coeffsRead = 0;
    m_coeffsWrite = 0;

    m_mvsDistortion = 0;
    m_mvsDistortionTemp = 0;

    m_weighted_prediction = m_sh->slice_type == P_SLICE ? m_pps->weighted_pred_flag : m_pps->weighted_bipred_flag;
}

// Fill up decoder context for new CTB using previous CTB values and values stored for top row
void DecodingContext::UpdateCurrCUContext(uint32_t lastCUAddr, uint32_t newCUAddr)
{
    // Set local pointers to real array start positions
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    uint32_t lastCUX = (lastCUAddr % m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    uint32_t lastCUY = (lastCUAddr / m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    uint32_t newCUX = (newCUAddr % m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    uint32_t newCUY = (newCUAddr / m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;

    int32_t bottom_row_offset = m_CurrCTBStride * (m_CurrCTBStride - 2);
    if (newCUY > 0)
    {
        if (newCUX > lastCUX)
        {
            // Init top margin from previous row
            for (int32_t i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i] = TopNgbrs[newCUX + i];
            }
            // Store bottom margin for next row if next CTB is to the right. This is necessary for left-top diagonal
            for (int32_t i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
            }
        }
        else if (newCUX < lastCUX)
        {
            // Store bottom margin for next row if next CTB is to the left-below. This is necessary for right-top diagonal
            for (int32_t i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
            }
            // Init top margin from previous row
            for (int32_t i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i] = TopNgbrs[newCUX + i];
            }
        }
        else // New CTB right under previous CTB
        {
            // Copy data from bottom row
            for (int32_t i = 0; i < m_CurrCTBStride; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[i] = CurrCTBFlags[bottom_row_offset + i];
            }
        }
    }
    else
    {
        // Should be reset from the beginning and not filled up until 2nd row
        for (int32_t i = 0; i < m_CurrCTBStride; i++)
            VM_ASSERT(CurrCTBFlags[i].data == 0);
    }

    if (newCUX != lastCUX)
    {
        // Store bottom margin for next row if next CTB is not underneath. This is necessary for left-top diagonal
        for (int32_t i = 1; i < m_CurrCTBStride - 1; i++)
        {
            TopNgbrs[lastCUX + i].data = CurrCTBFlags[bottom_row_offset + i].data;
        }
    }

    if (lastCUY == newCUY)
    {
        // Copy left margin from right
        for (int32_t i = 1; i < m_CurrCTBStride - 1; i++)
        {
            CurrCTBFlags[i * m_CurrCTBStride] = CurrCTBFlags[i * m_CurrCTBStride + m_CurrCTBStride - 2];
        }
    }
    else
    {
        for (int32_t i = 1; i < m_CurrCTBStride; i++)
            CurrCTBFlags[i * m_CurrCTBStride].data = 0;
    }

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    // Clean inside of CU context
    for (int32_t i = 1; i < m_CurrCTBStride; i++)
        memset(&CurrCTBFlags[i * m_CurrCTBStride + 1], 0, (m_CurrCTBStride - 1)*sizeof(H265FrameHLDNeighborsInfo));
}

// Update reconstruct information with neighbour information for intra prediction
void DecodingContext::UpdateRecCurrCTBContext(int32_t lastCUAddr, int32_t newCUAddr)
{
    int32_t maxCUSzIn4x4 = m_sps->MaxCUSize >> 2;
    int32_t lCTBXx4 = lastCUAddr % m_sps->WidthInCU;
    int32_t nCTBXx4 = newCUAddr % m_sps->WidthInCU;
    bool   isSameY = ((lastCUAddr-lCTBXx4) == (newCUAddr-nCTBXx4));

    lCTBXx4 <<= (m_sps->log2_max_luma_coding_block_size - 2);
    nCTBXx4 <<= (m_sps->log2_max_luma_coding_block_size - 2);

    if(nCTBXx4 && isSameY) {
        m_RecLfIntraFlags[0] = m_RecLfIntraFlags[maxCUSzIn4x4];
        memset(m_RecLfIntraFlags+1, 0, maxCUSzIn4x4*sizeof(uint32_t));
    } else {
        memset(m_RecLfIntraFlags, 0, (maxCUSzIn4x4+1)*sizeof(uint32_t));
    }

    uint32_t tDiag, diagIds;

    tDiag = (((uint32_t)(m_RecTpIntraRowFlags[(nCTBXx4>>4)-1])) | ((uint32_t)(m_RecTpIntraRowFlags[(nCTBXx4>>4)]) << 16)) >> 15;
    tDiag = (tDiag >> ((nCTBXx4 & 0xF))) & 0x1;

    uint32_t mask = ((0x1 << maxCUSzIn4x4) - 1) << (lCTBXx4 & 0xF) & 0x0000FFFF;
    m_RecTpIntraRowFlags[lCTBXx4>>4] &= ~mask;
    m_RecTpIntraRowFlags[lCTBXx4>>4] |= ((m_RecTpIntraFlags[maxCUSzIn4x4] << (lCTBXx4 & 0xF)) & 0x0000FFFF);

    memset(m_RecTpIntraFlags, 0, (maxCUSzIn4x4+1)*sizeof(uint32_t));
    m_RecTpIntraFlags[0] =  ((uint32_t)(m_RecTpIntraRowFlags[nCTBXx4>>4])) | ((uint32_t)(m_RecTpIntraRowFlags[(nCTBXx4>>4)+1]) << 16);
    m_RecTpIntraFlags[0] >>= (nCTBXx4 & 0xF);

    diagIds  = (m_RecTpIntraFlags[0] << 1) | tDiag;
    tDiag    = m_RecLfIntraFlags[0];

    for(int32_t i=0; i<maxCUSzIn4x4-1; i++) {
        diagIds <<= 1; diagIds |= (tDiag & 0x1); tDiag >>= 1;
    }

    *m_RecTLIntraFlags = diagIds;
}

// Clean up all availability information for decoder
void DecodingContext::ResetRowBuffer()
{
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    for (uint32_t i = 0; i < m_sps->NumPartitionsInFrameWidth + 2; i++)
        TopNgbrs[i].data = 0;

    for (int32_t i = 0; i < m_CurrCTBStride * m_CurrCTBStride; i++)
        CurrCTBFlags[i].data = 0;
}

// Clean up all availability information for reconstruct
void DecodingContext::ResetRecRowBuffer()
{
    memset(m_RecIntraFlagsHolder, 0, sizeof(m_RecIntraFlagsHolder));
}

// Set new QP value and calculate scaled values for luma and chroma
void DecodingContext::SetNewQP(int32_t newQP, int32_t chroma_offset_idx)
{
    if (newQP == m_LastValidQP)
        return;

    if (chroma_offset_idx == -1)
    {
        m_LastValidQP = newQP;

        int32_t qpScaledY = m_LastValidQP + m_sps->m_QPBDOffsetY;
        m_ScaledQP[COMPONENT_LUMA].m_QPRem = qpScaledY % 6;
        m_ScaledQP[COMPONENT_LUMA].m_QPPer = qpScaledY / 6;
        m_ScaledQP[COMPONENT_LUMA].m_QPScale =
            g_invQuantScales[m_ScaledQP[COMPONENT_LUMA].m_QPRem] << m_ScaledQP[COMPONENT_LUMA].m_QPPer;
    }

    if (chroma_offset_idx == -1)
        chroma_offset_idx = m_LastValidOffsetIndex;
    else if (chroma_offset_idx == 0)
        m_LastValidOffsetIndex = -1;
    else
        m_LastValidOffsetIndex = chroma_offset_idx;

    int32_t qpOffsetCb = m_pps->pps_cb_qp_offset + m_sh->slice_cb_qp_offset;
    int32_t qpOffsetCr = m_pps->pps_cr_qp_offset + m_sh->slice_cr_qp_offset;
    if (chroma_offset_idx != -1)
    {
        qpOffsetCb += m_pps->cb_qp_offset_list[chroma_offset_idx];
        qpOffsetCr += m_pps->cr_qp_offset_list[chroma_offset_idx];
    }

    int32_t qpBdOffsetC = m_sps->m_QPBDOffsetC;
    int32_t qpScaledCb = mfx::clamp(m_LastValidQP + qpOffsetCb, -qpBdOffsetC, 57);
    int32_t qpScaledCr = mfx::clamp(m_LastValidQP + qpOffsetCr, -qpBdOffsetC, 57);

    int32_t chromaScaleIndex = m_sps->ChromaArrayType != CHROMA_FORMAT_420 ? 1 : 0;

    if (qpScaledCb < 0)
        qpScaledCb = qpScaledCb + qpBdOffsetC;
    else
        qpScaledCb = g_ChromaScale[chromaScaleIndex][qpScaledCb] + qpBdOffsetC;

    if (qpScaledCr < 0)
        qpScaledCr = qpScaledCr + qpBdOffsetC;
    else
        qpScaledCr = g_ChromaScale[chromaScaleIndex][qpScaledCr] + qpBdOffsetC;

    m_ScaledQP[COMPONENT_CHROMA_U].m_QPRem = qpScaledCb % 6;
    m_ScaledQP[COMPONENT_CHROMA_U].m_QPPer = qpScaledCb / 6;
    m_ScaledQP[COMPONENT_CHROMA_U].m_QPScale =
        g_invQuantScales[m_ScaledQP[COMPONENT_CHROMA_U].m_QPRem] << m_ScaledQP[COMPONENT_CHROMA_U].m_QPPer;

    m_ScaledQP[COMPONENT_CHROMA_V].m_QPRem = qpScaledCr % 6;
    m_ScaledQP[COMPONENT_CHROMA_V].m_QPPer = qpScaledCr / 6;
    m_ScaledQP[COMPONENT_CHROMA_V].m_QPScale =
        g_invQuantScales[m_ScaledQP[COMPONENT_CHROMA_V].m_QPRem] << m_ScaledQP[COMPONENT_CHROMA_V].m_QPPer;
}

H265SegmentDecoder::H265SegmentDecoder(TaskBroker_H265 * pTaskBroker)
    : H265SegmentDecoderBase(pTaskBroker)
{
    m_pSlice = 0;

    m_cu = 0;
    // prediction buffer
    m_Prediction.reset(new H265Prediction()); //PREDICTION

    m_ppcYUVResi = 0;
    m_TrQuant = 0;

    m_MaxDepth = 0;
    m_bakAbsPartIdxQp = 0;
    m_context = 0;
    m_IsCuChromaQpOffsetCoded = 0;
    m_BakAbsPartIdxChroma = 0;
    m_minCUDQPSize = 0;
    m_DecodeDQPFlag = 0;
    m_pSliceHeader = 0;
    m_context_single_thread.reset(new DecodingContext());
} // H265SegmentDecoder::H265SegmentDecoder(TaskBroker_H265 * pTaskBroker)

H265SegmentDecoder::~H265SegmentDecoder(void)
{
    Release();
} // H265SegmentDecoder::~H265SegmentDecoder(void)

// Initialize new slice decoder instance
void H265SegmentDecoder::create(H265SeqParamSet* sps)
{
    static PartitionInfo g_rasterOffsets;

    m_MaxDepth = sps->MaxCUDepth + 1;

    if (!m_ppcYUVResi || (uint32_t)m_ppcYUVResi->lumaSize().width != sps->MaxCUSize || (uint32_t)m_ppcYUVResi->lumaSize().height != sps->MaxCUSize)
    {
        // initialize partition order.
        g_rasterOffsets.Init(sps);

        if (!m_ppcYUVResi)
        {
            m_ppcYUVResi = new H265DecYUVBufferPadded;
        }
        else
        {
            m_ppcYUVResi->destroy();
        }

        m_ppcYUVResi->createPredictionBuffer(sps);
    }

    if (!m_TrQuant)
        m_TrQuant = new H265TrQuant(); //TRQUANT
}

void H265SegmentDecoder::destroy()
{
}

// Release memory
void H265SegmentDecoder::Release(void)
{
    delete m_TrQuant;
    m_TrQuant = NULL; //TRQUANT

    if (m_ppcYUVResi)
    {
        m_ppcYUVResi->destroy();
        delete m_ppcYUVResi;
        m_ppcYUVResi = NULL;
    }

} // void H265SegmentDecoder::Release(void)

// Initialize decoder's number
UMC::Status H265SegmentDecoder::Init(int32_t iNumber)
{
    // release object before initialization
    Release();

    // save ordinal number
    m_iNumber = iNumber;

    return UMC::UMC_OK;

} // Status H265SegmentDecoder::Init(int32_t sNumber)

// Decode SAO truncated rice offset value. HEVC spec 9.3.3.2
int32_t H265SegmentDecoder::parseSaoMaxUvlc(int32_t maxSymbol)
{
    VM_ASSERT(maxSymbol != 0);

    uint32_t code = m_pBitStream->DecodeSingleBinEP_CABAC();

    if (code == 0)
    {
        return 0;
    }

    int32_t i = 1;
    for (;;)
    {
        code = m_pBitStream->DecodeSingleBinEP_CABAC();

        if (code == 0)
        {
            break;
        }

        i++;

        if (i == maxSymbol)
        {
            break;
        }
    }

    return i;
}

// Decode SAO type idx
int32_t H265SegmentDecoder::parseSaoTypeIdx()
{
    if (m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_TYPE_IDX_HEVC]) == 0)
    {
        return 0;
    }
    else
    {
        return (!m_pBitStream->DecodeSingleBinEP_CABAC()) ? 5 : 1;
    }
}

// Decode SAO plane type and offsets
void H265SegmentDecoder::parseSaoOffset(SAOLCUParam* psSaoLcuParam, uint32_t compIdx)
{
    int32_t typeIdx;

    if (compIdx == 2)
    {
        typeIdx = psSaoLcuParam->m_typeIdx[1];
    }
    else
    {
        typeIdx = parseSaoTypeIdx() - 1;
        psSaoLcuParam->m_typeIdx[compIdx] = (int8_t)typeIdx;
    }

    if (typeIdx < 0)
        return;

    int32_t bitDepth = compIdx ? m_pSeqParamSet->bit_depth_chroma : m_pSeqParamSet->bit_depth_luma;
    int32_t offsetTh = (1 << MFX_MIN(bitDepth - 5, 5)) - 1;

    if (typeIdx == SAO_BO)
    {
        // Band offset
        for (int32_t i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            psSaoLcuParam->m_offset[compIdx][i] = parseSaoMaxUvlc(offsetTh);
        }

        for (int32_t i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            if (psSaoLcuParam->m_offset[compIdx][i] != 0)
            {
                int32_t sign = m_pBitStream->DecodeSingleBinEP_CABAC();

                if (sign)
                {
                    psSaoLcuParam->m_offset[compIdx][i] = -psSaoLcuParam->m_offset[compIdx][i];
                }
            }
        }

        psSaoLcuParam->m_subTypeIdx[compIdx] = (int8_t)m_pBitStream->DecodeBypassBins_CABAC(5);
    }
    else if (typeIdx < 4)
    {
        // Edge offsets
        psSaoLcuParam->m_offset[compIdx][0] = parseSaoMaxUvlc(offsetTh);
        psSaoLcuParam->m_offset[compIdx][1] = parseSaoMaxUvlc(offsetTh);
        psSaoLcuParam->m_offset[compIdx][2] = - parseSaoMaxUvlc(offsetTh);
        psSaoLcuParam->m_offset[compIdx][3] = - parseSaoMaxUvlc(offsetTh);

        if (compIdx < 2)
        {
            psSaoLcuParam->m_subTypeIdx[compIdx] = (int8_t)m_pBitStream->DecodeBypassBins_CABAC(2);
            psSaoLcuParam->m_typeIdx[compIdx] += psSaoLcuParam->m_subTypeIdx[compIdx];
        }
    }
}

// Decode CTB SAO information
void H265SegmentDecoder::DecodeSAOOneLCU()
{
    if (m_pSliceHeader->slice_sao_luma_flag || m_pSliceHeader->slice_sao_chroma_flag)
    {
        uint32_t curCUAddr = m_cu->CUAddr;
        int32_t numCuInWidth  = m_pCurrentFrame->m_CodingData->m_WidthInCU;
        int32_t sliceStartRS = m_pCurrentFrame->m_CodingData->getCUOrderMap(m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->getCD()->getNumPartInCU());
        int32_t cuAddrLeftInSlice = curCUAddr - 1;
        int32_t cuAddrUpInSlice  = curCUAddr - numCuInWidth;
        int32_t rx = curCUAddr % numCuInWidth;
        int32_t ry = curCUAddr / numCuInWidth;
        int32_t sao_merge_left_flag = 0;
        int32_t sao_merge_up_flag   = 0;

        if (rx > 0)
        {
            if (cuAddrLeftInSlice >= sliceStartRS &&
                m_pCurrentFrame->m_CodingData->getTileIdxMap(cuAddrLeftInSlice) == m_pCurrentFrame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                sao_merge_left_flag = 1;
            }
        }

        if (ry > 0)
        {
            if (cuAddrUpInSlice >= sliceStartRS &&
                m_pCurrentFrame->m_CodingData->getTileIdxMap(cuAddrUpInSlice) == m_pCurrentFrame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                sao_merge_up_flag = 1;
            }
        }

        parseSaoOneLcuInterleaving(m_pSliceHeader->slice_sao_luma_flag != 0, m_pSliceHeader->slice_sao_chroma_flag != 0, sao_merge_left_flag, sao_merge_up_flag);
    }
}

// Parse merge flags and offsets if needed
void H265SegmentDecoder::parseSaoOneLcuInterleaving(bool saoLuma,
                                                    bool saoChroma,
                                                    int32_t sao_merge_left_flag,
                                                    int32_t sao_merge_up_flag)
{
    int32_t iAddr = m_cu->CUAddr;

    SAOLCUParam* saoLcuParam = m_pCurrentFrame->getCD()->m_saoLcuParam;

    sao_merge_left_flag = sao_merge_left_flag ? m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]) : 0;
    sao_merge_up_flag = (sao_merge_up_flag && !sao_merge_left_flag) ? m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]) : 0;

    for (int32_t iCompIdx = 0; iCompIdx < (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400 ? 3 : 1); iCompIdx++)
    {
        if ((iCompIdx == 0 && saoLuma) || (iCompIdx > 0 && saoChroma))
        {
            if (!sao_merge_left_flag && !sao_merge_up_flag)
            {
                parseSaoOffset(&saoLcuParam[iAddr], iCompIdx);
            }
            else
            {
                if (sao_merge_left_flag)
                    saoLcuParam[iAddr] = saoLcuParam[iAddr - 1];
                else
                    saoLcuParam[iAddr] = saoLcuParam[iAddr - m_pCurrentFrame->m_CodingData->m_WidthInCU];
            }

            saoLcuParam[iAddr].sao_merge_left_flag = sao_merge_left_flag;
            saoLcuParam[iAddr].sao_merge_up_flag = sao_merge_up_flag;
        }
        else
        {
            if (iCompIdx < 2)
            {
                saoLcuParam[iAddr].m_typeIdx[iCompIdx] = -1;
                saoLcuParam[iAddr].m_subTypeIdx[iCompIdx] = 0;
            }
        }
    }
}

// Parse truncated rice symbol. HEVC spec 9.3.3.2
void H265SegmentDecoder::ReadUnaryMaxSymbolCABAC(uint32_t& uVal, uint32_t CtxIdx, int32_t Offset, uint32_t MaxSymbol)
{
    if (0 == MaxSymbol)
    {
        uVal = 0;
        return;
    }

    uVal = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    if (uVal == 0 || MaxSymbol == 1)
    {
        return;
    }

    uint32_t val = 0;
    uint32_t cont;

    do
    {
        cont = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx + Offset);
        val++;
    }
    while( cont && ( val < MaxSymbol - 1 ) );

    if ( cont && ( val == MaxSymbol - 1 ) )
    {
        val++;
    }

    uVal = val;
}

// Decode CU recursively
void H265SegmentDecoder::DecodeCUCABAC(uint32_t AbsPartIdx, uint32_t Depth, uint32_t& IsLast)
{
    uint32_t CurNumParts = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    uint32_t QNumParts = CurNumParts >> 2;

    uint32_t more_depth = 0;

    uint32_t LPelX = m_cu->m_rasterToPelX[AbsPartIdx];
    int32_t PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    LPelX += m_cu->m_CUPelX;
    uint32_t RPelX = LPelX + (m_pCurrentFrame->getCD()->m_MaxCUWidth >> Depth) - 1;
    uint32_t TPelY = m_cu->m_rasterToPelY[AbsPartIdx];
    int32_t PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    TPelY += m_cu->m_CUPelY;
    uint32_t BPelY = TPelY + (m_pCurrentFrame->getCD()->m_MaxCUWidth >> Depth) - 1;
    int32_t PartSize = m_pSeqParamSet->MaxCUSize >> (Depth + m_pSeqParamSet->log2_min_transform_block_size);

    // Check for split implicit and explicit
    if (RPelX < m_pSeqParamSet->pic_width_in_luma_samples && BPelY < m_pSeqParamSet->pic_height_in_luma_samples)
    {
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)
            m_cu->setDepth(Depth, AbsPartIdx);
        else
        {
            more_depth = DecodeSplitFlagCABAC(PartX, PartY, Depth);
            m_cu->setDepth(Depth + more_depth, AbsPartIdx);
        }
    }
    else
    {
        more_depth = 1;
    }

    if (m_pSliceHeader->cu_chroma_qp_offset_enabled_flag && (m_pSeqParamSet->MaxCUSize >> Depth) >= (m_pSeqParamSet->MaxCUSize >> m_pPicParamSet->diff_cu_chroma_qp_offset_depth))
        m_IsCuChromaQpOffsetCoded = false;

    if (more_depth)
    {
        uint32_t Idx = AbsPartIdx;
        if ((m_pSeqParamSet->MaxCUSize >> Depth) == m_minCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            m_DecodeDQPFlag = true;
            m_context->SetNewQP(getRefQP(AbsPartIdx));
        }

        for ( uint32_t PartUnitIdx = 0; PartUnitIdx < 4; PartUnitIdx++ )
        {
            LPelX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[Idx];
            TPelY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[Idx];

            if (!IsLast && (LPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (TPelY < m_pSeqParamSet->pic_height_in_luma_samples))
            {
                DecodeCUCABAC(Idx, Depth + 1, IsLast);
            }
            else
            {
                m_cu->setOutsideCUPart(Idx, Depth + 1);
            }

            Idx += QNumParts;
        }

        if (m_pPicParamSet->cu_qp_delta_enabled_flag && (m_pSeqParamSet->MaxCUSize >> Depth) == m_minCUDQPSize)
        {
            if (m_DecodeDQPFlag)
            {
                m_context->SetNewQP(getRefQP(AbsPartIdx));
            }
        }
        return;
    }

    if (m_pPicParamSet->cu_qp_delta_enabled_flag && (m_pSeqParamSet->MaxCUSize >> Depth) >= m_minCUDQPSize)
    {
        m_DecodeDQPFlag = true;
        m_context->SetNewQP(getRefQP(AbsPartIdx));
    }

    bool transquant_bypass = false;
    if (m_pPicParamSet->transquant_bypass_enabled_flag)
        transquant_bypass = DecodeCUTransquantBypassFlag(AbsPartIdx);

    bool skipped = false;
    if (m_pSliceHeader->slice_type != I_SLICE)
        skipped = DecodeSkipFlagCABAC(PartX, PartY);

    if (skipped)
    {
        // Handle skip
        m_cu->setPredMode(MODE_INTER, AbsPartIdx);
        m_cu->setPartSizeSubParts(PART_SIZE_2Nx2N, AbsPartIdx);
        m_cu->setSize(m_context->m_sps->MaxCUSize >> Depth, AbsPartIdx);
        m_cu->setIPCMFlag(false, AbsPartIdx);

        H265MVInfo MvBufferNeighbours[MERGE_MAX_NUM_CAND]; // double length for mv of both lists
        uint8_t InterDirNeighbours[MERGE_MAX_NUM_CAND];

        for (int32_t ui = 0; ui < m_cu->m_SliceHeader->max_num_merge_cand; ++ui)
        {
            InterDirNeighbours[ui] = 0;
        }
        int32_t MergeIndex = DecodeMergeIndexCABAC();
        getInterMergeCandidates(AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, MergeIndex);

        H265MVInfo &MVi = MvBufferNeighbours[MergeIndex];
        for (uint32_t RefListIdx = 0; RefListIdx < 2; RefListIdx++)
        {
            if (0 == m_cu->m_SliceHeader->m_numRefIdx[RefListIdx])
            {
                MVi.m_refIdx[RefListIdx] = -1;
            }
        }

        UpdatePUInfo(LPelX >> m_pSeqParamSet->log2_min_transform_block_size, TPelY >> m_pSeqParamSet->log2_min_transform_block_size, PartSize, PartSize, MVi);

        m_cu->setCbfSubParts(0, 0, 0, AbsPartIdx, Depth);
        if (m_pPicParamSet->cu_qp_delta_enabled_flag)
            m_context->SetNewQP(m_DecodeDQPFlag ? getRefQP(AbsPartIdx) : m_cu->m_CodedQP);

        BeforeCoeffs(AbsPartIdx, Depth);
        FinishDecodeCU(AbsPartIdx, Depth, IsLast);
        UpdateNeighborBuffers(AbsPartIdx, Depth, true);
        return;
    }

    int32_t PredMode = DecodePredModeCABAC(AbsPartIdx);
    DecodePartSizeCABAC(AbsPartIdx, Depth);

    bool isFirstPartMerge = false;

    if (MODE_INTRA == PredMode)
    {
        if (PART_SIZE_2Nx2N == m_cu->GetPartitionSize(AbsPartIdx))
        {
            DecodeIPCMInfoCABAC(AbsPartIdx, Depth);

            if (m_cu->GetIPCMFlag(AbsPartIdx))
            {
                if (m_pPicParamSet->cu_qp_delta_enabled_flag)
                    m_context->SetNewQP(m_DecodeDQPFlag ? getRefQP(AbsPartIdx) : m_cu->m_CodedQP);

                BeforeCoeffs(AbsPartIdx, Depth);
                FinishDecodeCU(AbsPartIdx, Depth, IsLast);
                UpdateNeighborBuffers(AbsPartIdx, Depth, false);
                return;
            }
        }
        else
            m_cu->setIPCMFlag(false, AbsPartIdx);

        BeforeCoeffs(AbsPartIdx, Depth);
        DecodeIntraDirLumaAngCABAC(AbsPartIdx, Depth);
        if (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400)
            DecodeIntraDirChromaCABAC(AbsPartIdx, Depth);
    }
    else
    {
        m_cu->setIPCMFlag(false, AbsPartIdx);
        BeforeCoeffs(AbsPartIdx, Depth);
        isFirstPartMerge = DecodePUWiseCABAC(AbsPartIdx, Depth);
    }

    if (m_pPicParamSet->cu_qp_delta_enabled_flag)
        m_context->SetNewQP(m_DecodeDQPFlag ? getRefQP(AbsPartIdx) : m_cu->m_CodedQP);

    // Coefficient decoding
    DecodeCoeff(AbsPartIdx, Depth, m_DecodeDQPFlag, isFirstPartMerge);
    FinishDecodeCU(AbsPartIdx, Depth, IsLast);
}

// Decode CU split flag
bool H265SegmentDecoder::DecodeSplitFlagCABAC(int32_t PartX, int32_t PartY, uint32_t Depth)
{
    uint32_t uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SPLIT_CODING_UNIT_FLAG_HEVC] + getCtxSplitFlag(PartX, PartY, Depth));

    return uVal != 0;
}

// Decode CU transquant bypass flag
bool H265SegmentDecoder::DecodeCUTransquantBypassFlag(uint32_t AbsPartIdx)
{
    uint32_t uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANSQUANT_BYPASS_HEVC]);
    m_cu->setCUTransquantBypass(uVal ? true : false, AbsPartIdx);
    return uVal ? true : false;
}

// Decode CU skip flag
bool H265SegmentDecoder::DecodeSkipFlagCABAC(int32_t PartX, int32_t PartY)
{
    if (I_SLICE == m_pSliceHeader->slice_type)
    {
        return false;
    }

    uint32_t uVal = 0;
    uint32_t CtxSkip = getCtxSkipFlag(PartX, PartY);
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SKIP_FLAG_HEVC] + CtxSkip);

    return uVal != 0;
}

// Decode inter PU merge index
uint32_t H265SegmentDecoder::DecodeMergeIndexCABAC(void)
{
    uint32_t NumCand = MERGE_MAX_NUM_CAND;
    uint32_t UnaryIdx = 0;
    NumCand = m_pSliceHeader->max_num_merge_cand;

    if (NumCand >= 1)
    {
        for ( ; UnaryIdx < NumCand - 1; ++UnaryIdx)
        {
            uint32_t uVal = 0;

            if (UnaryIdx == 0)
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_IDX_HEVC]);
            else
                uVal = m_pBitStream->DecodeSingleBinEP_CABAC();

            if (0 == uVal)
                break;
        }
    }

    return UnaryIdx;
}

// Decode inter PU MV predictor index
void H265SegmentDecoder::DecodeMVPIdxPUCABAC(uint32_t AbsPartAddr, uint32_t PartIdx, EnumRefPicList RefList, H265MVInfo &MVi, uint8_t InterDir)
{
    uint32_t MVPIdx = 0;
    AMVPInfo AMVPInfo;

    if (InterDir & (1 << RefList))
    {
        VM_ASSERT(MVi.m_refIdx[RefList] >= 0);
        ReadUnaryMaxSymbolCABAC(MVPIdx, ctxIdxOffsetHEVC[MVP_IDX_HEVC], 1, AMVP_MAX_NUM_CAND - 1);
    }
    else
    {
        MVi.m_refIdx[RefList] = -1;
        return;
    }

    fillMVPCand(AbsPartAddr, PartIdx, RefList, MVi.m_refIdx[RefList], &AMVPInfo);

    if (MVi.m_refIdx[RefList] >= 0)
        MVi.m_mv[RefList] = AMVPInfo.MVCandidate[MVPIdx] + MVi.m_mv[RefList];
}

// Decode CU prediction mode
int32_t H265SegmentDecoder::DecodePredModeCABAC(uint32_t AbsPartIdx)
{
    int32_t PredMode = MODE_INTER;

    if (I_SLICE == m_cu->m_SliceHeader->slice_type)
    {
        PredMode = MODE_INTRA;
    }
    else
    {
        uint32_t uVal;
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PRED_MODE_HEVC]);
        PredMode += uVal;
    }

    m_cu->setPredMode(EnumPredMode(PredMode), AbsPartIdx);
    return PredMode;
}

// Decode partition size. HEVC spec 9.3.3.5
void H265SegmentDecoder::DecodePartSizeCABAC(uint32_t AbsPartIdx, uint32_t Depth)
{
    uint32_t uVal, uMode = 0;
    EnumPartSize Mode;

    if (MODE_INTRA == m_cu->GetPredictionMode(AbsPartIdx))
    {
        uVal = 1;
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PART_SIZE_HEVC]);
        }

        Mode = uVal ? PART_SIZE_2Nx2N : PART_SIZE_NxN;
        m_cu->setSize(m_pSeqParamSet->MaxCUSize >> Depth, AbsPartIdx);

        uint32_t TrLevel = 0;
        uint32_t WidthInBit  = g_ConvertToBit[m_cu->GetWidth(AbsPartIdx)] + 2;
        uint32_t TrSizeInBit = g_ConvertToBit[m_pSeqParamSet->m_maxTrSize] + 2;
        TrLevel            = WidthInBit >= TrSizeInBit ? WidthInBit - TrSizeInBit : 0;
        if (Mode == PART_SIZE_NxN)
        {
            m_cu->setTrIdx(1 + TrLevel, AbsPartIdx, Depth);
        }
        else
        {
            m_cu->setTrIdx(TrLevel, AbsPartIdx, Depth);
        }
    }
    else
    {
        uint32_t MaxNumBits = 2;
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth && !((m_pSeqParamSet->MaxCUSize >> Depth) == 8))
        {
            MaxNumBits++;
        }
        for (uint32_t ui = 0; ui < MaxNumBits; ui++)
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PART_SIZE_HEVC] + ui);
            if (uVal)
            {
                break;
            }
            uMode++;
        }
        Mode = (EnumPartSize) uMode;

        if (m_pSeqParamSet->m_AMPAcc[Depth])
        {
            if (Mode == PART_SIZE_2NxN)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? PART_SIZE_2NxnU : PART_SIZE_2NxnD);
                }
            }
            else if (Mode == PART_SIZE_Nx2N)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? PART_SIZE_nLx2N : PART_SIZE_nRx2N);
                }
            }
        }

        m_cu->setSize(m_pSeqParamSet->MaxCUSize >> Depth, AbsPartIdx);
    }
    m_cu->setPartSizeSubParts(Mode, AbsPartIdx);
}

// Decode IPCM CU flag and samples
void H265SegmentDecoder::DecodeIPCMInfoCABAC(uint32_t AbsPartIdx, uint32_t Depth)
{
    if (!m_pSeqParamSet->pcm_enabled_flag
        || (m_cu->GetWidth(AbsPartIdx) > (1 << m_pSeqParamSet->log2_max_pcm_luma_coding_block_size))
        || (m_cu->GetWidth(AbsPartIdx) < (1 << m_pSeqParamSet->log2_min_pcm_luma_coding_block_size)))
    {
        m_cu->setIPCMFlag(false, AbsPartIdx);
        return;
    }

    uint32_t uVal;

    uVal = m_pBitStream->DecodeTerminatingBit_CABAC();
    m_cu->setIPCMFlag(uVal != 0, AbsPartIdx);

    if (!uVal)
        return;

    DecodePCMAlignBits();

    m_cu->setPartSizeSubParts(PART_SIZE_2Nx2N, AbsPartIdx);
    m_cu->setLumaIntraDirSubParts(INTRA_LUMA_DC_IDX, AbsPartIdx, Depth);
    m_cu->setSize(m_pSeqParamSet->MaxCUSize >> Depth, AbsPartIdx);
    m_cu->setTrIdx(0, AbsPartIdx, Depth);

    // LUMA
    uint32_t size = m_cu->GetWidth(AbsPartIdx);

    m_reconstructor->DecodePCMBlock(m_pBitStream, &m_context->m_coeffsWrite, size, m_pSeqParamSet->pcm_sample_bit_depth_luma);

    if (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400)
    {
        size >>= 1;
        m_reconstructor->DecodePCMBlock(m_pBitStream, &m_context->m_coeffsWrite, size, m_pSeqParamSet->pcm_sample_bit_depth_chroma);
        m_reconstructor->DecodePCMBlock(m_pBitStream, &m_context->m_coeffsWrite, size, m_pSeqParamSet->pcm_sample_bit_depth_chroma);
        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
        {
            m_reconstructor->DecodePCMBlock(m_pBitStream, &m_context->m_coeffsWrite, size, m_pSeqParamSet->pcm_sample_bit_depth_chroma);
            m_reconstructor->DecodePCMBlock(m_pBitStream, &m_context->m_coeffsWrite, size, m_pSeqParamSet->pcm_sample_bit_depth_chroma);
        }
    }

    m_pBitStream->ResetBac_CABAC();
}

// Decode PCM alignment zero bits.
void H265SegmentDecoder::DecodePCMAlignBits()
{
    int32_t iVal = m_pBitStream->getNumBitsUntilByteAligned();

    if (iVal)
        m_pBitStream->GetBits(iVal);
}

// Decode luma intra direction
void H265SegmentDecoder::DecodeIntraDirLumaAngCABAC(uint32_t AbsPartIdx, uint32_t Depth)
{
    EnumPartSize mode = m_cu->GetPartitionSize(AbsPartIdx);
    uint32_t PartNum = mode == PART_SIZE_NxN ? 4 : 1;
    uint32_t PartOffset = (m_cu->m_Frame->m_CodingData->m_NumPartitions >> (m_cu->GetDepth(AbsPartIdx) << 1)) >> 2;
    uint32_t prev_intra_luma_pred_flag[4];
    int32_t IPredMode;

    if (mode == PART_SIZE_NxN)
        Depth++;

    // Prev intra luma pred flag
    for (uint32_t j = 0; j < PartNum; j++)
    {
        prev_intra_luma_pred_flag[j] = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_LUMA_PRED_MODE_HEVC]);
    }

    for (uint32_t j = 0; j < PartNum; j++)
    {
        int32_t Preds[3] = {-1, -1, -1};
        uint32_t PredNum = 3;

        getIntraDirLumaPredictor(AbsPartIdx + PartOffset * j, Preds);

        if (prev_intra_luma_pred_flag[j])
        {
            // mpm idx
            uint32_t mpm_idx = m_pBitStream->DecodeSingleBinEP_CABAC();
            if (mpm_idx)
            {
                mpm_idx = m_pBitStream->DecodeSingleBinEP_CABAC();
                mpm_idx++;
            }
            IPredMode = Preds[mpm_idx];
        }
        else
        {
            // Rem intra luma pred mode
            IPredMode = m_pBitStream->DecodeBypassBins_CABAC(5);

            if (Preds[0] > Preds[1])
            {
                std::swap(Preds[0], Preds[1]);
            }
            if (Preds[0] > Preds[2])
            {
                std::swap(Preds[0], Preds[2]);
            }
            if (Preds[1] > Preds[2])
            {
                std::swap(Preds[1], Preds[2]);
            }
            for (uint32_t i = 0; i < PredNum; i++)
            {
                IPredMode += (IPredMode >= Preds[i]);
            }
        }
        m_cu->setLumaIntraDirSubParts(IPredMode, AbsPartIdx + PartOffset * j, Depth);
    }
}

// Decode intra chroma direction. HEVC spec 9.3.3.6
void H265SegmentDecoder::DecodeIntraDirChromaCABAC(uint32_t AbsPartIdx, uint32_t Depth)
{
    uint32_t uVal;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_CHROMA_PRED_MODE_HEVC]);

    //switch codeword
    if (uVal == 0)
    {
        //INTRA_DM_CHROMA_IDX case
        uVal = m_cu->GetLumaIntra(AbsPartIdx);
    }
    else
    {
        uint32_t IPredMode;

        IPredMode = m_pBitStream->DecodeBypassBins_CABAC(2);
        uint32_t AllowedChromaDir[INTRA_NUM_CHROMA_MODE];
        m_cu->getAllowedChromaDir(AbsPartIdx, AllowedChromaDir);
        uVal = AllowedChromaDir[IPredMode];

        if (uVal == INTRA_DM_CHROMA_IDX)
        {
            uVal = m_cu->GetLumaIntra(AbsPartIdx);
        }
    }

    if (uVal >= INTRA_DM_CHROMA_IDX)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
    {
        uVal = g_Chroma422IntraPredModeC[uVal];
    }

    m_cu->setChromIntraDirSubParts(uVal, AbsPartIdx, Depth);
    return;
}

// Decode inter PU information
bool H265SegmentDecoder::DecodePUWiseCABAC(uint32_t AbsPartIdx, uint32_t Depth)
{
    EnumPartSize PartSize = m_cu->GetPartitionSize(AbsPartIdx);
    uint32_t NumPU = (PartSize == PART_SIZE_2Nx2N ? 1 : (PartSize == PART_SIZE_NxN ? 4 : 2));
    uint32_t PUOffset = (g_PUOffset[uint32_t(PartSize)] << ((m_pSeqParamSet->MaxCUDepth - Depth) << 1)) >> 4;

    H265MVInfo MvBufferNeighbours[MERGE_MAX_NUM_CAND]; // double length for mv of both lists
    uint8_t InterDirNeighbours[MERGE_MAX_NUM_CAND];

    for (int32_t ui = 0; ui < m_pSliceHeader->max_num_merge_cand; ui++ )
        InterDirNeighbours[ui] = 0;

    bool isFirstMerge = false;

    for (uint32_t PartIdx = 0, SubPartIdx = AbsPartIdx; PartIdx < NumPU; PartIdx++, SubPartIdx += PUOffset)
    {
        uint8_t InterDir = 0;
        // Decode merge flag
        bool bMergeFlag = DecodeMergeFlagCABAC();
        if (0 == PartIdx)
            isFirstMerge = bMergeFlag;

        int32_t PartWidth, PartHeight;
        m_cu->getPartSize(AbsPartIdx, PartIdx, PartWidth, PartHeight);

        int32_t LPelX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[SubPartIdx];
        int32_t TPelY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[SubPartIdx];
        int32_t PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
        int32_t PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
        PartWidth >>= m_pSeqParamSet->log2_min_transform_block_size;
        PartHeight >>= m_pSeqParamSet->log2_min_transform_block_size;

        H265MVInfo MVi;
        MVi.m_refIdx[REF_PIC_LIST_0] = MVi.m_refIdx[REF_PIC_LIST_1] = -1;

        if (bMergeFlag)
        {
            // Merge branch
            uint32_t MergeIndex = DecodeMergeIndexCABAC();

            if ((m_pPicParamSet->log2_parallel_merge_level - 2) > 0 && PartSize != PART_SIZE_2Nx2N && m_cu->GetWidth(AbsPartIdx) <= 8)
            {
                m_cu->setPartSizeSubParts(PART_SIZE_2Nx2N, AbsPartIdx);
                getInterMergeCandidates(AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, MergeIndex);
                m_cu->setPartSizeSubParts(PartSize, AbsPartIdx);
            }
            else
            {
                getInterMergeCandidates(SubPartIdx, PartIdx, MvBufferNeighbours, InterDirNeighbours, MergeIndex);
            }
            InterDir = InterDirNeighbours[MergeIndex];

            for (uint32_t RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0)
                {
                    H265MVInfo &mvb = MvBufferNeighbours[MergeIndex];
                    if (mvb.m_refIdx[RefListIdx] >= 0)
                        MVi.setMVInfo(RefListIdx, mvb.m_refIdx[RefListIdx], mvb.m_mv[RefListIdx]);
                }
            }
        }
        else
        {
            // AMVP branch
            InterDir = DecodeInterDirPUCABAC(SubPartIdx);
            for (uint32_t RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0)
                {
                    MVi.m_refIdx[RefListIdx] = DecodeRefFrmIdxPUCABAC(EnumRefPicList(RefListIdx), InterDir);
                    DecodeMVdPUCABAC(EnumRefPicList(RefListIdx), MVi.m_mv[RefListIdx], InterDir);
                    DecodeMVPIdxPUCABAC(SubPartIdx, PartIdx, EnumRefPicList(RefListIdx), MVi, InterDir);
                }
            }
        }

        if ((InterDir == 3) && m_cu->isBipredRestriction(AbsPartIdx, PartIdx))
            MVi.m_refIdx[REF_PIC_LIST_1] = -1;

        // Fill up local context and colocated lookup map
        UpdatePUInfo(PartX, PartY, PartWidth, PartHeight, MVi);
    }

    return isFirstMerge;
}

// Decode merge flag
bool H265SegmentDecoder::DecodeMergeFlagCABAC(void)
{
    uint32_t uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_FLAG_HEVC]);
    return uVal ? true : false;
}

// Decode inter direction for a PU block. HEVC spec 9.3.3.7
uint8_t H265SegmentDecoder::DecodeInterDirPUCABAC(uint32_t AbsPartIdx)
{
    uint8_t InterDir;

    if (P_SLICE == m_pSliceHeader->slice_type)
    {
        InterDir = 1;
    }
    else
    {
        uint32_t uVal;
        uint32_t Ctx = m_cu->GetDepth(AbsPartIdx);

        uVal = 0;
        if (m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N || m_cu->GetWidth(AbsPartIdx) != 8)
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTER_DIR_HEVC] + Ctx);

        if (uVal)
        {
            uVal = 2;
        }
        else
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTER_DIR_HEVC] + 4);
            VM_ASSERT(uVal == 0 || uVal == 1);
        }

        uVal++;
        InterDir = uint8_t(uVal);
    }

    return InterDir;
}

// Decode truncated rice reference frame index for AMVP PU
RefIndexType H265SegmentDecoder::DecodeRefFrmIdxPUCABAC(EnumRefPicList RefList, uint8_t InterDir)
{
    RefIndexType RefFrmIdx;
    int32_t ParseRefFrmIdx = InterDir & (1 << RefList);

    if (m_pSliceHeader->m_numRefIdx[RefList] > 1 && ParseRefFrmIdx)
    {
        uint32_t uVal;
        uint32_t Ctx = 0;

        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[REF_FRAME_IDX_HEVC] + Ctx);
        if (uVal)
        {
            uint32_t RefNum = m_pSliceHeader->m_numRefIdx[RefList] - 2;
            Ctx++;
            uint32_t i;
            for(i = 0; i < RefNum; ++i)
            {
                if(i == 0)
                    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[REF_FRAME_IDX_HEVC] + Ctx);
                else
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();

                if(uVal == 0)
                    break;
            }
            uVal = i + 1;
        }
        RefFrmIdx = RefIndexType(uVal);
    }
    else if (!ParseRefFrmIdx)
        RefFrmIdx = NOT_VALID;
    else
        RefFrmIdx = 0;

    return RefFrmIdx;
}

// Decode MV delta. HEVC spec 7.3.8.9
void H265SegmentDecoder::DecodeMVdPUCABAC(EnumRefPicList RefList, H265MotionVector &MVd, uint8_t InterDir)
{
    if (InterDir & (1 << RefList))
    {
        uint32_t uVal;
        int32_t HorAbs, VerAbs;
        uint32_t HorSign = 0, VerSign = 0;

        if (m_pSliceHeader->mvd_l1_zero_flag && RefList == REF_PIC_LIST_1 && InterDir == 3)
        {
            HorAbs = 0;
            VerAbs = 0;
        }
        else
        {
            HorAbs = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC]);
            VerAbs = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC]);

            const bool HorAbsGr0 = HorAbs != 0;
            const bool VerAbsGr0 = VerAbs != 0;
            uint32_t Ctx = 1;

            if(HorAbsGr0)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC] + Ctx);
                HorAbs += uVal;
            }

            if(VerAbsGr0)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC] + Ctx);
                VerAbs += uVal;
            }

            if(HorAbsGr0)
            {
                if(2 == HorAbs)
                {
                    ReadEpExGolombCABAC(uVal, 1);
                    HorAbs += uVal;
                }

                HorSign = m_pBitStream->DecodeSingleBinEP_CABAC();
            }

            if(VerAbsGr0)
            {
                if(2 == VerAbs)
                {
                    ReadEpExGolombCABAC(uVal, 1);
                    VerAbs += uVal;
                }

                VerSign = m_pBitStream->DecodeSingleBinEP_CABAC();
            }
        }

        if (HorSign)
            HorAbs = -HorAbs;
        if (VerSign)
            VerAbs = -VerAbs;

        MVd.Horizontal = (int16_t)HorAbs;
        MVd.Vertical = (int16_t)VerAbs;
    }
}

// Decode EG1 coded abs_mvd_minus2 values
void H265SegmentDecoder::ReadEpExGolombCABAC(uint32_t& Value, uint32_t Count)
{
    uint32_t uVal = 0;
    uint32_t uBit = 1;

    while (uBit)
    {
        uBit = m_pBitStream->DecodeSingleBinEP_CABAC();
        uVal += uBit << Count++;
    }

    if (--Count)
    {
        uint32_t bins;
        bins = m_pBitStream->DecodeBypassBins_CABAC(Count);
        uVal += bins;
    }

    Value = uVal;

    return;
}

// Decode all CU coefficients
void H265SegmentDecoder::DecodeCoeff(uint32_t AbsPartIdx, uint32_t Depth, bool& CodeDQP, bool isFirstPartMerge)
{
    if (MODE_INTRA != m_cu->GetPredictionMode(AbsPartIdx))
    {
        uint32_t QtRootCbf = 1;
        // 2Nx2N merge cannot have a zero root CBF because in that case it should be skip CU
        if (!(m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N && isFirstPartMerge))
        {
            ParseQtRootCbfCABAC(QtRootCbf);
        }
        if (!QtRootCbf)
        {
            m_cu->setCbfSubParts(0, 0, 0, AbsPartIdx, Depth);
            m_cu->setTrIdx(0 , AbsPartIdx, Depth);
            UpdateNeighborBuffers(AbsPartIdx, Depth, false);
            return;
        }
    }

    const uint32_t Log2TrafoSize = m_pSeqParamSet->log2_max_luma_coding_block_size - Depth;

    m_bakAbsPartIdxQp = AbsPartIdx;
    DecodeTransform(AbsPartIdx, Depth, Log2TrafoSize, 0, CodeDQP);
}

// Recursively decode TU data
void H265SegmentDecoder::DecodeTransform(uint32_t AbsPartIdx, uint32_t Depth, uint32_t  log2TrafoSize, uint32_t trafoDepth, bool& CodeDQP)
{
    uint32_t Subdiv;

    if (log2TrafoSize == 2)
    {
        if ((AbsPartIdx & 0x03) == 0)
        {
            m_BakAbsPartIdxChroma = AbsPartIdx;
        }
    }

    int32_t IntraSplitFlag = m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA && m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_NxN;

    uint32_t Log2MaxTrafoSize = m_pSeqParamSet->log2_max_transform_block_size;
    uint32_t Log2MinTrafoSize = m_pSeqParamSet->log2_min_transform_block_size;

    uint32_t MaxTrafoDepth = m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? (m_pSeqParamSet->max_transform_hierarchy_depth_intra + IntraSplitFlag ) : m_pSeqParamSet->max_transform_hierarchy_depth_inter;

    // Check for implicit or explicit TU split
    if (log2TrafoSize <= Log2MaxTrafoSize && log2TrafoSize > Log2MinTrafoSize && trafoDepth < MaxTrafoDepth && !(IntraSplitFlag && (trafoDepth == 0)) )
    {
        VM_ASSERT(log2TrafoSize > m_cu->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
        ParseTransformSubdivFlagCABAC(Subdiv, 5 - log2TrafoSize);
    }
    else
    {
        int32_t interSplitFlag = m_pSeqParamSet->max_transform_hierarchy_depth_inter == 0 && m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTER &&
            m_cu->GetPartitionSize(AbsPartIdx) != PART_SIZE_2Nx2N && trafoDepth == 0;

        Subdiv = ((IntraSplitFlag && trafoDepth == 0) || interSplitFlag || log2TrafoSize > Log2MaxTrafoSize) ? 1 : 0;
    }

    const bool FirstCbfOfCUFlag = trafoDepth == 0;
    if (FirstCbfOfCUFlag)
    {
        m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_U, AbsPartIdx, Depth);
        m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_V, AbsPartIdx, Depth);

        if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
        {
            m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_U1, AbsPartIdx, Depth);
            m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_V1, AbsPartIdx, Depth);
        }
    }

    if (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400)
    {
        if (FirstCbfOfCUFlag || log2TrafoSize > 2)
        {
            if (FirstCbfOfCUFlag || m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, trafoDepth - 1))
            {
                ParseQtCbfCABAC(AbsPartIdx, COMPONENT_CHROMA_U, trafoDepth, Depth);
                if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 && (!Subdiv || log2TrafoSize == 3))
                {
                    ParseQtCbfCABAC(AbsPartIdx, COMPONENT_CHROMA_U1, trafoDepth, Depth);
                }
            }

            if (FirstCbfOfCUFlag || m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, trafoDepth - 1))
            {
                ParseQtCbfCABAC(AbsPartIdx, COMPONENT_CHROMA_V, trafoDepth, Depth);
                if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 && (!Subdiv || log2TrafoSize == 3))
                {
                    ParseQtCbfCABAC(AbsPartIdx , COMPONENT_CHROMA_V1, trafoDepth, Depth);
                }
            }
        }
        else
        {
            m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, trafoDepth - 1) << trafoDepth, COMPONENT_CHROMA_U, AbsPartIdx, Depth);
            m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, trafoDepth - 1) << trafoDepth, COMPONENT_CHROMA_V, AbsPartIdx, Depth);
            if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
            {
                m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx, trafoDepth - 1) << trafoDepth, COMPONENT_CHROMA_U1, AbsPartIdx, Depth);
                m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx, trafoDepth - 1) << trafoDepth, COMPONENT_CHROMA_V1, AbsPartIdx, Depth);
            }
        }
    }

    if (Subdiv)
    {
        log2TrafoSize--; Depth++;

        const uint32_t QPartNum = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
        const uint32_t StartAbsPartIdx = AbsPartIdx;
        uint8_t YCbf = 0;
        uint8_t UCbf = 0;
        uint8_t VCbf = 0;
        uint8_t U1Cbf = 0;
        uint8_t V1Cbf = 0;

        // Recursively parse sub-TUs
        for (int32_t i = 0; i < 4; i++)
        {
            DecodeTransform(AbsPartIdx, Depth, log2TrafoSize, trafoDepth+1, CodeDQP);

            YCbf |= m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx, trafoDepth + 1);
            UCbf |= m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, trafoDepth + 1);
            VCbf |= m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, trafoDepth + 1);
            if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
            {
                U1Cbf |= m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx, trafoDepth + 1);
                V1Cbf |= m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx, trafoDepth + 1);
            }

            AbsPartIdx += QPartNum;
        }

        for (uint32_t ui = 0; ui < 4 * QPartNum; ++ui)
        {
            m_cu->GetCbf(COMPONENT_LUMA)[StartAbsPartIdx + ui] |= YCbf << trafoDepth;
            m_cu->GetCbf(COMPONENT_CHROMA_U)[StartAbsPartIdx + ui] |= (UCbf) << trafoDepth;
            m_cu->GetCbf(COMPONENT_CHROMA_V)[StartAbsPartIdx + ui] |= (VCbf) << trafoDepth;
            if (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422)
            {
                m_cu->GetCbf(COMPONENT_CHROMA_U1)[StartAbsPartIdx + ui] |= (U1Cbf) << trafoDepth;
                m_cu->GetCbf(COMPONENT_CHROMA_V1)[StartAbsPartIdx + ui] |= (V1Cbf) << trafoDepth;
            }
        }
    }
    else
    {
        VM_ASSERT(Depth >= m_cu->GetDepth(AbsPartIdx));
        if (m_cu->GetPredictionMode(AbsPartIdx) != MODE_INTRA && trafoDepth == 0 && !m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx) && !m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx) &&
            !(m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 && (m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx))))
        {
            m_cu->setCbfSubParts( 1 << trafoDepth, COMPONENT_LUMA, AbsPartIdx, Depth);
        }
        else
        {
            ParseQtCbfCABAC(AbsPartIdx, COMPONENT_LUMA, trafoDepth, Depth);
        }

        // transform_unit begin
        uint8_t cbfY = m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx, trafoDepth);
        uint8_t cbfU;
        uint8_t cbfV;
        uint8_t cbfU1;
        uint8_t cbfV1;

        if (log2TrafoSize == 2 && (AbsPartIdx & 0x3) == 0x3)
        {
            cbfU = m_cu->GetCbf(COMPONENT_CHROMA_U, m_BakAbsPartIdxChroma, trafoDepth);
            cbfV = m_cu->GetCbf(COMPONENT_CHROMA_V, m_BakAbsPartIdxChroma, trafoDepth);
            cbfU1 = (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422) ? m_cu->GetCbf(COMPONENT_CHROMA_U1, m_BakAbsPartIdxChroma, trafoDepth) : 0;
            cbfV1 = (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422) ? m_cu->GetCbf(COMPONENT_CHROMA_V1, m_BakAbsPartIdxChroma, trafoDepth) : 0;
        }
        else
        {
            cbfU = m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, trafoDepth);
            cbfV = m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, trafoDepth);
            cbfU1 = (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422) ? m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx, trafoDepth) : 0;
            cbfV1 = (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422) ? m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx, trafoDepth) : 0;
        }

        bool cbrChroma = cbfU || cbfV || cbfU1 || cbfV1;
        if (cbfY || cbrChroma)
        {
            // Update TU QP value
            if (m_pPicParamSet->cu_qp_delta_enabled_flag && CodeDQP)
            {
                DecodeQP(m_bakAbsPartIdxQp);
                // Change QP recorded in CU block of local context to a new value
                UpdateNeighborDecodedQP(m_bakAbsPartIdxQp, m_cu->GetDepth(AbsPartIdx));
                m_cu->UpdateTUQpInfo(m_bakAbsPartIdxQp, m_context->GetQP(), m_cu->GetDepth(AbsPartIdx));
                CodeDQP = false;
            }

            if (cbrChroma && !m_cu->GetCUTransquantBypass(AbsPartIdx) && m_pSliceHeader->cu_chroma_qp_offset_enabled_flag && !m_IsCuChromaQpOffsetCoded)
            {
                DecodeQPChromaAdujst();
                m_IsCuChromaQpOffsetCoded = true;
            }
        }

        m_cu->setTrIdx(trafoDepth, AbsPartIdx, Depth);

        // At this place all necessary TU data is known, so store it in local context
        UpdateNeighborBuffers(AbsPartIdx, Depth, false);

        uint32_t coeffSize = 1 << (log2TrafoSize << 1);

        // Parse luma coefficients
        if (cbfY)
        {
            ParseCoeffNxNCABAC(m_context->m_coeffsWrite, AbsPartIdx, log2TrafoSize, COMPONENT_LUMA);
            m_context->m_coeffsWrite += coeffSize;
        }

        // Parse chroma coefficients
        if (log2TrafoSize > 2)
        {
            coeffSize = 1 << ((log2TrafoSize - 1) << 1);

            if (cbfU)
            {
                ParseCoeffNxNCABAC(m_context->m_coeffsWrite, AbsPartIdx, log2TrafoSize-1, COMPONENT_CHROMA_U);
                m_context->m_coeffsWrite += coeffSize;
            }

            if (cbfU1)
            {
                ParseCoeffNxNCABAC(m_context->m_coeffsWrite, AbsPartIdx, log2TrafoSize-1, COMPONENT_CHROMA_U1);
                m_context->m_coeffsWrite += coeffSize;
            }

            if (cbfV)
            {
                ParseCoeffNxNCABAC(m_context->m_coeffsWrite, AbsPartIdx, log2TrafoSize-1, COMPONENT_CHROMA_V);
                m_context->m_coeffsWrite += coeffSize;
            }

            if (cbfV1)
            {
                ParseCoeffNxNCABAC(m_context->m_coeffsWrite, AbsPartIdx, log2TrafoSize-1, COMPONENT_CHROMA_V1);
                m_context->m_coeffsWrite += coeffSize;
            }

        }
        else
        {
            if ( (AbsPartIdx & 0x3) == 0x3 )
            {
                if (cbfU)
                {
                    ParseCoeffNxNCABAC(m_context->m_coeffsWrite, m_BakAbsPartIdxChroma, log2TrafoSize, COMPONENT_CHROMA_U);
                    m_context->m_coeffsWrite += coeffSize;
                }

                if (cbfU1)
                {
                    ParseCoeffNxNCABAC(m_context->m_coeffsWrite, m_BakAbsPartIdxChroma, log2TrafoSize, COMPONENT_CHROMA_U1);
                    m_context->m_coeffsWrite += coeffSize;
                }

                if (cbfV)
                {
                    ParseCoeffNxNCABAC(m_context->m_coeffsWrite, m_BakAbsPartIdxChroma, log2TrafoSize, COMPONENT_CHROMA_V);
                    m_context->m_coeffsWrite += coeffSize;
                }
                if (cbfV1)
                {
                    ParseCoeffNxNCABAC(m_context->m_coeffsWrite, m_BakAbsPartIdxChroma, log2TrafoSize, COMPONENT_CHROMA_V1);
                    m_context->m_coeffsWrite += coeffSize;
                }
            }
        }
    }
}

// Decode explicit TU split
void H265SegmentDecoder::ParseTransformSubdivFlagCABAC(uint32_t& SubdivFlag, uint32_t Log2TransformBlockSize)
{
    SubdivFlag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANS_SUBDIV_FLAG_HEVC] + Log2TransformBlockSize);
}

// Returns selected CABAC context for CBF with specified depth
uint32_t getCtxQtCbf(ComponentPlane plane, uint32_t TrDepth)
{
    if (plane)
    {
        return TrDepth;
    }
    else
    {
        return TrDepth == 0 ? 1 : 0;
    }
}

// Decode quad tree CBF value
void H265SegmentDecoder::ParseQtCbfCABAC(uint32_t AbsPartIdx, ComponentPlane plane, uint32_t TrDepth, uint32_t Depth)
{
    const uint32_t Ctx = getCtxQtCbf(plane, TrDepth);

    uint32_t uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_CBF_HEVC] + Ctx + NUM_CONTEXT_QT_CBF * (plane ? COMPONENT_CHROMA : plane));

    m_cu->setCbfSubParts(uVal << TrDepth, plane, AbsPartIdx, Depth);
}

// Decode root CU CBF value
void H265SegmentDecoder::ParseQtRootCbfCABAC(uint32_t& QtRootCbf)
{
    uint32_t uVal;
    const uint32_t Ctx = 0;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_ROOT_CBF_HEVC] + Ctx);
    QtRootCbf = uVal;
}

void H265SegmentDecoder::DecodeQPChromaAdujst()
{
    uint32_t cu_chroma_qp_offset_flag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[CU_CHROMA_QP_OFFSET_FLAG]);
    if (!cu_chroma_qp_offset_flag)
    {
        m_context->SetNewQP(0, 0);
        return;
    }

    uint32_t cu_chroma_qp_offset_idx = 1;
    if (cu_chroma_qp_offset_flag && m_pPicParamSet->chroma_qp_offset_list_len > 1)
    {
        ReadUnaryMaxSymbolCABAC(cu_chroma_qp_offset_idx, ctxIdxOffsetHEVC[CU_CHROMA_QP_OFFSET_IDX], 0, m_pPicParamSet->chroma_qp_offset_list_len - 1);
        cu_chroma_qp_offset_idx++;
    }

    m_context->SetNewQP(0, cu_chroma_qp_offset_idx);
}

// Decode and set new QP value
void H265SegmentDecoder::DecodeQP(uint32_t AbsPartIdx)
{
    if (m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        ParseDeltaQPCABAC(AbsPartIdx);
    }
}

// Decode and set new QP value. HEVC spec 9.3.3.8
void H265SegmentDecoder::ParseDeltaQPCABAC(uint32_t AbsPartIdx)
{
    int32_t qp;
    uint32_t uiDQp;

    ReadUnaryMaxSymbolCABAC(uiDQp, ctxIdxOffsetHEVC[DQP_HEVC], 1, CU_DQP_TU_CMAX);

    if (uiDQp >= CU_DQP_TU_CMAX)
    {
        uint32_t uVal;
        ReadEpExGolombCABAC(uVal, CU_DQP_EG_k);
        uiDQp += uVal;
    }

    if (uiDQp > 0)
    {
        uint32_t Sign = m_pBitStream->DecodeSingleBinEP_CABAC();
        int32_t CuQpDeltaVal = Sign ? -(int32_t)uiDQp : uiDQp;

        int32_t QPBdOffsetY = m_pSeqParamSet->getQpBDOffsetY();

        if (CuQpDeltaVal < -(26 + QPBdOffsetY / 2) || CuQpDeltaVal > (25 + QPBdOffsetY / 2))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        qp = (((int32_t)getRefQP(AbsPartIdx) + CuQpDeltaVal + 52 + 2 * QPBdOffsetY) % (52 + QPBdOffsetY)) - QPBdOffsetY;
    }
    else
    {
        qp = getRefQP(AbsPartIdx);
    }

    m_context->SetNewQP(qp);
    m_cu->m_CodedQP = (uint8_t)qp;
}

// Check whether this CU was last in slice
void H265SegmentDecoder::FinishDecodeCU(uint32_t AbsPartIdx, uint32_t Depth, uint32_t& IsLast)
{
    IsLast = DecodeSliceEnd(AbsPartIdx, Depth);
    if (m_pSliceHeader->cu_chroma_qp_offset_enabled_flag && !m_IsCuChromaQpOffsetCoded)
        m_context->SetNewQP(0, 0); // clear chroma qp offsets
}

// Copy CU data from position 0 to all other subparts
// This information is needed for reconstruct which may be done in another thread
void H265SegmentDecoder::BeforeCoeffs(uint32_t AbsPartIdx, uint32_t Depth)
{
    H265CodingUnitData * data = m_cu->GetCUData(AbsPartIdx);
    data->qp = (int8_t)m_context->GetQP();
    m_cu->SetCUDataSubParts(AbsPartIdx, Depth);
}

// Decode slice end flag if necessary
bool H265SegmentDecoder::DecodeSliceEnd(uint32_t AbsPartIdx, uint32_t Depth)
{
    uint32_t IsLast;
    H265DecoderFrame* pFrame = m_pCurrentFrame;
    uint32_t CurNumParts = pFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    uint32_t Width = m_pSeqParamSet->pic_width_in_luma_samples;
    uint32_t Height = m_pSeqParamSet->pic_height_in_luma_samples;
    uint32_t GranularityWidth = m_pSeqParamSet->MaxCUSize;
    uint32_t PosX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[AbsPartIdx];
    uint32_t PosY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[AbsPartIdx];

    if (((PosX + m_cu->GetWidth(AbsPartIdx)) % GranularityWidth == 0 || (PosX + m_cu->GetWidth(AbsPartIdx) == Width))
        && ((PosY + m_cu->GetWidth(AbsPartIdx)) % GranularityWidth == 0|| (PosY + m_cu->GetWidth(AbsPartIdx) == Height)))
    {
        IsLast = m_pBitStream->DecodeTerminatingBit_CABAC();
    }
    else
    {
        IsLast=0;
    }

    if (IsLast)
    {
        m_pSliceHeader->m_sliceSegmentCurEndCUAddr = m_cu->getSCUAddr() + AbsPartIdx + CurNumParts - 1;
    }
    else
    {
        IsLast = m_pBitStream->CheckBSLeft();
    }

    return (IsLast > 0);
}

// Returns CABAC context index for sig_coeff_flag. HEVC spec 9.3.4.2.5
static inline H265_FORCEINLINE uint32_t getSigCtxInc(int32_t patternSigCtx,
                                 uint32_t scanIdx,
                                 const uint32_t PosX,
                                 const uint32_t PosY,
                                 const uint32_t log2BlockSize,
                                 bool IsLuma)
{
    //LUMA map
    static const uint32_t ctxIndMap[16] =
    {
        0, 1, 4, 5,
        2, 3, 4, 5,
        6, 6, 8, 8,
        7, 7, 8, 8
    };

    static const uint32_t lCtx[4] = {0x00010516, 0x06060606, 0x000055AA, 0xAAAAAAAA};

    if ((PosX|PosY) == 0)
        return 0;

    if (log2BlockSize == 2)
    {
        return ctxIndMap[(PosY<<2) + PosX];
    }

    uint32_t Offset = log2BlockSize == 3 ? (scanIdx == SCAN_DIAG ? 9 : 15) : (IsLuma ? 21 : 12);

    int32_t posXinSubset = PosX & 0x3;
    int32_t posYinSubset = PosY & 0x3;

    int32_t cnt = (lCtx[patternSigCtx] >> (((posXinSubset<<2) + posYinSubset)<<1)) & 0x3;

    return ((IsLuma && ((PosX|PosY) > 3) ) ? 3 : 0) + Offset + cnt;
}

static const uint8_t diagOffs[8] = {0, 1, 3, 6, 10, 15, 21, 28};
static const uint32_t stXoffs[3] = { 0xFB9E4910, 0xE4E4E4E4, 0xFFAA5500 };
static const uint32_t stYoffs[3] = { 0xEDB1B184, 0xFFAA5500, 0xE4E4E4E4 };

// Raster to Z-scan conversion
static inline H265_FORCEINLINE uint32_t Rst2ZS(const uint32_t x, const uint32_t y, const uint32_t l2w)
{
    const uint32_t diagId = x + y;
    const uint32_t w = 1<<l2w;

    return (diagId < w) ? (diagOffs[diagId] + x) : ((1<<(l2w<<1)) - diagOffs[((w<<1)-1)-diagId] + (w-y-1));
}

// Parse TU coefficients
void H265SegmentDecoder::ParseCoeffNxNCABAC(CoeffsPtr pCoef, uint32_t AbsPartIdx, uint32_t Log2BlockSize, ComponentPlane plane)
{
    if (m_pSeqParamSet->scaling_list_enabled_flag)
    {
        ParseCoeffNxNCABACOptimized<true>(pCoef, AbsPartIdx, Log2BlockSize, plane);
    }
    else
    {
        ParseCoeffNxNCABACOptimized<false>(pCoef, AbsPartIdx, Log2BlockSize, plane);
    }
}

// turn off the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif // _MSVC_LANG

// Parse TU coefficients
template <bool scaling_list_enabled_flag>
void H265SegmentDecoder::ParseCoeffNxNCABACOptimized(CoeffsPtr pCoef, uint32_t AbsPartIdx, uint32_t Log2BlockSize, ComponentPlane plane)
{
    const uint32_t Size    = 1 << Log2BlockSize;
    const uint32_t L2Width = Log2BlockSize-2;

    const bool  IsLuma  = (plane == COMPONENT_LUMA);
    const bool  isBypass = m_cu->GetCUTransquantBypass(AbsPartIdx);
    const bool  beValid = isBypass ? false : m_pPicParamSet->sign_data_hiding_enabled_flag != 0;

    const uint32_t baseCoeffGroupCtxIdx = ctxIdxOffsetHEVC[SIG_COEFF_GROUP_FLAG_HEVC] + (IsLuma ? 0 : NUM_CONTEXT_SIG_COEFF_GROUP_FLAG);
    const uint32_t baseCtxIdx = ctxIdxOffsetHEVC[SIG_FLAG_HEVC] + ( IsLuma ? 0 : NUM_CONTEXT_SIG_FLAG_LUMA);

   if (m_pPicParamSet->transform_skip_enabled_flag && L2Width <= m_pPicParamSet->log2_max_transform_skip_block_size_minus2 && !isBypass)
        ParseTransformSkipFlags(AbsPartIdx, plane);

    // adujst U1, V1 to U/V
   if (plane == COMPONENT_CHROMA_U1)
       plane = COMPONENT_CHROMA_U;
   if (plane == COMPONENT_CHROMA_V1)
       plane = COMPONENT_CHROMA_V;

    memset(pCoef, 0, sizeof(Coeffs) << (Log2BlockSize << 1));

    uint32_t ScanIdx = m_cu->getCoefScanIdx(AbsPartIdx, Log2BlockSize, IsLuma, m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA);
    uint32_t PosLastY, PosLastX;
    uint32_t BlkPosLast = ParseLastSignificantXYCABAC(PosLastX, PosLastY, L2Width, IsLuma, ScanIdx);

    pCoef[BlkPosLast] = 1;

    uint32_t LastScanSet;

    uint32_t CGPosX    = PosLastX >> 2;
    uint32_t CGPosY    = PosLastY >> 2;
    uint32_t LastXinCG = PosLastX & 0x3;
    uint32_t LastYinCG = PosLastY & 0x3;
    int32_t LastPinCG;

    switch (ScanIdx) {
    case SCAN_VER:
        LastScanSet = (CGPosX << L2Width) + CGPosY;
        LastPinCG   = (LastXinCG << 2) + LastYinCG;
        break;
    case SCAN_HOR:
        LastScanSet = (CGPosY << L2Width) + CGPosX;
        LastPinCG   = (LastYinCG << 2) + LastXinCG;
        break;
    default:
    case SCAN_DIAG:
        LastScanSet = Rst2ZS(CGPosX, CGPosY, L2Width);
        LastPinCG   = Rst2ZS(LastXinCG, LastYinCG, 2);
        break;
    }

    uint32_t c1 = 1;

    uint16_t SCGroupFlagRightMask = 0;
    uint16_t SCGroupFlagLowerMask = 0;
    int16_t pos[SCAN_SET_SIZE];

    const uint8_t *sGCr = scanGCZr + (1<<(L2Width<<1))*(ScanIdx+2) - LastScanSet - 1;

    uint32_t c_Log2TrSize = g_ConvertToBit[Size] + 2;
    uint32_t bit_depth = IsLuma ? m_context->m_sps->bit_depth_luma : m_context->m_sps->bit_depth_chroma;
    uint32_t TransformShift = MAX_TR_DYNAMIC_RANGE - bit_depth - c_Log2TrSize;

    int32_t Shift;
    int32_t Add(0), Scale(0);
    int16_t *pDequantCoef = NULL;
    bool shift_right = true;

    bool use_scaling_list =
        scaling_list_enabled_flag &&
        //We take into account the value of [transform_skip_flag] only for nTbS which is greater than 4. See 8.6.3 Scaling process for transform coefficients
        (!m_cu->GetTransformSkip(plane, AbsPartIdx) || L2Width == 0);
    if (use_scaling_list)
    {
        int32_t QPRem = m_context->m_ScaledQP[plane].m_QPRem;
        int32_t QPPer = m_context->m_ScaledQP[plane].m_QPPer;

        Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift + 4;
        pDequantCoef = m_context->getDequantCoeff(plane + (m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 0 : 3), QPRem, c_Log2TrSize - 2);

        if (Shift > QPPer)
        {
            Shift -= QPPer;
            Add = 1 << (Shift - 1);
        }
        else
        {
            Shift = QPPer - Shift;
            shift_right = false;
        }
    }
    else
    {
        Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift;
        Add = 1 << (Shift - 1);
        Scale = m_context->m_ScaledQP[plane].m_QPScale;
    }

    for (int32_t SubSet = LastScanSet; SubSet >= 0; SubSet--)
    {
        uint32_t absSum;
        uint32_t CGBlkPos;
        uint32_t GoRiceParam = 0;
        bool   SigCoeffGroup;
        uint32_t numNonZero = 0;
        int32_t lPosNZ = -1, fPosNZ;

        if ((uint32_t)SubSet == LastScanSet) {
            lPosNZ = fPosNZ = -1;
            LastPinCG--;
            pos[numNonZero] = (int16_t)BlkPosLast;
            numNonZero = 1;
        }

        CGBlkPos = *sGCr++;
        CGPosY   = CGBlkPos >> L2Width;
        CGPosX   = CGBlkPos - (CGPosY << L2Width);

        uint16_t patternSigCtx = ((SCGroupFlagRightMask >> CGPosY) & 0x1) | (((SCGroupFlagLowerMask >> CGPosX) & 0x1)<<1);

        if ((uint32_t)SubSet == LastScanSet || SubSet == 0) {
            SigCoeffGroup = 1;
        } else {
            SigCoeffGroup = m_pBitStream->DecodeSingleBin_CABAC(baseCoeffGroupCtxIdx + (patternSigCtx != 0)) != 0;
        }

        uint32_t BlkPos, PosY, PosX, Sig, CtxSig, xyOffs = LastPinCG<<1;

        if (SigCoeffGroup)
        {
            SCGroupFlagRightMask |= (1 << CGPosY);
            SCGroupFlagLowerMask |= (1 << CGPosX);
            for(int32_t cPos = 0; cPos <= LastPinCG; cPos++)
            {
                PosY    = (CGPosY<<2) + ((stYoffs[ScanIdx] >> xyOffs) & 0x3);
                PosX    = (CGPosX<<2) + ((stXoffs[ScanIdx] >> xyOffs) & 0x3);
                BlkPos  = (PosY << Log2BlockSize) + PosX;
                xyOffs -=2;

                if (cPos < LastPinCG || SubSet == 0 || numNonZero)
                {
                    CtxSig = getSigCtxInc(patternSigCtx, ScanIdx, PosX, PosY, Log2BlockSize, IsLuma);
                    Sig = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx + CtxSig);
                }
                else
                {
                    Sig = 1;
                }

                pCoef[BlkPos] = (int16_t)Sig;
                pos[numNonZero] = (int16_t)BlkPos;
                _cmovnz_intrin( Sig, lPosNZ, cPos );
                _cmovz_intrin( numNonZero, fPosNZ, lPosNZ );
                numNonZero += Sig;

/* ML: original code
                if (Sig)
                {
                    if(numNonZero==0) fPosNZ = cPos;
                    lPosNZ = cPos;

                    pos[numNonZero] = BlkPos;
                    numNonZero++;
                }
*/
            }
        } else {
            SCGroupFlagRightMask &= ~(1 << CGPosY);
            SCGroupFlagLowerMask &= ~(1 << CGPosX);
        }

        if (numNonZero)
        {
            bool signHidden = (lPosNZ - fPosNZ) > LAST_MINUS_FIRST_SIG_SCAN_THRESHOLD;
            absSum = 0;
            uint32_t CtxSet = (SubSet > 0 && IsLuma) ? 2 : 0;
            uint32_t Bin;

            if (c1 == 0)
                CtxSet++;

            c1 = 1;

            uint32_t baseCtxIdx = ctxIdxOffsetHEVC[ONE_FLAG_HEVC] + (CtxSet<<2) + (IsLuma ? 0 : NUM_CONTEXT_ONE_FLAG_LUMA);

            int32_t absCoeff[SCAN_SET_SIZE];
            for (uint32_t i = 0; i < numNonZero; i++)
                absCoeff[i] = 1;

            int32_t numC1Flag = MFX_MIN(numNonZero, LARGER_THAN_ONE_FLAG_NUMBER);
            int32_t firstC2FlagIdx = -1;

            for (int32_t idx = 0; idx < numC1Flag; idx++)
            {
                Bin = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx + c1);
                if (Bin == 1)
                {
                    c1 = 0;
                    if (firstC2FlagIdx == -1)
                        firstC2FlagIdx = idx;
                }
                else if ((c1 < 3) && (c1 > 0))
                  c1++;

                absCoeff[idx] = Bin + 1;
            }

            if (c1 == 0)
            {
                baseCtxIdx = ctxIdxOffsetHEVC[ABS_FLAG_HEVC] + CtxSet + (IsLuma ? 0 : NUM_CONTEXT_ABS_FLAG_LUMA);
                if (firstC2FlagIdx != -1)
                {
                    Bin = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx);
                    absCoeff[firstC2FlagIdx] = Bin + 2;
                }
            }

            uint32_t coeffSigns;
            if (signHidden && beValid)
            {
                coeffSigns = m_pBitStream->DecodeBypassBins_CABAC(numNonZero - 1);
                coeffSigns <<= 32 - (numNonZero-1);
            }
            else
            {
                coeffSigns = m_pBitStream->DecodeBypassBins_CABAC(numNonZero);
                coeffSigns <<= 32 - numNonZero;
            }

            int32_t FirstCoeff2 = 1;
            if (c1 == 0 || numNonZero > LARGER_THAN_ONE_FLAG_NUMBER)
            {
                for (uint32_t idx = 0; idx < numNonZero; idx++)
                {
                    int32_t baseLevel  = (idx < LARGER_THAN_ONE_FLAG_NUMBER)? (2 + FirstCoeff2) : 1;

                    if (absCoeff[ idx ] == baseLevel)
                    {
                        uint32_t Level;
                        ReadCoefRemainExGolombCABAC(Level, GoRiceParam);
                        absCoeff[idx] = Level + baseLevel;
                        if (absCoeff[idx] > 3 * (1 << GoRiceParam))
                            GoRiceParam = MFX_MIN(GoRiceParam + 1, 4);
                    }

                    if (absCoeff[idx] >= 2)
                    {
                        FirstCoeff2 = 0;
                    }
                }
            }

            for (uint32_t idx = 0; idx < numNonZero; idx++)
            {
                int32_t blkPos = pos[idx];
                Coeffs coef;

                coef = (Coeffs)absCoeff[idx];
                absSum += absCoeff[idx];

                if (idx == numNonZero - 1 && signHidden && beValid)
                {
                    if (absSum & 0x1)
                        coef = -coef;
                }
                else
                {
                    int32_t sign = static_cast<int32_t>(coeffSigns) >> 31;
                    coef = (Coeffs)((coef ^ sign) - sign);
                    coeffSigns <<= 1;
                }

                if (isBypass)
                {
                    pCoef[blkPos] = (Coeffs)coef;
                }
                else
                {
                    int32_t coeffQ;
                    if (use_scaling_list)
                    {
                        if (shift_right)
                            coeffQ = (coef * pDequantCoef[blkPos] + Add) >> Shift;
                        else
                        {
                            coeffQ = (Saturate(-32768, 32767, coef * pDequantCoef[blkPos])) << Shift;
                        }
                    }
                    else
                    {
                        coeffQ = (coef * Scale + Add) >> Shift;
                    }

                    pCoef[blkPos] = (Coeffs)Saturate(-32768, 32767, coeffQ);
                }
            }
        }

        LastPinCG = 15;
    }
}

// restore the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif // _MSVC_LANG

// Parse TU transform skip flag
void H265SegmentDecoder::ParseTransformSkipFlags(uint32_t AbsPartIdx, ComponentPlane plane)
{
    uint32_t CtxIdx = ctxIdxOffsetHEVC[TRANSFORM_SKIP_HEVC] + (plane ? NUM_CONTEXT_TRANSFORMSKIP_FLAG : 0);
    uint32_t useTransformSkip = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    m_cu->setTransformSkip(useTransformSkip, plane, AbsPartIdx);
}

// Decode X and Y coordinates of last significant coefficient in a TU block
uint32_t H265SegmentDecoder::ParseLastSignificantXYCABAC(uint32_t &PosLastX, uint32_t &PosLastY, uint32_t L2Width, bool IsLuma, uint32_t ScanIdx)
{
    uint32_t mGIdx = g_GroupIdx[(1<<(L2Width+2)) - 1];

    uint32_t blkSizeOffset = IsLuma ? (L2Width*3 + ((L2Width+1)>>2)) : NUM_CONTEX_LAST_COEF_FLAG_XY;
    uint32_t shift = IsLuma ? ((L2Width+3) >> 2) : L2Width;
    uint32_t CtxIdxX = ctxIdxOffsetHEVC[LAST_X_HEVC] + blkSizeOffset;
    uint32_t CtxIdxY = ctxIdxOffsetHEVC[LAST_Y_HEVC] + blkSizeOffset;

    // Context selection is in HEVC spec 9.3.4.2.3
    for (PosLastX = 0; PosLastX < mGIdx; PosLastX++)
    {
        if (!m_pBitStream->DecodeSingleBin_CABAC(CtxIdxX + (PosLastX >> shift))) break;
    }

    for (PosLastY = 0; PosLastY < mGIdx; PosLastY++)
    {
        if (!m_pBitStream->DecodeSingleBin_CABAC(CtxIdxY + (PosLastY >> shift))) break;
    }

    if (PosLastX > 3 )
    {
        uint32_t Temp  = 0;
        int32_t Count = (PosLastX - 2) >> 1;
        for (int32_t i = 0; i < Count; i++)
        {
            Temp |= m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp <<= 1;
        }
        PosLastX = g_MinInGroup[PosLastX] + (Temp >> 1);
    }

    if (PosLastY > 3)
    {
        uint32_t Temp  = 0;
        int32_t Count = (PosLastY - 2) >> 1;
        for (int32_t i = 0; i < Count; i++)
        {
            Temp |= m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp <<= 1;
        }
        PosLastY = g_MinInGroup[PosLastY] + (Temp >> 1);
    }

    if (ScanIdx == SCAN_VER) {
        uint32_t tPos = PosLastX; PosLastX = PosLastY; PosLastY = tPos;
    }

    return PosLastX + (PosLastY << (L2Width+2));
}

// Decode coeff_abs_level_remaining value. HEVC spec 9.3.3.9
void H265SegmentDecoder::ReadCoefRemainExGolombCABAC(uint32_t &Symbol, uint32_t &Param)
{
    uint32_t prefix = 0;
    uint32_t CodeWord = 0;

    do
    {
        prefix++;
        CodeWord = m_pBitStream->DecodeSingleBinEP_CABAC();
    }
    while (CodeWord);

    CodeWord = 1 - CodeWord;
    prefix -= CodeWord;
    CodeWord = 0;

    if (prefix < COEFF_ABS_LEVEL_REMAIN_BIN_THRESHOLD)
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(Param);
        Symbol = (prefix << Param) + CodeWord;
    }
    else
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(prefix - COEFF_ABS_LEVEL_REMAIN_BIN_THRESHOLD + Param);
        Symbol = (((1 << (prefix - COEFF_ABS_LEVEL_REMAIN_BIN_THRESHOLD)) + COEFF_ABS_LEVEL_REMAIN_BIN_THRESHOLD - 1) << Param) + CodeWord;
    }
}

// Recursively produce CU reconstruct from decoded values
void H265SegmentDecoder::ReconstructCU(uint32_t AbsPartIdx, uint32_t Depth)
{
    bool BoundaryFlag = false;
    int32_t Size = m_pSeqParamSet->MaxCUSize >> Depth;
    uint32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    uint32_t LPelX = m_cu->m_CUPelX + XInc;
    uint32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    uint32_t TPelY = m_cu->m_CUPelY + YInc;

    if (LPelX + Size > m_pSeqParamSet->pic_width_in_luma_samples || TPelY + Size > m_pSeqParamSet->pic_height_in_luma_samples)
    {
        BoundaryFlag = true;
    }

    if (((Depth < m_cu->GetDepth(AbsPartIdx)) && (Depth < m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)) || BoundaryFlag)
    {
        uint32_t NextDepth = Depth + 1;
        uint32_t QNumParts = m_cu->m_NumPartition >> (NextDepth << 1);
        uint32_t Idx = AbsPartIdx;
        for (uint32_t PartIdx = 0; PartIdx < 4; PartIdx++, Idx += QNumParts)
        {
            if (BoundaryFlag)
            {
                LPelX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[Idx];
                TPelY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[Idx];
                if (LPelX >= m_pSeqParamSet->pic_width_in_luma_samples ||
                    TPelY >= m_pSeqParamSet->pic_height_in_luma_samples)
                { // !insideFrame
                    continue;
                }
            }

            ReconstructCU(Idx, NextDepth);
        }
        return;
    }

    switch (m_cu->GetPredictionMode(AbsPartIdx))
    {
        case MODE_INTER:
            ReconInter(AbsPartIdx, Depth);
            UpdateRecNeighboursBuffersN(XInc >> 2, YInc >> 2, Size, false);
            break;
        case MODE_INTRA:
            ReconIntraQT(AbsPartIdx, Depth);
            break;
        default:
            //VM_ASSERT(0); // maybe it is restoring from error
        break;
    }
}

// Perform inter CU reconstruction
void H265SegmentDecoder::ReconInter(uint32_t AbsPartIdx, uint32_t Depth)
{
    // inter prediction
    m_Prediction->MotionCompensation(m_cu, AbsPartIdx, Depth);

    // clip for only non-zero cbp case
    if (m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx) ||
        (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 && (m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx))))
    {
        // inter recon
        uint32_t Size = m_cu->GetWidth(AbsPartIdx);
        m_TrQuant->InvRecurTransformNxN(m_cu, AbsPartIdx, Size, 0);
    }
}

// Place IPCM decoded samples to reconstruct frame
void H265SegmentDecoder::ReconPCM(uint32_t AbsPartIdx, uint32_t Depth)
{
    uint32_t size = m_pSeqParamSet->MaxCUSize >> Depth;
    m_reconstructor->ReconstructPCMBlock(m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx), m_pCurrentFrame->pitch_luma(),
        m_pSeqParamSet->bit_depth_luma - m_pSeqParamSet->pcm_sample_bit_depth_luma, // luma bit shift
        m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx), m_pCurrentFrame->pitch_chroma(),
        m_pSeqParamSet->bit_depth_chroma - m_pSeqParamSet->pcm_sample_bit_depth_chroma, // chroma bit shift
        &m_context->m_coeffsRead, size);
}

// Fill up inter information in local decoding context and colocated lookup table
void H265SegmentDecoder::UpdatePUInfo(uint32_t PartX, uint32_t PartY, uint32_t PartWidth, uint32_t PartHeight, H265MVInfo &MVi)
{
    VM_ASSERT(MVi.m_refIdx[REF_PIC_LIST_0] >= 0 || MVi.m_refIdx[REF_PIC_LIST_1] >= 0);

    for (uint32_t RefListIdx = 0; RefListIdx < 2; RefListIdx++)
    {
        if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0 && MVi.m_refIdx[RefListIdx] >= 0)
        {
            H265DecoderRefPicList::ReferenceInformation &refInfo = m_pRefPicList[RefListIdx][MVi.m_refIdx[RefListIdx]];
            MVi.m_index[RefListIdx] = (int8_t)refInfo.refFrame->GetFrameMID();

            if (MVi.m_mv[RefListIdx].Vertical > m_context->m_mvsDistortionTemp)
                m_context->m_mvsDistortionTemp = MVi.m_mv[RefListIdx].Vertical;
        }
    }

    uint32_t mask = (1 << m_pSeqParamSet->MaxCUDepth) - 1;
    PartX &= mask;
    PartY &= mask;

    int32_t stride = m_context->m_CurrCTBStride;

    // Colocated table
    int32_t CUX = (m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + PartX;
    int32_t CUY = (m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + PartY;
    H265MVInfo *colInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

    for (uint32_t y = 0; y < PartHeight; y++)
    {
        for (uint32_t x = 0; x < PartWidth; x++)
        {
            colInfo[x] = MVi;
        }
        colInfo += m_pSeqParamSet->NumPartitionsInFrameWidth;
    }

    // Decoding context
    H265FrameHLDNeighborsInfo *pInfo;
    H265FrameHLDNeighborsInfo info;
    info.data = 0;
    info.members.IsAvailable = 1;

    // Bottom row
    pInfo = &m_context->m_CurrCTBFlags[(PartY + PartHeight - 1) * stride + PartX];
    for (uint32_t i = 0; i < PartWidth; i++)
    {
        pInfo[i] = info;
    }

    // Right column
    pInfo = &m_context->m_CurrCTBFlags[PartY * stride + PartX];
    for (uint32_t i = 0; i < PartHeight; i++)
    {
        pInfo[PartWidth - 1] = info;
        pInfo += stride;
    }
}

// Fill up basic CU information in local decoder context
void H265SegmentDecoder::UpdateNeighborBuffers(uint32_t AbsPartIdx, uint32_t Depth, bool isSkipped)
{
    int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartSize = m_pSeqParamSet->NumPartitionsInCUSize >> Depth;
    int32_t stride = m_context->m_CurrCTBStride;

    H265FrameHLDNeighborsInfo *pInfo;
    H265FrameHLDNeighborsInfo info;
    info.members.IsAvailable = 1;
    info.members.IsIntra = m_cu->GetPredictionMode(AbsPartIdx);
    info.members.Depth = m_cu->GetDepth(AbsPartIdx);
    info.members.SkipFlag = isSkipped;
    info.members.IntraDir = m_cu->GetLumaIntra(AbsPartIdx);
    info.members.qp = (int8_t)m_context->GetQP();

    // Bottom row
    pInfo = &m_context->m_CurrCTBFlags[(YInc + PartSize - 1) * stride + XInc];
    for (int32_t i = 0; i < PartSize; i++)
    {
        pInfo[i] = info;
    }

    // Right column
    pInfo = &m_context->m_CurrCTBFlags[YInc * stride + XInc];
    for (int32_t i = 0; i < PartSize; i++)
    {
        pInfo[PartSize - 1] = info;
        pInfo += stride;
    }

    if (info.members.IsIntra)
    {
        int32_t CUX = (m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + XInc;
        int32_t CUY = (m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + YInc;
        H265MVInfo *colInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

        for (int32_t i = 0; i < PartSize; i++)
        {
            for (int32_t j = 0; j < PartSize; j++)
            {
                colInfo[j].m_refIdx[REF_PIC_LIST_0] = -1;
                colInfo[j].m_refIdx[REF_PIC_LIST_1] = -1;
            }
            colInfo += m_pSeqParamSet->NumPartitionsInFrameWidth;
        }
    }
}

// Change QP recorded in CU block of local context to a new value
void H265SegmentDecoder::UpdateNeighborDecodedQP(uint32_t AbsPartIdx, uint32_t Depth)
{
    int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartSize = m_pSeqParamSet->NumPartitionsInCUSize >> Depth;
    int32_t qp = m_context->GetQP();

    for (int32_t y = YInc; y < YInc + PartSize; y++)
        for (int32_t x = XInc; x < XInc + PartSize; x++)
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * y + x].members.qp = (int8_t)qp;
}

// Update intra availability flags for reconstruct
void H265SegmentDecoder::UpdateRecNeighboursBuffersN(int32_t PartX, int32_t PartY, int32_t PartSize, bool IsIntra)
{
    uint32_t maskL, maskT, maskTL;
    bool   isIntra = (m_pPicParamSet->constrained_intra_pred_flag) ? (IsIntra ? 1 : 0) : 1;

    PartSize >>= 2;

    uint32_t maxCUSzIn4x4 = m_pSeqParamSet->MaxCUSize >> 2;
    int32_t diagId = (PartX+(maxCUSzIn4x4-PartY)-PartSize);

    maskTL = ((0x1<<((PartSize<<1)-1))-1) << diagId;
    maskL = (0x1<<PartSize) - 1;
    maskT = maskL << PartX;
    maskL = maskL << PartY;

    if(isIntra) {
        *m_context->m_RecTLIntraFlags |= maskTL;
        switch (PartSize) {
        case 16:
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;

            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
        case 8:
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
        case 4:
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecLfIntraFlags[++PartX] |= maskL;
            m_context->m_RecTpIntraFlags[++PartY] |= maskT; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
        case 2:
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
        case 1:
        default:
            m_context->m_RecLfIntraFlags[++PartX] |= maskL; m_context->m_RecTpIntraFlags[++PartY] |= maskT;
        }
    } else {
        maskT = ~maskT; maskL = ~maskL;

        *m_context->m_RecTLIntraFlags &= ~maskTL;
        switch (PartSize) {
        case 16:
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;

            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
        case 8:
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
        case 4:
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecLfIntraFlags[++PartX] &= maskL;
            m_context->m_RecTpIntraFlags[++PartY] &= maskT; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
        case 2:
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
        case 1:
        default:
            m_context->m_RecLfIntraFlags[++PartX] &= maskL; m_context->m_RecTpIntraFlags[++PartY] &= maskT;
        }
    }
}

// Calculate CABAC context for split flag. HEVC spec 9.3.4.2.2
uint32_t H265SegmentDecoder::getCtxSplitFlag(int32_t PartX, int32_t PartY, uint32_t Depth)
{
    uint32_t uiCtx;

    uiCtx = 0;

    if ((m_context->m_CurrCTBFlags[PartY * m_context->m_CurrCTBStride + PartX - 1].members.Depth > Depth))
    {
        uiCtx = 1;
    }

    if ((m_context->m_CurrCTBFlags[(PartY - 1) * m_context->m_CurrCTBStride + PartX].members.Depth > Depth))
    {
        uiCtx += 1;
    }

    return uiCtx;
}

// Calculate CABAC context for skip flag. HEVC spec 9.3.4.2.2
uint32_t H265SegmentDecoder::getCtxSkipFlag(int32_t PartX, int32_t PartY)
{
    uint32_t uiCtx = 0;

    if (m_context->m_CurrCTBFlags[PartY * m_context->m_CurrCTBStride + PartX - 1].members.SkipFlag)
    {
        uiCtx = 1;
    }

    if (m_context->m_CurrCTBFlags[(PartY - 1) * m_context->m_CurrCTBStride + PartX].members.SkipFlag)
    {
        uiCtx += 1;
    }

    return uiCtx;
}

// Get predictor array of intra directions for intra luma direction decoding. HEVC spec 8.4.2
void H265SegmentDecoder::getIntraDirLumaPredictor(uint32_t AbsPartIdx, int32_t IntraDirPred[])
{
    int32_t LeftIntraDir = INTRA_LUMA_DC_IDX;
    int32_t AboveIntraDir = INTRA_LUMA_DC_IDX;
    int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;

    int32_t PartOffsetY = (m_pSeqParamSet->NumPartitionsInCU >> (m_cu->GetDepth(AbsPartIdx) << 1)) >> 1;
    int32_t PartOffsetX = PartOffsetY >> 1;

    // Get intra direction of left PU
    if ((AbsPartIdx & PartOffsetX) == 0)
    {
        LeftIntraDir = m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.IsIntra ? m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.IntraDir : INTRA_LUMA_DC_IDX;
    }
    else
    {
        LeftIntraDir = m_cu->GetLumaIntra(AbsPartIdx - PartOffsetX);
    }

    // Get intra direction of above PU
    if ((AbsPartIdx & PartOffsetY) == 0)
    {
        if (YInc > 0)
            AboveIntraDir = m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.IsIntra ? m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.IntraDir : INTRA_LUMA_DC_IDX;
    }
    else
    {
        AboveIntraDir = m_cu->GetLumaIntra(AbsPartIdx - PartOffsetY);
    }

    if (LeftIntraDir == AboveIntraDir)
    {
        if (LeftIntraDir > 1) // angular modes
        {
            IntraDirPred[0] = LeftIntraDir;
            IntraDirPred[1] = ((LeftIntraDir + 29) % 32) + 2;
            IntraDirPred[2] = ((LeftIntraDir - 1 ) % 32) + 2;
        }
        else //non-angular
        {
            IntraDirPred[0] = INTRA_LUMA_PLANAR_IDX;
            IntraDirPred[1] = INTRA_LUMA_DC_IDX;
            IntraDirPred[2] = INTRA_LUMA_VER_IDX;
        }
    }
    else
    {
        IntraDirPred[0] = LeftIntraDir;
        IntraDirPred[1] = AboveIntraDir;

        if (LeftIntraDir && AboveIntraDir) //both modes are non-planar
        {
            IntraDirPred[2] = INTRA_LUMA_PLANAR_IDX;
        }
        else
        {
            IntraDirPred[2] = (LeftIntraDir + AboveIntraDir) < 2 ? INTRA_LUMA_VER_IDX : INTRA_LUMA_DC_IDX;
        }
    }
}

// Calculate CU QP value based on previously used QP values. HEVC spec 8.6.1
int32_t H265SegmentDecoder::getRefQP(int32_t AbsPartIdx)
{
    int32_t shift = (m_pSeqParamSet->MaxCUDepth - m_pPicParamSet->diff_cu_qp_delta_depth) << 1;
    uint32_t AbsQpMinCUIdx = (AbsPartIdx >> shift) << shift;
    int32_t QPXInc = m_cu->m_rasterToPelX[AbsQpMinCUIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t QPYInc = m_cu->m_rasterToPelY[AbsQpMinCUIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    int8_t qpL, qpA, lastValidQP = 0;

    if (QPXInc == 0 || QPYInc == 0)
    {
        int32_t lastValidPartIdx = m_cu->getLastValidPartIdx(AbsQpMinCUIdx);
        if (lastValidPartIdx >= 0)
        {
            int32_t x = m_cu->m_rasterToPelX[lastValidPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
            int32_t y = m_cu->m_rasterToPelY[lastValidPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
            lastValidQP = m_context->m_CurrCTBFlags[y * m_context->m_CurrCTBStride + x].members.qp;
        }
        else
            return m_context->GetQP();
    }

    if (QPXInc != 0)
        qpL = m_context->m_CurrCTBFlags[QPYInc * m_context->m_CurrCTBStride + QPXInc - 1].members.qp;
    else
        qpL = lastValidQP;

    if (QPYInc != 0)
        qpA = m_context->m_CurrCTBFlags[(QPYInc - 1) * m_context->m_CurrCTBStride + QPXInc].members.qp;
    else
        qpA = lastValidQP;

    return (qpL + qpA + 1) >> 1;
}

// Returns whether two motion vectors are different enough based on merge parallel level
static bool inline isDiffMER(uint32_t plevel, int32_t xN, int32_t yN, int32_t xP, int32_t yP)
{
    if ((xN >> plevel) != (xP >> plevel))
    {
        return true;
    }
    if ((yN >> plevel) != (yP >> plevel))
    {
        return true;
    }
    return false;
}

// Returns whether motion information in two cells of collocated motion table is the same
bool H265SegmentDecoder::hasEqualMotion(int32_t dir1, int32_t dir2)
{
    H265MVInfo &motionInfo1 = m_pCurrentFrame->m_CodingData->m_colocatedInfo[dir1];
    H265MVInfo &motionInfo2 = m_pCurrentFrame->m_CodingData->m_colocatedInfo[dir2];

    if (motionInfo1.m_refIdx[REF_PIC_LIST_0] != motionInfo2.m_refIdx[REF_PIC_LIST_0] ||
        motionInfo1.m_refIdx[REF_PIC_LIST_1] != motionInfo2.m_refIdx[REF_PIC_LIST_1])
        return false;

    if (motionInfo1.m_refIdx[REF_PIC_LIST_0] >= 0 && motionInfo1.m_mv[REF_PIC_LIST_0] != motionInfo2.m_mv[REF_PIC_LIST_0])
        return false;

    if (motionInfo1.m_refIdx[REF_PIC_LIST_1] >= 0 && motionInfo1.m_mv[REF_PIC_LIST_1] != motionInfo2.m_mv[REF_PIC_LIST_1])
        return false;

    return true;
}

#define UPDATE_MV_INFO(dir)                                                                         \
    CandIsInter[Count] = true;                                                                      \
    H265MVInfo &motionInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo[dir];                   \
    if (motionInfo.m_refIdx[REF_PIC_LIST_0] >= 0)                                                   \
    {                                                                                               \
        InterDirNeighbours[Count] = 1;                                                              \
        MVBufferNeighbours[Count].setMVInfo(REF_PIC_LIST_0, motionInfo.m_refIdx[REF_PIC_LIST_0], motionInfo.m_mv[REF_PIC_LIST_0]); \
    }                                                                                               \
    else                                                                                            \
        InterDirNeighbours[Count] = 0;                                                              \
                                                                                                    \
    if (B_SLICE == m_pSliceHeader->slice_type && motionInfo.m_refIdx[REF_PIC_LIST_1] >= 0)          \
    {                                                                                               \
        InterDirNeighbours[Count] += 2;                                                             \
        MVBufferNeighbours[Count].setMVInfo(REF_PIC_LIST_1, motionInfo.m_refIdx[REF_PIC_LIST_1], motionInfo.m_mv[REF_PIC_LIST_1]); \
    }                                                                                               \
                                                                                                    \
    if (mrgCandIdx == Count)                                                                        \
    {                                                                                               \
        return;                                                                                     \
    }                                                                                               \
    Count++;


// Prepare an array of motion vector candidates for merge mode. HEVC spec 8.5.3.2.2
void H265SegmentDecoder::getInterMergeCandidates(uint32_t AbsPartIdx, uint32_t PartIdx, H265MVInfo *MVBufferNeighbours,
                                                 uint8_t *InterDirNeighbours, int32_t mrgCandIdx)
{
    static const uint32_t PriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
    static const uint32_t PriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};

    bool CandIsInter[MERGE_MAX_NUM_CAND];
    for (int32_t ind = 0; ind < m_pSliceHeader->max_num_merge_cand; ++ind)
    {
        CandIsInter[ind] = false;
        MVBufferNeighbours[ind].m_refIdx[REF_PIC_LIST_0] = NOT_VALID;
        MVBufferNeighbours[ind].m_refIdx[REF_PIC_LIST_1] = NOT_VALID;
    }

    // compute the location of the current PU
    uint32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    uint32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    int32_t LPelX = m_cu->m_CUPelX + XInc;
    int32_t TPelY = m_cu->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;

    int32_t TUPartNumberInGMV = m_pSeqParamSet->NumPartitionsInFrameWidth * PartY + PartX;

    int32_t nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    int32_t PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;
    EnumPartSize CurPS = m_cu->GetPartitionSize(AbsPartIdx);

    int32_t Count = 0;

    // left
    int32_t leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    int32_t leftAddrGMV = TUPartNumberInGMV + m_pSeqParamSet->NumPartitionsInFrameWidth * (PartHeight - 1) - 1;

    bool isAvailableA1 = m_context->m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == PART_SIZE_Nx2N || CurPS == PART_SIZE_nLx2N || CurPS == PART_SIZE_nRx2N));

    if (isAvailableA1)
    {
        UPDATE_MV_INFO(leftAddrGMV);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above
    int32_t aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    int32_t aboveAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + (PartWidth - 1);
    bool isAvailableB1 = m_context->m_CurrCTBFlags[aboveAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW - 1, TPelY - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == PART_SIZE_2NxN || CurPS == PART_SIZE_2NxnU || CurPS == PART_SIZE_2NxnD));

    if (isAvailableB1 && (!isAvailableA1 || !hasEqualMotion(leftAddrGMV, aboveAddrGMV)))
    {
        UPDATE_MV_INFO(aboveAddrGMV);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above right
    int32_t aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    int32_t aboveRightAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + PartWidth;
    bool isAvailableB0 = m_context->m_CurrCTBFlags[aboveRightAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveRightAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW, TPelY - 1, LPelX, TPelY);

    if (isAvailableB0 && (!isAvailableB1 || !hasEqualMotion(aboveAddrGMV, aboveRightAddrGMV)))
    {
        UPDATE_MV_INFO(aboveRightAddrGMV);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // left bottom
    int32_t leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;
    int32_t leftBottomAddrGMV = TUPartNumberInGMV - 1 + m_pSeqParamSet->NumPartitionsInFrameWidth * PartHeight;
    bool isAvailableA0 = m_context->m_CurrCTBFlags[leftBottomAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftBottomAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH, LPelX, TPelY);

    if (isAvailableA0 && (!isAvailableA1 || !hasEqualMotion(leftAddrGMV, leftBottomAddrGMV)))
    {
        UPDATE_MV_INFO(leftBottomAddrGMV);
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above left
    if (Count < 4 && PartX > 0)
    {
        int32_t aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;
        int32_t aboveLeftAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth - 1;
        bool isAvailableB2 = m_context->m_CurrCTBFlags[aboveLeftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveLeftAddr].members.IsIntra &&
            isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY - 1, LPelX, TPelY);

         if (isAvailableB2 &&
            (!isAvailableA1 || !hasEqualMotion(leftAddrGMV, aboveLeftAddrGMV)) &&
            (!isAvailableB1 || !hasEqualMotion(aboveAddrGMV, aboveLeftAddrGMV)))
        {
            UPDATE_MV_INFO(aboveLeftAddrGMV);
        }
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // Temporal MV prediction. HEVC spec 8.5.3.2.8
    if (m_pSliceHeader->slice_temporal_mvp_enabled_flag)
    {
        uint32_t bottomRightPartX = PartX + PartWidth;
        uint32_t bottomRightPartY = PartY + PartHeight;
        uint32_t centerPartX = PartX + (PartWidth >> 1);
        uint32_t centerPartY = PartY + (PartHeight >> 1);
        bool ExistMV = false;
        bool checkBottomRight = true;

        if ((bottomRightPartX << m_pSeqParamSet->log2_min_transform_block_size) >= m_pSeqParamSet->pic_width_in_luma_samples ||
            (bottomRightPartY << m_pSeqParamSet->log2_min_transform_block_size) >= m_pSeqParamSet->pic_height_in_luma_samples ||
            YInc + PartHeight >= m_pCurrentFrame->getNumPartInCUSize()) // is not at the last column of LCU But is last row of LCU
        {
            checkBottomRight = false;
        }

        RefIndexType RefIdx = 0;
        H265MotionVector ColMV;
        uint8_t dir = 0;
        int32_t ArrayAddr = Count;

        if (checkBottomRight)
            ExistMV = GetColMVP(REF_PIC_LIST_0, bottomRightPartX, bottomRightPartY, ColMV, RefIdx);

        if (!ExistMV)
            ExistMV = GetColMVP(REF_PIC_LIST_0, centerPartX, centerPartY, ColMV, RefIdx);

        if (ExistMV)
        {
            dir |= 1;
            MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_0, RefIdx, ColMV);
        }

        if (B_SLICE == m_pSliceHeader->slice_type)
        {
            ExistMV = false;
            if (checkBottomRight)
                ExistMV = GetColMVP(REF_PIC_LIST_1, bottomRightPartX, bottomRightPartY, ColMV, RefIdx);

            if (!ExistMV)
                ExistMV = GetColMVP(REF_PIC_LIST_1, centerPartX, centerPartY, ColMV, RefIdx);

            if (ExistMV)
            {
                dir |= 2;
                MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_1, RefIdx, ColMV);
            }
        }

        if (dir != 0)
        {
            InterDirNeighbours[ArrayAddr] = dir;
            CandIsInter[ArrayAddr] = true;
            if (mrgCandIdx == Count)
            {
                return;
            }
            Count++;
        }
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    int32_t ArrayAddr = Count;
    int32_t Cutoff = ArrayAddr;

    // Combine candidates. HEVC spec 8.5.3.2.4
    if (m_pSliceHeader->slice_type == B_SLICE)
    {
        for (int32_t idx = 0; idx < Cutoff * (Cutoff - 1) && ArrayAddr != m_pSliceHeader->max_num_merge_cand; idx++)
        {
            int32_t i = PriorityList0[idx];
            int32_t j = PriorityList1[idx];
            if (CandIsInter[i] && CandIsInter[j] && (InterDirNeighbours[i] & 0x1) && (InterDirNeighbours[j] & 0x2))
            {
                int32_t RefPOCL0 = m_pRefPicList[REF_PIC_LIST_0][MVBufferNeighbours[i].m_refIdx[REF_PIC_LIST_0]].refFrame->GetFrameMID();
                int32_t RefPOCL1 = m_pRefPicList[REF_PIC_LIST_1][MVBufferNeighbours[j].m_refIdx[REF_PIC_LIST_1]].refFrame->GetFrameMID();

                if (RefPOCL0 == RefPOCL1 && MVBufferNeighbours[i].m_mv[REF_PIC_LIST_0] == MVBufferNeighbours[j].m_mv[REF_PIC_LIST_1])
                {
                    CandIsInter[ArrayAddr] = false;
                }
                else
                {
                    CandIsInter[ArrayAddr] = true;
                    InterDirNeighbours[ArrayAddr] = 3;
                    // get MV from cand[i] and cand[j]
                    MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_0, MVBufferNeighbours[i].m_refIdx[REF_PIC_LIST_0], MVBufferNeighbours[i].m_mv[REF_PIC_LIST_0]);
                    MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_1, MVBufferNeighbours[j].m_refIdx[REF_PIC_LIST_1], MVBufferNeighbours[j].m_mv[REF_PIC_LIST_1]);

                    ArrayAddr++;
                }
            }
        }
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    int32_t numRefIdx = (m_pSliceHeader->slice_type == B_SLICE) ? MFX_MIN(m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0], m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_1]) : m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0];
    int8_t r = 0;
    int32_t refcnt = 0;

    // Add zero MVs. HEVC spec 8.5.3.2.5
    while (ArrayAddr < m_pSliceHeader->max_num_merge_cand)
    {
        CandIsInter[ArrayAddr] = true;
        InterDirNeighbours[ArrayAddr] = 1;
        MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_0, r, H265MotionVector(0, 0));

        if (m_pSliceHeader->slice_type == B_SLICE)
        {
            InterDirNeighbours[ArrayAddr] = 3;
            MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_1, r, H265MotionVector(0, 0));
        }

        ArrayAddr++;

        if (refcnt == numRefIdx - 1)
            r = 0;
        else
        {
            r++;
            refcnt++;
        }
    }
}

// Prepare an array of motion vector candidates for AMVP mode. HEVC spec 8.5.3.2.6
void H265SegmentDecoder::fillMVPCand(uint32_t AbsPartIdx, uint32_t PartIdx, EnumRefPicList RefPicList, int32_t RefIdx, AMVPInfo* pInfo)
{
    H265MotionVector MvPred;
    bool Added = false;

    pInfo->NumbOfCands = 0;
    if (RefIdx < 0)
    {
        return;
    }

    // compute the location of the current PU
    int32_t XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    int32_t YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    int32_t LPelX = m_cu->m_CUPelX + XInc;
    int32_t TPelY = m_cu->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    int32_t TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;
    int32_t TUPartNumberInGMV = m_pSeqParamSet->NumPartitionsInFrameWidth * PartY + PartX;

    int32_t nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    uint32_t PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    uint32_t PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;

    // Left predictor search
    int32_t leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    int32_t leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;

    uint32_t leftAddrMV = TUPartNumberInGMV + m_pSeqParamSet->NumPartitionsInFrameWidth * (PartHeight - 1) - 1;
    int32_t leftBottomAddrMV = TUPartNumberInGMV - 1 + m_pSeqParamSet->NumPartitionsInFrameWidth * PartHeight;

    for (;;)
    {
        Added = true;
        if (AddMVPCand(pInfo, RefPicList, RefIdx, leftBottomAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[leftBottomAddrMV]))
            break;

        if (AddMVPCand(pInfo, RefPicList, RefIdx, leftAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[leftAddrMV]))
            break;

        if (AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftBottomAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[leftBottomAddrMV]))
            break;

        if (AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[leftAddrMV]))
            break;

        Added = false;
        break;
    }

    // Above predictor search
    int32_t aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    int32_t aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    int32_t aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;

    int32_t aboveAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + (PartWidth - 1);
    int32_t aboveRightAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + PartWidth;
    int32_t aboveLeftAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth - 1;

    for (;;)
    {
        Added = true;

        if (AddMVPCand(pInfo, RefPicList, RefIdx, aboveRightAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveRightAddrGMV]))
            break;

        if (AddMVPCand(pInfo, RefPicList, RefIdx, aboveAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveAddrGMV]))
            break;

        if (AddMVPCand(pInfo, RefPicList, RefIdx, aboveLeftAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveLeftAddrGMV]))
            break;

        Added = false;
        break;
    }

    if (pInfo->NumbOfCands == 2)
        Added = true;
    else
    {
        Added = m_context->m_CurrCTBFlags[leftBottomAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftBottomAddr].members.IsIntra;

        if (!Added)
            Added = m_context->m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftAddr].members.IsIntra;
    }

    if (!Added)
    {
        for (;;)
        {
            Added = true;

            if (AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveRightAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveRightAddrGMV]))
                break;

            if (AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveAddrGMV]))
                break;

            if (AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveLeftAddr, m_pCurrentFrame->m_CodingData->m_colocatedInfo[aboveLeftAddrGMV]))
                break;

            Added = false;
            break;
        }
    }

    if (pInfo->NumbOfCands == 2)
    {
        if (pInfo->MVCandidate[0] == pInfo->MVCandidate[1])
        {
            pInfo->NumbOfCands = 1;
        }
    }

    // Temporal MV prediction. HEVC spec 8.5.3.2.8
    if (m_pSliceHeader->slice_temporal_mvp_enabled_flag)
    {
        uint32_t bottomRightPartX = PartX + PartWidth;
        uint32_t bottomRightPartY = PartY + PartHeight;
        H265MotionVector ColMv;
        bool checkBottomRight = true;

        if ((bottomRightPartX << m_pSeqParamSet->log2_min_transform_block_size) >= m_pSeqParamSet->pic_width_in_luma_samples ||
            (bottomRightPartY << m_pSeqParamSet->log2_min_transform_block_size) >= m_pSeqParamSet->pic_height_in_luma_samples ||
            YInc + PartHeight >= m_pCurrentFrame->getNumPartInCUSize()) // is not at the last column of LCU But is last row of LCU
        {
            checkBottomRight = false;
        }

        if (checkBottomRight && GetColMVP(RefPicList, bottomRightPartX, bottomRightPartY, ColMv, RefIdx))
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = ColMv;
        }
        else
        {
            uint32_t centerPartX = PartX + (PartWidth >> 1);
            uint32_t centerPartY = PartY + (PartHeight >> 1);

            if (GetColMVP(RefPicList, centerPartX, centerPartY, ColMv, RefIdx))
            {
                pInfo->MVCandidate[pInfo->NumbOfCands++] = ColMv;
            }
        }
    }

    // Add zero MVs. HEVC spec 8.5.3.2.5
    if (pInfo->NumbOfCands > AMVP_MAX_NUM_CAND)
    {
        pInfo->NumbOfCands = AMVP_MAX_NUM_CAND;
    }
    while (pInfo->NumbOfCands < AMVP_MAX_NUM_CAND)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands].setZero();
        pInfo->NumbOfCands++;
    }

    return;
}

// Check availability of spatial MV candidate which points to the same reference frame. HEVC spec 8.5.3.2.7 #6
bool H265SegmentDecoder::AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, int32_t RefIdx, int32_t NeibAddr, const H265MVInfo &motionInfo)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    int32_t candRefIdx = motionInfo.m_refIdx[RefPicList];
    if (candRefIdx >= 0 && m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList][candRefIdx].refFrame->m_PicOrderCnt)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands++] = motionInfo.m_mv[RefPicList];
        return true;
    }

    EnumRefPicList RefPicList2nd = EnumRefPicList(1 - RefPicList);
    RefIndexType NeibRefIdx = motionInfo.m_refIdx[RefPicList2nd];

    if (NeibRefIdx >= 0)
    {
        if(m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList2nd][NeibRefIdx].refFrame->m_PicOrderCnt)
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = motionInfo.m_mv[RefPicList2nd];
            return true;
        }
    }
    return false;
}

// Compute scaling factor from POC difference
static int32_t GetDistScaleFactor(int32_t DiffPocB, int32_t DiffPocD)
{
    if (DiffPocD == DiffPocB)
    {
        return 4096;
    }
    else
    {
        int32_t TDB = mfx::clamp(DiffPocB, -128, 127);
        int32_t TDD = mfx::clamp(DiffPocD, -128, 127);
        int32_t X = (0x4000 + abs(TDD / 2)) / TDD;
        int32_t Scale = mfx::clamp((TDB * X + 32) >> 6, -4096, 4095);
        return Scale;
    }
}

// Check availability of spatial motion vector candidate for any reference frame. HEVC spec 8.5.3.2.7 #7
bool H265SegmentDecoder::AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, int32_t RefIdx, int32_t NeibAddr, const H265MVInfo &motionInfo)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    int32_t CurrRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt;
    bool IsCurrRefLongTerm = m_pRefPicList[RefPicList][RefIdx].isLongReference;

    for (int32_t i = 0; i < 2; i++)
    {
        const RefIndexType &NeibRefIdx = motionInfo.m_refIdx[RefPicList];

        if (NeibRefIdx >= 0)
        {
            bool IsNeibRefLongTerm = m_pRefPicList[RefPicList][NeibRefIdx].isLongReference;

            if (IsNeibRefLongTerm == IsCurrRefLongTerm)
            {
                const H265MotionVector &MvPred = motionInfo.m_mv[RefPicList];

                if (IsCurrRefLongTerm)
                {
                    pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred;
                }
                else
                {
                    int32_t NeibRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][NeibRefIdx].refFrame->m_PicOrderCnt;
                    int32_t Scale = GetDistScaleFactor(CurrRefTb, NeibRefTb);

                    if (Scale == 4096)
                        pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred;
                    else
                        pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred.scaleMV(Scale);
                }

                return true;
            }
        }

        RefPicList = EnumRefPicList(1 - RefPicList);
    }

    return false;
}

// Check availability of collocated motion vector with given coordinates. HEVC spec 8.5.3.2.9
bool H265SegmentDecoder::GetColMVP(EnumRefPicList refPicListIdx, uint32_t PartX, uint32_t PartY, H265MotionVector &rcMv, int32_t refIdx)
{
    bool isCurrRefLongTerm, isColRefLongTerm;

    uint32_t Log2MinTUSize = m_pSeqParamSet->log2_min_transform_block_size;

    /* MV compression */
    if (Log2MinTUSize < 4)
    {
        uint32_t shift = (4 - Log2MinTUSize);
        PartX = (PartX >> shift) << shift;
        PartY = (PartY >> shift) << shift;
    }

    EnumRefPicList colPicListIdx = EnumRefPicList((B_SLICE == m_pSliceHeader->slice_type) ? 1 - m_pSliceHeader->collocated_from_l0_flag : 0);
    H265DecoderFrame *colPic = m_pRefPicList[colPicListIdx][m_pSliceHeader->collocated_ref_idx].refFrame;
    uint32_t colocatedPartNumber = PartY * m_pSeqParamSet->NumPartitionsInFrameWidth + PartX;

    // Intra mode is marked in ref list 0 only
    if (m_pSliceHeader->m_CheckLDC)
        colPicListIdx = refPicListIdx;
    else if (m_pSliceHeader->collocated_from_l0_flag)
        colPicListIdx = REF_PIC_LIST_1;
    else
        colPicListIdx = REF_PIC_LIST_0;

    if (COL_TU_INVALID_INTER == colPic->m_CodingData->GetTUFlags(colPicListIdx, colocatedPartNumber))
    {
        colPicListIdx = EnumRefPicList(1 - colPicListIdx);
        if (COL_TU_INVALID_INTER == colPic->m_CodingData->GetTUFlags(colPicListIdx, colocatedPartNumber))
            return false;
    }

    isCurrRefLongTerm = m_pRefPicList[refPicListIdx][refIdx].isLongReference;

    if (COL_TU_LT_INTER == colPic->m_CodingData->GetTUFlags(colPicListIdx, colocatedPartNumber))
        isColRefLongTerm = true;
    else
    {
        VM_ASSERT(COL_TU_ST_INTER == colPic->m_CodingData->GetTUFlags(colPicListIdx, colocatedPartNumber));
        isColRefLongTerm = false;
    }

    if (isCurrRefLongTerm != isColRefLongTerm)
        return false;

    H265MotionVector &cMvPred = colPic->m_CodingData->GetMV(colPicListIdx, colocatedPartNumber);

    if (isCurrRefLongTerm)
    {
        rcMv = cMvPred;
    }
    else
    {
        int32_t currRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[refPicListIdx][refIdx].refFrame->m_PicOrderCnt;
        int32_t colRefTb = colPic->m_CodingData->GetTUPOCDelta(colPicListIdx, colocatedPartNumber);
        int32_t scale = GetDistScaleFactor(currRefTb, colRefTb);

        if (scale == 4096)
            rcMv = cMvPred;
        else
            rcMv = cMvPred.scaleMV(scale);
    }

    return true;
}

} // namespace UMC_HEVC_DECODER
#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
