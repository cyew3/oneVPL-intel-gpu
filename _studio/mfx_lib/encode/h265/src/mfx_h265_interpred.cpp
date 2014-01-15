//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"

using namespace MFX_HEVC_PP;

namespace H265Enc {

bool H265CU::CheckIdenticalMotion(Ipp32u abs_part_idx)
{
    EncoderRefPicListStruct *pList[2];
    T_RefIdx ref_idx[2];
    pList[0] = &(cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(cslice->m_pRefPicList[0].m_RefPicListL1);
    ref_idx[0] = data[abs_part_idx].refIdx[0];
    ref_idx[1] = data[abs_part_idx].refIdx[1];
// TODO optimize: check B_SLISE && !weighted on entrance, POC after mv matched
    if(cslice->slice_type == B_SLICE && !par->cpps->weighted_bipred_flag)
    {
        if(ref_idx[0] >= 0 && ref_idx[1] >= 0)
        {
            Ipp32s RefPOCL0 = pList[0]->m_RefPicList[ref_idx[0]]->PicOrderCnt();
            Ipp32s RefPOCL1 = pList[1]->m_RefPicList[ref_idx[1]]->PicOrderCnt();
            if(RefPOCL0 == RefPOCL1 &&
                data[abs_part_idx].mv[0] == data[abs_part_idx].mv[1])
            {
                return true;
            }
        }
    }
    return false;
}

void H265CU::clipMV(H265MV& rcMv)
{
    Ipp32s MvShift = 2;
    Ipp32s offset = 8;
    Ipp32s HorMax = (par->Width + offset - ctb_pelx - 1) << MvShift;
    Ipp32s HorMin = ( - (Ipp32s) par->MaxCUSize - offset - (Ipp32s) ctb_pelx + 1) << MvShift;

    Ipp32s VerMax = (par->Height + offset - ctb_pely - 1) << MvShift;
    Ipp32s VerMin = ( - (Ipp32s) par->MaxCUSize - offset - (Ipp32s) ctb_pely + 1) << MvShift;

    rcMv.mvx = (Ipp16s) IPP_MIN(HorMax, IPP_MAX(HorMin, rcMv.mvx));
    rcMv.mvy = (Ipp16s) IPP_MIN(VerMax, IPP_MAX(VerMin, rcMv.mvy));
}

const Ipp16s g_lumaInterpolateFilter[4][8] =
{
    {  0, 0,   0, 64,  0,   0, 0,  0 },
    { -1, 4, -10, 58, 17,  -5, 1,  0 },
    { -1, 4, -11, 40, 40, -11, 4, -1 },
    {  0, 1,  -5, 17, 58, -10, 4, -1 }
};

const Ipp16s g_chromaInterpolateFilter[8][4] =
{
    {  0, 64,  0,  0 },
    { -2, 58, 10, -2 },
    { -4, 54, 16, -2 },
    { -6, 46, 28, -4 },
    { -4, 36, 36, -4 },
    { -4, 28, 46, -6 },
    { -2, 16, 54, -4 },
    { -2, 10, 58, -2 }
};

static void CopyPU(const PixType *in_pSrc,
                          Ipp32u in_SrcPitch, // in samples
                          Ipp16s* in_pDst,
                          Ipp32u in_DstPitch, // in samples
                          Ipp32s width,
                          Ipp32s height,
                          Ipp32s shift)
{
    const PixType *pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}


void WriteAverageToPic(
    const Ipp8u * pSrc0,
    Ipp32u        in_Src0Pitch,      // in samples
    const Ipp8u * pSrc1,
    Ipp32u        in_Src1Pitch,      // in samples
    Ipp8u * H265_RESTRICT pDst,
    Ipp32u        in_DstPitch,       // in samples
    Ipp32s        width,
    Ipp32s        height)
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
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}


void H265CU::PredInterUni(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList,
                          Ipp32s PartIdx, PixType *dst, Ipp32s dst_pitch, bool bi, Ipp8u is_luma,
                          MFX_HEVC_PP::EnumAddAverageType eAddAverage)
{
    EncoderRefPicListStruct *pList[2];
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][PartAddr];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    pList[0] = &(cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(cslice->m_pRefPicList[0].m_RefPicListL1);
    Ipp32s RefIdx = data[PartAddr].refIdx[RefPicList];
    VM_ASSERT(RefIdx >= 0);

    H265MV MV = data[PartAddr].mv[RefPicList];
    clipMV(MV);

    H265Frame *PicYUVRef = pList[RefPicList]->m_RefPicList[RefIdx];

    Ipp32s in_SrcPitch, in_dstPicPitch, in_dstBufPitch, refOffset;
    PixType *in_pSrc;
    PixType *in_dstPic;
    Ipp16s *in_dstBuf;

    Ipp32s in_dx, in_dy;
    Ipp32s bitDepth, shift, tap;
    Ipp16s offset;

    if (is_luma) {
        // LUMA
        using UMC_HEVC_DECODER::TEXT_LUMA;
        Ipp32s in_SrcPitch2 = 0;
        PixType *in_pSrc2 = 0;
        if ( eAddAverage == AVERAGE_FROM_PIC ) {
            EnumRefPicList RefPicList2 = (EnumRefPicList)!RefPicList;
            Ipp32s RefIdx2 = data[PartAddr].refIdx[RefPicList2];
            H265Frame *PicYUVRef2 = pList[RefPicList2]->m_RefPicList[RefIdx2];
            in_SrcPitch2 = PicYUVRef2->pitch_luma;
            H265MV MV2 = data[PartAddr].mv[RefPicList2];
            clipMV(MV2);
            Ipp32s refOffset2 = ctb_pelx + (MV2.mvx >> 2) + (ctb_pely + (MV2.mvy >> 2)) * in_SrcPitch2;
            in_pSrc2 = PicYUVRef2->y + ((PUStartRow * in_SrcPitch2 + PUStartColumn) << par->Log2MinTUSize) + refOffset2;
        }

        in_SrcPitch = PicYUVRef->pitch_luma;
        refOffset = ctb_pelx + (MV.mvx >> 2) + (ctb_pely + (MV.mvy >> 2)) * in_SrcPitch;
        in_pSrc = PicYUVRef->y +
            ((PUStartRow * pitch_rec + PUStartColumn) << par->Log2MinTUSize) + refOffset;

        in_dstBufPitch = par->MaxCUSize;
        in_dstBuf = pred_buf_y[ (eAddAverage == AVERAGE_FROM_BUF ? !RefPicList : RefPicList) ];
        in_dstBuf += ((PUStartRow * par->MaxCUSize + PUStartColumn) << par->Log2MinTUSize);

        in_dstPicPitch = dst_pitch;
        in_dstPic = dst + ((PUStartRow * dst_pitch + PUStartColumn) << par->Log2MinTUSize);


        bitDepth = BIT_DEPTH_LUMA;
        tap = 8;
        in_dx = MV.mvx & 3;
        in_dy = MV.mvy & 3;

        shift = 6;
        offset = 32;

        if (bi)
        {
            shift = bitDepth - 8;
            offset = 0;
        }

        if ((in_dx == 0) && (in_dy == 0))
        {
            shift = 0;

            if (bi)
                shift = 14 - bitDepth;

            if (!bi) {
                for (Ipp32s j = 0; j < Height; j++) {
                    small_memcpy( in_dstPic, in_pSrc, Width );
                    in_pSrc += in_SrcPitch;
                    in_dstPic += in_dstPicPitch;
                }
            }
            else {
                if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC ) {
                    WriteAverageToPic( in_pSrc, in_SrcPitch, in_pSrc2, in_SrcPitch2, in_dstPic, in_dstPicPitch, Width, Height );
                } else {
                    VM_ASSERT(0); // should not be here
                }
            }
        }
        else if (in_dy == 0)
        {
            if (!bi) // Write directly into buffer
                Interpolate<TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstBuf, in_dstBufPitch, in_dx, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch);
            else if (eAddAverage == AVERAGE_FROM_PIC)
                Interpolate<TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2);
        }
        else if (in_dx == 0)
        {
            if (!bi) // Write directly into buffer
                Interpolate<TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstBuf, in_dstBufPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch);
            else if (eAddAverage == AVERAGE_FROM_PIC)
                Interpolate<TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2);
        }
        else
        {
            Ipp16s tmp[80 * 80];
            Ipp16s *tmpBuf = tmp + 80 * 8 + 8;
            Ipp32s tmpPitch = 80;

            Interpolate<TEXT_LUMA>( MFX_HEVC_PP::INTERP_HOR,  in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch,
                                                      tmpBuf, tmpPitch, in_dx, Width, Height + tap - 1, bitDepth - 8, 0, bitDepth);

            shift = 20 - bitDepth;
            offset = 1 << (19 - bitDepth);
            if (bi) {
                shift = 6;
                offset = 0;
            }

            if (!bi) // Write directly into buffer
                Interpolate<TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstBuf, in_dstBufPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch );
            else // eAddAverage == AVERAGE_FROM_PIC
                Interpolate<TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2 );
        }
    } else {
        // CHROMA
        using UMC_HEVC_DECODER::TEXT_CHROMA;
        Width >>= 1;
        Height >>= 1;
        bitDepth = BIT_DEPTH_CHROMA;
        tap = 4;
        in_dx = MV.mvx & 7;
        in_dy = MV.mvy & 7;

        Ipp32s in_SrcPitch2 = 0;
        PixType *in_pSrc2 = 0;
        if ( eAddAverage == AVERAGE_FROM_PIC ) {
            EnumRefPicList RefPicList2 = (EnumRefPicList)!RefPicList;
            Ipp32s RefIdx2 = data[PartAddr].refIdx[RefPicList2];
            H265Frame *PicYUVRef2 = pList[RefPicList2]->m_RefPicList[RefIdx2];
            in_SrcPitch2 = PicYUVRef2->pitch_luma;
            H265MV MV2 = data[PartAddr].mv[RefPicList2];
            clipMV(MV2);
            Ipp32s refOffset2 = (ctb_pelx) + (MV2.mvx >> 3) * 2 + ((ctb_pely >> 1) + (MV2.mvy >> 3)) * in_SrcPitch2;
            in_pSrc2 = PicYUVRef2->uv + ((PUStartRow * in_SrcPitch2 / 2 + PUStartColumn) << par->Log2MinTUSize) + refOffset2;
        }
        in_SrcPitch = PicYUVRef->pitch_luma;
        refOffset = (ctb_pelx) + (MV.mvx >> 3) * 2 + ((ctb_pely >> 1) + (MV.mvy >> 3)) * in_SrcPitch;
        in_pSrc = PicYUVRef->uv + ((PUStartRow * in_SrcPitch / 2 + PUStartColumn) << par->Log2MinTUSize) + refOffset;

        in_dstBufPitch = par->MaxCUSize;
        in_dstBuf = pred_buf_uv[ (eAddAverage == AVERAGE_FROM_BUF ? !RefPicList : RefPicList) ];
        in_dstBuf += ((PUStartRow * par->MaxCUSize + PUStartColumn * 2) << par->Log2MinTUSize);

        in_dstPicPitch = dst_pitch;
        in_dstPic = dst + ((PUStartRow * dst_pitch / 2 + PUStartColumn) << par->Log2MinTUSize);

        shift = 6;
        offset = 32;

        if (bi)
        {
            shift = bitDepth - 8;
            offset = 0;
        }

        if ((in_dx == 0) && (in_dy == 0))
        {
            shift = 0;

            if (bi)
                shift = 14 - bitDepth;

            if (!bi) {
                for (Ipp32s j = 0; j < Height; j++) {
                    small_memcpy( in_dstPic, in_pSrc, Width * 2 );
                    in_pSrc += in_SrcPitch;
                    in_dstPic += in_dstPicPitch;
                }
            }
            else {
                if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC ) {
                    WriteAverageToPic( in_pSrc, in_SrcPitch, in_pSrc2, in_SrcPitch2, in_dstPic, in_dstPicPitch, Width * 2, Height );
                } else {
                    VM_ASSERT(0); // should not be here
                }
            }
        }
        else if (in_dy == 0)
        {
            if (!bi) // Write directly into buffer
                Interpolate<TEXT_CHROMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_CHROMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstBuf, in_dstBufPitch, in_dx, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_CHROMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch);
            else if (eAddAverage == AVERAGE_FROM_PIC)
                Interpolate<TEXT_CHROMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dx, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2);
        }
        else if (in_dx == 0)
        {
            if (!bi) // Write directly into buffer
                Interpolate<TEXT_CHROMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_CHROMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstBuf, in_dstBufPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_CHROMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch);
            else if (eAddAverage == AVERAGE_FROM_PIC)
                Interpolate<TEXT_CHROMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2);
        }
        else
        {
            Ipp16s tmp[80 * 80];
            Ipp16s *tmpBuf = tmp + 80 * 8 + 8;
            Ipp32s tmpPitch = 80;

            Interpolate<TEXT_CHROMA>( MFX_HEVC_PP::INTERP_HOR,  in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, tmpPitch, in_dx, Width, Height + tap - 1, bitDepth - 8, 0, bitDepth);

            shift = 20 - bitDepth;
            offset = 1 << (19 - bitDepth);
            if (bi) {
                shift = 6;
                offset = 0;
            }

            if (!bi) // Write directly into buffer
                Interpolate<TEXT_CHROMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_NO)
                Interpolate<TEXT_CHROMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstBuf, in_dstBufPitch, in_dy, Width, Height, shift, offset, bitDepth);
            else if (eAddAverage == AVERAGE_FROM_BUF)
                Interpolate<TEXT_CHROMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_BUF, in_dstBuf, in_dstBufPitch );
            else // eAddAverage == AVERAGE_FROM_PIC
                Interpolate<TEXT_CHROMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * tmpPitch, tmpPitch, in_dstPic, in_dstPicPitch, in_dy, Width, Height, shift, offset, bitDepth, AVERAGE_FROM_PIC, in_pSrc2, in_SrcPitch2 );
        }
    }
}


