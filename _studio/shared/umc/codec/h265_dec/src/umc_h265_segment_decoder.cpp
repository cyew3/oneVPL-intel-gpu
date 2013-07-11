/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
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

namespace UMC_HEVC_DECODER
{

DecodingContext::DecodingContext()
{
    m_sps = 0;
    m_pps = 0;
    m_frame = 0;
    m_TopNgbrs = 0;
    m_TopMVInfo = 0;
    m_CurrCTBFlags = 0;
    m_CurrCTB = 0;
    m_CurrCTBStride = 0;
}

void DecodingContext::Init(H265Task *task)
{
    m_sps = task->m_pSlice->GetSeqParam();
    m_pps = task->m_pSlice->GetPicParam();
    m_frame = task->m_pSlice->GetCurrentFrame();

    if (m_TopNgbrsHolder.size() < m_sps->NumPartitionsInFrameWidth + 2 || m_TopMVInfoHolder.size() < m_sps->NumPartitionsInFrameWidth + 2)
    {
        // Zero element is left-top diagonal from zero CTB
        m_TopNgbrsHolder.resize(m_sps->NumPartitionsInFrameWidth + 2);
        // Zero element is left-top diagonal from zero CTB
        m_TopMVInfoHolder.resize(m_sps->NumPartitionsInFrameWidth + 2);
    }

    m_TopNgbrs = &m_TopNgbrsHolder[1];
    m_TopMVInfo = &m_TopMVInfoHolder[1];

    m_CurrCTBStride = (m_sps->NumPartitionsInCUSize + 2);
    if (m_CurrCTBHolder.size() < (Ipp32u)(m_CurrCTBStride * m_CurrCTBStride))
    {
        m_CurrCTBHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
        m_CurrCTBFlagsHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
    }

    m_CurrCTB = &m_CurrCTBHolder[m_CurrCTBStride + 1];
    m_CurrCTBFlags = &m_CurrCTBFlagsHolder[m_CurrCTBStride + 1];

    Ipp32s sliceNum = task->m_pSlice->GetSliceNum();
    m_refPicList[0] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_refPicList[1] = m_frame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;
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

    if (newCUY > 0)
    {
        if (newCUX > lastCUX)
        {
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i].data = TopNgbrs[newCUX + i].data;
                CurrCTB[i].mvinfo[0] = TopMVInfo[newCUX + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = TopMVInfo[newCUX + i].mvinfo[1];
            }
            // Store bottom margin for next row if next CTB is to the right. This is necessary for left-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
            }
        }
        else if (newCUX < lastCUX)
        {
            // Store bottom margin for next row if next CTB is to the left-below. This is necessary for right-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
            }
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i].data = TopNgbrs[newCUX + i].data;
                CurrCTB[i].mvinfo[0] = TopMVInfo[newCUX + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = TopMVInfo[newCUX + i].mvinfo[1];
            }
        }
        else // New CTB right under previous CTB
        {
            // Copy data from bottom row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                TopNgbrs[lastCUX + i].data = CurrCTBFlags[i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                CurrCTB[i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
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
            TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
            TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
            TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
        }
    }

    if (lastCUY == newCUY)
    {
        // Copy left margin from right
        for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            CurrCTBFlags[i * m_CurrCTBStride].data = CurrCTBFlags[i * m_CurrCTBStride + m_CurrCTBStride - 2].data;

        for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
        {
            CurrCTB[i * m_CurrCTBStride].mvinfo[0] = CurrCTB[i * m_CurrCTBStride + m_CurrCTBStride - 2].mvinfo[0];
            CurrCTB[i * m_CurrCTBStride].mvinfo[1] = CurrCTB[i * m_CurrCTBStride + m_CurrCTBStride - 2].mvinfo[1];
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

void DecodingContext::ResetRowBuffer()
{
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    for (Ipp32u i = 0; i < m_sps->NumPartitionsInFrameWidth + 2; i++)
        TopNgbrs[i].data = 0;

    for (Ipp32s i = 0; i < m_CurrCTBStride * m_CurrCTBStride; i++)
        CurrCTBFlags[i].data = 0;
}

H265SegmentDecoder::H265SegmentDecoder(TaskBroker_H265 * pTaskBroker)
{
    m_pTaskBroker = pTaskBroker;

    m_pSlice = 0;

    bit_depth_luma = 8;
    bit_depth_chroma = 8;

    //h265 members from TDecCu:
    m_curCU = NULL;
    // prediction buffer
    m_Prediction.reset(new H265Prediction()); //PREDICTION

    m_ppcYUVResi = 0;
    m_TrQuant = 0;

    m_MaxDepth = 0;

    m_context = new DecodingContext();

    g_SigLastScan_inv[0][0] = 0;
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
    m_DecodeDQPFlag = false;

    m_MaxDepth = sps->MaxCUDepth + 1;

    if (!m_ppcYUVResi || m_ppcYUVResi->lumaSize().width != sps->MaxCUWidth || m_ppcYUVResi->lumaSize().height != sps->MaxCUHeight)
    {
        // initialize partition order.
        Ipp32u* Tmp = &g_ZscanToRaster[0];
        initZscanToRaster(m_MaxDepth, 1, 0, Tmp);
        initRasterToZscan(sps->MaxCUWidth, sps->MaxCUHeight, m_MaxDepth );

        // initialize conversion matrix from partition index to pel
        initRasterToPelXY(sps->MaxCUWidth, sps->MaxCUHeight, m_MaxDepth);

        if (!m_ppcYUVResi)
        {
            m_ppcYUVResi = new H265DecYUVBufferPadded;
            m_ppcYUVReco = new H265DecYUVBufferPadded;
        }
        else
        {
            m_ppcYUVResi->destroy();
            m_ppcYUVReco->destroy();
        }

        m_ppcYUVResi->create(sps->MaxCUWidth, sps->MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), sps->MaxCUWidth, sps->MaxCUHeight, sps->MaxCUDepth);
        m_ppcYUVReco->create(sps->MaxCUWidth, sps->MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), sps->MaxCUWidth, sps->MaxCUHeight, sps->MaxCUDepth);
    }

    if (!m_TrQuant)
        m_TrQuant = new H265TrQuant(); //TRQUANT

    m_TrQuant->Init(sps->MaxCUWidth, sps->MaxCUHeight, m_pSeqParamSet->m_maxTrSize);
    m_SAO.init(sps);

    if (!g_SigLastScan_inv[0][0])
    {
        for(int i = 0; i < 3; i++)
        {
            CumulativeArraysAllocation(4, 0, &g_SigLastScan_inv[i][0], 2*16, &g_SigLastScan_inv[i][1], 2*64, &g_SigLastScan_inv[i][2], 2*16*16, &g_SigLastScan_inv[i][3], 2*32*32);

            for(Ipp16u j = 0; j < 16; j++)
            {
                g_SigLastScan_inv[i][0][g_SigLastScan[i][0][j]] = j;
            }
            for(Ipp16u j = 0; j < 64; j++)
            {
                g_SigLastScan_inv[i][1][g_SigLastScan[i][1][j]] = j;
            }
            for(Ipp16u j = 0; j < 16*16; j++)
            {
                g_SigLastScan_inv[i][2][g_SigLastScan[i][2][j]] = j;
            }
            for(Ipp16u j = 0; j < 32*32; j++)
            {
               g_SigLastScan_inv[i][3][g_SigLastScan[i][3][j]] = j;
            }
        }
    }
}

void H265SegmentDecoder::destroy()
{
    m_SAO.destroy();
}

void H265SegmentDecoder::Release(void)
{
    delete m_TrQuant;
    m_TrQuant = NULL; //TRQUANT

    if (g_SigLastScan_inv[0][0])
    {
        for(int i = 0; i < 3; i++)
        {
            CumulativeFree(g_SigLastScan_inv[i][0]);
        }
    }

   g_SigLastScan_inv[0][0] = 0;

    if (m_ppcYUVResi)
    {
        m_ppcYUVResi->destroy();
        delete m_ppcYUVResi;
        m_ppcYUVResi = NULL;

        m_ppcYUVReco->destroy();
        delete m_ppcYUVReco;
        m_ppcYUVReco = NULL;
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
        VM_ASSERT(false);
        return 0;
    }
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

    if (compIdx==2)
    {
        uiSymbol = (Ipp32u)( psSaoLcuParam->m_typeIdx + 1);
    }
    else
    {
        parseSaoTypeIdx(uiSymbol);
    }

    psSaoLcuParam->m_typeIdx = (Ipp32s)uiSymbol - 1;

    if (uiSymbol)
    {
        psSaoLcuParam->m_length = iTypeLength[psSaoLcuParam->m_typeIdx];

        Ipp32s bitDepth = compIdx ? g_bitDepthC : g_bitDepthY;
        Ipp32s offsetTh = 1 << IPP_MIN(bitDepth - 5, 5);

        if( psSaoLcuParam->m_typeIdx == SAO_BO )
        {
            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                parseSaoMaxUvlc(uiSymbol, offsetTh -1 );
                psSaoLcuParam->m_offset[i] = uiSymbol;
            }

            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                if (psSaoLcuParam->m_offset[i] != 0)
                {
                    uiSymbol = m_pBitStream->DecodeSingleBinEP_CABAC();

                    if (uiSymbol)
                    {
                        psSaoLcuParam->m_offset[i] = -psSaoLcuParam->m_offset[i] ;
                    }
                }
            }
            parseSaoUflc(5, uiSymbol);
            psSaoLcuParam->m_subTypeIdx = uiSymbol;
        }
        else if( psSaoLcuParam->m_typeIdx < 4 )
        {
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[0] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[1] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[2] = -(Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[3] = -(Ipp32s)uiSymbol;
            if (compIdx != 2)
            {
                parseSaoUflc(2, uiSymbol );
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

void H265SegmentDecoder::DecodeSAOOneLCU(H265CodingUnit* pCU)
{
    if (m_pSeqParamSet->sample_adaptive_offset_enabled_flag && (m_pSliceHeader->m_SaoEnabledFlag ||
                                     m_pSliceHeader->m_SaoEnabledFlagChroma))
    {
        H265DecoderFrame* m_Frame = pCU->m_Frame;
        SAOParams *saoParam  = &m_pSliceHeader->m_SAOParam;
        Ipp32u curCUAddr = pCU->CUAddr;
        Ipp32s numCuInWidth  = m_Frame->m_CodingData->m_WidthInCU;
        Ipp32s cuAddrInSlice = curCUAddr - m_Frame->m_CodingData->getCUOrderMap(m_pSliceHeader->SliceCurStartCUAddr/m_Frame->getCD()->getNumPartInCU());
        Ipp32s cuAddrUpInSlice  = cuAddrInSlice - numCuInWidth;
        Ipp32s rx = curCUAddr % numCuInWidth;
        Ipp32s ry = curCUAddr / numCuInWidth;
        Ipp32s allowMergeLeft = 1;
        Ipp32s allowMergeUp   = 1;

        saoParam->m_bSaoFlag[0] = m_pSliceHeader->m_SaoEnabledFlag;

        //if (curCUAddr == iStartCUAddr)
        {
            saoParam->m_bSaoFlag[1] = m_pSliceHeader->m_SaoEnabledFlagChroma;
        }

        if (rx != 0)
        {
            if (m_Frame->m_CodingData->getTileIdxMap(curCUAddr - 1) != m_Frame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeLeft = 0;
            }
        }

        if (ry != 0)
        {
            if (m_Frame->m_CodingData->getTileIdxMap(curCUAddr - numCuInWidth) != m_Frame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeUp = 0;
            }
        }

        parseSaoOneLcuInterleaving(rx, ry, saoParam, pCU, cuAddrInSlice, cuAddrUpInSlice, allowMergeLeft, allowMergeUp);
    }
}

void H265SegmentDecoder::parseSaoOneLcuInterleaving(Ipp32s rx,
                                                    Ipp32s ry,
                                                    SAOParams* pSaoParam,
                                                    H265CodingUnit* pcCU,
                                                    Ipp32s iCUAddrInSlice,
                                                    Ipp32s iCUAddrUpInSlice,
                                                    Ipp32s allowMergeLeft,
                                                    Ipp32s allowMergeUp)
{
    Ipp32s iAddr = pcCU->CUAddr;
    Ipp32u uiSymbol;
    SAOLCUParam* saoLcuParam[3];

    saoLcuParam[0] = pcCU->m_Frame->m_saoLcuParam[0];
    saoLcuParam[1] = pcCU->m_Frame->m_saoLcuParam[1];
    saoLcuParam[2] = pcCU->m_Frame->m_saoLcuParam[2];

    for (Ipp32s iCompIdx=0; iCompIdx<3; iCompIdx++)
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

    if (pSaoParam->m_bSaoFlag[0] || pSaoParam->m_bSaoFlag[1])
    {
        if (rx>0 && iCUAddrInSlice!=0 && allowMergeLeft)
        {
            parseSaoMerge(uiSymbol);
            saoLcuParam[0][iAddr].m_mergeLeftFlag = uiSymbol != 0;
        }
        if (saoLcuParam[0][iAddr].m_mergeLeftFlag==0)
        {
            if ((ry > 0) && (iCUAddrUpInSlice>=0) && allowMergeUp)
            {
                parseSaoMerge(uiSymbol);
                saoLcuParam[0][iAddr].m_mergeUpFlag = uiSymbol != 0;
            }
        }
    }

    for (Ipp32s iCompIdx=0; iCompIdx<3; iCompIdx++)
    {
        if ((iCompIdx == 0  && pSaoParam->m_bSaoFlag[0]) || (iCompIdx > 0  && pSaoParam->m_bSaoFlag[1]) )
        {
            if (rx>0 && iCUAddrInSlice!=0 && allowMergeLeft)
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = saoLcuParam[0][iAddr].m_mergeLeftFlag;
            }
            else
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = 0;
            }

            if (saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag==0)
            {
                if ((ry > 0) && (iCUAddrUpInSlice>=0) && allowMergeUp)
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = saoLcuParam[0][iAddr].m_mergeUpFlag;
                }
                else
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = 0;
                }
                if (!saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag)
                {
                    saoLcuParam[2][iAddr].m_typeIdx = saoLcuParam[1][iAddr].m_typeIdx;
                    parseSaoOffset(&(saoLcuParam[iCompIdx][iAddr]), iCompIdx);
                }
                else
                {
                    copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr], &saoLcuParam[iCompIdx][iAddr-pcCU->m_Frame->m_CodingData->m_WidthInCU]);
                }
            }
            else
            {
                copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr],  &saoLcuParam[iCompIdx][iAddr-1]);
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

