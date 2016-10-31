//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder.h"
#include "umc_h265_tr_quant.h"
#include "umc_h265_frame_info.h"
#include "mfx_h265_optimization.h"
#include "umc_h265_task_broker.h"

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))

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
    m_RecTpIntraRowFlags = (Ipp16u*)(m_RecIntraFlagsHolder + 35) + 1;

    // One element outside of CTB at each side for neighbour CTBs
    m_CurrCTBStride = (m_sps->NumPartitionsInCUSize + 2);
    if (m_CurrCTBFlagsHolder.size() < (Ipp32u)(m_CurrCTBStride * m_CurrCTBStride))
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

    Ipp32s sliceNum = slice->GetSliceNum();
    m_refPicList[0] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_refPicList[1] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;

    if (m_needToSplitDecAndRec && !slice->GetSliceHeader()->dependent_slice_segment_flag)
    {
        ResetRowBuffer();
        m_LastValidQP = slice->m_SliceHeader.SliceQP ^ 1; // Force QP recalculation because QP offsets may be different in new slice
        SetNewQP(slice->m_SliceHeader.SliceQP);
    }

    Ipp32s oneCUSize = sizeof(CoeffsPtr) * m_sps->MaxCUSize*m_sps->MaxCUSize;
    oneCUSize = oneCUSize*3 / 2; // add chroma coeffs

    Ipp32s rowCoeffSize;
    Ipp32s rowCount;

    Ipp32s widthInCU = m_sps->WidthInCU;

    if (task->m_threadingInfo)
    {
        widthInCU = task->m_threadingInfo->processInfo.m_width;
    }

    // decide number of buffers
    if (slice->m_iMaxMB - slice->m_iFirstMB > widthInCU)
    {
        rowCount = (slice->m_iMaxMB - slice->m_iFirstMB + widthInCU - 1) / widthInCU;
        rowCount = IPP_MIN(4, rowCount);
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
void DecodingContext::UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr)
{
    // Set local pointers to real array start positions
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    Ipp32u lastCUX = (lastCUAddr % m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    Ipp32u lastCUY = (lastCUAddr / m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    Ipp32u newCUX = (newCUAddr % m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;
    Ipp32u newCUY = (newCUAddr / m_sps->WidthInCU) * m_sps->NumPartitionsInCUSize;

    Ipp32s bottom_row_offset = m_CurrCTBStride * (m_CurrCTBStride - 2);
    if (newCUY > 0)
    {
        if (newCUX > lastCUX)
        {
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i] = TopNgbrs[newCUX + i];
            }
            // Store bottom margin for next row if next CTB is to the right. This is necessary for left-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
            }
        }
        else if (newCUX < lastCUX)
        {
            // Store bottom margin for next row if next CTB is to the left-below. This is necessary for right-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
            }
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i] = TopNgbrs[newCUX + i];
            }
        }
        else // New CTB right under previous CTB
        {
            // Copy data from bottom row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[i] = CurrCTBFlags[bottom_row_offset + i];
            }
        }
    }
    else
    {
        // Should be reset from the beginning and not filled up until 2nd row
        for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            VM_ASSERT(CurrCTBFlags[i].data == 0);
    }

    if (newCUX != lastCUX)
    {
        // Store bottom margin for next row if next CTB is not underneath. This is necessary for left-top diagonal
        for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
        {
            TopNgbrs[lastCUX + i].data = CurrCTBFlags[bottom_row_offset + i].data;
        }
    }

    if (lastCUY == newCUY)
    {
        // Copy left margin from right
        for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
        {
            CurrCTBFlags[i * m_CurrCTBStride] = CurrCTBFlags[i * m_CurrCTBStride + m_CurrCTBStride - 2];
        }
    }
    else
    {
        for (Ipp32s i = 1; i < m_CurrCTBStride; i++)
            CurrCTBFlags[i * m_CurrCTBStride].data = 0;
    }

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    // Clean inside of CU context
    for (Ipp32s i = 1; i < m_CurrCTBStride; i++)
        memset(&CurrCTBFlags[i * m_CurrCTBStride + 1], 0, (m_CurrCTBStride - 1)*sizeof(H265FrameHLDNeighborsInfo));
}

// Update reconstruct information with neighbour information for intra prediction
void DecodingContext::UpdateRecCurrCTBContext(Ipp32s lastCUAddr, Ipp32s newCUAddr)
{
    Ipp32s maxCUSzIn4x4 = m_sps->MaxCUSize >> 2;
    Ipp32s lCTBXx4 = lastCUAddr % m_sps->WidthInCU;
    Ipp32s nCTBXx4 = newCUAddr % m_sps->WidthInCU;
    bool   isSameY = ((lastCUAddr-lCTBXx4) == (newCUAddr-nCTBXx4));

    lCTBXx4 <<= (m_sps->log2_max_luma_coding_block_size - 2);
    nCTBXx4 <<= (m_sps->log2_max_luma_coding_block_size - 2);

    if(nCTBXx4 && isSameY) {
        m_RecLfIntraFlags[0] = m_RecLfIntraFlags[maxCUSzIn4x4];
        memset(m_RecLfIntraFlags+1, 0, maxCUSzIn4x4*sizeof(Ipp32u));
    } else {
        memset(m_RecLfIntraFlags, 0, (maxCUSzIn4x4+1)*sizeof(Ipp32u));
    }

    Ipp32u tDiag, diagIds;

    tDiag = (((Ipp32u)(m_RecTpIntraRowFlags[(nCTBXx4>>4)-1])) | ((Ipp32u)(m_RecTpIntraRowFlags[(nCTBXx4>>4)]) << 16)) >> 15;
    tDiag = (tDiag >> ((nCTBXx4 & 0xF))) & 0x1;

    Ipp32u mask = ((0x1 << maxCUSzIn4x4) - 1) << (lCTBXx4 & 0xF) & 0x0000FFFF;
    m_RecTpIntraRowFlags[lCTBXx4>>4] &= ~mask;
    m_RecTpIntraRowFlags[lCTBXx4>>4] |= ((m_RecTpIntraFlags[maxCUSzIn4x4] << (lCTBXx4 & 0xF)) & 0x0000FFFF);

    memset(m_RecTpIntraFlags, 0, (maxCUSzIn4x4+1)*sizeof(Ipp32u));
    m_RecTpIntraFlags[0] =  ((Ipp32u)(m_RecTpIntraRowFlags[nCTBXx4>>4])) | ((Ipp32u)(m_RecTpIntraRowFlags[(nCTBXx4>>4)+1]) << 16);
    m_RecTpIntraFlags[0] >>= (nCTBXx4 & 0xF);

    diagIds  = (m_RecTpIntraFlags[0] << 1) | tDiag;
    tDiag    = m_RecLfIntraFlags[0];

    for(Ipp32s i=0; i<maxCUSzIn4x4-1; i++) {
        diagIds <<= 1; diagIds |= (tDiag & 0x1); tDiag >>= 1;
    }

    *m_RecTLIntraFlags = diagIds;
}

// Clean up all availability information for decoder
void DecodingContext::ResetRowBuffer()
{
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    for (Ipp32u i = 0; i < m_sps->NumPartitionsInFrameWidth + 2; i++)
        TopNgbrs[i].data = 0;

    for (Ipp32s i = 0; i < m_CurrCTBStride * m_CurrCTBStride; i++)
        CurrCTBFlags[i].data = 0;
}

// Clean up all availability information for reconstruct
void DecodingContext::ResetRecRowBuffer()
{
    memset(m_RecIntraFlagsHolder, 0, sizeof(m_RecIntraFlagsHolder));
}

