/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder.h"
#include "umc_h265_frame_info.h"

//#define NEW_DEBLOCKING

enum
{
    VERT_FILT,
    HOR_FILT
};

namespace UMC_HEVC_DECODER
{

static Ipp32s tcTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    14, 16, 18, 20, 22, 24
};

static Ipp32s betaTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64
};

#define   QpUV(iQpY)  ( ((iQpY) < 0) ? (iQpY) : (((iQpY) > 57) ? ((iQpY)-6) : g_ChromaScale[(iQpY)]) )

void H265SegmentDecoder::DeblockSegment(H265Task & task)
{
    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(task.m_iFirstMB);
    H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);

    if (NULL == pSlice)
        return;

    for (Ipp32s i = task.m_iFirstMB; i < task.m_iFirstMB + task.m_iMBToProcess; i++)
    {
        DeblockOneLCU(i);
    }
} // void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstMB, Ipp32s iNumMBs)

static Ipp8u H265_FORCEINLINE MVIsnotEq(const H265MotionVector &mv0, const H265MotionVector &mv1)
{
    //return abs(mv0.Horizontal - mv1.Horizontal) >= 4 || abs(mv0.Vertical - mv1.Vertical) >= 4;
    return (7 <= (Ipp32u)(mv0.Horizontal - mv1.Horizontal + 3) || 7 <= (Ipp32u)(mv0.Vertical - mv1.Vertical + 3)) ? 1 : 0;
}

void H265SegmentDecoder::GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265EdgeData *edge)
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
        if (((mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_0]) &&
            (mvinfoQ->m_pocDelta[REF_PIC_LIST_1] == mvinfoP->m_pocDelta[REF_PIC_LIST_1])) ||
            ((mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_1]) &&
            (mvinfoQ->m_pocDelta[REF_PIC_LIST_1] == mvinfoP->m_pocDelta[REF_PIC_LIST_0])))
        {
            if (mvinfoP->m_pocDelta[REF_PIC_LIST_0] != mvinfoP->m_pocDelta[REF_PIC_LIST_1])
            {
                if (mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_0])
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
        Ipp32s POCDeltaQ, POCDeltaP;
        H265MotionVector *MVQ, *MVP;
        
        if (mvinfoQ->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_0];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_1];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_1];
        }

        if (mvinfoP->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_0];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_1];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_1];
        }

        if (POCDeltaQ == POCDeltaP)
        {
            edge->strength = MVIsnotEq(*MVQ, *MVP);
            return;
        }

        edge->strength = 1;
        return;
    }
}

#ifndef NEW_DEBLOCKING

void H265SegmentDecoder::CleanLeftEdges(bool leftAvailable, H265EdgeData *ctb_start_edge, Ipp32s height)
{
    if (!leftAvailable)
    {
        for (Ipp32s j = 0; j < (height >> 3); j++)
        {
            VM_ASSERT(ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 0].strength == 3);
            VM_ASSERT(ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 1].strength == 3);

            ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 0].strength = 0;
            ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 1].strength = 0;
        }
    }
}

void H265SegmentDecoder::CleanTopEdges(bool topAvailable, H265EdgeData *ctb_start_edge, Ipp32s width)
{
    if (!topAvailable)
    {
        for (Ipp32s i = 0; i < (width >> 3); i++)
        {
            VM_ASSERT(ctb_start_edge[i * 4 + 4 + 2].strength == 3 || m_bIsNeedWADeblocking);
            VM_ASSERT(ctb_start_edge[i * 4 + 4 + 3].strength == 3 || m_bIsNeedWADeblocking);

            ctb_start_edge[i * 4 + 4 + 2].strength = 0;
            ctb_start_edge[i * 4 + 4 + 3].strength = 0;
        }
    }
}

void H265SegmentDecoder::CleanRightHorEdges(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;

    for (Ipp32u j = 0; j < (m_pSeqParamSet->MaxCUSize >> 3); j++)
    {
        ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + (m_pSeqParamSet->MaxCUSize >> 1) + 2].strength = 0;
        ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + (m_pSeqParamSet->MaxCUSize >> 1) + 3].strength = 0;
    }
}