void H265SegmentDecoder::DecodeCUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    Ipp32u CurNumParts = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts = CurNumParts >> 2;

    bool boundaryFlag = false;
    Ipp32u LPelX = pCU->m_CUPelX + g_RasterToPelX[ g_ZscanToRaster[AbsPartIdx] ];
    Ipp32u RPelX = LPelX + (m_pCurrentFrame->getCD()->m_MaxCUWidth >> Depth) - 1;
    Ipp32u TPelY = pCU->m_CUPelY + g_RasterToPelY[ g_ZscanToRaster[AbsPartIdx] ];
    Ipp32u BPelY = TPelY + (m_pCurrentFrame->getCD()->m_MaxCUHeight >> Depth) - 1;

    bool bStartInCU = pCU->getSCUAddr() + AbsPartIdx + CurNumParts > m_pSliceHeader->m_sliceSegmentCurStartCUAddr && pCU->getSCUAddr() + AbsPartIdx < m_pSliceHeader->m_sliceSegmentCurStartCUAddr;
    if ((!bStartInCU) && (RPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (BPelY < m_pSeqParamSet->pic_height_in_luma_samples))
    {
        DecodeSplitFlagCABAC(pCU, AbsPartIdx, Depth);
    }
    else
    {
        boundaryFlag = true;
    }

    if (boundaryFlag || ((Depth < pCU->m_DepthArray[AbsPartIdx]) && (Depth < m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)))
    {
        Ipp32u Idx = AbsPartIdx;
        if ((m_pSeqParamSet->MaxCUWidth >> Depth) == m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            m_DecodeDQPFlag = true;
            pCU->setQPSubParts( pCU->getRefQP(AbsPartIdx), AbsPartIdx, Depth ); // set QP to default QP
        }

        for ( Ipp32u PartUnitIdx = 0; PartUnitIdx < 4; PartUnitIdx++ )
        {
            LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[Idx]];
            TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[Idx]];

            bool SubInSliceFlag = pCU->getSCUAddr() + Idx + QNumParts > m_pSliceHeader->m_sliceSegmentCurStartCUAddr;
            if (SubInSliceFlag)

            {
                if ((LPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (TPelY < m_pSeqParamSet->pic_height_in_luma_samples))
                {
                    DecodeCUCABAC(pCU, Idx, Depth + 1, IsLast);
                }
                else
                {
                    pCU->setOutsideCUPart(Idx, Depth + 1);
                }
            }
            if (IsLast)
            {
                break;
            }

            Idx += QNumParts;
        }
        if ((m_pSeqParamSet->MaxCUWidth >> Depth) == m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            if (m_DecodeDQPFlag)
            {
                pCU->setQPSubParts(pCU->getRefQP(AbsPartIdx), AbsPartIdx, Depth); // set QP to default QP
            }
        }
        return;
    }
    if ((m_pSeqParamSet->MaxCUWidth >> Depth) >= m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        m_DecodeDQPFlag = true;
        pCU->setQPSubParts(pCU->getRefQP(AbsPartIdx), AbsPartIdx, Depth ); // set QP to default QP
    }

    if (m_pPicParamSet->transquant_bypass_enable_flag)
        DecodeCUTransquantBypassFlag(pCU, AbsPartIdx, Depth);

    bool skipped = false;
    if (m_pSliceHeader->slice_type != I_SLICE)
        skipped = DecodeSkipFlagCABAC(pCU, AbsPartIdx, Depth);

    if (skipped)
    {
        MVBuffer MvBufferNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
        Ipp8u InterDirNeighbours[MRG_MAX_NUM_CANDS];
        Ipp32s numValidMergeCand = 0;

        for (Ipp32s ui = 0; ui < pCU->m_SliceHeader->m_MaxNumMergeCand; ++ui)
        {
            InterDirNeighbours[ui] = 0;
        }
        Ipp32s MergeIndex = DecodeMergeIndexCABAC();
        getInterMergeCandidates(pCU, AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);

        Ipp32s Size = (m_pSeqParamSet->MaxCUWidth >> Depth);

        m_puinfo[0].PartAddr = AbsPartIdx;
        m_puinfo[0].Width = Size;
        m_puinfo[0].Height = Size;

        H265MVInfo &MVi = m_puinfo[0].interinfo;
        MVi.mvinfo[REF_PIC_LIST_0].RefIdx = MVi.mvinfo[REF_PIC_LIST_1].RefIdx = -1;

        for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
        {
            if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0)
            {
                MVBuffer &mvb = MvBufferNeighbours[2 * MergeIndex + RefListIdx];
                if (mvb.RefIdx >= 0)
                    MVi.mvinfo[RefListIdx] = mvb;
            }
        }

        Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
        Size >>= m_pSeqParamSet->log2_min_transform_block_size;

        UpdatePUInfo(pCU, PartX, PartY, Size, Size, m_puinfo[0]);

        pCU->setCbfSubParts( 0, 0, 0, AbsPartIdx, Depth);
        pCU->setTrStartSubParts(AbsPartIdx, Depth);
        FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
        UpdateNeighborBuffers(pCU, AbsPartIdx, true);
        return;
    }

    Ipp32s PredMode = DecodePredModeCABAC(pCU, AbsPartIdx, Depth);
    DecodePartSizeCABAC(pCU, AbsPartIdx, Depth);

    if (MODE_INTRA == PredMode)
    {
        // Inter mode is marked later when RefIdx is read from bitstream
        Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartWidth = pCU->m_WidthArray[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartHeight = pCU->m_HeightArray[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;

        pCU->m_Frame->m_CodingData->setBlockFlags(COL_TU_INTRA, REF_PIC_LIST_0, PartX, PartY, PartWidth, PartHeight);
    }

    bool isFirstPartMerge = false;

    if (MODE_INTRA == PredMode)
    {
        if (SIZE_2Nx2N == pCU->m_PartSizeArray[AbsPartIdx])
        {
            DecodeIPCMInfoCABAC(pCU, AbsPartIdx, Depth);
            pCU->setTrStartSubParts(AbsPartIdx, Depth);

            if (pCU->m_IPCMFlag[AbsPartIdx])
            {
                FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
                UpdateNeighborBuffers(pCU, AbsPartIdx, false);
                return;
            }
        }
        else
            pCU->setIPCMFlagSubParts(false, AbsPartIdx, Depth);

        DecodeIntraDirLumaAngCABAC(pCU, AbsPartIdx, Depth);
        DecodeIntraDirChromaCABAC(pCU, AbsPartIdx, Depth);
    }
    else
    {
        pCU->setIPCMFlagSubParts(false, AbsPartIdx, Depth);
        isFirstPartMerge = DecodePUWiseCABAC(pCU, AbsPartIdx, Depth);
    }

    Ipp32u CurrWidth = pCU->m_WidthArray[AbsPartIdx];
    Ipp32u CurrHeight = pCU->m_HeightArray[AbsPartIdx];

    // Coefficient decoding
    bool CodeDQPFlag = m_DecodeDQPFlag;
    DecodeCoeff(pCU, AbsPartIdx, Depth, CurrWidth, CurrHeight, CodeDQPFlag, isFirstPartMerge);
    m_DecodeDQPFlag = CodeDQPFlag;

    FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
    UpdateNeighborBuffers(pCU, AbsPartIdx, false);
}

void H265SegmentDecoder::DecodeSplitFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)
    {
        pCU->setDepthSubParts(Depth, AbsPartIdx);
        return;
    }

    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SPLIT_CODING_UNIT_FLAG_HEVC] + getCtxSplitFlag(pCU, AbsPartIdx, Depth));
    pCU->setDepthSubParts( Depth + uVal, AbsPartIdx );

    return;
}

void H265SegmentDecoder::DecodeCUTransquantBypassFlag(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANSQUANT_BYPASS_HEVC]);
    pCU->setCUTransquantBypassSubParts(uVal ? true : false, AbsPartIdx, Depth);
}

bool H265SegmentDecoder::DecodeSkipFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (I_SLICE == m_pSliceHeader->slice_type)
    {
        return false;
    }

    Ipp32u uVal = 0;
    Ipp32u CtxSkip = getCtxSkipFlag(AbsPartIdx);
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SKIP_FLAG_HEVC] + CtxSkip);

    if (uVal)
    {
        pCU->setPredModeSubParts(MODE_INTER, AbsPartIdx, Depth);
        pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
        pCU->setSizeSubParts(m_context->m_sps->MaxCUWidth >> Depth, m_context->m_sps->MaxCUHeight >> Depth, AbsPartIdx, Depth);
        pCU->setIPCMFlagSubParts(false, AbsPartIdx, Depth);
        return true;
    }
    else
        return false;
}