// Set new QP value and calculate scaled values for luma and chroma
void DecodingContext::SetNewQP(Ipp32s newQP, Ipp32s chroma_offset_idx)
{
    if (chroma_offset_idx != -1) // update chroma part
    {
        Ipp32s qpOffsetCb = m_pps->pps_cb_qp_offset + m_sh->slice_cb_qp_offset + m_pps->cb_qp_offset_list[chroma_offset_idx];
        Ipp32s qpOffsetCr = m_pps->pps_cr_qp_offset + m_sh->slice_cr_qp_offset + m_pps->cr_qp_offset_list[chroma_offset_idx];
        Ipp32s qpBdOffsetC = m_sps->m_QPBDOffsetC;
        Ipp32s qpScaledCb = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCb);
        Ipp32s qpScaledCr = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCr);

        Ipp32s chromaScaleIndex = m_sps->ChromaArrayType != CHROMA_FORMAT_420 ? 1 : 0;

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

        return;
    }
    
    if (newQP == m_LastValidQP)
        return;

    m_LastValidQP = newQP;

    Ipp32s qpScaledY = m_LastValidQP + m_sps->m_QPBDOffsetY;
    Ipp32s qpOffsetCb = m_pps->pps_cb_qp_offset + m_sh->slice_cb_qp_offset;
    Ipp32s qpOffsetCr = m_pps->pps_cr_qp_offset + m_sh->slice_cr_qp_offset;
    Ipp32s qpBdOffsetC = m_sps->m_QPBDOffsetC;
    Ipp32s qpScaledCb = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCb);
    Ipp32s qpScaledCr = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCr);

    Ipp32s chromaScaleIndex = m_sps->ChromaArrayType != CHROMA_FORMAT_420 ? 1 : 0;

    if (qpScaledCb < 0)
        qpScaledCb = qpScaledCb + qpBdOffsetC;
    else
        qpScaledCb = g_ChromaScale[chromaScaleIndex][qpScaledCb] + qpBdOffsetC;

    if (qpScaledCr < 0)
        qpScaledCr = qpScaledCr + qpBdOffsetC;
    else
        qpScaledCr = g_ChromaScale[chromaScaleIndex][qpScaledCr] + qpBdOffsetC;

    m_ScaledQP[COMPONENT_LUMA].m_QPRem = qpScaledY % 6;
    m_ScaledQP[COMPONENT_LUMA].m_QPPer = qpScaledY / 6;
    m_ScaledQP[COMPONENT_LUMA].m_QPScale =
        g_invQuantScales[m_ScaledQP[COMPONENT_LUMA].m_QPRem] << m_ScaledQP[COMPONENT_LUMA].m_QPPer;

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

    if (!m_ppcYUVResi || (Ipp32u)m_ppcYUVResi->lumaSize().width != sps->MaxCUSize || (Ipp32u)m_ppcYUVResi->lumaSize().height != sps->MaxCUSize)
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
UMC::Status H265SegmentDecoder::Init(Ipp32s iNumber)
{
    // release object before initialization
    Release();

    // save ordinal number
    m_iNumber = iNumber;

    return UMC::UMC_OK;

} // Status H265SegmentDecoder::Init(Ipp32s sNumber)

// Decode SAO truncated rice offset value. HEVC spec 9.3.3.2
Ipp32s H265SegmentDecoder::parseSaoMaxUvlc(Ipp32s maxSymbol)
{
    VM_ASSERT(maxSymbol != 0);

    Ipp32u code = m_pBitStream->DecodeSingleBinEP_CABAC();

    if (code == 0)
    {
        return 0;
    }

    Ipp32s i = 1;
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
Ipp32s H265SegmentDecoder::parseSaoTypeIdx()
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
void H265SegmentDecoder::parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx)
{
    Ipp32s typeIdx;

    if (compIdx == 2)
    {
        typeIdx = psSaoLcuParam->m_typeIdx[1];
    }
    else
    {
        typeIdx = parseSaoTypeIdx() - 1;
        psSaoLcuParam->m_typeIdx[compIdx] = (Ipp8s)typeIdx;
    }

    if (typeIdx < 0)
        return;

    Ipp32s bitDepth = compIdx ? m_pSeqParamSet->bit_depth_chroma : m_pSeqParamSet->bit_depth_luma;
    Ipp32s offsetTh = (1 << IPP_MIN(bitDepth - 5, 5)) - 1;

    if (typeIdx == SAO_BO)
    {
        // Band offset
        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            psSaoLcuParam->m_offset[compIdx][i] = parseSaoMaxUvlc(offsetTh);
        }

        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            if (psSaoLcuParam->m_offset[compIdx][i] != 0)
            {
                Ipp32s sign = m_pBitStream->DecodeSingleBinEP_CABAC();

                if (sign)
                {
                    psSaoLcuParam->m_offset[compIdx][i] = -psSaoLcuParam->m_offset[compIdx][i];
                }
            }
        }

        psSaoLcuParam->m_subTypeIdx[compIdx] = (Ipp8s)m_pBitStream->DecodeBypassBins_CABAC(5);
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
            psSaoLcuParam->m_subTypeIdx[compIdx] = (Ipp8s)m_pBitStream->DecodeBypassBins_CABAC(2);
            psSaoLcuParam->m_typeIdx[compIdx] += psSaoLcuParam->m_subTypeIdx[compIdx];
        }
    }
}

// Decode CTB SAO information
void H265SegmentDecoder::DecodeSAOOneLCU()
{
    if (m_pSliceHeader->slice_sao_luma_flag || m_pSliceHeader->slice_sao_chroma_flag)
    {
        Ipp32u curCUAddr = m_cu->CUAddr;
        Ipp32s numCuInWidth  = m_pCurrentFrame->m_CodingData->m_WidthInCU;
        Ipp32s sliceStartRS = m_pCurrentFrame->m_CodingData->getCUOrderMap(m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->getCD()->getNumPartInCU());
        Ipp32s cuAddrLeftInSlice = curCUAddr - 1;
        Ipp32s cuAddrUpInSlice  = curCUAddr - numCuInWidth;
        Ipp32s rx = curCUAddr % numCuInWidth;
        Ipp32s ry = curCUAddr / numCuInWidth;
        Ipp32s sao_merge_left_flag = 0;
        Ipp32s sao_merge_up_flag   = 0;

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
                                                    Ipp32s sao_merge_left_flag,
                                                    Ipp32s sao_merge_up_flag)
{
    Ipp32s iAddr = m_cu->CUAddr;

    SAOLCUParam* saoLcuParam = m_pCurrentFrame->getCD()->m_saoLcuParam;

    sao_merge_left_flag = sao_merge_left_flag ? m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]) : 0;
    sao_merge_up_flag = (sao_merge_up_flag && !sao_merge_left_flag) ? m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]) : 0;

    for (Ipp32s iCompIdx = 0; iCompIdx < (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400 ? 3 : 1); iCompIdx++)
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
void H265SegmentDecoder::ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol)
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

    Ipp32u val = 0;
    Ipp32u cont;

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
void H265SegmentDecoder::DecodeCUCABAC(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    Ipp32u CurNumParts = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts = CurNumParts >> 2;

    Ipp32u more_depth = 0;

    Ipp32u LPelX = m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    LPelX += m_cu->m_CUPelX;
    Ipp32u RPelX = LPelX + (m_pCurrentFrame->getCD()->m_MaxCUWidth >> Depth) - 1;
    Ipp32u TPelY = m_cu->m_rasterToPelY[AbsPartIdx];
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    TPelY += m_cu->m_CUPelY;
    Ipp32u BPelY = TPelY + (m_pCurrentFrame->getCD()->m_MaxCUWidth >> Depth) - 1;
    Ipp32s PartSize = m_pSeqParamSet->MaxCUSize >> (Depth + m_pSeqParamSet->log2_min_transform_block_size);

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
        Ipp32u Idx = AbsPartIdx;
        if ((m_pSeqParamSet->MaxCUSize >> Depth) == m_minCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            m_DecodeDQPFlag = true;
            m_context->SetNewQP(getRefQP(AbsPartIdx));
        }

        for ( Ipp32u PartUnitIdx = 0; PartUnitIdx < 4; PartUnitIdx++ )
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
        Ipp8u InterDirNeighbours[MERGE_MAX_NUM_CAND];

        for (Ipp32s ui = 0; ui < m_cu->m_SliceHeader->max_num_merge_cand; ++ui)
        {
            InterDirNeighbours[ui] = 0;
        }
        Ipp32s MergeIndex = DecodeMergeIndexCABAC();
        getInterMergeCandidates(AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, MergeIndex);

        H265MVInfo &MVi = MvBufferNeighbours[MergeIndex];
        for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
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

    Ipp32s PredMode = DecodePredModeCABAC(AbsPartIdx);
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
bool H265SegmentDecoder::DecodeSplitFlagCABAC(Ipp32s PartX, Ipp32s PartY, Ipp32u Depth)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SPLIT_CODING_UNIT_FLAG_HEVC] + getCtxSplitFlag(PartX, PartY, Depth));

    return uVal != 0;
}

