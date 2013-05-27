/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2012 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder.h"
#include "umc_h264_dec_deblocking.h"
#include "umc_h264_frame_info.h"

namespace UMC
{

void H264SegmentDecoder::DeblockMacroblockISliceMBAFF()
{
    Ipp32s mbtype = (m_gmbinfo->mbs + m_CurMBAddr)->mbtype;

    if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype)) {
        return;
    }

    // prepare deblocking parameters
    ResetDeblockingVariablesMBAFF();

    if (!m_deblockingParams.isNeedToDecblock)
        return;

    PrepareDeblockingParametersISliceMBAFF();

    if (m_deblockingParams.deblockChroma)
    {
        Ipp32u color_format = m_pCurrentFrame->m_chroma_format;

        if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
            (this->*DeblockChroma[color_format])(VERTICAL_DEBLOCKING);
        else
            (this->*DeblockChromaVerticalMBAFF[color_format])();
        if (0 == m_deblockingParams.ExtraHorizontalEdge)
            (this->*DeblockChroma[color_format])(HORIZONTAL_DEBLOCKING);
        else
            (this->*DeblockChromaHorizontalMBAFF[color_format])();
    }

    // perform luma deblocking
    if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
        DeblockLuma(VERTICAL_DEBLOCKING);
    else
        DeblockLumaVerticalMBAFF();

    if (0 == m_deblockingParams.ExtraHorizontalEdge)
        DeblockLuma(HORIZONTAL_DEBLOCKING);
    else
        DeblockLumaHorizontalMBAFF();

} // void H264SegmentDecoder::DeblockMacroblockISliceMBAFF()

void H264SegmentDecoder::DeblockMacroblockPSliceMBAFF()
{
/*
    if (1) {
        DeblockMacroblockBSliceMBAFF();
        return;
    }*/

    InitDeblockingSliceBoundaryInfo();

    if (!m_deblockingParams.m_isSameSlice)
    {
        DeblockMacroblockBSliceMBAFF();
        return;
    }


    Ipp32s mbtype = (m_gmbinfo->mbs + m_CurMBAddr)->mbtype;

    if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype)) {
        return;
    }

    // prepare deblocking parameters
    ResetDeblockingVariablesMBAFF();

    if (!m_deblockingParams.isNeedToDecblock)
        return;

    PrepareDeblockingParametersPSliceMBAFF();

    // perform chroma deblocking
    if (m_deblockingParams.deblockChroma)
    {
        Ipp32u color_format = m_pCurrentFrame->m_chroma_format;

        if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
            (this->*DeblockChroma[color_format])(VERTICAL_DEBLOCKING);
        else
            (this->*DeblockChromaVerticalMBAFF[color_format])();
        if (0 == m_deblockingParams.ExtraHorizontalEdge)
            (this->*DeblockChroma[color_format])(HORIZONTAL_DEBLOCKING);
        else
            (this->*DeblockChromaHorizontalMBAFF[color_format])();
    }

    // perform luma deblocking
    if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
        DeblockLuma(VERTICAL_DEBLOCKING);
    else
        DeblockLumaVerticalMBAFF();

    if (0 == m_deblockingParams.ExtraHorizontalEdge)
        DeblockLuma(HORIZONTAL_DEBLOCKING);
    else
        DeblockLumaHorizontalMBAFF();
} // void H264SegmentDecoder::DeblockMacroblockPSliceMBAFF(Ipp32s m_CurMBAddr)

void H264SegmentDecoder::DeblockMacroblockBSliceMBAFF()
{
    Ipp32s mbtype = (m_gmbinfo->mbs + m_CurMBAddr)->mbtype;

    if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype)) {
        return;
    }
    // prepare deblocking parameters
    ResetDeblockingVariablesMBAFF();

    if (!m_deblockingParams.isNeedToDecblock)
        return;

    PrepareDeblockingParametersBSliceMBAFF();

    // perform chroma deblocking
    {
        Ipp32u color_format = m_pCurrentFrame->m_chroma_format;

        if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
            (this->*DeblockChroma[color_format])(VERTICAL_DEBLOCKING);
        else
            (this->*DeblockChromaVerticalMBAFF[color_format])();
        if (0 == m_deblockingParams.ExtraHorizontalEdge)
            (this->*DeblockChroma[color_format])(HORIZONTAL_DEBLOCKING);
        else
            (this->*DeblockChromaHorizontalMBAFF[color_format])();
    }

    // perform luma deblocking
    if (0 == m_deblockingParams.UseComplexVerticalDeblocking)
        DeblockLuma(VERTICAL_DEBLOCKING);
    else
        DeblockLumaVerticalMBAFF();


    if (0 == m_deblockingParams.ExtraHorizontalEdge)
        DeblockLuma(HORIZONTAL_DEBLOCKING);
    else
        DeblockLumaHorizontalMBAFF();

} // void H264SegmentDecoder::DeblockMacroblockBSliceMBAFF(Ipp32s m_CurMBAddr)