Ipp32u H265SegmentDecoder::DecodeMergeIndexCABAC(void)
{
    Ipp32u NumCand = MRG_MAX_NUM_CANDS;
    Ipp32u UnaryIdx = 0;
    NumCand = m_pSliceHeader->m_MaxNumMergeCand;

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

void H265SegmentDecoder::DecodeMVPIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartAddr, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList, MVBuffer &MVb, Ipp8u InterDir)
{
    Ipp32u MVPIdx = 0;
    AMVPInfo AMVPInfo;

    if (InterDir & (1 << RefList))
    {
        VM_ASSERT(MVb.RefIdx >= 0);
        ReadUnaryMaxSymbolCABAC(MVPIdx, ctxIdxOffsetHEVC[MVP_IDX_HEVC], 1, AMVP_MAX_NUM_CANDS - 1);
    }
    else
    {
        MVb.RefIdx = -1;
        return;
    }

    fillMVPCand(pCU, AbsPartAddr, PartIdx, RefList, MVb.RefIdx, &AMVPInfo);

    if (MVb.RefIdx >= 0)
        MVb.MV = AMVPInfo.MVCandidate[MVPIdx] + MVb.MV;
}

Ipp32s H265SegmentDecoder::DecodePredModeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32s PredMode = MODE_INTER;

    if (I_SLICE == pCU->m_SliceHeader->slice_type)
    {
        PredMode = MODE_INTRA;
    }
    else
    {
        Ipp32u uVal;
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PRED_MODE_HEVC]);
        PredMode += uVal;
    }

    pCU->setPredModeSubParts(EnumPredMode(PredMode), AbsPartIdx, Depth);
    return PredMode;
}

/** parse partition size
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Void
 */
void H265SegmentDecoder::DecodePartSizeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal, uMode = 0;
    EnumPartSize Mode;

    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx])
    {
        uVal = 1;
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PART_SIZE_HEVC]);
            //m_pcTDecBinIf->decodeBin( uiSymbol, m_cCUPartSizeSCModel.get( 0, 0, 0) );
        }
        Mode = uVal ? SIZE_2Nx2N : SIZE_NxN;
        pCU->setSizeSubParts(m_pSeqParamSet->MaxCUWidth >> Depth, m_pSeqParamSet->MaxCUHeight >> Depth, AbsPartIdx, Depth);

        Ipp32u TrLevel = 0;
        Ipp32u WidthInBit  = g_ConvertToBit[pCU->m_WidthArray[AbsPartIdx]] + 2;
        Ipp32u TrSizeInBit = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize] + 2;
        TrLevel            = WidthInBit >= TrSizeInBit ? WidthInBit - TrSizeInBit : 0;
        if (Mode == SIZE_NxN)
        {
            pCU->setTrIdxSubParts(1 + TrLevel, AbsPartIdx, Depth);
        }
        else
        {
            pCU->setTrIdxSubParts(TrLevel, AbsPartIdx, Depth);
        }
    }
    else
    {
        Ipp32u MaxNumBits = 2;
        if (Depth == m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth &&
            !((m_pSeqParamSet->MaxCUWidth >> Depth) == 8 && (m_pSeqParamSet->MaxCUHeight >> Depth) == 8))
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

        if (pCU->m_SliceHeader->m_SeqParamSet->m_AMPAcc[Depth])
        {
            if (Mode == SIZE_2NxN)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? SIZE_2NxnU : SIZE_2NxnD);
                }
            }
            else if (Mode == SIZE_Nx2N)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? SIZE_nLx2N : SIZE_nRx2N);
                }
            }
        }

        pCU->setSizeSubParts(m_pSeqParamSet->MaxCUWidth >> Depth, m_pSeqParamSet->MaxCUHeight >> Depth, AbsPartIdx, Depth);
    }
    pCU->setPartSizeSubParts(Mode, AbsPartIdx, Depth);
}

void H265SegmentDecoder::DecodeIPCMInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (!m_pSeqParamSet->pcm_enabled_flag
        || (pCU->m_WidthArray[AbsPartIdx] > (1 << m_pSeqParamSet->log2_max_pcm_luma_coding_block_size))
        || (pCU->m_WidthArray[AbsPartIdx] < (1 << m_pSeqParamSet->log2_min_pcm_luma_coding_block_size)))
    {
        pCU->setIPCMFlagSubParts(false, AbsPartIdx, Depth);
        return;
    }

    Ipp32u uVal;

    uVal = m_pBitStream->DecodeTerminatingBit_CABAC();
    pCU->setIPCMFlagSubParts(uVal != 0, AbsPartIdx, Depth);

    if (uVal)
    {
        DecodePCMAlignBits();

        pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
        pCU->setLumaIntraDirSubParts(DC_IDX, AbsPartIdx, Depth);
        pCU->setSizeSubParts(m_pSeqParamSet->MaxCUWidth >> Depth, m_pSeqParamSet->MaxCUHeight >> Depth, AbsPartIdx, Depth);
        pCU->setTrIdxSubParts(0, AbsPartIdx, Depth);

        Ipp32u MinCoeffSize = pCU->m_Frame->getMinCUWidth() * pCU->m_Frame->getMinCUHeight();
        Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
        Ipp32u ChromaOffset = LumaOffset >> 2;

        Ipp32u Width;
        Ipp32u Height;
        Ipp32u SampleBits;
        Ipp32u X, Y;

        // LUMA
        H265PlanePtrYCommon PCMSampleY = pCU->m_IPCMSampleY + LumaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx];
        Height = pCU->m_HeightArray[AbsPartIdx];
        SampleBits = pCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthLuma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleY[X] = (H265PlaneYCommon)Sample;
            }
            PCMSampleY += Width;
        }

        // CHROMA U
        H265PlanePtrUVCommon PCMSampleUV = pCU->m_IPCMSampleCb + ChromaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx] / 2;
        Height = pCU->m_HeightArray[AbsPartIdx] / 2;
        SampleBits = m_pSeqParamSet->getPCMBitDepthChroma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
            }
            PCMSampleUV += Width;
        }

        // CHROMA V
        PCMSampleUV = pCU->m_IPCMSampleCr + ChromaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx] / 2;
        Height = pCU->m_HeightArray[AbsPartIdx] / 2;
        SampleBits = m_pSeqParamSet->getPCMBitDepthChroma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
            }
            PCMSampleUV += Width;
        }

        m_pBitStream->ResetBac_CABAC();
    }
}

// Decode PCM alignment zero bits.
void H265SegmentDecoder::DecodePCMAlignBits()
{
    Ipp32s iVal = m_pBitStream->getNumBitsUntilByteAligned();

    if (iVal)
        m_pBitStream->GetBits(iVal);
}

void H265SegmentDecoder::DecodeIntraDirLumaAngCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize mode = pCU->getPartitionSize(AbsPartIdx);
    Ipp32u PartNum = mode == SIZE_NxN ? 4 : 1;
    Ipp32u PartOffset = (pCU->m_Frame->m_CodingData->m_NumPartitions >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 2;
    Ipp32u mpmPred[4];
    Ipp32u uVal;
    Ipp32s IPredMode;

    if (mode == SIZE_NxN)
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

        getIntraDirLumaPredictor(pCU, AbsPartIdx + PartOffset * j, Preds);

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
        pCU->setLumaIntraDirSubParts(IPredMode, AbsPartIdx + PartOffset * j, Depth);
    }
}

void H265SegmentDecoder::DecodeIntraDirChromaCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_CHROMA_PRED_MODE_HEVC]);

    //switch codeword
    if (uVal == 0)
    {
        uVal = DM_CHROMA_IDX;
    }
    else
    {
        Ipp32u IPredMode;

        IPredMode = m_pBitStream->DecodeBypassBins_CABAC(2);
        Ipp32u AllowedChromaDir[NUM_CHROMA_MODE];
        pCU->getAllowedChromaDir(AbsPartIdx, AllowedChromaDir);
        uVal = AllowedChromaDir[IPredMode];
    }
    pCU->setChromIntraDirSubParts(uVal, AbsPartIdx, Depth);
    return;
}

bool H265SegmentDecoder::DecodePUWiseCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize PartSize = (EnumPartSize) pCU->m_PartSizeArray[AbsPartIdx];
    Ipp32u NumPU = (PartSize == SIZE_2Nx2N ? 1 : (PartSize == SIZE_NxN ? 4 : 2));
    Ipp32u PUOffset = (g_PUOffset[Ipp32u(PartSize)] << ((m_pSeqParamSet->MaxCUDepth - Depth) << 1)) >> 4;

    MVBuffer MvBufferNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
    Ipp8u InterDirNeighbours[MRG_MAX_NUM_CANDS];

    for (Ipp32s ui = 0; ui < pCU->m_SliceHeader->m_MaxNumMergeCand; ui++ )
    {
        InterDirNeighbours[ui] = 0;
    }
    Ipp32s numValidMergeCand = 0;
    bool isFirstMerge = false;

    for (Ipp32u PartIdx = 0, SubPartIdx = AbsPartIdx; PartIdx < NumPU; PartIdx++, SubPartIdx += PUOffset)
    {
        Ipp8u InterDir = 0;
        bool bMergeFlag = DecodeMergeFlagCABAC();
        if (0 == PartIdx)
            isFirstMerge = bMergeFlag;

        Ipp32s PartWidth, PartHeight;
        pCU->getPartSize(AbsPartIdx, PartIdx, PartWidth, PartHeight);

        m_puinfo[PartIdx].PartAddr = SubPartIdx;
        m_puinfo[PartIdx].Width = PartWidth;
        m_puinfo[PartIdx].Height = PartHeight;

        Ipp32s LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[SubPartIdx]];
        Ipp32s TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[SubPartIdx]];
        Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
        PartWidth >>= m_pSeqParamSet->log2_min_transform_block_size;
        PartHeight >>= m_pSeqParamSet->log2_min_transform_block_size;

        H265MVInfo &MVi = m_puinfo[PartIdx].interinfo;
        MVi.mvinfo[REF_PIC_LIST_0].RefIdx = MVi.mvinfo[REF_PIC_LIST_1].RefIdx = -1;

        if (bMergeFlag)
        {
            Ipp32u MergeIndex = DecodeMergeIndexCABAC();

            if ((m_pPicParamSet->log2_parallel_merge_level - 2) > 0 && PartSize != SIZE_2Nx2N && pCU->m_WidthArray[AbsPartIdx] <= 8)
            {
                pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
                getInterMergeCandidates(pCU, AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, -1);
                pCU->setPartSizeSubParts(PartSize, AbsPartIdx, Depth);
            }
            else
            {
                getInterMergeCandidates(pCU, SubPartIdx, PartIdx, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);
            }
            InterDir = InterDirNeighbours[MergeIndex];

            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (pCU->m_SliceHeader->m_numRefIdx[RefListIdx] > 0)
                {
                    MVBuffer &mvb = MvBufferNeighbours[2 * MergeIndex + RefListIdx];
                    if (mvb.RefIdx >= 0)
                        MVi.mvinfo[RefListIdx] = mvb;
                }
            }
        }
        else
        {
            InterDir = DecodeInterDirPUCABAC(pCU, SubPartIdx);
            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (pCU->m_SliceHeader->m_numRefIdx[RefListIdx] > 0)
                {
                    MVi.mvinfo[RefListIdx].RefIdx = DecodeRefFrmIdxPUCABAC(pCU, SubPartIdx, Depth, PartIdx, EnumRefPicList(RefListIdx), InterDir);
                    DecodeMVdPUCABAC(EnumRefPicList(RefListIdx), MVi.mvinfo[RefListIdx].MV, InterDir);
                    DecodeMVPIdxPUCABAC(pCU, SubPartIdx, Depth, PartIdx, EnumRefPicList(RefListIdx), MVi.mvinfo[RefListIdx], InterDir);
                }
            }
        }

        if ((InterDir == 3) && pCU->isBipredRestriction(AbsPartIdx, PartIdx))
            MVi.mvinfo[REF_PIC_LIST_1].RefIdx = -1;

        UpdatePUInfo(pCU, PartX, PartY, PartWidth, PartHeight, m_puinfo[PartIdx]);
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
Ipp8u H265SegmentDecoder::DecodeInterDirPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    Ipp8u InterDir;

    if (P_SLICE == m_pSliceHeader->slice_type)
    {
        InterDir = 1;
    }
    else
    {
        Ipp32u uVal;
        Ipp32u Ctx = pCU->getCtxInterDir(AbsPartIdx);

        uVal = 0;
        if (pCU->m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N || pCU->m_HeightArray[AbsPartIdx] != 8)
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
        InterDir = uVal;
    }

    return InterDir;
}

