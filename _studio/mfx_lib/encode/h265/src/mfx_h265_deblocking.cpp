//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_frame.h"
//#include "mfx_h265_optimization.h"

namespace H265Enc {

#define VERT_FILT 0
#define HOR_FILT  1

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

static bool MVIsnotEq(H265MV mv0,
                      H265MV mv1)
{
    Ipp16s mvxd = (Ipp16s)ABS(mv0.mvx - mv1.mvx);
    Ipp16s mvyd = (Ipp16s)ABS(mv0.mvy - mv1.mvy);

    if ((mvxd >= 4) || (mvyd >= 4))
    {
        return true;
    }
    else
    {
        return false;
    }
}

template <typename PixType>
void H265CU<PixType>::DeblockOneCrossLuma(Ipp32s curPixelColumn,
                                 Ipp32s curPixelRow)
{
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    srcDstStride = m_pitchRecLuma;
    baseSrcDst = m_yRec + curPixelRow * srcDstStride + curPixelColumn;

    start = 0;
    end = 2;

    if (m_ctbPelY + curPixelRow == m_region_border_top + 8)
        start = -1;
    if (m_ctbPelY + curPixelRow >= m_region_border_bottom)
        end = 1;

    for (i = start; i < end; i++)
    {
        edge = &m_edge[y + ((i + 1) >> 1) - 1][x][(i + 1) & 1];

        if (edge->strength > 0)
        {
            h265_FilterEdgeLuma_I(edge, baseSrcDst + 4 * (i - 1) * srcDstStride,
                           srcDstStride, VERT_FILT, m_par->bitDepthLuma);
        }
    }


    start = 0;
    end = 2;
    if (m_ctbPelX + curPixelColumn == m_region_border_left + 8)
        start = -1;
    if (m_ctbPelX + curPixelColumn >= m_region_border_right)
        end = 1;
    
    for (i = start; i < end; i++)
    {
        edge = &m_edge[y][x + ((i + 1) >> 1) - 1][((i + 1) & 1) + 2];

        if (edge->strength > 0)
        {
            h265_FilterEdgeLuma_I(edge, baseSrcDst + 4 * (i - 1), srcDstStride, HOR_FILT, m_par->bitDepthLuma);
        }
    }
}


template <typename PixType>
void H265CU<PixType>::DeblockOneCrossChroma(Ipp32s curPixelColumn,
                                   Ipp32s curPixelRow)
{
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    srcDstStride = m_pitchRecChroma;
    baseSrcDst = m_uvRec + (curPixelRow >> m_par->chromaShiftH) * srcDstStride + (curPixelColumn << m_par->chromaShiftWInv);

    start = 0;
    end = 2;

    if (m_ctbPelY + curPixelRow == m_region_border_top + (8 << m_par->chromaShiftH))
        start = -1;
    if (m_ctbPelY + curPixelRow >= m_region_border_bottom)
        end = 1;

    for (i = start; i < end; i++)
    {
        Ipp32s idxY, idxN;
        if (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV420) {
            idxY = y + i;
            idxN = 0;
        } else {
            idxY = y + ((i + 1) >> 1);
            idxN = (i + 1) & 1;
        }
        
        edge = &m_edge[idxY - 1][x][idxN];

        if (edge->strength > 1)
        {
            h265_FilterEdgeChroma_Interleaved_I(
                edge,
                baseSrcDst + 4 * (i - 1) * srcDstStride,
                srcDstStride,
                VERT_FILT,
                GetChromaQP(edge->qp, 0, m_par->chromaFormatIdc, 8),
                GetChromaQP(edge->qp, 0, m_par->chromaFormatIdc, 8),
                m_par->bitDepthChroma);
        }
    }

    start = 0;
    end = 2;
    if (m_ctbPelX + curPixelColumn == m_region_border_left + (8 << m_par->chromaShiftW))
        start = -1;
    if (m_ctbPelX + curPixelColumn >= m_region_border_right)
        end = 1;

    for (i = start; i < end; i++)
    {
        Ipp32s idxX, idxN;
        if ( m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444) {
            idxX = x + i;
            idxN = 2;
        } else {
            idxX = x + ((i + 1) >> 1);
            idxN = ((i + 1) & 1) + 2;
        }
        edge = &m_edge[y][idxX - 1][idxN];

        if (edge->strength > 1)
        {
            h265_FilterEdgeChroma_Interleaved_I(
                edge,
                baseSrcDst + 4 * 2 * (i - 1),
                srcDstStride,
                HOR_FILT,
                GetChromaQP(edge->qp, 0, m_par->chromaFormatIdc, 8),
                GetChromaQP(edge->qp, 0, m_par->chromaFormatIdc, 8),
                m_par->bitDepthChroma);
        }
    }
}

template <typename PixType>
void H265CU<PixType>::GetEdgeStrength(H265CUPtr *pcCUQptr,
                             H265EdgeData *edge,
                             Ipp32s curPixelColumn,
                             Ipp32s curPixelRow,
                             Ipp32s crossSliceBoundaryFlag,
                             Ipp32s crossTileBoundaryFlag,
                             Ipp32s tcOffset,
                             Ipp32s betaOffset,
                             Ipp32s dir)
{
    Ipp32s log2MinTUSize = m_par->QuadtreeTULog2MinSize;
    Ipp32u uiPartQ, uiPartP;
    H265CUPtr CUPPtr;
    H265CUData *pcCUP, *pcCUQ;
    pcCUQ = pcCUQptr->ctbData;

    edge->strength = 0;

    uiPartQ = h265_scan_r2z4[((curPixelRow >> log2MinTUSize) * PITCH_TU) + (curPixelColumn >> log2MinTUSize)];

    if (dir == HOR_FILT)
    {
        if ((curPixelRow >> log2MinTUSize) == ((curPixelRow - 1) >> log2MinTUSize))
        {
            return;
        }
        GetPuAboveOf(&CUPPtr, pcCUQptr, uiPartQ, !crossSliceBoundaryFlag, /*false, false,*/ false, !crossTileBoundaryFlag);
    }
    else
    {
        if ((curPixelColumn >> log2MinTUSize) == ((curPixelColumn - 1) >> log2MinTUSize))
        {
            return;
        }
        GetPuLeftOf(&CUPPtr, pcCUQptr, uiPartQ, !crossSliceBoundaryFlag/*, false*/, !crossTileBoundaryFlag);
    }

    if (CUPPtr.ctbData == NULL)
    {
        return;
    }

    uiPartP = CUPPtr.absPartIdx;
    pcCUP = CUPPtr.ctbData;

    edge->deblockP = 1;
    edge->deblockQ = 1;

    edge->tcOffset = (Ipp8s)tcOffset;
    edge->betaOffset = (Ipp8s)betaOffset;
/*
    if ((pcCUP->m_IPCMFlag[uiPartP] != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
    {
        edge->deblockP = 0;
    }

    if ((pcCUQ->m_IPCMFlag[uiPartQ] != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
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
*/
    edge->qp = (pcCUQ[uiPartQ].qp + pcCUP[uiPartP].qp + 1) >> 1;
    Ipp32s depthp = pcCUP[uiPartP].depth + pcCUP[uiPartP].trIdx;
    Ipp32s depthq = pcCUQ[uiPartQ].depth + pcCUQ[uiPartQ].trIdx;

    if ((pcCUQ != pcCUP) ||
        (pcCUP[uiPartP].depth != pcCUQ[uiPartQ].depth) ||
        (depthp != depthq) ||
        ((uiPartP ^ uiPartQ) >> ((m_par->MaxCUDepth - depthp) << 1)))
    {
        if ((pcCUQ[uiPartQ].predMode == MODE_INTRA) ||
            (pcCUP[uiPartP].predMode == MODE_INTRA))
        {
            edge->strength = 2;
            return;
        }

        if (((pcCUQ[uiPartQ].cbf[0] >> pcCUQ[uiPartQ].trIdx) != 0) ||
            ((pcCUP[uiPartP].cbf[0] >> pcCUP[uiPartP].trIdx) != 0))
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if ((pcCUQ[uiPartQ].predMode == MODE_INTRA) ||
            (pcCUP[uiPartP].predMode == MODE_INTRA))
        {
            edge->strength = 0;
            return;
        }
    }

    /* Check MV */
    {
        Ipp32s RefPOCQ0, RefPOCQ1, RefPOCP0, RefPOCP1;
        Ipp32s numRefsP, numRefsQ;
        H265MV MVQ0, MVQ1, MVP0, MVP1;
        Ipp32s i;

        numRefsP = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUP[uiPartP].refIdx[RefPicList] >= 0)
            {
                numRefsP++;
            }
        }

