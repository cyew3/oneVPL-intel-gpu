//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder.h"

namespace UMC
{
static inline void GetScaledMV(Ipp32s pos,
                        H264DecoderMotionVector *directMVs,
                        FactorArrayValue *pDistScaleFactorMV,
                        Ipp32s RefIndexL0,
                        Ipp32s scale,
                        Ipp32s scale_idx,
                        H264DecoderMotionVector &pFwdMV,
                        H264DecoderMotionVector &pBwdMV,
                        Ipp32s mvDistortion[2])
{
    H264DecoderMotionVector MV = directMVs[pos];

    switch(scale)
    {
    case 1:
        MV.mvy /= 2;
        break;
    case -1:
        MV.mvy *= 2;
        break;
    }

    pFwdMV.mvx = (Ipp16s)((MV.mvx * pDistScaleFactorMV[RefIndexL0 >> scale_idx] + 128) >> 8);
    pFwdMV.mvy = (Ipp16s)((MV.mvy * pDistScaleFactorMV[RefIndexL0 >> scale_idx] + 128) >> 8);
    pBwdMV.mvx = (Ipp16s)(pFwdMV.mvx - MV.mvx);
    pBwdMV.mvy = (Ipp16s)(pFwdMV.mvy - MV.mvy);

    if (pFwdMV.mvy > mvDistortion[0])
        mvDistortion[0] = pFwdMV.mvy;

    if (pBwdMV.mvy > mvDistortion[1])
        mvDistortion[1] = pBwdMV.mvy;
}

#if 1
void H264SegmentDecoder::DecodeDirectMotionVectorsTemporal(bool is_direct_mb)
{
    // Use forward and backward ratios to scale the reference vectors to
    // produce the forward and backward vectors, for either a 16x16 macroblock
    // (16 vectors) or an 8x8 block (4 vectors)

    // When bRefMBIsInter is false, set the MV to zero; for that case,
    // decodeDirectMotionVectors_8x8Inference is used because it is faster.

    if (!m_pRefPicList[1][0])
    {
        for (int sb = 0; sb<4; sb++)
        {
            if (m_cur_mb.GlobalMacroblockInfo->sbtype[sb] != SBTYPE_DIRECT)
            {
                continue;
            }

            Ipp32s sboffset = subblock_block_mapping[sb];
            storeInformationInto8x8(&m_cur_mb.MVs[0]->MotionVectors[sboffset], zeroVector);
            storeInformationInto8x8(&m_cur_mb.MVs[1]->MotionVectors[sboffset], zeroVector);
            m_cur_mb.GetReferenceIndexStruct(0)->refIndexs[sb] = 0;
            m_cur_mb.GetReferenceIndexStruct(1)->refIndexs[sb] = 0;

            m_cur_mb.LocalMacroblockInfo->sbdir[sb]=D_DIR_DIRECT;
            m_cur_mb.GlobalMacroblockInfo->sbtype[sb]=SBTYPE_8x8;
        }

        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;
        return;
    }

    FactorArrayValue *pDistScaleFactorMV;

    Ipp8s RefIndexL0 = 0, RefIndexL1 = 0;

    // set up pointers to where MV and RefIndex will be stored
    Ipp32s scale_idx = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo) | (m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE);
    RefIndexL1 = 0;

    bool isAll8x8Same = true;

    H264DecoderMotionVector *pFwdMV = &m_cur_mb.MVs[0]->MotionVectors[0];
    H264DecoderMotionVector *pBwdMV = &m_cur_mb.MVs[1]->MotionVectors[0];
    RefIndexType *pRefIndexL0 = m_cur_mb.GetReferenceIndexStruct(0)->refIndexs;
    RefIndexType *pRefIndexL1 = m_cur_mb.GetReferenceIndexStruct(1)->refIndexs;

#ifdef __ICL
        __assume_aligned(pRefIndexL0, 4);
        __assume_aligned(pRefIndexL1, 4);
        __assume_aligned(pFwdMV, 16);
        __assume_aligned(pBwdMV, 16);
#endif // __ICL

    H264DecoderFrameEx *firstRefBackFrame = (H264DecoderFrameEx *)m_pRefPicList[1][0];