void H264SegmentDecoder::ResetDeblockingVariablesMBAFF()
{
    PlanePtrYCommon   pY;
    PlanePtrUVCommon  pU;
    PlanePtrUVCommon  pV;
    Ipp32s offset;
    Ipp32s mbXOffset, mbYOffset;
    Ipp32s pitch_luma = m_pCurrentFrame->pitch_luma();
    Ipp32s pitch_chroma = m_pCurrentFrame->pitch_chroma();
    Ipp32s nCurrMB_X, nCurrMB_Y;
    const H264SliceHeader *pHeader;
    Ipp32s nFieldMacroblockMode;
    bool nv12_support = (m_pCurrentFrame->GetColorFormat() == NV12) && !deblocking_IL;
    Ipp32s disable_deblocking_filter_idc;
    Ipp32s disable_deblocking_filter_idc_from_stream;

    // load slice header
    pHeader = (m_bFrameDeblocking) ?
            (m_pCurrentFrame->GetAU(m_field_index)->GetSliceByNumber(m_gmbinfo->mbs[m_CurMBAddr].slice_id)->GetSliceHeader()) :
            (m_pSliceHeader);
 
    if (deblocking_IL) {
        disable_deblocking_filter_idc = pHeader->disable_inter_layer_deblocking_filter_idc;
        disable_deblocking_filter_idc_from_stream = pHeader->disable_inter_layer_deblocking_filter_idc_from_stream;
    } else {
        disable_deblocking_filter_idc = pHeader->disable_deblocking_filter_idc;
        disable_deblocking_filter_idc_from_stream = pHeader->disable_deblocking_filter_idc_from_stream;
    }

    if (deblocking_IL || m_bFrameDeblocking) {
        m_deblockingParams.deblockEdgesStage2 = deblocking_stage2 &&
            (disable_deblocking_filter_idc_from_stream == DEBLOCK_FILTER_ON_2_PASS ||
            disable_deblocking_filter_idc_from_stream == DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA);
    } else {
        m_deblockingParams.deblockEdgesStage2 = disable_deblocking_filter_idc == DEBLOCK_FILTER_ON_2_PASS ? 1 : 0;
    }

    Ipp32s pixel_luma_sz    = bit_depth_luma > 8 ? 2 : 1;
    Ipp32s pixel_chroma_sz  = bit_depth_chroma > 8 ? 2 : 1;

    if (deblocking_IL)
    {
        H264DecoderLayerDescriptor * layerMb = &m_mbinfo.layerMbs[(pHeader->ref_layer_dq_id >> 4)];
        pY = layerMb->m_pYPlane;
        pU = layerMb->m_pUPlane;
        pV = layerMb->m_pVPlane;

        pitch_luma = (m_pCurrentFrame->lumaSize().width + 32)* pixel_luma_sz;
        pitch_chroma = (m_pCurrentFrame->chromaSize().width + 16)* pixel_chroma_sz;
    } else {
        // load planes
        pY = m_pYPlane;
        if (nv12_support) {
            pU = m_pUVPlane;
            pV = NULL;
        } else {
            pU = m_pUPlane;
            pV = m_pVPlane;
        }

        pitch_luma = m_uPitchLuma;
        pitch_chroma = m_uPitchChroma;
    }

    m_deblockingParams.pitch_luma = pitch_luma;
    m_deblockingParams.pitch_chroma = pitch_chroma;

    // prepare macroblock variables
    nCurrMB_X = ((m_CurMBAddr>>1) % mb_width);
    nCurrMB_Y = ((m_CurMBAddr>>1) / mb_width)*2 + (m_CurMBAddr&1);
    mbXOffset = nCurrMB_X * 16;
    mbYOffset = nCurrMB_Y * 16;

    // calc plane's offsets
    offset = mbXOffset + (mbYOffset * pitch_luma);
    pY += offset*pixel_luma_sz;

    m_deblockingParams.isNeedToDecblock = 1;
    m_deblockingParams.deblockChroma = 1;

    // obtain macroblock mode
    nFieldMacroblockMode = GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr]);
    // set external edge variables
    // there are only two values:
    // zero - do not deblocking
    // nonzero - do deblocking
    m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = nCurrMB_X;
    m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = (nFieldMacroblockMode) ? (1 < nCurrMB_Y) : (nCurrMB_Y);

    if (disable_deblocking_filter_idc_from_stream >= DEBLOCK_FILTER_ON_NO_CHROMA)
    {
        m_deblockingParams.deblockChroma = 0;
    }

    if (m_deblockingParams.deblockEdgesStage2)
    {
        m_deblockingParams.isNeedToDecblock = 0;

        if ( m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
        {
            if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                m_gmbinfo->mbs[m_CurMBAddr - 2].slice_id)
                m_deblockingParams.isNeedToDecblock = 1;
        }

        if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
        {
            if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2].slice_id
                && !(!nFieldMacroblockMode && (m_CurMBAddr & 1)))
                m_deblockingParams.isNeedToDecblock = 1;
        }
    } else if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == disable_deblocking_filter_idc) {
        if (deblocking_stage2) {
            m_deblockingParams.isNeedToDecblock = 0;
        } else {
            // don't filter at slice boundaries
            if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
            {
                if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                    m_gmbinfo->mbs[m_CurMBAddr - 2].slice_id)
                    m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
            }

            if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
            {
                if (m_gmbinfo->mbs[m_CurMBAddr].slice_id !=
                    m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2].slice_id
                    && !(!nFieldMacroblockMode && (m_CurMBAddr & 1)))
                    m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            }
        }
    } else {
        if (deblocking_stage2) {
            m_deblockingParams.isNeedToDecblock = 0;
        }
    }

    switch (m_pCurrentFrame->m_chroma_format)
    {
    case CHROMA_FORMAT_420:
        if (nv12_support && !deblocking_IL)
            offset = mbXOffset + ((mbYOffset * pitch_chroma) >> 1);
        else
            offset = (mbXOffset + (mbYOffset * pitch_chroma)) >> 1;
        break;

    case CHROMA_FORMAT_422:
        offset = (mbXOffset >> 1) + (mbYOffset * pitch_chroma);
        break;

    case CHROMA_FORMAT_400:
        offset = 0;
    case CHROMA_FORMAT_444:
        break;

    default:
        VM_ASSERT(false);
        break;
    }

    pU += offset*pixel_chroma_sz;
    pV += offset*pixel_chroma_sz;

    // obtain macroblock mode
    // correct y, u & v offset
    if (nFieldMacroblockMode)
    {
        if (m_CurMBAddr & 1)
        {
            pY -= 15 * pitch_luma * pixel_luma_sz;

            Ipp32s ff_offset = (m_pCurrentFrame->m_chroma_format != 1) ? 15 : 7;
            pU -= ff_offset * pitch_chroma * pixel_chroma_sz;
            pV -= ff_offset * pitch_chroma * pixel_chroma_sz;
        }

        pitch_luma *= 2;
        pitch_chroma *= 2;
    }

    // set external edge variables
    // there are only two values:
    // zero - do not deblocking
    // nonzero - do deblocking

    // set left MB couple adresses
    {
        m_deblockingParams.nLeft[0] = m_CurMBAddr - 2 - (m_CurMBAddr&1);
        m_deblockingParams.nLeft[1] = m_deblockingParams.nLeft[0] + 1;
    }

    // set "complex deblocking" flag
    // when left & current macroblock are coded in different modes
    m_deblockingParams.UseComplexVerticalDeblocking = 0;
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        if (nFieldMacroblockMode !=
            GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr - 2]))
        {
            // when left macroblocks aren't intra blocks
            // or have different QP
            //if (IS_INTER_MBTYPE((m_gmbinfo->mbs + m_deblockingParams.nLeft[0])->mbtype) ||
            //    IS_INTER_MBTYPE((m_gmbinfo->mbs + m_deblockingParams.nLeft[1])->mbtype) ||
            //    (m_mbinfo.mbs[m_deblockingParams.nLeft[0]].QP != m_mbinfo.mbs[m_deblockingParams.nLeft[1]].QP))
            //{
                m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
                m_deblockingParams.UseComplexVerticalDeblocking = 1;
            //}
        }
    }

    // obtain extra horizontal edge condition
    // when above macroblock is field coded and
    // current macroblock is frame coded
    m_deblockingParams.ExtraHorizontalEdge = 0;
    if ((0 == (m_CurMBAddr & 1)) &&
        (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING]))
    {
        if ((0 == nFieldMacroblockMode) &&
            (0 != GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2])))
            m_deblockingParams.ExtraHorizontalEdge = 1;
    }

    // reset external edges strength
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING], 0);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING], 0);

    // set neighbour addreses
    m_deblockingParams.nNeighbour[VERTICAL_DEBLOCKING] = m_CurMBAddr - 2;

    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        if (0 == nFieldMacroblockMode)
        {
            if ((m_CurMBAddr&1)==0)
            {
                if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2]))
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2;
                else
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2 + 1;
            }
            else
            {
                m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - 1;
            }
        }
        else
        {
            if ((m_CurMBAddr&1)==0)
            {
                if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2]))
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2;
                else
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2 + 1;
            }
            else
            {
                if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr - mb_width * 2]))
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2;
                else
                    m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] = m_CurMBAddr - mb_width * 2;
            }
        }
    }

    // set deblocking flag(s)
    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = 0;
    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = 0;

    // save variables
    m_deblockingParams.pLuma = pY;
    if (nv12_support)
    {
        m_deblockingParams.pChroma[0] = pU;
        m_deblockingParams.pChroma[1] = pU + 1;
    }
    else
    {
        m_deblockingParams.pChroma[0] = pU;
        m_deblockingParams.pChroma[1] = pV;
    }

    m_deblockingParams.pitch_luma = pitch_luma;
    m_deblockingParams.pitch_chroma = pitch_chroma;

    m_deblockingParams.nMaxMVector = (nFieldMacroblockMode) ? (2) : (4);
    m_deblockingParams.MBFieldCoded = nFieldMacroblockMode;

    // set slice's variables
    m_deblockingParams.nAlphaC0Offset = pHeader->slice_alpha_c0_offset;
    m_deblockingParams.nBetaOffset = pHeader->slice_beta_offset;

    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 0)->m_RefPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 1)->m_RefPicList;
    m_pFields[0] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 0)->m_Flags;
    m_pFields[1] = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[m_CurMBAddr].slice_id, 1)->m_Flags;

    m_cur_mb.GlobalMacroblockInfo = &m_gmbinfo->mbs[m_CurMBAddr];
    m_cur_mb.LocalMacroblockInfo = &m_mbinfo.mbs[m_CurMBAddr];
    m_cur_mb.MVs[0] = GetMVs(m_gmbinfo, 0, m_CurMBAddr);
    m_cur_mb.MVs[1] = GetMVs(m_gmbinfo, 1, m_CurMBAddr);
    m_cur_mb.RefIdxs[0] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[0];
    m_cur_mb.RefIdxs[1] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[1];

    m_deblockingParams.deblockInfo.pAlpha = m_deblockingParams.Alpha;
    m_deblockingParams.deblockInfo.pBeta = m_deblockingParams.Beta;
    m_deblockingParams.deblockInfo.pThresholds = m_deblockingParams.Clipping;

} // void H264SegmentDecoder::ResetDeblockingVariablesMBAFF(DeblockingParametersMBAFF *pParams)

