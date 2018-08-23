//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_dec_internal_cabac.h"

namespace UMC
{

const
uint8_t pCodFBD[5][4] =
{
//fwd
{CodInBS, CodNone, CodInBS, CodNone},
//bwd
{CodNone, CodInBS, CodNone, CodInBS},
//nothing
{CodNone, CodInBS, CodNone, CodInBS},
//nothing
{CodNone, CodInBS, CodNone, CodInBS},
//both
{CodInBS, CodInBS, CodInBS, CodInBS},
};

const
uint8_t pCodTemplate[16] =
{
    CodNone, CodLeft, CodLeft, CodLeft,    CodAbov, CodLeft, CodLeft, CodLeft,
    CodAbov, CodLeft, CodLeft, CodLeft,    CodAbov, CodLeft, CodLeft, CodLeft
};

const
uint32_t sb_x[4][16] =
{
    {
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
    },
    {
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
    },
    {
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
        0, 1, 0, 1,
    },
    {
        0, 1, 0, 1,
        2, 3, 2, 3,
        0, 1, 0, 1,
        2, 3, 2, 3
    }
};

const
uint32_t sb_y[4][16] =
{
    {
        0, 0, 1, 1,
        0, 0, 1, 1,
        0, 0, 1, 1,
        0, 0, 1, 1,
    },
    {
        0, 0, 1, 1,
        0, 0, 1, 1,
        0, 0, 1, 1,
        0, 0, 1, 1,
    },
    {
        0, 0, 1, 1,
        2, 2, 3, 3,
        0, 0, 1, 1,
        2, 2, 3, 3
    },
    {
        0, 0, 1, 1,
        0, 0, 1, 1,
        2, 2, 3, 3,
        2, 2, 3, 3
    }
};

const
int32_t sign_mask[2] =
{
    0, -1
};

// ---------------------------------------------------------------------------
//  H264SegmentDecoder::GetRefIdx4x4_CABAC()
//    get ref_idx and update info for all 4x4 blocks
// ---------------------------------------------------------------------------

void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,
                                              const uint8_t* , //pBlkIdx,
                                              const uint8_t* pCodRIx,
                                              uint32_t ListNum)
{
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

    for (int32_t i = 0; i < 4; i ++)
    {
        int32_t j = subblock_block_mapping[i];

        switch (pCodRIx[j])
        {
        case CodNone:
            pRIx[i] = -1;
            break;

        case CodInBS:
            if (nActive > 1)
            {
                int8_t refIdx;
                refIdx = (int8_t) GetSE_RefIdx_CABAC(ListNum, i);
                if (refIdx >= (int8_t) nActive || refIdx < 0)
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
                pRIx[i] = refIdx;
            }
            else
            {
                pRIx[i] = 0;
            }
            break;

        case CodLeft:
            pRIx[i] = pRIx[i - 1];
            break;

        case CodAbov:
            pRIx[i] = pRIx[i - 2];
            break;

        case CodSkip:
            break;
        }
    }    // for i
} // void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,


// ---------------------------------------------------------------------------
//  H264SegmentDecoder::GetRefIdx4x4_CABAC()
//    get ref_idx and update info for all 4x4 blocks
// ---------------------------------------------------------------------------

void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,
                                              const uint8_t pCodRIx,
                                              uint32_t ListNum)
{
    int8_t refIdx = 0;
    if(pCodRIx == CodNone)
    {
        refIdx = -1;
    }
    else if (nActive > 1)
    {
        refIdx = (int8_t) GetSE_RefIdx_CABAC(ListNum, 0);
        if (refIdx >= (int8_t) nActive || refIdx < 0)
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }
    }

    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pRIx, 4);
#endif

    fill_n<RefIndexType>(pRIx, 4, refIdx);

} // void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,

void H264SegmentDecoder::GetRefIdx4x4_16x8_CABAC(const uint32_t nActive,
                                                   const uint8_t* pCodRIx,
                                                   uint32_t ListNum)
{
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pRIx, 8);
#endif
    RefIndexType refIdx = 0;

    if(pCodRIx[0] == CodNone)
    {
        refIdx = -1;
    }
    else if (nActive > 1)
    {
        refIdx = (int8_t) GetSE_RefIdx_CABAC(ListNum,0);
        if (refIdx >= (int8_t) nActive || refIdx < 0)
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }
    }

    fill_n<int8_t>(pRIx, 2, refIdx);

    if(pCodRIx[8] == CodNone)
    {
        refIdx = -1;
    }
    else if (nActive > 1)
    {
        refIdx = (int8_t) GetSE_RefIdx_CABAC(ListNum, 2);
        if (refIdx >= (int8_t) nActive || refIdx < 0)
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }
    }
    else
    {
        refIdx = 0;
    }

    fill_n<int8_t>(&pRIx[2], 2, refIdx);

} // void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,

