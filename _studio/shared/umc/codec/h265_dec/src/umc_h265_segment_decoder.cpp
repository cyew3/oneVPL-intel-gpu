/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder.h"
#include "umc_h265_segment_decoder_templates.h"
#include "umc_h265_bitstream.h"
#include "h265_tr_quant.h"
#include "umc_h265_frame_info.h"
#include "mfx_h265_optimization.h"

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
    m_CurrCTB = 0;
    m_CurrCTBStride = 0;
    m_LastValidQP = 0;

    m_needToSplitDecAndRec = true;
}

void DecodingContext::Reset()
{
    if (m_needToSplitDecAndRec)
        ResetRowBuffer();
}

void DecodingContext::Init(H265Slice *slice)
{
    m_sps = slice->GetSeqParam();
    m_pps = slice->GetPicParam();
    m_sh = &slice->m_SliceHeader;
    m_frame = slice->GetCurrentFrame();

    if (m_TopNgbrsHolder.size() < m_sps->NumPartitionsInFrameWidth + 2 || m_TopMVInfoHolder.size() < m_sps->NumPartitionsInFrameWidth + 2)
    {
        // Zero element is left-top diagonal from zero CTB
        m_TopNgbrsHolder.resize(m_sps->NumPartitionsInFrameWidth + 2);
        m_TopMVInfoHolder.resize(m_sps->NumPartitionsInFrameWidth + 2);
    }

    m_TopNgbrs = &m_TopNgbrsHolder[1];

    VM_ASSERT(m_sps->pic_width_in_luma_samples <= 11776); // Top Intra flags placeholder can store up to (11776 / 4 / 32) + 1 flags
    m_RecTpIntraFlags    = m_RecIntraFlagsHolder;
    m_RecLfIntraFlags    = m_RecIntraFlagsHolder + 17;
    m_RecTLIntraFlags    = m_RecIntraFlagsHolder + 34;
    m_RecTpIntraRowFlags = (Ipp16u*)(m_RecIntraFlagsHolder + 35) + 1;

    m_CurrCTBStride = (m_sps->NumPartitionsInCUSize + 2);
    if (m_CurrCTBHolder.size() < (Ipp32u)(m_CurrCTBStride * m_CurrCTBStride))
    {
        m_CurrCTBHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
        m_CurrCTBFlagsHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
    }

    m_CurrCTB = &m_CurrCTBHolder[m_CurrCTBStride + 1];
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

    m_mvsDistortion = 0;
}

void DecodingContext::UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr)
{
    // Set local pointers to real array start positions
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];
    H265MVInfo *CurrCTB = &m_CurrCTBHolder[0];
    H265MVInfo *TopMVInfo = &m_TopMVInfoHolder[0];

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
                CurrCTB[i] = TopMVInfo[newCUX + i];
            }
            // Store bottom margin for next row if next CTB is to the right. This is necessary for left-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
                TopMVInfo[lastCUX + i] = CurrCTB[bottom_row_offset + i];
            }
        }
        else if (newCUX < lastCUX)
        {
            // Store bottom margin for next row if next CTB is to the left-below. This is necessary for right-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[bottom_row_offset + i];
                TopMVInfo[lastCUX + i] = CurrCTB[bottom_row_offset + i];
            }
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i] = TopNgbrs[newCUX + i];
                CurrCTB[i] = TopMVInfo[newCUX + i];
            }
        }
        else // New CTB right under previous CTB
        {
            // Copy data from bottom row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                TopNgbrs[lastCUX + i] = CurrCTBFlags[i] = CurrCTBFlags[bottom_row_offset + i];
                CurrCTB[i] = CurrCTB[bottom_row_offset + i];
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
            TopMVInfo[lastCUX + i] = CurrCTB[bottom_row_offset + i];
        }
    }

    if (lastCUY == newCUY)
    {
        // Copy left margin from right
        for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
        {
            CurrCTBFlags[i * m_CurrCTBStride] = CurrCTBFlags[i * m_CurrCTBStride + m_CurrCTBStride - 2];
            CurrCTB[i * m_CurrCTBStride] = CurrCTB[i * m_CurrCTBStride + m_CurrCTBStride - 2];
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
        for (Ipp32s j = 1; j < m_CurrCTBStride; j++)
            CurrCTBFlags[i * m_CurrCTBStride + j].data = 0;
}

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

void DecodingContext::ResetRowBuffer()
{
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    for (Ipp32u i = 0; i < m_sps->NumPartitionsInFrameWidth + 2; i++)
        TopNgbrs[i].data = 0;

    for (Ipp32s i = 0; i < m_CurrCTBStride * m_CurrCTBStride; i++)
        CurrCTBFlags[i].data = 0;
}

void DecodingContext::ResetRecRowBuffer()
{
    memset(m_RecIntraFlagsHolder, 0, sizeof(m_RecIntraFlagsHolder));
}

void DecodingContext::SetNewQP(Ipp32s newQP)
{
    if (newQP == m_LastValidQP)
        return;

    m_LastValidQP = newQP;

    Ipp32s qpScaledY = m_LastValidQP + m_sps->m_QPBDOffsetY;
    Ipp32s qpOffsetCb = m_pps->pps_cb_qp_offset + m_sh->slice_cb_qp_offset;
    Ipp32s qpOffsetCr = m_pps->pps_cr_qp_offset + m_sh->slice_cr_qp_offset;
    Ipp32s qpBdOffsetC = m_sps->m_QPBDOffsetC;
    Ipp32s qpScaledCb = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCb);
    Ipp32s qpScaledCr = Clip3(-qpBdOffsetC, 57, m_LastValidQP + qpOffsetCr);

    if (qpScaledCb < 0)
        qpScaledCb = qpScaledCb + qpBdOffsetC;
    else
        qpScaledCb = g_ChromaScale[qpScaledCb] + qpBdOffsetC;

    if (qpScaledCr < 0)
        qpScaledCr = qpScaledCr + qpBdOffsetC;
    else
        qpScaledCr = g_ChromaScale[qpScaledCr] + qpBdOffsetC;

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

void H265SegmentDecoder::SaveCTBContext()
{
    Ipp32s CUX = m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s CUY = m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    H265MVInfo *colInfo = m_pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

    for (Ipp32u y = 0; y < m_pSeqParamSet->NumPartitionsInCUSize; y++)
        MFX_INTERNAL_CPY(colInfo + y * m_pSeqParamSet->NumPartitionsInFrameWidth,
        m_context->m_CurrCTB + y * m_context->m_CurrCTBStride, sizeof(H265MVInfo) * m_pSeqParamSet->NumPartitionsInCUSize);
}

H265SegmentDecoder::H265SegmentDecoder(TaskBroker_H265 * pTaskBroker)
{
    m_pTaskBroker = pTaskBroker;

    m_pSlice = 0;

    m_cu = 0;
    // prediction buffer
    m_Prediction.reset(new H265Prediction()); //PREDICTION

    m_ppcYUVResi = 0;
    m_TrQuant = 0;

    m_MaxDepth = 0;

    m_context = 0;
    m_context_single_thread.reset(new DecodingContext());
} // H265SegmentDecoder::H265SegmentDecoder(H264SliceStore &Store)

H265SegmentDecoder::~H265SegmentDecoder(void)
{
    Release();
} // H265SegmentDecoder::~H265SegmentDecoder(void)

/**
 \param    uiMaxDepth    total number of allowable depth
 \param    uiMaxWidth    largest CU width
 \param    uiMaxHeight   largest CU height
 */

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

        m_ppcYUVResi->create(sps->MaxCUSize, sps->MaxCUSize, sizeof(Ipp16s), sizeof(Ipp16s));
    }

    if (!m_TrQuant)
        m_TrQuant = new H265TrQuant(); //TRQUANT
}

void H265SegmentDecoder::destroy()
{
}

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

UMC::Status H265SegmentDecoder::Init(Ipp32s iNumber)
{
    // release object before initialization
    Release();

    // save ordinal number
    m_iNumber = iNumber;

    return UMC::UMC_OK;

} // Status H265SegmentDecoder::Init(Ipp32s sNumber)