void H264SegmentDecoder::DeblockLumaVerticalMBAFF()
{
    //
    // step 1. Perform complex deblocking on external edge
    //
    {
        Ipp32s AlphaC0Offset = m_deblockingParams.nAlphaC0Offset;
        Ipp32s BetaOffset = m_deblockingParams.nBetaOffset;
        Ipp32s pmq_QP = m_mbinfo.mbs[m_CurMBAddr].QP;
        Ipp32s pmp_QP;
        Ipp8u *pClipTab;
        Ipp32s QP;
        Ipp32s index;
        Ipp8u *pStrength = m_deblockingParams.StrengthComplex;
        Ipp32s i;

        m_deblockingParams.deblockInfo.pBs = pStrength;
        m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_luma;
        m_deblockingParams.deblockInfo.pBs = m_deblockingParams.StrengthComplex;

        // prepare variables
        for (i = 0;i < 2;i += 1)
        {
            // get upper neighbour block QP
            pmp_QP = m_mbinfo.mbs[m_deblockingParams.nLeft[i]].QP;

            // luma variables
            QP = (pmp_QP + pmq_QP + 1) >> 1 ;

            // external edge variables
            index = IClip(0, 51, QP + BetaOffset);
            m_deblockingParams.deblockInfo.pBeta[i] = (Ipp8u)(BETA_TABLE[index]);

            index = IClip(0, 51, QP + AlphaC0Offset);
            m_deblockingParams.deblockInfo.pAlpha[i] = (Ipp8u)(ALPHA_TABLE[index]);
            pClipTab = CLIP_TAB[index];

            // create clipping values
            m_deblockingParams.deblockInfo.pThresholds[i * 4 + 0] = (Ipp8u)(pClipTab[pStrength[i * 4 + 0]]);
            m_deblockingParams.deblockInfo.pThresholds[i * 4 + 1] = (Ipp8u)(pClipTab[pStrength[i * 4 + 1]]);
            m_deblockingParams.deblockInfo.pThresholds[i * 4 + 2] = (Ipp8u)(pClipTab[pStrength[i * 4 + 2]]);
            m_deblockingParams.deblockInfo.pThresholds[i * 4 + 3] = (Ipp8u)(pClipTab[pStrength[i * 4 + 3]]);
        }

        // perform deblocking
        if (0 == GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr]))
        {
            if (bit_depth_luma > 8)
            {
                IppDeblocking16u[8]((Ipp16u*)m_deblockingParams.pLuma,
                                    m_deblockingParams.pitch_luma * 2,
                                    &m_deblockingParams.deblockInfo.pAlpha[0],
                                    &m_deblockingParams.deblockInfo.pBeta[0],
                                    m_deblockingParams.deblockInfo.pThresholds,
                                    pStrength,
                                    bit_depth_luma);
                IppDeblocking16u[8]((Ipp16u*)m_deblockingParams.pLuma + m_deblockingParams.pitch_luma,
                                    m_deblockingParams.pitch_luma * 2,
                                    &m_deblockingParams.deblockInfo.pAlpha[1],
                                    &m_deblockingParams.deblockInfo.pBeta[1],
                                    m_deblockingParams.deblockInfo.pThresholds + 4,
                                    pStrength + 4,
                                    bit_depth_luma);
            }
            else
            {
                m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_luma * 2;
                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pLuma;

                IppDeblocking[8](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pLuma + m_deblockingParams.pitch_luma;

                m_deblockingParams.deblockInfo.pAlpha += 1;
                m_deblockingParams.deblockInfo.pBeta += 1;
                m_deblockingParams.deblockInfo.pThresholds += 4;
                m_deblockingParams.deblockInfo.pBs += 4;
                IppDeblocking[8](&m_deblockingParams.deblockInfo);
                m_deblockingParams.deblockInfo.pAlpha -= 1;
                m_deblockingParams.deblockInfo.pBeta -= 1;
                m_deblockingParams.deblockInfo.pThresholds -= 4;
                m_deblockingParams.deblockInfo.pBs -= 4;
            }
        }
        else
        {
            if (bit_depth_luma > 8)
            {
                IppDeblocking16u[8]((Ipp16u*)m_deblockingParams.pLuma,
                                    m_deblockingParams.pitch_luma,
                                    &m_deblockingParams.deblockInfo.pAlpha[0],
                                    &m_deblockingParams.deblockInfo.pBeta[0],
                                    m_deblockingParams.deblockInfo.pThresholds,
                                    pStrength,
                                    bit_depth_luma);
                IppDeblocking16u[8]((Ipp16u*)m_deblockingParams.pLuma + m_deblockingParams.pitch_luma * 8,
                                    m_deblockingParams.pitch_luma,
                                    &m_deblockingParams.deblockInfo.pAlpha[1],
                                    &m_deblockingParams.deblockInfo.pBeta[1],
                                    m_deblockingParams.deblockInfo.pThresholds + 4,
                                    pStrength + 4,
                                    bit_depth_luma);
            }
            else
            {
                m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_luma;
                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pLuma;

                IppDeblocking[8](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pLuma + m_deblockingParams.pitch_luma * 8;
                m_deblockingParams.deblockInfo.pAlpha += 1;
                m_deblockingParams.deblockInfo.pBeta += 1;
                m_deblockingParams.deblockInfo.pThresholds += 4;
                m_deblockingParams.deblockInfo.pBs += 4;
                IppDeblocking[8](&m_deblockingParams.deblockInfo);
                m_deblockingParams.deblockInfo.pAlpha -= 1;
                m_deblockingParams.deblockInfo.pBeta -= 1;
                m_deblockingParams.deblockInfo.pThresholds -= 4;
                m_deblockingParams.deblockInfo.pBs -= 4;
            }
        }
    }

    //
    // step 2. Perform complex deblocking on internal edges
    //
    DeblockLuma(VERTICAL_DEBLOCKING);

} // void H264SegmentDecoder::DeblockLumaVerticalMBAFF()

void H264SegmentDecoder::DeblockChromaVerticalMBAFF400()
{
    // there is nothing to do

} // void H264SegmentDecoder::DeblockChromaVerticalMBAFF400()

void H264SegmentDecoder::DeblockChromaVerticalMBAFF420()
{
    bool nv12_support = m_pCurrentFrame->GetColorFormat() == NV12 && !deblocking_IL;

    Ipp32s AlphaC0Offset = m_deblockingParams.nAlphaC0Offset;
    Ipp32s BetaOffset = m_deblockingParams.nBetaOffset;
    Ipp32s pmq_QP = m_mbinfo.mbs[m_CurMBAddr].QP;
    Ipp32s pmp_QP;
    Ipp8u *pClipTab;
    Ipp32s QP;
    Ipp32s index;
    Ipp8u *pStrength = m_deblockingParams.StrengthComplex;
    Ipp32s nPlane;
    Ipp32s chroma_qp_offset = ~(m_pPicParamSet->chroma_qp_index_offset[0]);

    m_deblockingParams.deblockInfo.pBs = pStrength;
    m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma;

    if (nv12_support)
    {
        //
        // step 1. Perform complex deblocking on external edge
        //
        {
            for (nPlane = 0; nPlane < 2; nPlane += 1)
            {
                // prepare variables
                //if (chroma_qp_offset != m_pPicParamSet->chroma_qp_index_offset[nPlane])
                {
                    Ipp32s i;

                    chroma_qp_offset = m_pPicParamSet->chroma_qp_index_offset[nPlane];

                    // prepare variables
                    for (i = 0;i < 2;i += 1)
                    {
                        // get upper neighbour block QP
                        pmp_QP = m_mbinfo.mbs[m_deblockingParams.nLeft[i]].QP;

                        // external edge variables
                        QP = (QP_SCALE_CR[IClip(0, 51, pmp_QP + chroma_qp_offset)] +
                                QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)] + 1) >> 1;

                        // external edge variables
                        index = IClip(0, 51, QP + BetaOffset);
                        m_deblockingParams.deblockInfo.pBeta[i + 2*nPlane] = (Ipp8u)(BETA_TABLE[index]);

                        index = IClip(0, 51, QP + AlphaC0Offset);
                        m_deblockingParams.deblockInfo.pAlpha[i + 2*nPlane] = (Ipp8u)(ALPHA_TABLE[index]);
                        pClipTab = CLIP_TAB[index];

                        // create clipping values
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 0 + 8*nPlane] = (Ipp8u)(pClipTab[pStrength[i * 4 + 0]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 1 + 8*nPlane] = (Ipp8u)(pClipTab[pStrength[i * 4 + 1]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 2 + 8*nPlane] = (Ipp8u)(pClipTab[pStrength[i * 4 + 2]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 3 + 8*nPlane] = (Ipp8u)(pClipTab[pStrength[i * 4 + 3]]);
                    }
                }
            }

            // perform deblocking
            if (0 == GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr]))
            {
                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[0];
                m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma * 2;

                IppDeblocking[12](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[0] + m_deblockingParams.pitch_chroma;

                m_deblockingParams.deblockInfo.pAlpha += 1;
                m_deblockingParams.deblockInfo.pBeta += 1;
                m_deblockingParams.deblockInfo.pThresholds += 4;
                m_deblockingParams.deblockInfo.pBs += 4;

                IppDeblocking[12](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pAlpha -= 1;
                m_deblockingParams.deblockInfo.pBeta -= 1;
                m_deblockingParams.deblockInfo.pThresholds -= 4;
                m_deblockingParams.deblockInfo.pBs -= 4;

            }
            else
            {
                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[0];
                m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma;

                IppDeblocking[12](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[0] + 4 * m_deblockingParams.pitch_chroma;
                m_deblockingParams.deblockInfo.pAlpha += 1;
                m_deblockingParams.deblockInfo.pBeta += 1;
                m_deblockingParams.deblockInfo.pThresholds += 4;
                m_deblockingParams.deblockInfo.pBs += 4;

                IppDeblocking[12](&m_deblockingParams.deblockInfo);

                m_deblockingParams.deblockInfo.pAlpha -= 1;
                m_deblockingParams.deblockInfo.pBeta -= 1;
                m_deblockingParams.deblockInfo.pThresholds -= 4;
                m_deblockingParams.deblockInfo.pBs -= 4;
            }
        }
    }
    else
    {

        //
        // step 1. Perform complex deblocking on external edge
        //
        {
            for (nPlane = 0; nPlane < 2; nPlane += 1)
            {
                // prepare variables
                if (chroma_qp_offset != m_pPicParamSet->chroma_qp_index_offset[nPlane])
                {
                    Ipp32s i;

                    chroma_qp_offset = m_pPicParamSet->chroma_qp_index_offset[nPlane];

                    // prepare variables
                    for (i = 0;i < 2;i += 1)
                    {
                        // get upper neighbour block QP
                        pmp_QP = m_mbinfo.mbs[m_deblockingParams.nLeft[i]].QP;

                        // external edge variables
                        QP = (QP_SCALE_CR[IClip(0, 51, pmp_QP + chroma_qp_offset)] +
                                QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)] + 1) >> 1;

                        // external edge variables
                        index = IClip(0, 51, QP + BetaOffset);
                        m_deblockingParams.deblockInfo.pBeta[i] = (Ipp8u)(BETA_TABLE[index]);

                        index = IClip(0, 51, QP + AlphaC0Offset);
                        m_deblockingParams.deblockInfo.pAlpha[i] = (Ipp8u)(ALPHA_TABLE[index]);
                        pClipTab = CLIP_TAB[index];

                        // create clipping values
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 0] = (Ipp8u)(pClipTab[pStrength[i * 4 + 0]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 1] = (Ipp8u)(pClipTab[pStrength[i * 4 + 1]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 2] = (Ipp8u)(pClipTab[pStrength[i * 4 + 2]]);
                        m_deblockingParams.deblockInfo.pThresholds[i * 4 + 3] = (Ipp8u)(pClipTab[pStrength[i * 4 + 3]]);
                    }
                }

                // perform deblocking
                if (0 == GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr]))
                {
                    if (bit_depth_chroma > 8)
                    {
                        IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane],
                                            m_deblockingParams.pitch_chroma * 2,
                                            &m_deblockingParams.deblockInfo.pAlpha[0],
                                            &m_deblockingParams.deblockInfo.pBeta[0],
                                            m_deblockingParams.deblockInfo.pThresholds,
                                            pStrength,
                                            bit_depth_chroma);
                        IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma,
                                            m_deblockingParams.pitch_chroma * 2,
                                            &m_deblockingParams.deblockInfo.pAlpha[1],
                                            &m_deblockingParams.deblockInfo.pBeta[1],
                                            m_deblockingParams.deblockInfo.pThresholds + 4,
                                            pStrength + 4,
                                            bit_depth_chroma);
                    }
                    else
                    {
                        m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[nPlane];
                        m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma * 2;

                        IppDeblocking[9](&m_deblockingParams.deblockInfo);

                        m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma;
                        m_deblockingParams.deblockInfo.pAlpha += 1;
                        m_deblockingParams.deblockInfo.pBeta += 1;
                        m_deblockingParams.deblockInfo.pThresholds += 4;
                        m_deblockingParams.deblockInfo.pBs += 4;

                        IppDeblocking[9](&m_deblockingParams.deblockInfo);

                        m_deblockingParams.deblockInfo.pAlpha -= 1;
                        m_deblockingParams.deblockInfo.pBeta -= 1;
                        m_deblockingParams.deblockInfo.pThresholds -= 4;
                        m_deblockingParams.deblockInfo.pBs -= 4;

                    }
                }
                else
                {
                    if (bit_depth_chroma > 8)
                    {
                        IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane],
                                            m_deblockingParams.pitch_chroma,
                                            &m_deblockingParams.deblockInfo.pAlpha[0],
                                            &m_deblockingParams.deblockInfo.pBeta[0],
                                            m_deblockingParams.deblockInfo.pThresholds,
                                            pStrength,
                                            bit_depth_chroma);
                        IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 4,
                                            m_deblockingParams.pitch_chroma,
                                            &m_deblockingParams.deblockInfo.pAlpha[1],
                                            &m_deblockingParams.deblockInfo.pBeta[1],
                                            m_deblockingParams.deblockInfo.pThresholds + 4,
                                            pStrength + 4,
                                            bit_depth_chroma);
                    }
                    else
                    {
                        m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[nPlane];
                        m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma;

                        IppDeblocking[9](&m_deblockingParams.deblockInfo);

                        m_deblockingParams.deblockInfo.pSrcDstPlane = m_deblockingParams.pChroma[nPlane] + 4 * m_deblockingParams.pitch_chroma;
                        m_deblockingParams.deblockInfo.pAlpha += 1;
                        m_deblockingParams.deblockInfo.pBeta += 1;
                        m_deblockingParams.deblockInfo.pThresholds += 4;
                        m_deblockingParams.deblockInfo.pBs += 4;

                        IppDeblocking[9](&m_deblockingParams.deblockInfo);

                        m_deblockingParams.deblockInfo.pAlpha -= 1;
                        m_deblockingParams.deblockInfo.pBeta -= 1;
                        m_deblockingParams.deblockInfo.pThresholds -= 4;
                        m_deblockingParams.deblockInfo.pBs -= 4;
                    }
                }
            }
        }
    }
    //
    // step 2. Perform complex deblocking on internal edges
    //
    (this->*DeblockChroma[CHROMA_FORMAT_420])(VERTICAL_DEBLOCKING);

} // void H264SegmentDecoder::DeblockChromaVerticalMBAFF420()