void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr)
{
    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(curLCUAddr);
    m_pSliceHeader = curLCU->m_SliceHeader;

    if (curLCU->m_SliceHeader->slice_deblocking_filter_disabled_flag)
        return;

    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s width, height;
    Ipp32s i, j;
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    VM_ASSERT (!curLCU->m_SliceHeader->slice_deblocking_filter_disabled_flag || m_bIsNeedWADeblocking);

    width = frameWidthInSamples - curLCU->m_CUPelX;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    height = frameHeightInSamples - curLCU->m_CUPelY;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    bool picLBoundary = ((curLCU->m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((curLCU->m_CUPelY == 0) ? true : false);
    Ipp32s numLCUInPicWidth = m_pCurrentFrame->m_CodingData->m_WidthInCU;

    curLCU->m_AvailBorder.m_left = !picLBoundary;
    curLCU->m_AvailBorder.m_top = !picTBoundary;

    if (!curLCU->m_SliceHeader->m_PicParamSet->loop_filter_across_tiles_enabled_flag)
    {
        Ipp32u tileID = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr);

        if (picLBoundary)
            curLCU->m_AvailBorder.m_left = false;
        else
        {
            curLCU->m_AvailBorder.m_left = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - 1) == tileID ? true : false;
            CleanLeftEdges(curLCU->m_AvailBorder.m_left, ctb_start_edge, height);
        }

        if (picTBoundary)
            curLCU->m_AvailBorder.m_top = false;
        else
        {
            curLCU->m_AvailBorder.m_top = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - numLCUInPicWidth) == tileID ? true : false;
            CleanTopEdges(curLCU->m_AvailBorder.m_top, ctb_start_edge, width);
        }
    }

    if (!curLCU->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag)
    {
        if (picLBoundary)
            curLCU->m_AvailBorder.m_left = false;
        else
        {
            H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - 1);
            Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
            Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

            curLCU->m_AvailBorder.m_left = refSA == SA ? true : false;
            CleanLeftEdges(curLCU->m_AvailBorder.m_left, ctb_start_edge, height);
        }

        if (picTBoundary)
            curLCU->m_AvailBorder.m_top = false;
        else
        {
            H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - numLCUInPicWidth);
            Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
            Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

            curLCU->m_AvailBorder.m_top = refSA == SA ? true : false;
            CleanTopEdges(curLCU->m_AvailBorder.m_top, ctb_start_edge, width);
        }
    }

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
            DeblockOneCrossLuma(curLCU, i, j);
        }
    }

    for (j = 0; j < height; j += 16)
    {
        for (i = 0; i < width; i += 16)
        {
            DeblockOneCrossChroma(curLCU, i, j);
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossLuma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;

    Ipp32s srcDstStride = m_pCurrentFrame->pitch_luma();
    H265PlaneYCommon* baseSrcDst = m_pCurrentFrame->GetLumaAddr(curLCU->CUAddr);

    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    H265EdgeData *edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + 1) + 0;
    for (Ipp32s i = 0; i < 2; i++)
    {
        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<VERT_FILT>(m_pSeqParamSet->log2_min_transform_block_size,
            curLCU->m_CUPelX + curPixelColumn, curLCU->m_CUPelY + curPixelRow + 4 * i, edge);

        if (edge->strength > 0)
        {
            m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
        }

        if (m_pSeqParamSet->log2_min_transform_block_size < 3)
        {
            edge++;
        }
    }

    Ipp32s end = 2;
    if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 8)
    {
        end = 3;
    }

    if (m_bIsNeedWADeblocking)
    {
        Ipp32u width = frameWidthInSamples - curLCU->m_CUPelX;

        if (width > m_pSeqParamSet->MaxCUSize)
        {
            width = m_pSeqParamSet->MaxCUSize;
        }

        if (curPixelColumn == (Ipp32s)width - 8)
        {
            if (m_hasTiles)
            {
                H265CodingUnit* nextAddr = m_pCurrentFrame->m_CodingData->getCU(curLCU->CUAddr + 1);
                if (nextAddr && nextAddr->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                {
                    end = 3;
                }
            }
            else
            {
                if (m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(curLCU->CUAddr) == (Ipp32s)curLCU->m_SliceHeader->m_sliceSegmentCurEndCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                {
                    H265Slice * nextSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx + 1);
                    if (!nextSlice || nextSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                    {
                        end = 3;
                    }
                }
            }
        }
    }

    for (Ipp32s i = 0; i < end; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + ((i + 1) >> 1)) + (((i + 1) & 1) + 2);

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<HOR_FILT>(m_pSeqParamSet->log2_min_transform_block_size,
            curLCU->m_CUPelX + curPixelColumn + 4 * (i - 1), curLCU->m_CUPelY + curPixelRow, edge);

        if (edge->strength > 0)
        {
            m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn + 4 * (i - 1), curPixelRow, HOR_FILT, m_pSeqParamSet->bit_depth_luma);
        }

        if (m_pSeqParamSet->log2_min_transform_block_size >= 3 && i == 1)
        {
            *(edge+1) = *edge;
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossChroma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    H265EdgeData *edge;

    Ipp32s srcDstStride = m_pCurrentFrame->pitch_chroma();
    H265PlaneUVCommon*baseSrcDst = m_pCurrentFrame->GetCbCrAddr(curLCU->CUAddr);

    Ipp32s chromaCbQpOffset = m_pPicParamSet->pps_cb_qp_offset;
    Ipp32s chromaCrQpOffset = m_pPicParamSet->pps_cr_qp_offset;

    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    for (Ipp32s i = 0; i < 2; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (y + i) + 4 * (x + 1) + 0;

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<VERT_FILT>(m_pSeqParamSet->log2_min_transform_block_size,
            curLCU->m_CUPelX + curPixelColumn, curLCU->m_CUPelY + curPixelRow + 4 * i, edge);

        if (edge->strength > 1)
        {
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDst, srcDstStride, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * i, VERT_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }
    }

    Ipp32s end = 2;
    if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 16)
    {
        end = 3;
    }

    if (m_bIsNeedWADeblocking)
    {
        Ipp32u width = frameWidthInSamples - curLCU->m_CUPelX;

        if (width > m_pSeqParamSet->MaxCUSize)
        {
            width = m_pSeqParamSet->MaxCUSize;
        }
       
        if (curPixelColumn == (Ipp32s)width - 16)
        {
            if (m_hasTiles)
            {
                H265CodingUnit* nextAddr = m_pCurrentFrame->m_CodingData->getCU(curLCU->CUAddr + 1);
                if (nextAddr && nextAddr->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                {
                    end = 3;
                }
            }
            else
            {
                if (m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(curLCU->CUAddr) == (Ipp32s)curLCU->m_SliceHeader->m_sliceSegmentCurEndCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                {
                    H265Slice * nextSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx + 1);
                    if (!nextSlice || nextSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                    {
                        end = 3;
                    }
                }
            }
        }

    }

    for (Ipp32s i = 0; i < end; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + i) + 2;

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<HOR_FILT>(m_pSeqParamSet->log2_min_transform_block_size,
            curLCU->m_CUPelX + curPixelColumn + 4 * (i - 1), curLCU->m_CUPelY + curPixelRow, edge);

        if (edge->strength > 1)
        {
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDst, srcDstStride, (curPixelColumn>>1) + 4 * (i - 1), (curPixelRow >> 1), HOR_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }
    }
}

void H265SegmentDecoder::GetCTBEdgeStrengths(void)
{
    if (m_pCurrentFrame->m_pSlicesInfo->GetSliceCount() == 1 && !m_pPicParamSet->tiles_enabled_flag)
    {
        if (m_pSeqParamSet->log2_min_transform_block_size == 2)
            GetCTBEdgeStrengthsSimple<2>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 3)
            GetCTBEdgeStrengthsSimple<3>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 4)
            GetCTBEdgeStrengthsSimple<4>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 5)
            GetCTBEdgeStrengthsSimple<5>();
    }
    else
    {
        if (m_pSeqParamSet->log2_min_transform_block_size == 2)
            GetCTBEdgeStrengths<2>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 3)
            GetCTBEdgeStrengths<3>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 4)
            GetCTBEdgeStrengths<4>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 5)
            GetCTBEdgeStrengths<5>();
    }
}