SegmentDecoderHPBase_H265 *CreateSD_H265(Ipp32s bit_depth_luma, Ipp32s bit_depth_chroma)
{
    if (bit_depth_chroma > 8 || bit_depth_luma > 8)
    {
        return CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u>::CreateSoftSegmentDecoder();    }
    else
    {
        return CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u>::CreateSoftSegmentDecoder();
    }

} // SegmentDecoderHPBase_H265 *CreateSD(Ipp32s bit_depth_luma,

static
void InitializeSDCreator()
{
    CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u>::CreateSoftSegmentDecoder();
}

class SDInitializer
{
public:
    SDInitializer()
    {
        InitializeSDCreator();
    }
};

static SDInitializer tableInitializer;

void H265SegmentDecoder::parseSaoMaxUvlc(Ipp32u& val, Ipp32u maxSymbol)
{
    if (maxSymbol == 0)
    {
        val = 0;
        return;
    }

    Ipp32u code;
    Ipp32u  i;

    code = m_pBitStream->DecodeSingleBinEP_CABAC();

    if (code == 0)
    {
        val = 0;
        return;
    }

    i = 1;
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

    val = i;
}

void H265SegmentDecoder::parseSaoUflc(Ipp32s length, Ipp32u&  riVal)
{
    riVal = m_pBitStream->DecodeBypassBins_CABAC(length);
}

void H265SegmentDecoder::parseSaoMerge(Ipp32u&  ruiVal)
{
    ruiVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]);
}

void H265SegmentDecoder::parseSaoTypeIdx(Ipp32u&  ruiVal)
{
    Ipp32u uiCode;
    uiCode = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_TYPE_IDX_HEVC]);

    if (uiCode == 0)
    {
        ruiVal = 0;
    }
    else
    {
        uiCode = m_pBitStream->DecodeSingleBinEP_CABAC();

        if (uiCode == 0)
        {
            ruiVal = 5;
        }
        else
        {
            ruiVal = 1;
        }
    }
}

void H265SegmentDecoder::parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx)
{
    Ipp32u uiSymbol;
    static Ipp32s iTypeLength[MAX_NUM_SAO_TYPE] =
    {
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_BO_LEN
    };

    if (compIdx == 2)
    {
        uiSymbol = (Ipp32u)(psSaoLcuParam->m_typeIdx + 1);
    }
    else
    {
        parseSaoTypeIdx(uiSymbol);
    }

    psSaoLcuParam->m_typeIdx = (Ipp32s)uiSymbol - 1;

    if (uiSymbol)
    {
        psSaoLcuParam->m_length = iTypeLength[psSaoLcuParam->m_typeIdx];

        Ipp32s bitDepth = compIdx ? m_pSeqParamSet->bit_depth_chroma : m_pSeqParamSet->bit_depth_luma;
        Ipp32s offsetTh = 1 << IPP_MIN(bitDepth - 5, 5);

        if (psSaoLcuParam->m_typeIdx == SAO_BO)
        {
            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                parseSaoMaxUvlc(uiSymbol, offsetTh - 1);
                psSaoLcuParam->m_offset[i] = uiSymbol;
            }

            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                if (psSaoLcuParam->m_offset[i] != 0)
                {
                    uiSymbol = m_pBitStream->DecodeSingleBinEP_CABAC();

                    if (uiSymbol)
                    {
                        psSaoLcuParam->m_offset[i] = -psSaoLcuParam->m_offset[i];
                    }
                }
            }
            parseSaoUflc(5, uiSymbol);
            psSaoLcuParam->m_subTypeIdx = uiSymbol;
        }
        else if (psSaoLcuParam->m_typeIdx < 4)
        {
            parseSaoMaxUvlc(uiSymbol, offsetTh - 1);
            psSaoLcuParam->m_offset[0] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh - 1);
            psSaoLcuParam->m_offset[1] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh - 1);
            psSaoLcuParam->m_offset[2] = -(Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh - 1);
            psSaoLcuParam->m_offset[3] = -(Ipp32s)uiSymbol;
            if (compIdx != 2)
            {
                parseSaoUflc(2, uiSymbol);
                psSaoLcuParam->m_subTypeIdx = (Ipp32s)uiSymbol;
                psSaoLcuParam->m_typeIdx += psSaoLcuParam->m_subTypeIdx;
            }
        }
    }
    else
    {
        psSaoLcuParam->m_length = 0;
    }
}

inline void copySaoOneLcuParam(SAOLCUParam* dst,  SAOLCUParam* src)
{
    Ipp32s i;
    dst->m_typeIdx = src->m_typeIdx;

    if (dst->m_typeIdx != -1)
    {
        dst->m_subTypeIdx = src->m_subTypeIdx;
        dst->m_length = src->m_length;
        for (i = 0; i < dst->m_length; i++)
        {
            dst->m_offset[i] = src->m_offset[i];
        }
    }
    else
    {
        dst->m_length = 0;
        for (i = 0; i < SAO_BO_LEN; i++)
        {
            dst->m_offset[i] = 0;
        }
    }
}

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
        Ipp32s allowMergeLeft = 0;
        Ipp32s allowMergeUp   = 0;

        if (rx > 0)
        {
            if (cuAddrLeftInSlice >= sliceStartRS &&
                m_pCurrentFrame->m_CodingData->getTileIdxMap(cuAddrLeftInSlice) == m_pCurrentFrame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeLeft = 1;
            }
        }

        if (ry > 0)
        {
            if (cuAddrUpInSlice >= sliceStartRS &&
                m_pCurrentFrame->m_CodingData->getTileIdxMap(cuAddrUpInSlice) == m_pCurrentFrame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeUp = 1;
            }
        }

        parseSaoOneLcuInterleaving(m_pSliceHeader->slice_sao_luma_flag != 0, m_pSliceHeader->slice_sao_chroma_flag != 0, allowMergeLeft, allowMergeUp);
    }
}

void H265SegmentDecoder::parseSaoOneLcuInterleaving(bool saoLuma,
                                                    bool saoChroma,
                                                    Ipp32s allowMergeLeft,
                                                    Ipp32s allowMergeUp)
{
    Ipp32s iAddr = m_cu->CUAddr;
    Ipp32u uiSymbol;
    SAOLCUParam* saoLcuParam[3];

    saoLcuParam[0] = m_pCurrentFrame->m_saoLcuParam[0];
    saoLcuParam[1] = m_pCurrentFrame->m_saoLcuParam[1];
    saoLcuParam[2] = m_pCurrentFrame->m_saoLcuParam[2];

    for (Ipp32s iCompIdx = 0; iCompIdx < 3; iCompIdx++)
    {
        saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag    = 0;
        saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag  = 0;
        saoLcuParam[iCompIdx][iAddr].m_subTypeIdx     = 0;
        saoLcuParam[iCompIdx][iAddr].m_typeIdx        = -1;
        saoLcuParam[iCompIdx][iAddr].m_offset[0]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[1]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[2]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[3]     = 0;

    }

    if (allowMergeLeft)
    {
        parseSaoMerge(uiSymbol);
        saoLcuParam[0][iAddr].m_mergeLeftFlag = uiSymbol != 0;
    }
    if (allowMergeUp && saoLcuParam[0][iAddr].m_mergeLeftFlag == 0)
    {
        parseSaoMerge(uiSymbol);
        saoLcuParam[0][iAddr].m_mergeUpFlag = uiSymbol != 0;
    }

    for (Ipp32s iCompIdx = 0; iCompIdx < 3; iCompIdx++)
    {
        if ((iCompIdx == 0 && saoLuma) || (iCompIdx > 0 && saoChroma))
        {
            if (allowMergeLeft)
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = saoLcuParam[0][iAddr].m_mergeLeftFlag;
            }
            else
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = 0;
            }

            if (saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag == 0)
            {
                if (allowMergeUp)
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = saoLcuParam[0][iAddr].m_mergeUpFlag;
                }
                else
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = 0;
                }

                if (saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag == 0)
                {
                    saoLcuParam[2][iAddr].m_typeIdx = saoLcuParam[1][iAddr].m_typeIdx;
                    parseSaoOffset(&(saoLcuParam[iCompIdx][iAddr]), iCompIdx);
                }
                else
                {
                    copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr], &saoLcuParam[iCompIdx][iAddr - m_pCurrentFrame->m_CodingData->m_WidthInCU]);
                }
            }
            else
            {
                copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr], &saoLcuParam[iCompIdx][iAddr - 1]);
            }
        }
        else
        {
            saoLcuParam[iCompIdx][iAddr].m_typeIdx = -1;
            saoLcuParam[iCompIdx][iAddr].m_subTypeIdx = 0;
        }
    }
}

