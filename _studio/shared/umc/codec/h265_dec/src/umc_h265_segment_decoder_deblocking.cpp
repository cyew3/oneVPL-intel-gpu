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
#include "umc_h265_frame_info.h"
#include "umc_h265_task_broker.h"

enum
{
    VERT_FILT,
    HOR_FILT
};

namespace UMC_HEVC_DECODER
{

// Do deblocking task for a range of CTBs specified in the task
void H265SegmentDecoder::DeblockSegment(H265Task & task)
{
    for (Ipp32s i = task.m_iFirstMB; i < task.m_iFirstMB + task.m_iMBToProcess; i++)
    {
        DeblockOneLCU(i);
    }
} // void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstMB, Ipp32s iNumMBs)

// Compare motion vectors for equality
static Ipp8u inline H265_FORCEINLINE MVIsnotEq(const H265MotionVector &mv0, const H265MotionVector &mv1)
{
    return (7 <= (Ipp32u)(mv0.Horizontal - mv1.Horizontal + 3) || 7 <= (Ipp32u)(mv0.Vertical - mv1.Vertical + 3)) ? 1 : 0;
}

// Edge strength calculation for two inter blocks
void H265SegmentDecoder::GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265PartialEdgeData *edge)
{
    Ipp32s numRefsQ = 0;
    for (Ipp32s i = 0; i < 2; i++)
        numRefsQ += (mvinfoQ->m_refIdx[i] >= 0) ? 1 : 0;

    Ipp32s numRefsP = 0;
    for (Ipp32s i = 0; i < 2; i++)
        numRefsP += (mvinfoP->m_refIdx[i] >= 0) ? 1 : 0;

    if (numRefsP != numRefsQ)
    {
        edge->strength = 1;
        return;
    }

    if (numRefsQ == 2)
    {
        if (((mvinfoQ->m_index[REF_PIC_LIST_0] == mvinfoP->m_index[REF_PIC_LIST_0]) &&
            (mvinfoQ->m_index[REF_PIC_LIST_1] == mvinfoP->m_index[REF_PIC_LIST_1])) ||
            ((mvinfoQ->m_index[REF_PIC_LIST_0] == mvinfoP->m_index[REF_PIC_LIST_1]) &&
            (mvinfoQ->m_index[REF_PIC_LIST_1] == mvinfoP->m_index[REF_PIC_LIST_0])))
        {
            if (mvinfoP->m_index[REF_PIC_LIST_0] != mvinfoP->m_index[REF_PIC_LIST_1])
            {
                if (mvinfoQ->m_index[REF_PIC_LIST_0] == mvinfoP->m_index[REF_PIC_LIST_0])
                {
                    edge->strength = MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1]);
                    return;
                }
                else
                {
                    edge->strength = MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0]);
                    return;
                }
            }
            else
            {
                edge->strength = (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1])) &&
                        (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0])) ? 1 : 0;
                return;
            }
        }

        edge->strength = 1;
        return;
    }
    else
    {
        Ipp32s indexQ, indexP;
        H265MotionVector *MVQ, *MVP;
        
        if (mvinfoQ->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            indexQ = mvinfoQ->m_index[REF_PIC_LIST_0];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            indexQ = mvinfoQ->m_index[REF_PIC_LIST_1];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_1];
        }

        if (mvinfoP->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            indexP = mvinfoP->m_index[REF_PIC_LIST_0];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            indexP = mvinfoP->m_index[REF_PIC_LIST_1];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_1];
        }

        if (indexQ == indexP)
        {
            edge->strength = MVIsnotEq(*MVQ, *MVP);
            return;
        }

        edge->strength = 1;
        return;
    }
}

// Calculate number of prediction units for horizondal edge
Ipp32s GetNumPartForHorFilt(H265CodingUnit* cu, Ipp32u absPartIdx)
{
    Ipp32s numParts;
    switch (cu->GetPartitionSize(absPartIdx))
    {
    case PART_SIZE_2NxN:
    case PART_SIZE_NxN:
    case PART_SIZE_2NxnU:
    case PART_SIZE_2NxnD:
        numParts = 2;
        break;
    default:
        numParts = 1;
        break;
    }

    return numParts;
}