void H265CU::InterPredCU(Ipp32s abs_part_idx, Ipp8u depth, PixType *dst, Ipp32s dst_pitch, Ipp8u is_luma)
{
    Ipp32s PartAddr;
    Ipp32s PartX, PartY, Width, Height;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );

    for (Ipp32s PartIdx = 0; PartIdx < getNumPartInter(abs_part_idx); PartIdx++)
    {
        GetPartOffsetAndSize(PartIdx, data[abs_part_idx].partSize,
            data[abs_part_idx].size, PartX, PartY, Width, Height);
        GetPartAddr(PartIdx, data[abs_part_idx].partSize, num_parts, PartAddr);

        PartAddr += abs_part_idx;

        Ipp32s RefIdx[2] = { data[PartAddr].refIdx[0], data[PartAddr].refIdx[1] };

        if (CheckIdenticalMotion(PartAddr) || RefIdx[0] < 0 || RefIdx[1] < 0) {
            EnumRefPicList refPicList = RefIdx[0] >= 0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            PredInterUni( PartAddr, Width, Height, refPicList, PartIdx, dst, dst_pitch, false, is_luma, AVERAGE_NO );

        } else {
            H265MV MV0 = data[PartAddr].mv[REF_PIC_LIST_0];
            H265MV MV1 = data[PartAddr].mv[REF_PIC_LIST_1];
            clipMV(MV0);
            clipMV(MV1);
            Ipp32s mv_interp0 = (MV0.mvx | MV0.mvy) & (is_luma ? 3 : 7);
            Ipp32s mv_interp1 = (MV1.mvx | MV1.mvy) & (is_luma ? 3 : 7);
            bool bOnlyOneIterp = !( mv_interp0 && mv_interp1 );
            EnumRefPicList refPicList = mv_interp0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;

            if ( bOnlyOneIterp )
                PredInterUni( PartAddr, Width, Height, refPicList, PartIdx, dst, dst_pitch, true, is_luma, AVERAGE_FROM_PIC );
            else
            {
                PredInterUni( PartAddr, Width, Height, REF_PIC_LIST_0, PartIdx, dst, dst_pitch, true, is_luma, AVERAGE_NO );
                PredInterUni( PartAddr, Width, Height, REF_PIC_LIST_1, PartIdx, dst, dst_pitch, true, is_luma, AVERAGE_FROM_BUF );
            }

                /*Ipp32s w0[3], w1[3], o0[3], o1[3], logWD[3], round[3], bitDepth = g_BitDepth;

                for (Ipp32s plane = 0; plane < 3; plane++)
                {
                    w0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iWeight;
                    w1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iWeight;
                    o0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iOffset;
                    o1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iOffset;

                    logWD[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].uiLog2WeightDenom;
                    logWD[plane] += 14 - bitDepth;
                    round[plane] = 0;

                    if (logWD[plane] >= 1)
                        round[plane] = 1 << (logWD[plane] - 1);
                    else
                        logWD[plane] = 0;
                }

                if (RefIdx[0] >= 0)
                    pPredYUV->CopyWeighted(&m_YUVPred[0], PartIdx, Width, Height, w0, o0, logWD, round);
                else if (RefIdx[1] >= 0)
                    pPredYUV->CopyWeighted(&m_YUVPred[1], PartIdx, Width, Height, w1, o1, logWD, round);
                else
                {
                    for (Ipp32s plane = 0; plane < 3; plane++)
                    {
                        logWD[plane] += 1;
                        round[plane] = (o0[plane] + o1[plane] + 1) << (logWD[plane] - 1);
                    }
                    pPredYUV->CopyWeightedBidi(&m_YUVPred[0], &m_YUVPred[1], PartIdx, Width, Height, w0, w1, logWD, round);
                }*/
        }
    }
}


