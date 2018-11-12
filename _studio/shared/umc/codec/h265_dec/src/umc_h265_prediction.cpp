// Copyright (c) 2012-2018 Intel Corporation
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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

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
        src = 16 + (const uint8_t*)src; dst = 16 + (uint8_t*)dst; len -= 16;
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
        m_temp_interpolarion_buffer = h265_new_array_throw<uint8_t>(2*128*128*2);
}

// Perform motion compensation prediction for a partition of CU
void H265Prediction::MotionCompensation(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Depth)
{
    if (m_context->m_sps->need16bitOutput)
        MotionCompensationInternal<uint16_t>(pCU, AbsPartIdx, Depth);
    else
        MotionCompensationInternal<uint8_t>(pCU, AbsPartIdx, Depth);
}

// Motion compensation with bit depth constant
template<typename PixType>
void H265Prediction::MotionCompensationInternal(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Depth)
{
    int32_t countPart = pCU->getNumPartInter(AbsPartIdx);
    uint32_t PUOffset = 0;

    if (countPart > 1)
    {
        EnumPartSize PartSize = pCU->GetPartitionSize(AbsPartIdx);
        PUOffset = (g_PUOffset[PartSize] << ((m_context->m_sps->MaxCUDepth - Depth) << 1)) >> 4;
    }

    // Loop over prediction units
    for (int32_t PartIdx = 0, subPartIdx = AbsPartIdx; PartIdx < countPart; PartIdx++, subPartIdx += PUOffset)
    {
        H265PUInfo PUi;

        pCU->getPartIndexAndSize(AbsPartIdx, PartIdx, PUi.Width, PUi.Height);
        PUi.PartAddr = subPartIdx;

        int32_t LPelX = pCU->m_CUPelX + pCU->m_rasterToPelX[subPartIdx];
        int32_t TPelY = pCU->m_CUPelY + pCU->m_rasterToPelY[subPartIdx];
        int32_t PartX = LPelX >> m_context->m_sps->log2_min_transform_block_size;
        int32_t PartY = TPelY >> m_context->m_sps->log2_min_transform_block_size;

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

    int32_t w0[3], w1[3], o0[3], o1[3], logWD[3], round[3];
    uint32_t PartAddr = PUi.PartAddr;
    int32_t Width = PUi.Width;
    int32_t Height = PUi.Height;

    for (int32_t plane = 0; plane < 3; plane++)
    {
        int32_t bitDepth = plane ? m_context->m_sps->bit_depth_chroma : m_context->m_sps->bit_depth_luma;
        int32_t const shift =
            m_context->m_sps->high_precision_offsets_enabled_flag ? 0 : bitDepth - 8;

        if (MVi.m_refIdx[1] >= 0)
        {
            w1[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].weight;
            o1[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].offset * (1 << shift);

            logWD[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_1][MVi.m_refIdx[1]][plane].log2_weight_denom;
        }

        if (MVi.m_refIdx[0] >= 0)
        {
            w0[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_0][MVi.m_refIdx[0]][plane].weight;
            o0[plane] = pCU->m_SliceHeader->pred_weight_table[REF_PIC_LIST_0][MVi.m_refIdx[0]][plane].offset * (1 << shift);

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
        for (int32_t plane = 0; plane < 3; plane++)
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
static void PrepareInterpSrc( H265CodingUnit* pCU, H265PUInfo &PUi, H265MotionVector const& MV, EnumRefPicList RefPicList,
                              H265InterpolationParams_8u& interpolateInfo, PlaneType* temp_interpolarion_buffer)
{
    VM_ASSERT(PUi.interinfo->m_refIdx[RefPicList] >= 0);

    uint32_t PartAddr = PUi.PartAddr;
    H265DecoderFrame *PicYUVRef = PUi.refFrame[RefPicList];

    int32_t in_SrcPitch = (c_plane_type == TEXT_CHROMA) ? PicYUVRef->pitch_chroma() : PicYUVRef->pitch_luma();

    interpolateInfo.pSrc = (c_plane_type == TEXT_CHROMA) ? (const uint8_t*)PicYUVRef->m_pUVPlane : (const uint8_t*)PicYUVRef->m_pYPlane;
    interpolateInfo.srcStep = in_SrcPitch;

    interpolateInfo.pointVector.x = MV.Horizontal;
    interpolateInfo.pointVector.y = MV.Vertical;

    uint32_t block_offset = (uint32_t)( ( (c_plane_type == TEXT_CHROMA) ?
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

template <EnumTextType c_plane_type, bool c_bi>
inline
void clipMV(H265MotionVector* MV, mfxSize const& size, H265CodingUnit const* CU)
{
    VM_ASSERT(CU);
    H265SeqParamSet const* sps = CU->m_SliceHeader->m_SeqParamSet;
    VM_ASSERT(sps);

#if 0
    //NOTE: temporary left for test purpose, should be removed after validation
    if (c_plane_type == TEXT_CHROMA)
    {
        MV->Horizontal = (int16_t)(MV->Horizontal * 2 / sps->SubWidthC());
        MV->Vertical   = (int16_t)(MV->Vertical * 2 / sps->SubHeightC());
    }
#else
    uint32_t const tap = 8;
    uint32_t const frac_shift = 2;

    int32_t const x_max =
        ((size.width  - 1) - CU->m_CUPelX + tap) << frac_shift;
    int32_t const y_max =
        ((size.height - 1) - CU->m_CUPelY + tap) << frac_shift;

    int32_t const x_min =
        ((-sps->MaxCUSize + 1) - CU->m_CUPelX - tap) << frac_shift;
    int32_t const y_min =
        ((-sps->MaxCUSize + 1) - CU->m_CUPelY - tap) << frac_shift;

    //take care of fraction - we have to restore them after clipping
    uint32_t const  low_bits_mask = 3;
    uint32_t const x_frac =
        MV->Horizontal & low_bits_mask;
    MV->Horizontal = Clip3(x_min, x_max, MV->Horizontal) | x_frac;

    uint32_t const y_frac =
        MV->Vertical & low_bits_mask;
    MV->Vertical    = Clip3(y_min, y_max, MV->Vertical)  | y_frac;

    if (c_plane_type == TEXT_CHROMA)
    {
        MV->Horizontal = (int16_t)(MV->Horizontal * 2 / sps->SubWidthC());
        MV->Vertical   = (int16_t)(MV->Vertical * 2 / sps->SubHeightC());
    }
#endif
}

// Interpolate one reference frame block
template <EnumTextType c_plane_type, bool c_bi, typename PlaneType>
void H265Prediction::PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage )
{
    int32_t bitDepth = ( c_plane_type == TEXT_CHROMA ) ? m_context->m_sps->bit_depth_chroma : m_context->m_sps->bit_depth_luma;;

    uint32_t PartAddr = PUi.PartAddr;
    int32_t Width = PUi.Width;
    int32_t Height = PUi.Height;

    // Hack to get correct offset in 2-byte elements
    int32_t in_DstPitch = (c_plane_type == TEXT_CHROMA) ? YUVPred->pitch_chroma() : YUVPred->pitch_luma();
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

    mfxSize clip_size =
        { (int)m_context->m_sps->pic_width_in_luma_samples, (int)m_context->m_sps->pic_height_in_luma_samples };
    H265MotionVector MV = PUi.interinfo->m_mv[RefPicList];
    clipMV<c_plane_type, c_bi>(&MV, clip_size, pCU);

    PrepareInterpSrc <c_plane_type, PlaneType>(pCU, PUi, MV, RefPicList, interpolateSrc, (PlaneType*)m_temp_interpolarion_buffer);
    const PlaneType * in_pSrc = (const PlaneType *)interpolateSrc.pSrc;
    int32_t in_SrcPitch = (int32_t)interpolateSrc.srcStep;
    int32_t in_SrcPic2Pitch = 0;

    const PlaneType *in_pSrcPic2 = NULL;
    if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC )
    {
        EnumRefPicList RefPicList2 = static_cast<EnumRefPicList>(RefPicList ^ 1);
        H265MotionVector MV2 = PUi.interinfo->m_mv[RefPicList2];
        clipMV<c_plane_type, c_bi>(&MV2, clip_size, pCU);

        PrepareInterpSrc <c_plane_type, PlaneType>(pCU, PUi, MV2, RefPicList2, interpolateSrc, (PlaneType*)m_temp_interpolarion_buffer + (128*128) );
        in_pSrcPic2 = (const PlaneType *)interpolateSrc.pSrc;
        in_SrcPic2Pitch = (int32_t)interpolateSrc.srcStep;
    }

    const int32_t low_bits_mask = ( c_plane_type == TEXT_CHROMA ) ? 7 : 3;
    int32_t in_dx = MV.Horizontal & low_bits_mask;
    int32_t in_dy = MV.Vertical & low_bits_mask;

    int32_t iPUWidth = Width;
    if ( c_plane_type == TEXT_CHROMA )
    {
        Width >>= m_context->m_sps->chromaShiftW;
        Height >>= m_context->m_sps->chromaShiftH;
    }

    uint32_t PicDstStride = ( c_plane_type == TEXT_CHROMA ) ? pCU->m_Frame->pitch_chroma() : pCU->m_Frame->pitch_luma();
    PlaneType *pPicDst = ( c_plane_type == TEXT_CHROMA ) ?
                (PlaneType*)pCU->m_Frame->GetCbCrAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride >> m_context->m_sps->chromaShiftH) :
                (PlaneType*)pCU->m_Frame->GetLumaAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride);

    int32_t const tap    = ( c_plane_type == TEXT_CHROMA ) ? 4 : 8;
    int32_t shift  = c_bi ? bitDepth - 8 : 6;
    int16_t offset = c_bi ? 0 : (1 << (shift - 1));

    if ((in_dx == 0) && (in_dy == 0))
    {
        // No subpel interpolation is needed
        if ( ! c_bi )
        {
            VM_ASSERT( eAddAverage == MFX_HEVC_PP::AVERAGE_NO );

            const PlaneType * pSrc = in_pSrc;
            for (int32_t j = 0; j < Height; j++)
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
        int16_t tmpBuf[80 * 80];
        uint32_t tmpStride = iPUWidth + tap;

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
                uint32_t in_Src0Pitch,      // in samples
                const PixType * pSrc1,
                uint32_t in_Src1Pitch,      // in samples
                PixType* H265_RESTRICT pDst,
                uint32_t in_DstPitch,       // in samples
                int32_t width,
                int32_t height )
{
#ifdef __INTEL_COMPILER
#pragma ivdep
#endif // __INTEL_COMPILER
    for (int j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif // __INTEL_COMPILER
        for (int i = 0; i < width; i++)
             pDst[i] = (((uint16_t)pSrc0[i] + (uint16_t)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

// Copy prediction unit extending its bits for later addition with PU from another reference
// ML: OPT: TODO: Parameterize for const shift
template <typename PixType>
void H265Prediction::CopyExtendPU(const PixType * in_pSrc,
                                uint32_t in_SrcPitch, // in samples
                                int16_t* H265_RESTRICT in_pDst,
                                uint32_t in_DstPitch, // in samples
                                int32_t width,
                                int32_t height,
                                int c_shift)
{
    const PixType * pSrc = in_pSrc;
    int16_t *pDst = in_pDst;
    int32_t i, j;

#ifdef __INTEL_COMPILER
#pragma ivdep
#endif // __INTEL_COMPILER
    for (j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
#pragma vector always
#endif // __INTEL_COMPILER
        for (i = 0; i < width; i++)
        {
            pDst[i] = (int16_t)(((int32_t)pSrc[i]) << c_shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

// Do weighted prediction from one reference frame
template<typename PixType>
void H265Prediction::CopyWeighted(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, uint32_t CUAddr, uint32_t PartIdx, uint32_t Width, uint32_t Height, int32_t *w, int32_t *o, int32_t *logWD, int32_t *round, uint32_t bit_depth, uint32_t bit_depth_chroma)
{
    CoeffsPtr pSrc = (CoeffsPtr)src->m_pYPlane + GetAddrOffset(PartIdx, src->lumaSize().width);
    CoeffsPtr pSrcUV = (CoeffsPtr)src->m_pUVPlane + GetAddrOffset(PartIdx, src->chromaSize().width);

    uint32_t DstStride = frame->pitch_luma();
    uint8_t const isChroma422 = src->m_chroma_format == 2;

    if (sizeof(PixType) == 1)
    {
        PlanePtrY pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        PlanePtrUV pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> !isChroma422);

        MFX_HEVC_PP::NAME(h265_CopyWeighted_S16U8)(pSrc, pSrcUV, pDst, pDstUV, src->pitch_luma(), frame->pitch_luma(), src->pitch_chroma(), frame->pitch_chroma(), isChroma422, Width, Height, w, o, logWD, round);
    }
    else
    {
        uint16_t* pDst = (uint16_t*)frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        uint16_t* pDstUV = (uint16_t*)frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> !isChroma422);

        MFX_HEVC_PP::NAME(h265_CopyWeighted_S16U16)(pSrc, pSrcUV, pDst, pDstUV, src->pitch_luma(), frame->pitch_luma(), src->pitch_chroma(), frame->pitch_chroma(), isChroma422, Width, Height, w, o, logWD, round, bit_depth, bit_depth_chroma);
    }
}

// Do weighted prediction from two reference frames
template<typename PixType>
void H265Prediction::CopyWeightedBidi(H265DecoderFrame* frame, H265DecYUVBufferPadded* src0, H265DecYUVBufferPadded* src1, uint32_t CUAddr, uint32_t PartIdx, uint32_t Width, uint32_t Height, int32_t *w0, int32_t *w1, int32_t *logWD, int32_t *round, uint32_t bit_depth, uint32_t bit_depth_chroma)
{
    CoeffsPtr pSrc0 = (CoeffsPtr)src0->m_pYPlane + GetAddrOffset(PartIdx, src0->lumaSize().width);
    CoeffsPtr pSrcUV0 = (CoeffsPtr)src0->m_pUVPlane + GetAddrOffset(PartIdx, src0->chromaSize().width);

    CoeffsPtr pSrc1 = (CoeffsPtr)src1->m_pYPlane + GetAddrOffset(PartIdx, src1->lumaSize().width);
    CoeffsPtr pSrcUV1 = (CoeffsPtr)src1->m_pUVPlane + GetAddrOffset(PartIdx, src1->chromaSize().width);

    uint32_t DstStride = frame->pitch_luma();
    uint8_t const isChroma422 = src0->m_chroma_format == 2;

    if (sizeof(PixType) == 1)
    {
        PlanePtrY pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        PlanePtrUV pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> !isChroma422);

        MFX_HEVC_PP::NAME(h265_CopyWeightedBidi_S16U8)(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, src0->pitch_luma(), src1->pitch_luma(), frame->pitch_luma(), src0->pitch_chroma(), src1->pitch_chroma(), frame->pitch_chroma(), isChroma422, Width, Height, w0, w1, logWD, round);
    }
    else
    {
        uint16_t* pDst = (uint16_t* )frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
        uint16_t* pDstUV = (uint16_t* )frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> !isChroma422);

        MFX_HEVC_PP::NAME(h265_CopyWeightedBidi_S16U16)(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, src0->pitch_luma(), src1->pitch_luma(), frame->pitch_luma(), src0->pitch_chroma(), src1->pitch_chroma(), frame->pitch_chroma(), isChroma422, Width, Height, w0, w1, logWD, round, bit_depth, bit_depth_chroma);
    }
}

// Calculate address offset inside of source frame
int32_t H265Prediction::GetAddrOffset(uint32_t PartUnitIdx, uint32_t width)
{
    int32_t blkX = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelX[PartUnitIdx];
    int32_t blkY = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelY[PartUnitIdx];

    return blkX + blkY * width;
}

} // end namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