template<Ipp32s tusize> void H265SegmentDecoder::GetCTBEdgeStrengths(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;
    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s xPel, yPel;

    Ipp32s height = m_pSeqParamSet->pic_height_in_luma_samples - m_curCU->m_CUPelY;
    if (height > maxCUSize)
        height = maxCUSize;

    Ipp32s width = m_pSeqParamSet->pic_width_in_luma_samples - m_curCU->m_CUPelX;
    if (width > maxCUSize)
        width = maxCUSize;

    Ipp32s curTULine = 0;
    H265EdgeData *edgeLine = ctb_start_edge;
    for (yPel = 0; yPel < height; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;
        curTULine = m_context->m_CurrCTBStride * (yPel >> tusize);

        if (m_curCU->m_CUPelX == 0)
        {
            edge[2].strength = edge[3].strength = 0;
        }

        for (xPel = 0; xPel < width; xPel += 8)
        {
            Ipp32s curTU = curTULine + (xPel >> tusize);
            Ipp32s leftTU, upTU;

            if (tusize <= 3)
            {
                leftTU = curTU - 1;
                upTU = curTU - m_context->m_CurrCTBStride;
            }
            else if (tusize == 4)
            {
                leftTU = curTU - ((xPel & 8) ? 0 : 1);
                upTU = curTU - ((yPel & 8) ? 0 : m_context->m_CurrCTBStride);
            }
            else
            {
                leftTU = curTU - ((xPel & 24) ? 0 : 1);
                upTU = curTU - ((yPel & 24) ? 0 : m_context->m_CurrCTBStride);
            }

            edge += 4;

            // Vertical edge
            GetEdgeStrength<tusize, VERT_FILT>(curTU, leftTU, edge, xPel, yPel);
            if (tusize < 3)
                GetEdgeStrength<tusize, VERT_FILT>(curTU + m_context->m_CurrCTBStride, leftTU + m_context->m_CurrCTBStride, edge + 1, xPel, yPel + 4);
            else
                edge[1] = edge[0];

            // Horizontal edge
            GetEdgeStrength<tusize, HOR_FILT>(curTU, upTU, edge + 2, xPel, yPel);
            if (tusize < 3)
                GetEdgeStrength<tusize, HOR_FILT>(curTU + 1, upTU + 1, edge + 3, xPel + 4, yPel);
            else
                edge[3] = edge[2];
        }

        for (; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    for (; yPel < maxCUSize; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;

        for (xPel = 0; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }
}

template<Ipp32s tusize, Ipp32s dir> void H265SegmentDecoder::GetEdgeStrength(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, Ipp32s xQ, Ipp32s yQ)
{
    H265FrameHLDNeighborsInfo infoQ = m_context->m_CurrCTBFlags[tuQ];
    H265FrameHLDNeighborsInfo infoP = m_context->m_CurrCTBFlags[tuP];

    edge->tcOffset = (Ipp8s)m_pSliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_pSliceHeader->slice_beta_offset;

    bool anotherCU;
    H265EdgeData *next_edge;
    if (dir == VERT_FILT)
    {
        anotherCU = (xQ == 0);
        next_edge = edge + 4;
    }
    else
    {
        anotherCU = (yQ == 0);
        next_edge = edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    edge->deblockQ = infoQ.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoQ.members.IsIPCM)
            edge->deblockQ = 0;
    }

    H265FrameHLDNeighborsInfo save;
    if (tusize < 3)
    {
        if (dir == VERT_FILT)
            save = m_context->m_CurrCTBFlags[tuQ + 1];
        else
            save = m_context->m_CurrCTBFlags[tuQ + m_context->m_CurrCTBStride];
    }
    else
        save = m_context->m_CurrCTBFlags[tuQ];

    next_edge->isIntraP = save.members.IsIntra;
    next_edge->isTrCbfYP = save.members.IsTrCbfY;
    next_edge->deblockP = edge->deblockQ;
    next_edge->qpP = save.members.qp;

    if (tusize == 2 || tusize == 3)
    {
        if (!infoP.members.IsAvailable)
        {
            // Another slice or tile case. May need a delayed strength calculation
            if (!anotherCU)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            if (dir == VERT_FILT)
            {
                if (m_curCU->m_CUPelX + xQ == 0)
                {
                    edge->strength = 0;
                    return;
                }
            }
            else
            {
                if (m_curCU->m_CUPelY + yQ == 0)
                {
                    edge->strength = 0;
                    return;
                }
            }

            // delayed calculation
            edge->qp = infoQ.members.qp;
            edge->isIntraQ = infoQ.members.IsIntra;
            edge->isTrCbfYQ = infoQ.members.IsTrCbfY;
            edge->strength = 3;
            return;
        }
    }
    else
    {
        if (!infoP.members.IsAvailable && anotherCU)
        {
            // Another slice or tile case. May need a delayed strength calculation
            if (dir == VERT_FILT)
            {
                if (m_curCU->m_CUPelX + xQ == 0)
                {
                    edge->strength = 0;
                    return;
                }
            }
            else
            {
                if (m_curCU->m_CUPelY + yQ == 0)
                {
                    edge->strength = 0;
                    return;
                }
            }

            // delayed calculation
            edge->qp = infoQ.members.qp;
            edge->isIntraQ = infoQ.members.IsIntra;
            edge->isTrCbfYQ = infoQ.members.IsTrCbfY;
            edge->strength = 3;
            return;
        }
    }

    edge->qp = (infoQ.members.qp + edge->qpP + 1) >> 1;

    // Intra
    if (anotherCU || infoQ.members.TrStart != infoP.members.TrStart)
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 2;
            return;
        }

        if (infoQ.members.IsTrCbfY || infoP.members.IsTrCbfY)
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 0;
            return;
        }
    }

    H265MVInfo *mvinfoQ = m_context->m_CurrCTB + tuQ;
    H265MVInfo *mvinfoP = m_context->m_CurrCTB + tuP;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

