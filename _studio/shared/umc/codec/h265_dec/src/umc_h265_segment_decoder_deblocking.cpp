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

            if (edge->strength > 0)
            {
                FilterEdgeLuma(edge, baseSrcDst + 4 * (i - 1) * srcDstStride,
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

            if (edge->strength > 0)
            {
                FilterEdgeLuma(edge, baseSrcDst + 4 * (i - 1),
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

            if (edge->strength > 1)
            {
                FilterEdgeChroma(edge, baseSrcDst + 4 * (i - 1) * srcDstStride,
                    srcDstStride, chromaCbQpOffset, chromaCrQpOffset, VERT_FILT);
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

            if (edge->strength > 1)
            {
                FilterEdgeChroma(edge, baseSrcDst + 4 * (i - 1) * 2,
                    srcDstStride, chromaCbQpOffset, chromaCrQpOffset, HOR_FILT);
            }
        }
    }
}

void H265SegmentDecoder::FilterEdgeLuma(H265EdgeData *edge,
                                        H265PlaneYCommon *srcDst,
                                        Ipp32s srcDstStride,
                                        Ipp32s dir)
{
    Ipp32s bitDepthLuma = 8;
    Ipp32s tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
    Ipp32s bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
    Ipp32s tc =  tcTable[tcIdx] * (1 << (bitDepthLuma - 8));
    Ipp32s beta = betaTable[bIdx] * (1 << (bitDepthLuma - 8));
    Ipp32s sideThreshhold = (beta + (beta >> 1)) >> 3;
    Ipp32s offset, strDstStep;
    Ipp32s i;

    if (dir == VERT_FILT)
    {
        offset = 1;
        strDstStep = srcDstStride;
    }
    else
    {
        offset = srcDstStride;
        strDstStep = 1;
    }

    {
        Ipp32s dp0 = abs(srcDst[0*strDstStep-3*offset] - 2 * srcDst[0*strDstStep-2*offset] + srcDst[0*strDstStep-1*offset]);
        Ipp32s dp3 = abs(srcDst[3*strDstStep-3*offset] - 2 * srcDst[3*strDstStep-2*offset] + srcDst[3*strDstStep-1*offset]);
        Ipp32s dq0 = abs(srcDst[0*strDstStep+0*offset] - 2 * srcDst[0*strDstStep+1*offset] + srcDst[0*strDstStep+2*offset]);
        Ipp32s dq3 = abs(srcDst[3*strDstStep+0*offset] - 2 * srcDst[3*strDstStep+1*offset] + srcDst[3*strDstStep+2*offset]);
        Ipp32s d0 = dp0 + dq0;
        Ipp32s d3 = dp3 + dq3;
        Ipp32s dq = dq0 + dq3;
        Ipp32s dp = dp0 + dp3;
        Ipp32s d = d0 + d3;

        if (d < beta)
        {
            Ipp32s ds0 = abs(srcDst[0*strDstStep-4*offset] - srcDst[0*strDstStep-1*offset]) +
                         abs(srcDst[0*strDstStep+3*offset] - srcDst[0*strDstStep+0*offset]);
            Ipp32s ds3 = abs(srcDst[3*strDstStep-4*offset] - srcDst[3*strDstStep-1*offset]) +
                         abs(srcDst[3*strDstStep+3*offset] - srcDst[3*strDstStep+0*offset]);
            Ipp32s dm0 = abs(srcDst[0*strDstStep-1*offset] - srcDst[0*strDstStep+0*offset]);
            Ipp32s dm3 = abs(srcDst[3*strDstStep-1*offset] - srcDst[3*strDstStep+0*offset]);
            bool strongFiltering = false;

            if ((ds0 < (beta >> 3)) && (2 * d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) &&
                (ds3 < (beta >> 3)) && (2 * d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
            {
                strongFiltering = true;
            }

            for (i = 0; i < 4; i++)
            {
                Ipp32s p0 = srcDst[-1*offset];
                Ipp32s p1 = srcDst[-2*offset];
                Ipp32s p2 = srcDst[-3*offset];
                Ipp32s q0 = srcDst[0*offset];
                Ipp32s q1 = srcDst[1*offset];
                Ipp32s q2 = srcDst[2*offset];
                Ipp32s tmp;

                if (strongFiltering)
                {
                    if (edge->deblockP)
                    {
                        Ipp32s p3 = srcDst[-4*offset];
                        tmp = p1 + p0 + q0;
                        srcDst[-1*offset] = (H265PlaneYCommon)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                        srcDst[-2*offset] = (H265PlaneYCommon)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                        srcDst[-3*offset] = (H265PlaneYCommon)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                    }

                    if (edge->deblockQ)
                    {
                        Ipp32s q3 = srcDst[3*offset];

                        tmp = q1 + q0 + p0;
                        srcDst[0*offset] = (H265PlaneYCommon)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                        srcDst[1*offset] = (H265PlaneYCommon)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                        srcDst[2*offset] = (H265PlaneYCommon)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                    }
                }
                else
                {
                    Ipp32s delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                    if (abs(delta) < tc * 10)
                    {
                        delta = Clip3(-tc, tc, delta);

                        if (edge->deblockP)
                        {
                            srcDst[-1*offset] = (H265PlaneYCommon)(Clip3(0, 255, (p0 + delta)));

                            if (dp < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                                srcDst[-2*offset] = (H265PlaneYCommon)(Clip3(0, 255, (p1 + tmp)));
                            }
                        }

                        if (edge->deblockQ)
                        {
                            srcDst[0] = (H265PlaneYCommon)(Clip3(0, 255, (q0 - delta)));

                            if (dq < sideThreshhold)
                            {
                                tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                                srcDst[1*offset] = (H265PlaneYCommon)(Clip3(0, 255, (q1 + tmp)));
                            }
                        }
                    }
                }

                srcDst += strDstStep;
            }
        }
    }
}

#define   QpUV(iQpY)  ( ((iQpY) < 0) ? (iQpY) : (((iQpY) > 57) ? ((iQpY)-6) : g_ChromaScale[(iQpY)]) )

void H265SegmentDecoder::FilterEdgeChroma(H265EdgeData *edge,
                                          H265PlaneUVCommon *srcDst,
                                          Ipp32s srcDstStride,
                                          Ipp32s chromaCbQpOffset,
                                          Ipp32s chromaCrQpOffset,
                                          Ipp32s dir)
{
    Ipp32s bitDepthChroma = 8;
    Ipp32s qpCb = QpUV(edge->qp + chromaCbQpOffset);
    Ipp32s qpCr = QpUV(edge->qp + chromaCrQpOffset);
    Ipp32s tcIdxCb = Clip3(0, 53, qpCb + 2 * (edge->strength - 1) + edge->tcOffset);
    Ipp32s tcIdxCr = Clip3(0, 53, qpCr + 2 * (edge->strength - 1) + edge->tcOffset);
    Ipp32s tcCb =  tcTable[tcIdxCb] * (1 << (bitDepthChroma - 8));
    Ipp32s tcCr =  tcTable[tcIdxCr] * (1 << (bitDepthChroma - 8));
    Ipp32s offset, strDstStep;
    Ipp32s i;

    if (dir == VERT_FILT)
    {
        offset = 2;
        strDstStep = srcDstStride;
    }
    else
    {
        offset = srcDstStride;
        strDstStep = 2;
    }

    for (i = 0; i < 4; i++)
    {
        Ipp32s p0Cb = srcDst[-1*offset];
        Ipp32s p1Cb = srcDst[-2*offset];
        Ipp32s q0Cb = srcDst[0*offset];
        Ipp32s q1Cb = srcDst[1*offset];
        Ipp32s deltaCb = ((((q0Cb - p0Cb) << 2) + p1Cb - q1Cb + 4) >> 3);
        deltaCb = Clip3(-tcCb, tcCb, deltaCb);

        Ipp32s p0Cr = srcDst[-1*offset + 1];
        Ipp32s p1Cr = srcDst[-2*offset + 1];
        Ipp32s q0Cr = srcDst[0*offset + 1];
        Ipp32s q1Cr = srcDst[1*offset + 1];
        Ipp32s deltaCr = ((((q0Cr - p0Cr) << 2) + p1Cr - q1Cr + 4) >> 3);
        deltaCr = Clip3(-tcCr, tcCr, deltaCr);

        if (edge->deblockP)
        {
            srcDst[-offset] = (H265PlaneUVCommon)(Clip3(0, 255, (p0Cb + deltaCb)));
            srcDst[-offset + 1] = (H265PlaneUVCommon)(Clip3(0, 255, (p0Cr + deltaCr)));
        }

        if (edge->deblockQ)
        {
            srcDst[0] = (H265PlaneUVCommon)(Clip3(0, 255, (q0Cb - deltaCb)));
            srcDst[1] = (H265PlaneUVCommon)(Clip3(0, 255, (q0Cr - deltaCr)));
        }

        srcDst += strDstStep;
    }
}

static H265_FORCEINLINE bool MVIsnotEq(H265MotionVector mv0,
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
    Ipp32s log2MinTUSize = m_pSeqParamSet->getQuadtreeTULog2MinSize();
    H265CodingUnit* pcCUP;
    Ipp32u uiPartQ, uiPartP;

    pcCUQ->m_AbsIdxInLCU = 0;

    edge->strength = 0;

    uiPartQ = g_RasterToZscan[((curPixelRow >> log2MinTUSize) << (log2LCUSize - log2MinTUSize)) + (curPixelColumn >> log2MinTUSize)];

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

    edge->qp = (pcCUQ->m_QPArray[uiPartQ] + pcCUP->m_QPArray[uiPartP] + 1) >> 1;

    if ((pcCUQ->CUAddr != pcCUP->CUAddr)||
        (pcCUQ->m_TrStartArray[uiPartQ] != pcCUP->m_TrStartArray[uiPartP]))
    {
        if ((pcCUQ->m_PredModeArray[uiPartQ] == MODE_INTRA) ||
            (pcCUP->m_PredModeArray[uiPartP] == MODE_INTRA))
        {
            edge->strength = 2;
            return;
        }

        if (((pcCUQ->m_Cbf[0][uiPartQ] >> pcCUQ->m_TrIdxArray[uiPartQ]) != 0) ||
            ((pcCUP->m_Cbf[0][uiPartP] >> pcCUP->m_TrIdxArray[uiPartP]) != 0))
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if ((pcCUQ->m_PredModeArray[uiPartQ] == MODE_INTRA) ||
            (pcCUP->m_PredModeArray[uiPartP] == MODE_INTRA))
        {
            edge->strength = 0;
            return;
        }
    }

    /* Check MV */
    {
        Ipp32s RefPOCQ0, RefPOCQ1, RefPOCP0, RefPOCP1;
        Ipp32s numRefsP, numRefsQ;
        H265MotionVector MVQ0, MVQ1, MVP0, MVP1;
        Ipp32s i;

        numRefsP = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUP->m_CUMVbuffer[RefPicList].RefIdx[uiPartP] >= 0)
            {
                numRefsP++;
            }
        }

        numRefsQ = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUQ->m_CUMVbuffer[RefPicList].RefIdx[uiPartQ] >= 0)
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
            RefPOCQ0 = m_pCurrentFrame->GetRefPicList(pcCUQ->m_SliceIdx, REF_PIC_LIST_0)->m_refPicList[pcCUQ->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[uiPartQ]].refFrame->m_PicOrderCnt;
            RefPOCQ1 = m_pCurrentFrame->GetRefPicList(pcCUQ->m_SliceIdx, REF_PIC_LIST_1)->m_refPicList[pcCUQ->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[uiPartQ]].refFrame->m_PicOrderCnt;
            MVQ0 = pcCUQ->m_CUMVbuffer[REF_PIC_LIST_0].MV[uiPartQ];
            MVQ1 = pcCUQ->m_CUMVbuffer[REF_PIC_LIST_1].MV[uiPartQ];
            RefPOCP0 = m_pCurrentFrame->GetRefPicList(pcCUP->m_SliceIdx, REF_PIC_LIST_0)->m_refPicList[pcCUP->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[uiPartP]].refFrame->m_PicOrderCnt;
            RefPOCP1 = m_pCurrentFrame->GetRefPicList(pcCUP->m_SliceIdx, REF_PIC_LIST_1)->m_refPicList[pcCUP->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[uiPartP]].refFrame->m_PicOrderCnt;
            MVP0 = pcCUP->m_CUMVbuffer[REF_PIC_LIST_0].MV[uiPartP];
            MVP1 = pcCUP->m_CUMVbuffer[REF_PIC_LIST_1].MV[uiPartP];

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
            EnumRefPicList RefPicList;

            RefPicList = ((pcCUQ->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[uiPartQ] >= 0) ? REF_PIC_LIST_0 : REF_PIC_LIST_1);
            RefPOCQ0 = m_pCurrentFrame->GetRefPicList(pcCUQ->m_SliceIdx, RefPicList)->m_refPicList[pcCUQ->m_CUMVbuffer[RefPicList].RefIdx[uiPartQ]].refFrame->m_PicOrderCnt;
            MVQ0 = pcCUQ->m_CUMVbuffer[RefPicList].MV[uiPartQ];

            RefPicList = ((pcCUP->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[uiPartP] >= 0) ? REF_PIC_LIST_0 : REF_PIC_LIST_1);
            RefPOCP0 = m_pCurrentFrame->GetRefPicList(pcCUP->m_SliceIdx, RefPicList)->m_refPicList[pcCUP->m_CUMVbuffer[RefPicList].RefIdx[uiPartP]].refFrame->m_PicOrderCnt;
            MVP0 = pcCUP->m_CUMVbuffer[RefPicList].MV[uiPartP];

            if (RefPOCQ0 == RefPOCP0)
            {
                Ipp32s bs = (MVIsnotEq(MVP0, MVQ0) ? 1 : 0);
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
            }
            else
            {
                for (i = 0; i < width; i += 8)
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
            for (i = 0; i < width; i += 8)
            {
                for (e = 0; e < 2; e++)
                {
                    m_edge[0][(i >> 3)+1][e].strength = 0;
                }
            }
        }
    }

    if ((curLCU->CUAddr == curLCU->m_SliceHeader->m_sliceAddr) || (cross))
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
            }
            else
            {
                for (j = 0; j < (height >> 3); j++)
                {
                    m_edge[j+1][0][2].strength = 0;
                    m_edge[j+1][0][3].strength = 0;
                }
            }
        }
        else
        {
            for (j = 0; j < (height >> 3); j++)
            {
                m_edge[j+1][0][2].strength = 0;
                m_edge[j+1][0][3].strength = 0;
            }
        }
    }
    else
    {
        if (curLCU->m_CUPelX == 0)
        {
            for (j = 0; j < (height >> 3); j++)
            {
                m_edge[j+1][0][2].strength = 0;
                m_edge[j+1][0][3].strength = 0;
            }
        }
        else
        {
            for (j = 0; j < (height >> 3); j++)
            {
                m_edge[j+1][0][2] = m_edge[j+1][maxCUSize >> 3][2];
                m_edge[j+1][0][3] = m_edge[j+1][maxCUSize >> 3][3];
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
    }
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