RefIndexType H265SegmentDecoder::DecodeRefFrmIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList, Ipp8u InterDir)
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
        RefFrmIdx = uVal;
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

        if (m_pSliceHeader->m_MvdL1Zero && RefList == REF_PIC_LIST_1 && InterDir == 3)
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
void H265SegmentDecoder::DecodeCoeff(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u Width, Ipp32u Height, bool& CodeDQP, bool isFirstPartMerge)
{
    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx])
    {
    }
    else
    {
        Ipp32u QtRootCbf = 1;
        if (!(pCU->m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N && isFirstPartMerge))
        {
            ParseQtRootCbfCABAC(QtRootCbf);
        }
        if (!QtRootCbf)
        {
            pCU->setCbfSubParts( 0, 0, 0, AbsPartIdx, Depth);
            pCU->setTrIdxSubParts( 0 , AbsPartIdx, Depth);
            pCU->setTrStartSubParts(AbsPartIdx, Depth);
            return;
        }
    }

    Ipp32u minCoeffSize = pCU->m_Frame->getMinCUWidth() * pCU->m_Frame->getMinCUHeight();
    Ipp32u lumaOffset = minCoeffSize * AbsPartIdx;
    DecodeTransform(pCU, lumaOffset, AbsPartIdx, Depth, Width, Height, 0, CodeDQP);
}

void H265SegmentDecoder::DecodeTransform(H265CodingUnit* pCU, Ipp32u offsetLuma,
                                         Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  width, Ipp32u height,
                                         Ipp32u TrIdx, bool& CodeDQP)
{
    Ipp32u offsetChroma = offsetLuma >> 2;
    Ipp32u Subdiv;
    const Ipp32u Log2TrafoSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth] + 2 - Depth;

    if (0 == TrIdx)
        m_bakAbsPartIdxCU = AbsPartIdx;

    if (Log2TrafoSize == 2)
    {
        Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
        if ((AbsPartIdx % partNum) == 0)
        {
            m_BakAbsPartIdx   = AbsPartIdx;
            m_BakChromaOffset = offsetChroma;
        }
    }

    if (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA && pCU->m_PartSizeArray[AbsPartIdx] == SIZE_NxN && Depth == pCU->m_DepthArray[AbsPartIdx])
    {
        Subdiv = 1;
    }
    else if ((pCU->m_SliceHeader->m_SeqParamSet->max_transform_hierarchy_depth_inter == 1) && (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTER) && (pCU->m_PartSizeArray[AbsPartIdx] != SIZE_2Nx2N) && (Depth == pCU->m_DepthArray[AbsPartIdx]))
    {
        Subdiv = (Log2TrafoSize > pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
    }

    else if (Log2TrafoSize > pCU->m_SliceHeader->m_SeqParamSet->log2_max_transform_block_size)
    {
        Subdiv = 1;
    }
    else if (Log2TrafoSize == pCU->m_SliceHeader->m_SeqParamSet->log2_min_transform_block_size)
    {
        Subdiv = 0;
    }
    else if (Log2TrafoSize == pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx))
    {
        Subdiv = 0;
    }
    else
    {
        VM_ASSERT(Log2TrafoSize > pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
        ParseTransformSubdivFlagCABAC(Subdiv, 5 - Log2TrafoSize);
    }

    const Ipp32u TrDepth = Depth - pCU->m_DepthArray[AbsPartIdx];

    {
        const bool FirstCbfOfCUFlag = TrDepth == 0;
        if (FirstCbfOfCUFlag)
        {
            pCU->setCbfSubParts( 0, TEXT_CHROMA_U, AbsPartIdx, Depth);
            pCU->setCbfSubParts( 0, TEXT_CHROMA_V, AbsPartIdx, Depth);
        }

        if (FirstCbfOfCUFlag || Log2TrafoSize > 2)
        {
            if (FirstCbfOfCUFlag || pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth - 1))
                ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_CHROMA_U, TrDepth, Depth);
            if (FirstCbfOfCUFlag || pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth - 1))
                ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_CHROMA_V, TrDepth, Depth);
        }
        else
        {
            pCU->setCbfSubParts(pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth - 1) << TrDepth, TEXT_CHROMA_U, AbsPartIdx, Depth);
            pCU->setCbfSubParts(pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth - 1) << TrDepth, TEXT_CHROMA_V, AbsPartIdx, Depth);
        }
    }

    if (Subdiv)
    {
        Ipp32u size;
        width  >>= 1;
        height >>= 1;
        size = width * height;
        TrIdx++;
        ++Depth;
        const Ipp32u QPartNum = pCU->m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
        const Ipp32u StartAbsPartIdx = AbsPartIdx;
        Ipp32u YCbf = 0;
        Ipp32u UCbf = 0;
        Ipp32u VCbf = 0;

        for (Ipp32s i = 0; i < 4; i++)
        {
            DecodeTransform(pCU, offsetLuma, AbsPartIdx, Depth, width, height, TrIdx, CodeDQP);

            YCbf |= pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrDepth + 1);
            UCbf |= pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth + 1);
            VCbf |= pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth + 1);
            AbsPartIdx += QPartNum;
            offsetLuma += size;
        }

        for (Ipp32u ui = 0; ui < 4 * QPartNum; ++ui)
        {
            pCU->getCbf(TEXT_LUMA)[StartAbsPartIdx + ui] |= YCbf << TrDepth;
            pCU->getCbf(TEXT_CHROMA_U)[StartAbsPartIdx + ui] |= UCbf << TrDepth;
            pCU->getCbf(TEXT_CHROMA_V)[StartAbsPartIdx + ui] |= VCbf << TrDepth;
        }
    }
    else
    {
        assert(Depth >= pCU->m_DepthArray[AbsPartIdx]);
        pCU->setTrIdxSubParts(TrDepth, AbsPartIdx, Depth);
        pCU->setTrStartSubParts(AbsPartIdx, Depth);

        pCU->setCbfSubParts(0, TEXT_LUMA, AbsPartIdx, Depth);
        if (pCU->m_PredModeArray[AbsPartIdx] != MODE_INTRA && Depth == pCU->m_DepthArray[AbsPartIdx] && !pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, 0) && !pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, 0))
        {
            pCU->setCbfSubParts( 1 << TrDepth, TEXT_LUMA, AbsPartIdx, Depth);
        }
        else
        {
            ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_LUMA, TrDepth, Depth);
        }
        // transform_unit begin
        Ipp32u cbfY = pCU->getCbf(AbsPartIdx, TEXT_LUMA    , TrIdx);
        Ipp32u cbfU = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrIdx);
        Ipp32u cbfV = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrIdx);
        if (Log2TrafoSize == 2)
        {
            Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
            if ((AbsPartIdx % partNum) == (partNum - 1))
            {
                cbfU = pCU->getCbf(m_BakAbsPartIdx, TEXT_CHROMA_U, TrIdx);
                cbfV = pCU->getCbf(m_BakAbsPartIdx, TEXT_CHROMA_V, TrIdx);
            }
        }
        if (cbfY || cbfU || cbfV)
        {
            // dQP: only for LCU
            if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
            {
                if (CodeDQP)
                {
                    DecodeQP(pCU, m_bakAbsPartIdxCU);
                    CodeDQP = false;
                }
            }
        }
        if (cbfY)
        {
            Ipp32s trWidth = width;
            Ipp32s trHeight = height;
            ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffY + offsetLuma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_LUMA);
        }
        if (Log2TrafoSize > 2)
        {
            Ipp32s trWidth = width >> 1;
            Ipp32s trHeight = height >> 1;

            if (cbfU)
            {
                ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCb + offsetChroma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_U);
            }
            if (cbfV)
            {
                ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCr + offsetChroma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_V);
            }
        }
        else
        {
            Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
            if ((AbsPartIdx % partNum) == (partNum - 1))
            {
                Ipp32s trWidth = width;
                Ipp32s trHeight = height;

                if (cbfU)
                {
                    ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCb + m_BakChromaOffset), m_BakAbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_U);
                }
                if (cbfV)
                {
                    ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCr + m_BakChromaOffset), m_BakAbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_V);
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

void H265SegmentDecoder::ParseQtCbfCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, EnumTextType Type, Ipp32u TrDepth, Ipp32u Depth)
{
    Ipp32u uVal;
    const Ipp32u Ctx = pCU->getCtxQtCbf(Type, TrDepth);

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_CBF_HEVC] + Ctx + NUM_QT_CBF_CTX * (Type ? TEXT_CHROMA : Type));
    //m_pcTDecBinIf->decodeBin( uiSymbol , m_cCUQtCbfSCModel.get( 0, eType ? eType - 1: eType, uiCtx ) );

    pCU->setCbfSubParts(uVal << TrDepth, Type, AbsPartIdx, Depth);
}

void H265SegmentDecoder::ParseQtRootCbfCABAC(Ipp32u& QtRootCbf)
{
    Ipp32u uVal;
    const Ipp32u Ctx = 0;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_ROOT_CBF_HEVC] + Ctx);
    QtRootCbf = uVal;
}

void H265SegmentDecoder::DecodeQP(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
    {
        ParseDeltaQPCABAC(pCU, AbsPartIdx, pCU->m_DepthArray[AbsPartIdx]);
    }
}

void H265SegmentDecoder::ParseDeltaQPCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
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
        Ipp32s QPBdOffsetY = pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetY();

        Sign = m_pBitStream->DecodeSingleBinEP_CABAC();
        iDQp = uiDQp;
        if (Sign)
            iDQp = -iDQp;

        qp = (((Ipp32s)pCU->getRefQP(AbsPartIdx) + iDQp + 52 + 2 * QPBdOffsetY) % (52 + QPBdOffsetY)) - QPBdOffsetY;
    }
    else
    {
        iDQp=0;
        qp = pCU->getRefQP(AbsPartIdx);
    }

    pCU->setQPSubParts(qp, AbsPartIdx, Depth);
    pCU->m_CodedQP = (Ipp8u)qp;
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

void H265SegmentDecoder::FinishDecodeCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
    {
        pCU->setQPSubParts(m_DecodeDQPFlag ? pCU->getRefQP(AbsPartIdx) : pCU->m_CodedQP, AbsPartIdx, Depth); // set QP to default QP
    }

    IsLast = DecodeSliceEnd(pCU, AbsPartIdx, Depth);
}

/**decode end-of-slice flag
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Bool
 */