// Calculate number of prediction units for vertical edge
Ipp32s GetNumPartForVertFilt(H265CodingUnit* cu, Ipp32u absPartIdx)
{
    Ipp32s numParts;
    switch (cu->GetPartitionSize(absPartIdx))
    {
    case PART_SIZE_Nx2N:
    case PART_SIZE_NxN:
    case PART_SIZE_nLx2N:
    case PART_SIZE_nRx2N:
        numParts = 2;
        break;
    default:
        numParts = 1;
        break;
    }

    return numParts;
}

// Recursively deblock edges inside of TU
void H265SegmentDecoder::DeblockTU(Ipp32u absPartIdx, Ipp32u trDepth)
{
    Ipp32u stopDepth = m_cu->GetDepth(absPartIdx) + m_cu->GetTrIndex(absPartIdx);
    if (trDepth < stopDepth)
    {
        trDepth++;
        Ipp32u depth = trDepth;
        Ipp32u partOffset = m_cu->m_NumPartition >> (depth << 1);
        
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += partOffset)
        {
            DeblockTU(absPartIdx, trDepth);
        }

        return;
    }

    Ipp32s size = m_cu->GetWidth(absPartIdx) >> m_cu->GetTrIndex(absPartIdx);

    for (Ipp32s edgeType = VERT_FILT; edgeType <= HOR_FILT; edgeType++)
    {
        Ipp32u curPixelColumn = m_cu->m_rasterToPelX[absPartIdx];
        Ipp32u curPixelRow = m_cu->m_rasterToPelY[absPartIdx];

        if (isMin4x4Block && ((edgeType == VERT_FILT) ? (curPixelColumn % 8) : (curPixelRow % 8)))
        {
            continue;
        }

        if (edgeType == VERT_FILT)
        {
            H265EdgeData edge1;
            H265EdgeData *edge = &edge1;
            edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
            edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

            for (Ipp32s i = 0; i < size / 4; i++)
            {
                CalculateEdge<VERT_FILT, H265EdgeData>(edge, curPixelColumn, curPixelRow + 4 * i, true);

                if (edge->strength > 0)
                {
                    m_reconstructor->FilterEdgeLuma(edge, m_pY_CU_Plane, m_pitch, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
                }

                if (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400)
                {
                    bool additionalChromaCheck = (i % 2) == 0 && !(curPixelRow % 8);

                    if (edge->strength > 1 && !(curPixelColumn % 16) && (!m_pSeqParamSet->chromaShiftH || additionalChromaCheck))
                    {
                        m_reconstructor->FilterEdgeChroma(edge, m_pUV_CU_Plane, m_pitch, (curPixelColumn >> 1), (curPixelRow + 4 * i) >> m_pSeqParamSet->chromaShiftH, VERT_FILT, m_pPicParamSet->pps_cb_qp_offset, m_pPicParamSet->pps_cr_qp_offset, m_pSeqParamSet->bit_depth_chroma);
                    }
                }
            }

            if (m_deblockPredData[edgeType].predictionExist && (curPixelRow >= m_deblockPredData[edgeType].saveRow && curPixelRow <= m_deblockPredData[edgeType].saveRow + m_deblockPredData[edgeType].saveHeight) &&
                (m_deblockPredData[edgeType].saveColumn > curPixelColumn && m_deblockPredData[edgeType].saveColumn < curPixelColumn + size))
            {
                curPixelColumn = m_deblockPredData[edgeType].saveColumn;

                for (Ipp32s i = 0; i < size / 4; i++)
                {
                    CalculateEdge<VERT_FILT, H265EdgeData>(edge, curPixelColumn, curPixelRow + 4 * i, false);

                    if (edge->strength > 0)
                    {
                        m_reconstructor->FilterEdgeLuma(edge, m_pY_CU_Plane, m_pitch, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
                    }

                    if (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400)
                    {
                        bool additionalChromaCheck = (i % 2) == 0 && !(curPixelRow % 8);
                        if (edge->strength > 1 && !(curPixelColumn % 16) && (!m_pSeqParamSet->chromaShiftH || additionalChromaCheck))
                        {
                            m_reconstructor->FilterEdgeChroma(edge, m_pUV_CU_Plane, m_pitch, (curPixelColumn >> 1), (curPixelRow + 4 * i) >> m_pSeqParamSet->chromaShiftH, VERT_FILT, m_pPicParamSet->pps_cb_qp_offset, m_pPicParamSet->pps_cr_qp_offset, m_pSeqParamSet->bit_depth_chroma);
                        }
                    }
                }
            }
        }
        else
        {
            Ipp32s minSize = (isMin4x4Block ? 4 : 8);

            Ipp32s x = curPixelColumn >> (2 + !isMin4x4Block);
            Ipp32s y = curPixelRow >> 3;

            H265PartialEdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
            H265PartialEdgeData *edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x;

            for (Ipp32s i = 0; i < size / minSize; i++)
            {
                CalculateEdge<HOR_FILT, H265PartialEdgeData>(edge, curPixelColumn + minSize * i, curPixelRow, true);

                edge++;
            }

            if (m_deblockPredData[edgeType].predictionExist && (curPixelColumn >= m_deblockPredData[edgeType].saveColumn && curPixelColumn <= m_deblockPredData[edgeType].saveColumn + m_deblockPredData[edgeType].saveHeight) &&
                (m_deblockPredData[edgeType].saveRow > curPixelRow && m_deblockPredData[edgeType].saveRow < curPixelRow + size))
            {
                curPixelRow = m_deblockPredData[edgeType].saveRow;
                y = curPixelRow >> 3;

                edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x;

                for (Ipp32s i = 0; i < size / minSize; i++)
                {
                    CalculateEdge<HOR_FILT, H265PartialEdgeData>(edge, curPixelColumn + minSize * i, curPixelRow, false);

                    edge++;
                }
            }
        }
    }
}

// Recursively deblock edges inside of CU
void H265SegmentDecoder::DeblockCURecur(Ipp32u absPartIdx, Ipp32u depth)
{
    if (m_cu->GetDepth(absPartIdx) > depth)
    {
        depth++;
        Ipp32u partOffset = m_cu->m_NumPartition >> (depth << 1);
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += partOffset)
        {
            DeblockCURecur(absPartIdx, depth);
        }
        return;
    }

    for (Ipp32s edgeType = VERT_FILT;  edgeType <= HOR_FILT; edgeType++)
    {
        Ipp32s countPart = (edgeType == VERT_FILT) ? GetNumPartForVertFilt(m_cu, absPartIdx) : GetNumPartForHorFilt(m_cu, absPartIdx);
        m_deblockPredData[edgeType].predictionExist = false;

        if (countPart == 2)
        {
            Ipp32u width;
            Ipp32u height;

            Ipp32s isFourParts = m_cu->getNumPartInter(absPartIdx) == 4;

            Ipp32u PUOffset = (g_PUOffset[m_cu->GetPartitionSize(absPartIdx)] << ((m_pSeqParamSet->MaxCUDepth - depth) << 1)) >> 4;
            Ipp32s subPartIdx = absPartIdx + PUOffset;
            if (isFourParts && edgeType == HOR_FILT)
                subPartIdx += PUOffset;

            m_cu->getPartIndexAndSize(absPartIdx, 1, width, height);

            Ipp32u curPixelColumn = m_cu->m_rasterToPelX[subPartIdx];
            Ipp32u curPixelRow = m_cu->m_rasterToPelY[subPartIdx];

            if (m_pSeqParamSet->MinCUSize != 4 || (edgeType == VERT_FILT ? ((curPixelColumn % 8) == 0) : ((curPixelRow % 8) == 0)))
            {
                m_deblockPredData[edgeType].predictionExist = true;
                m_deblockPredData[edgeType].saveColumn = curPixelColumn;
                m_deblockPredData[edgeType].saveRow = curPixelRow;
                m_deblockPredData[edgeType].saveHeight = (edgeType == VERT_FILT) ? height : width;
                m_deblockPredData[edgeType].saveHeight <<= isFourParts;
            }
        }
    }

    DeblockTU(absPartIdx, depth);
}