void H265SegmentDecoder::ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol)
{
    if (0 == MaxSymbol)
    {
        uVal = 0;
        return;
    }

    uVal = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);
    //m_pcTDecBinIf->decodeBin( ruiSymbol, pcSCModel[0] );

    if (uVal == 0 || MaxSymbol == 1)
    {
        return;
    }

    Ipp32u val = 0;
    Ipp32u cont;

    do
    {
        cont = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx + Offset);
        //m_pcTDecBinIf->decodeBin( uiCont, pcSCModel[ iOffset ] );
        val++;
    }
    while( cont && ( val < MaxSymbol - 1 ) );

    if ( cont && ( val == MaxSymbol - 1 ) )
    {
        val++;
    }

    uVal = val;
}

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

            if ((LPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (TPelY < m_pSeqParamSet->pic_height_in_luma_samples))
            {
                DecodeCUCABAC(Idx, Depth + 1, IsLast);
            }
            else
            {
                m_cu->setOutsideCUPart(Idx, Depth + 1);
            }

            if (IsLast)
            {
                break;
            }

            Idx += QNumParts;
        }

        if ((m_pSeqParamSet->MaxCUSize >> Depth) == m_minCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            if (m_DecodeDQPFlag)
            {
                m_context->SetNewQP(getRefQP(AbsPartIdx));
            }
        }
        return;
    }

    if ((m_pSeqParamSet->MaxCUSize >> Depth) >= m_minCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
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
        m_cu->setPredMode(MODE_INTER, AbsPartIdx);
        m_cu->setPartSizeSubParts(PART_SIZE_2Nx2N, AbsPartIdx);
        m_cu->setSize(m_context->m_sps->MaxCUSize >> Depth, AbsPartIdx);
        m_cu->setIPCMFlag(false, AbsPartIdx);

        H265MVInfo MvBufferNeighbours[MERGE_MAX_NUM_CAND]; // double length for mv of both lists
        Ipp8u InterDirNeighbours[MERGE_MAX_NUM_CAND];
        Ipp32s numValidMergeCand = 0;

        for (Ipp32s ui = 0; ui < m_cu->m_SliceHeader->max_num_merge_cand; ++ui)
        {
            InterDirNeighbours[ui] = 0;
        }
        Ipp32s MergeIndex = DecodeMergeIndexCABAC();
        getInterMergeCandidates(AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);

        H265MVInfo &MVi = MvBufferNeighbours[MergeIndex];
        for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
        {
            if (0 == m_cu->m_SliceHeader->m_numRefIdx[RefListIdx])
                MVi.m_refIdx[RefListIdx] = -1;
        }

        UpdatePUInfo(LPelX >> m_pSeqParamSet->log2_min_transform_block_size, TPelY >> m_pSeqParamSet->log2_min_transform_block_size, PartSize, PartSize, MVi);

        m_cu->setCbfSubParts(0, 0, 0, AbsPartIdx, Depth);
        if (m_cu->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
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
        // Inter mode is marked later when RefIdx is read from bitstream
        if (PART_SIZE_2Nx2N == m_cu->GetPartitionSize(AbsPartIdx))
        {
            DecodeIPCMInfoCABAC(AbsPartIdx, Depth);

            if (m_cu->GetIPCMFlag(AbsPartIdx))
            {
                if (m_cu->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
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
        DecodeIntraDirChromaCABAC(AbsPartIdx, Depth);
    }
    else
    {
        m_cu->setIPCMFlag(false, AbsPartIdx);
        BeforeCoeffs(AbsPartIdx, Depth);
        isFirstPartMerge = DecodePUWiseCABAC(AbsPartIdx, Depth);
    }

    if (m_cu->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
        m_context->SetNewQP(m_DecodeDQPFlag ? getRefQP(AbsPartIdx) : m_cu->m_CodedQP);

    // Coefficient decoding
    DecodeCoeff(AbsPartIdx, Depth, m_DecodeDQPFlag, isFirstPartMerge);
    FinishDecodeCU(AbsPartIdx, Depth, IsLast);
}

bool H265SegmentDecoder::DecodeSplitFlagCABAC(Ipp32s PartX, Ipp32s PartY, Ipp32u Depth)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SPLIT_CODING_UNIT_FLAG_HEVC] + getCtxSplitFlag(PartX, PartY, Depth));

    return uVal != 0;
}

bool H265SegmentDecoder::DecodeCUTransquantBypassFlag(Ipp32u AbsPartIdx)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANSQUANT_BYPASS_HEVC]);
    m_cu->setCUTransquantBypass(uVal ? true : false, AbsPartIdx);
    return uVal ? true : false;
}

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

/** parse partition size
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Void
 */
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

    Ipp32u MinCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUWidth();
    Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    // LUMA
    H265PlanePtrYCommon PCMSampleY = (H265PlanePtrYCommon)(m_cu->m_TrCoeffY + LumaOffset);
    Ipp32u Width = m_cu->GetWidth(AbsPartIdx);
    Ipp32u SampleBits = m_pSeqParamSet->pcm_sample_bit_depth_luma;

    for (Ipp32u Y = 0; Y < Width; Y++)
    {
        for (Ipp32u X = 0; X < Width; X++)
        {
            Ipp32u Sample = 0;
            Sample = m_pBitStream->GetBits(SampleBits);
            PCMSampleY[X] = (H265PlaneYCommon)Sample;
        }
        PCMSampleY += Width;
    }

    // CHROMA U
    H265PlanePtrUVCommon PCMSampleUV = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCb + ChromaOffset);
    Width >>= 1;
    SampleBits = m_pSeqParamSet->pcm_sample_bit_depth_chroma;

    for (Ipp32u Y = 0; Y < Width; Y++)
    {
        for (Ipp32u X = 0; X < Width; X++)
        {
            Ipp32u Sample = 0;
            Sample = m_pBitStream->GetBits(SampleBits);
            PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
        }
        PCMSampleUV += Width;
    }

    // CHROMA V
    PCMSampleUV = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCr + ChromaOffset);

    for (Ipp32u Y = 0; Y < Width; Y++)
    {
        for (Ipp32u X = 0; X < Width; X++)
        {
            Ipp32u Sample = 0;
            Sample = m_pBitStream->GetBits(SampleBits);
            PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
        }
        PCMSampleUV += Width;
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

void H265SegmentDecoder::DecodeIntraDirLumaAngCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize mode = m_cu->GetPartitionSize(AbsPartIdx);
    Ipp32u PartNum = mode == PART_SIZE_NxN ? 4 : 1;
    Ipp32u PartOffset = (m_cu->m_Frame->m_CodingData->m_NumPartitions >> (m_cu->GetDepth(AbsPartIdx) << 1)) >> 2;
    Ipp32u mpmPred[4];
    Ipp32u uVal;
    Ipp32s IPredMode;

    if (mode == PART_SIZE_NxN)
        Depth++;

    for (Ipp32u j = 0; j < PartNum; j++)
    {
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_LUMA_PRED_MODE_HEVC]);
        mpmPred[j] = uVal;
    }

    for (Ipp32u j = 0; j < PartNum; j++)
    {
        Ipp32s Preds[3] = {-1, -1, -1};
        Ipp32u PredNum = 3;

        getIntraDirLumaPredictor(AbsPartIdx + PartOffset * j, Preds);

        if (mpmPred[j])
        {
            uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
            if (uVal)
            {
                uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                uVal++;
            }
            IPredMode = Preds[uVal];
        }
        else
        {
            IPredMode = 0;
            uVal = m_pBitStream->DecodeBypassBins_CABAC(5);
            IPredMode = uVal;

            //postponed sorting of MPMs (only in remaining branch)
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

void H265SegmentDecoder::DecodeIntraDirChromaCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_CHROMA_PRED_MODE_HEVC]);

    //switch codeword
    if (uVal == 0)
    {
        uVal = INTRA_DM_CHROMA_IDX;
    }
    else
    {
        Ipp32u IPredMode;

        IPredMode = m_pBitStream->DecodeBypassBins_CABAC(2);
        Ipp32u AllowedChromaDir[INTRA_NUM_CHROMA_MODE];
        m_cu->getAllowedChromaDir(AbsPartIdx, AllowedChromaDir);
        uVal = AllowedChromaDir[IPredMode];
    }
    m_cu->setChromIntraDirSubParts(uVal, AbsPartIdx, Depth);
    return;
}