bool H265SegmentDecoder::DecodeSliceEnd(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u IsLast;
    H265DecoderFrame* pFrame = pCU->m_Frame;
    H265SliceHeader* pSliceHeader = pCU->m_SliceHeader;
    Ipp32u CurNumParts = pFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u Width = pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples;
    Ipp32u Height = pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples;
    Ipp32u GranularityWidth = m_pSeqParamSet->MaxCUWidth;
    Ipp32u PosX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u PosY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];

    if (((PosX + pCU->m_WidthArray[AbsPartIdx]) % GranularityWidth == 0 || (PosX + pCU->m_WidthArray[AbsPartIdx] == Width))
        && ((PosY + pCU->m_HeightArray[AbsPartIdx]) % GranularityWidth == 0|| (PosY + pCU->m_HeightArray[AbsPartIdx] == Height)))
    {
        IsLast = m_pBitStream->DecodeTerminatingBit_CABAC();
        //m_pcEntropyDecoder->decodeTerminatingBit( uiIsLast );
    }
    else
    {
        IsLast=0;
    }
    if (IsLast)
    {
        if (pSliceHeader->m_DependentSliceSegmentFlag)
        {
            pSliceHeader->m_sliceSegmentCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
        }
        else
        {
            pSliceHeader->SliceCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
            pSliceHeader->m_sliceSegmentCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
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

H265_FORCEINLINE Ipp32u getSigCtxInc(Ipp32s patternSigCtx,
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

void H265SegmentDecoder::ParseCoeffNxNCABAC(H265CodingUnit* pCU, H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type)
{
    assert(sizeof(Depth) != 0); //wtf unreferenced parameter
    if (Width > pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize)
    {
        Width = pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize;
        Height = pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize;
    }

    if (pCU->m_SliceHeader->m_PicParamSet->getUseTransformSkip())
        ParseTransformSkipFlags(pCU, AbsPartIdx, Width, Height, Depth, Type);
    
    bool IsLuma = (Type==TEXT_LUMA);

    //----- parse significance map -----
    const Ipp32u Log2BlockSize = g_ConvertToBit[Width] + 2;

    memset(pCoef, 0, sizeof(H265CoeffsCommon) << (Log2BlockSize << 1));

    Ipp32u ScanIdx = pCU->getCoefScanIdx(AbsPartIdx, Log2BlockSize, IsLuma, pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA);

    //===== decode last significant =====
    Ipp32u BlkPosLast = ParseLastSignificantXYCABAC(Log2BlockSize-2, IsLuma, ScanIdx);

    pCoef[BlkPosLast] = 1;

    //===== decode significance flags =====
    const Ipp16u* scan = g_SigLastScan[ScanIdx][Log2BlockSize - 2];
    Ipp32u ScanPosLast = g_SigLastScan_inv[ScanIdx][Log2BlockSize - 2][BlkPosLast];

    Ipp32u baseCoeffGroupCtxIdx = ctxIdxOffsetHEVC[SIG_COEFF_GROUP_FLAG_HEVC] + (IsLuma ? 0 : NUM_SIG_CG_FLAG_CTX);
    Ipp32u baseCtxIdx = ctxIdxOffsetHEVC[SIG_FLAG_HEVC] + ( IsLuma ? 0 : NUM_SIG_FLAG_CTX_LUMA);

    const Ipp32s LastScanSet = ScanPosLast >> LOG2_SCAN_SET_SIZE;
    Ipp32u c1 = 1;
    Ipp32u GoRiceParam       = 0;

    bool beValid = pCU->m_CUTransquantBypass[AbsPartIdx] ? false : pCU->m_SliceHeader->m_PicParamSet->sign_data_hiding_flag;
    Ipp32u absSum;

    Ipp64u SCGroupFlagMask     = 0;
    Ipp16u SCGroupFlagRightMask = 0;
    Ipp16u SCGroupFlagLowerMask  = 0;

    const Ipp16u *scanCG;

    scanCG = g_SigLastScan[ScanIdx][Log2BlockSize > 3 ? Log2BlockSize - 2 - 2 : 0];

    if (Log2BlockSize == 3)
        scanCG = g_sigLastScan8x8[ScanIdx];
    else if (Log2BlockSize == 5)
        scanCG = g_sigLastScanCG32x32;
    
    Ipp32s ScanPosSig = (Ipp32s) ScanPosLast;
    for (Ipp32s SubSet = LastScanSet; SubSet >= 0; SubSet--)
    {
        Ipp32s SubPos     = SubSet << LOG2_SCAN_SET_SIZE;
        GoRiceParam       = 0;
        Ipp32s numNonZero = 0;
        Ipp32s lastNZPosInCG = -1;
        Ipp32s firstNZPosInCG = SCAN_SET_SIZE;

        Ipp32s pos[SCAN_SET_SIZE];
        if (ScanPosSig == (Ipp32s) ScanPosLast)
        {
            lastNZPosInCG  = ScanPosSig;
            firstNZPosInCG = ScanPosSig;
            ScanPosSig--;
            pos[numNonZero] = BlkPosLast;
            numNonZero = 1;
        }

        // decode significant_coeffgroup_flag
        Ipp32s CGBlkPos = scanCG[SubSet];
        Ipp32s CGPosY   = CGBlkPos >> (Log2BlockSize - 2);
        Ipp32s CGPosX   = CGBlkPos - (CGPosY << (Log2BlockSize - 2));
        Ipp32u SigCoeffGroup;

        if (SubSet == LastScanSet || SubSet == 0)
        {
            SCGroupFlagMask |= (1 << CGBlkPos);
            SigCoeffGroup = 1;
        }
        else
        {
            Ipp32u CtxSig = ((SCGroupFlagRightMask >> CGPosY) & 0x1) || ((SCGroupFlagLowerMask >> CGPosX) & 0x1);

            SigCoeffGroup = m_pBitStream->DecodeSingleBin_CABAC(baseCoeffGroupCtxIdx + CtxSig);
            SCGroupFlagMask &= ~(1 << CGBlkPos);
            SCGroupFlagMask |= (SigCoeffGroup << CGBlkPos);
        }

        // decode significant_coeff_flag
        Ipp32u patternSigCtx = ((SCGroupFlagRightMask >> CGPosY) & 0x1) | (((SCGroupFlagLowerMask >> CGPosX) & 0x1)<<1);

        SCGroupFlagRightMask &= ~(1 << CGPosY);
        SCGroupFlagRightMask |= (SigCoeffGroup << CGPosY);
        SCGroupFlagLowerMask &= ~(1 << CGPosX);
        SCGroupFlagLowerMask |= (SigCoeffGroup << CGPosX);

        Ipp32u BlkPos, PosY, PosX, Sig, CtxSig;

        for (; ScanPosSig >= SubPos; ScanPosSig--)
        {
            BlkPos  = scan[ScanPosSig];
            PosY    = BlkPos >> Log2BlockSize;
            PosX    = BlkPos - (PosY << Log2BlockSize);
            Sig     = 0;

            if (SCGroupFlagMask&(1 << CGBlkPos))
            {
                if (ScanPosSig > SubPos || SubSet == 0 || numNonZero)
                {
                    CtxSig = getSigCtxInc(patternSigCtx, ScanIdx, PosX, PosY, Log2BlockSize, IsLuma);
                    Sig = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx + CtxSig);
                }
                else
                {
                    Sig = 1;
                }
            }

            pCoef[BlkPos] = (Ipp16s) Sig;
            if (Sig)
            {
                pos[numNonZero] = BlkPos;
                numNonZero++;
                if (lastNZPosInCG == -1)
                    lastNZPosInCG = ScanPosSig;
                firstNZPosInCG = ScanPosSig;
            }
        }

        if (numNonZero)
        {
            bool signHidden = (lastNZPosInCG - firstNZPosInCG >= SBH_THRESHOLD);
            absSum = 0;
            Ipp32u CtxSet = (SubSet > 0 && IsLuma) ? 2 : 0;
            Ipp32u Bin;

            if (c1 == 0)
                CtxSet++;

            c1 = 1;

            Ipp32u baseCtxIdx = ctxIdxOffsetHEVC[ONE_FLAG_HEVC] + (CtxSet<<2) + (IsLuma ? 0 : NUM_ONE_FLAG_CTX_LUMA);

            Ipp32s absCoeff[SCAN_SET_SIZE];
            for (Ipp32s i = 0; i < numNonZero; i++)
                absCoeff[i] = 1;

            Ipp32s numC1Flag = IPP_MIN(numNonZero, C1FLAG_NUMBER);
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
                baseCtxIdx = ctxIdxOffsetHEVC[ABS_FLAG_HEVC] + CtxSet + (IsLuma ? 0 : NUM_ABS_FLAG_CTX_LUMA);
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
            if (c1 == 0 || numNonZero > C1FLAG_NUMBER)
            {
                for (Ipp32s idx = 0; idx < numNonZero; idx++)
                {
                    Ipp32s baseLevel  = (idx < C1FLAG_NUMBER)? (2 + FirstCoeff2) : 1;

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

            for (Ipp32s idx = 0; idx < numNonZero; idx++)
            {
                Ipp32s blkPos = pos[idx];

                // Signs applied later.
                pCoef[blkPos] = (H265CoeffsCommon)absCoeff[idx];
                absSum += absCoeff[idx];

                if (idx == numNonZero - 1 && signHidden && beValid)
                {
                    // Infer sign of 1st element.
                    if (absSum & 0x1)
                        pCoef[blkPos] = -pCoef[blkPos];
                }
                else
                {
                    Ipp32s sign = static_cast<Ipp32s>(coeffSigns) >> 31;
                    pCoef[blkPos] = (H265CoeffsCommon)((pCoef[blkPos] ^ sign) - sign);
                    coeffSigns <<= 1;
                }
                pCoef[blkPos] = Clip3(H265_COEFF_MIN, H265_COEFF_MAX, pCoef[blkPos]);

            }
        }
    }

    return;
}

void H265SegmentDecoder::ParseTransformSkipFlags(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type)
{
    if (pCU->m_CUTransquantBypass[AbsPartIdx])
        return;

    if(Width != 4 || Height != 4)
        return;

    Ipp32u useTransformSkip;
    Ipp32u CtxIdx = ctxIdxOffsetHEVC[TRANSFORM_SKIP_HEVC] + (Type ? TEXT_CHROMA : TEXT_LUMA) * NUM_TRANSFORMSKIP_FLAG_CTX;
    useTransformSkip = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    if (Type != TEXT_LUMA)
    {
        const Ipp32u Log2TrafoSize = g_ConvertToBit[pCU->m_Frame->m_CodingData->m_MaxCUWidth] + 2 - Depth;
        if(Log2TrafoSize == 2)
        {
            Depth --;
        }
    }

    pCU->setTransformSkipSubParts(useTransformSkip, Type, AbsPartIdx, Depth);
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

Ipp32u H265SegmentDecoder::ParseLastSignificantXYCABAC(Ipp32u L2Width, bool IsLuma, Ipp32u ScanIdx)
{
    Ipp32u mGIdx = g_GroupIdx[(1<<(L2Width+2)) - 1];

    Ipp32u blkSizeOffset = IsLuma ? (L2Width*3 + ((L2Width+1)>>2)) : NUM_CTX_LAST_FLAG_XY;
    Ipp32u shift = IsLuma ? ((L2Width+3) >> 2) : L2Width;
    Ipp32u CtxIdxX = ctxIdxOffsetHEVC[LAST_X_HEVC] + blkSizeOffset;
    Ipp32u CtxIdxY = ctxIdxOffsetHEVC[LAST_Y_HEVC] + blkSizeOffset;
    Ipp32u PosLastX, PosLastY;

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
        return PosLastY + (PosLastX << (L2Width+2));
    } else {
        return PosLastX + (PosLastY << (L2Width+2));
    }
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

    if (prefix < COEF_REMAIN_BIN_REDUCTION)
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(Param);
        Symbol = (prefix << Param) + CodeWord;
    }
    else
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(prefix - COEF_REMAIN_BIN_REDUCTION + Param);
        Symbol = (((1 << (prefix - COEF_REMAIN_BIN_REDUCTION)) + COEF_REMAIN_BIN_REDUCTION - 1) << Param) + CodeWord;
    }
}

void H265SegmentDecoder::ReconstructCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{

    H265DecoderFrame* frame = pCU->m_Frame;

    bool BoundaryFlag = false;
    Ipp32u LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u RPelX = LPelX + (m_pSeqParamSet->MaxCUWidth >> Depth) - 1;
    Ipp32u TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u BPelY = TPelY + (m_pSeqParamSet->MaxCUHeight >> Depth) - 1;

    Ipp32u CurNumParts = frame->getCD()->getNumPartInCU() >> (Depth << 1);
    H265SliceHeader* pSliceHeader = pCU->m_SliceHeader;
    bool bStartInCU = pCU->getSCUAddr() + AbsPartIdx + CurNumParts > pSliceHeader->m_sliceSegmentCurStartCUAddr && pCU->getSCUAddr() + AbsPartIdx < pSliceHeader->m_sliceSegmentCurStartCUAddr;
    if (bStartInCU || (RPelX >= pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples) || (BPelY >= pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples))
    {
        BoundaryFlag = true;
    }

    if (((Depth < pCU->m_DepthArray[AbsPartIdx]) && (Depth < m_pSeqParamSet->MaxCUDepth - m_pSeqParamSet->AddCUDepth)) || BoundaryFlag)
    {
        Ipp32u NextDepth = Depth + 1;
        Ipp32u QNumParts = pCU->m_NumPartition >> (NextDepth << 1);
        Ipp32u Idx = AbsPartIdx;
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, Idx += QNumParts)
        {
            if (BoundaryFlag)
            {
                LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[Idx]];
                TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[Idx]];
                bool binSlice = (pCU->getSCUAddr() + Idx + QNumParts > pSliceHeader->m_sliceSegmentCurStartCUAddr) && (pCU->getSCUAddr() + Idx < pSliceHeader->m_sliceSegmentCurEndCUAddr);
                bool insideFrame = binSlice && (LPelX < pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples) && (TPelY < pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples);
                if (!insideFrame)
                {
                    continue;
                }

            }

            ReconstructCU(pCU, Idx, NextDepth);
        }
        return;
    }

    switch (pCU->m_PredModeArray[AbsPartIdx])
    {
        case MODE_INTER:
            ReconInter(pCU, AbsPartIdx, Depth);
            break;
        case MODE_INTRA:
            ReconIntraQT(pCU, AbsPartIdx, Depth);
            break;
        default:
            VM_ASSERT(0);
        break;
    }

    if (pCU->isLosslessCoded(AbsPartIdx) && !pCU->m_IPCMFlag[AbsPartIdx])
    {
        // Saving reconstruct contents in PCM buffer is necessary for later PCM restoration when SAO is enabled
        FillPCMBuffer(pCU, AbsPartIdx, Depth);
    }
}

