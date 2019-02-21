// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_segment_decoder.h"
#include "umc_h264_dec_deblocking.h"
#include "umc_h264_frame_info.h"
#include "umc_h264_dec_ippwrap.h"

namespace UMC
{

void H264SegmentDecoder::DeblockFrame(int32_t iFirstMB, int32_t iNumMBs)
{
    m_bFrameDeblocking = true;

    if (m_bError && m_isSliceGroups) // skip deblocking
    {
        m_CurMBAddr = iFirstMB + iNumMBs;
        return;
    }

    for (m_CurMBAddr = iFirstMB; m_CurMBAddr < iFirstMB + iNumMBs; m_CurMBAddr++)
    {
        DeblockMacroblockMSlice();
    }

} // void H264SegmentDecoder::DeblockFrame(int32_t uFirstMB, int32_t uNumMBs)

void H264SegmentDecoder::DeblockSegment(int32_t iFirstMB, int32_t nBorder)
{
    // no filtering edges of this slice

    if (DEBLOCK_FILTER_OFF == m_pSliceHeader->disable_deblocking_filter_idc)
        return;

    // set frame deblocking flag
    m_bFrameDeblocking = false;

    void (H264SegmentDecoder::*pDecFunc)(PrepareMBParams);
    PrepareMBParams prepareMBParams;

    int32_t MBYAdjust = 0;
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec && m_field_index)
        MBYAdjust = mb_height/2;

    m_CurMBAddr = iFirstMB;

    // set initial macroblock coordinates
    m_CurMB_X = ((m_CurMBAddr >> (int32_t) m_isMBAFF) % mb_width);
    m_CurMB_Y = ((m_CurMBAddr >> (int32_t) m_isMBAFF) / mb_width) - MBYAdjust;
    m_CurMB_Y <<= (int32_t) m_isMBAFF;

    InitDeblockingOnce();

    if (m_isMBAFF)
    {
        pDecFunc = &H264SegmentDecoder::DeblockMacroblockMBAFF;
        // select optimized deblocking function
        switch (m_pSliceHeader->slice_type)
        {
        case INTRASLICE:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersISliceMBAFF;
            break;

        case PREDSLICE:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersPSliceMBAFF;
            break;

        case BPREDSLICE:
        default:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersBSliceMBAFF;
            break;
        }
    }
    // non-MBAFF case
    else
    {
        pDecFunc = &H264SegmentDecoder::DeblockMacroblock;

        // select optimized deblocking function
        switch (m_pSliceHeader->slice_type)
        {
        case INTRASLICE:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersISlice;
            break;

        case PREDSLICE:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersPSlice;
            break;

        case BPREDSLICE:
        default:
            prepareMBParams = &H264SegmentDecoder::PrepareDeblockingParametersBSlice;
            break;
        }
    }

    for (int32_t curMB = iFirstMB; curMB < nBorder; curMB += 1)
    {
        (this->*pDecFunc)(prepareMBParams);

        // set next MB coordinates
        if (0 == m_isMBAFF)
            m_CurMB_X += 1;
        else
        {
            m_CurMB_X += m_CurMBAddr & 1;
            m_CurMB_Y ^= 1;
        }
        // set next MB addres
        m_CurMBAddr += 1;
    }
} // void H264SegmentDecoder::DeblockSegment(int32_t iFirstMB, int32_t iNumMBs)

void H264SegmentDecoder::DeblockMacroblockMSlice()
{
    H264Slice* pSlice = m_pCurrentFrame->GetAU(m_pCurrentFrame->GetNumberByParity(m_field_index))->GetSliceByNumber(m_gmbinfo->mbs[m_CurMBAddr].slice_id);

    // when deblocking isn't required
    if (!pSlice || (DEBLOCK_FILTER_OFF == pSlice->GetSliceHeader()->disable_deblocking_filter_idc))
        return;

    // select optimized deblocking function
    switch (pSlice->GetSliceHeader()->slice_type)
    {
    case INTRASLICE:
        DeblockMacroblock(&H264SegmentDecoder::PrepareDeblockingParametersISlice);
        break;

    case PREDSLICE:
        DeblockMacroblock(&H264SegmentDecoder::PrepareDeblockingParametersPSlice);
        break;

    case BPREDSLICE:
        DeblockMacroblock(&H264SegmentDecoder::PrepareDeblockingParametersBSlice);
        break;

    default:
        // illegal case. it should never hapen.
        VM_ASSERT(false);
        break;
    }
} // void H264SegmentDecoder::DeblockMacroblockMSlice(int32_t m_CurMBAddr)