// Decode CU transquant bypass flag
bool H265SegmentDecoder::DecodeCUTransquantBypassFlag(Ipp32u AbsPartIdx)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANSQUANT_BYPASS_HEVC]);
    m_cu->setCUTransquantBypass(uVal ? true : false, AbsPartIdx);
    return uVal ? true : false;
}

// Decode CU skip flag
bool H265SegmentDecoder::DecodeSkipFlagCABAC(Ipp32s PartX, Ipp32s PartY)
{
    if (I_SLICE == m_pSliceHeader->slice_type)
    {
        return false;
    }

    Ipp32u uVal = 0;
    Ipp32u CtxSkip = getCtxSkipFlag(PartX, PartY);
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SKIP_FLAG_HEVC] + CtxSkip);

    return uVal != 0;
}

// Decode inter PU merge index
Ipp32u H265SegmentDecoder::DecodeMergeIndexCABAC(void)
{
    Ipp32u NumCand = MERGE_MAX_NUM_CAND;
    Ipp32u UnaryIdx = 0;
    NumCand = m_pSliceHeader->max_num_merge_cand;

    if (NumCand >= 1)
    {
        for ( ; UnaryIdx < NumCand - 1; ++UnaryIdx)
        {
            Ipp32u uVal = 0;

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
void H265SegmentDecoder::DecodeMVPIdxPUCABAC(Ipp32u AbsPartAddr, Ipp32u PartIdx, EnumRefPicList RefList, H265MVInfo &MVi, Ipp8u InterDir)
{
    Ipp32u MVPIdx = 0;
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
Ipp32s H265SegmentDecoder::DecodePredModeCABAC(Ipp32u AbsPartIdx)
{
    Ipp32s PredMode = MODE_INTER;

    if (I_SLICE == m_cu->m_SliceHeader->slice_type)
    {
        PredMode = MODE_INTRA;
    }
    else
    {
        Ipp32u uVal;
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PRED_MODE_HEVC]);
        PredMode += uVal;
    }

    m_cu->setPredMode(EnumPredMode(PredMode), AbsPartIdx);
    return PredMode;
}

// Decode partition size. HEVC spec 9.3.3.5
void H265SegmentDecoder::DecodePartSizeCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal, uMode = 0;
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

        Ipp32u TrLevel = 0;
        Ipp32u WidthInBit  = g_ConvertToBit[m_cu->GetWidth(AbsPartIdx)] + 2;
        Ipp32u TrSizeInBit = g_ConvertToBit[m_pSeqParamSet->m_maxTrSize] + 2;
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
        Ipp32u MaxNumBits = 2;
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth && !((m_pSeqParamSet->MaxCUSize >> Depth) == 8))
        {
            MaxNumBits++;
        }
        for (Ipp32u ui = 0; ui < MaxNumBits; ui++)
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
void H265SegmentDecoder::DecodeIPCMInfoCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (!m_pSeqParamSet->pcm_enabled_flag
        || (m_cu->GetWidth(AbsPartIdx) > (1 << m_pSeqParamSet->log2_max_pcm_luma_coding_block_size))
        || (m_cu->GetWidth(AbsPartIdx) < (1 << m_pSeqParamSet->log2_min_pcm_luma_coding_block_size)))
    {
        m_cu->setIPCMFlag(false, AbsPartIdx);
        return;
    }

    Ipp32u uVal;

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
    Ipp32u size = m_cu->GetWidth(AbsPartIdx);

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
    Ipp32s iVal = m_pBitStream->getNumBitsUntilByteAligned();

    if (iVal)
        m_pBitStream->GetBits(iVal);
}

// Decode luma intra direction
void H265SegmentDecoder::DecodeIntraDirLumaAngCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize mode = m_cu->GetPartitionSize(AbsPartIdx);
    Ipp32u PartNum = mode == PART_SIZE_NxN ? 4 : 1;
    Ipp32u PartOffset = (m_cu->m_Frame->m_CodingData->m_NumPartitions >> (m_cu->GetDepth(AbsPartIdx) << 1)) >> 2;
    Ipp32u prev_intra_luma_pred_flag[4];
    Ipp32s IPredMode;

    if (mode == PART_SIZE_NxN)
        Depth++;

    // Prev intra luma pred flag
    for (Ipp32u j = 0; j < PartNum; j++)
    {
        prev_intra_luma_pred_flag[j] = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_LUMA_PRED_MODE_HEVC]);
    }

    for (Ipp32u j = 0; j < PartNum; j++)
    {
        Ipp32s Preds[3] = {-1, -1, -1};
        Ipp32u PredNum = 3;

        getIntraDirLumaPredictor(AbsPartIdx + PartOffset * j, Preds);

        if (prev_intra_luma_pred_flag[j])
        {
            // mpm idx
            Ipp32u mpm_idx = m_pBitStream->DecodeSingleBinEP_CABAC();
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
            for (Ipp32u i = 0; i < PredNum; i++)
            {
                IPredMode += (IPredMode >= Preds[i]);
            }
        }
        m_cu->setLumaIntraDirSubParts(IPredMode, AbsPartIdx + PartOffset * j, Depth);
    }
}

// Decode intra chroma direction. HEVC spec 9.3.3.6
void H265SegmentDecoder::DecodeIntraDirChromaCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_CHROMA_PRED_MODE_HEVC]);

    //switch codeword
    if (uVal == 0)
    {
        //INTRA_DM_CHROMA_IDX case
        uVal = m_cu->GetLumaIntra(AbsPartIdx);
    }
    else
    {
        Ipp32u IPredMode;

        IPredMode = m_pBitStream->DecodeBypassBins_CABAC(2);
        Ipp32u AllowedChromaDir[INTRA_NUM_CHROMA_MODE];
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
bool H265SegmentDecoder::DecodePUWiseCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize PartSize = m_cu->GetPartitionSize(AbsPartIdx);
    Ipp32u NumPU = (PartSize == PART_SIZE_2Nx2N ? 1 : (PartSize == PART_SIZE_NxN ? 4 : 2));
    Ipp32u PUOffset = (g_PUOffset[Ipp32u(PartSize)] << ((m_pSeqParamSet->MaxCUDepth - Depth) << 1)) >> 4;

    H265MVInfo MvBufferNeighbours[MERGE_MAX_NUM_CAND]; // double length for mv of both lists
    Ipp8u InterDirNeighbours[MERGE_MAX_NUM_CAND];

    for (Ipp32s ui = 0; ui < m_pSliceHeader->max_num_merge_cand; ui++ )
        InterDirNeighbours[ui] = 0;

    bool isFirstMerge = false;

    for (Ipp32u PartIdx = 0, SubPartIdx = AbsPartIdx; PartIdx < NumPU; PartIdx++, SubPartIdx += PUOffset)
    {
        Ipp8u InterDir = 0;
        // Decode merge flag
        bool bMergeFlag = DecodeMergeFlagCABAC();
        if (0 == PartIdx)
            isFirstMerge = bMergeFlag;

        Ipp32s PartWidth, PartHeight;
        m_cu->getPartSize(AbsPartIdx, PartIdx, PartWidth, PartHeight);

        Ipp32s LPelX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[SubPartIdx];
        Ipp32s TPelY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[SubPartIdx];
        Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
        PartWidth >>= m_pSeqParamSet->log2_min_transform_block_size;
        PartHeight >>= m_pSeqParamSet->log2_min_transform_block_size;

        H265MVInfo MVi;
        MVi.m_refIdx[REF_PIC_LIST_0] = MVi.m_refIdx[REF_PIC_LIST_1] = -1;

        if (bMergeFlag)
        {
            // Merge branch
            Ipp32u MergeIndex = DecodeMergeIndexCABAC();

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

            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
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
            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
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
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_FLAG_HEVC]);
    return uVal ? true : false;
}

