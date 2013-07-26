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

#include "umc_h265_segment_decoder.h"
#include "umc_h265_frame_info.h"

#define VERT_FILT 0
#define HOR_FILT  1

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

void H265SegmentDecoder::DeblockFrame(Ipp32s iFirstMB, Ipp32s iNumMBs)
{
    Ipp32s i;

    for (i = iFirstMB; i < iFirstMB + iNumMBs; i++)
    {
        H265CodingUnit* curLCU = m_pCurrentFrame->getCU(i);
        H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);

        if (NULL == pSlice)
            continue;

        DeblockOneLCU(i, 1);
    }

} // void H265SegmentDecoder::DeblockFrame(Ipp32s uFirstMB, Ipp32s uNumMBs)

void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstCU, Ipp32s nBorder)
{

    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(iFirstCU);
    H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);
    Ipp32s i;

    if (NULL == pSlice)
        return;

    for (i = iFirstCU; i < nBorder; i++)
    {
        DeblockOneLCU(i, 0);
    }
} // void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstMB, Ipp32s iNumMBs)

void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr,
                                       Ipp32s cross)
{
    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(curLCUAddr);
    H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);
    Ipp32s maxCUSize = m_pSeqParamSet->getMaxCUWidth();
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s width, height;
    Ipp32s i, j;

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

#ifdef _DEBUG
    if (curLCU->m_CUPelX == 0)
    {
        for (j = 0; j < 9; j++)
            for (i = 0; i < 9; i++)
                for (Ipp32s e = 0; e < 4; e++)
                    m_edge[j][i][e].strength = 100;
    }
    else
    {
        for (j = 0; j < maxCUSize >> 3; j++)
            for (i = 0; i < maxCUSize >> 3; i++)
                for (Ipp32s e = 0; e < 4; e++)
                    m_edge[j][i][e].strength = 100;
    }