void H264SegmentDecoder::DeblockChromaVerticalMBAFF422()
{
    //
    // step 1. Perform complex deblocking on external edge
    //
    {
        Ipp8u Clipping[16];
        Ipp8u Alpha[2];
        Ipp8u Beta[2];
        Ipp32s AlphaC0Offset = m_deblockingParams.nAlphaC0Offset;
        Ipp32s BetaOffset = m_deblockingParams.nBetaOffset;
        Ipp32s pmq_QP = m_mbinfo.mbs[m_CurMBAddr].QP;
        Ipp32s pmp_QP;
        Ipp8u *pClipTab;
        Ipp32s QP;
        Ipp32s index;
        Ipp8u *pStrength = m_deblockingParams.StrengthComplex;
        Ipp32s nPlane;
        Ipp32s chroma_qp_offset = ~(m_pPicParamSet->chroma_qp_index_offset[0]);

        m_deblockingParams.deblockInfo.pBs = pStrength;
        m_deblockingParams.deblockInfo.srcDstStep = m_deblockingParams.pitch_chroma;

        for (nPlane = 0; nPlane < 2; nPlane += 1)
        {
            // prepare variables
            if (chroma_qp_offset != m_pPicParamSet->chroma_qp_index_offset[nPlane])
            {
                Ipp32s i;

                chroma_qp_offset = m_pPicParamSet->chroma_qp_index_offset[nPlane];

                // prepare variables
                for (i = 0;i < 2;i += 1)
                {
                    // get upper neighbour block QP
                    pmp_QP = m_mbinfo.mbs[m_deblockingParams.nLeft[i]].QP;

                    // external edge variables
                    QP = (QP_SCALE_CR[IClip(0, 51, pmp_QP + chroma_qp_offset)] +
                          QP_SCALE_CR[IClip(0, 51, pmq_QP + chroma_qp_offset)] + 1) >> 1;

                    // external edge variables
                    index = IClip(0, 51, QP + BetaOffset);
                    Beta[i] = (Ipp8u)(BETA_TABLE[index]);

                    index = IClip(0, 51, QP + AlphaC0Offset);
                    Alpha[i] = (Ipp8u)(ALPHA_TABLE[index]);
                    pClipTab = CLIP_TAB[index];

                    // create clipping values
                    Clipping[i * 4 + 0] = (Ipp8u)(pClipTab[pStrength[i * 4 + 0]]);
                    Clipping[i * 4 + 1] = (Ipp8u)(pClipTab[pStrength[i * 4 + 1]]);
                    Clipping[i * 4 + 2] = (Ipp8u)(pClipTab[pStrength[i * 4 + 2]]);
                    Clipping[i * 4 + 3] = (Ipp8u)(pClipTab[pStrength[i * 4 + 3]]);
                }
            }

            // perform deblocking
            if (0 == GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr]))
            {
                if (bit_depth_chroma > 8)
                {
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane],
                                        m_deblockingParams.pitch_chroma * 4,
                                        &Alpha[0],
                                        &Beta[0],
                                        Clipping,
                                        pStrength,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma,
                                        m_deblockingParams.pitch_chroma * 4,
                                        &Alpha[1],
                                        &Beta[1],
                                        Clipping + 4,
                                        pStrength + 4,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 2,
                                        m_deblockingParams.pitch_chroma * 4,
                                        &Alpha[0],
                                        &Beta[0],
                                        Clipping,
                                        pStrength,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 3,
                                        m_deblockingParams.pitch_chroma * 4,
                                        &Alpha[1],
                                        &Beta[1],
                                        Clipping + 4,
                                        pStrength + 4,
                                        bit_depth_chroma);
                }
                else
                {
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                }
            }
            else
            {
                if (bit_depth_chroma > 8)
                {
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane],
                                        m_deblockingParams.pitch_chroma,
                                        &Alpha[0],
                                        &Beta[0],
                                        Clipping,
                                        pStrength,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 4,
                                        m_deblockingParams.pitch_chroma,
                                        &Alpha[0],
                                        &Beta[0],
                                        Clipping,
                                        pStrength,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 8,
                                        m_deblockingParams.pitch_chroma,
                                        &Alpha[1],
                                        &Beta[1],
                                        Clipping + 4,
                                        pStrength + 4,
                                        bit_depth_chroma);
                    IppDeblocking16u[9]((Ipp16u*)m_deblockingParams.pChroma[nPlane] + m_deblockingParams.pitch_chroma * 12,
                                        m_deblockingParams.pitch_chroma,
                                        &Alpha[1],
                                        &Beta[1],
                                        Clipping + 4,
                                        pStrength + 4,
                                        bit_depth_chroma);
                }
                else
                {
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                    IppDeblocking[9](&m_deblockingParams.deblockInfo);
                }
            }
        }
    }

    //
    // step 2. Perform complex deblocking on internal edges
    //
    (this->*DeblockChroma[CHROMA_FORMAT_422])(VERTICAL_DEBLOCKING);

} // void H264SegmentDecoder::DeblockChromaVerticalMBAFF422()