template<Ipp32s dir> void H265SegmentDecoder::GetEdgeStrengthDelayed(Ipp32s tusize, Ipp32s x, Ipp32s y, H265EdgeData *edge)
{
    edge->qp = (edge->qp + edge->qpP + 1) >> 1;

    if (edge->isIntraQ || edge->isIntraP)
    {
        edge->strength = 2;
        return;
    }
    else if (edge->isTrCbfYQ || edge->isTrCbfYP)
    {
        edge->strength = 1;
        return;
    }

    x >>= tusize;
    y >>= tusize;

    H265MVInfo *mvinfoQ = m_pCurrentFrame->m_CodingData->m_colocatedInfo + y * m_pSeqParamSet->NumPartitionsInFrameWidth + x;

    if (dir == VERT_FILT)
    {
        x -= 1;
        VM_ASSERT(x >= 0);
    }
    else
    {
        y -= 1;
        VM_ASSERT(y >= 0);
    }

    H265MVInfo *mvinfoP = m_pCurrentFrame->m_CodingData->m_colocatedInfo + y * m_pSeqParamSet->NumPartitionsInFrameWidth + x;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

template<Ipp32s tusize> void H265SegmentDecoder::GetCTBEdgeStrengthsSimple(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;
    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s xPel, yPel;

    Ipp32s height = m_pSeqParamSet->pic_height_in_luma_samples - m_curCU->m_CUPelY;
    if (height > maxCUSize)
        height = maxCUSize;

    Ipp32s width = m_pSeqParamSet->pic_width_in_luma_samples - m_curCU->m_CUPelX;
    if (width > maxCUSize)
        width = maxCUSize;

    Ipp32s curTULine = 0;
    H265EdgeData *edgeLine = ctb_start_edge;
    for (yPel = 0; yPel < height; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;
        curTULine = m_context->m_CurrCTBStride * (yPel >> tusize);

        if (m_curCU->m_CUPelX == 0)
        {
            edge[2].strength = edge[3].strength = 0;
        }

        for (xPel = 0; xPel < width; xPel += 8)
        {
            Ipp32s curTU = curTULine + (xPel >> tusize);
            Ipp32s leftTU, upTU;

            if (tusize <= 3)
            {
                leftTU = curTU - 1;
                upTU = curTU - m_context->m_CurrCTBStride;
            }
            else if (tusize == 4)
            {
                leftTU = curTU - ((xPel & 8) ? 0 : 1);
                upTU = curTU - ((yPel & 8) ? 0 : m_context->m_CurrCTBStride);
            }
            else
            {
                leftTU = curTU - ((xPel & 24) ? 0 : 1);
                upTU = curTU - ((yPel & 24) ? 0 : m_context->m_CurrCTBStride);
            }

            edge += 4;

            // Vertical edge
            bool anotherCU = (xPel == 0);
            GetEdgeStrengthSimple<tusize>(curTU, leftTU, edge, anotherCU);
            if (tusize < 3)
                GetEdgeStrengthSimple<tusize>(curTU + m_context->m_CurrCTBStride, leftTU + m_context->m_CurrCTBStride, edge + 1, anotherCU);
            else
                edge[1] = edge[0];

            // Horizontal edge
            anotherCU = (yPel == 0);
            GetEdgeStrengthSimple<tusize>(curTU, upTU, edge + 2, anotherCU);
            if (tusize < 3)
                GetEdgeStrengthSimple<tusize>(curTU + 1, upTU + 1, edge + 3, anotherCU);
            else
                edge[3] = edge[2];
        }

        for (; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    for (; yPel < maxCUSize; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;

        for (xPel = 0; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }
}

template<Ipp32s tusize> void H265SegmentDecoder::GetEdgeStrengthSimple(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, bool anotherCU)
{
    H265FrameHLDNeighborsInfo infoQ = m_context->m_CurrCTBFlags[tuQ];
    H265FrameHLDNeighborsInfo infoP = m_context->m_CurrCTBFlags[tuP];

    edge->tcOffset = (Ipp8s)m_pSliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_pSliceHeader->slice_beta_offset;

    edge->deblockQ = infoQ.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoQ.members.IsIPCM)
            edge->deblockQ = 0;
    }
    edge->deblockP = infoP.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoP.members.IsIPCM)
            edge->deblockP = 0;
    }

    if (tusize == 2 || tusize == 3)
    {
        if (!infoP.members.IsAvailable)
        {
            // Left or top edge of frame
            VM_ASSERT(anotherCU);
            edge->strength = 0;
            return;
        }
    }
    else
    {
        if (!infoP.members.IsAvailable && anotherCU)
        {
            // Left or top edge of frame
            edge->strength = 0;
            return;
        }
    }

    edge->qp = (infoQ.members.qp + infoP.members.qp + 1) >> 1;

    // Intra
    if (anotherCU || infoQ.members.TrStart != infoP.members.TrStart)
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 2;
            return;
        }

        if (infoQ.members.IsTrCbfY || infoP.members.IsTrCbfY)
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 0;
            return;
        }
    }

    H265MVInfo *mvinfoQ = m_context->m_CurrCTB + tuQ;
    H265MVInfo *mvinfoP = m_context->m_CurrCTB + tuP;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

