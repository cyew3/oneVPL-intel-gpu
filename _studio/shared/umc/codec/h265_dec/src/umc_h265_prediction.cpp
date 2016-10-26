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

#include "umc_h265_prediction.h"
#include "umc_h265_ipplevel.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_segment_decoder_mt.h"

using namespace MFX_HEVC_PP;

namespace UMC_HEVC_DECODER
{

// Fast memcpy inline function for small memory blocks like 4-32 bytes, used in interpolation
static void inline H265_FORCEINLINE  small_memcpy( void* dst, const void* src, int len )
{
#if defined( __INTEL_COMPILER ) // || defined( __GNUC__ )  // TODO: check with GCC
    // 128-bit loads/stores first with then REP MOVSB, aligning dst on 16-bit to avoid costly store splits
    int peel = (0xf & (-(size_t)dst));
    __asm__ ( "cld" );
    if (peel) {
        if (peel > len)
            peel = len;
        len -= peel;
        __asm__ ( "rep movsb" : "+c" (peel), "+S" (src), "+D" (dst) :: "memory" );
    }
    while (len > 15) {
        __m128i v_tmp;
        __asm__ ( "movdqu (%[src_]), %%xmm0; movdqu %%xmm0, (%[dst_])" : : [src_] "S" (src), [dst_] "D" (dst) : "%xmm0", "memory" );
        src = 16 + (const Ipp8u*)src; dst = 16 + (Ipp8u*)dst; len -= 16;
    }
    if (len > 0)
        __asm__ ( "rep movsb" : "+c" (len), "+S" (src), "+D" (dst) :: "memory" );
#else
    MFX_INTERNAL_CPY(dst, src, len);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / destructor / initialize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265Prediction::H265Prediction()
{
    m_MaxCUSize = 0;
    m_temp_interpolarion_buffer = 0;
    m_context = 0;
}

H265Prediction::~H265Prediction()
{
    m_MaxCUSize = 0;

    m_YUVPred[0].destroy();
    m_YUVPred[1].destroy();

    delete[] m_temp_interpolarion_buffer;
    m_temp_interpolarion_buffer = 0;
}

// Allocate temporal buffers which may be necessary to store interpolated reference frames
void H265Prediction::InitTempBuff(DecodingContext* context)
{
    m_context = context;
    const H265SeqParamSet * sps = m_context->m_sps;

    if (m_MaxCUSize == sps->MaxCUSize)
        return;

    if (m_MaxCUSize > 0)
    {
        m_YUVPred[0].destroy();
        m_YUVPred[1].destroy();
    }

    m_MaxCUSize = sps->MaxCUSize;

    // new structure
    m_YUVPred[0].createPredictionBuffer(sps);
    m_YUVPred[1].createPredictionBuffer(sps);

    if (!m_temp_interpolarion_buffer)
        m_temp_interpolarion_buffer = h265_new_array_throw<Ipp8u>(2*128*128*2);
}

// Perform motion compensation prediction for a partition of CU
void H265Prediction::MotionCompensation(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (m_context->m_sps->need16bitOutput)
        MotionCompensationInternal<Ipp16u>(pCU, AbsPartIdx, Depth);
    else
        MotionCompensationInternal<Ipp8u>(pCU, AbsPartIdx, Depth);
}

// Motion compensation with bit depth constant
template<typename PixType>
void H265Prediction::MotionCompensationInternal(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32s countPart = pCU->getNumPartInter(AbsPartIdx);
    Ipp32u PUOffset = 0;

    if (countPart > 1)
    {
        EnumPartSize PartSize = pCU->GetPartitionSize(AbsPartIdx);
        PUOffset = (g_PUOffset[PartSize] << ((m_context->m_sps->MaxCUDepth - Depth) << 1)) >> 4;
    }

    // Loop over prediction units
    for (Ipp32s PartIdx = 0, subPartIdx = AbsPartIdx; PartIdx < countPart; PartIdx++, subPartIdx += PUOffset)
    {
        H265PUInfo PUi;

        pCU->getPartIndexAndSize(AbsPartIdx, PartIdx, PUi.Width, PUi.Height);
        PUi.PartAddr = subPartIdx;

        Ipp32s LPelX = pCU->m_CUPelX + pCU->m_rasterToPelX[subPartIdx];
        Ipp32s TPelY = pCU->m_CUPelY + pCU->m_rasterToPelY[subPartIdx];
        Ipp32s PartX = LPelX >> m_context->m_sps->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_context->m_sps->log2_min_transform_block_size;

        PUi.interinfo = &(m_context->m_frame->getCD()->GetTUInfo(m_context->m_sps->NumPartitionsInFrameWidth * PartY + PartX));
        const H265MVInfo &MVi = *PUi.interinfo;

        PUi.refFrame[REF_PIC_LIST_0] = MVi.m_refIdx[REF_PIC_LIST_0] >= 0 ? m_context->m_refPicList[REF_PIC_LIST_0][MVi.m_refIdx[REF_PIC_LIST_0]].refFrame : 0;
        PUi.refFrame[REF_PIC_LIST_1] = MVi.m_refIdx[REF_PIC_LIST_1] >= 0 ? m_context->m_refPicList[REF_PIC_LIST_1][MVi.m_refIdx[REF_PIC_LIST_1]].refFrame : 0;

        VM_ASSERT(MVi.m_refIdx[REF_PIC_LIST_0] >= 0 || MVi.m_refIdx[REF_PIC_LIST_1] >= 0);

        if ((CheckIdenticalMotion(pCU, MVi) || !(MVi.m_refIdx[REF_PIC_LIST_0] >= 0 && MVi.m_refIdx[REF_PIC_LIST_1] >= 0)))
        {
            // One reference frame
            EnumRefPicList direction = MVi.m_refIdx[REF_PIC_LIST_0] >= 0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            if (!m_context->m_weighted_prediction)
            {
                PredInterUni<TEXT_LUMA, false, PixType>(pCU, PUi, direction, &m_YUVPred[0]);
                PredInterUni<TEXT_CHROMA, false, PixType>(pCU, PUi, direction, &m_YUVPred[0]);
            }
            else
            {
                PredInterUni<TEXT_LUMA, true, PixType>(pCU, PUi, direction, &m_YUVPred[0]);
                PredInterUni<TEXT_CHROMA, true, PixType>(pCU, PUi, direction, &m_YUVPred[0]);
            }
        }
        else
        {
            // Two reference frames
            bool bOnlyOneIterpY = false, bOnlyOneIterpC = false;
            EnumRefPicList picListY = REF_PIC_LIST_0, picListC = REF_PIC_LIST_0;

            if (!m_context->m_weighted_prediction)
            {
                // Check if at least one ref doesn't require subsample interpolation to add average directly from pic
                H265MotionVector MV = MVi.m_mv[REF_PIC_LIST_0];
                int hor = MV.Horizontal * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubWidthC();
                int ver = MV.Vertical * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubHeightC();
                int mv_interp0C = (hor | ver) & 7;
                int mv_interp0 = (MV.Horizontal | MV.Vertical) & 3;

                MV = MVi.m_mv[REF_PIC_LIST_1];
                hor = MV.Horizontal * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubWidthC();
                ver = MV.Vertical * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubHeightC();
                int mv_interp1C = (hor | ver) & 7;
                int mv_interp1 = (MV.Horizontal | MV.Vertical) & 3;

                bOnlyOneIterpC = !( mv_interp0C && mv_interp1C);
                if (mv_interp0C == 0)
                    picListC = REF_PIC_LIST_1;

                bOnlyOneIterpY = !( mv_interp0 && mv_interp1 );
                if (mv_interp0 == 0)
                    picListY = REF_PIC_LIST_1;
            }

            MFX_HEVC_PP::EnumAddAverageType addAverage = m_context->m_weighted_prediction ? MFX_HEVC_PP::AVERAGE_NO : MFX_HEVC_PP::AVERAGE_FROM_BUF;
            H265DecYUVBufferPadded* pYuvPred = &m_YUVPred[ m_context->m_weighted_prediction ? REF_PIC_LIST_1 : REF_PIC_LIST_0];

            if ( bOnlyOneIterpY )
                PredInterUni<TEXT_LUMA, true, PixType>( pCU, PUi, picListY, m_YUVPred, MFX_HEVC_PP::AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_LUMA, true, PixType>( pCU, PUi, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_LUMA, true, PixType>( pCU, PUi, REF_PIC_LIST_1, pYuvPred, addAverage );
            }

            if ( bOnlyOneIterpC )
                PredInterUni<TEXT_CHROMA, true, PixType>( pCU, PUi, picListC, m_YUVPred, MFX_HEVC_PP::AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_CHROMA, true, PixType>( pCU, PUi, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_CHROMA, true, PixType>( pCU, PUi, REF_PIC_LIST_1, pYuvPred, addAverage );
            }
        }

        if (m_context->m_weighted_prediction)
        {
            WeightedPrediction<PixType>(pCU, PUi);
        }
    }
}

// Perform weighted addition from one or two reference sources
template<typename PixType>
void H265Prediction::WeightedPrediction(H265CodingUnit* pCU, const H265PUInfo & PUi)
{
    const H265MVInfo &MVi = *PUi.interinfo;

    Ipp32s w0[3], w1[3], o0[3], o1[3], logWD[3], round[3];
    Ipp32u PartAddr = PUi.PartAddr;
    Ipp32s Width = PUi.Width;
    Ipp32s Height = PUi.Height;

    for (Ipp32s plane = 0; plane < 3; plane++)
    {
        Ipp32s bitDepth = plane ? m_context->m_sps->bit_depth_chroma : m_context->m_sps->bit_depth_luma;

        if (MVi.m_refIdx[1] >= 0)
        {
            w1[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].weight;
            o1[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].offset * (1 << (bitDepth - 8));

            logWD[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].log2_weight_denom;
        }

        if (MVi.m_refIdx[0] >= 0)
        {
            w0[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_0][MVi.m_refIdx[0]][plane].weight;
            o0[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_0][MVi.m_refIdx[0]][plane].offset * (1 << (bitDepth - 8));

            logWD[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_0][MVi.m_refIdx[0]][plane].log2_weight_denom;
        }

        logWD[plane] += 14 - bitDepth;
        round[plane] = 0;

        if (logWD[plane] >= 1)
            round[plane] = 1 << (logWD[plane] - 1);
        else
            logWD[plane] = 0;
    }

    if (MVi.m_refIdx[0] >= 0 && MVi.m_refIdx[1] >= 0)
    {
        for (Ipp32s plane = 0; plane < 3; plane++)
        {
            logWD[plane] += 1;
            round[plane] = (o0[plane] + o1[plane] + 1) << (logWD[plane] - 1);
        }
        CopyWeightedBidi<PixType>(pCU->m_Frame, &m_YUVPred[0], &m_YUVPred[1], pCU->CUAddr, PartAddr, Width, Height, w0, w1, logWD, round, m_context->m_sps->bit_depth_luma, m_context->m_sps->bit_depth_chroma);
    }
    else if (MVi.m_refIdx[0] >= 0)
        CopyWeighted<PixType>(pCU->m_Frame, &m_YUVPred[0], pCU->CUAddr, PartAddr, Width, Height, w0, o0, logWD, round, m_context->m_sps->bit_depth_luma, m_context->m_sps->bit_depth_chroma);
    else
        CopyWeighted<PixType>(pCU->m_Frame, &m_YUVPred[0], pCU->CUAddr, PartAddr, Width, Height, w1, o1, logWD, round, m_context->m_sps->bit_depth_luma, m_context->m_sps->bit_depth_chroma);
}

// Returns true if reference indexes in ref lists point to the same frame and motion vectors in these references are equal
bool H265Prediction::CheckIdenticalMotion(H265CodingUnit* pCU, const H265MVInfo &mvInfo)
{
    if(pCU->m_SliceHeader->slice_type == B_SLICE && !m_context->m_pps->weighted_bipred_flag &&
        mvInfo.m_refIdx[REF_PIC_LIST_0] >= 0 && mvInfo.m_refIdx[REF_PIC_LIST_1] >= 0 &&
        mvInfo.m_index[REF_PIC_LIST_0] == mvInfo.m_index[REF_PIC_LIST_1] &&
        mvInfo.m_mv[REF_PIC_LIST_0] == mvInfo.m_mv[REF_PIC_LIST_1])
        return true;
    else
        return false;
}

// Expand source border in case motion vector points outside of the frame
template <EnumTextType c_plane_type, typename PlaneType>
static void PrepareInterpSrc( H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList,
                              H265InterpolationParams_8u& interpolateInfo, PlaneType* temp_interpolarion_buffer)
{
    VM_ASSERT(PUi.interinfo->m_refIdx[RefPicList] >= 0);

    Ipp32u PartAddr = PUi.PartAddr;
    H265MotionVector MV = PUi.interinfo->m_mv[RefPicList];
    H265DecoderFrame *PicYUVRef = PUi.refFrame[RefPicList];

    Ipp32s in_SrcPitch = (c_plane_type == TEXT_CHROMA) ? PicYUVRef->pitch_chroma() : PicYUVRef->pitch_luma();

    interpolateInfo.pSrc = (c_plane_type == TEXT_CHROMA) ? (const Ipp8u*)PicYUVRef->m_pUVPlane : (const Ipp8u*)PicYUVRef->m_pYPlane;
    interpolateInfo.srcStep = in_SrcPitch;

    if (c_plane_type == TEXT_CHROMA)
    {
        MV.Horizontal = (Ipp16s)(MV.Horizontal * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubWidthC());
        MV.Vertical = (Ipp16s)(MV.Vertical * 2 / pCU->m_SliceHeader->m_SeqParamSet->SubHeightC());
    }

    interpolateInfo.pointVector.x = MV.Horizontal;
    interpolateInfo.pointVector.y = MV.Vertical;

    Ipp32u block_offset = (Ipp32u)( ( (c_plane_type == TEXT_CHROMA) ?
                                      (PlaneType*)PicYUVRef->GetCbCrAddr(pCU->CUAddr, PartAddr) :
                                      (PlaneType*)PicYUVRef->GetLumaAddr(pCU->CUAddr, PartAddr) )
                          - (PlaneType*)interpolateInfo.pSrc);

    // ML: TODO: make sure compiler generates only one division
    interpolateInfo.pointBlockPos.x = block_offset % in_SrcPitch;
    interpolateInfo.pointBlockPos.y = block_offset / in_SrcPitch;

    interpolateInfo.blockWidth = PUi.Width;
    interpolateInfo.blockHeight = PUi.Height;

    if ( c_plane_type == TEXT_CHROMA )
    {
        interpolateInfo.pointBlockPos.x >>= pCU->m_SliceHeader->m_SeqParamSet->chromaShiftW;
        interpolateInfo.blockWidth >>= pCU->m_SliceHeader->m_SeqParamSet->chromaShiftW;
        interpolateInfo.blockHeight >>= pCU->m_SliceHeader->m_SeqParamSet->chromaShiftH;
    }

    (c_plane_type == TEXT_CHROMA) ? ippiInterpolateChromaBlock(&interpolateInfo, temp_interpolarion_buffer) : ippiInterpolateLumaBlock(&interpolateInfo, temp_interpolarion_buffer);
}

// Interpolate one reference frame block
template <EnumTextType c_plane_type, bool c_bi, typename PlaneType>
void H265Prediction::PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage )
{
    Ipp32s bitDepth = ( c_plane_type == TEXT_CHROMA ) ? m_context->m_sps->bit_depth_chroma : m_context->m_sps->bit_depth_luma;;

    Ipp32u PartAddr = PUi.PartAddr;
    Ipp32s Width = PUi.Width;
    Ipp32s Height = PUi.Height;

    // Hack to get correct offset in 2-byte elements
    Ipp32s in_DstPitch = (c_plane_type == TEXT_CHROMA) ? YUVPred->pitch_chroma() : YUVPred->pitch_luma();
    CoeffsPtr in_pDst = (c_plane_type == TEXT_CHROMA) ?
                            (CoeffsPtr)YUVPred->m_pUVPlane + GetAddrOffset(PartAddr, YUVPred->chromaSize().width) :
                            (CoeffsPtr)YUVPred->m_pYPlane + GetAddrOffset(PartAddr, YUVPred->lumaSize().width);

    H265InterpolationParams_8u interpolateSrc;
    interpolateSrc.frameSize.width = m_context->m_sps->pic_width_in_luma_samples;
    interpolateSrc.frameSize.height = m_context->m_sps->pic_height_in_luma_samples;
    if (c_plane_type == TEXT_CHROMA)
    {
        interpolateSrc.frameSize.width >>= m_context->m_sps->chromaShiftW;
        interpolateSrc.frameSize.height >>= m_context->m_sps->chromaShiftH;
    }

    PrepareInterpSrc <c_plane_type, PlaneType>(pCU, PUi, RefPicList, interpolateSrc, (PlaneType*)m_temp_interpolarion_buffer);
    const PlaneType * in_pSrc = (const PlaneType *)interpolateSrc.pSrc;
    Ipp32s in_SrcPitch = (Ipp32s)interpolateSrc.srcStep;
    Ipp32s in_SrcPic2Pitch = 0;

    const PlaneType *in_pSrcPic2 = NULL;
    if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC )
    {
        PrepareInterpSrc <c_plane_type, PlaneType>(pCU, PUi, (EnumRefPicList)(RefPicList ^ 1), interpolateSrc, (PlaneType*)m_temp_interpolarion_buffer + (128*128) );
        in_pSrcPic2 = (const PlaneType *)interpolateSrc.pSrc;
        in_SrcPic2Pitch = (Ipp32s)interpolateSrc.srcStep;
    }

    Ipp32s tap = ( c_plane_type == TEXT_CHROMA ) ? 4 : 8;
    Ipp32s shift = c_bi ? bitDepth - 8 : 6;
    Ipp16s offset = c_bi ? 0 : (1 << (shift - 1));

    const Ipp32s low_bits_mask = ( c_plane_type == TEXT_CHROMA ) ? 7 : 3;
    H265MotionVector MV = PUi.interinfo->m_mv[RefPicList];

    if (c_plane_type == TEXT_CHROMA)
    {
        MV.Horizontal = (Ipp16s)(MV.Horizontal * 2 / m_context->m_sps->SubWidthC());
        MV.Vertical = (Ipp16s)(MV.Vertical * 2 / m_context->m_sps->SubHeightC());
    }

    Ipp32s in_dx = MV.Horizontal & low_bits_mask;
    Ipp32s in_dy = MV.Vertical & low_bits_mask;

    Ipp32s iPUWidth = Width;
    if ( c_plane_type == TEXT_CHROMA )
    {
        Width >>= m_context->m_sps->chromaShiftW;
        Height >>= m_context->m_sps->chromaShiftH;
    }

    Ipp32u PicDstStride = ( c_plane_type == TEXT_CHROMA ) ? pCU->m_Frame->pitch_chroma() : pCU->m_Frame->pitch_luma();
    PlaneType *pPicDst = ( c_plane_type == TEXT_CHROMA ) ?
                (PlaneType*)pCU->m_Frame->GetCbCrAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride >> m_context->m_sps->chromaShiftH) :
                (PlaneType*)pCU->m_Frame->GetLumaAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride);