void H264SegmentDecoder::GetRefIdx4x4_8x16_CABAC(const uint32_t nActive,
                                                   const uint8_t* pCodRIx,
                                                   uint32_t ListNum)
{
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pRIx, 16);
#endif
    if(pCodRIx[0] == CodNone)
    {
        pRIx[0] = pRIx[2] = -1;
    }
    else if (nActive > 1)
    {
        RefIndexType refIdx;
        refIdx = (RefIndexType) GetSE_RefIdx_CABAC(ListNum,0);
        if (refIdx >= (RefIndexType) nActive || refIdx < 0)
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }
        pRIx[0] = pRIx[2] = refIdx;
    }
    else
    {
        pRIx[0] = pRIx[2] = 0;
    }

    if(pCodRIx[2] == CodNone)
    {
        pRIx[1] = pRIx[3] = -1;
    }
    else if (nActive > 1)
    {
        RefIndexType refIdx;
        refIdx = (RefIndexType) GetSE_RefIdx_CABAC(ListNum, 1);
        if (refIdx >= (int8_t) nActive || refIdx < 0)
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }
        pRIx[1] = pRIx[3] = refIdx;
   }
    else
    {
        pRIx[1] = pRIx[3] = 0;
    }
} // void H264SegmentDecoder::GetRefIdx4x4_CABAC(const uint32_t nActive,

int32_t H264SegmentDecoder::GetSE_RefIdx_CABAC(uint32_t ListNum,
                                              uint32_t block8x8)
{
    uint32_t ctxIdxInc = 0;
    int32_t ref_idx = 0;

    // new
    RefIndexType LeftRefIdx = 0;
    RefIndexType TopRefIdx = 0;

    int32_t iTopMB = m_CurMBAddr, iLeftMB = m_CurMBAddr;

    int32_t leftFlag = 0;
    int32_t topFlag = 0;

    if (!(block8x8 & 1)) // on left edge
    {
        int32_t BlockNum = subblock_block_mapping[block8x8]; // MBAFF case: i.e. second 8x8 block can use first 8x8 block of left MB ()
        iLeftMB = m_cur_mb.CurrentBlockNeighbours.mbs_left[BlockNum / 4].mb_num;

        if (0 <= iLeftMB)
        {
            int32_t iNum;

            iNum = m_cur_mb.CurrentBlockNeighbours.mbs_left[BlockNum / 4].block_num;

            LeftRefIdx = GetReferenceIndex(m_gmbinfo, ListNum, iLeftMB, iNum);

            int32_t left_block8x8 = block_subblock_mapping[iNum] / 4;

            bool is_skip = (IS_INTRA_MBTYPE(m_gmbinfo->mbs[iLeftMB].mbtype)) ||
                GetMBDirectSkipFlag(m_gmbinfo->mbs[iLeftMB]) ||
                !(m_mbinfo.mbs[iLeftMB].sbdir[left_block8x8] < D_DIR_DIRECT);

            leftFlag = is_skip ? 0 : 1;
        }
    } else {
        bool is_skip = !(m_cur_mb.LocalMacroblockInfo->sbdir[block8x8 - 1] < D_DIR_DIRECT);
        leftFlag = is_skip ? 0 : 1;
        LeftRefIdx = m_cur_mb.GetRefIdx(ListNum, block8x8 - 1);
    }

    if (block8x8 < 2) // on top edge
    {
        iTopMB = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num;

        if (0 <= iTopMB)
        {
            TopRefIdx = GetRefIdx(m_gmbinfo, ListNum, iTopMB, block8x8 + 2);

            bool is_skip = IS_INTRA_MBTYPE(m_gmbinfo->mbs[iTopMB].mbtype) ||
                GetMBDirectSkipFlag(m_gmbinfo->mbs[iTopMB]) ||
                !(m_mbinfo.mbs[iTopMB].sbdir[block8x8 + 2] < D_DIR_DIRECT);

            topFlag = is_skip ? 0 : 1;
        }
    } else {
        bool is_skip = !(m_cur_mb.LocalMacroblockInfo->sbdir[block8x8 - 2] < D_DIR_DIRECT);
        topFlag = is_skip ? 0 : 1;
        TopRefIdx = m_cur_mb.GetRefIdx(ListNum, block8x8 - 2);
    }

    if (0 <= iLeftMB)
    {
        uint8_t lval = (uint8_t) (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo) <
                              GetMBFieldDecodingFlag(m_gmbinfo->mbs[iLeftMB]));

        if ((LeftRefIdx > lval) && leftFlag)
            ctxIdxInc ++;
    }

    if (0 <= iTopMB)
    {
        uint8_t tval = (uint8_t) (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo) <
                              GetMBFieldDecodingFlag(m_gmbinfo->mbs[iTopMB]));

        if ((TopRefIdx > tval) && topFlag)
            ctxIdxInc += 2;
    }

    if (m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[REF_IDX_L0] + ctxIdxInc)) // binIdx 0
    {
        ref_idx ++;

        if (m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[REF_IDX_L0] + 4)) // binIdx 1
        {
            ref_idx ++;

            while (m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[REF_IDX_L0] + 5)) // binIdx 2+
                ref_idx ++;
        }
    }

    return ref_idx;

} // int32_t H264SegmentDecoder::GetSE_RefIdx_CABAC(uint32_t ListNum, uint32_t BlockNum)