void H265SegmentDecoder::ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u InitTrDepth = (pCU->m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N ? 0 : 1);
    Ipp32u NumPart = pCU->getNumPartInter(AbsPartIdx);
    Ipp32u NumQParts = pCU->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
    NumQParts >>= 2;

    if (pCU->m_IPCMFlag[AbsPartIdx])
    {
        ReconPCM(pCU, AbsPartIdx, Depth);
        return;
    }

    for (Ipp32u PU = 0; PU < NumPart; PU++)
    {
        IntraLumaRecQT(pCU, InitTrDepth, AbsPartIdx + PU * NumQParts);
    }

    Ipp32u ChromaPredMode = pCU->m_ChromaIntraDir[AbsPartIdx];
    if (ChromaPredMode == DM_CHROMA_IDX)
    {
        ChromaPredMode = pCU->m_LumaIntraDir[AbsPartIdx];
    }

    for (Ipp32u PU = 0; PU < NumPart; PU++)
    {
        IntraChromaRecQT(pCU, InitTrDepth, AbsPartIdx + PU * NumQParts, ChromaPredMode);
    }
}

void H265SegmentDecoder::ReconInter(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // inter prediction
    m_Prediction->MotionCompensation(pCU, m_ppcYUVReco, AbsPartIdx, Depth, m_puinfo);

    // clip for only non-zero cbp case
    if ((pCU->getCbf(AbsPartIdx, TEXT_LUMA)) || (pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U)) || (pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V )))
    {
        // inter recon
        DecodeInterTexture(pCU, AbsPartIdx);
    }
}

void H265SegmentDecoder::DecodeInterTexture(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    Ipp32u Size = pCU->m_WidthArray[AbsPartIdx];

    m_TrQuant->InvRecurTransformNxN(pCU, AbsPartIdx, Size, 0);
}

void H265SegmentDecoder::ReconPCM(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    // Luma
    Ipp32u Size  = (m_pSeqParamSet->MaxCUWidth >> Depth);
    Ipp32u MinCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUHeight();
    Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265PlanePtrYCommon pPcmY = pCU->m_IPCMSampleY + LumaOffset;
    H265PlanePtrYCommon pPicReco = m_pCurrentFrame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u PitchLuma = m_pCurrentFrame->pitch_luma();
    Ipp32u PcmLeftShiftBit = g_bitDepthY - m_pSeqParamSet->pcm_bit_depth_luma;

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
    H265PlanePtrUVCommon pPcmCb = pCU->m_IPCMSampleCb + ChromaOffset;
    H265PlanePtrUVCommon pPcmCr = pCU->m_IPCMSampleCr + ChromaOffset;
    pPicReco = m_pCurrentFrame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u PitchChroma = m_pCurrentFrame->pitch_chroma();
    PcmLeftShiftBit = g_bitDepthC - m_pSeqParamSet->pcm_bit_depth_chroma;

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

void H265SegmentDecoder::FillPCMBuffer(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    // Luma
    Ipp32u Width  = (m_pSeqParamSet->MaxCUWidth >> Depth);
    Ipp32u Height = (m_pSeqParamSet->MaxCUHeight >> Depth);
    Ipp32u MinCoeffSize = m_pCurrentFrame->getMinCUWidth() * m_pCurrentFrame->getMinCUHeight();
    Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265PlanePtrYCommon pPcmY = pCU->m_IPCMSampleY + LumaOffset;
    H265PlanePtrYCommon pRecoY = m_pCurrentFrame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u stride = m_pCurrentFrame->pitch_luma();

    for(Ipp32u y = 0; y < Height; y++)
    {
        for(Ipp32u x = 0; x < Width; x++)
        {
            pPcmY[x] = pRecoY[x];
        }
        pPcmY += Width;
        pRecoY += stride;
    }

    // Cb and Cr
    Width >>= 1;
    Height >>= 1;

    H265PlanePtrUVCommon pPcmCb = pCU->m_IPCMSampleCb + ChromaOffset;
    H265PlanePtrUVCommon pPcmCr = pCU->m_IPCMSampleCr + ChromaOffset;
    H265PlanePtrUVCommon pRecoCbCr = m_pCurrentFrame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
    stride = m_pCurrentFrame->pitch_chroma();

    for(Ipp32u y = 0; y < Height; y++)
    {
        for(Ipp32u x = 0; x < Width; x++)
        {
            pPcmCb[x] = pRecoCbCr[x * 2];
            pPcmCr[x] = pRecoCbCr[x * 2 + 1];
        }
        pPcmCr += Width;
        pPcmCb += Width;
        pRecoCbCr += stride;
    }
}

/** Function for deriving recontructed PU/CU Luma sample with QTree structure
 * \param pcCU pointer of current CU
 * \param uiTrDepth current tranform split depth
 * \param uiAbsPartIdx  part index
 * \param pcRecoYuv pointer to reconstructed sample arrays
 * \param pcPredYuv pointer to prediction sample arrays
 * \param pcResiYuv pointer to residue sample arrays
 *
 \ This function dervies recontructed PU/CU Luma sample with recursive QTree structure
 */
void H265SegmentDecoder::IntraLumaRecQT(H265CodingUnit* pCU,
                     Ipp32u TrDepth,
                     Ipp32u AbsPartIdx)
{
    Ipp32u FullDepth = pCU->m_DepthArray[AbsPartIdx] + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx);
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraLumaRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart);
        }
    }
}

/** Function for deriving recontructed PU/CU chroma samples with QTree structure
 * \param pcCU pointer of current CU
 * \param uiTrDepth current tranform split depth
 * \param uiAbsPartIdx  part index
 * \param pcRecoYuv pointer to reconstructed sample arrays
 * \param pcPredYuv pointer to prediction sample arrays
 * \param pcResiYuv pointer to residue sample arrays
 *
 \ This function dervies recontructed PU/CU chroma samples with QTree recursive structure
 */
void H265SegmentDecoder::IntraChromaRecQT(H265CodingUnit* pCU,
                     Ipp32u TrDepth,
                     Ipp32u AbsPartIdx,
                     Ipp32u ChromaPredMode)
{
    Ipp32u FullDepth = pCU->m_DepthArray[AbsPartIdx] + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx, ChromaPredMode);
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraChromaRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart, ChromaPredMode);
        }
    }
}

void H265SegmentDecoder::IntraRecLumaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    Ipp32u Size = pCU->m_WidthArray[AbsPartIdx] >> TrDepth;

    bool NeighborFlags[4 * MAX_NUM_SPU_W + 1];
    Ipp32s NumIntraNeighbor = 0;

    // compute the location of the current PU
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;
    Ipp32s NumUnitsInCU = Size >> m_pSeqParamSet->log2_min_transform_block_size;

    if (XInc > 0 && g_RasterToZscan[g_ZscanToRaster[AbsPartIdx] - 1 + NumUnitsInCU * m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            NeighborFlags[NumUnitsInCU - 1 - i] = false;
    }
    else
    {
        NumIntraNeighbor += isIntraBelowLeftAvailable(TUPartNumberInCTB - 1 + NumUnitsInCU * m_context->m_CurrCTBStride, PartY, YInc, NumUnitsInCU, NeighborFlags + NumUnitsInCU - 1);
    }
    NumIntraNeighbor += isIntraLeftAvailable(TUPartNumberInCTB - 1, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 2) - 1);

    NeighborFlags[NumUnitsInCU * 2] = m_context->m_CurrCTBFlags[TUPartNumberInCTB - 1 - m_context->m_CurrCTBStride].members.IsAvailable &&
        (m_context->m_CurrCTBFlags[TUPartNumberInCTB - 1 - m_context->m_CurrCTBStride].members.IsIntra || !m_pPicParamSet->constrained_intra_pred_flag);
    NumIntraNeighbor += (Ipp32s)(NeighborFlags[NumUnitsInCU * 2]);

    NumIntraNeighbor += isIntraAboveAvailable(TUPartNumberInCTB - m_context->m_CurrCTBStride, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 2) + 1);
    if (YInc > 0 && g_RasterToZscan[g_ZscanToRaster[AbsPartIdx] + NumUnitsInCU - m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            NeighborFlags[NumUnitsInCU * 3 + 1 + i] = false;
    }
    else
    {
        if (YInc == 0)
            NumIntraNeighbor += isIntraAboveRightAvailableOtherCTB(PartX, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 3) + 1);
        else
            NumIntraNeighbor += isIntraAboveRightAvailable(TUPartNumberInCTB + NumUnitsInCU - m_context->m_CurrCTBStride, PartX, XInc, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 3) + 1);
    }

    InitNeighbourPatternLuma(pCU, AbsPartIdx, TrDepth,
        m_Prediction->GetPredicBuf(),
        m_Prediction->GetPredicBufWidth(),
        m_Prediction->GetPredicBufHeight(),
        NeighborFlags, NumIntraNeighbor);

    //===== get prediction signal =====
    Ipp32u LumaPredMode = pCU->m_LumaIntraDir[AbsPartIdx];
    H265PlanePtrYCommon pRecIPred = m_context->m_frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u RecIPredStride = pCU->m_Frame->pitch_luma();
    m_Prediction->PredIntraLumaAng(LumaPredMode, pRecIPred, RecIPredStride, Size);

    //===== inverse transform =====
    if (!pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrDepth))
        return;

    m_TrQuant->SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_LUMA, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetY, 0);

    Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_LUMA];
    Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth * pCU->m_SliceHeader->m_SeqParamSet->MaxCUHeight) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1);
    H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffY + (NumCoeffInc * AbsPartIdx);
    bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_LUMA]][AbsPartIdx] != 0;

    m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, pCU->m_LumaIntraDir[AbsPartIdx],
        pRecIPred, RecIPredStride, pCoeff, Size, Size, scalingListType, useTransformSkip);
}