// Decode inter direction for a PU block. HEVC spec 9.3.3.7
Ipp8u H265SegmentDecoder::DecodeInterDirPUCABAC(Ipp32u AbsPartIdx)
{
    Ipp8u InterDir;

    if (P_SLICE == m_pSliceHeader->slice_type)
    {
        InterDir = 1;
    }
    else
    {
        Ipp32u uVal;
        Ipp32u Ctx = m_cu->GetDepth(AbsPartIdx);

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
        InterDir = Ipp8u(uVal);
    }

    return InterDir;
}

// Decode truncated rice reference frame index for AMVP PU
RefIndexType H265SegmentDecoder::DecodeRefFrmIdxPUCABAC(EnumRefPicList RefList, Ipp8u InterDir)
{
    RefIndexType RefFrmIdx;
    Ipp32s ParseRefFrmIdx = InterDir & (1 << RefList);

    if (m_pSliceHeader->m_numRefIdx[RefList] > 1 && ParseRefFrmIdx)
    {
        Ipp32u uVal;
        Ipp32u Ctx = 0;

        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[REF_FRAME_IDX_HEVC] + Ctx);
        if (uVal)
        {
            Ipp32u RefNum = m_pSliceHeader->m_numRefIdx[RefList] - 2;
            Ctx++;
            Ipp32u i;
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
void H265SegmentDecoder::DecodeMVdPUCABAC(EnumRefPicList RefList, H265MotionVector &MVd, Ipp8u InterDir)
{
    if (InterDir & (1 << RefList))
    {
        Ipp32u uVal;
        Ipp32s HorAbs, VerAbs;
        Ipp32u HorSign = 0, VerSign = 0;

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
            Ipp32u Ctx = 1;

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

        MVd.Horizontal = (Ipp16s)HorAbs;
        MVd.Vertical = (Ipp16s)VerAbs;
    }
}

// Decode EG1 coded abs_mvd_minus2 values
void H265SegmentDecoder::ReadEpExGolombCABAC(Ipp32u& Value, Ipp32u Count)
{
    Ipp32u uVal = 0;
    Ipp32u uBit = 1;

    while (uBit)
    {
        uBit = m_pBitStream->DecodeSingleBinEP_CABAC();
        uVal += uBit << Count++;
    }

    if (--Count)
    {
        Ipp32u bins;
        bins = m_pBitStream->DecodeBypassBins_CABAC(Count);
        uVal += bins;
    }

    Value = uVal;

    return;
}

// Decode all CU coefficients
void H265SegmentDecoder::DecodeCoeff(Ipp32u AbsPartIdx, Ipp32u Depth, bool& CodeDQP, bool isFirstPartMerge)
{
    if (MODE_INTRA != m_cu->GetPredictionMode(AbsPartIdx))
    {
        Ipp32u QtRootCbf = 1;
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

    const Ipp32u Log2TrafoSize = m_pSeqParamSet->log2_max_luma_coding_block_size - Depth;

    m_bakAbsPartIdxQp = AbsPartIdx;
    DecodeTransform(AbsPartIdx, Depth, Log2TrafoSize, 0, CodeDQP);
}

// Recursively decode TU data
void H265SegmentDecoder::DecodeTransform(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  log2TrafoSize, Ipp32u trafoDepth, bool& CodeDQP)
{
    Ipp32u Subdiv;

    if (log2TrafoSize == 2)
    {
        if ((AbsPartIdx & 0x03) == 0)
        {
            m_BakAbsPartIdxChroma = AbsPartIdx;
        }
    }

    Ipp32s IntraSplitFlag = m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA && m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_NxN;

    Ipp32u Log2MaxTrafoSize = m_pSeqParamSet->log2_max_transform_block_size;
    Ipp32u Log2MinTrafoSize = m_pSeqParamSet->log2_min_transform_block_size;

    Ipp32u MaxTrafoDepth = m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? (m_pSeqParamSet->max_transform_hierarchy_depth_intra + IntraSplitFlag ) : m_pSeqParamSet->max_transform_hierarchy_depth_inter;

    // Check for implicit or explicit TU split
    if (log2TrafoSize <= Log2MaxTrafoSize && log2TrafoSize > Log2MinTrafoSize && trafoDepth < MaxTrafoDepth && !(IntraSplitFlag && (trafoDepth == 0)) )
    {
        VM_ASSERT(log2TrafoSize > m_cu->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
        ParseTransformSubdivFlagCABAC(Subdiv, 5 - log2TrafoSize);
    }
    else
    {
        Ipp32s interSplitFlag = m_pSeqParamSet->max_transform_hierarchy_depth_inter == 0 && m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTER &&
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

        const Ipp32u QPartNum = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
        const Ipp32u StartAbsPartIdx = AbsPartIdx;
        Ipp8u YCbf = 0;
        Ipp8u UCbf = 0;
        Ipp8u VCbf = 0;
        Ipp8u U1Cbf = 0;
        Ipp8u V1Cbf = 0;

        // Recursively parse sub-TUs
        for (Ipp32s i = 0; i < 4; i++)
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

        for (Ipp32u ui = 0; ui < 4 * QPartNum; ++ui)
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
        Ipp8u cbfY = m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx, trafoDepth);
        Ipp8u cbfU;
        Ipp8u cbfV;
        Ipp8u cbfU1;
        Ipp8u cbfV1;

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

        Ipp32u coeffSize = 1 << (log2TrafoSize << 1);

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
void H265SegmentDecoder::ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize)
{
    SubdivFlag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANS_SUBDIV_FLAG_HEVC] + Log2TransformBlockSize);
}

// Returns selected CABAC context for CBF with specified depth
Ipp32u getCtxQtCbf(ComponentPlane plane, Ipp32u TrDepth)
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
void H265SegmentDecoder::ParseQtCbfCABAC(Ipp32u AbsPartIdx, ComponentPlane plane, Ipp32u TrDepth, Ipp32u Depth)
{
    const Ipp32u Ctx = getCtxQtCbf(plane, TrDepth);

    Ipp32u uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_CBF_HEVC] + Ctx + NUM_CONTEXT_QT_CBF * (plane ? COMPONENT_CHROMA : plane));

    m_cu->setCbfSubParts(uVal << TrDepth, plane, AbsPartIdx, Depth);
}

// Decode root CU CBF value
void H265SegmentDecoder::ParseQtRootCbfCABAC(Ipp32u& QtRootCbf)
{
    Ipp32u uVal;
    const Ipp32u Ctx = 0;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_ROOT_CBF_HEVC] + Ctx);
    QtRootCbf = uVal;
}

void H265SegmentDecoder::DecodeQPChromaAdujst()
{
    Ipp32u cu_chroma_qp_offset_flag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[CU_CHROMA_QP_OFFSET_FLAG]);
    if (!cu_chroma_qp_offset_flag)
        return;

    Ipp32u cu_chroma_qp_offset_idx = 1;
    if (cu_chroma_qp_offset_flag && m_pPicParamSet->chroma_qp_offset_list_len > 1)
    {
        ReadUnaryMaxSymbolCABAC(cu_chroma_qp_offset_idx, ctxIdxOffsetHEVC[CU_CHROMA_QP_OFFSET_IDX], 0, m_pPicParamSet->chroma_qp_offset_list_len - 1);
        cu_chroma_qp_offset_idx++;
    }

    m_context->SetNewQP(0, cu_chroma_qp_offset_idx);
}

// Decode and set new QP value
void H265SegmentDecoder::DecodeQP(Ipp32u AbsPartIdx)
{
    if (m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        ParseDeltaQPCABAC(AbsPartIdx);
    }
}

// Decode and set new QP value. HEVC spec 9.3.3.8
void H265SegmentDecoder::ParseDeltaQPCABAC(Ipp32u AbsPartIdx)
{
    Ipp32s qp;
    Ipp32u uiDQp;

    ReadUnaryMaxSymbolCABAC(uiDQp, ctxIdxOffsetHEVC[DQP_HEVC], 1, CU_DQP_TU_CMAX);

    if (uiDQp >= CU_DQP_TU_CMAX)
    {
        Ipp32u uVal;
        ReadEpExGolombCABAC(uVal, CU_DQP_EG_k);
        uiDQp += uVal;
    }

    if (uiDQp > 0)
    {
        Ipp32u Sign = m_pBitStream->DecodeSingleBinEP_CABAC();
        Ipp32s CuQpDeltaVal = Sign ? -(Ipp32s)uiDQp : uiDQp;

        Ipp32s QPBdOffsetY = m_pSeqParamSet->getQpBDOffsetY();

        if (CuQpDeltaVal < -(26 + QPBdOffsetY / 2) || CuQpDeltaVal > (25 + QPBdOffsetY / 2))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM); 

        qp = (((Ipp32s)getRefQP(AbsPartIdx) + CuQpDeltaVal + 52 + 2 * QPBdOffsetY) % (52 + QPBdOffsetY)) - QPBdOffsetY;
    }
    else
    {
        qp = getRefQP(AbsPartIdx);
    }

    m_context->SetNewQP(qp);
    m_cu->m_CodedQP = (Ipp8u)qp;
}