bool H265SegmentDecoder::DecodePUWiseCABAC(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize PartSize = m_cu->GetPartitionSize(AbsPartIdx);
    Ipp32u NumPU = (PartSize == PART_SIZE_2Nx2N ? 1 : (PartSize == PART_SIZE_NxN ? 4 : 2));
    Ipp32u PUOffset = (g_PUOffset[Ipp32u(PartSize)] << ((m_pSeqParamSet->MaxCUDepth - Depth) << 1)) >> 4;

    H265MVInfo MvBufferNeighbours[MERGE_MAX_NUM_CAND]; // double length for mv of both lists
    Ipp8u InterDirNeighbours[MERGE_MAX_NUM_CAND];

    for (Ipp32s ui = 0; ui < m_pSliceHeader->max_num_merge_cand; ui++ )
        InterDirNeighbours[ui] = 0;

    Ipp32s numValidMergeCand = 0;
    bool isFirstMerge = false;

    for (Ipp32u PartIdx = 0, SubPartIdx = AbsPartIdx; PartIdx < NumPU; PartIdx++, SubPartIdx += PUOffset)
    {
        Ipp8u InterDir = 0;
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
            Ipp32u MergeIndex = DecodeMergeIndexCABAC();

            if ((m_pPicParamSet->log2_parallel_merge_level - 2) > 0 && PartSize != PART_SIZE_2Nx2N && m_cu->GetWidth(AbsPartIdx) <= 8)
            {
                m_cu->setPartSizeSubParts(PART_SIZE_2Nx2N, AbsPartIdx);
                getInterMergeCandidates(AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, -1);
                m_cu->setPartSizeSubParts(PartSize, AbsPartIdx);
            }
            else
            {
                getInterMergeCandidates(SubPartIdx, PartIdx, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);
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

        UpdatePUInfo(PartX, PartY, PartWidth, PartHeight, MVi);
    }

    return isFirstMerge;
}

bool H265SegmentDecoder::DecodeMergeFlagCABAC(void)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_FLAG_HEVC]);
    return uVal ? true : false;
}

// decode inter direction for a PU block
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


// decode coefficients
void H265SegmentDecoder::DecodeCoeff(Ipp32u AbsPartIdx, Ipp32u Depth, bool& CodeDQP, bool isFirstPartMerge)
{
    if (MODE_INTRA != m_cu->GetPredictionMode(AbsPartIdx))
    {
        Ipp32u QtRootCbf = 1;
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

    Ipp32u minCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUWidth();
    Ipp32u lumaOffset = minCoeffSize * AbsPartIdx;
    const Ipp32u Log2TrafoSize = m_pSeqParamSet->log2_max_luma_coding_block_size - Depth;

    m_bakAbsPartIdxCU = AbsPartIdx;
    DecodeTransform(lumaOffset, AbsPartIdx, Depth, Log2TrafoSize, 0, CodeDQP);
}

void H265SegmentDecoder::DecodeTransform(Ipp32u offsetLuma, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  l2width, Ipp32u TrIdx, bool& CodeDQP)
{
    Ipp32u offsetChroma = offsetLuma >> 2;
    Ipp32u Subdiv;

    if (l2width == 2)
    {
        if ((AbsPartIdx & 0x03) == 0)
        {
            m_BakAbsPartIdx   = AbsPartIdx;
            m_BakChromaOffset = offsetChroma;
        }
    }

    if (m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA && m_cu->GetPartitionSize(AbsPartIdx) == PART_SIZE_NxN && Depth == m_cu->GetDepth(AbsPartIdx))
    {
        Subdiv = 1;
    }
    else if ((m_pSeqParamSet->max_transform_hierarchy_depth_inter == 1) && (m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTER) && (m_cu->GetPartitionSize(AbsPartIdx) != PART_SIZE_2Nx2N) && (Depth == m_cu->GetDepth(AbsPartIdx)))
    {
        Subdiv = (l2width > m_cu->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
    }
    else if (l2width > m_pSeqParamSet->log2_max_transform_block_size)
    {
        Subdiv = 1;
    }
    else if (l2width == m_pSeqParamSet->log2_min_transform_block_size)
    {
        Subdiv = 0;
    }
    else if (l2width == m_cu->getQuadtreeTULog2MinSizeInCU(AbsPartIdx))
    {
        Subdiv = 0;
    }
    else
    {
        VM_ASSERT(l2width > m_cu->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
        ParseTransformSubdivFlagCABAC(Subdiv, 5 - l2width);
    }

    const Ipp32u TrDepth = Depth - m_cu->GetDepth(AbsPartIdx);

    {
        const bool FirstCbfOfCUFlag = TrDepth == 0;
        if (FirstCbfOfCUFlag)
        {
            m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_U, AbsPartIdx, Depth);
            m_cu->setCbfSubParts( 0, COMPONENT_CHROMA_V, AbsPartIdx, Depth);
        }

        if (FirstCbfOfCUFlag || l2width > 2)
        {
            if (FirstCbfOfCUFlag || m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrDepth - 1))
                ParseQtCbfCABAC(AbsPartIdx, COMPONENT_CHROMA_U, TrDepth, Depth);
            if (FirstCbfOfCUFlag || m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrDepth - 1))
                ParseQtCbfCABAC(AbsPartIdx, COMPONENT_CHROMA_V, TrDepth, Depth);
        }
        else
        {
            m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrDepth - 1) << TrDepth, COMPONENT_CHROMA_U, AbsPartIdx, Depth);
            m_cu->setCbfSubParts(m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrDepth - 1) << TrDepth, COMPONENT_CHROMA_V, AbsPartIdx, Depth);
        }
    }

    if (Subdiv)
    {
        l2width--; Depth++;

        Ipp32u size = 1 << (l2width << 1);
        const Ipp32u QPartNum = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
        const Ipp32u StartAbsPartIdx = AbsPartIdx;
        Ipp32u YCbf = 0;
        Ipp32u UCbf = 0;
        Ipp32u VCbf = 0;

        for (Ipp32s i = 0; i < 4; i++)
        {
            DecodeTransform(offsetLuma, AbsPartIdx, Depth, l2width, TrIdx+1, CodeDQP);

            YCbf |= m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx, TrDepth + 1);
            UCbf |= m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrDepth + 1);
            VCbf |= m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrDepth + 1);
            AbsPartIdx += QPartNum;
            offsetLuma += size;
        }

        for (Ipp32u ui = 0; ui < 4 * QPartNum; ++ui)
        {
            m_cu->GetCbf(COMPONENT_LUMA)[StartAbsPartIdx + ui] |= YCbf << TrDepth;
            m_cu->GetCbf(COMPONENT_CHROMA_U)[StartAbsPartIdx + ui] |= UCbf << TrDepth;
            m_cu->GetCbf(COMPONENT_CHROMA_V)[StartAbsPartIdx + ui] |= VCbf << TrDepth;
        }
    }
    else
    {
        VM_ASSERT(Depth >= m_cu->GetDepth(AbsPartIdx));

        m_cu->setCbfSubParts(0, COMPONENT_LUMA, AbsPartIdx, Depth);
        if (m_cu->GetPredictionMode(AbsPartIdx) != MODE_INTRA && Depth == m_cu->GetDepth(AbsPartIdx) && !m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx) && !m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx))
        {
            m_cu->setCbfSubParts( 1 << TrDepth, COMPONENT_LUMA, AbsPartIdx, Depth);
        }
        else
        {
            ParseQtCbfCABAC(AbsPartIdx, COMPONENT_LUMA, TrDepth, Depth);
        }
            // transform_unit begin
        Ipp32u cbfY = m_cu->GetCbf(COMPONENT_LUMA    , AbsPartIdx, TrIdx);
        Ipp32u cbfU = m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrIdx);
        Ipp32u cbfV = m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrIdx);

        if (l2width == 2 && (AbsPartIdx & 0x3) == 0x3 )
        {
            cbfU = m_cu->GetCbf(COMPONENT_CHROMA_U, m_BakAbsPartIdx, TrIdx);
            cbfV = m_cu->GetCbf(COMPONENT_CHROMA_V, m_BakAbsPartIdx, TrIdx);
        }

        if (cbfY || cbfU || cbfV)
        {
            // dQP: only for LCU
            if (m_pPicParamSet->cu_qp_delta_enabled_flag)
            {
                if (CodeDQP)
                {
                    DecodeQP(m_bakAbsPartIdxCU);
                    UpdateNeighborDecodedQP(m_bakAbsPartIdxCU, m_cu->GetDepth(AbsPartIdx));
                    //if (m_bakAbsPartIdxCU != AbsPartIdx)
                        m_cu->UpdateTUQpInfo(m_bakAbsPartIdxCU, m_context->GetQP(), m_cu->GetDepth(AbsPartIdx));
                    CodeDQP = false;
                }
            }
        }

        m_cu->UpdateTUInfo(AbsPartIdx, TrDepth, Depth);

        UpdateNeighborBuffers(AbsPartIdx, Depth, false);

        if (cbfY)
        {
            ParseCoeffNxNCABAC(m_cu->m_TrCoeffY + offsetLuma, AbsPartIdx, l2width, Depth, COMPONENT_LUMA);
        }

        if (l2width > 2)
        {
            if (cbfU)
            {
                ParseCoeffNxNCABAC(m_cu->m_TrCoeffCb + offsetChroma, AbsPartIdx, l2width-1, Depth, COMPONENT_CHROMA_U);
            }
            if (cbfV)
            {
                ParseCoeffNxNCABAC(m_cu->m_TrCoeffCr + offsetChroma, AbsPartIdx, l2width-1, Depth, COMPONENT_CHROMA_V);
            }
        }
        else
        {
            if ( (AbsPartIdx & 0x3) == 0x3 )
            {
                if (cbfU)
                {
                    ParseCoeffNxNCABAC(m_cu->m_TrCoeffCb + m_BakChromaOffset, m_BakAbsPartIdx, l2width, Depth, COMPONENT_CHROMA_U);
                }
                if (cbfV)
                {
                    ParseCoeffNxNCABAC(m_cu->m_TrCoeffCr + m_BakChromaOffset, m_BakAbsPartIdx, l2width, Depth, COMPONENT_CHROMA_V);
                }
            }
        }
        // transform_unit end
    }
}