H264DecoderMotionVector H264SegmentDecoder::GetSE_MVD_CABAC(uint32_t ListNum, uint32_t BlockNum)
{
    uint32_t ctxIdxIncx = 0;
    uint32_t ctxIdxIncy = 0;
    int32_t mvdx = 0,mvdy = 0;
    uint32_t lcode,k;
    uint32_t mv_y;
    int32_t code;
    int32_t iBlock;
    const H264DecoderMotionVector *LeftMVd = 0;
    const H264DecoderMotionVector *TopMVd = 0;
    int32_t iTopMB = m_CurMBAddr, iLeftMB = m_CurMBAddr;

    if (BLOCK_IS_ON_LEFT_EDGE(BlockNum))
    {
        iLeftMB = m_cur_mb.CurrentBlockNeighbours.mbs_left[BlockNum / 4].mb_num;
        iBlock = m_cur_mb.CurrentBlockNeighbours.mbs_left[BlockNum / 4].block_num;

        if (0 <= iLeftMB)
        {
            bool is_skip = (IS_INTRA_MBTYPE(m_gmbinfo->mbs[iLeftMB].mbtype)) ||
                GetMBDirectSkipFlag(m_gmbinfo->mbs[iLeftMB]) ||
                !(m_mbinfo.mbs[iLeftMB].sbdir[subblock_block_membership[iBlock]] < D_DIR_DIRECT);

            if (is_skip)
            {
                LeftMVd = &zeroVector;
            }
            else
            {
                LeftMVd = m_mbinfo.MVDeltas[ListNum][iLeftMB].MotionVectors + iBlock;
            }
        }
    }
    else
    {
        LeftMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors + BlockNum - 1;
    }

    if (BLOCK_IS_ON_TOP_EDGE(BlockNum))
    {
        iTopMB = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num;
        iBlock = m_cur_mb.CurrentBlockNeighbours.mb_above.block_num + BlockNum;

        if (0 <= iTopMB)
        {
            bool is_skip = (IS_INTRA_MBTYPE(m_gmbinfo->mbs[iTopMB].mbtype)) ||
                GetMBDirectSkipFlag(m_gmbinfo->mbs[iTopMB]) ||
                !(m_mbinfo.mbs[iTopMB].sbdir[subblock_block_membership[iBlock]] < D_DIR_DIRECT);

            if (is_skip)
            {
                TopMVd = &zeroVector;
            }
            else
            {
                TopMVd = m_mbinfo.MVDeltas[ListNum][iTopMB].MotionVectors + iBlock;
            }
        }
    }
    else
    {
        TopMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors + BlockNum - 4;
    }

    if (0 <= iLeftMB)
    {
        int32_t sign = sign_mask[LeftMVd->mvx<0];
        ctxIdxIncx += ((((int32_t)LeftMVd->mvx) ^ sign) - sign);

        sign = sign_mask[LeftMVd->mvy<0];
        mv_y = ((((int32_t)LeftMVd->mvy) ^ sign) - sign);

        if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[iLeftMB]) >
            pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            mv_y <<= 1;
        else if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[iLeftMB]) <
                 pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            mv_y >>= 1;

        ctxIdxIncy += mv_y;
    }

    if (0 <= iTopMB)
    {
        int32_t sign = sign_mask[TopMVd->mvx<0];
        ctxIdxIncx += ((((int32_t)TopMVd->mvx) ^ sign) - sign);

        sign = sign_mask[TopMVd->mvy<0];
        mv_y = ((((int32_t)TopMVd->mvy) ^ sign) - sign);

        if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[iTopMB]) >
            pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            mv_y <<= 1;
        else if (GetMBFieldDecodingFlag(m_gmbinfo->mbs[iTopMB]) <
                 pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            mv_y >>= 1;

        ctxIdxIncy += mv_y;
    }

    ctxIdxIncx = ctxIdxIncx < 3? 0: ctxIdxIncx <= 32? 1: 2;
    ctxIdxIncy = ctxIdxIncy < 3? 0: ctxIdxIncy <= 32? 1: 2;

    if(m_pBitStream->DecodeSingleBin_CABAC(
                    ctxIdxOffset[MVD_L0_0] + ctxIdxIncx))
    {
        code = m_pBitStream->DecodeSingleBin_CABAC(
                        ctxIdxOffset[MVD_L0_0] + 3);

        if (code)
        {
            code = 0;
            ctxIdxIncx = 4;
            do
            {
                lcode = m_pBitStream->DecodeSingleBin_CABAC(
                                    ctxIdxOffset[MVD_L0_0] + ctxIdxIncx);
                if (6 > ctxIdxIncx) ctxIdxIncx ++;
                code++;
            }
            while(code < 7 && lcode);

            if (lcode)
            {
                k = 1 << 3;
                while(m_pBitStream->DecodeBypass_CABAC())
                {
                    code += k;
                    k <<= 1;
                }
                uint32_t symb = 0;
                while(k >>=1)
                {
                    if (m_pBitStream->DecodeBypass_CABAC())
                      symb |= k;
                }
                code += symb + 1;
            }

        }
        code++;

        mvdx = m_pBitStream->DecodeBypassSign_CABAC(code);
    }

    if(m_pBitStream->DecodeSingleBin_CABAC(
                ctxIdxOffset[MVD_L0_1] + ctxIdxIncy))
    {
        code = m_pBitStream->DecodeSingleBin_CABAC(
                        ctxIdxOffset[MVD_L0_1] + 3);

        if (code)
        {
            code = 0;
            ctxIdxIncy = 4;
            do
            {
                lcode = m_pBitStream->DecodeSingleBin_CABAC(
                                    ctxIdxOffset[MVD_L0_1] + ctxIdxIncy);
                if (6 > ctxIdxIncy) ctxIdxIncy ++;
                code++;
            }
            while(code < 7 && lcode);

            if (lcode)
            {
                k = 1 << 3;
                while(m_pBitStream->DecodeBypass_CABAC())
                {
                    code += k;
                    k <<= 1;
                }
                uint32_t symb = 0;
                while(k >>=1)
                {
                    if (m_pBitStream->DecodeBypass_CABAC())
                      symb |= k;
                }
                code += symb + 1;
            }
        }
        code++;

        mvdy = m_pBitStream->DecodeBypassSign_CABAC(code);
    }

    H264DecoderMotionVector mvd;
    mvd.mvx = (int16_t) mvdx;
    mvd.mvy = (int16_t) mvdy;

    return mvd;

} // H264DecoderMotionVector H264SegmentDecoder::GetSE_MVD_CABAC(int32_t ListNum, int32_t BlockNum)