#endif

    if (pSlice->getDeblockingFilterDisable())
    {
        for (j = 0; j < (height >> 3); j++)
        {
            m_edge[j+1][maxCUSize >> 3][2].strength = 0;
            m_edge[j+1][maxCUSize >> 3][3].strength = 0;
        }

        SetEdges(curLCU, width, height, cross, 0);

        /* Deblock Other LCU */

        for (i = 0; i < width; i += 8)
        {
            DeblockOneCrossLuma(curLCU, i, 0, 1, 0, cross);
        }

        for (j = 0; j < height; j += 8)
        {
            DeblockOneCrossLuma(curLCU, 0, j, 0, 1, cross);
        }

        for (i = 0; i < width; i += 16)
        {
            DeblockOneCrossChroma(curLCU, i, 0, 1, 0, cross);
        }

        for (j = 0; j < height; j += 16)
        {
            DeblockOneCrossChroma(curLCU, 0, j, 0, 1, cross);
        }

        return;

    }

    SetEdges(curLCU, width, height, cross, 1);

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
            DeblockOneCrossLuma(curLCU, i, j, 0, 0, cross);
        }
    }

    for (j = 0; j < height; j += 16)
    {
        for (i = 0; i < width; i += 16)
        {
            DeblockOneCrossChroma(curLCU, i, j, 0, 0, cross);
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossLuma(H265CodingUnit* curLCU,
                                             Ipp32s curPixelColumn,
                                             Ipp32s curPixelRow,
                                             Ipp32s onlyOneUp,
                                             Ipp32s onlyOneLeft,
                                             Ipp32s cross)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    H265PlaneYCommon* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    if (onlyOneUp && !cross)
    {
        return;
    }

    srcDstStride = m_pCurrentFrame->pitch_luma();
    baseSrcDst = m_pCurrentFrame->GetLumaAddr(curLCU->CUAddr) +
                 curPixelRow * srcDstStride + curPixelColumn;

    if (!onlyOneLeft)
    {
        if (onlyOneUp)
        {
            start = 0;
            end = 1;
        }
        else
        {
            if (cross)
            {
                start = 0;
                end = 2;
                if ((Ipp32s)curLCU->m_CUPelY + curPixelRow >= frameHeightInSamples - 8)
                {
                    end = 3;
                }
            }
            else
            {
                start = 1;
                end = 3;
            }
        }

        for (i = start; i < end; i++)
        {
            edge = &m_edge[y + ((i + 1) >> 1)][x + 1][(i + 1) & 1];

            VM_ASSERT(edge->strength < 3);
            if (edge->strength > 0)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeLuma_8u_I(edge, baseSrcDst + 4 * (i - 1) * srcDstStride,
                               srcDstStride, VERT_FILT);
            }
        }
    }

    if (!onlyOneUp)
    {
        if (onlyOneLeft)
        {
            end = 1;
        }
        else
        {
            end = 2;
            if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 8)
            {
                end = 3;
            }
        }

        for (i = 0; i < end; i++)
        {
            edge = &m_edge[y + 1][x + ((i + 1) >> 1)][((i + 1) & 1) + 2];

            VM_ASSERT(edge->strength < 3);
            if (edge->strength > 0)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeLuma_8u_I(edge, baseSrcDst + 4 * (i - 1),
                               srcDstStride, HOR_FILT);
            }
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossChroma(H265CodingUnit* curLCU,
                                               Ipp32s curPixelColumn,
                                               Ipp32s curPixelRow,
                                               Ipp32s onlyOneUp,
                                               Ipp32s onlyOneLeft,
                                               Ipp32s cross)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    H265PlaneUVCommon* baseSrcDst;
    Ipp32s srcDstStride;
    Ipp32s chromaCbQpOffset, chromaCrQpOffset;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    if (onlyOneUp && !cross)
    {
        return;
    }

    srcDstStride = m_pCurrentFrame->pitch_chroma();
    baseSrcDst = m_pCurrentFrame->GetCbCrAddr(curLCU->CUAddr) +
                 (curPixelRow >> 1) * srcDstStride + (curPixelColumn >> 1) * 2;

    chromaCbQpOffset = m_pPicParamSet->getChromaCbQpOffset();
    chromaCrQpOffset = m_pPicParamSet->getChromaCrQpOffset();

    if (!onlyOneLeft)
    {
        if (onlyOneUp)
        {
            start = 0;
            end = 1;
        }
        else
        {
            if (cross)
            {
                start = 0;
                end = 2;
                if ((Ipp32s)curLCU->m_CUPelY + curPixelRow >= frameHeightInSamples - 16)
                {
                    end = 3;
                }
            }
            else
            {
                start = 1;
                end = 3;
            }
        }

        for (i = start; i < end; i++)
        {
            edge = &m_edge[y + i][x + 1][0];

            VM_ASSERT(edge->strength < 3);
            if (edge->strength > 1)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeChroma_Interleaved_8u_I(
                    edge, 
                    baseSrcDst + 4 * (i - 1) * srcDstStride,
                    srcDstStride, 
                    chromaCbQpOffset, 
                    chromaCrQpOffset, 
                    VERT_FILT,
                    QpUV(edge->qp + chromaCbQpOffset),
                    QpUV(edge->qp + chromaCrQpOffset));
            }
        }
    }

    if (!onlyOneUp)
    {
        if (onlyOneLeft)
        {
            end = 1;
        }
        else
        {
            end = 2;
            if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 16)
            {
                end = 3;
            }
        }

        for (i = 0; i < end; i++)
        {
            edge = &m_edge[y + 1][x + i][2];

            VM_ASSERT(edge->strength < 3);
            if (edge->strength > 1)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeChroma_Interleaved_8u_I(
                    edge, 
                    baseSrcDst + 4 * (i - 1) * 2,
                    srcDstStride, 
                    chromaCbQpOffset, 
                    chromaCrQpOffset, 
                    HOR_FILT,
                    QpUV(edge->qp + chromaCbQpOffset),
                    QpUV(edge->qp + chromaCrQpOffset));
            }
        }
    }
}


static bool H265_FORCEINLINE MVIsnotEq(H265MotionVector mv0,
                      H265MotionVector mv1)
{
    H265MotionVector mv = mv0 - mv1;

    if ((mv.getAbsHor() >= 4) || (mv.getAbsVer() >= 4))
    {
        return true;
    }
    else
    {
        return false;
    }
}