void H264SegmentDecoder::DeblockChromaVerticalMBAFF444()
{
    /* this function is under development */

} // void H264SegmentDecoder::DeblockChromaVerticalMBAFF444()

void H264SegmentDecoder::DeblockLumaHorizontalMBAFF()
{
    Ipp8u bTmp[16];
    Ipp32s pitch = m_deblockingParams.pitch_luma;

    Ipp32s luma_pixel_sz = bit_depth_luma > 8 ? 2 : 1;

    //
    // luma deblocking
    //

    //
    // step 1. Deblock origin external edge
    //
    {
        // save internal edges strength
        CopyEdgeStrength(bTmp + 0, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0);
        CopyEdgeStrength(bTmp + 4, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4);
        CopyEdgeStrength(bTmp + 8, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8);
        CopyEdgeStrength(bTmp + 12, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12);
        // skip all internal edges
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, 0);
        // set pitch
        m_deblockingParams.pitch_luma *= 2;
        // perform deblocking
        DeblockLuma(HORIZONTAL_DEBLOCKING);
    }

    //
    // step 2. Deblock extra external edge
    //
    {
        // set extra edge strength
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING], m_deblockingParams.StrengthExtra);
        // correct neighbour MB info
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] += 1;
        // correct Y pointer
        m_deblockingParams.pLuma += pitch*luma_pixel_sz;
        // perform deblocking
        DeblockLuma(HORIZONTAL_DEBLOCKING);
        // restore values
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] -= 1;
        m_deblockingParams.pLuma -= pitch*luma_pixel_sz;
        m_deblockingParams.pitch_luma = pitch;
    }

    //
    // step 3. Deblock internal edges
    //
    {
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
        // set internal edge strength
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 0);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, bTmp + 4);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, bTmp + 8);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, bTmp + 12);
        // perform deblocking
        DeblockLuma(HORIZONTAL_DEBLOCKING);
        // restore strength
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, bTmp + 0);
    }

} // void H264SegmentDecoder::DeblockLumaHorizontalMBAFF()

void H264SegmentDecoder::DeblockChromaHorizontalMBAFF400()
{
    // there is nothing to do

} // void H264SegmentDecoder::DeblockChromaHorizontalMBAFF400()

void H264SegmentDecoder::DeblockChromaHorizontalMBAFF420()
{
    Ipp8u bTmp[16];
    Ipp32s pitch = m_deblockingParams.pitch_chroma;
    Ipp32s chroma_pixel_sz = bit_depth_chroma > 8 ? 2 : 1;
    ChromaDeblockingFunction pFunc = DeblockChroma[m_pCurrentFrame->m_chroma_format];

    //
    // chroma deblocking
    //

    //
    // step 1. Deblock origin external edge
    //
    {
        // save internal edges strength
        CopyEdgeStrength(bTmp + 0, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0);
        CopyEdgeStrength(bTmp + 4, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4);
        CopyEdgeStrength(bTmp + 8, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8);
        CopyEdgeStrength(bTmp + 12, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12);
        // skip all internal edges
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, 0);
        // set pitch
        m_deblockingParams.pitch_chroma *= 2;

        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
    }

    //
    // step 2. Deblock extra external edge
    //
    {
        // set extra edge strength
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING], m_deblockingParams.StrengthExtra);
        // correct neighbour MB info
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] += 1;
        // correct U & V pointer
        m_deblockingParams.pChroma[0] += pitch*chroma_pixel_sz;
        m_deblockingParams.pChroma[1] += pitch*chroma_pixel_sz;
        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
        // restore values
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] -= 1;
        m_deblockingParams.pChroma[0] -= pitch*chroma_pixel_sz;
        m_deblockingParams.pChroma[1] -= pitch*chroma_pixel_sz;
        m_deblockingParams.pitch_chroma = pitch;
    }

    //
    // step 3. Deblock internal edges
    //
    {
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
        // set internal edge strength
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 0);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, bTmp + 4);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, bTmp + 8);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, bTmp + 12);
        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
        // restore strength
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, bTmp + 0);
    }

} // void H264SegmentDecoder::DeblockChromaHorizontalMBAFF420()

void H264SegmentDecoder::DeblockChromaHorizontalMBAFF422()
{
    Ipp8u bTmp[16];
    Ipp32s pitch = m_deblockingParams.pitch_chroma;
    Ipp32s chroma_pixel_sz = bit_depth_chroma > 8 ? 2 : 1;
    ChromaDeblockingFunction pFunc = DeblockChroma[m_pCurrentFrame->m_chroma_format];

    //
    // chroma deblocking
    //

    //
    // step 1. Deblock origin external edge
    //
    {
        // save internal edges strength
        CopyEdgeStrength(bTmp + 0, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0);
        CopyEdgeStrength(bTmp + 4, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4);
        CopyEdgeStrength(bTmp + 8, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8);
        CopyEdgeStrength(bTmp + 12, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12);
        // skip all internal edges
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, 0);
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, 0);
        // set pitch
        m_deblockingParams.pitch_chroma *= 2;
        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
    }

    //
    // step 2. Deblock extra external edge
    //
    {
        // set extra edge strength
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING], m_deblockingParams.StrengthExtra);
        // correct neighbour MB info
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] += 1;
        // correct U & V pointer
        m_deblockingParams.pChroma[0] += pitch*chroma_pixel_sz;
        m_deblockingParams.pChroma[1] += pitch*chroma_pixel_sz;
        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
        // restore values
        m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] -= 1;
        m_deblockingParams.pChroma[0] -= pitch*chroma_pixel_sz;
        m_deblockingParams.pChroma[1] -= pitch*chroma_pixel_sz;
        m_deblockingParams.pitch_chroma = pitch;
    }

    //
    // step 3. Deblock internal edges
    //
    {
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
        // set internal edge strength
        SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 0);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, bTmp + 4);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, bTmp + 8);
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, bTmp + 12);
        // perform deblocking
        (this->*pFunc)(HORIZONTAL_DEBLOCKING);
        // restore strength
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        CopyEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, bTmp + 0);
    }

} // void H264SegmentDecoder::DeblockChromaHorizontalMBAFF422()

void H264SegmentDecoder::DeblockChromaHorizontalMBAFF444()
{
    /* the function is under development */

} // void H264SegmentDecoder::DeblockChromaHorizontalMBAFF444()