void H264SegmentDecoder::DecodeMBQPDelta_CABAC(void)
{
    uint32_t code;

    // decode QP delta
    {
        uint32_t ctxIdxInc;

        // check for usual case of zero QP delta
        ctxIdxInc = (m_prev_dquant) ? (1) : (0);
        code = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[MB_QP_DELTA] +
                                                ctxIdxInc);
        if (code)
        {
            code = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[MB_QP_DELTA] +
                                                    2);
            if (code)
            {
                while (m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[MB_QP_DELTA] + 3))
                {
                    code += 1;
                    if (code > 50*2)
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
            }

            code += 1;
        }
    }

    // calculate new QP
    {
        int32_t qpdelta;
        int32_t bitdepth_luma_qp_scale;

        qpdelta = (int32_t) ((code + 1) / 2);
        // least significant bit is int32_t bit
        if (0 == (code & 0x01))
            qpdelta = -qpdelta;

        m_cur_mb.LocalMacroblockInfo->QP = (int8_t) (m_cur_mb.LocalMacroblockInfo->QP + qpdelta);

        bitdepth_luma_qp_scale = 6 * (bit_depth_luma - 8);
        m_cur_mb.LocalMacroblockInfo->QP  = (int8_t) (((m_cur_mb.LocalMacroblockInfo->QP +
                                                       52 +
                                                       2 * bitdepth_luma_qp_scale) %
                                                      (bitdepth_luma_qp_scale + 52)) -
                                                     bitdepth_luma_qp_scale);
        m_prev_dquant = qpdelta;
    }

} // void H264SegmentDecoder::DecodeMBQPDelta_CABAC(void)

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