    if ((in_dx == 0) && (in_dy == 0))
    {
        // No subpel interpolation is needed
        if ( ! c_bi )
        {
            VM_ASSERT( eAddAverage == MFX_HEVC_PP::AVERAGE_NO );

            const PlaneType * pSrc = in_pSrc;
            for (Ipp32s j = 0; j < Height; j++)
            {
                small_memcpy( pPicDst, pSrc, iPUWidth*sizeof(PlaneType) );
                pSrc += in_SrcPitch;
                pPicDst += PicDstStride;
            }
        }
        else
        {
            if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC )
                WriteAverageToPic<PlaneType>( in_pSrc, in_SrcPitch, in_pSrcPic2, in_SrcPic2Pitch, pPicDst, PicDstStride, iPUWidth, Height );
            else if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF )
                VM_ASSERT(0); // it should have been passed with AVERAGE_FROM_PIC in H265Prediction::MotionCompensation with the other block
            else // weighted prediction still requires intermediate copies
            {
                const int c_shift = 14 - bitDepth;
                int copy_width = Width;
                if (c_plane_type == TEXT_CHROMA)
                    copy_width <<= 1;

                CopyExtendPU<PlaneType>(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, copy_width, Height, c_shift);
            }
        }
    }
    else if (in_dy == 0)
    {
        // Only horizontal interpolation is needed
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else if (in_dx == 0)
    {
        // Only vertical interpolation is needed
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp32u tmpStride = iPUWidth + tap;

        // Do horizontal interpolation into a temporal buffer
        Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_HOR,
                                   in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, tmpStride,
                                   in_dx, Width, Height + tap - 1, bitDepth - 8, 0, bitDepth, 0);

        shift = c_bi ? 6 : 20 - bitDepth;
        offset = c_bi ? 0 : 1 << (19 - bitDepth);

        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, in_pDst, in_DstPitch,
                                       in_dy, Width, Height, shift, offset, bitDepth, 0);
        else if (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_PP::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, bitDepth, 0, MFX_HEVC_PP::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
}