void H264SegmentDecoder::PrepareDeblockingParametersStrengthsMBAFF()
{
    Ipp32s i, j, dir, mbtype, mbtype_n, cbp_coeffs, cbp_coeffs_n, cbp_residual, cbp_residual_n;
    Ipp32s field_cur, complex, addr;
    Ipp8u s;

    if (!m_spatial_resolution_change && !deblocking_IL)
        return;

    for (dir = 0; dir < 2; dir ++) {
        if (dir) {
            complex = m_deblockingParams.ExtraHorizontalEdge;
        } else {
            complex = m_deblockingParams.UseComplexVerticalDeblocking;
        }
        mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;
        field_cur = m_cur_mb.GlobalMacroblockInfo->mbflags.fdf;

        cbp_coeffs = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs << 1;
        cbp_residual = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual << 1;
        if (m_deblockingParams.ExternalEdgeFlag[dir] || complex) {
            addr = m_deblockingParams.nNeighbour[dir];
            mbtype_n = (m_gmbinfo->mbs + addr)->mbtype;
            cbp_coeffs_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_coeffs << 1;
            cbp_residual_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_residual << 1;
        } else {
            mbtype_n = cbp_coeffs_n = cbp_residual_n = 0;
        }

        for (i = complex && !dir ? 4 : 0; i < 16; i++) {
            s = 10;

            if (pGetMB8x8TSFlag(m_cur_mb.GlobalMacroblockInfo) && (i & 4)) {
                m_deblockingParams.Strength[dir][i] = 0;
                continue;
            }

            if (m_spatial_resolution_change) {
                if (i >= 4) {
                    if (mbtype == MBTYPE_INTRA_BL) {
                        s = (cbp_coeffs & INTERNAL_BLOCKS_MASK[dir][i-4]) ? 1 : 0;
                    }
                } else if (m_deblockingParams.ExternalEdgeFlag[dir] || complex) {
                    if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype_n)) {
                        s = 0;
                    }
                    else if (mbtype == MBTYPE_INTRA_BL && mbtype_n == MBTYPE_INTRA_BL) {
                        s = ((cbp_coeffs & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][i]) ||
                            (cbp_coeffs_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][i]))
                            ? 1 : 0;
                    } else if (mbtype == MBTYPE_INTRA_BL) {
                        if (IS_INTRA_MBTYPE_NOT_BL(mbtype_n))
                            s = (!dir || (dir && !complex && !field_cur)) ? 4 : 3;
                        else
                            s = (cbp_residual_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][i])
                            ? 2 : 1;

                    } else if (mbtype_n == MBTYPE_INTRA_BL) {
                        if (IS_INTRA_MBTYPE_NOT_BL(mbtype))
                            s = (!dir || (dir && !complex && !field_cur)) ? 4 : 3;
                        else
                            s = (cbp_residual & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][i])
                            ? 2 : 1;
                    }
                }
            } else {
                if (deblocking_IL && m_deblockingParams.ExternalEdgeFlag[dir] &&
                    !IS_INTRA_MBTYPE(mbtype_n) && i < 4 ) {
                        s = 0;
                }
            }
            if (s < 10) {
                m_deblockingParams.Strength[dir][i] = s;
            }
        }

        if (complex) {
            if (dir == 0) {
                for (j = 0; j < 2; j++) {
                    Ipp8u *bs = m_deblockingParams.StrengthComplex + j * 4;
                    Ipp32s nNeighbourBlockInc = (m_CurMBAddr & 1) ? 2 : 0;

                    addr = m_deblockingParams.nLeft[j];
                    mbtype_n = (m_gmbinfo->mbs + addr)->mbtype;
                    cbp_coeffs_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_coeffs << 1;
                    cbp_residual_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_residual << 1;

                    for (i = 0; i < 4; i++) {
                        Ipp32s idx_cur, idx_nei;
                        if (field_cur) {
                            idx_cur = i / 2 + j * 2;
                            idx_nei = i;

                        } else {
                            idx_cur = i;
                            idx_nei = i / 2 + nNeighbourBlockInc;
                        }

                        s = 10;
                        if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype_n)) {
                            s = 0;
                        }
                        else if (mbtype == MBTYPE_INTRA_BL && mbtype_n == MBTYPE_INTRA_BL) {
                            s = ((cbp_coeffs & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx_cur]) ||
                                (cbp_coeffs_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx_nei]))
                                ? 1 : 0;
                        } else if (mbtype == MBTYPE_INTRA_BL) {
                            if (IS_INTRA_MBTYPE_NOT_BL(mbtype_n))
                                s = 4;
                            else
                                s = (cbp_residual_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx_nei])
                                ? 2 : 1;

                        } else if (mbtype_n == MBTYPE_INTRA_BL) {
                            if (IS_INTRA_MBTYPE_NOT_BL(mbtype))
                                s = 4;
                            else
                                s = (cbp_residual & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx_cur])
                                ? 2 : 1;
                        }
                        if (s < 10) {
                            bs[i] = s;
                        }
                    }
                }
            } else {
                for (j = 0; j < 1; j++) {
                    Ipp8u *bs = m_deblockingParams.StrengthExtra;

                    if (!field_cur) {
                        addr = m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] + 1;
                    } else {
                        addr = m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING];
                    }

                    mbtype_n = (m_gmbinfo->mbs + addr)->mbtype;
                    cbp_coeffs_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_coeffs << 1;
                    cbp_residual_n = (m_mbinfo.mbs + addr)->cbp4x4_luma_residual << 1;

                    for (i = 0; i < 4; i++) {
                        Ipp32s idx_cur, idx_nei;

                        idx_cur = idx_nei = i;

                        s = 10;
                        if (deblocking_IL && !IS_INTRA_MBTYPE(mbtype_n)) {
                            s = 0;
                        }
                        else if (mbtype == MBTYPE_INTRA_BL && mbtype_n == MBTYPE_INTRA_BL) {
                            s = ((cbp_coeffs & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx_cur]) ||
                                (cbp_coeffs_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx_nei]))
                                ? 1 : 0;
                        } else if (mbtype == MBTYPE_INTRA_BL) {
                            if (IS_INTRA_MBTYPE_NOT_BL(mbtype_n))
                                s = 3;
                            else
                                s = (cbp_residual_n & EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx_nei])
                                ? 2 : 1;

                        } else if (mbtype_n == MBTYPE_INTRA_BL) {
                            if (IS_INTRA_MBTYPE_NOT_BL(mbtype))
                                s = 3;
                            else
                                s = (cbp_residual & EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx_cur])
                                ? 2 : 1;
                        }
                        if (s < 10) {
                            bs[i] = s;
                        }
                    }
                }
            }
        }
    }
}

void H264SegmentDecoder::PrepareDeblockingParametersISliceMBAFF()
{
    // set deblocking flag(s)
    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = 1;
    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = 1;

    // fill "complex deblocking" strength
    if (m_deblockingParams.UseComplexVerticalDeblocking)
    {
        SetEdgeStrength(m_deblockingParams.StrengthComplex + 0, 4);
        SetEdgeStrength(m_deblockingParams.StrengthComplex + 4, 4);
    }

    // calculate strengths
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        // deblocking with strong deblocking of external edge
        SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 0, 4);
    }

    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 12, 3);

    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        if ((GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_CurMBAddr])) ||
            (m_deblockingParams.ExtraHorizontalEdge))
        {
            // deblocking field macroblock with external edge
            SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 3);
        }
        else
        {
            // deblocking with strong deblocking of external edge
            SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 4);
        }
    }

    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, 3);

    if (m_deblockingParams.ExtraHorizontalEdge)
    {
        // set extra edge strength
        SetEdgeStrength(m_deblockingParams.StrengthExtra + 0, 3);
        SetEdgeStrength(m_deblockingParams.StrengthExtra + 4, 0);
        SetEdgeStrength(m_deblockingParams.StrengthExtra + 8, 0);
        SetEdgeStrength(m_deblockingParams.StrengthExtra + 12, 0);
    }

    PrepareDeblockingParametersStrengthsMBAFF();
} // void H264SegmentDecoder::PrepareDeblockingParametersISliceMBAFF()

void H264SegmentDecoder::PrepareDeblockingParametersPSliceMBAFF()
{
    Ipp32s mbtype = (m_gmbinfo->mbs + m_CurMBAddr)->mbtype;
    Ipp32s nAboveMBFieldCoded = 0;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        PrepareDeblockingParametersISliceMBAFF();
        return;
    }

    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
        nAboveMBFieldCoded = GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING]]);
    // when there are no special cases
    if ((0 == nAboveMBFieldCoded) &&
        (0 == m_deblockingParams.MBFieldCoded) &&
        (0 == m_deblockingParams.UseComplexVerticalDeblocking))
    {
        // prepare whole macroblock parameters
        PrepareDeblockingParametersPSlice();
        return;
    }

    // when current macroblock is frame coded
    if (0 == m_deblockingParams.MBFieldCoded)
    {
        // prepare "complex vertical deblocking" parameters
        if (m_deblockingParams.UseComplexVerticalDeblocking)
            PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge();

        // fill vertical edges parameters
        PrepareDeblockingParametersPSlice4(VERTICAL_DEBLOCKING);

        // fill extra edge parameters
        if (m_deblockingParams.ExtraHorizontalEdge)
        {
            // set correct above neighbour
            m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] += 1;
            // obtain parameters
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            // restore above neighbour
            m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] -= 1;
            // copy parameters to right place
            CopyEdgeStrength(m_deblockingParams.StrengthExtra, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING]);

            // fill horizontal edges parameters
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            PrepareDeblockingParametersPSlice4(HORIZONTAL_DEBLOCKING);
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        }
        else
            PrepareDeblockingParametersPSlice4(HORIZONTAL_DEBLOCKING);

    }
    // when current macroblock is field coded
    else
    {
        // prepare "complex vertical deblocking" parameters
        if (m_deblockingParams.UseComplexVerticalDeblocking)
            PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge();

        // fill vertical edges parameters
        PrepareDeblockingParametersPSlice4MBAFFField(VERTICAL_DEBLOCKING);

        // when above macroblock is frame coded
        if ((nAboveMBFieldCoded != m_deblockingParams.MBFieldCoded) &&
            (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING]))
        {
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            PrepareDeblockingParametersPSlice4MBAFFField(HORIZONTAL_DEBLOCKING);
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        }
        else
            PrepareDeblockingParametersPSlice4MBAFFField(HORIZONTAL_DEBLOCKING);
    }

    PrepareDeblockingParametersStrengthsMBAFF();
} // void H264SegmentDecoder::PrepareDeblockingParametersPSliceMBAFF()