#else

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

void H265SegmentDecoder::DeblockOneLCU(H265CodingUnit* cu, Ipp32u absPartIdx, Ipp32u depth, Ipp32s edgeType)
{
    if (cu->GetDepth(absPartIdx) > depth)
    {
        depth++;
        Ipp32u PartOffset = cu->m_NumPartition >> (depth << 1);
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += PartOffset)
        {
            DeblockOneLCU(cu, absPartIdx, depth, edgeType);
        }
        return;
    }

    Ipp32s countPart = GetNumPartForVertFilt(cu, absPartIdx);
    Ipp32u PUOffset = (g_PUOffset[cu->GetPartitionSize(absPartIdx)] << ((m_context->m_sps->MaxCUDepth - depth) << 1)) >> 4;

    H265EdgeData edge1;
    H265EdgeData *edge = &edge1;

    for (Ipp32s PartIdx = 0, subPartIdx = absPartIdx; PartIdx < countPart; PartIdx++, subPartIdx += PUOffset)
    {
        Ipp32u partAddr;
        Ipp32u width;
        Ipp32u height;

        cu->getPartIndexAndSize(absPartIdx, depth, PartIdx, partAddr, width, height);

        //Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;

        Ipp32u curPixelColumn = cu->m_rasterToPelX[subPartIdx];
        Ipp32u curPixelRow = cu->m_rasterToPelY[subPartIdx];

        Ipp32s LPelX = cu->m_CUPelX + curPixelColumn;
        Ipp32s TPelY = cu->m_CUPelY + curPixelRow;

        Ipp32s x = curPixelColumn >> 3;
        Ipp32s y = curPixelRow >> 3;

        Ipp32s srcDstStride = m_pCurrentFrame->pitch_luma();
        H265PlaneYCommon* baseSrcDst = m_pCurrentFrame->GetLumaAddr(cu->CUAddr);

        Ipp32s srcDstStrideChroma = m_pCurrentFrame->pitch_chroma();
        H265PlaneYCommon* baseSrcDstChroma = m_pCurrentFrame->GetCbCrAddr(cu->CUAddr);

        for (Ipp32u i = 0; i < height / 4; i++)
        {
            CalculateEdge<VERT_FILT>(cu, edge, curPixelColumn, curPixelRow + 4 * i);

            if (edge->strength > 0)
            {
                m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
            }

            if (edge->strength > 1 && i == 0 && !(curPixelColumn % 16))
            {
                m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * i, VERT_FILT, m_pPicParamSet->pps_cb_qp_offset, m_pPicParamSet->pps_cr_qp_offset, m_pSeqParamSet->bit_depth_chroma);
            }
        }
    }
}