void H265SegmentDecoder::ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize)
{
    SubdivFlag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANS_SUBDIV_FLAG_HEVC] + Log2TransformBlockSize);
}

void H265SegmentDecoder::ParseQtCbfCABAC(Ipp32u AbsPartIdx, ComponentPlane plane, Ipp32u TrDepth, Ipp32u Depth)
{
    Ipp32u uVal;
    const Ipp32u Ctx = m_cu->getCtxQtCbf(plane, TrDepth);

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_CBF_HEVC] + Ctx + NUM_CONTEXT_QT_CBF * (plane ? COMPONENT_CHROMA : plane));

    m_cu->setCbfSubParts(uVal << TrDepth, plane, AbsPartIdx, Depth);
}

void H265SegmentDecoder::ParseQtRootCbfCABAC(Ipp32u& QtRootCbf)
{
    Ipp32u uVal;
    const Ipp32u Ctx = 0;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_ROOT_CBF_HEVC] + Ctx);
    QtRootCbf = uVal;
}

void H265SegmentDecoder::DecodeQP(Ipp32u AbsPartIdx)
{
    if (m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        ParseDeltaQPCABAC(AbsPartIdx);
    }
}

void H265SegmentDecoder::ParseDeltaQPCABAC(Ipp32u AbsPartIdx)
{
    Ipp32s qp;
    Ipp32u uiDQp;
    Ipp32s iDQp;

    Ipp32u uVal;

    ReadUnaryMaxSymbolCABAC(uiDQp, ctxIdxOffsetHEVC[DQP_HEVC], 1, CU_DQP_TU_CMAX);

    if(uiDQp >= CU_DQP_TU_CMAX)
    {
        ReadEpExGolombCABAC(uVal, CU_DQP_EG_k);
        uiDQp += uVal;
    }

    if (uiDQp > 0)
    {
        Ipp32u Sign;
        Ipp32s QPBdOffsetY = m_pSeqParamSet->getQpBDOffsetY();

        Sign = m_pBitStream->DecodeSingleBinEP_CABAC();
        iDQp = uiDQp;
        if (Sign)
            iDQp = -iDQp;

        qp = (((Ipp32s)getRefQP(AbsPartIdx) + iDQp + 52 + 2 * QPBdOffsetY) % (52 + QPBdOffsetY)) - QPBdOffsetY;
    }
    else
    {
        iDQp=0;
        qp = getRefQP(AbsPartIdx);
    }

    m_context->SetNewQP(qp);
    m_cu->m_CodedQP = (Ipp8u)qp;
}

void H265SegmentDecoder::ReadUnarySymbolCABAC(Ipp32u& Value, Ipp32s ctxIdx, Ipp32s Offset)
{
    Value = m_pBitStream->DecodeSingleBin_CABAC(ctxIdx);
    //m_pcTDecBinIf->decodeBin( ruiSymbol, pcSCModel[0] );

    if (!Value)
    {
        return;
    }

    Ipp32u uVal = 0;
    Ipp32u Cont;

    do
    {
        Cont = m_pBitStream->DecodeSingleBin_CABAC(ctxIdx + Offset);
        //m_pcTDecBinIf->decodeBin( uiCont, pcSCModel[ iOffset ] );
        uVal++;
    }
    while (Cont);

    Value = uVal;
}

void H265SegmentDecoder::FinishDecodeCU(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    IsLast = DecodeSliceEnd(AbsPartIdx, Depth);
}

void H265SegmentDecoder::BeforeCoeffs(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    H265CodingUnitData * data = m_cu->GetCUData(AbsPartIdx);
    data->qp = (Ipp8s)m_context->GetQP();
    data->trStart = Ipp8u(AbsPartIdx);
    m_cu->SetCUDataSubParts(AbsPartIdx, Depth);
}

/**decode end-of-slice flag
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Bool
 */
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

    return (IsLast > 0);
}

//LUMA map
static const Ipp32u ctxIndMap[16] =
{
    0, 1, 4, 5,
    2, 3, 4, 5,
    6, 6, 8, 8,
    7, 7, 8, 8
};

static const Ipp32u lCtx[4] = {0x00010516, 0x06060606, 0x000055AA, 0xAAAAAAAA};