void H265CU::ME_Interpolate_old(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp16s *buf, Ipp32s buf_pitch) const
{
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;

    Ipp16s *in_pDst = buf;
    Ipp32s in_DstPitch = buf_pitch;

    Ipp32s in_dx, in_dy;
    Ipp32s bitDepth, shift, offset, tap;

    Ipp32s refOffset = ctb_pelx + me_info->posx + (MV->mvx >> 2) + (ctb_pely + me_info->posy + (MV->mvy >> 2)) * in_SrcPitch;
    in_pSrc += refOffset;

    bitDepth = BIT_DEPTH_LUMA;
    tap = 8;
    in_dx = MV->mvx & 3;
    in_dy = MV->mvy & 3;

    shift = 6;
    offset = 32;

    //if (bi)
    //{
    //    shift = bitDepth - 8;
    //    offset = 0;
    //}

    if ((in_dx == 0) && (in_dy == 0))
    {
        shift = 0;

        //    if (bi)
        //        shift = 14 - bitDepth;

        CopyPU(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, width, height, shift);
    }
    else if (in_dy == 0)
    {
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, width, height, shift, (Ipp16s)offset, bitDepth);
    }
    else if (in_dx == 0)
    {
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, width, height, shift, (Ipp16s)offset, bitDepth);
    }
    else
    {
        Ipp16s tmp[80 * 80];
        Ipp16s *tmpBuf = tmp + 80 * 8 + 8;

        shift = 20 - bitDepth;
        offset = 1 << (19 - bitDepth);

        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80, in_dx, width, height + tap, bitDepth - 8, 0, bitDepth);

        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch, in_dy, width, height, shift, (Ipp16s)offset, bitDepth);
    }

    return;
} // void H265CU::ME_Interpolate(...)


void H265CU::ME_Interpolate(H265MEInfo* me_info, H265MV* MV, PixType *src, Ipp32s srcPitch, Ipp8u *dst, Ipp32s dstPitch) const
{
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx = MV->mvx & 3;
    Ipp32s dy = MV->mvy & 3;
    Ipp32s bitDepth = BIT_DEPTH_LUMA;

    Ipp32s refOffset = ctb_pelx + me_info->posx + (MV->mvx >> 2) + (ctb_pely + me_info->posy + (MV->mvy >> 2)) * srcPitch;
    src += refOffset;

    if (dx == 0 && dy == 0)
    {
    }
    else if (dy == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32, bitDepth);
    }
    else if (dx == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32, bitDepth);
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 8, bitDepth - 8, 0, bitDepth);

        Ipp32s shift  = 20 - bitDepth;
        Ipp16s offset = 1 << (shift - 1);
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth);
    }

    return;
} // void H265CU::ME_Interpolate(...)

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