void H264SegmentDecoder::DeblockMacroblock(PrepareMBParams prepareMBParams)
{
    // prepare deblocking parameters
    ResetDeblockingVariables();

    (this->*prepareMBParams)();

    DeblockChroma(VERTICAL_DEBLOCKING);
    DeblockChroma(HORIZONTAL_DEBLOCKING);

    // perform deblocking
    DeblockLuma(VERTICAL_DEBLOCKING);
    DeblockLuma(HORIZONTAL_DEBLOCKING);
}

void H264SegmentDecoder::DeblockLuma(uint32_t dir)
{
    if (!m_deblockingParams.DeblockingFlag[dir])
        return;

    uint8_t thresholds[32];
    uint8_t alpha[4];
    uint8_t beta[4];

    int32_t AlphaC0Offset = m_deblockingParams.nAlphaC0Offset;
    int32_t BetaOffset = m_deblockingParams.nBetaOffset;
    int32_t pmq_QP = m_cur_mb.LocalMacroblockInfo->QP;

    uint8_t *pClipTab;
    int32_t QP;
    int32_t index;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    //
    // correct strengths for high profile
    //
    if (GetMB8x8TSFlag(*m_cur_mb.GlobalMacroblockInfo))
    {
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 12, 0);
    }

    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        int32_t pmp_QP;

        // get neighbour block QP
        pmp_QP = m_mbinfo.mbs[m_deblockingParams.nNeighbour[dir]].QP;

        // luma variables
        QP = (pmp_QP + pmq_QP + 1) >> 1 ;

        // external edge variables
        index = IClip(0, 51, QP + BetaOffset);
        beta[0] = (uint8_t) (BETA_TABLE[index]);

        index = IClip(0, 51, QP + AlphaC0Offset);
        alpha[0] = (uint8_t) (ALPHA_TABLE[index]);

        pClipTab = CLIP_TAB[index];

        // create clipping values
        thresholds[0] = (uint8_t) (pClipTab[pStrength[0]]);
        thresholds[1] = (uint8_t) (pClipTab[pStrength[1]]);
        thresholds[2] = (uint8_t) (pClipTab[pStrength[2]]);
        thresholds[3] = (uint8_t) (pClipTab[pStrength[3]]);
    }

    // internal edge variables
    QP = pmq_QP;

    index = IClip(0, 51, QP + BetaOffset);
    beta[1] = (uint8_t) (BETA_TABLE[index]);

    index = IClip(0, 51, QP + AlphaC0Offset);
    alpha[1] = (uint8_t) (ALPHA_TABLE[index]);

    pClipTab = CLIP_TAB[index];

    // create clipping values
    {
        int32_t edge;

        for (edge = 1;edge < 4;edge += 1)
        {
            if (*((uint32_t *) (pStrength + edge * 4)))
            {
                // create clipping values
                thresholds[edge * 4 + 0] = (uint8_t) (pClipTab[pStrength[edge * 4 + 0]]);
                thresholds[edge * 4 + 1] = (uint8_t) (pClipTab[pStrength[edge * 4 + 1]]);
                thresholds[edge * 4 + 2] = (uint8_t) (pClipTab[pStrength[edge * 4 + 2]]);
                thresholds[edge * 4 + 3] = (uint8_t) (pClipTab[pStrength[edge * 4 + 3]]);
            }
        }
    }

    if (bit_depth_luma > 8)
    {
        MFX_H264_PP::GetH264Dispatcher()->FilterDeblockingLumaEdge((uint16_t*)m_deblockingParams.pLuma,
                              m_deblockingParams.pitch_luma,
                              alpha,
                              beta,
                              thresholds,
                              pStrength,
                              bit_depth_luma,
                              dir);
    }
    else
    {
        MFX_H264_PP::GetH264Dispatcher()->FilterDeblockingLumaEdge((uint8_t*)m_deblockingParams.pLuma,
                              m_deblockingParams.pitch_luma,
                              alpha,
                              beta,
                              thresholds,
                              pStrength,
                              bit_depth_luma,
                              dir);
    }
} // void H264SegmentDecoder::DeblockLuma(uint32_t dir)