    Ipp32s temp = 0;
    Ipp32s scale;
    Ipp32s MBCol = GetColocatedLocation(firstRefBackFrame, GetReferenceField(m_pFields[1], 0), temp, &scale);
    bool isAll4x4RealSame1[4];
    bool isAll8x8RealSame = false;
    bool isAll8x8RealSame1 = false;

    switch(firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype)
    {
    case MBTYPE_INTER_16x8:
    case MBTYPE_INTER_8x16:
        isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
        break;
    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
        isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = false;
        break;

    case MBTYPE_INTRA:
    case MBTYPE_INTRA_16x16:
    case MBTYPE_PCM:
        if (is_direct_mb)
        {
            fill_n<Ipp8s>(&m_cur_mb.LocalMacroblockInfo->sbdir[0], 4, D_DIR_DIRECT);
            fill_n<Ipp8s>(pRefIndexL0, 4, 0);
            fill_n<Ipp8s>(pRefIndexL1, 4, 0);
            fill_n<H264DecoderMotionVector>(pFwdMV, 16, zeroVector);
            fill_n<H264DecoderMotionVector>(pBwdMV, 16, zeroVector);
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
            return;
        }
        break;

    default: // 16x16
        isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
        isAll8x8RealSame = is_direct_mb;
        isAll8x8RealSame1 = true;

        if (is_direct_mb)
        {
            H264DecoderMotionVector * directMVs;
            // Get colocated MV and translated ref index
            switch(m_pCurrentFrame->m_PictureStructureForDec)
            {
            case FLD_STRUCTURE:
                GetDirectTemporalMVFLD(MBCol, 0, directMVs, RefIndexL0);
                break;
            case FRM_STRUCTURE:
                GetDirectTemporalMV(MBCol, 0, directMVs, RefIndexL0);
                break;
            case AFRM_STRUCTURE:
                GetDirectTemporalMVMBAFF(MBCol, 0, directMVs, RefIndexL0);
                break;
            default:
                VM_ASSERT(false);
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
            }

            if (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            {
                Ipp32s curfield = (m_CurMBAddr & 1);
                Ipp32s ref0field = curfield ^ (RefIndexL0&1);
                pDistScaleFactorMV = m_pSlice->GetDistScaleFactorMVAFF()->values[curfield][ref0field][curfield];
            }
            else
            {
                pDistScaleFactorMV = m_pSlice->GetDistScaleFactorMV()->values;
            }

            H264DecoderMotionVector mvFwd;
            H264DecoderMotionVector mvBwd;

            GetScaledMV(0, directMVs, pDistScaleFactorMV,
                RefIndexL0, scale, scale_idx, mvFwd, mvBwd, m_MVDistortion);

            fill_n<Ipp8s>(&pRefIndexL0[0], 4, RefIndexL0);
            fill_n<Ipp8s>(&pRefIndexL1[0], 4, RefIndexL1);
            fill_n<H264DecoderMotionVector>(&pFwdMV[0], 16, mvFwd);
            fill_n<H264DecoderMotionVector>(&pBwdMV[0], 16, mvBwd);
            fill_n<Ipp8s>(&m_cur_mb.LocalMacroblockInfo->sbdir[0], 4, D_DIR_DIRECT);

            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
            return;
        }
        break;
    };

    Ipp32s sb;
    for (sb = 0; sb < 4; sb++)
    {
        if (m_cur_mb.GlobalMacroblockInfo->sbtype[sb] != SBTYPE_DIRECT)
        {
            isAll8x8Same = false;
            isAll8x8RealSame1 = false;
            continue;
        }

        Ipp32s sboffset = subblock_block_mapping[sb];

        Ipp32s scaleValue;
        Ipp32s MBColumn = GetColocatedLocation(firstRefBackFrame, GetReferenceField(m_pFields[1], 0), sboffset, &scaleValue);

        bool isAll4x4RealSame = isAll4x4RealSame1[sb];

        bool bRefMBIsInter = IS_INTER_MBTYPE(firstRefBackFrame->m_mbinfo.mbs[MBColumn].mbtype);

        m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_8x8;
        m_cur_mb.LocalMacroblockInfo->sbdir[sb] = D_DIR_DIRECT;

        if (bRefMBIsInter)
        {
            H264DecoderMotionVector * directMVs;
            // Get colocated MV and translated ref index
            switch(m_pCurrentFrame->m_PictureStructureForDec)
            {
            case FLD_STRUCTURE:
                GetDirectTemporalMVFLD(MBColumn, sboffset, directMVs, RefIndexL0);
                break;
            case FRM_STRUCTURE:
                GetDirectTemporalMV(MBColumn, sboffset, directMVs, RefIndexL0);
                break;
            case AFRM_STRUCTURE:
                GetDirectTemporalMVMBAFF(MBColumn, sboffset, directMVs, RefIndexL0);
                break;
            default:
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
            }

            if (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            {
                Ipp32s curfield = (m_CurMBAddr & 1);
                Ipp32s ref0field = curfield ^ (RefIndexL0&1);
                pDistScaleFactorMV = m_pSlice->GetDistScaleFactorMVAFF()->values[curfield][ref0field][curfield];
            }
            else
            {
                pDistScaleFactorMV = m_pSlice->GetDistScaleFactorMV()->values;
            }

            if (isAll4x4RealSame)
            {
                H264DecoderMotionVector mvFwd;
                H264DecoderMotionVector mvBwd;

                GetScaledMV(sboffset, directMVs, pDistScaleFactorMV,
                    RefIndexL0, scaleValue, scale_idx, mvFwd, mvBwd, m_MVDistortion);

                if (isAll8x8RealSame)
                {
                    fill_n<Ipp8s>(&pRefIndexL0[sb], 4, RefIndexL0);
                    fill_n<Ipp8s>(&pRefIndexL1[sb], 4, RefIndexL1);
                    fill_struct_n<H264DecoderMotionVector>(&pFwdMV[sboffset], 16, mvFwd);
                    fill_struct_n<H264DecoderMotionVector>(&pBwdMV[sboffset], 16, mvBwd);
                    break;
                }

                pRefIndexL0[sb] = RefIndexL0;
                pRefIndexL1[sb] = RefIndexL1;
                storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[sboffset], mvFwd);
                storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[sboffset], mvBwd);
            }
            else
            {
                Ipp32s sbtype = firstRefBackFrame->m_mbinfo.mbs[MBColumn].sbtype[sb];

                switch (sbtype)
                {
                case SBTYPE_8x8:
                    GetScaledMV(sboffset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[sboffset], pBwdMV[sboffset], m_MVDistortion);

                    storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[sboffset], pFwdMV[sboffset]);
                    storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[sboffset], pBwdMV[sboffset]);
                    break;
                case SBTYPE_8x4:
                    {
                    Ipp32s offset = sboffset;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    pFwdMV[offset + 1] = pFwdMV[offset];
                    pBwdMV[offset + 1] = pBwdMV[offset];

                    offset += 4;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    pFwdMV[offset + 1] = pFwdMV[offset];
                    pBwdMV[offset + 1] = pBwdMV[offset];
                    m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_8x4;
                    }
                    break;
                case SBTYPE_4x8:
                    {
                    Ipp32s offset = sboffset;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    pFwdMV[offset + 4] = pFwdMV[offset];
                    pBwdMV[offset + 4] = pBwdMV[offset];

                    offset += 1;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    pFwdMV[offset + 4] = pFwdMV[offset];
                    pBwdMV[offset + 4] = pBwdMV[offset];
                    m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_4x8;
                    }
                    break;
                default: //SBTYPE_4x4
                    {
                    Ipp32s offset = sboffset;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    offset++;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    offset += 3;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    offset++;
                    GetScaledMV(offset, directMVs, pDistScaleFactorMV,
                        RefIndexL0, scaleValue, scale_idx, pFwdMV[offset], pBwdMV[offset], m_MVDistortion);

                    m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_4x4;
                    }
                    break;
                }

                pRefIndexL0[sb] = RefIndexL0;
                pRefIndexL1[sb] = RefIndexL1;
            }
        }
        else
        {
            pRefIndexL0[sb] = 0;
            pRefIndexL1[sb] = 0;
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[sboffset], zeroVector);
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[sboffset], zeroVector);
        }
        /*if (isAll8x8Same)
        {
            isAll8x8Same &= isAll4x4Same &
                !((pFwdMV[sboffset].mvx ^ first_mv_fwd_all->mvx) |
                (pFwdMV[sboffset].mvy ^ first_mv_fwd_all->mvy) |
                (pBwdMV[sboffset].mvx ^ first_mv_bwd_all->mvx) |
                (pBwdMV[sboffset].mvy ^ first_mv_bwd_all->mvy) |
                (RefIndexL0 ^ *first_refL0_all) |
                (RefIndexL1 ^ *first_refL1_all));
        }*/

    } // for (sb)

    isAll8x8Same = isAll8x8RealSame1;
    // set mbtype to 8x8 if it was not; use larger type if possible
    if (isAll8x8Same)
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
    }
    else
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;
    }
} // void H264SegmentDecoder::DecodeDirectMotionVectorsTemporal(Ipp32u sboffset,
#endif