#if (HEVC_OPT_CHANGES & 32)
template< Ipp32s dir>
void H265SegmentDecoder::GetEdgeStrength(H265CodingUnit* pcCUQ,
                                         H265EdgeData *edge,
                                         Ipp32s curPixelColumn,
                                         Ipp32s curPixelRow,
                                         Ipp32s crossSliceBoundaryFlag,
                                         Ipp32s crossTileBoundaryFlag,
                                         Ipp32s tcOffset,
                                         Ipp32s betaOffset)
#else
void H265SegmentDecoder::GetEdgeStrength(H265CodingUnit* pcCUQ,
                                         H265EdgeData *edge,
                                         Ipp32s curPixelColumn,
                                         Ipp32s curPixelRow,
                                         Ipp32s crossSliceBoundaryFlag,
                                         Ipp32s crossTileBoundaryFlag,
                                         Ipp32s tcOffset,
                                         Ipp32s betaOffset,
                                         Ipp32s dir)
#endif
{
    Ipp32s log2LCUSize = g_ConvertToBit[m_pSeqParamSet->getMaxCUWidth()] + 2;
    Ipp32s log2MinTUSize = m_pSeqParamSet->log2_min_transform_block_size;
    H265CodingUnit* pcCUP;
    Ipp32u uiPartQ, uiPartP;

    pcCUQ->m_AbsIdxInLCU = 0;

    edge->strength = 0;

    uiPartQ = pcCUQ->m_rasterToZscan[((curPixelRow >> log2MinTUSize) << (log2LCUSize - log2MinTUSize)) + (curPixelColumn >> log2MinTUSize)];

    if (dir == HOR_FILT)
    {
        if ((curPixelRow >> log2MinTUSize) == ((curPixelRow - 1) >> log2MinTUSize))
        {
            return;
        }
        pcCUP = pcCUQ->getPUAbove(uiPartP, uiPartQ, !crossSliceBoundaryFlag, false, !crossTileBoundaryFlag);
    }
    else
    {
        if ((curPixelColumn >> log2MinTUSize) == ((curPixelColumn - 1) >> log2MinTUSize))
        {
            return;
        }
        pcCUP = pcCUQ->getPULeft(uiPartP, uiPartQ, !crossSliceBoundaryFlag, !crossTileBoundaryFlag);
    }

    if (pcCUP == NULL)
    {
        return;
    }

    edge->deblockP = 1;
    edge->deblockQ = 1;

    edge->tcOffset = (Ipp8s)tcOffset;
    edge->betaOffset = (Ipp8s)betaOffset;

    if ((pcCUP->GetIPCMFlag(uiPartP) != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
    {
        edge->deblockP = 0;
    }

    if ((pcCUQ->GetIPCMFlag(uiPartQ) != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
    {
        edge->deblockQ = 0;
    }

    if ((pcCUP->m_CUTransquantBypass[uiPartP] != 0))
    {
        edge->deblockP = 0;
    }

    if ((pcCUQ->m_CUTransquantBypass[uiPartQ] != 0))
    {
        edge->deblockQ = 0;
    }

    edge->qp = (pcCUQ->GetQP(uiPartQ) + pcCUP->GetQP(uiPartP) + 1) >> 1;

    // TODO: Use raster coordinates passed as arguments to this function
    // P coordinate
    Ipp32u XIncP = pcCUP->m_rasterToPelX[uiPartP];
    Ipp32u YIncP = pcCUP->m_rasterToPelY[uiPartP];
    Ipp32s LPelXP = pcCUP->m_CUPelX + XIncP;
    Ipp32s TPelYP = pcCUP->m_CUPelY + YIncP;
    Ipp32s PartXP = LPelXP >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartYP = TPelYP >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32u PartNumberP = PartYP * m_pCurrentFrame->getNumPartInCUSize() * m_pCurrentFrame->getFrameWidthInCU() + PartXP;

    // Q coordinate
    Ipp32u XIncQ = pcCUP->m_rasterToPelX[uiPartQ];
    Ipp32u YIncQ = pcCUP->m_rasterToPelY[uiPartQ];
    Ipp32s LPelXQ = pcCUQ->m_CUPelX + XIncQ;
    Ipp32s TPelYQ = pcCUQ->m_CUPelY + YIncQ;
    Ipp32s PartXQ = LPelXQ >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s PartYQ = TPelYQ >> m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32u PartNumberQ = PartYQ * m_pCurrentFrame->getNumPartInCUSize() * m_pCurrentFrame->getFrameWidthInCU() + PartXQ;

    // Intra CUs
    if ((pcCUQ->CUAddr != pcCUP->CUAddr)||
        (pcCUQ->m_TrStartArray[uiPartQ] != pcCUP->m_TrStartArray[uiPartP]))
    {
        if (COL_TU_INTRA == m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberP) ||
            COL_TU_INTRA == m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberQ))
        {
            edge->strength = 2;
            return;
        }

        if (((pcCUQ->GetCbf(0, uiPartQ) >> pcCUQ->m_TrIdxArray[uiPartQ]) != 0) ||
            ((pcCUP->GetCbf(0, uiPartP) >> pcCUP->m_TrIdxArray[uiPartP]) != 0))
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (COL_TU_INTRA == m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberP) ||
            COL_TU_INTRA == m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberQ))
        {
            edge->strength = 0;
            return;
        }
    }

    // Inter CUs
    /* Check MV */
    {
        Ipp32s numRefsP, numRefsQ;
        Ipp32s i;

        numRefsP = 0;

        for (i = 0; i < 2; i++)
        {
            if (COL_TU_INVALID_INTER != m_pCurrentFrame->m_CodingData->GetTUFlags((EnumRefPicList)i, PartNumberP))
            {
                numRefsP++;
            }
        }

        numRefsQ = 0;

        for (i = 0; i < 2; i++)
        {
            if (COL_TU_INVALID_INTER != m_pCurrentFrame->m_CodingData->GetTUFlags((EnumRefPicList)i, PartNumberQ))
            {
                numRefsQ++;
            }
        }

        if (numRefsP != numRefsQ)
        {
            edge->strength = 1;
            return;
        }

        if (numRefsQ == 2)
        {
            // Two references in both CUs
            Ipp32s RefPOCP0 = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(REF_PIC_LIST_0, PartNumberP);
            Ipp32s RefPOCP1 = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(REF_PIC_LIST_1, PartNumberP);
            H265MotionVector & MVP0 = m_pCurrentFrame->m_CodingData->GetMV(REF_PIC_LIST_0, PartNumberP);
            H265MotionVector & MVP1 = m_pCurrentFrame->m_CodingData->GetMV(REF_PIC_LIST_1, PartNumberP);

            Ipp32s RefPOCQ0 = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(REF_PIC_LIST_0, PartNumberQ);
            Ipp32s RefPOCQ1 = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(REF_PIC_LIST_1, PartNumberQ);
            H265MotionVector & MVQ0 = m_pCurrentFrame->m_CodingData->GetMV(REF_PIC_LIST_0, PartNumberQ);
            H265MotionVector & MVQ1 = m_pCurrentFrame->m_CodingData->GetMV(REF_PIC_LIST_1, PartNumberQ);

            if (((RefPOCQ0 == RefPOCP0) && (RefPOCQ1 == RefPOCP1)) ||
                ((RefPOCQ0 == RefPOCP1) && (RefPOCQ1 == RefPOCP0)))

            {
                if (RefPOCP0 != RefPOCP1)
                {
                    if (RefPOCP0 == RefPOCQ0)
                    {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    }
                    else
                    {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    }
                }
                else
                {
                    Ipp32s bs = (((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) &&
                                  (MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0))) ? 1 : 0);
                    edge->strength = (Ipp8u)bs;
                    return;
                }
            }

            edge->strength = 1;
            return;
        }
        else
        {
            // One reference in both CUs
            EnumRefPicList RefPicList;

            RefPicList = COL_TU_INVALID_INTER != m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberP) ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            Ipp32s RefPOCP = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(RefPicList, PartNumberP);
            H265MotionVector MVP = m_pCurrentFrame->m_CodingData->GetMV(RefPicList, PartNumberP);

            RefPicList = COL_TU_INVALID_INTER != m_pCurrentFrame->m_CodingData->GetTUFlags(REF_PIC_LIST_0, PartNumberQ) ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            Ipp32s RefPOCQ = m_pCurrentFrame->m_CodingData->GetTUPOCDelta(RefPicList, PartNumberQ);
            H265MotionVector MVQ = m_pCurrentFrame->m_CodingData->GetMV(RefPicList, PartNumberQ);

            if (RefPOCQ == RefPOCP)
            {
                Ipp32s bs = (MVIsnotEq(MVP, MVQ) ? 1 : 0);
                edge->strength = (Ipp8u)bs;
                return;
            }

            edge->strength = 1;
            return;
        }
    }
}