void H265SegmentDecoder::DeblockOneCross(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow, bool isNeedAddHorDeblock)
{
    Ipp32s srcDstStride = m_pCurrentFrame->pitch_luma();
    H265PlaneYCommon* baseSrcDst = m_pCurrentFrame->GetLumaAddr(curLCU->CUAddr);

    Ipp32s srcDstStrideChroma = m_pCurrentFrame->pitch_chroma();
    H265PlaneUVCommon*baseSrcDstChroma = m_pCurrentFrame->GetCbCrAddr(curLCU->CUAddr);

    Ipp32s chromaCbQpOffset = m_pPicParamSet->pps_cb_qp_offset;
    Ipp32s chromaCrQpOffset = m_pPicParamSet->pps_cr_qp_offset;

    H265EdgeData edgeForCheck;
    H265EdgeData *edge = &edgeForCheck;
    edge->tcOffset = (Ipp8s)curLCU->m_SliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)curLCU->m_SliceHeader->slice_beta_offset;

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
    }

    Ipp32s end = isNeedAddHorDeblock ? 3 : 2;
    for (Ipp32s i = 0; i < end; i++)
    {
        Ipp32s currPixelColumnTemp = curPixelColumn + 4 * (i - 1);
        H265CodingUnit* curLCUTemp = curLCU;

        if (currPixelColumnTemp < 0)
        {
            currPixelColumnTemp += m_pSeqParamSet->MaxCUSize;

            if (curLCU->m_CUPelX)
            {
                curLCUTemp = m_pCurrentFrame->getCU(curLCU->CUAddr - 1);
                if (curLCUTemp->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                    continue;

                edge->tcOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_tc_offset;
                edge->betaOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_beta_offset;
            }
            else
                continue;
        }

        CalculateEdge<HOR_FILT>(curLCUTemp, edge, currPixelColumnTemp, curPixelRow);

        if (edge->strength > 0)
        {
            m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn + 4 * (i - 1), curPixelRow, HOR_FILT, m_pSeqParamSet->bit_depth_luma);
        }

        if (edge->strength > 1 && i != 1 && !(curPixelRow % 16))
        {
            Ipp32s column = (curPixelColumn>>1) + 4 * (i - 1);
            if (i == 2)
                column = (curPixelColumn>>1) + 4 * (i - 2);
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, column, (curPixelRow >> 1), HOR_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }

        if (curPixelColumn + 4 * (i - 1) < 0)
        {
            // restore offsets
            edge->tcOffset = (Ipp8s)curLCU->m_SliceHeader->slice_tc_offset;
            edge->betaOffset = (Ipp8s)curLCU->m_SliceHeader->slice_beta_offset;
        }
    }
}