static H265_FORCEINLINE Ipp32u getSigCtxInc(Ipp32s patternSigCtx,
                                 Ipp32u scanIdx,
                                 const Ipp32u PosX,
                                 const Ipp32u PosY,
                                 const Ipp32u log2BlockSize,
                                 bool IsLuma)
{
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

static H265_FORCEINLINE Ipp32u Rst2ZS(const Ipp32u x, const Ipp32u y, const Ipp32u l2w)
{
    const Ipp32u diagId = x + y;
    const Ipp32u w = 1<<l2w;

    return (diagId < w) ? (diagOffs[diagId] + x) : ((1<<(l2w<<1)) - diagOffs[((w<<1)-1)-diagId] + (w-y-1));
}

void H265SegmentDecoder::ParseCoeffNxNCABAC(H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, Ipp32u Depth, ComponentPlane plane)
{
    if (m_pSeqParamSet->scaling_list_enabled_flag)
    {
        ParseCoeffNxNCABACOptimized<true>(pCoef, AbsPartIdx, Log2BlockSize, Depth, plane);
    }
    else
    {
        ParseCoeffNxNCABACOptimized<false>(pCoef, AbsPartIdx, Log2BlockSize, Depth, plane);
    }
}

template <bool scaling_list_enabled_flag>
void H265SegmentDecoder::ParseCoeffNxNCABACOptimized(H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, Ipp32u Depth, ComponentPlane plane)
{
    const Ipp32u Size    = 1 << Log2BlockSize;
    const Ipp32u L2Width = Log2BlockSize-2;

    const bool   IsLuma  = (plane == COMPONENT_LUMA);
    const bool   beValid = m_cu->GetCUTransquantBypass(AbsPartIdx) ? false : m_pPicParamSet->sign_data_hiding_enabled_flag;

    const Ipp32u baseCoeffGroupCtxIdx = ctxIdxOffsetHEVC[SIG_COEFF_GROUP_FLAG_HEVC] + (IsLuma ? 0 : NUM_CONTEXT_SIG_COEFF_GROUP_FLAG);
    const Ipp32u baseCtxIdx = ctxIdxOffsetHEVC[SIG_FLAG_HEVC] + ( IsLuma ? 0 : NUM_CONTEXT_SIG_FLAG_LUMA);

    if (m_pPicParamSet->transform_skip_enabled_flag && !L2Width)
        ParseTransformSkipFlags(AbsPartIdx, Depth, plane);
    
    memset(pCoef, 0, sizeof(H265CoeffsCommon) << (Log2BlockSize << 1));

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
    Ipp32s idx = plane;
    Ipp32s QPRem = m_context->m_ScaledQP[idx].m_QPRem;
    Ipp32s QPPer = m_context->m_ScaledQP[idx].m_QPPer;

    Ipp32s Shift;
    Ipp32s Add, Scale;
    Ipp16s *pDequantCoef;
    bool shift_right = true;

    if (scaling_list_enabled_flag)
    {
        Shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - TransformShift + 4;
        pDequantCoef = m_context->getDequantCoeff(idx + (m_cu->GetPredictionMode(AbsPartIdx) == MODE_INTRA ? 0 : 3), QPRem, c_Log2TrSize - 2);

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
        Scale = m_context->m_ScaledQP[idx].m_QPScale;
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
                H265CoeffsCommon coef;

                coef = (H265CoeffsCommon)absCoeff[idx];
                absSum += absCoeff[idx];

                if (idx == numNonZero - 1 && signHidden && beValid)
                {
                    if (absSum & 0x1)
                        coef = -coef;
                }
                else
                {
                    Ipp32s sign = static_cast<Ipp32s>(coeffSigns) >> 31;
                    coef = (H265CoeffsCommon)((coef ^ sign) - sign);
                    coeffSigns <<= 1;
                }

                if (m_cu->GetCUTransquantBypass(AbsPartIdx))
                {
                    pCoef[blkPos] = (H265CoeffsCommon)coef;
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

                    pCoef[blkPos] = (H265CoeffsCommon)Saturate(-32768, 32767, coeffQ);
                }
            }
        }

        LastPinCG = 15;
    }
}

void H265SegmentDecoder::ParseTransformSkipFlags(Ipp32u AbsPartIdx, Ipp32u Depth, ComponentPlane plane)
{
    if (m_cu->GetCUTransquantBypass(AbsPartIdx))
        return;

    Ipp32u useTransformSkip;
    Ipp32u CtxIdx = ctxIdxOffsetHEVC[TRANSFORM_SKIP_HEVC] + (plane ? COMPONENT_CHROMA : COMPONENT_LUMA) * NUM_CONTEXT_TRANSFORMSKIP_FLAG;
    useTransformSkip = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    if (plane != COMPONENT_LUMA)
    {
        const Ipp32u Log2TrafoSize = g_ConvertToBit[m_pCurrentFrame->m_CodingData->m_MaxCUWidth] + 2 - Depth;
        if(Log2TrafoSize == 2)
        {
            Depth --;
        }
    }

    m_cu->setTransformSkip(useTransformSkip, plane, AbsPartIdx);
}

/** Parse (X,Y) position of the last significant coefficient
 * \param uiPosLastX reference to X component of last coefficient
 * \param uiPosLastY reference to Y component of last coefficient
 * \param uiWidth block width
 * \param eTType plane type / luminance or chrominance
 * \param uiCTXIdx block size context
 * \param uiScanIdx scan type (zig-zag, hor, ver)
 * \returns Void
 * This method decodes the X and Y component within a block of the last significant coefficient.
 */

Ipp32u H265SegmentDecoder::ParseLastSignificantXYCABAC(Ipp32u &PosLastX, Ipp32u &PosLastY, Ipp32u L2Width, bool IsLuma, Ipp32u ScanIdx)
{
    Ipp32u mGIdx = g_GroupIdx[(1<<(L2Width+2)) - 1];

    Ipp32u blkSizeOffset = IsLuma ? (L2Width*3 + ((L2Width+1)>>2)) : NUM_CONTEX_LAST_COEF_FLAG_XY;
    Ipp32u shift = IsLuma ? ((L2Width+3) >> 2) : L2Width;
    Ipp32u CtxIdxX = ctxIdxOffsetHEVC[LAST_X_HEVC] + blkSizeOffset;
    Ipp32u CtxIdxY = ctxIdxOffsetHEVC[LAST_Y_HEVC] + blkSizeOffset;

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

/** Parsing of coeff_abs_level_minus3
 * \param ruiSymbol reference to coeff_abs_level_minus3
 * \param ruiGoRiceParam reference to Rice parameter
 * \returns Void
 */
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

void H265SegmentDecoder::ReconstructCU(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    bool BoundaryFlag = false;
    Ipp32s Size = m_pSeqParamSet->MaxCUSize >> Depth;
    Ipp32u XInc = m_cu->m_rasterToPelX[AbsPartIdx];
    Ipp32u LPelX = m_cu->m_CUPelX + XInc;
    Ipp32u YInc = m_cu->m_rasterToPelY[AbsPartIdx];
    Ipp32u TPelY = m_cu->m_CUPelY + YInc;

    H265SliceHeader* pSliceHeader = m_cu->m_SliceHeader;
    if (LPelX + Size > pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples || TPelY + Size > pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples)
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
                if (LPelX >= pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples ||
                    TPelY >= pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples)
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
            VM_ASSERT(0);
        break;
    }

    if (m_cu->isLosslessCoded(AbsPartIdx) && !m_cu->GetIPCMFlag(AbsPartIdx))
    {
        // Saving reconstruct contents in PCM buffer is necessary for later PCM restoration when SAO is enabled
        FillPCMBuffer(AbsPartIdx, Depth);
    }
}

void H265SegmentDecoder::ReconInter(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // inter prediction
    m_Prediction->MotionCompensation(m_cu, AbsPartIdx, Depth);

    // clip for only non-zero cbp case
    if ((m_cu->GetCbf(COMPONENT_LUMA, AbsPartIdx)) || (m_cu->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx)) || (m_cu->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx)))
    {
        // inter recon
        DecodeInterTexture(AbsPartIdx);
    }
}