void H265SegmentDecoder::IntraRecChromaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         Ipp32u ChromaPredMode)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    Ipp32u FullDepth  = pCU->m_DepthArray[AbsPartIdx] + TrDepth;
    Ipp32u Log2TrSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth >> FullDepth] + 2;
    if (Log2TrSize == 2)
    {
        VM_ASSERT(TrDepth > 0);
        TrDepth--;
        Ipp32u QPDiv = pCU->m_Frame->getCD()->getNumPartInCU() >> ((pCU->m_DepthArray[AbsPartIdx] + TrDepth) << 1);
        bool FirstQFlag = ((AbsPartIdx % QPDiv) == 0);
        if (!FirstQFlag)
        {
            return;
        }
    }

    Ipp32u Size = pCU->m_WidthArray[AbsPartIdx] >> TrDepth;

    bool NeighborFlags[4 * MAX_NUM_SPU_W + 1];
    Ipp32s NumIntraNeighbor = 0;

    // compute the location of the current PU
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;
    Ipp32s NumUnitsInCU = Size >> m_pSeqParamSet->log2_min_transform_block_size;

    // TODO: Use neighbours information from Luma block
    if (XInc > 0 && g_RasterToZscan[g_ZscanToRaster[AbsPartIdx] - 1 + NumUnitsInCU * m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            NeighborFlags[NumUnitsInCU - 1 - i] = false;
    }
    else
        NumIntraNeighbor += isIntraBelowLeftAvailable(TUPartNumberInCTB - 1 + NumUnitsInCU * m_context->m_CurrCTBStride, PartY, YInc, NumUnitsInCU, NeighborFlags + NumUnitsInCU - 1);
    NumIntraNeighbor += isIntraLeftAvailable(TUPartNumberInCTB - 1, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 2) - 1);

    NeighborFlags[NumUnitsInCU * 2] = m_context->m_CurrCTBFlags[TUPartNumberInCTB - 1 - m_context->m_CurrCTBStride].members.IsAvailable &&
        (m_context->m_CurrCTBFlags[TUPartNumberInCTB - 1 - m_context->m_CurrCTBStride].members.IsIntra || !m_pPicParamSet->constrained_intra_pred_flag);
    NumIntraNeighbor += (Ipp32s)(NeighborFlags[NumUnitsInCU * 2]);

    NumIntraNeighbor += isIntraAboveAvailable(TUPartNumberInCTB - m_context->m_CurrCTBStride, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 2) + 1);
    if (YInc > 0 && g_RasterToZscan[g_ZscanToRaster[AbsPartIdx] + NumUnitsInCU - m_pSeqParamSet->NumPartitionsInCUSize] > AbsPartIdx)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
            NeighborFlags[NumUnitsInCU * 3 + 1 + i] = false;
    }
    else
    {
        if (YInc == 0)
            NumIntraNeighbor += isIntraAboveRightAvailableOtherCTB(PartX, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 3) + 1);
        else
            NumIntraNeighbor += isIntraAboveRightAvailable(TUPartNumberInCTB + NumUnitsInCU - m_context->m_CurrCTBStride, PartX, XInc, NumUnitsInCU, NeighborFlags + (NumUnitsInCU * 3) + 1);
    }

    InitNeighbourPatternChroma(pCU, AbsPartIdx, TrDepth,
        m_Prediction->GetPredicBuf(),
        m_Prediction->GetPredicBufWidth(),
        m_Prediction->GetPredicBufHeight(),
        NeighborFlags, NumIntraNeighbor);

    //===== get prediction signal =====
    Size = pCU->m_WidthArray[AbsPartIdx] >> (TrDepth + 1);
    H265PlanePtrUVCommon pPatChroma = m_Prediction->GetPredicBuf();
    H265PlanePtrUVCommon pRecIPred = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
    Ipp32u RecIPredStride = pCU->m_Frame->pitch_chroma();

    m_Prediction->PredIntraChromaAng(pPatChroma, ChromaPredMode, pRecIPred, RecIPredStride, Size);

    bool chromaUPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth) != 0;
    bool chromaVPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth) != 0;

    if (!chromaUPresent && !chromaVPresent)
        return;

    //===== inverse transform =====
    Ipp32u NumCoeffInc = ((pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth * pCU->m_SliceHeader->m_SeqParamSet->MaxCUHeight) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1)) >> 2;
    Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

    // Cb
    if (chromaUPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCbQpOffset() + pCU->m_SliceHeader->slice_cb_qp_offset;
        m_TrQuant->SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_CHROMA_U, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_U];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCb + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCbAddr();
        bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_U]][AbsPartIdx] != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT,
            pResi, residualPitch, pCoeff, Size, Size, scalingListType, useTransformSkip);
    }

    // Cr
    if (chromaVPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCrQpOffset() + pCU->m_SliceHeader->slice_cr_qp_offset;
        m_TrQuant->SetQPforQuant(pCU->m_QPArray[AbsPartIdx], TEXT_CHROMA_V, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_V];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCr + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCrAddr();
        bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_V]][AbsPartIdx] != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT,
            pResi, residualPitch, pCoeff, Size, Size, scalingListType, useTransformSkip);
    }

    //===== reconstruction =====
    {
        H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCbAddr();
        H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCrAddr();
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

void H265SegmentDecoder::UpdatePUInfo(H265CodingUnit *pCU, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, H265PUInfo &PUi)
{
    H265MVInfo &MVi = PUi.interinfo;
    VM_ASSERT(MVi.mvinfo[REF_PIC_LIST_0].RefIdx >= 0 || MVi.mvinfo[REF_PIC_LIST_1].RefIdx >= 0);

    for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
    {
        if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0 && MVi.mvinfo[RefListIdx].RefIdx >= 0)
        {
            H265DecoderRefPicList::ReferenceInformation &refInfo = m_pRefPicList[RefListIdx][MVi.mvinfo[RefListIdx].RefIdx];
            PUi.refFrame[RefListIdx] = refInfo.refFrame;
            Ipp16s POCDelta = Ipp16s(m_pCurrentFrame->m_PicOrderCnt - refInfo.refFrame->m_PicOrderCnt);
            pCU->m_Frame->m_CodingData->setBlockInfo(refInfo.isLongReference ? COL_TU_LT_INTER : COL_TU_ST_INTER, POCDelta, MVi.mvinfo[RefListIdx].MV, MVi.mvinfo[RefListIdx].RefIdx, 
                (EnumRefPicList)RefListIdx, PartX, PartY, PartWidth, PartHeight);
        }
        else
        {
            pCU->m_Frame->m_CodingData->setBlockFlags(COL_TU_INVALID_INTER, (EnumRefPicList)RefListIdx, PartX, PartY, PartWidth, PartHeight);
        }
    }

    Ipp32u mask = (1 << m_pSeqParamSet->MaxCUDepth) - 1;
    PartX &= mask;
    PartY &= mask;

    H265FrameHLDNeighborsInfo info;
    info.data = 0;
    info.members.IsAvailable = 1;

    // Bottom row
    for (Ipp32u i = 0; i < PartWidth; i++)
    {
        m_context->m_CurrCTBFlags[(PartY + PartHeight - 1) * m_context->m_CurrCTBStride + PartX + i].data = info.data;
        m_context->m_CurrCTB[(PartY + PartHeight - 1) * m_context->m_CurrCTBStride + PartX + i] = MVi;
    }

    // Right column
    for (Ipp32u i = 0; i < PartHeight; i++)
    {
        m_context->m_CurrCTBFlags[(PartY + i) * m_context->m_CurrCTBStride + PartX + PartWidth - 1].data = info.data;
        m_context->m_CurrCTB[(PartY + i) * m_context->m_CurrCTBStride + PartX + PartWidth - 1] = MVi;
    }
}

void H265SegmentDecoder::UpdateNeighborBuffers(H265CodingUnit* pCU, Ipp32u AbsPartIdx, bool isSkipped)
{
    Ipp32s i;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartSize = pCU->m_WidthArray[AbsPartIdx] >> m_pSeqParamSet->log2_min_transform_block_size;

    H265FrameHLDNeighborsInfo info;
    info.data = 0;
    info.members.IsAvailable = 1;
    info.members.IsIntra = pCU->m_PredModeArray[AbsPartIdx];
    info.members.Depth = pCU->m_DepthArray[AbsPartIdx];
    info.members.SkipFlag = isSkipped;
    info.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx];
    Ipp16u data = info.data;

    if (info.members.IsIntra)
    {
        // Fill up inside of whole CU to predict intra parts inside of it
        for (Ipp32s y = YInc; y < YInc + PartSize - 1; y++)
            for (Ipp32s x = XInc; x < XInc + PartSize - 1; x++)
                m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * y + x].data = data;
    }

    if (SIZE_2Nx2N == pCU->m_PartSizeArray[AbsPartIdx])
    {
        for (i = 0; i < PartSize; i++)
        {
            // Bottom row
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data;
            // Right column
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data;
        }
    }
    else
    {
        H265FrameHLDNeighborsInfo info1, info2, info3;
        Ipp16u data1, data2, data3;
        Ipp32u PartOffset = (pCU->m_Frame->m_CodingData->m_NumPartitions >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 2;

        info1.data = data;
        info2.data = data;
        info3.data = data;
        info1.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset];
        info2.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset * 2];
        info3.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset * 3];
        data1 = info1.data;
        data2 = info2.data;
        data3 = info3.data;

        VM_ASSERT(PartSize >> 1);

        for (i = 0; i < PartSize >> 1; i++)
        {
            // Bottom row
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data2;
            // Right column
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data1;
        }

        for (i = PartSize >> 1; i < PartSize; i++)
        {
            // Bottom row
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data3;
            // Right column
            m_context->m_CurrCTBFlags[m_context->m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data3;
        }
    }
}

Ipp32u H265SegmentDecoder::getCtxSplitFlag(H265CodingUnit *, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uiCtx;

    uiCtx = 0;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;

    if ((m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.Depth > Depth))
    {
        uiCtx = 1;
    }

    if ((m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.Depth > Depth))
    {
        uiCtx += 1;
    }

    return uiCtx;
}

Ipp32u H265SegmentDecoder::getCtxSkipFlag(Ipp32u AbsPartIdx)
{
    Ipp32u uiCtx = 0;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;

    if (m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.SkipFlag)
    {
        uiCtx = 1;
    }

    if (m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.SkipFlag)
    {
        uiCtx += 1;
    }

    return uiCtx;
}