void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr)
{
    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(curLCUAddr);
    m_pSliceHeader = curLCU->m_SliceHeader;

    if (curLCU->m_SliceHeader->slice_deblocking_filter_disabled_flag)
        return;

    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;

    VM_ASSERT (!curLCU->m_SliceHeader->slice_deblocking_filter_disabled_flag || m_bIsNeedWADeblocking);

    Ipp32s width = frameWidthInSamples - curLCU->m_CUPelX;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    Ipp32s height = frameHeightInSamples - curLCU->m_CUPelY;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    bool picLBoundary = ((curLCU->m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((curLCU->m_CUPelY == 0) ? true : false);
    Ipp32s numLCUInPicWidth = m_pCurrentFrame->m_CodingData->m_WidthInCU;

    curLCU->m_AvailBorder.m_left = !picLBoundary;
    curLCU->m_AvailBorder.m_top = !picTBoundary;

    if (!curLCU->m_SliceHeader->m_PicParamSet->loop_filter_across_tiles_enabled_flag)
    {
        Ipp32u tileID = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr);

        if (picLBoundary)
            curLCU->m_AvailBorder.m_left = false;
        else
        {
            curLCU->m_AvailBorder.m_left = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - 1) == tileID ? true : false;
        }

        if (picTBoundary)
            curLCU->m_AvailBorder.m_top = false;
        else
        {
            curLCU->m_AvailBorder.m_top = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - numLCUInPicWidth) == tileID ? true : false;
        }
    }

    if (!curLCU->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag)
    {
        if (picLBoundary)
            curLCU->m_AvailBorder.m_left = false;
        else
        {
            if (curLCU->m_AvailBorder.m_left)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - 1);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

                curLCU->m_AvailBorder.m_left = refSA == SA ? true : false;
            }
        }

        if (picTBoundary)
            curLCU->m_AvailBorder.m_top = false;
        else
        {
            if (curLCU->m_AvailBorder.m_top)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - numLCUInPicWidth);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

                curLCU->m_AvailBorder.m_top = refSA == SA ? true : false;
            }
        }
    }

#if 0
    DeblockOneLCU(curLCU, 0, 0, VERT_FILT);