void H265SegmentDecoder::DecodeInterTexture(Ipp32u AbsPartIdx)
{
    Ipp32u Size = m_cu->GetWidth(AbsPartIdx);

    m_TrQuant->InvRecurTransformNxN(m_cu, AbsPartIdx, Size, 0);
}

void H265SegmentDecoder::ReconPCM(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // Luma
    Ipp32u Size  = (m_pSeqParamSet->MaxCUSize >> Depth);
    Ipp32u MinCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUWidth();
    Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265PlanePtrYCommon pPcmY = (H265PlanePtrYCommon)(m_cu->m_TrCoeffY + LumaOffset);
    H265PlanePtrYCommon pPicReco = m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx);
    Ipp32u PitchLuma = m_pCurrentFrame->pitch_luma();
    Ipp32u PcmLeftShiftBit = m_pSeqParamSet->bit_depth_luma - m_pSeqParamSet->pcm_sample_bit_depth_luma;

    for (Ipp32u Y = 0; Y < Size; Y++)
    {
        for (Ipp32u X = 0; X < Size; X++)
        {
            pPicReco[X] = pPcmY[X] << PcmLeftShiftBit;
        }
        pPcmY += Size;
        pPicReco += PitchLuma;
    }

    // Cb and Cr
    Size >>= 1;
    H265PlanePtrUVCommon pPcmCb = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCb + ChromaOffset);
    H265PlanePtrUVCommon pPcmCr = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCr + ChromaOffset);
    pPicReco = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx);
    Ipp32u PitchChroma = m_pCurrentFrame->pitch_chroma();
    PcmLeftShiftBit = m_pSeqParamSet->bit_depth_chroma - m_pSeqParamSet->pcm_sample_bit_depth_chroma;

    for (Ipp32u Y = 0; Y < Size; Y++)
    {
        for (Ipp32u X = 0; X < Size; X++)
        {
            pPicReco[X * 2] = pPcmCb[X] << PcmLeftShiftBit;
            pPicReco[X * 2 + 1] = pPcmCr[X] << PcmLeftShiftBit;
        }
        pPcmCb += Size;
        pPcmCr += Size;
        pPicReco += PitchChroma;
    }
}

void H265SegmentDecoder::FillPCMBuffer(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // Luma
    Ipp32u Size = (m_pSeqParamSet->MaxCUSize >> Depth);
    Ipp32u MinCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUWidth();
    Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265PlanePtrYCommon pPcmY = (H265PlanePtrYCommon)(m_cu->m_TrCoeffY + LumaOffset);
    H265PlanePtrYCommon pRecoY = m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr, AbsPartIdx);
    Ipp32u stride = m_pCurrentFrame->pitch_luma();

    for(Ipp32u y = 0; y < Size; y++)
    {
        for(Ipp32u x = 0; x < Size; x++)
        {
            pPcmY[x] = pRecoY[x];
        }
        pPcmY += Size;
        pRecoY += stride;
    }

    // Cb and Cr
    Size >>= 1;

    H265PlanePtrUVCommon pPcmCb = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCb + ChromaOffset);
    H265PlanePtrUVCommon pPcmCr = (H265PlanePtrUVCommon)(m_cu->m_TrCoeffCr + ChromaOffset);
    H265PlanePtrUVCommon pRecoCbCr = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr, AbsPartIdx);
    stride = m_pCurrentFrame->pitch_chroma();

    for(Ipp32u y = 0; y < Size; y++)
    {
        for(Ipp32u x = 0; x < Size; x++)
        {
            pPcmCb[x] = pRecoCbCr[x * 2];
            pPcmCr[x] = pRecoCbCr[x * 2 + 1];
        }
        pPcmCr += Size;
        pPcmCb += Size;
        pRecoCbCr += stride;
    }
}

void H265SegmentDecoder::UpdatePUInfo(Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, H265MVInfo &MVi)
{
    VM_ASSERT(MVi.m_refIdx[REF_PIC_LIST_0] >= 0 || MVi.m_refIdx[REF_PIC_LIST_1] >= 0);

    for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
    {
        if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0 && MVi.m_refIdx[RefListIdx] >= 0)
        {
            H265DecoderRefPicList::ReferenceInformation &refInfo = m_pRefPicList[RefListIdx][MVi.m_refIdx[RefListIdx]];
            Ipp32s POCDelta = m_pCurrentFrame->m_PicOrderCnt - refInfo.refFrame->m_PicOrderCnt;
            MVi.m_pocDelta[RefListIdx] = POCDelta;
            MVi.m_flags[RefListIdx] = Ipp8u(refInfo.isLongReference ? COL_TU_LT_INTER : COL_TU_ST_INTER);

            if (MVi.m_mv[RefListIdx].Vertical > m_context->m_mvsDistortion)
                m_context->m_mvsDistortion = MVi.m_mv[RefListIdx].Vertical;
        }
        else
        {
            MVi.m_flags[RefListIdx] = COL_TU_INVALID_INTER;
        }
    }

    Ipp32u mask = (1 << m_pSeqParamSet->MaxCUDepth) - 1;
    PartX &= mask;
    PartY &= mask;

    H265MVInfo *pMVInfo;
    Ipp32s stride = m_context->m_CurrCTBStride;
    
    pMVInfo = &m_context->m_CurrCTB[PartY * stride + PartX];
    for (Ipp32u y = 0; y < PartHeight; y++)
    {
        for (Ipp32u x = 0; x < PartWidth; x++)
        {
            pMVInfo[x] = MVi;
        }
        pMVInfo += stride;
    }

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
        H265MVInfo *pMVInfo = &m_context->m_CurrCTB[YInc * stride + XInc];
        for (Ipp32s i = 0; i < PartSize; i++)
        {
            for (Ipp32s j = 0; j < PartSize; j++)
            {
                pMVInfo[j].m_flags[REF_PIC_LIST_0] = COL_TU_INTRA;
            }
            pMVInfo += stride;
        }
    }
}

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

bool H265SegmentDecoder::hasEqualMotion(Ipp32s dir1, Ipp32s dir2)
{
    if (m_context->m_CurrCTB[dir1].m_refIdx[REF_PIC_LIST_0] != m_context->m_CurrCTB[dir2].m_refIdx[REF_PIC_LIST_0] ||
        m_context->m_CurrCTB[dir1].m_refIdx[REF_PIC_LIST_1] != m_context->m_CurrCTB[dir2].m_refIdx[REF_PIC_LIST_1])
        return false;

    if (m_context->m_CurrCTB[dir1].m_refIdx[REF_PIC_LIST_0] >= 0 && m_context->m_CurrCTB[dir1].m_mv[REF_PIC_LIST_0] != m_context->m_CurrCTB[dir2].m_mv[REF_PIC_LIST_0])
        return false;

    if (m_context->m_CurrCTB[dir1].m_refIdx[REF_PIC_LIST_1] >= 0 && m_context->m_CurrCTB[dir1].m_mv[REF_PIC_LIST_1] != m_context->m_CurrCTB[dir2].m_mv[REF_PIC_LIST_1])
        return false;

    return true;
}

#define UPDATE_MV_INFO(dir)                                                                         \
    CandIsInter[Count] = true;                                                                      \
    if (m_context->m_CurrCTB[dir].m_refIdx[REF_PIC_LIST_0] >= 0)                                      \
    {                                                                                               \
        InterDirNeighbours[Count] = 1;                                                              \
        MVBufferNeighbours[Count].setMVInfo(REF_PIC_LIST_0, m_context->m_CurrCTB[dir].m_refIdx[REF_PIC_LIST_0], m_context->m_CurrCTB[dir].m_mv[REF_PIC_LIST_0]); \
    }                                                                                               \
    else                                                                                            \
        InterDirNeighbours[Count] = 0;                                                              \
                                                                                                    \
    if (B_SLICE == m_pSliceHeader->slice_type && m_context->m_CurrCTB[dir].m_refIdx[REF_PIC_LIST_1] >= 0) \
    {                                                                                               \
        InterDirNeighbours[Count] += 2;                                                             \
        MVBufferNeighbours[Count].setMVInfo(REF_PIC_LIST_1, m_context->m_CurrCTB[dir].m_refIdx[REF_PIC_LIST_1], m_context->m_CurrCTB[dir].m_mv[REF_PIC_LIST_1]); \
    }                                                                                               \
                                                                                                    \
    if (mrgCandIdx == Count)                                                                        \
    {                                                                                               \
        return;                                                                                     \
    }                                                                                               \
    Count++;


