//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "umc_vp8_decoder.h"
using namespace UMC;

namespace UMC
{

const Ipp8u vp8_mvmode_contexts[6][4] =
{
  {  7,    1,   1, 143},
  { 14,   18,  14, 107},
  { 135,  64,  57,  68},
  {  60,  56, 128,  65},
  { 159, 134, 128,  34},
  { 234, 188, 128,  28}
};

const Ipp8s vp8_mvmode_tree[8] =
{
  -VP8_MV_ZERO,    2,
  -VP8_MV_NEAREST, 4,
  -VP8_MV_NEAR,    6,
  -VP8_MV_NEW,    -VP8_MV_SPLIT
};


const Ipp8s vp8_short_mv_tree[2*(VP8_MV_LONG - VP8_MV_SHORT)] =
{ // *2 to save a shift
    2,    8,  /* "0" subtree, "1" subtree */
    4,    6,  /* "00" subtree", "01" subtree */
  - 0,  -1*2, /* 0 = "000", 1 = "001" */
  -2*2, -3*2, /* 2 = "010", 3 = "011" */
    10,   12, /* "10" subtree, "11" subtree */
  -4*2, -5*2, /* 4 = "100", 5 = "101" */
  -6*2, -7*2  /* 6 = "110", 7 = "111" */
};


const Ipp8u vp8_mv_partitions_probs[3] = 
{
  110, 111, 150
};


const Ipp8s vp8_mv_partitions_tree[2*(VP8_MV_NUM_PARTITIONS - 1)] = 
{
  -VP8_MV_16,          2,                /* MV_16 = "0" */
  -VP8_MV_QUARTERS,    4,                /* mv_quarters = "10" */
  -VP8_MV_TOP_BOTTOM, -VP8_MV_LEFT_RIGHT /* top_bottom = "110", left_right = "111" */
};


const Ipp8u vp8_block_mv_probs[5][VP8_NUM_B_MV_MODES - 1] =
{
  {147, 136, 18},
  {106, 145,  1},
  {179, 121,  1},
  {223,   1, 34},
  {208,   1,  1}
};

const Ipp8s vp8_block_mv_tree[2 * (VP8_NUM_B_MV_MODES - 1)] =
{
  -VP8_B_MV_LEFT, 2,            /* LEFT = "0" */
  -VP8_B_MV_ABOVE, 4,           /* ABOVE = "10" */
  -VP8_B_MV_ZERO, -VP8_B_MV_NEW /* ZERO = "110", NEW = "111" */
};

typedef struct _vp8_MbPartitionInfo
{
  Ipp8u numParts;
  Ipp8u offset;

} vp8_MbPartitionInfo;


vp8_MbPartitionInfo vp8_mb_partition[VP8_MV_NUM_PARTITIONS] =
{
  {2,  8},
  {2,  2},
  {4,  2},
  {16, 1}
};


#define DECODE_SEGMENT_ID \
{ \
  Ipp8u bit, prob; \
  prob = m_FrameInfo.segmentTreeProbabilities[0]; \
  VP8_DECODE_BOOL(pBoolDec, prob, bit); \
  if (bit) \
  { \
    prob = m_FrameInfo.segmentTreeProbabilities[2]; \
    VP8_DECODE_BOOL(pBoolDec, prob, bit); \
    pMb->segmentID = 2 + bit; \
  } \
  else \
  { \
    prob = m_FrameInfo.segmentTreeProbabilities[1]; \
    VP8_DECODE_BOOL(pBoolDec, prob, bit); \
    pMb->segmentID = bit; \
  } \
}


static Ipp32s vp8_decode_mv_component(vp8BooleanDecoder *pBoolDec, Ipp8u *pContexts)
{
  Ipp8u bit;
  Ipp32s i;
  Ipp32s mv = 0;

  VP8_DECODE_BOOL(pBoolDec, pContexts[VP8_MV_IS_SHORT], bit);
  if (bit)
  {
    for (i = 0; i < 3; i++)
    {
      VP8_DECODE_BOOL(pBoolDec, pContexts[VP8_MV_LONG + i], bit);
      mv += bit << i;
    }

    for (i = VP8_MV_LONG_BITS - 1; i > 3; i--)
    {
      VP8_DECODE_BOOL(pBoolDec, pContexts[VP8_MV_LONG + i], bit);
      mv += bit << i;
    }

    if (mv < 8)
      mv += 8;
    else
    {
      VP8_DECODE_BOOL(pBoolDec, pContexts[VP8_MV_LONG + 3], bit);
      mv += bit << 3;
    }

    mv <<= 1;
  }
  else
    mv = vp8_ReadTree(pBoolDec, vp8_short_mv_tree, &pContexts[VP8_MV_SHORT]);

  if (mv)
  {
    VP8_DECODE_BOOL(pBoolDec, pContexts[VP8_MV_SIGN], bit);

    if (bit)
      mv = -mv;
  }
 
  return mv;
} // vp8_decode_mv_component()


Status VP8VideoDecoder::DecodeMbModes_Intra(vp8BooleanDecoder *pBoolDec)
{
  Ipp32u mb_row;
  Ipp32u mb_col;
  Ipp32s i;

  vp8BooleanDecoder *pBooldec = &m_BoolDecoder[0];

  vp8_MbInfo *pMb = m_pMbInfo;
  vp8_MbInfo *pPrevMb;
  vp8_MbInfo *pMbUp;

  for (mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    pPrevMb = &m_MbExternal;

    for (mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
    {
      pMb->segmentID = 0;
      pMb->refFrame  = VP8_INTRA_FRAME;

      if (m_FrameInfo.updateSegmentMap)
        DECODE_SEGMENT_ID

      if (m_FrameInfo.mbSkipEnabled)
        VP8_DECODE_BOOL(pBooldec, m_FrameInfo.skipFalseProb, pMb->skipCoeff)
      else
        pMb->skipCoeff = 0; //???

      pMb->mode = (Ipp8u)vp8_ReadTree(pBooldec, vp8_kf_mb_mode_y_tree, vp8_kf_mb_mode_y_probs);
      if (VP8_MB_B_PRED == pMb->mode)
      {
        Ipp32s modeUp;
        Ipp32s modeL;

        pMbUp = mb_row ? pMb - m_FrameInfo.mbStep : &m_MbExternal;

        for (i = 0; i < 4; i++)
        {
          modeUp = pMbUp->blockMode[12+i];
          modeL  = i ? pMb->blockMode[i-1] : pPrevMb->blockMode[3];

          pMb->blockMode[i] = (Ipp8u)vp8_ReadTree(pBooldec, vp8_block_mode_tree, vp8_kf_block_mode_probs[modeUp][modeL]);
        }

        for (i = 4; i < 16; i++)
        {
          modeUp = pMb->blockMode[i-4];
          modeL  = (i & 3) ? pMb->blockMode[i-1] : pPrevMb->blockMode[i+3];

          pMb->blockMode[i] = (Ipp8u)vp8_ReadTree(pBooldec, vp8_block_mode_tree, vp8_kf_block_mode_probs[modeUp][modeL]);
        }
      }
      else
      {
        if(pMb->mode < sizeof(vp8_mbmode_2_blockmode_u32) / sizeof(vp8_mbmode_2_blockmode_u32[0]))
            for (i = 0; i < 4; i++)
                *(Ipp32u*)(pMb->blockMode + 4*i) = vp8_mbmode_2_blockmode_u32[pMb->mode];
        // else what ? 
      }

      pMb->modeUV = (Ipp8u)vp8_ReadTree(pBooldec, vp8_mb_mode_uv_tree, vp8_kf_mb_mode_uv_probs);

      pPrevMb = pMb;
      pMb++;
    }
  }

  return UMC_OK;
} // VP8VideoDecoder::DecodeMbModes_Intra()


#define VP8_MV_BORDER 128 // 16 pixels, 1/8th-pel MV


#define VP8_MV_BIAS(ref0, ref1, mv) \
  if (m_RefreshInfo.refFrameBiasTable[ref0 ^ ref1]) \
  { \
    mv.mvx = -mv.mvx; \
    mv.mvy = -mv.mvy; \
  }


#define VP8_LIMIT_MV(mv) \
  if ((mv).mvx < limitL) \
    (mv).mvx = (Ipp16s)limitL; \
  else if ((mv).mvx > limitR) \
    (mv).mvx = (Ipp16s)limitR; \
  \
  if ((mv).mvy < limitT) \
    (mv).mvy = (Ipp16s)limitT; \
  else if ((mv).mvy > limitB) \
    (mv).mvy = (Ipp16s)limitB;


Status VP8VideoDecoder::DecodeMbModesMVs_Inter(vp8BooleanDecoder *pBoolDec)
{
  Status sts = UMC_OK;

  Ipp32s mb_row;
  Ipp32s mb_col;
  Ipp32s i;
  Ipp32s j;

  Ipp8u bit;
  Ipp8u probIntra;
  Ipp8u probLast;
  Ipp8u probGF;

  vp8BooleanDecoder *pBooldec = &m_BoolDecoder[0];

  vp8_MbInfo *pMb = m_pMbInfo;
  vp8_MbInfo *pMbLeft;
  vp8_MbInfo *pMbUp;
  vp8_MbInfo *pMbUpLeft;

  Ipp32s  limitL;
  Ipp32s  limitR;
  Ipp32s  limitT;
  Ipp32s  limitB;

  enum { CNT_ZERO, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV };

  probIntra = (Ipp8u)DecodeValue_Prob128(pBoolDec, 8);
  probLast  = (Ipp8u)DecodeValue_Prob128(pBoolDec, 8);
  probGF    = (Ipp8u)DecodeValue_Prob128(pBoolDec, 8);

  VP8_DECODE_BOOL_PROB128(pBoolDec, bit);
  if (bit)
  {
    for (i = 0; i < VP8_NUM_MB_MODES_Y - 1; i++)
      m_FrameProbs.mbModeProbY[i] = (Ipp8u)DecodeValue_Prob128(pBoolDec, 8);
  }

  VP8_DECODE_BOOL_PROB128(pBoolDec, bit);
  if (bit)
  {
    for (i = 0; i < VP8_NUM_MB_MODES_UV - 1; i++)
      m_FrameProbs.mbModeProbUV[i] = (Ipp8u)DecodeValue_Prob128(pBoolDec, 8);
  }

  // update MV contexts
  for (i = 0; i < 2; i++)
  {
    const Ipp8u *pProb = vp8_mv_update_probs[i];
    Ipp8u       *pCtxt = m_FrameProbs.mvContexts[i];

    for (j = 0; j < VP8_NUM_MV_PROBS; j++)
    {
      Ipp8u bit;
      Ipp8u contxt;

      VP8_DECODE_BOOL(pBoolDec, pProb[j], bit);
      if (bit)
      {
        contxt   = (Ipp8u)DecodeValue_Prob128(pBoolDec, 7) << 1;
        pCtxt[j] = contxt ? contxt : 1;
      }
    }
  }

  for (mb_row = 0; mb_row < static_cast<signed>(m_FrameInfo.mbPerCol); mb_row++)
  {
    pMbUpLeft = &m_MbExternal;
    pMbLeft   = &m_MbExternal;
    limitT    = -((mb_row + 1) << 7); // 16 pixels, 1/8th-pel MV, 16-pel padding
    limitB    = (m_FrameInfo.mbPerCol - mb_row) << 7;

    for (mb_col = 0; mb_col < static_cast<signed>(m_FrameInfo.mbPerRow); mb_col++)
    {
      limitL = -((mb_col + 1) << 7); // 16 pixels, 1/8th-pel MV
      limitR = (m_FrameInfo.mbPerRow - mb_col) << 7;

      if (m_FrameInfo.updateSegmentMap)
        DECODE_SEGMENT_ID

      if (m_FrameInfo.mbSkipEnabled)
        VP8_DECODE_BOOL(pBooldec, m_FrameInfo.skipFalseProb, pMb->skipCoeff)
      else
        pMb->skipCoeff = 0; //???

      VP8_DECODE_BOOL(pBooldec, probIntra, pMb->refFrame);

      if (pMb->refFrame)
      {
        VP8_DECODE_BOOL(pBooldec, probLast, bit);
        if (bit)
        {
          VP8_DECODE_BOOL(pBooldec, probGF, bit);
          pMb->refFrame = (Ipp8u)(VP8_GOLDEN_FRAME + bit); // gold or altref
        }

        Ipp8u            mvModeProbs[4];
        Ipp32s           mvCnt[4];
        vp8_MotionVector nearMVs[4];
        vp8_MotionVector mv;

        // CNT_ZERO, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV

        nearMVs[CNT_ZERO].s32    = 0;
        nearMVs[CNT_NEAREST].s32 = 0;
        nearMVs[CNT_NEAR].s32    = 0;

        mvCnt[CNT_ZERO]    = 0;
        mvCnt[CNT_NEAREST] = 0;
        mvCnt[CNT_NEAR]    = 0;
        mvCnt[CNT_SPLITMV] = 0;

        if (!mb_row)
        {
          if (pMbLeft->refFrame != VP8_INTRA_FRAME)
          {
            if (pMbLeft->mv.s32)
            {
              nearMVs[CNT_NEAREST].s32 = pMbLeft->mv.s32;

              VP8_MV_BIAS(pMbLeft->refFrame, pMb->refFrame, nearMVs[CNT_NEAREST]);
              VP8_LIMIT_MV(nearMVs[CNT_NEAREST]);

              nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;
              mvCnt[CNT_NEAREST]    = 2;
            }
            else
              mvCnt[CNT_ZERO] = 2;
          }

          mvCnt[CNT_SPLITMV] = pMbLeft->mode == VP8_MV_SPLIT ? 2 : 0;
          pMbUp    = &m_MbExternal;
        }
        else
        {
          Ipp32s           indx = 0;
          vp8_MotionVector curMV;
          vp8_MbInfo      *pNeighbMb;

          // process Above Mb
          pNeighbMb = pMb - m_FrameInfo.mbStep;
          pMbUp     = pMb - m_FrameInfo.mbStep;

          if (pNeighbMb->refFrame != VP8_INTRA_FRAME)
          {
            if (pNeighbMb->mv.s32)
            {
              nearMVs[CNT_NEAREST].s32 = pNeighbMb->mv.s32;

              VP8_MV_BIAS(pNeighbMb->refFrame, pMb->refFrame, nearMVs[CNT_NEAREST]);

              mvCnt[CNT_NEAREST] = 2;
              indx               = 1;
            }
            else
              mvCnt[CNT_ZERO] = 2;
          }

          //process Left Mb
          pNeighbMb = pMbLeft;

          if (pNeighbMb->refFrame != VP8_INTRA_FRAME)
          {
            if (pNeighbMb->mv.s32) 
            {
              curMV.s32 = pNeighbMb->mv.s32;

              VP8_MV_BIAS(pNeighbMb->refFrame, pMb->refFrame, curMV);

              if (curMV.s32 != nearMVs[CNT_NEAREST].s32)
              {
                indx++;
                nearMVs[indx].s32 = curMV.s32;
              }

              mvCnt[indx] += 2;
            }
            else
              mvCnt[CNT_ZERO] += 2;
          }

          // process AboveLeft Mb
          pNeighbMb = pMbUpLeft;

          if (pNeighbMb->refFrame != VP8_INTRA_FRAME)
          {
            if (pNeighbMb->mv.s32)
            {
              curMV.s32 = pNeighbMb->mv.s32;

              VP8_MV_BIAS(pNeighbMb->refFrame, pMb->refFrame, curMV);

              if (curMV.s32 != nearMVs[indx].s32)
              {
                indx++;
                nearMVs[indx].s32 = curMV.s32;
              }

              mvCnt[indx] += 1;
            }
            else
              mvCnt[CNT_ZERO] += 1;
          }

          if(mvCnt[CNT_SPLITMV] &&
            (nearMVs[CNT_NEAREST].s32 == nearMVs[CNT_SPLITMV].s32))
          {
            mvCnt[CNT_NEAREST] += 1;
          }

          if(mvCnt[CNT_NEAR] > mvCnt[CNT_NEAREST])
          {//swap MV[1] & MV[2]
            Ipp32s tmp;

            tmp = nearMVs[CNT_NEAR].s32;
            nearMVs[CNT_NEAR].s32    = nearMVs[CNT_NEAREST].s32;
            nearMVs[CNT_NEAREST].s32 = tmp;

            tmp = mvCnt[CNT_NEAR];
            mvCnt[CNT_NEAR]    = mvCnt[CNT_NEAREST];
            mvCnt[CNT_NEAREST] = tmp;
          }

          indx = (indx > 2) ? 2 : indx; //???

          for (i = 1; i <= indx; i++)
          {
            VP8_LIMIT_MV(nearMVs[i]);
          }


          if (mvCnt[CNT_NEAREST] >= mvCnt[CNT_ZERO])
            nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;

          mvCnt[CNT_SPLITMV] = (pMbLeft->mode   == VP8_MV_SPLIT ? 2 : 0) +
                               (pMbUp->mode     == VP8_MV_SPLIT ? 2 : 0) +
                               (pMbUpLeft->mode == VP8_MV_SPLIT ? 1 : 0);

          pMbUpLeft = pMbUp;
        }

        for (i = 0; i < 4; i++)
          mvModeProbs[i] = vp8_mvmode_contexts[mvCnt[i]][i];

        pMb->mode = (Ipp8u)vp8_ReadTree(pBooldec, vp8_mvmode_tree, mvModeProbs);

        switch (pMb->mode)
        {
        case VP8_MV_NEAREST:
        case VP8_MV_NEAR:
          mv.s32 = nearMVs[pMb->mode - VP8_MV_NEAREST + 1].s32;
          break;

        case VP8_MV_NEW:
          mv.mvy = (Ipp16s)vp8_decode_mv_component(pBooldec, m_FrameProbs.mvContexts[0]);
          mv.mvx = (Ipp16s)vp8_decode_mv_component(pBooldec, m_FrameProbs.mvContexts[1]);

          mv.mvx += nearMVs[CNT_ZERO].mvx;
          mv.mvy += nearMVs[CNT_ZERO].mvy;
          break;

        case VP8_MV_SPLIT:
        {
          Ipp32s partition, i;

          partition       = vp8_ReadTree(pBooldec, vp8_mv_partitions_tree, vp8_mv_partitions_probs);
          pMb->partitions = (Ipp8u)partition;

          vp8_MbPartitionInfo *pInfo  = &vp8_mb_partition[partition];
          Ipp8u                offset = pInfo->offset;
          Ipp32s               bl     = 0;

          for (i = 0; i < pInfo->numParts; i++)
          {
            Ipp32s           mvl;
            Ipp32s           mvu;
            vp8_MotionVector bmv;

            if (bl < 4)
              mvu = pMbUp->blockMV[bl + 12].s32;
            else
              mvu = pMb->blockMV[bl - 4].s32;

            mvl = (bl & 3) ? pMb->blockMV[bl - 1].s32 : pMbLeft->blockMV[bl + 3].s32;

            if (mvl == mvu)
              j = mvl ? 3 : 4;
            else
              j = (mvl ? 0 : 1) + (mvu ? 0 : 2);

            const Ipp8u *pProbs = vp8_block_mv_probs[j];
            Ipp8u        blMode = (Ipp8u)vp8_ReadTree(pBooldec, vp8_block_mv_tree, pProbs);

            switch (blMode)
            {
            case VP8_B_MV_LEFT:
              bmv.s32 = mvl;
              break;

            case VP8_B_MV_ABOVE:
              bmv.s32 = mvu;
              break;

            case VP8_B_MV_NEW:
              bmv.mvy = (Ipp16s)(nearMVs[CNT_ZERO].mvy + vp8_decode_mv_component(pBooldec, m_FrameProbs.mvContexts[0]));
              bmv.mvx = (Ipp16s)(nearMVs[CNT_ZERO].mvx + vp8_decode_mv_component(pBooldec, m_FrameProbs.mvContexts[1]));
              break;

            case VP8_B_MV_ZERO:
            default:
              bmv.s32 = 0;
              break;
            }

            if (partition == VP8_MV_TOP_BOTTOM) //16*8
            {
              for (j = 0; j < 8; j++)
              {
                pMb->blockMode[bl + j]   = blMode;
                pMb->blockMV[bl + j].s32 = bmv.s32;
              }
            }
            else if (partition == VP8_MV_LEFT_RIGHT) //8x16
            {
              for (j = 0; j < 16; j += 4)
              {
                pMb->blockMode[bl + j]     = blMode;
                pMb->blockMode[bl + j + 1] = blMode;

                pMb->blockMV[bl + j].s32     = bmv.s32;
                pMb->blockMV[bl + j + 1].s32 = bmv.s32;
              }
            }
            else if (partition == VP8_MV_QUARTERS)
            {
              for (j = 0; j < 8; j += 4)
              {
                pMb->blockMode[bl + j]     = blMode;
                pMb->blockMode[bl + j + 1] = blMode;

                pMb->blockMV[bl + j].s32 = bmv.s32;
                pMb->blockMV[bl + j + 1].s32 = bmv.s32;
              }

              if (i == 1)
                bl += 4;

            }
            else // VP8_MV_16
            {
              pMb->blockMode[bl]   = blMode;
              pMb->blockMV[bl].s32 = bmv.s32;
            }

            bl += offset;
          }
          break;
        }

        case VP8_MV_ZERO:
        default:
          mv.s32 = 0;
          break;
        }

        if (pMb->mode != VP8_MV_SPLIT)
        {
          pMb->mv.s32 = mv.s32;

          for (j = 0; j < 16; j++)
            pMb->blockMV[j].s32 = mv.s32;
        }
        else
         pMb->mv.s32 = pMb->blockMV[15].s32; //?????
      }
      else // if pMb->refFrame==VP8_INTRA_FRAME
      {
        memset(pMb->blockMV, 0, 16*sizeof(vp8_MotionVector));

        pMb->mv.s32 = 0;
        pMb->mode   = (Ipp8u)vp8_ReadTree(pBooldec, vp8_mb_mode_y_tree, m_FrameProbs.mbModeProbY);

        if (pMb->mode == VP8_MB_B_PRED)
        {
          for (j = 0; j < 16; j++) // "vp8_block_mode_probs" is fixed table
            pMb->blockMode[j] = (Ipp8u)vp8_ReadTree(pBooldec, vp8_block_mode_tree, vp8_block_mode_probs);
        }

        pMb->modeUV = (Ipp8u)vp8_ReadTree(pBooldec, vp8_mb_mode_uv_tree, m_FrameProbs.mbModeProbUV);
      }

      pMbLeft   = pMb;

      if(mb_row)
        pMbUpLeft = pMbLeft - m_FrameInfo.mbStep;

      pMb++;
    }
  }

  return sts;
} // VP8VideoDecoder::DecodeMbModesMVs_Inter()


} // namespace UMC

#endif // UMC_ENABLE_VP8_VIDEO_DECODER