#endif

    bool isNeddAddHorDeblock = false;

    if ((Ipp32s)curLCU->m_CUPelX + width >= frameWidthInSamples - 8)
    {
        isNeddAddHorDeblock = true;
    }

    if (m_bIsNeedWADeblocking && !isNeddAddHorDeblock)
    {
        Ipp32u width = frameWidthInSamples - curLCU->m_CUPelX;

        if (width > m_pSeqParamSet->MaxCUSize)
        {
            width = m_pSeqParamSet->MaxCUSize;
        }

        if (m_hasTiles)
        {
            H265CodingUnit* nextAddr = m_pCurrentFrame->m_CodingData->getCU(curLCU->CUAddr + 1);
            if (nextAddr && nextAddr->m_SliceHeader->slice_deblocking_filter_disabled_flag)
            {
                isNeddAddHorDeblock = true;
            }
        }
        else
        {
            if (m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(curLCU->CUAddr) == (Ipp32s)curLCU->m_SliceHeader->m_sliceSegmentCurEndCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
            {
                H265Slice * nextSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx + 1);
                if (!nextSlice || nextSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                {
                    isNeddAddHorDeblock = true;
                }
            }
        }
    }

    for (Ipp32s j = 0; j < height; j += 8)
    {
        for (Ipp32s i = 0; i < width; i += 8)
        {
            DeblockOneCross(curLCU, i, j, i == width - 8 ? isNeddAddHorDeblock : false);
        }
    }
}

template <Ipp32s direction>
void H265SegmentDecoder::CalculateEdge(H265CodingUnit* cu, H265EdgeData * edge, Ipp32s xPel, Ipp32s yPel)
{
    Ipp32s tusize = m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s y = yPel >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s x = xPel >> m_pSeqParamSet->log2_min_transform_block_size;

    Ipp32s tuPAddr = y*m_pCurrentFrame->m_CodingData->m_NumPartInWidth + x;
    Ipp32s tuQAddr = y*m_pCurrentFrame->m_CodingData->m_NumPartInWidth + x;

    bool anotherCU;
    H265CodingUnit* cuP = cu;

    if (direction == VERT_FILT)
    {
        if (tusize <= 3)
        {
            tuPAddr -= 1;
        }
        else if (tusize == 4)
        {
            tuPAddr -= (xPel & 8) ? 0 : 1;
        }
        else
        {
            tuPAddr -= (xPel & 24) ? 0 : 1;
        }
        
        anotherCU = xPel == 0;
        if (anotherCU)
        {
            if (!cu->m_AvailBorder.m_left)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(cu->CUAddr - 1);
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
    }
    else
    {
        if (tusize <= 3)
        {
            tuPAddr -= m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
        else if (tusize == 4)
        {
            tuPAddr -= (yPel & 8) ? 0 : m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
        else
        {
            tuPAddr -= (yPel & 24) ? 0 : m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }

        anotherCU = yPel == 0;
        if (anotherCU)
        {
            if (!cu->m_AvailBorder.m_top)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(cu->CUAddr - m_pCurrentFrame->getFrameWidthInCU());
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartitions;
        }
    }

    if (tuPAddr == tuQAddr)
    {
        edge->strength = 0;
        return;
    }

    tuPAddr = cu->m_rasterToZscan[tuPAddr];
    tuQAddr = cu->m_rasterToZscan[tuQAddr];

    H265CodingUnitData * tuP = cuP->GetCUData(tuPAddr);
    H265CodingUnitData * tuQ = cu->GetCUData(tuQAddr);

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

    // Intra
    if (anotherCU || tuQ->trStart != tuP->trStart || tuQ->trIndex != tuP->trIndex)
    {
        if (tuQ->predMode || tuP->predMode) // predMode > 0 - intra
        {
            edge->strength = 2;
            return;
        }

        if (cu->GetCbf(COMPONENT_LUMA, tuQ->trStart, tuQ->trIndex) != 0 || cuP->GetCbf(COMPONENT_LUMA, tuP->trStart, tuP->trIndex) != 0)
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (tuQ->predMode || tuP->predMode) // predMode > 0 - intra
        {
            edge->strength = 0;
            return;
        }
    }

    Ipp32s PartX = (cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + x;
    Ipp32s PartY = (cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + y;

    H265MVInfo *mvinfoQ = &m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX);
    H265MVInfo *mvinfoP;

    if (direction == VERT_FILT)
    {
        mvinfoP = &(m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX - 1));
    }
    else
    {
        mvinfoP = &(m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * (PartY - 1) + PartX));
    }

    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

void H265SegmentDecoder::CleanRightHorEdges()
{

}

void H265SegmentDecoder::GetCTBEdgeStrengths()
{
}

#endif


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