// Deblock horizontal edge
void H265SegmentDecoder::DeblockOneCross(Ipp32s curPixelColumn, Ipp32s curPixelRow, bool isNeedAddHorDeblock)
{
    Ipp32s chromaCbQpOffset = m_pPicParamSet->pps_cb_qp_offset;
    Ipp32s chromaCrQpOffset = m_pPicParamSet->pps_cr_qp_offset;

    /*H265EdgeData edgeForCheck;
    H265EdgeData *edge = &edgeForCheck;
    edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

    for (Ipp32s i = 0; i < 2; i++)
    {
        CalculateEdge<VERT_FILT>(curLCU, edge, curPixelColumn, curPixelRow + 4 * i);

        if (edge->strength == 0)
            continue;

        m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);

        if (edge->strength > 1 && i == 0 && !(curPixelColumn % 16))
        {
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * i, VERT_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }
    }*/

    Ipp32s x = curPixelColumn >> (2 + !isMin4x4Block);
    Ipp32s y = curPixelRow >> 3;

    H265PartialEdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
    H265PartialEdgeData *hor_edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x - 1;

    H265EdgeData edge;
    edge.tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
    edge.betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

    Ipp32s end = isNeedAddHorDeblock ? 3 : 2;
    for (Ipp32s i = 0; i < end; i++)
    {
        Ipp32s currPixelColumnTemp = curPixelColumn + 4 * (i - 1);
        H265CodingUnit* curLCUTemp = m_cu;

        if (currPixelColumnTemp < 0)
        {
            currPixelColumnTemp += m_pSeqParamSet->MaxCUSize;

            if (m_cu->m_CUPelX)
            {
                curLCUTemp = m_pCurrentFrame->getCU(m_cu->CUAddr - 1);
                if (curLCUTemp->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                {
                    hor_edge++;
                    continue;
                }

                edge.tcOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_tc_offset;
                edge.betaOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_beta_offset;
            }
            else
            {
                hor_edge++;
                continue;
            }
        }

        //CalculateEdge<HOR_FILT>(curLCUTemp, edge, currPixelColumnTemp, curPixelRow);
        *(H265PartialEdgeData*)&edge = *hor_edge;

        if (edge.strength > 0)
        {
            m_reconstructor->FilterEdgeLuma(&edge, m_pY_CU_Plane, m_pitch, curPixelColumn + 4 * (i - 1), curPixelRow, HOR_FILT, m_pSeqParamSet->bit_depth_luma);
        }

        bool additionalChromaCheck = !(curPixelRow % 16);
        if (edge.strength > 1 && i != 1 && (!m_pSeqParamSet->chromaShiftH || additionalChromaCheck) && (m_pSeqParamSet->ChromaArrayType != CHROMA_FORMAT_400))
        {
            Ipp32s column = (curPixelColumn>>1) + 4 * (i - 1);
            if (i == 2)
                column = (curPixelColumn>>1) + 4 * (i - 2);
            m_reconstructor->FilterEdgeChroma(&edge, m_pUV_CU_Plane, m_pitch, column, (curPixelRow >> m_pSeqParamSet->chromaShiftH), HOR_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }

        if (curPixelColumn + 4 * (i - 1) < 0)
        {
            // restore offsets
            edge.tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
            edge.betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;
        }

        /*if (hor_edge->strength != edge->strength || (hor_edge->qp != edge->qp && edge->strength))
        {
            __asm int 3;
        }*/

        if (isMin4x4Block || i == 0)
        {
            hor_edge++;
        }
    }
}

// Deblock edges inside of one CTB, left and top of it
void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr)
{
    m_cu = m_pCurrentFrame->getCU(curLCUAddr);
    m_pSliceHeader = m_cu->m_SliceHeader;

    if (!m_cu->m_SliceHeader || m_cu->m_SliceHeader->slice_deblocking_filter_disabled_flag)
        return;

    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;

    VM_ASSERT (!m_cu->m_SliceHeader->slice_deblocking_filter_disabled_flag || m_bIsNeedWADeblocking);

    Ipp32s width = frameWidthInSamples - m_cu->m_CUPelX;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    Ipp32s height = frameHeightInSamples - m_cu->m_CUPelY;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    isMin4x4Block = m_pSeqParamSet->MinCUSize == 4 ? 1 : 0;

    bool picLBoundary = ((m_cu->m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((m_cu->m_CUPelY == 0) ? true : false);
    Ipp32s numLCUInPicWidth = m_pCurrentFrame->m_CodingData->m_WidthInCU;

    m_cu->m_AvailBorder.m_left = !picLBoundary;
    m_cu->m_AvailBorder.m_top = !picTBoundary;

    if (!m_pPicParamSet->loop_filter_across_tiles_enabled_flag)
    {
        Ipp32u tileID = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr);

        if (picLBoundary)
            m_cu->m_AvailBorder.m_left = false;
        else
        {
            m_cu->m_AvailBorder.m_left = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - 1) == tileID ? true : false;
        }

        if (picTBoundary)
            m_cu->m_AvailBorder.m_top = false;
        else
        {
            m_cu->m_AvailBorder.m_top = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - numLCUInPicWidth) == tileID ? true : false;
        }
    }

    if (!m_cu->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag)
    {
        if (picLBoundary)
            m_cu->m_AvailBorder.m_left = false;
        else
        {
            if (m_cu->m_AvailBorder.m_left)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - 1);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = m_cu->m_SliceHeader->SliceCurStartCUAddr;

                m_cu->m_AvailBorder.m_left = refSA == SA ? true : false;
            }
        }

        if (picTBoundary)
            m_cu->m_AvailBorder.m_top = false;
        else
        {
            if (m_cu->m_AvailBorder.m_top)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - numLCUInPicWidth);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = m_cu->m_SliceHeader->SliceCurStartCUAddr;

                m_cu->m_AvailBorder.m_top = refSA == SA ? true : false;
            }
        }
    }

    bool isNeedAddHorDeblock = false;

    if ((Ipp32s)m_cu->m_CUPelX + width > frameWidthInSamples - 8)
    {
        isNeedAddHorDeblock = true;
    }
    else
    {
        if (m_bIsNeedWADeblocking)
        {
            Ipp32u width = frameWidthInSamples - m_cu->m_CUPelX;

            if (width > m_pSeqParamSet->MaxCUSize)
            {
                width = m_pSeqParamSet->MaxCUSize;
            }

            if (m_hasTiles)
            {
                H265CodingUnit* nextAddr = (m_cu->CUAddr + 1 >= (Ipp32u)m_pCurrentFrame->getCD()->m_NumCUsInFrame) ? 0 : m_pCurrentFrame->getCD()->getCU(m_cu->CUAddr + 1);
                if (!nextAddr || nextAddr->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                {
                    isNeedAddHorDeblock = true;
                }
            }
            else
            {
                if (m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(m_cu->CUAddr) == (Ipp32s)m_cu->m_SliceHeader->m_sliceSegmentCurEndCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                {
                    H265Slice * nextSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(m_cu->m_SliceIdx + 1);
                    if (!nextSlice || nextSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                    {
                        isNeedAddHorDeblock = true;
                    }
                }
            }
        }
    }

    m_pitch = m_pCurrentFrame->pitch_luma();
    m_pY_CU_Plane = m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr);
    m_pUV_CU_Plane = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr);

#if 1
    H265PartialEdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
    Ipp32s minSize = isMin4x4Block ? 4 : 8;
    
    for (int i = 0; i < (height >> 3); i++)
    {
        memset(ctb_start_edge, 0, sizeof(H265PartialEdgeData)*width / minSize);
        ctb_start_edge += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    DeblockCURecur(0, 0);
#endif

    for (Ipp32s j = 0; j < height; j += 8)
    {
        for (Ipp32s i = 0; i < width; i += 8)
        {
            DeblockOneCross(i, j, i == width - 8 ? isNeedAddHorDeblock : false);
        }
    }
}

// Calculate edge strength
template <Ipp32s direction, typename EdgeType>
void H265SegmentDecoder::CalculateEdge(EdgeType * edge, Ipp32s xPel, Ipp32s yPel, bool diffTr)
{
    Ipp32s tusize = m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s y = yPel >> tusize;
    Ipp32s x = xPel >> tusize;

    Ipp32s tuQAddr = y*m_pCurrentFrame->m_CodingData->m_NumPartInWidth + x;
    Ipp32s tuPAddr;

    bool anotherCU;
    H265CodingUnit* cuP = m_cu;

    if (direction == VERT_FILT)
    {
        tuPAddr = tuQAddr - 1;
        
        anotherCU = xPel == 0;
        if (anotherCU)
        {
            if (!m_cu->m_AvailBorder.m_left)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(m_cu->CUAddr - 1);
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
    }
    else
    {
        tuPAddr = tuQAddr - m_pCurrentFrame->m_CodingData->m_NumPartInWidth;

        anotherCU = yPel == 0;
        if (anotherCU)
        {
            if (!m_cu->m_AvailBorder.m_top)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(m_cu->CUAddr - m_pCurrentFrame->getFrameWidthInCU());
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartitions;
        }
    }

    tuPAddr = m_cu->m_rasterToZscan[tuPAddr];
    tuQAddr = m_cu->m_rasterToZscan[tuQAddr];

    H265CodingUnitData * tuP = cuP->GetCUData(tuPAddr);
    H265CodingUnitData * tuQ = m_cu->GetCUData(tuQAddr);

    edge->deblockQ = tuQ->cu_transform_bypass ? 0 : 1;
    edge->deblockP = tuP->cu_transform_bypass ? 0 : 1;

    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (tuQ->pcm_flag)
            edge->deblockQ = 0;

        if (tuP->pcm_flag)
            edge->deblockP = 0;
    }

    edge->qp = (tuQ->qp + tuP->qp + 1) >> 1;

    if (tuQ->predMode || tuP->predMode) // predMode > 0 - intra or out of frame bounds
    {
        edge->strength = (tuQ->predMode == 2 || tuP->predMode == 2) ? 0 : 2;  // predMode equals 2 means out of frame bounds !!
        return;
    }

    // Intra
    if (anotherCU || diffTr)
    {
        if (m_cu->GetCbf(COMPONENT_LUMA, tuQAddr, tuQ->trIndex) != 0 || cuP->GetCbf(COMPONENT_LUMA, tuPAddr, tuP->trIndex) != 0)
        {
            edge->strength = 1;
            return;
        }
    }

    Ipp32s PartX = (m_cu->m_CUPelX >> tusize) + x;
    Ipp32s PartY = (m_cu->m_CUPelY >> tusize) + y;

    H265MVInfo *mvinfoQ = &m_pCurrentFrame->getCD()->GetTUInfo(m_pSeqParamSet->NumPartitionsInFrameWidth * PartY + PartX);
    H265MVInfo *mvinfoP;

    if (direction == VERT_FILT)
    {
        mvinfoP = mvinfoQ - 1;
    }
    else
    {
        mvinfoP = mvinfoQ - m_pSeqParamSet->NumPartitionsInFrameWidth;
    }

    GetEdgeStrengthInter(mvinfoQ, mvinfoP, (H265PartialEdgeData*)edge);
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