        numRefsQ = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUQ[uiPartQ].refIdx[RefPicList] >= 0)
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
            RefPOCQ0 = m_currFrame->m_refPicList[0].m_refFrames[pcCUQ[uiPartQ].refIdx[0]]->m_poc;
            RefPOCQ1 = m_currFrame->m_refPicList[1].m_refFrames[pcCUQ[uiPartQ].refIdx[1]]->m_poc;
            MVQ0 = pcCUQ[uiPartQ].mv[0];
            MVQ1 = pcCUQ[uiPartQ].mv[1];
            RefPOCP0 = m_currFrame->m_refPicList[0].m_refFrames[pcCUP[uiPartP].refIdx[0]]->m_poc;
            RefPOCP1 = m_currFrame->m_refPicList[1].m_refFrames[pcCUP[uiPartP].refIdx[1]]->m_poc;
            MVP0 = pcCUP[uiPartP].mv[0];
            MVP1 = pcCUP[uiPartP].mv[1];

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
            if (pcCUQ[uiPartQ].refIdx[REF_PIC_LIST_0] >= 0) {
                RefPOCQ0 = m_currFrame->m_refPicList[0].m_refFrames[pcCUQ[uiPartQ].refIdx[0]]->m_poc;
                MVQ0 = pcCUQ[uiPartQ].mv[0];
            }
            else {
                RefPOCQ0 = m_currFrame->m_refPicList[1].m_refFrames[pcCUQ[uiPartQ].refIdx[1]]->m_poc;
                MVQ0 = pcCUQ[uiPartQ].mv[1];
            }

            if (pcCUP[uiPartP].refIdx[0] >= 0) {
                RefPOCP0 = m_currFrame->m_refPicList[0].m_refFrames[pcCUP[uiPartP].refIdx[0]]->m_poc;
                MVP0 = pcCUP[uiPartP].mv[0];
            }
            else {
                RefPOCP0 = m_currFrame->m_refPicList[1].m_refFrames[pcCUP[uiPartP].refIdx[1]]->m_poc;
                MVP0 = pcCUP[uiPartP].mv[1];
            }

            if (RefPOCQ0 == RefPOCP0) {
                Ipp32s bs = (MVIsnotEq(MVP0, MVQ0) ? 1 : 0);
                edge->strength = (Ipp8u)bs;
                return;
            }

            edge->strength = 1;
            return;
        }
    }
}