inline bool CompareBlocks(H264DecoderMotionVector *pFwdMV, Ipp8s *pRefIndexL0,
                                 Ipp32s pos1, Ipp32s pos2)
{
    return !((pFwdMV[pos1].mvx ^ pFwdMV[pos2].mvx) |
                    (pFwdMV[pos1].mvy ^ pFwdMV[pos2].mvy) |
                    (pRefIndexL0[pos1] ^ pRefIndexL0[pos2]));
}

Ipp32s compare[10];

void H264SegmentDecoder::AdujstMvsAndType()
{
    return;

#if 0
    bool isAll8x8Same = true;

    H264DecoderMotionVector *pFwdMV = &m_cur_mb.MVs[0]->MotionVectors[0];
    H264DecoderMotionVector *pBwdMV = &m_cur_mb.MVs[1]->MotionVectors[0];
    Ipp8s *pRefIndexL0 = &m_cur_mb.RefIdxs[0]->RefIdxs[0];
    //Ipp8s *pRefIndexL1 = &m_cur_mb.RefIdxs[1]->RefIdxs[0];

    H264DecoderMotionVector * first_mv_fwd_all = &(pFwdMV[0]);
    //H264DecoderMotionVector * first_mv_bwd_all = &(pBwdMV[0]);
    Ipp8s *first_refL0_all = &pRefIndexL0[0];
    //Ipp8s *first_refL1_all = &pRefIndexL1[0];

    //bool isAll8x8RealSame = false;
    //bool isAll8x8RealSame1 = false;

    switch(m_cur_mb.GlobalMacroblockInfo->mbtype)
    {
    case MBTYPE_INTER_16x8:
        printf("");
        //__asm int 3;
        break;
    case MBTYPE_INTER_8x16:
        printf("");
        //__asm int 3;
        break;
    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
        {
            /*for (Ipp32s sb = 1; sb < 4; sb++)
            {
                bool isAll4x4Same = true;
                Ipp32s sboffset = subblock_block_mapping[sb];

                isAll8x8Same &= isAll4x4Same &
                    CompareBlocks(pFwdMV, pRefIndexL0, 0, sboffset);

            }

            m_cur_mb.GlobalMacroblockInfo->mbtype = isAll8x8Same ? MBTYPE_INTER : MBTYPE_INTER_8x8;*/

            printf("16x16 - %d\n", compare[0]);
            printf("16x8 - %d\n", compare[1]);
            printf("8x16 - %d\n", compare[2]);
            printf("8x8 - %d\n", compare[3]);

            if (CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[0], subblock_block_mapping[1]) &&
                CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[1], subblock_block_mapping[2]) &&
                CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[2], subblock_block_mapping[3]))
            {
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER;
                compare[0]++;
                return;
            }

            if (CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[0], subblock_block_mapping[1]) &&
                CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[2], subblock_block_mapping[3]))
            {
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_16x8;
                compare[1]++;
                return;
            }

            if (CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[0], subblock_block_mapping[2]) &&
                CompareBlocks(pFwdMV, pRefIndexL0, subblock_block_mapping[1], subblock_block_mapping[3]))
            {
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x16;
                compare[2]++;
                return;
            }

            compare[3]++;
        }
        break;
    };
#endif
}


} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