void H264SegmentDecoder::DeblockChroma(uint32_t dir)
{
    if (!m_deblockingParams.DeblockingFlag[dir] || !m_pCurrentFrame->m_chroma_format)
        return;

    uint8_t thresholds[32] = {};
    uint8_t alpha[4] = {};
    uint8_t beta[4] = {};

    int32_t AlphaC0Offset = m_deblockingParams.nAlphaC0Offset;
    int32_t BetaOffset = m_deblockingParams.nBetaOffset;
    int32_t pmq_QP = m_cur_mb.LocalMacroblockInfo->QP;
    uint8_t *pClipTab;
    int32_t QP;
    int32_t index;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    int32_t nPlane;
    int32_t chroma_qp_offset = ~(m_pPicParamSet->chroma_qp_index_offset[0]);

    for (nPlane = 0; nPlane < 2; nPlane += 1)
    {
        uint32_t planeOffset = (HORIZONTAL_DEBLOCKING == dir && m_pCurrentFrame->m_chroma_format == 2) ? 16*nPlane : 8*nPlane;

        if (chroma_qp_offset != m_pPicParamSet->chroma_qp_index_offset[nPlane])
        {
            chroma_qp_offset = m_pPicParamSet->chroma_qp_index_offset[nPlane];

            if (m_deblockingParams.ExternalEdgeFlag[dir])
            {
                int32_t pmp_QP;

                // get left block QP
                pmp_QP = m_mbinfo.mbs[m_deblockingParams.nNeighbour[dir]].QP;

                // external edge variables
                QP = (QP_SCALE_CR[IClip(0, 51, pmp_QP + chroma_qp_offset)] +
                        QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)] + 1) >> 1;

                index = IClip(0, 51, QP + BetaOffset);
                beta[0 + 2*nPlane] = (uint8_t) (BETA_TABLE[index]);

                index = IClip(0, 51, QP + AlphaC0Offset);
                alpha[0 + 2*nPlane] = (uint8_t) (ALPHA_TABLE[index]);

                pClipTab = CLIP_TAB[index];

                // create clipping values
                thresholds[0 + planeOffset] = (uint8_t) (pClipTab[pStrength[0]]);
                thresholds[1 + planeOffset] = (uint8_t) (pClipTab[pStrength[1]]);
                thresholds[2 + planeOffset] = (uint8_t) (pClipTab[pStrength[2]]);
                thresholds[3 + planeOffset] = (uint8_t) (pClipTab[pStrength[3]]);
            }

            // internal edge variables
            QP = QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)];

            index = IClip(0, 51, QP + BetaOffset);
            beta[1 + 2*nPlane] = (uint8_t) (BETA_TABLE[index]);

            index = IClip(0, 51, QP + AlphaC0Offset);
            alpha[1 + 2*nPlane] = (uint8_t) (ALPHA_TABLE[index]);

            pClipTab = CLIP_TAB[index];

            if (HORIZONTAL_DEBLOCKING == dir && m_pCurrentFrame->m_chroma_format == 2)
            {
                // create clipping values
                thresholds[4 + planeOffset] = (uint8_t) (pClipTab[pStrength[4]]);
                thresholds[5 + planeOffset] = (uint8_t) (pClipTab[pStrength[5]]);
                thresholds[6 + planeOffset] = (uint8_t) (pClipTab[pStrength[6]]);
                thresholds[7 + planeOffset] = (uint8_t) (pClipTab[pStrength[7]]);
                thresholds[8 + planeOffset] = (uint8_t) (pClipTab[pStrength[8]]);
                thresholds[9 + planeOffset] = (uint8_t) (pClipTab[pStrength[9]]);
                thresholds[10 + planeOffset] = (uint8_t) (pClipTab[pStrength[10]]);
                thresholds[11 + planeOffset] = (uint8_t) (pClipTab[pStrength[11]]);
                thresholds[12 + planeOffset] = (uint8_t) (pClipTab[pStrength[12]]);
                thresholds[13 + planeOffset] = (uint8_t) (pClipTab[pStrength[13]]);
                thresholds[14 + planeOffset] = (uint8_t) (pClipTab[pStrength[14]]);
                thresholds[15 + planeOffset] = (uint8_t) (pClipTab[pStrength[15]]);
            }
            else
            {
                thresholds[4 + 8*nPlane] = (uint8_t) (pClipTab[pStrength[8]]);
                thresholds[5 + 8*nPlane] = (uint8_t) (pClipTab[pStrength[9]]);
                thresholds[6 + 8*nPlane] = (uint8_t) (pClipTab[pStrength[10]]);
                thresholds[7 + 8*nPlane] = (uint8_t) (pClipTab[pStrength[11]]);
            }
        }
        else
        { // need to duplicate info
            if (HORIZONTAL_DEBLOCKING == dir && m_pCurrentFrame->m_chroma_format == 2)
            {
                if (*((uint32_t *) (pStrength)))
                {
                    MFX_INTERNAL_CPY(&thresholds[0 + 16], &thresholds[0], 4);
                    alpha[2] = alpha[0];
                    beta[2] = beta[0];
                }

                if (*((uint32_t *) (pStrength + 4)))
                {
                    MFX_INTERNAL_CPY(&thresholds[4 + 16], &thresholds[4], 4);
                    alpha[1 + 2] = alpha[1];
                    beta[1 + 2] = beta[1];
                }

                if (*((uint32_t *) (pStrength + 8)))
                {
                    MFX_INTERNAL_CPY(&thresholds[8 + 16], &thresholds[8], 4);
                    alpha[1 + 2] = alpha[1];
                    beta[1 + 2] = beta[1];
                }

                if (*((uint32_t *) (pStrength + 12)))
                {
                    MFX_INTERNAL_CPY(&thresholds[12 + 16], &thresholds[12], 4);
                    alpha[1 + 2] = alpha[1];
                    beta[1 + 2] = beta[1];
                }
            }
            else
            {
                if (*((uint32_t *) (pStrength)))
                {
                    MFX_INTERNAL_CPY(&thresholds[0 + 8], &thresholds[0], 4);
                    alpha[2] = alpha[0];
                    beta[2] = beta[0];
                }

                if (*((uint32_t *) (pStrength + 8)))
                {
                    MFX_INTERNAL_CPY(&thresholds[0 + 12], &thresholds[4], 4);
                    alpha[1 + 2] = alpha[1];
                    beta[1 + 2] = beta[1];
                }
            }
        }
    } // for plane

    if (bit_depth_chroma > 8)
    {
        FilterDeblockingChromaEdge<uint16_t>((uint16_t *)m_deblockingParams.pChroma, m_deblockingParams.pitch_chroma,
                                    alpha, beta,
                                    thresholds, pStrength,
                                    bit_depth_chroma,
                                    m_pCurrentFrame->m_chroma_format,
                                    dir);
    }
    else
    {
        FilterDeblockingChromaEdge<uint8_t>(m_deblockingParams.pChroma, m_deblockingParams.pitch_chroma,
                                    alpha, beta,
                                    thresholds, pStrength,
                                    bit_depth_chroma,
                                    m_pCurrentFrame->m_chroma_format,
                                    dir);
   }
} // void H264SegmentDecoder::DeblockChroma422(uint32_t dir)