// Calculate mean average from two references
template<typename PixType>
void H265Prediction::WriteAverageToPic(
                const PixType * pSrc0,
                Ipp32u in_Src0Pitch,      // in samples
                const PixType * pSrc1,
                Ipp32u in_Src1Pitch,      // in samples
                PixType* H265_RESTRICT pDst,
                Ipp32u in_DstPitch,       // in samples
                Ipp32s width,
                Ipp32s height )
{
    #pragma ivdep
    for (int j = 0; j < height; j++)
    {
        #pragma ivdep
        #pragma vector always
        for (int i = 0; i < width; i++)
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

// Copy prediction unit extending its bits for later addition with PU from another reference
// ML: OPT: TODO: Parameterize for const shift
template <typename PixType>
void H265Prediction::CopyExtendPU(const PixType * in_pSrc,
                                Ipp32u in_SrcPitch, // in samples
                                Ipp16s* H265_RESTRICT in_pDst,
                                Ipp32u in_DstPitch, // in samples
                                Ipp32s width,
                                Ipp32s height,
                                int c_shift)
{
    const PixType * pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

    #pragma ivdep
    for (j = 0; j < height; j++)
    {
        #pragma vector always
        for (i = 0; i < width; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << c_shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

// Do weighted prediction from one reference frame
template<typename PixType>
void H265Prediction::CopyWeighted(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    CoeffsPtr pSrc = (CoeffsPtr)src->m_pYPlane + GetAddrOffset(PartIdx, src->lumaSize().width);
    CoeffsPtr pSrcUV = (CoeffsPtr)src->m_pUVPlane + GetAddrOffset(PartIdx, src->chromaSize().width);

    Ipp32u DstStride = frame->pitch_luma();

    if (sizeof(PixType) == 1)
    {
        PlanePtrY pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        PlanePtrUV pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

        MFX_HEVC_PP::NAME(h265_CopyWeighted_S16U8)(pSrc, pSrcUV, pDst, pDstUV, src->pitch_luma(), frame->pitch_luma(), src->pitch_chroma(), frame->pitch_chroma(), Width, Height, w, o, logWD, round);
    }
    else
    {
        Ipp16u* pDst = (Ipp16u*)frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        Ipp16u* pDstUV = (Ipp16u*)frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

        MFX_HEVC_PP::NAME(h265_CopyWeighted_S16U16)(pSrc, pSrcUV, pDst, pDstUV, src->pitch_luma(), frame->pitch_luma(), src->pitch_chroma(), frame->pitch_chroma(), Width, Height, w, o, logWD, round, bit_depth, bit_depth_chroma);
    }
}

// Do weighted prediction from two reference frames
template<typename PixType>
void H265Prediction::CopyWeightedBidi(H265DecoderFrame* frame, H265DecYUVBufferPadded* src0, H265DecYUVBufferPadded* src1, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    CoeffsPtr pSrc0 = (CoeffsPtr)src0->m_pYPlane + GetAddrOffset(PartIdx, src0->lumaSize().width);
    CoeffsPtr pSrcUV0 = (CoeffsPtr)src0->m_pUVPlane + GetAddrOffset(PartIdx, src0->chromaSize().width);

    CoeffsPtr pSrc1 = (CoeffsPtr)src1->m_pYPlane + GetAddrOffset(PartIdx, src1->lumaSize().width);
    CoeffsPtr pSrcUV1 = (CoeffsPtr)src1->m_pUVPlane + GetAddrOffset(PartIdx, src1->chromaSize().width);

    Ipp32u DstStride = frame->pitch_luma();

    if (sizeof(PixType) == 1)
    {
        PlanePtrY pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        PlanePtrUV pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

        MFX_HEVC_PP::NAME(h265_CopyWeightedBidi_S16U8)(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, src0->pitch_luma(), src1->pitch_luma(), frame->pitch_luma(), src0->pitch_chroma(), src1->pitch_chroma(), frame->pitch_chroma(), Width, Height, w0, w1, logWD, round);
    }
    else
    {
        Ipp16u* pDst = (Ipp16u* )frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        Ipp16u* pDstUV = (Ipp16u* )frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

        MFX_HEVC_PP::NAME(h265_CopyWeightedBidi_S16U16)(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, src0->pitch_luma(), src1->pitch_luma(), frame->pitch_luma(), src0->pitch_chroma(), src1->pitch_chroma(), frame->pitch_chroma(), Width, Height, w0, w1, logWD, round, bit_depth, bit_depth_chroma);
    }
}

// Calculate address offset inside of source frame
Ipp32s H265Prediction::GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width)
{
    Ipp32s blkX = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelX[PartUnitIdx];
    Ipp32s blkY = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelY[PartUnitIdx];

    return blkX + blkY * width;
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