// Check whether this CU was last in slice
void H265SegmentDecoder::FinishDecodeCU(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    IsLast = DecodeSliceEnd(AbsPartIdx, Depth);
    if (m_pSliceHeader->cu_chroma_qp_offset_enabled_flag)
        m_context->SetNewQP(0, 0); // clear chroma qp offsets
}

// Copy CU data from position 0 to all other subparts
// This information is needed for reconstruct which may be done in another thread
void H265SegmentDecoder::BeforeCoeffs(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    H265CodingUnitData * data = m_cu->GetCUData(AbsPartIdx);
    data->qp = (Ipp8s)m_context->GetQP();
    m_cu->SetCUDataSubParts(AbsPartIdx, Depth);
}

// Decode slice end flag if necessary
bool H265SegmentDecoder::DecodeSliceEnd(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u IsLast;
    H265DecoderFrame* pFrame = m_pCurrentFrame;
    Ipp32u CurNumParts = pFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u Width = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32u Height = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32u GranularityWidth = m_pSeqParamSet->MaxCUSize;
    Ipp32u PosX = m_cu->m_CUPelX + m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32u PosY = m_cu->m_CUPelY + m_cu->m_rasterToPelY[AbsPartIdx];

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
        if (m_pSliceHeader->dependent_slice_segment_flag)
        {
            m_pSliceHeader->m_sliceSegmentCurEndCUAddr = m_cu->getSCUAddr() + AbsPartIdx + CurNumParts - 1;
        }
        else
        {
            m_pSliceHeader->m_sliceSegmentCurEndCUAddr = m_cu->getSCUAddr() + AbsPartIdx + CurNumParts - 1;
        }
    }
    else
    {
        IsLast = m_pBitStream->CheckBSLeft();
    }

    return (IsLast > 0);
}

// Returns CABAC context index for sig_coeff_flag. HEVC spec 9.3.4.2.5
static inline H265_FORCEINLINE Ipp32u getSigCtxInc(Ipp32s patternSigCtx,
                                 Ipp32u scanIdx,
                                 const Ipp32u PosX,
                                 const Ipp32u PosY,
                                 const Ipp32u log2BlockSize,
                                 bool IsLuma)
{
    //LUMA map
    static const Ipp32u ctxIndMap[16] =
    {
        0, 1, 4, 5,
        2, 3, 4, 5,
        6, 6, 8, 8,
        7, 7, 8, 8
    };

    static const Ipp32u lCtx[4] = {0x00010516, 0x06060606, 0x000055AA, 0xAAAAAAAA};

    if ((PosX|PosY) == 0)
        return 0;

    if (log2BlockSize == 2)
    {
        return ctxIndMap[(PosY<<2) + PosX];
    }

    Ipp32u Offset = log2BlockSize == 3 ? (scanIdx == SCAN_DIAG ? 9 : 15) : (IsLuma ? 21 : 12);

    Ipp32s posXinSubset = PosX & 0x3;
    Ipp32s posYinSubset = PosY & 0x3;

    Ipp32s cnt = (lCtx[patternSigCtx] >> (((posXinSubset<<2) + posYinSubset)<<1)) & 0x3;

    return ((IsLuma && ((PosX|PosY) > 3) ) ? 3 : 0) + Offset + cnt;
}

static const Ipp8u diagOffs[8] = {0, 1, 3, 6, 10, 15, 21, 28};
static const Ipp32u stXoffs[3] = { 0xFB9E4910, 0xE4E4E4E4, 0xFFAA5500 };
static const Ipp32u stYoffs[3] = { 0xEDB1B184, 0xFFAA5500, 0xE4E4E4E4 };

// Raster to Z-scan conversion
static inline H265_FORCEINLINE Ipp32u Rst2ZS(const Ipp32u x, const Ipp32u y, const Ipp32u l2w)
{
    const Ipp32u diagId = x + y;
    const Ipp32u w = 1<<l2w;

    return (diagId < w) ? (diagOffs[diagId] + x) : ((1<<(l2w<<1)) - diagOffs[((w<<1)-1)-diagId] + (w-y-1));
}

// Parse TU coefficients
void H265SegmentDecoder::ParseCoeffNxNCABAC(CoeffsPtr pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, ComponentPlane plane)
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

#pragma warning(disable: 4127)