//Edges

//  0
//2_|_3
//  |
//  1

void H265SegmentDecoder::SetEdges(H265CodingUnit* curLCU,
                                  Ipp32s width,
                                  Ipp32s height,
                                  Ipp32s cross,
                                  Ipp32s calculateCurLCU)
{
    H265CodingUnit *leftLCU, *aboveLCU;
    H265Slice* pSlice;
    Ipp32s maxCUSize = m_pSeqParamSet->getMaxCUWidth();
    Ipp32s curPixelColumn, curPixelRow;
    Ipp32s crossSliceBoundaryFlag, crossTileBoundaryFlag;
    Ipp32s tcOffset, betaOffset;
    H265EdgeData edge;
    Ipp32u uiPartP;
    Ipp32s i, j, e;

    if (cross)
    {
        aboveLCU = curLCU->getPUAbove(uiPartP, 0, 0, false, 0);

        if (aboveLCU)
        {
            pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(aboveLCU->m_SliceIdx);

            if (!pSlice->getDeblockingFilterDisable())
            {
                crossSliceBoundaryFlag = pSlice->getLFCrossSliceBoundaryFlag();
                crossTileBoundaryFlag = pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag();
                tcOffset = pSlice->getLoopFilterTcOffset() << 1;
                betaOffset = pSlice->getLoopFilterBetaOffset() << 1;

                for (i = 0; i < width; i += 8)
                {
                    for (e = 0; e < 2; e++)
                    {
                        curPixelColumn = i;
                        curPixelRow = (maxCUSize - 8 + 4 * e);

#if (HEVC_OPT_CHANGES & 32)
                        // ML: OPT: inline/constant propagation of direction
                        // ML: OPT: Passing pointer to edge structure in array directly to avoid failed forwarding from byte store to int load
                        GetEdgeStrength< VERT_FILT >(aboveLCU, &m_edge[0][(i >> 3)+1][e],
                                        curPixelColumn, curPixelRow,
                                        crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                        tcOffset, betaOffset);
#else
                        GetEdgeStrength(aboveLCU, &edge, curPixelColumn, curPixelRow,
                                        crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                        tcOffset, betaOffset, VERT_FILT);

                        m_edge[0][(i >> 3)+1][e] = edge;
#endif
                    }
                }
                for (; i < maxCUSize; i += 8)
                    m_edge[0][(i >> 3) + 1][0].strength = m_edge[0][(i >> 3) + 1][1].strength = 0;
            }
            else
            {
                for (i = 0; i < maxCUSize; i += 8)
                {
                    for (e = 0; e < 2; e++)
                    {
                        m_edge[0][(i >> 3)+1][e].strength = 0;
                    }
                }
            }
        }
        else
        {
            for (i = 0; i < maxCUSize; i += 8)
            {
                for (e = 0; e < 2; e++)
                {
                    m_edge[0][(i >> 3)+1][e].strength = 0;
                }
            }
        }
    }

    if ((curLCU->CUAddr == curLCU->m_SliceHeader->slice_segment_address) || (cross))
    {
        leftLCU = curLCU->getPULeft(uiPartP, 0, 0, 0);

        if (leftLCU)
        {
            pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(leftLCU->m_SliceIdx);

            if (!pSlice->getDeblockingFilterDisable())
            {
                crossSliceBoundaryFlag = pSlice->getLFCrossSliceBoundaryFlag();
                crossTileBoundaryFlag = pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag();
                tcOffset = pSlice->getLoopFilterTcOffset() << 1;
                betaOffset = pSlice->getLoopFilterBetaOffset() << 1;

                for (j = 0; j < height; j += 8)
                {
                    for (e = 2; e < 4; e++)
                    {
                        curPixelColumn = (maxCUSize - 8 + 4 * (e - 2));
                        curPixelRow = j;

#if (HEVC_OPT_CHANGES & 32)
                        // ML: OPT: inline/constant propagation of direction
                        // ML: OPT: Passing pointer to edge structure in array directly to avoid failed forwarding from byte store to int load
                        GetEdgeStrength< HOR_FILT >(leftLCU, 
                                        &m_edge[(j >> 3)+1][0][e],
                                        curPixelColumn, curPixelRow,
                                        crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                        tcOffset, betaOffset);
#else
                        GetEdgeStrength(leftLCU, &edge, curPixelColumn, curPixelRow,
                                        crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                        tcOffset, betaOffset, HOR_FILT);

                        m_edge[(j >> 3)+1][0][e] = edge;
#endif
                    }
                }
                for (; j < maxCUSize; j += 8)
                    m_edge[(j >> 3) + 1][0][2].strength = m_edge[(j >> 3) + 1][0][3].strength = 0;
            }
            else
            {
                for (j = 0; j < (maxCUSize >> 3); j++)
                {
                    m_edge[j + 1][0][2].strength = 0;
                    m_edge[j + 1][0][3].strength = 0;
                }
            }
        }
        else
        {
            for (j = 0; j < (maxCUSize >> 3); j++)
            {
                m_edge[j + 1][0][2].strength = 0;
                m_edge[j + 1][0][3].strength = 0;
            }
        }
    }
    else
    {
        if (curLCU->m_CUPelX == 0)
        {
            for (j = 0; j < (maxCUSize >> 3); j++)
            {
                m_edge[j + 1][0][2].strength = 0;
                m_edge[j + 1][0][3].strength = 0;
            }
        }
        else
        {
            for (j = 0; j < (maxCUSize >> 3); j++)
            {
                m_edge[j + 1][0][2] = m_edge[j + 1][maxCUSize >> 3][2];
                m_edge[j + 1][0][3] = m_edge[j + 1][maxCUSize >> 3][3];
            }
        }
    }

    if (!calculateCurLCU)
        return;

    pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);
    crossSliceBoundaryFlag = pSlice->getLFCrossSliceBoundaryFlag();
    crossTileBoundaryFlag = pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag();
    tcOffset = pSlice->getLoopFilterTcOffset() << 1;
    betaOffset = pSlice->getLoopFilterBetaOffset() << 1;

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
#if (HEVC_OPT_CHANGES & 32)
            // ML: OPT: Breaking loop into 2 to allow constant propogation of '*_FILT' into GetEdgeStrength()
            // ML: OPT: Passing pointer to edge structure in array directly to avoid failed forwarding from byte store to int load
            for (e = 0; e < 2; e++)
            {
                curPixelColumn = i;
                curPixelRow = (j + 4 * e);

                GetEdgeStrength< VERT_FILT >(curLCU, &m_edge[(j >> 3)+1][(i >> 3)+1][e],
                                curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset);
            }

            for (; e < 4; e++)
            {
                curPixelColumn = (i + 4 * (e - 2));
                curPixelRow = j;

                GetEdgeStrength< HOR_FILT >(curLCU, &edge,
                                curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset);

                m_edge[(j >> 3)+1][(i >> 3)+1][e] = edge;
            }