void H264SegmentDecoder::PrepareDeblockingParametersBSliceMBAFF()
{
    Ipp32s mbtype = (m_gmbinfo->mbs + m_CurMBAddr)->mbtype;
    Ipp32s nAboveMBFieldCoded = 0;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        PrepareDeblockingParametersISliceMBAFF();
        return;
    }

    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
        nAboveMBFieldCoded = GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING]]);
    // when there are no special cases
    if ((0 == nAboveMBFieldCoded) &&
        (0 == m_deblockingParams.MBFieldCoded) &&
        (0 == m_deblockingParams.UseComplexVerticalDeblocking))
    {
        // prepare whole macroblock parameters
        PrepareDeblockingParametersBSlice();
        return;
    }

    // when current macroblock is frame coded
    if (0 == m_deblockingParams.MBFieldCoded)
    {
        // prepare "complex vertical deblocking" parameters
        if (m_deblockingParams.UseComplexVerticalDeblocking)
            PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge();

        // fill vertical edges parameters
        PrepareDeblockingParametersBSlice4(VERTICAL_DEBLOCKING);

        // fill extra edge parameters
        if (m_deblockingParams.ExtraHorizontalEdge)
        {
            // set correct above neighbour
            m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] += 1;
            // obtain parameters
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            // restore above neighbour
            m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING] -= 1;
            // copy parameters to right place
            CopyEdgeStrength(m_deblockingParams.StrengthExtra, m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING]);

            // fill horizontal edges parameters
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            PrepareDeblockingParametersBSlice4(HORIZONTAL_DEBLOCKING);
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        }
        else
            PrepareDeblockingParametersBSlice4(HORIZONTAL_DEBLOCKING);
    }
    // when current macroblock is field coded
    else
    {
        // prepare "complex vertical deblocking" parameters
        if (m_deblockingParams.UseComplexVerticalDeblocking)
            PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge();

        // fill vertical edges parameters
        PrepareDeblockingParametersBSlice4MBAFFField(VERTICAL_DEBLOCKING);

        // when above macroblock is frame coded
        if ((nAboveMBFieldCoded != m_deblockingParams.MBFieldCoded) &&
            (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING]))
        {
            PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
            PrepareDeblockingParametersBSlice4MBAFFField(HORIZONTAL_DEBLOCKING);
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 1;
        }
        else
            PrepareDeblockingParametersBSlice4MBAFFField(HORIZONTAL_DEBLOCKING);
    }

    PrepareDeblockingParametersStrengthsMBAFF();
} // void H264SegmentDecoder::PrepareDeblockingParametersBSliceMBAFF()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge()
{
    Ipp32u cbp4x4_luma_residual = (m_mbinfo.mbs + m_CurMBAddr)->cbp4x4_luma_residual << 1;
    Ipp8u *pStrength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];
    Ipp32s nNeighbour;

    //
    // external edge
    //

    // mixed edge is always deblocked
    {
        Ipp32s *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING]);
        *pDeblockingFlag = 1;
    }

    // select neighbour addres
    nNeighbour = m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING];

    // when neighbour macroblock isn't intra
    if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
    {
        H264DecoderMacroblockLocalInfo *pNeighbour;
        Ipp32s idx;

        // select neighbour
        pNeighbour = m_mbinfo.mbs + nNeighbour;

        // cicle on blocks
        for (idx = 0;idx < 4;idx += 1)
        {
            Ipp32s blkQ, blkP;

            blkQ = EXTERNAL_BLOCK_MASK[HORIZONTAL_DEBLOCKING][CURRENT_BLOCK][idx];
            blkP = EXTERNAL_BLOCK_MASK[HORIZONTAL_DEBLOCKING][NEIGHBOUR_BLOCK][idx];

            // when one of couple of blocks has coeffs
            if ((cbp4x4_luma_residual & blkQ) ||
                ((pNeighbour->cbp4x4_luma_residual << 1) & blkP))
                pStrength[idx] = 2;
            // when blocks nave no coeffs
            // we set strength is 1 in mixed mode
            else
                pStrength[idx] = 1;
        }
    }
    // external edge required in strong filtering
    else
        SetEdgeStrength(pStrength + 0, 3);

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge()
{
    Ipp32u cbp4x4_luma_residual = (m_mbinfo.mbs + m_CurMBAddr)->cbp4x4_luma_residual << 1;
    Ipp8u *pStrength = m_deblockingParams.StrengthComplex;
    Ipp32s i, nNeighbourBlockInc;

    // mixed edge is always deblocked
    {
        Ipp32s *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING]);
        *pDeblockingFlag = 1;
    }

    // calculate neighbour block addition
    nNeighbourBlockInc = (m_CurMBAddr & 1) ? 2 : 0;

    // we fill parameters using 2 passes
    for (i = 0;i < 2;i += 1)
    {
        Ipp32s nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nLeft[i];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            Ipp32s idx;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32s blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[VERTICAL_DEBLOCKING][CURRENT_BLOCK][idx];
                blkP = EXTERNAL_BLOCK_MASK[VERTICAL_DEBLOCKING][NEIGHBOUR_BLOCK][idx / 2 + nNeighbourBlockInc];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma_residual & blkQ) ||
                    ((pNeighbour->cbp4x4_luma_residual << 1) & blkP))
                    pStrength[idx] = 2;
                // when blocks have no coeffs
                // set strength is 1
                else
                    pStrength[idx] = 1;
            }
        }
        // external edge required in strong filtering
        else
            SetEdgeStrength(pStrength + 0, 4);

        pStrength += 4;
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge()
{
    Ipp32u cbp4x4_luma_residual = (m_mbinfo.mbs + m_CurMBAddr)->cbp4x4_luma_residual << 1;
    Ipp8u *pStrength = m_deblockingParams.StrengthComplex;
    Ipp32s i;

    // mixed edge is always deblocked
    {
        Ipp32s *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING]);
        *pDeblockingFlag = 1;
    }

    // we fill parameters using 2 passes
    for (i = 0;i < 2;i += 1)
    {
        Ipp32s nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nLeft[i];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            Ipp32s idx;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32s blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[VERTICAL_DEBLOCKING][CURRENT_BLOCK][idx / 2 + i * 2];
                blkP = EXTERNAL_BLOCK_MASK[VERTICAL_DEBLOCKING][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma_residual & blkQ) ||
                    ((pNeighbour->cbp4x4_luma_residual << 1) & blkP))
                    pStrength[idx] = 2;
                // when blocks have no coeffs
                // set strength is 1
                else
                    pStrength[idx] = 1;
            }
        }
        // external edge required in strong filtering
        else
            SetEdgeStrength(pStrength + 0, 4);

        pStrength += 4;
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFField(Ipp32u dir)
{
    Ipp32u cbp4x4_luma_residual = (m_mbinfo.mbs + m_CurMBAddr)->cbp4x4_luma_residual << 1;
    Ipp8u *pStrength = m_deblockingParams.Strength[dir];
    Ipp32s *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);

    //
    // external edge
    //

    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        Ipp32s nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            Ipp32s idx;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32s blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma_residual & blkQ) ||
                    ((pNeighbour->cbp4x4_luma_residual << 1) & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32s nBlock, nNeighbourBlock;
                    size_t iRefQ, iRefP;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                    {
                        nBlock = idx * 4;
                        nNeighbourBlock = idx * 4 + 3;
                    }
                    else
                    {
                        nBlock = idx;
                        nNeighbourBlock = idx + 12;
                    }

                    {
                        H264DecoderFrame **pRefPicList;
                        Ipp32s index;

                        // select reference index for current block
                        index = m_cur_mb.GetReferenceIndex(0, nBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 0)->m_RefPicList;
                            iRefQ = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefQ = (size_t)-1;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            iRefP = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefP = (size_t)-1;
                    }

                    VM_ASSERT((iRefP != -1) || (iRefQ != -1));

                    // when reference indexes are equal
                    if (iRefQ == iRefP)
                    {
                        H264DecoderMotionVector &pVectorQ = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                        H264DecoderMotionVector &pVectorP = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);

                        // compare motion vectors
                        if ((4 <= abs(pVectorQ.mvx - pVectorP.mvx)) ||
                            (2 <= abs(pVectorQ.mvy - pVectorP.mvy)))
                        {
                            pStrength[idx] = 1;
                            *pDeblockingFlag = 1;
                        }
                        else
                            pStrength[idx] = 0;
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if (HORIZONTAL_DEBLOCKING == dir)
                SetEdgeStrength(pStrength + 0, 3);
            else
                SetEdgeStrength(pStrength + 0, 4);
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32s idx;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32s blkQ;

            blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp4x4_luma_residual & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // compare motion vectors & reference indexes
            else
            {
                Ipp32s nBlock, nNeighbourBlock;
                size_t iRefQ, iRefP;

                // calc block and neighbour block number
                if (VERTICAL_DEBLOCKING == dir)
                {
                    nBlock = (idx & 3) * 4 + (idx >> 2);
                    nNeighbourBlock = nBlock - 1;
                }
                else
                {
                    nBlock = idx;
                    nNeighbourBlock = idx - 4;
                }

                VM_ASSERT(-1 == m_cur_mb.GetReferenceIndex(1, nBlock));
                VM_ASSERT(-1 == m_cur_mb.GetReferenceIndex(1, nNeighbourBlock));

                {
                    H264DecoderFrame **pRefPicList;
                    Ipp32s index;

                    pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 0)->m_RefPicList;

                    // select reference index for current block
                    index = m_cur_mb.GetReferenceIndex(0, nBlock);
                    iRefQ = (index < 0) ?
                            (-1) :
                            (pRefPicList[index >> 1]->DeblockPicID(index & 1));

                    // select reference index for previous block
                    index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
                    iRefP = (index < 0) ?
                            (-1) :
                            pRefPicList[index >> 1]->DeblockPicID(index & 1);
                }

                VM_ASSERT((iRefP != -1) || (iRefQ != -1));

                // when reference indexes are equal
                if (iRefQ == iRefP)
                {
                    H264DecoderMotionVector & pVectorQ = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                    H264DecoderMotionVector & pVectorP = GetMV(m_gmbinfo, 0, m_CurMBAddr, nNeighbourBlock);

                    // compare motion vectors
                    if ((4 <= abs(pVectorQ.mvx - pVectorP.mvx)) ||
                        (2 <= abs(pVectorQ.mvy - pVectorP.mvy)))
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                    else
                        pStrength[idx] = 0;
                }
                // when reference indexes are different
                else
                {
                    pStrength[idx] = 1;
                    *pDeblockingFlag = 1;
                }
            }
        }
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice4MBAFFField(Ipp32u dir)