void H264SegmentDecoder::InitDeblockingOnce()
{
    // load slice header
    const H264SliceHeader *pHeader = (m_bFrameDeblocking) ?
              (m_pCurrentFrame->GetAU(m_pCurrentFrame->GetNumberByParity(m_field_index))->GetSliceByNumber(m_gmbinfo->mbs[m_CurMBAddr].slice_id)->GetSliceHeader()) :
              (m_pSliceHeader);

    m_deblockingParams.nMaxMVector = (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec) ? (2) : (4);
    m_deblockingParams.MBFieldCoded = (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec);

    // set slice's variables
    m_deblockingParams.nAlphaC0Offset = pHeader->slice_alpha_c0_offset;
    m_deblockingParams.nBetaOffset = pHeader->slice_beta_offset;

    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 0)->m_RefPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 1)->m_RefPicList;
    m_pFields[0] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 0)->m_Flags;
    m_pFields[1] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 1)->m_Flags;

    m_pSliceHeader = pHeader;

    m_cur_mb.isInited = false;
    m_cur_mb.GlobalMacroblockInfo = &m_gmbinfo->mbs[m_CurMBAddr];
    m_cur_mb.LocalMacroblockInfo = &m_mbinfo.mbs[m_CurMBAddr];
    m_cur_mb.MVs[0] = &m_gmbinfo->MV[0][m_CurMBAddr];
    m_cur_mb.MVs[1] = &m_gmbinfo->MV[1][m_CurMBAddr];
    m_cur_mb.RefIdxs[0] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[0];
    m_cur_mb.RefIdxs[1] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[1];

    InitDeblockingSliceBoundaryInfo();
}