#else
            for (e = 0; e < 4; e++)
            {
                if (e < 2)
                {
                    curPixelColumn = i;
                    curPixelRow = (j + 4 * e);
                    dir = VERT_FILT;
                }
                else
                {
                    curPixelColumn = (i + 4 * (e - 2));
                    curPixelRow = j;
                    dir = HOR_FILT;
                }

                GetEdgeStrength(curLCU, &edge, curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset, dir);

                m_edge[(j >> 3)+1][(i >> 3)+1][e] = edge;
            }
#endif
        }
        for (; i < maxCUSize; i += 8)
            m_edge[(j >> 3) + 1][(i >> 3) + 1][0].strength = m_edge[(j >> 3) + 1][(i >> 3) + 1][1].strength =
            m_edge[(j >> 3) + 1][(i >> 3) + 1][2].strength = m_edge[(j >> 3) + 1][(i >> 3) + 1][3].strength = 0;
    }
    for (; j < maxCUSize; j += 8)
        for (i = 0; i < maxCUSize; i += 8)
            m_edge[(j >> 3) + 1][(i >> 3) + 1][0].strength = m_edge[(j >> 3) + 1][(i >> 3) + 1][1].strength =
            m_edge[(j >> 3) + 1][(i >> 3) + 1][2].strength = m_edge[(j >> 3) + 1][(i >> 3) + 1][3].strength = 0;
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