static Ipp32u PriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
static Ipp32u PriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};

void H265SegmentDecoder::getInterMergeCandidates(Ipp32u AbsPartIdx, Ipp32u PartIdx, H265MVInfo *MVBufferNeighbours,
                                                 Ipp8u *InterDirNeighbours, Ipp32s &numValidMergeCand, Ipp32s mrgCandIdx)
{
    numValidMergeCand = m_pSliceHeader->max_num_merge_cand;
    bool CandIsInter[MERGE_MAX_NUM_CAND];
    for (Ipp32s ind = 0; ind < numValidMergeCand; ++ind)
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

    Ipp32s nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32s PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;
    EnumPartSize CurPS = m_cu->GetPartitionSize(AbsPartIdx);

    Ipp32s Count = 0;

    // left
    Ipp32s leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    bool isAvailableA1 = m_context->m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == PART_SIZE_Nx2N || CurPS == PART_SIZE_nLx2N || CurPS == PART_SIZE_nRx2N));

    if (isAvailableA1)
    {
        UPDATE_MV_INFO(leftAddr);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above
    Ipp32s aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    bool isAvailableB1 = m_context->m_CurrCTBFlags[aboveAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW - 1, TPelY - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == PART_SIZE_2NxN || CurPS == PART_SIZE_2NxnU || CurPS == PART_SIZE_2NxnD));

    if (isAvailableB1 && (!isAvailableA1 || !hasEqualMotion(leftAddr, aboveAddr)))
    {
        UPDATE_MV_INFO(aboveAddr);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above right
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    bool isAvailableB0 = m_context->m_CurrCTBFlags[aboveRightAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveRightAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW, TPelY - 1, LPelX, TPelY);

    if (isAvailableB0 && (!isAvailableB1 || !hasEqualMotion(aboveAddr, aboveRightAddr)))
    {
        UPDATE_MV_INFO(aboveRightAddr);
    }

    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // left bottom
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;
    bool isAvailableA0 = m_context->m_CurrCTBFlags[leftBottomAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftBottomAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH, LPelX, TPelY);

    if (isAvailableA0 && (!isAvailableA1 || !hasEqualMotion(leftAddr, leftBottomAddr)))
    {
        UPDATE_MV_INFO(leftBottomAddr);
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

    // above left
    if (Count < 4 && PartX > 0)
    {
        Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;
        bool isAvailableB2 = m_context->m_CurrCTBFlags[aboveLeftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveLeftAddr].members.IsIntra &&
            isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY - 1, LPelX, TPelY);

         if (isAvailableB2 &&
            (!isAvailableA1 || !hasEqualMotion(leftAddr, aboveLeftAddr)) &&
            (!isAvailableB1 || !hasEqualMotion(aboveAddr, aboveLeftAddr)))
        {
            UPDATE_MV_INFO(aboveLeftAddr);
        }
    }

    // early termination
    if (Count == m_pSliceHeader->max_num_merge_cand)
        return;

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

    if (m_pSliceHeader->slice_type == B_SLICE)
    {
        for (Ipp32s idx = 0; idx < Cutoff * (Cutoff - 1) && ArrayAddr != m_pSliceHeader->max_num_merge_cand; idx++)
        {
            Ipp32s i = PriorityList0[idx];
            Ipp32s j = PriorityList1[idx];
            if (CandIsInter[i] && CandIsInter[j] && (InterDirNeighbours[i] & 0x1) && (InterDirNeighbours[j] & 0x2))
            {
                CandIsInter[ArrayAddr] = true;
                InterDirNeighbours[ArrayAddr] = 3;

                // get MV from cand[i] and cand[j]
                MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_0, MVBufferNeighbours[i].m_refIdx[REF_PIC_LIST_0], MVBufferNeighbours[i].m_mv[REF_PIC_LIST_0]);
                MVBufferNeighbours[ArrayAddr].setMVInfo(REF_PIC_LIST_1, MVBufferNeighbours[j].m_refIdx[REF_PIC_LIST_1], MVBufferNeighbours[j].m_mv[REF_PIC_LIST_1]);

                Ipp32s RefPOCL0 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_0][MVBufferNeighbours[ArrayAddr].m_refIdx[REF_PIC_LIST_0]];
                Ipp32s RefPOCL1 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_1][MVBufferNeighbours[ArrayAddr].m_refIdx[REF_PIC_LIST_1]];
                if (RefPOCL0 == RefPOCL1 && MVBufferNeighbours[ArrayAddr].m_mv[REF_PIC_LIST_0] == MVBufferNeighbours[ArrayAddr].m_mv[REF_PIC_LIST_1])
                {
                    CandIsInter[ArrayAddr] = false;
                }
                else
                {
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

    numValidMergeCand = ArrayAddr;
}

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

    Ipp32s nPSW, nPSH;
    m_cu->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32u PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32u PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;

    // Left predictor search
    Ipp32s leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_context->m_CurrCTBStride * PartHeight;

    Added = AddMVPCand(pInfo, RefPicList, RefIdx, leftBottomAddr);
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, leftAddr);
    }
    if (!Added)
    {
        Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftBottomAddr);

        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftAddr);
        }
    }

    // Above predictor search
    Ipp32s aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride - 1;

    Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveRightAddr);
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveAddr);
    }
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveLeftAddr);
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
        Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveRightAddr);

        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveAddr);
        }
        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveLeftAddr);
        }
    }

    if (pInfo->NumbOfCands == 2)
    {
        if (pInfo->MVCandidate[0] == pInfo->MVCandidate[1])
        {
            pInfo->NumbOfCands = 1;
        }
    }

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

bool H265SegmentDecoder::AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    Ipp32s candRefIdx = m_context->m_CurrCTB[NeibAddr].m_refIdx[RefPicList];
    if (candRefIdx >= 0 && m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList][candRefIdx].refFrame->m_PicOrderCnt)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands++] = m_context->m_CurrCTB[NeibAddr].m_mv[RefPicList];
        return true;
    }

    EnumRefPicList RefPicList2nd = EnumRefPicList(1 - RefPicList);
    RefIndexType NeibRefIdx = m_context->m_CurrCTB[NeibAddr].m_refIdx[RefPicList2nd];

    if (NeibRefIdx >= 0)
    {
        if(m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList2nd][NeibRefIdx].refFrame->m_PicOrderCnt)
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = m_context->m_CurrCTB[NeibAddr].m_mv[RefPicList2nd];
            return true;
        }
    }
    return false;
}

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

bool H265SegmentDecoder::AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr)
{
    if (!m_context->m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_context->m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    Ipp32s CurrRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt;
    bool IsCurrRefLongTerm = m_pRefPicList[RefPicList][RefIdx].isLongReference;

    for (Ipp32s i = 0; i < 2; i++)
    {
        RefIndexType NeibRefIdx = m_context->m_CurrCTB[NeibAddr].m_refIdx[RefPicList];

        if (NeibRefIdx >= 0)
        {
            bool IsNeibRefLongTerm = m_pRefPicList[RefPicList][NeibRefIdx].isLongReference;

            if (IsNeibRefLongTerm == IsCurrRefLongTerm)
            {
                H265MotionVector MvPred = m_context->m_CurrCTB[NeibAddr].m_mv[RefPicList];

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
    Ipp32u colocatedPartNumber = PartY * m_pCurrentFrame->getNumPartInCUSize() * m_pCurrentFrame->getFrameWidthInCU() + PartX;

    // Intra mode is marked in ref list 0 only
    if (COL_TU_INTRA == colPic->m_CodingData->GetTUFlags(REF_PIC_LIST_0, colocatedPartNumber))
        return false;

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