void H265SegmentDecoder::getIntraDirLumaPredictor(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32s IntraDirPred[])
{
    Ipp32s LeftIntraDir = DC_IDX;
    Ipp32s AboveIntraDir = DC_IDX;
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->log2_min_transform_block_size;

    Ipp32s PartOffsetY = (m_pSeqParamSet->NumPartitionsInCU >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 1;
    Ipp32s PartOffsetX = PartOffsetY >> 1;

    // Get intra direction of left PU
    if ((AbsPartIdx & PartOffsetX) == 0)
    {
        LeftIntraDir = m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.IsIntra ? m_context->m_CurrCTBFlags[YInc * m_context->m_CurrCTBStride + XInc - 1].members.IntraDir : DC_IDX;
    }
    else
    {
        LeftIntraDir = pCU->m_LumaIntraDir[AbsPartIdx - PartOffsetX];
    }

    // Get intra direction of above PU
    if ((AbsPartIdx & PartOffsetY) == 0)
    {
        if (YInc > 0)
            AboveIntraDir = m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.IsIntra ? m_context->m_CurrCTBFlags[(YInc - 1) * m_context->m_CurrCTBStride + XInc].members.IntraDir : DC_IDX;
    }
    else
    {
        AboveIntraDir = pCU->m_LumaIntraDir[AbsPartIdx - PartOffsetY];
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
            IntraDirPred[0] = PLANAR_IDX;
            IntraDirPred[1] = DC_IDX;
            IntraDirPred[2] = VER_IDX;
        }
    }
    else
    {
        IntraDirPred[0] = LeftIntraDir;
        IntraDirPred[1] = AboveIntraDir;

        if (LeftIntraDir && AboveIntraDir) //both modes are non-planar
        {
            IntraDirPred[2] = PLANAR_IDX;
        }
        else
        {
            IntraDirPred[2] = (LeftIntraDir + AboveIntraDir) < 2 ? VER_IDX : DC_IDX;
        }
    }
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
    if (m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].RefIdx != m_context->m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_0].RefIdx ||
        m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].RefIdx != m_context->m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_1].RefIdx)
        return false;

    if (m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].RefIdx >= 0 && m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].MV != m_context->m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_0].MV)
        return false;

    if (m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].RefIdx >= 0 && m_context->m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].MV != m_context->m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_1].MV)
        return false;

    return true;
}

#define UPDATE_MV_INFO(dir)                                                                         \
    CandIsInter[Count] = true;                                                                      \
    if (m_context->m_CurrCTB[dir].mvinfo[REF_PIC_LIST_0].RefIdx >= 0)                                          \
    {                                                                                               \
        InterDirNeighbours[Count] = 1;                                                              \
        MVBufferNeighbours[Count << 1] = m_context->m_CurrCTB[dir].mvinfo[REF_PIC_LIST_0];                     \
    }                                                                                               \
    else                                                                                            \
        InterDirNeighbours[Count] = 0;                                                              \
                                                                                                    \
    if (B_SLICE == m_pSliceHeader->slice_type && m_context->m_CurrCTB[dir].mvinfo[REF_PIC_LIST_1].RefIdx >= 0) \
    {                                                                                               \
        InterDirNeighbours[Count] += 2;                                                             \
        MVBufferNeighbours[(Count << 1) + 1] = m_context->m_CurrCTB[dir].mvinfo[REF_PIC_LIST_1];               \
    }                                                                                               \
                                                                                                    \
    if (mrgCandIdx == Count)                                                                        \
    {                                                                                               \
        return;                                                                                     \
    }                                                                                               \
    Count++;


static Ipp32u PriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
static Ipp32u PriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};

void H265SegmentDecoder::getInterMergeCandidates(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, MVBuffer *MVBufferNeighbours,
                                                 Ipp8u *InterDirNeighbours, Ipp32s &numValidMergeCand, Ipp32s mrgCandIdx)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    numValidMergeCand = m_pSliceHeader->m_MaxNumMergeCand;
    bool CandIsInter[MRG_MAX_NUM_CANDS];
    for (Ipp32s ind = 0; ind < numValidMergeCand; ++ind)
    {
        CandIsInter[ind] = false;
        MVBufferNeighbours[ind << 1].RefIdx = NOT_VALID;
        MVBufferNeighbours[(ind << 1) + 1].RefIdx = NOT_VALID;
    }

    // compute the location of the current PU
    Ipp32u XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;

    Ipp32s nPSW, nPSH;
    pCU->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32s PartWidth = nPSW >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartHeight = nPSH >> m_pSeqParamSet->log2_min_transform_block_size;
    EnumPartSize CurPS = (EnumPartSize)pCU->m_PartSizeArray[AbsPartIdx];

    Ipp32s Count = 0;

    // left
    Ipp32s leftAddr = TUPartNumberInCTB + m_context->m_CurrCTBStride * (PartHeight - 1) - 1;
    bool isAvailableA1 = m_context->m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[leftAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == SIZE_Nx2N || CurPS == SIZE_nLx2N || CurPS == SIZE_nRx2N));

    if (isAvailableA1)
    {
        UPDATE_MV_INFO(leftAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // above
    Ipp32s aboveAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + (PartWidth - 1);
    bool isAvailableB1 = m_context->m_CurrCTBFlags[aboveAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW - 1, TPelY - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == SIZE_2NxN || CurPS == SIZE_2NxnU || CurPS == SIZE_2NxnD));

    if (isAvailableB1 && (!isAvailableA1 || !hasEqualMotion(leftAddr, aboveAddr)))
    {
        UPDATE_MV_INFO(aboveAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // above right
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_context->m_CurrCTBStride + PartWidth;
    bool isAvailableB0 = m_context->m_CurrCTBFlags[aboveRightAddr].members.IsAvailable && !m_context->m_CurrCTBFlags[aboveRightAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW, TPelY - 1, LPelX, TPelY);

    if (isAvailableB0 && (!isAvailableB1 || !hasEqualMotion(aboveAddr, aboveRightAddr)))
    {
        UPDATE_MV_INFO(aboveRightAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
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
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
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
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    if (m_pSliceHeader->m_enableTMVPFlag)
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
            MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(ColMV, RefIdx);
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
                MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(ColMV, RefIdx);
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
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    Ipp32s ArrayAddr = Count;
    Ipp32s Cutoff = ArrayAddr;

    if (m_pSliceHeader->slice_type == B_SLICE)
    {
        for (Ipp32s idx = 0; idx < Cutoff * (Cutoff - 1) && ArrayAddr != m_pSliceHeader->m_MaxNumMergeCand; idx++)
        {
            Ipp32s i = PriorityList0[idx];
            Ipp32s j = PriorityList1[idx];
            if (CandIsInter[i] && CandIsInter[j] && (InterDirNeighbours[i] & 0x1) && (InterDirNeighbours[j] & 0x2))
            {
                CandIsInter[ArrayAddr] = true;
                InterDirNeighbours[ArrayAddr] = 3;

                // get MV from cand[i] and cand[j]
                MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(MVBufferNeighbours[i << 1].MV, MVBufferNeighbours[i << 1].RefIdx);
                MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(MVBufferNeighbours[(j << 1) + 1].MV, MVBufferNeighbours[(j << 1) + 1].RefIdx);

                Ipp32s RefPOCL0 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_0][MVBufferNeighbours[(ArrayAddr << 1)].RefIdx];
                Ipp32s RefPOCL1 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_1][MVBufferNeighbours[(ArrayAddr << 1) + 1].RefIdx];
                if (RefPOCL0 == RefPOCL1 && MVBufferNeighbours[(ArrayAddr << 1)].MV == MVBufferNeighbours[(ArrayAddr << 1) + 1].MV)
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
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    Ipp32s numRefIdx = (m_pSliceHeader->slice_type == B_SLICE) ? IPP_MIN(m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0], m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_1]) : m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0];
    Ipp8s r = 0;
    Ipp32s refcnt = 0;

    while (ArrayAddr < m_pSliceHeader->m_MaxNumMergeCand)
    {
        CandIsInter[ArrayAddr] = true;
        InterDirNeighbours[ArrayAddr] = 1;
        MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(H265MotionVector(0, 0), r);

        if (m_pSliceHeader->slice_type == B_SLICE)
        {
            InterDirNeighbours[ArrayAddr] = 3;
            MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(H265MotionVector(0, 0), r);
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

void H265SegmentDecoder::fillMVPCand(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo)
{
    H265MotionVector MvPred;
    bool Added = false;

    pInfo->NumbOfCands = 0;
    if (RefIdx < 0)
    {
        return;
    }

    // compute the location of the current PU
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    YInc >>= m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartX = LPelX >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartY = TPelY >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s TUPartNumberInCTB = m_context->m_CurrCTBStride * YInc + XInc;

    Ipp32s nPSW, nPSH;
    pCU->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
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

    if (m_pSliceHeader->m_enableTMVPFlag)
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

    if (pInfo->NumbOfCands > AMVP_MAX_NUM_CANDS)
    {
        pInfo->NumbOfCands = AMVP_MAX_NUM_CANDS;
    }
    while (pInfo->NumbOfCands < AMVP_MAX_NUM_CANDS)
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

    if (m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList].RefIdx == RefIdx)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands++] = m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList].MV;
        return true;
    }

    EnumRefPicList RefPicList2nd = EnumRefPicList(1 - RefPicList);
    RefIndexType NeibRefIdx = m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList2nd].RefIdx;

    if (NeibRefIdx >= 0)
    {
        if(m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList2nd][NeibRefIdx].refFrame->m_PicOrderCnt)
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList2nd].MV;
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
        RefIndexType NeibRefIdx = m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList].RefIdx;

        if (NeibRefIdx >= 0)
        {
            bool IsNeibRefLongTerm = m_pRefPicList[RefPicList][NeibRefIdx].isLongReference;

            if (IsNeibRefLongTerm == IsCurrRefLongTerm)
            {
                H265MotionVector MvPred = m_context->m_CurrCTB[NeibAddr].mvinfo[RefPicList].MV;

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
    H265DecoderFrame *colPic = m_pRefPicList[colPicListIdx][m_pSliceHeader->m_ColRefIdx].refFrame;
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

Ipp32s H265SegmentDecoder::isIntraAboveAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    if (m_pPicParamSet->constrained_intra_pred_flag)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if (m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsIntra)
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
    }
    else
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if (m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable)
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
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isIntraLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    if (m_pPicParamSet->constrained_intra_pred_flag)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if (m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsIntra)
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
    }
    else
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if (m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable)
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
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isIntraAboveRightAvailableOtherCTB(Ipp32s PartX, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartX += NumUnitsInCU;

    if (m_pPicParamSet->constrained_intra_pred_flag)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
                m_context->m_TopNgbrs[PartX].members.IsAvailable &&
                m_context->m_TopNgbrs[PartX].members.IsIntra)
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
    }
    else
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
                m_context->m_TopNgbrs[PartX].members.IsAvailable)
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
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isIntraAboveRightAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartX, Ipp32s XInc, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartX += NumUnitsInCU;
    XInc += NumUnitsInCU;

    if (m_pPicParamSet->constrained_intra_pred_flag)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
                XInc < m_pCurrentFrame->getNumPartInCUSize() &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsIntra)
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
    }
    else
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartX << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_width_in_luma_samples &&
                XInc < m_pCurrentFrame->getNumPartInCUSize() &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable)
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
    }

    return NumIntra;
}

Ipp32s H265SegmentDecoder::isIntraBelowLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartY, Ipp32s YInc, Ipp32s NumUnitsInCU, bool *ValidFlags)
{
    Ipp32s NumIntra = 0;

    PartY += NumUnitsInCU;
    YInc += NumUnitsInCU;

    if (m_pPicParamSet->constrained_intra_pred_flag)
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartY << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_height_in_luma_samples &&
                YInc < m_pCurrentFrame->getNumPartInCUSize() &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsIntra)
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
    }
    else
    {
        for (Ipp32s i = 0; i < NumUnitsInCU; i++)
        {
            if ((PartY << m_pSeqParamSet->log2_min_transform_block_size) < m_pSeqParamSet->pic_height_in_luma_samples &&
                YInc < m_pCurrentFrame->getNumPartInCUSize() &&
                m_context->m_CurrCTBFlags[TUPartNumberInCTB].members.IsAvailable)
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
    }

    return NumIntra;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