void H264SegmentDecoder::InitDeblockingSliceBoundaryInfo()
{
    m_deblockingParams.m_isSameSlice = m_isSliceGroups ? 0 : 1;

    if (!m_isSliceGroups && DEBLOCK_FILTER_ON_NO_SLICE_EDGES != m_pSliceHeader->disable_deblocking_filter_idc && m_pSliceHeader->slice_type != INTRASLICE)
    {
        m_deblockingParams.m_isSameSlice = 0;

        int32_t MBYAdjust = 0;
        if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec && m_field_index)
            MBYAdjust = mb_height/2;

        int32_t prevRow = m_CurMB_Y ? (m_CurMB_Y  + MBYAdjust - 1)*mb_width : MBYAdjust*mb_width;

        if (m_isMBAFF)
            prevRow = m_CurMB_Y > 3 ? (m_CurMB_Y  - 3)*mb_width : 0;

        if (m_iSliceNumber == m_gmbinfo->mbs[prevRow].slice_id)
        {
            m_deblockingParams.m_isSameSlice = 1;
        }
        else
        {
            H264DecoderFrameInfo * info = m_pCurrentFrame->GetAU(m_pCurrentFrame->GetNumberByParity(m_field_index));
            m_deblockingParams.m_isSameSlice = (info->m_isBExist && info->m_isPExist) ? 0 : 1;
        }
    }
}

void H264SegmentDecoder::ResetDeblockingVariables()
{
    H264Slice *pSlice;
    const H264SliceHeader *pHeader;

    if (m_bFrameDeblocking) {
        pSlice = m_pCurrentFrame->GetAU(m_pCurrentFrame->GetNumberByParity(m_field_index))->GetSliceByNumber(m_gmbinfo->mbs[m_CurMBAddr].slice_id);
        pHeader = pSlice->GetSliceHeader();
    } else {
        pSlice = m_pSlice;
        pHeader = m_pSliceHeader;
    }

    if (m_isSliceGroups)
    {
        int32_t MBYAdjust = 0;
        if (m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE && m_field_index)
            MBYAdjust = mb_height/2;

        m_CurMB_X = (m_CurMBAddr % mb_width);
        m_CurMB_Y = (m_CurMBAddr / mb_width) - MBYAdjust;

        InitDeblockingOnce();
    }
    else
    {
        if (m_cur_mb.isInited)
        {
            m_cur_mb.GlobalMacroblockInfo++;
            m_cur_mb.LocalMacroblockInfo++;
            m_cur_mb.MVs[0]++;
            m_cur_mb.MVs[1]++;
            m_cur_mb.RefIdxs[0] = &m_cur_mb.GlobalMacroblockInfo->refIdxs[0];
            m_cur_mb.RefIdxs[1] = &m_cur_mb.GlobalMacroblockInfo->refIdxs[1];
        }

        m_cur_mb.isInited = true;
    }

    int32_t pixel_luma_sz    = bit_depth_luma > 8 ? 2 : 1;
    int32_t pixel_chroma_sz  = bit_depth_chroma > 8 ? 2 : 1;

    // prepare macroblock variables
    int32_t mbXOffset = m_CurMB_X * 16;
    int32_t mbYOffset = m_CurMB_Y * 16;

    int32_t color_format = m_pCurrentFrame->m_chroma_format;

    PlanePtrYCommon pY;
    PlaneUVCommon *pU, *pV;
    int32_t offset;
    int32_t pitch_luma, pitch_chroma;

    // load planes
    pY = m_pYPlane;
    pU = m_pUVPlane;
    pV = NULL;

    pitch_luma = m_uPitchLuma;
    pitch_chroma = m_uPitchChroma;

    m_deblockingParams.pitch_luma = pitch_luma;
    m_deblockingParams.pitch_chroma = pitch_chroma;

    offset = mbXOffset + (mbYOffset * pitch_luma);
    pY += offset*pixel_luma_sz;

    offset = mbXOffset +  ((mbYOffset >> (int32_t)(color_format <= 1)) * pitch_chroma);
    pU += offset*pixel_chroma_sz;
    pV += offset*pixel_chroma_sz;

    // set external edge variables
    m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = (m_CurMB_X != 0);
    m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = (m_CurMB_Y != 0);

    if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == pHeader->disable_deblocking_filter_idc)
    {
        // don't filter at slice boundaries
        if (m_CurMB_X)
        {
            if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                m_gmbinfo->mbs[m_CurMBAddr - 1].slice_id)
                m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
        }

        if (m_CurMB_Y)
        {
            if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                m_gmbinfo->mbs[m_CurMBAddr - mb_width].slice_id)
                m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
        }
    }

    // reset external edges strength
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING], 0);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING], 0);

    // set neighbour addreses
    m_deblockingParams.nNeighbour[VERTICAL_DEBLOCKING] = m_CurMBAddr - 1;
    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width;

    // set deblocking flag(s)
    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = 0;
    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = 0;


    // save variables
    m_deblockingParams.pLuma = pY;
    m_deblockingParams.pChroma = pU;
} // void H264SegmentDecoder::ResetDeblockingVariables(DeblockingParameters *pParams)

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