// Parse TU coefficients
template <bool scaling_list_enabled_flag>
void H265SegmentDecoder::ParseCoeffNxNCABACOptimized(CoeffsPtr pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, ComponentPlane plane)
{
    const Ipp32u Size    = 1 << Log2BlockSize;
    const Ipp32u L2Width = Log2BlockSize-2;

    const bool  IsLuma  = (plane == COMPONENT_LUMA);
    const bool  isBypass = m_cu->GetCUTransquantBypass(AbsPartIdx);
    const bool  beValid = isBypass ? false : m_pPicParamSet->sign_data_hiding_enabled_flag != 0;

    const Ipp32u baseCoeffGroupCtxIdx = ctxIdxOffsetHEVC[SIG_COEFF_GROUP_FLAG_HEVC] + (IsLuma ? 0 : NUM_CONTEXT_SIG_COEFF_GROUP_FLAG);
    const Ipp32u baseCtxIdx = ctxIdxOffsetHEVC[SIG_FLAG_HEVC] + ( IsLuma ? 0 : NUM_CONTEXT_SIG_FLAG_LUMA);

   if (m_pPicParamSet->transform_skip_enabled_flag && !L2Width && !isBypass)
        ParseTransformSkipFlags(AbsPartIdx, plane);

    // adujst U1, V1 to U/V
   if (plane == COMPONENT_CHROMA_U1)
       plane = COMPONENT_CHROMA_U;
   if (plane == COMPONENT_CHROMA_V1)
       plane = COMPONENT_CHROMA_V;

    memset(pCoef, 0, sizeof(Coeffs) << (Log2BlockSize << 1));

    Ipp32u ScanIdx = m_cu->getCoefScanIdx(AbsPartIdx, Log2BlockSize, IsLuma, m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA);
    Ipp32u PosLastY, PosLastX;
    Ipp32u BlkPosLast = ParseLastSignificantXYCABAC(PosLastX, PosLastY, L2Width, IsLuma, ScanIdx);

    pCoef[BlkPosLast] = 1;

    Ipp32u LastScanSet;

    Ipp32u CGPosX    = PosLastX >> 2;
    Ipp32u CGPosY    = PosLastY >> 2;
    Ipp32u LastXinCG = PosLastX & 0x3;
    Ipp32u LastYinCG = PosLastY & 0x3;
    Ipp32s LastPinCG;

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

    Ipp32u c1 = 1;

    Ipp16u SCGroupFlagRightMask = 0;
    Ipp16u SCGroupFlagLowerMask = 0;
    Ipp16s pos[SCAN_SET_SIZE];

    const Ipp8u *sGCr = scanGCZr + (1<<(L2Width<<1))*(ScanIdx+2) - LastScanSet - 1;

    Ipp32u c_Log2TrSize = g_ConvertToBit[Size] + 2;
    Ipp32u bit_depth = IsLuma ? m_context->m_sps->bit_depth_luma : m_context->m_sps->bit_depth_chroma;
    Ipp32u TransformShift = MAX_TR_DYNAMIC_RANGE - bit_depth - c_Log2TrSize;

    Ipp32s Shift;
    Ipp32s Add(0), Scale;
    Ipp16s *pDequantCoef;
    bool shift_right = true;

    if (scaling_list_enabled_flag)
    {
        Ipp32s QPRem = m_context->m_ScaledQP[plane].m_QPRem;
        Ipp32s QPPer = m_context->m_ScaledQP[plane].m_QPPer;

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

    for (Ipp32s SubSet = LastScanSet; SubSet >= 0; SubSet--)
    {
        Ipp32u absSum;
        Ipp32u CGBlkPos;
        Ipp32u GoRiceParam = 0;
        bool   SigCoeffGroup;
        Ipp32u numNonZero = 0;
        Ipp32s lPosNZ = -1, fPosNZ;

        if ((Ipp32u)SubSet == LastScanSet) {
            lPosNZ = fPosNZ = -1;
            LastPinCG--;
            pos[numNonZero] = (Ipp16s)BlkPosLast;
            numNonZero = 1;
        }

        CGBlkPos = *sGCr++;
        CGPosY   = CGBlkPos >> L2Width;
        CGPosX   = CGBlkPos - (CGPosY << L2Width);

        Ipp16u patternSigCtx = ((SCGroupFlagRightMask >> CGPosY) & 0x1) | (((SCGroupFlagLowerMask >> CGPosX) & 0x1)<<1);

        if ((Ipp32u)SubSet == LastScanSet || SubSet == 0) {
            SigCoeffGroup = 1;
        } else {
            SigCoeffGroup = m_pBitStream->DecodeSingleBin_CABAC(baseCoeffGroupCtxIdx + (patternSigCtx != 0)) != 0;
        }

        Ipp32u BlkPos, PosY, PosX, Sig, CtxSig, xyOffs = LastPinCG<<1;

        if (SigCoeffGroup)
        {
            SCGroupFlagRightMask |= (1 << CGPosY);
            SCGroupFlagLowerMask |= (1 << CGPosX);
            for(Ipp32s cPos = 0; cPos <= LastPinCG; cPos++)
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

                pCoef[BlkPos] = (Ipp16s)Sig;
                pos[numNonZero] = (Ipp16s)BlkPos;
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
            Ipp32u CtxSet = (SubSet > 0 && IsLuma) ? 2 : 0;
            Ipp32u Bin;

            if (c1 == 0)
                CtxSet++;

            c1 = 1;

            Ipp32u baseCtxIdx = ctxIdxOffsetHEVC[ONE_FLAG_HEVC] + (CtxSet<<2) + (IsLuma ? 0 : NUM_CONTEXT_ONE_FLAG_LUMA);

            Ipp32s absCoeff[SCAN_SET_SIZE];
            for (Ipp32u i = 0; i < numNonZero; i++)
                absCoeff[i] = 1;

            Ipp32s numC1Flag = IPP_MIN(numNonZero, LARGER_THAN_ONE_FLAG_NUMBER);
            Ipp32s firstC2FlagIdx = -1;

            for (Ipp32s idx = 0; idx < numC1Flag; idx++)
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

            Ipp32u coeffSigns;
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

            Ipp32s FirstCoeff2 = 1;
            if (c1 == 0 || numNonZero > LARGER_THAN_ONE_FLAG_NUMBER)
            {
                for (Ipp32u idx = 0; idx < numNonZero; idx++)
                {
                    Ipp32s baseLevel  = (idx < LARGER_THAN_ONE_FLAG_NUMBER)? (2 + FirstCoeff2) : 1;

                    if (absCoeff[ idx ] == baseLevel)
                    {
                        Ipp32u Level;
                        ReadCoefRemainExGolombCABAC(Level, GoRiceParam);
                        absCoeff[idx] = Level + baseLevel;
                        if (absCoeff[idx] > 3 * (1 << GoRiceParam))
                            GoRiceParam = IPP_MIN(GoRiceParam + 1, 4);
                    }

                    if (absCoeff[idx] >= 2)
                    {
                        FirstCoeff2 = 0;
                    }
                }
            }

            for (Ipp32u idx = 0; idx < numNonZero; idx++)
            {
                Ipp32s blkPos = pos[idx];
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
                    Ipp32s sign = static_cast<Ipp32s>(coeffSigns) >> 31;
                    coef = (Coeffs)((coef ^ sign) - sign);
                    coeffSigns <<= 1;
                }

                if (isBypass)
                {
                    pCoef[blkPos] = (Coeffs)coef;
                }
                else
                {
                    Ipp32s coeffQ;
                    if (scaling_list_enabled_flag)
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
#pragma warning(default: 4127)

// Parse TU transform skip flag
void H265SegmentDecoder::ParseTransformSkipFlags(Ipp32u AbsPartIdx, ComponentPlane plane)
{
    Ipp32u CtxIdx = ctxIdxOffsetHEVC[TRANSFORM_SKIP_HEVC] + (plane ? NUM_CONTEXT_TRANSFORMSKIP_FLAG : 0);
    Ipp32u useTransformSkip = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    m_cu->setTransformSkip(useTransformSkip, plane, AbsPartIdx);
}

// Decode X and Y coordinates of last significant coefficient in a TU block
Ipp32u H265SegmentDecoder::ParseLastSignificantXYCABAC(Ipp32u &PosLastX, Ipp32u &PosLastY, Ipp32u L2Width, bool IsLuma, Ipp32u ScanIdx)
{
    Ipp32u mGIdx = g_GroupIdx[(1<<(L2Width+2)) - 1];

    Ipp32u blkSizeOffset = IsLuma ? (L2Width*3 + ((L2Width+1)>>2)) : NUM_CONTEX_LAST_COEF_FLAG_XY;
    Ipp32u shift = IsLuma ? ((L2Width+3) >> 2) : L2Width;
    Ipp32u CtxIdxX = ctxIdxOffsetHEVC[LAST_X_HEVC] + blkSizeOffset;
    Ipp32u CtxIdxY = ctxIdxOffsetHEVC[LAST_Y_HEVC] + blkSizeOffset;

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
        Ipp32u Temp  = 0;
        Ipp32s Count = (PosLastX - 2) >> 1;
        for (Ipp32s i = 0; i < Count; i++)
        {
            Temp |= m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp <<= 1;
        }
        PosLastX = g_MinInGroup[PosLastX] + (Temp >> 1);
    }

    if (PosLastY > 3)
    {
        Ipp32u Temp  = 0;
        Ipp32s Count = (PosLastY - 2) >> 1;
        for (Ipp32s i = 0; i < Count; i++)
        {
            Temp |= m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp <<= 1;
        }
        PosLastY = g_MinInGroup[PosLastY] + (Temp >> 1);
    }

    if (ScanIdx == SCAN_VER) {
        Ipp32u tPos = PosLastX; PosLastX = PosLastY; PosLastY = tPos;
    }

    return PosLastX + (PosLastY << (L2Width+2));
}

// Decode coeff_abs_level_remaining value. HEVC spec 9.3.3.9
void H265SegmentDecoder::ReadCoefRemainExGolombCABAC(Ipp32u &Symbol, Ipp32u &Param)
{
    Ipp32u prefix = 0;
    Ipp32u CodeWord = 0;

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
void H265SegmentDecoder::ReconstructCU(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    bool BoundaryFlag = false;
    Ipp32s Size = m_pSeqParamSet->MaxCUSize >> Depth;
    Ipp32u XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32u LPelX = m_cu->m_CUPelX + XInc;
    Ipp32u YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    Ipp32u TPelY = m_cu->m_CUPelY + YInc;

    if (LPelX + Size > m_pSeqParamSet->pic_width_in_luma_samples || TPelY + Size > m_pSeqParamSet->pic_height_in_luma_samples)
    {
        BoundaryFlag = true;
    }

    if (((Depth < m_cu->GetDepth(AbsPartIdx)) && (Depth < m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)) || BoundaryFlag)
    {
        Ipp32u NextDepth = Depth + 1;
        Ipp32u QNumParts = m_cu->m_NumPartition >> (NextDepth << 1);
        Ipp32u Idx = AbsPartIdx;
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, Idx += QNumParts)
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
void H265SegmentDecoder::ReconInter(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // inter prediction
    m_Prediction->MotionCompensation(m_cu, AbsPartIdx, Depth);

    // clip for only non-zero cbp case
    if (m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx) ||
        (m_pSeqParamSet->ChromaArrayType == CHROMA_FORMAT_422 && (m_cu->GetCbf(COMPONENT_CHROMA_U1, AbsPartIdx) || m_cu->GetCbf(COMPONENT_CHROMA_V1, AbsPartIdx))))
    {
        // inter recon
        Ipp32u Size = m_cu->GetWidth(AbsPartIdx);
        m_TrQuant->InvRecurTransformNxN(m_cu, AbsPartIdx, Size, 0);
    }
}

// Place IPCM decoded samples to reconstruct frame
void H265SegmentDecoder::ReconPCM(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u size = m_pSeqParamSet->MaxCUSize >> Depth; 
    m_reconstructor->ReconstructPCMBlock(m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx), m_pCurrentFrame->pitch_luma(),
        m_pSeqParamSet->bit_depth_luma - m_pSeqParamSet->pcm_sample_bit_depth_luma, // luma bit shift
        m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx), m_pCurrentFrame->pitch_chroma(),
        m_pSeqParamSet->bit_depth_chroma - m_pSeqParamSet->pcm_sample_bit_depth_chroma, // chroma bit shift
        &m_context->m_coeffsRead, size);
}

// Fill up inter information in local decoding context and colocated lookup table
void H265SegmentDecoder::UpdatePUInfo(Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, H265MVInfo &MVi)
{
    VM_ASSERT(MVi.m_refIdx[REF_PIC_LIST_0] >= 0 || MVi.m_refIdx[REF_PIC_LIST_1] >= 0);

    for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
    {
        if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0 && MVi.m_refIdx[RefListIdx] >= 0)
        {
            H265DecoderRefPicList::ReferenceInformation &refInfo = m_pRefPicList[RefListIdx][MVi.m_refIdx[RefListIdx]];
            MVi.m_index[RefListIdx] = (Ipp8s)refInfo.refFrame->GetFrameMID();

            if (MVi.m_mv[RefListIdx].Vertical > m_context->m_mvsDistortionTemp)
                m_context->m_mvsDistortionTemp = MVi.m_mv[RefListIdx].Vertical;
        }
    }

    Ipp32u mask = (1 << m_pSeqParamSet->MaxCUDepth) - 1;
    PartX &= mask;
    PartY &= mask;

    Ipp32s stride = m_context->m_CurrCTBStride;

    // Colocated table
    Ipp32s CUX = (m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + PartX;
    Ipp32s CUY = (m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + PartY;
    H265MVInfo *colInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

    for (Ipp32u y = 0; y < PartHeight; y++)
    {
        for (Ipp32u x = 0; x < PartWidth; x++)
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
    for (Ipp32u i = 0; i < PartWidth; i++)
    {
        pInfo[i] = info;
    }

    // Right column
    pInfo = &m_context->m_CurrCTBFlags[PartY * stride + PartX];
    for (Ipp32u i = 0; i < PartHeight; i++)
    {
        pInfo[PartWidth - 1] = info;
        pInfo += stride;
    }
}

// Fill up basic CU information in local decoder context
void H265SegmentDecoder::UpdateNeighborBuffers(Ipp32u AbsPartIdx, Ipp32u Depth, bool isSkipped)
{
    Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartSize = m_pSeqParamSet->NumPartitionsInCUSize >> Depth;
    Ipp32s stride = m_context->m_CurrCTBStride;

    H265FrameHLDNeighborsInfo *pInfo;
    H265FrameHLDNeighborsInfo info;
    info.members.IsAvailable = 1;
    info.members.IsIntra = m_cu->GetPredictionMode(AbsPartIdx);
    info.members.Depth = m_cu->GetDepth(AbsPartIdx);
    info.members.SkipFlag = isSkipped;
    info.members.IntraDir = m_cu->GetLumaIntra(AbsPartIdx);
    info.members.qp = (Ipp8s)m_context->GetQP();

    // Bottom row
    pInfo = &m_context->m_CurrCTBFlags[(YInc + PartSize - 1) * stride + XInc];
    for (Ipp32s i = 0; i < PartSize; i++)
    {
        pInfo[i] = info;
    }

    // Right column
    pInfo = &m_context->m_CurrCTBFlags[YInc * stride + XInc];
    for (Ipp32s i = 0; i < PartSize; i++)
    {
        pInfo[PartSize - 1] = info;
        pInfo += stride;
    }

    if (info.members.IsIntra)
    {
        Ipp32s CUX = (m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + XInc;
        Ipp32s CUY = (m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + YInc;
        H265MVInfo *colInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

        for (Ipp32s i = 0; i < PartSize; i++)
        {
            for (Ipp32s j = 0; j < PartSize; j++)
            {
                colInfo[j].m_refIdx[REF_PIC_LIST_0] = -1;
                colInfo[j].m_refIdx[REF_PIC_LIST_1] = -1;
            }
            colInfo += m_pSeqParamSet->NumPartitionsInFrameWidth;
        }
    }
}

// Change QP recorded in CU block of local context to a new value
void H265SegmentDecoder::UpdateNeighborDecodedQP(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartSize = m_pSeqParamSet->NumPartitionsInCUSize >> Depth;
    Ipp32s qp = m_context->GetQP();

    m_context->SetNewQP(qp);

    for (Ipp32s y = YInc; y < YInc + PartSize; y++)
        for (Ipp32s x = XInc; x < XInc + PartSize; x++)
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * y + x].members.qp = (Ipp8s)qp;
}

// Update intra availability flags for reconstruct
void H265SegmentDecoder::UpdateRecNeighboursBuffersN(Ipp32s PartX, Ipp32s PartY, Ipp32s PartSize, bool IsIntra)
{
    Ipp32u maskL, maskT, maskTL;
    bool   isIntra = (m_pPicParamSet->constrained_intra_pred_flag) ? (IsIntra ? 1 : 0) : 1;

    PartSize >>= 2;

    Ipp32u maxCUSzIn4x4 = m_pSeqParamSet->MaxCUSize >> 2;
    Ipp32s diagId = (PartX+(maxCUSzIn4x4-PartY)-PartSize);

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
Ipp32u H265SegmentDecoder::getCtxSplitFlag(Ipp32s PartX, Ipp32s PartY, Ipp32u Depth)
{
    Ipp32u uiCtx;

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
Ipp32u H265SegmentDecoder::getCtxSkipFlag(Ipp32s PartX, Ipp32s PartY)
{
    Ipp32u uiCtx = 0;

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
void H265SegmentDecoder::getIntraDirLumaPredictor(Ipp32u AbsPartIdx, Ipp32s IntraDirPred[])
{
    Ipp32s LeftIntraDir = INTRA_LUMA_DC_IDX;
    Ipp32s AboveIntraDir = INTRA_LUMA_DC_IDX;
    Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;

    Ipp32s PartOffsetY = (m_pSeqParamSet->NumPartitionsInCU >> (m_cu->GetDepth(AbsPartIdx) << 1)) >> 1;
    Ipp32s PartOffsetX = PartOffsetY >> 1;

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
Ipp32s H265SegmentDecoder::getRefQP(Ipp32s AbsPartIdx)
{
    Ipp32s shift = (m_pSeqParamSet->MaxCUDepth - m_pPicParamSet->diff_cu_qp_delta_depth) << 1;
    Ipp32u AbsQpMinCUIdx = (AbsPartIdx >> shift) << shift;
    Ipp32s QPXInc = m_cu->m_rasterToPelX[AbsQpMinCUIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s QPYInc = m_cu->m_rasterToPelY[AbsQpMinCUIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp8s qpL, qpA, lastValidQP = 0;

    if (QPXInc == 0 || QPYInc == 0)
    {
        Ipp32s lastValidPartIdx = m_cu->getLastValidPartIdx(AbsQpMinCUIdx);
        if (lastValidPartIdx >= 0)
        {
            Ipp32s x = m_cu->m_rasterToPelX[lastValidPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
            Ipp32s y = m_cu->m_rasterToPelY[lastValidPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
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
static bool inline isDiffMER(Ipp32u plevel, Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP)
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
bool H265SegmentDecoder::hasEqualMotion(Ipp32s dir1, Ipp32s dir2)
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
void H265SegmentDecoder::getInterMergeCandidates(Ipp32u AbsPartIdx, Ipp32u PartIdx, H265MVInfo *MVBufferNeighbours,
                                                 Ipp8u *InterDirNeighbours, Ipp32s mrgCandIdx)
{
    static const Ipp32u PriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
    static const Ipp32u PriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};

    bool CandIsInter[MERGE_MAX_NUM_CAND];
    for (Ipp32s ind = 0; ind < m_pSliceHeader->max_num_merge_cand; ++ind)
    {
        CandIsInter[ind] = false;
        MVBufferNeighbours[ind].m_refIdx[REF_PIC_LIST_0] = NOT_VALID;
        MVBufferNeighbours[ind].m_refIdx[REF_PIC_LIST_1] = NOT_VALID;
    }

    // compute the location of the current PU
    Ipp32u XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32u YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    Ipp32s LPelX = m_cu->m_CUPelX + XInc;
    Ipp32s TPelY = m_cu->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;

    Ipp32s TUPartNumberInGMV = m_pSeqParamSet->NumPartitionsInFrameWidth * PartY + PartX;

    Ipp32s nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32s PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;
    EnumPartSize CurPS = m_cu->GetPartitionSize(AbsPartIdx);

    Ipp32s Count = 0;

    // left
    Ipp32s leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    Ipp32s leftAddrGMV = TUPartNumberInGMV + m_pSeqParamSet->NumPartitionsInFrameWidth * (PartHeight - 1) - 1;

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
    Ipp32s aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    Ipp32s aboveAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + (PartWidth - 1);
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
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    Ipp32s aboveRightAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + PartWidth;
    bool isAvailableB0 = m_context->m_CurrCTBFlags[aboveRightAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveRightAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW, TPelY - 1, LPelX, TPelY);

    if (isAvailableB0 && (!isAvailableB1 || !hasEqualMotion(aboveAddrGMV, aboveRightAddrGMV)))
    {
        UPDATE_MV_INFO(aboveRightAddrGMV);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // left bottom
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;
    Ipp32s leftBottomAddrGMV = TUPartNumberInGMV - 1 + m_pSeqParamSet->NumPartitionsInFrameWidth * PartHeight;
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
        Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;
        Ipp32s aboveLeftAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth - 1;
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
        Ipp32u bottomRightPartX = PartX + PartWidth;
        Ipp32u bottomRightPartY = PartY + PartHeight;
        Ipp32u centerPartX = PartX + (PartWidth >> 1);
        Ipp32u centerPartY = PartY + (PartHeight >> 1);
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
        Ipp8u dir = 0;
        Ipp32s ArrayAddr = Count;

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

    Ipp32s ArrayAddr = Count;
    Ipp32s Cutoff = ArrayAddr;

    // Combine candidates. HEVC spec 8.5.3.2.4
    if (m_pSliceHeader->slice_type == B_SLICE)
    {
        for (Ipp32s idx = 0; idx < Cutoff * (Cutoff - 1) && ArrayAddr != m_pSliceHeader->max_num_merge_cand; idx++)
        {
            Ipp32s i = PriorityList0[idx];
            Ipp32s j = PriorityList1[idx];
            if (CandIsInter[i] && CandIsInter[j] && (InterDirNeighbours[i] & 0x1) && (InterDirNeighbours[j] & 0x2))
            {
                Ipp32s RefPOCL0 = m_pRefPicList[REF_PIC_LIST_0][MVBufferNeighbours[i].m_refIdx[REF_PIC_LIST_0]].refFrame->GetFrameMID();
                Ipp32s RefPOCL1 = m_pRefPicList[REF_PIC_LIST_1][MVBufferNeighbours[j].m_refIdx[REF_PIC_LIST_1]].refFrame->GetFrameMID();

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

    Ipp32s numRefIdx = (m_pSliceHeader->slice_type == B_SLICE) ? IPP_MIN(m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0], m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_1]) : m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0];
    Ipp8s r = 0;
    Ipp32s refcnt = 0;

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
void H265SegmentDecoder::fillMVPCand(Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo)
{
    H265MotionVector MvPred;
    bool Added = false;

    pInfo->NumbOfCands = 0;
    if (RefIdx < 0)
    {
        return;
    }

    // compute the location of the current PU
    Ipp32s XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32s YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    Ipp32s LPelX = m_cu->m_CUPelX + XInc;
    Ipp32s TPelY = m_cu->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;
    Ipp32s TUPartNumberInGMV = m_pSeqParamSet->NumPartitionsInFrameWidth * PartY + PartX;

    Ipp32s nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32u PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32u PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;

    // Left predictor search
    Ipp32s leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;

    Ipp32u leftAddrMV = TUPartNumberInGMV + m_pSeqParamSet->NumPartitionsInFrameWidth * (PartHeight - 1) - 1;
    Ipp32s leftBottomAddrMV = TUPartNumberInGMV - 1 + m_pSeqParamSet->NumPartitionsInFrameWidth * PartHeight;

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
    Ipp32s aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;

    Ipp32s aboveAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + (PartWidth - 1);
    Ipp32s aboveRightAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth + PartWidth;
    Ipp32s aboveLeftAddrGMV = TUPartNumberInGMV - m_pSeqParamSet->NumPartitionsInFrameWidth - 1;

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
        Ipp32u bottomRightPartX = PartX + PartWidth;
        Ipp32u bottomRightPartY = PartY + PartHeight;
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
            Ipp32u centerPartX = PartX + (PartWidth >> 1);
            Ipp32u centerPartY = PartY + (PartHeight >> 1);

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
bool H265SegmentDecoder::AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr, const H265MVInfo &motionInfo)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    Ipp32s candRefIdx = motionInfo.m_refIdx[RefPicList];
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
static Ipp32s GetDistScaleFactor(Ipp32s DiffPocB, Ipp32s DiffPocD)
{
    if (DiffPocD == DiffPocB)
    {
        return 4096;
    }
    else
    {
        Ipp32s TDB = Clip3(-128, 127, DiffPocB);
        Ipp32s TDD = Clip3(-128, 127, DiffPocD);
        Ipp32s X = (0x4000 + abs(TDD / 2)) / TDD;
        Ipp32s Scale = Clip3(-4096, 4095, (TDB * X + 32) >> 6);
        return Scale;
    }
}

// Check availability of spatial motion vector candidate for any reference frame. HEVC spec 8.5.3.2.7 #7
bool H265SegmentDecoder::AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr, const H265MVInfo &motionInfo)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    Ipp32s CurrRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt;
    bool IsCurrRefLongTerm = m_pRefPicList[RefPicList][RefIdx].isLongReference;

    for (Ipp32s i = 0; i < 2; i++)
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
                    Ipp32s NeibRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][NeibRefIdx].refFrame->m_PicOrderCnt;
                    Ipp32s Scale = GetDistScaleFactor(CurrRefTb, NeibRefTb);

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
bool H265SegmentDecoder::GetColMVP(EnumRefPicList refPicListIdx, Ipp32u PartX, Ipp32u PartY, H265MotionVector &rcMv, Ipp32s refIdx)
{
    bool isCurrRefLongTerm, isColRefLongTerm;

    Ipp32u Log2MinTUSize = m_pSeqParamSet->log2_min_transform_block_size;

    /* MV compression */
    if (Log2MinTUSize < 4)
    {
        Ipp32u shift = (4 - Log2MinTUSize);
        PartX = (PartX >> shift) << shift;
        PartY = (PartY >> shift) << shift;
    }

    EnumRefPicList colPicListIdx = EnumRefPicList((B_SLICE == m_pSliceHeader->slice_type) ? 1 - m_pSliceHeader->collocated_from_l0_flag : 0);
    H265DecoderFrame *colPic = m_pRefPicList[colPicListIdx][m_pSliceHeader->collocated_ref_idx].refFrame;
    Ipp32u colocatedPartNumber = PartY * m_pSeqParamSet->NumPartitionsInFrameWidth + PartX;

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
        Ipp32s currRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[refPicListIdx][refIdx].refFrame->m_PicOrderCnt;
        Ipp32s colRefTb = colPic->m_CodingData->GetTUPOCDelta(colPicListIdx, colocatedPartNumber);
        Ipp32s scale = GetDistScaleFactor(currRefTb, colRefTb);

        if (scale == 4096)
            rcMv = cMvPred;
        else
            rcMv = cMvPred.scaleMV(scale);
    }

    return true;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