void H264SegmentDecoder::PrepareDeblockingParametersBSlice4MBAFFField(Ipp32u dir)
{
    Ipp32u cbp4x4_luma_residual = (m_mbinfo.mbs + m_CurMBAddr)->cbp4x4_luma_residual << 1;
    Ipp8u *pStrength = m_deblockingParams.Strength[dir];
    Ipp32s *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        Ipp32s nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            Ipp32s idx;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                Ipp32s blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma_residual & blkQ) ||
                    ((pNeighbour->cbp4x4_luma_residual << 1) & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    Ipp32s nBlock, nNeighbourBlock;
                    size_t iRefQFrw, iRefPFrw, iRefQBck, iRefPBck;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                    {
                        nBlock = idx * 4;
                        nNeighbourBlock = nBlock + 3;
                    }
                    else
                    {
                        nBlock = idx;
                        nNeighbourBlock = idx + 12;
                    }

                    {
                        H264DecoderFrame **pRefPicList;
                        Ipp32s index;

                        // select reference index for current block
                        index = m_cur_mb.GetReferenceIndex(0, nBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 0)->m_RefPicList;
                            iRefQFrw = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefQFrw = (size_t)-1;
                        index = m_cur_mb.GetReferenceIndex(1, nBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 1)->m_RefPicList;
                            iRefQBck = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefQBck = (size_t)-1;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            iRefPFrw = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefPFrw = (size_t)-1;
                        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_RefPicList;
                            iRefPBck = pRefPicList[index >> 1]->DeblockPicID(index & 1);
                        }
                        else
                            iRefPBck = (size_t)-1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            // select current block motion vectors
                            H264DecoderMotionVector & pVectorQFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                            H264DecoderMotionVector & pVectorQBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nBlock);

                            H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                            H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

                            // select previous block motion vectors
                            if (iRefQFrw != iRefPFrw)
                            {
                                swapValues(pVectorPFrw, pVectorPBck);
                            }

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw.mvx - pVectorPFrw.mvx)) ||
                                (2 <= abs(pVectorQFrw.mvy - pVectorPFrw.mvy)) ||
                                (4 <= abs(pVectorQBck.mvx - pVectorPBck.mvx)) ||
                                (2 <= abs(pVectorQBck.mvy - pVectorPBck.mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            // select current block motion vectors
                            H264DecoderMotionVector & pVectorQFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                            H264DecoderMotionVector & pVectorQBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nBlock);

                            // select previous block motion vectors
                            H264DecoderMotionVector & pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                            H264DecoderMotionVector & pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

                            // compare motion vectors
                            if ((4 <= abs(pVectorQFrw.mvx - pVectorPFrw.mvx)) ||
                                (2 <= abs(pVectorQFrw.mvy - pVectorPFrw.mvy)) ||
                                (4 <= abs(pVectorQBck.mvx - pVectorPBck.mvx)) ||
                                (2 <= abs(pVectorQBck.mvy - pVectorPBck.mvy)))
                            {
                                if ((4 <= abs(pVectorQFrw.mvx - pVectorPBck.mvx)) ||
                                    (2 <= abs(pVectorQFrw.mvy - pVectorPBck.mvy)) ||
                                    (4 <= abs(pVectorQBck.mvx - pVectorPFrw.mvx)) ||
                                    (2 <= abs(pVectorQBck.mvy - pVectorPFrw.mvy)))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if (HORIZONTAL_DEBLOCKING == dir)
                SetEdgeStrength(pStrength + 0, 3);
            else
                SetEdgeStrength(pStrength + 0, 4);
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        Ipp32s idx;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            Ipp32s blkQ;

            blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp4x4_luma_residual & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // compare motion vectors & reference indexes
            else
            {
                Ipp32s nBlock, nNeighbourBlock;
                size_t iRefQFrw, iRefQBck, iRefPFrw, iRefPBck;

                // calc block and neighbour block number
                if (VERTICAL_DEBLOCKING == dir)
                {
                    nBlock = (idx & 3) * 4 + (idx >> 2);
                    nNeighbourBlock = nBlock - 1;
                }
                else
                {
                    nBlock = idx;
                    nNeighbourBlock = idx - 4;
                }

                {
                    H264DecoderFrame **pRefPicList;
                    Ipp32s index;

                    // select forward reference pictures list
                    pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 0)->m_RefPicList;
                    // select forward reference index for block(s)
                    index = m_cur_mb.GetReferenceIndex(0, nBlock);
                    iRefQFrw = (index < 0) ?
                            (-1) :
                            (pRefPicList[index >> 1]->DeblockPicID(index & 1));
                    index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
                    iRefPFrw = (index < 0) ?
                            (-1) :
                            pRefPicList[index >> 1]->DeblockPicID(index & 1);

                    // select backward reference pictures list
                    pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + m_CurMBAddr)->slice_id, 1)->m_RefPicList;
                    // select backward reference index for block(s)
                    index = m_cur_mb.GetReferenceIndex(1, nBlock);
                    iRefQBck = (index < 0) ?
                            (-1) :
                            (pRefPicList[index >> 1]->DeblockPicID(index & 1));
                    index = m_cur_mb.GetReferenceIndex(1, nNeighbourBlock);
                    iRefPBck = (index < 0) ?
                            (-1) :
                            pRefPicList[index >> 1]->DeblockPicID(index & 1);
                }

                // when reference indexes are equal
                if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                    ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                {
                    // set initial value of strength
                    pStrength[idx] = 0;

                    // when forward and backward reference pictures of previous block are different
                    if (iRefPFrw != iRefPBck)
                    {
                        // select current block motion vectors
                        H264DecoderMotionVector & pVectorQFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                        H264DecoderMotionVector & pVectorQBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nBlock);

                        H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nNeighbourBlock);
                        H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nNeighbourBlock);

                        // select previous block motion vectors
                        if (iRefQFrw != iRefPFrw)
                        {
                            swapValues(pVectorPFrw, pVectorPBck);
                        }

                        // compare motion vectors
                        if ((4 <= abs(pVectorQFrw.mvx - pVectorPFrw.mvx)) ||
                            (2 <= abs(pVectorQFrw.mvy - pVectorPFrw.mvy)) ||
                            (4 <= abs(pVectorQBck.mvx - pVectorPBck.mvx)) ||
                            (2 <= abs(pVectorQBck.mvy - pVectorPBck.mvy)))
                        {
                            pStrength[idx] = 1;
                            *pDeblockingFlag = 1;
                        }
                    }
                    // when forward and backward reference pictures of previous block are equal
                    else
                    {
                        // select current block motion vectors
                        H264DecoderMotionVector & pVectorQFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nBlock);
                        H264DecoderMotionVector & pVectorQBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nBlock);

                        // select previous block motion vectors
                        H264DecoderMotionVector & pVectorPFrw = GetMV(m_gmbinfo, 0, m_CurMBAddr, nNeighbourBlock);
                        H264DecoderMotionVector & pVectorPBck = GetMV(m_gmbinfo, 1, m_CurMBAddr, nNeighbourBlock);

                        // compare motion vectors
                        if ((4 <= abs(pVectorQFrw.mvx - pVectorPFrw.mvx)) ||
                            (2 <= abs(pVectorQFrw.mvy - pVectorPFrw.mvy)) ||
                            (4 <= abs(pVectorQBck.mvx - pVectorPBck.mvx)) ||
                            (2 <= abs(pVectorQBck.mvy - pVectorPBck.mvy)))
                        {
                            if ((4 <= abs(pVectorQFrw.mvx - pVectorPBck.mvx)) ||
                                (2 <= abs(pVectorQFrw.mvy - pVectorPBck.mvy)) ||
                                (4 <= abs(pVectorQBck.mvx - pVectorPFrw.mvx)) ||
                                (2 <= abs(pVectorQBck.mvy - pVectorPFrw.mvy)))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                    }
                }
                // when reference indexes are different
                else
                {
                    pStrength[idx] = 1;
                    *pDeblockingFlag = 1;
                }
            }
        }
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice4MBAFFField(Ipp32u dir)

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