template <typename PixType>
void H265CU<PixType>::SetEdgesCTU(H265CUPtr *curLCU, Ipp32s width, Ipp32s height, Ipp32s x_inc, Ipp32s y_inc)
{
    Ipp32s curPixelColumn, curPixelRow;
    Ipp32s crossSliceBoundaryFlag, crossTileBoundaryFlag;
    Ipp32s tcOffset, betaOffset;
    H265EdgeData edge;
    Ipp32s dir;
    Ipp32s i, j, e;

    crossSliceBoundaryFlag = m_cslice->slice_loop_filter_across_slices_enabled_flag;
    crossTileBoundaryFlag = m_par->NumTiles > 1 ? m_par->deblockBordersFlag : 0;
    tcOffset = m_cslice->slice_tc_offset_div2 << 1;
    betaOffset = m_cslice->slice_beta_offset_div2 << 1;

    if (curLCU->ctbData) {
        for (j = 0; j < height; j += 8)
        {
            for (i = 0; i < width; i += 8)
            {
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

                    m_edge[(j >> 3) + y_inc][(i >> 3) + x_inc][e] = edge;
                }
            }
        }
    } else {
        for (j = 0; j < height; j += 8)
        {
            for (i = 0; i < width; i += 8)
            {
                for (e = 0; e < 4; e++)
                    m_edge[(j >> 3) + y_inc][(i >> 3) + x_inc][e].strength = 0;
            }
        }
    }
}

//Edges

//  0
//2_|_3
//  |
//  1

template <typename PixType>
void H265CU<PixType>::SetEdges(Ipp32s width, Ipp32s height)
{
    Ipp32s crossSliceBoundaryFlag = m_cslice->slice_loop_filter_across_slices_enabled_flag;
    Ipp32s crossTileBoundaryFlag = m_par->NumTiles > 1 ? m_par->deblockBordersFlag : 0;

    H265CUPtr curLCU;
    curLCU.ctbData = m_data;
    curLCU.ctbAddr = m_ctbAddr;
    curLCU.absPartIdx = 0;

    SetEdgesCTU(&curLCU, width, height, 0, 0);

    GetCtuBelow(&curLCU, !crossSliceBoundaryFlag, !crossTileBoundaryFlag);
    SetEdgesCTU(&curLCU, width, 1, 0, height >> 3);

    GetCtuRight(&curLCU, !crossSliceBoundaryFlag, !crossTileBoundaryFlag);
    SetEdgesCTU(&curLCU, 1, height, width >> 3, 0);

    GetCtuBelowRight(&curLCU, !crossSliceBoundaryFlag, !crossTileBoundaryFlag);
    SetEdgesCTU(&curLCU, 1, 1, width >> 3, height >> 3);
}

template <typename PixType>
void H265CU<PixType>::Deblock()
{
    Ipp32s widthInSamples = m_region_border_right;
    Ipp32s heightInSamples = m_region_border_bottom;
    Ipp32s width  = IPP_MIN(widthInSamples - m_ctbPelX,  m_par->MaxCUSize);
    Ipp32s height = IPP_MIN(heightInSamples - m_ctbPelY, m_par->MaxCUSize);

    SetEdges(width, height);

    Ipp32s i, j;
    for (j = 8; j <= height; j += 8) {
        for (i = 8; i <= width; i += 8) {
            DeblockOneCrossLuma(i, j);
        }
    }

    for (j = (8 << m_par->chromaShiftH); j <= height; j += (8 << m_par->chromaShiftH)) {
        for (i = (8 << m_par->chromaShiftW); i <= width; i += (8 << m_par->chromaShiftW)) {
            DeblockOneCrossChroma(i, j);
        }
    }
}

template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
