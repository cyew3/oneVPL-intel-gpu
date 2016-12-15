//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

#include "ippdefs.h"
#include "umc_defs.h"
#include "umc_h264_core_enc.h"
#include "umc_h264_bme.h"
#include "ippi.h"

#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)
#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif

namespace UMC_H264_ENCODER
{

static Ipp32s chromaDCOffset[3][16] = {
  { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0 , 0,  0,  0,  0,  0 },
  { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15}
};


Ipp32u Hadamard8x8_16_AbsSum(COEFFSTYPE *diff)
{
  __ALIGN16 Ipp32s tmpBuff[8][8];
  Ipp32s i;
  Ipp32u satd = 0;

  for (i = 0; i < 8; i++) {
    Ipp32s t0 = diff[i*16+0] + diff[i*16+4];
    Ipp32s t4 = diff[i*16+0] - diff[i*16+4];
    Ipp32s t1 = diff[i*16+1] + diff[i*16+5];
    Ipp32s t5 = diff[i*16+1] - diff[i*16+5];
    Ipp32s t2 = diff[i*16+2] + diff[i*16+6];
    Ipp32s t6 = diff[i*16+2] - diff[i*16+6];
    Ipp32s t3 = diff[i*16+3] + diff[i*16+7];
    Ipp32s t7 = diff[i*16+3] - diff[i*16+7];
    Ipp32s s0 = t0 + t2;
    Ipp32s s2 = t0 - t2;
    Ipp32s s1 = t1 + t3;
    Ipp32s s3 = t1 - t3;
    Ipp32s s4 = t4 + t6;
    Ipp32s s6 = t4 - t6;
    Ipp32s s5 = t5 + t7;
    Ipp32s s7 = t5 - t7;
    tmpBuff[i][0] = s0 + s1;
    tmpBuff[i][1] = s0 - s1;
    tmpBuff[i][2] = s2 + s3;
    tmpBuff[i][3] = s2 - s3;
    tmpBuff[i][4] = s4 + s5;
    tmpBuff[i][5] = s4 - s5;
    tmpBuff[i][6] = s6 + s7;
    tmpBuff[i][7] = s6 - s7;
  }
  for (i = 0; i < 8; i++) {
    Ipp32s t0 = tmpBuff[0][i] + tmpBuff[4][i];
    Ipp32s t4 = tmpBuff[0][i] - tmpBuff[4][i];
    Ipp32s t1 = tmpBuff[1][i] + tmpBuff[5][i];
    Ipp32s t5 = tmpBuff[1][i] - tmpBuff[5][i];
    Ipp32s t2 = tmpBuff[2][i] + tmpBuff[6][i];
    Ipp32s t6 = tmpBuff[2][i] - tmpBuff[6][i];
    Ipp32s t3 = tmpBuff[3][i] + tmpBuff[7][i];
    Ipp32s t7 = tmpBuff[3][i] - tmpBuff[7][i];
    Ipp32s s0 = t0 + t2;
    Ipp32s s2 = t0 - t2;
    Ipp32s s1 = t1 + t3;
    Ipp32s s3 = t1 - t3;
    Ipp32s s4 = t4 + t6;
    Ipp32s s6 = t4 - t6;
    Ipp32s s5 = t5 + t7;
    Ipp32s s7 = t5 - t7;
    satd += ABS(s0 + s1);
    satd += ABS(s0 - s1);
    satd += ABS(s2 + s3);
    satd += ABS(s2 - s3);
    satd += ABS(s4 + s5);
    satd += ABS(s4 - s5);
    satd += ABS(s6 + s7);
    satd += ABS(s6 - s7);
  }
  return (satd >> 2);
}

Ipp32u Hadamard4x4_16_AbsSum(COEFFSTYPE *diffBuff)
{
  __ALIGN16 Ipp32s tmpBuff[4][4];
  Ipp32s b;
  Ipp32s a01, a23, b01, b23;
  Ipp32u sad = 0;

  for (b = 0; b < 4; b ++) {
    a01 = diffBuff[b*16+0] + diffBuff[b*16+1];
    a23 = diffBuff[b*16+2] + diffBuff[b*16+3];
    b01 = diffBuff[b*16+0] - diffBuff[b*16+1];
    b23 = diffBuff[b*16+2] - diffBuff[b*16+3];
    tmpBuff[b][0] = a01 + a23;
    tmpBuff[b][1] = a01 - a23;
    tmpBuff[b][2] = b01 - b23;
    tmpBuff[b][3] = b01 + b23;
  }
  for (b = 0; b < 4; b ++) {
    a01 = tmpBuff[0][b] + tmpBuff[1][b];
    a23 = tmpBuff[2][b] + tmpBuff[3][b];
    b01 = tmpBuff[0][b] - tmpBuff[1][b];
    b23 = tmpBuff[2][b] - tmpBuff[3][b];
    sad += ABS(a01 + a23) + ABS(a01 - a23) + ABS(b01 - b23) + ABS(b01 + b23);
  }
  return (sad >> 1); // ??? the shift ?
}

Ipp32u Hadamard4x4_AbsSum(COEFFSTYPE diffBuff[4][4])
{
  __ALIGN16 Ipp32s tmpBuff[4][4];
  Ipp32s b;
  Ipp32s a01, a23, b01, b23;
  Ipp32u sad = 0;

  for (b = 0; b < 4; b ++) {
    a01 = diffBuff[b][0] + diffBuff[b][1];
    a23 = diffBuff[b][2] + diffBuff[b][3];
    b01 = diffBuff[b][0] - diffBuff[b][1];
    b23 = diffBuff[b][2] - diffBuff[b][3];
    tmpBuff[b][0] = a01 + a23;
    tmpBuff[b][1] = a01 - a23;
    tmpBuff[b][2] = b01 - b23;
    tmpBuff[b][3] = b01 + b23;
  }
  for (b = 0; b < 4; b ++) {
    a01 = tmpBuff[0][b] + tmpBuff[1][b];
    a23 = tmpBuff[2][b] + tmpBuff[3][b];
    b01 = tmpBuff[0][b] - tmpBuff[1][b];
    b23 = tmpBuff[2][b] - tmpBuff[3][b];
    sad += ABS(a01 + a23) + ABS(a01 - a23) + ABS(b01 - b23) + ABS(b01 + b23);
  }
  return (sad >> 1); // ??? the shift ?
}



Ipp32u Hadamard2x4_AbsSum(COEFFSTYPE diffBuff[4][4])
{
  Ipp32s d0 = diffBuff[0][0];
  Ipp32s d1 = diffBuff[0][1];
  Ipp32s d2 = diffBuff[1][0];
  Ipp32s d3 = diffBuff[1][1];
  Ipp32s a0 = d0 + d2;
  Ipp32s a1 = d1 + d3;
  Ipp32s a2 = d0 - d2;
  Ipp32s a3 = d1 - d3;
  Ipp32u sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
  d0 = diffBuff[2][0];
  d1 = diffBuff[2][1];
  d2 = diffBuff[3][0];
  d3 = diffBuff[3][1];
  a0 = d0 + d2;
  a1 = d1 + d3;
  a2 = d0 - d2;
  a3 = d1 - d3;
  sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
  return sad; // no shift ???
}

Ipp32u Hadamard4x2_AbsSum(COEFFSTYPE diffBuff[4][4])
{
  Ipp32s d0 = diffBuff[0][0];
  Ipp32s d1 = diffBuff[0][1];
  Ipp32s d2 = diffBuff[1][0];
  Ipp32s d3 = diffBuff[1][1];
  Ipp32s a0 = d0 + d2;
  Ipp32s a1 = d1 + d3;
  Ipp32s a2 = d0 - d2;
  Ipp32s a3 = d1 - d3;
  Ipp32u sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
  d0 = diffBuff[0][2];
  d1 = diffBuff[0][3];
  d2 = diffBuff[1][2];
  d3 = diffBuff[1][3];
  a0 = d0 + d2;
  a1 = d1 + d3;
  a2 = d0 - d2;
  a3 = d1 - d3;
  sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
  return sad; // no shift ???
}

Ipp32u Hadamard2x2_AbsSum(COEFFSTYPE diffBuff[4][4])
{
  Ipp32u sad;
  Ipp32s d0 = diffBuff[0][0];
  Ipp32s d1 = diffBuff[0][1];
  Ipp32s d2 = diffBuff[1][0];
  Ipp32s d3 = diffBuff[1][1];
  sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3);
  return sad;
}


Ipp32u SVCBaseMode_MB_Decision(
    void* state,
    H264Slice_8u16s *curr_slice)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Ipp32u best_sad, baseModeSAD;
    H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
    Ipp32s uMB = cur_mb.uMB;

    Ipp32s uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
    PIXTYPE *pRef = core_enc->m_pReconstructFrame->m_pYPlane +
      core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
    PIXTYPE *pPredBuf;
    bool useTransformForIntraDecision = curr_slice->m_use_transform_for_intra_decision;
    curr_slice->m_use_transform_for_intra_decision = 0;

    //COEFFSTYPE *y_residual = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
    Ipp16s *pResDiff = cur_mb.m_pResPredDiff;
    Ipp16s *pResDiffC = cur_mb.m_pResPredDiffC;
    Ipp32s RefLater8x8PackFlag = pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo);
    bool check2TransformModes;

    cur_mb.LocalMacroblockInfo->cbp_bits =
    cur_mb.LocalMacroblockInfo->cbp_bits_chroma =
    cur_mb.LocalMacroblockInfo->cbp = 0;
    //cur_mb.LocalMacroblockInfo->intra_chroma_mode = 0;
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
    cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
    cur_mb.LocalMacroblockInfo->cost = MAX_SAD;
    cur_mb.m_uIntraCBP4x4 = 0xffff;
    cur_mb.m_uIntraCBP8x8 = 0xffff;

    Ipp32s ref_intra = IS_TRUEINTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype);

    Ipp32s tryResidualPrediction = 0;
    if (!ref_intra && pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo))
    {
      if (curr_slice->m_adaptive_residual_prediction_flag)
        tryResidualPrediction = 1;
      else if (curr_slice->m_default_residual_prediction_flag)
        tryResidualPrediction = 2;
    }

    check2TransformModes = false;
    if (core_enc->m_spatial_resolution_change)
    {
        RefLater8x8PackFlag = 0;
        check2TransformModes = core_enc->m_info.transform_8x8_mode_flag;
    }

    pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, (tryResidualPrediction ? 1 : 0));
    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, RefLater8x8PackFlag);

//    VM_ASSERT(!(curr_slice->m_slice_type == BPREDSLICE && curr_slice->m_use_intra_mbs_only == true)); ???

    if (!ref_intra) {

      if (curr_slice->m_slice_type == INTRASLICE)
        return MAX_SAD;

      H264MotionVector mv_f[16];
      H264MotionVector mv_b[16];
      truncateMVs(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, mv_f);
      if (curr_slice->m_slice_type == BPREDSLICE)
        truncateMVs(state, curr_slice, cur_mb.MVs[LIST_1]->MotionVectors, mv_b);

      if ((core_enc->m_Analyse & (ANALYSE_RD_OPT | ANALYSE_RD_MODE)) && core_enc->m_PicParamSet->chroma_format_idc != 0) {
//        cur_mb.MacroblockCoeffsInfo->chromaNC = 0; // ???
        pPredBuf = cur_mb.mbChromaBaseMode.prediction;
        H264CoreEncoder_MCOneMBChroma_8u16s(state, curr_slice, mv_f, mv_b, pPredBuf);

        if (!core_enc->m_SliceHeader.MbaffFrameFlag && core_enc->m_spatial_resolution_change) {
            //RestoreIntraPrediction - copy intra predicted parts
            Ipp32s i,j;
            for (i=0; i<2; i++) {
              for (j=0; j<2; j++) {
                if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                Ipp32s offx = (core_enc->svc_ResizeMap[0][cur_mb.uMBx].border+1)>>1;
                Ipp32s offy = (core_enc->svc_ResizeMap[1][cur_mb.uMBy].border+1)>>1;
                IppiSize roiSize = {i?8-offx:offx, j?8-offy:offy};
                if(!i) offx = 0;
                if(!j) offy = 0;
                // NV12 !!
                Ipp32s x, y;
                PIXTYPE* ptr = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
                for (y=0; y<roiSize.height; y++) {
                  for (x=0; x<roiSize.width; x++) {
                    pPredBuf[(offy+y)*16 + offx+x] =
                      ptr[(offy+y)*cur_mb.mbPitchPixels + (offx+x) * 2];
                    pPredBuf[(offy+y)*16 + offx+x + 8] =
                      ptr[(offy+y)*cur_mb.mbPitchPixels + (offx+x) * 2 + 1];
                  }
                }
              }
            }
        }
      }

      pPredBuf = cur_mb.mbBaseMode.prediction;
      H264CoreEncoder_MCOneMBLuma_8u16s(state, curr_slice, mv_f, mv_b, pPredBuf);

      if (curr_slice->m_slice_type == BPREDSLICE)
          MC_bidirMB_luma( state, curr_slice, mv_f, mv_b, pPredBuf );

      if (!core_enc->m_SliceHeader.MbaffFrameFlag  && core_enc->m_spatial_resolution_change) {
          //RestoreIntraPrediction - copy intra predicted parts
          Ipp32s i,j;
          for (i=0; i<2; i++) {
            for (j=0; j<2; j++) {
              if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
              Ipp32s offx = core_enc->svc_ResizeMap[0][cur_mb.uMBx].border;
              Ipp32s offy = core_enc->svc_ResizeMap[1][cur_mb.uMBy].border;
              IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
              if(!i) offx = 0;
              if(!j) offy = 0;

              ippiCopy_8u_C1R(core_enc->m_pReconstructFrame->m_pYPlane +
                (cur_mb.uMBy*16 + offy) * core_enc->m_pReconstructFrame->m_pitchPixels + cur_mb.uMBx*16 + offx,
                core_enc->m_pReconstructFrame->m_pitchPixels,
                pPredBuf + offy*16 + offx,
                16,
                roiSize);
            }
          }
      }

    } else { // ref layer MB is intra

      //pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 0);

      if ((core_enc->m_Analyse & (ANALYSE_RD_OPT | ANALYSE_RD_MODE)) && core_enc->m_PicParamSet->chroma_format_idc != 0) {
#ifdef USE_NV12
        Ipp32s row, col;
        for (row = 0; row < 8; row++) {
          for (col = 0; col < 8; col++) {
            cur_mb.mbChromaBaseMode.prediction[row*16 + col] = core_enc->m_pReconstructFrame->m_pUPlane[uOffset + row*cur_mb.mbPitchPixels + col*2];
            cur_mb.mbChromaBaseMode.prediction[8 + row*16 + col] = core_enc->m_pReconstructFrame->m_pUPlane[1 + uOffset + row*cur_mb.mbPitchPixels + col*2];
          }
        }
#else
 // todo ???
#endif
      }
      if (core_enc->m_Analyse & (ANALYSE_RD_OPT | ANALYSE_RD_MODE))
        Copy16x16(pRef, cur_mb.mbPitchPixels, cur_mb.mbBaseMode.prediction, 16);
    }

    if (core_enc->m_Analyse & (ANALYSE_RD_OPT | ANALYSE_RD_MODE)) {
      H264BsBase *pBitstream;
      Ipp32u sad_nr = 0;
      H264MacroblockLocalInfo sLocalMBinfo = {0,}, *pLocalMBinfo = 0, sLocalMBinfo0;
      H264MacroblockGlobalInfo sGlobalMBinfo = {0,}, *pGlobalMBinfo = 0, sGlobalMBinfo0;

      baseModeSAD = 0;
      sad_nr = 0;

      pBitstream = curr_slice->m_pbitstream;
      if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        Ipp32u iQP = cur_mb.chromaQP;
        const Ipp32s l = lambda_sq[iQP];
        __ALIGN16 PIXTYPE pSrcSep[128];
        PIXTYPE *pSrc, *pUSrc, *pVSrc;

        // todo: have this made only once in the encoder !!! ???
        {
          pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
          pUSrc = pSrcSep;
          pVSrc = pSrcSep + 64;
          for (Ipp32s i = 0; i < 8; i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];
            pSrc += cur_mb.mbPitchPixels;
          }
        }

        H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);
        H264BsBase_CopyContextCABAC_Chroma(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field);
        curr_slice->m_pbitstream = (H264BsBase *)curr_slice->fakeBitstream;
//        cur_mb.LocalMacroblockInfo->cbp_bits_chroma = 0;
        //          H264CoreEncoder_Encode_MBBaseModeFlag_Fake_8u16s(state, curr_slice); ???

        if (tryResidualPrediction == 1) {
          sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
          sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;

          H264CoreEncoder_TransQuantChroma_RD_NV12_BaseMode_8u16s(state, curr_slice, pSrcSep, 1);

          baseModeSAD = SSD8x8(pUSrc, 8, cur_mb.mbChromaIntra.reconstruct, 16);
          baseModeSAD += SSD8x8(pVSrc, 8, cur_mb.mbChromaIntra.reconstruct+8, 16);

          H264CoreEncoder_Put_MBChroma_Fake_8u16s(state, curr_slice);
          baseModeSAD <<= 5;
          baseModeSAD += l*H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);

          pLocalMBinfo = cur_mb.LocalMacroblockInfo;
          pGlobalMBinfo = cur_mb.GlobalMacroblockInfo;
          cur_mb.LocalMacroblockInfo = &sLocalMBinfo;
          cur_mb.GlobalMacroblockInfo = &sGlobalMBinfo;
          pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, 0);

          H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);
          H264BsBase_CopyContextCABAC_Chroma(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field);
          curr_slice->m_pbitstream = (H264BsBase *)curr_slice->fakeBitstream;

          H264CoreEncoder_TransQuantChroma_RD_NV12_BaseMode_8u16s(state, curr_slice, pSrcSep, 0);

          sad_nr = SSD8x8(pUSrc, 8, cur_mb.mbChromaBaseMode.reconstruct, 16);
          sad_nr += SSD8x8(pVSrc, 8, cur_mb.mbChromaBaseMode.reconstruct+8, 16);

          H264CoreEncoder_Put_MBChroma_Fake_8u16s(state, curr_slice);
          sad_nr <<= 5;
          sad_nr += l*H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);

          cur_mb.LocalMacroblockInfo = pLocalMBinfo;
          cur_mb.GlobalMacroblockInfo = pGlobalMBinfo;


        } else {

          H264CoreEncoder_TransQuantChroma_RD_NV12_BaseMode_8u16s(state, curr_slice, pSrcSep, tryResidualPrediction);
          baseModeSAD = SSD8x8(pUSrc, 8, cur_mb.mbChromaBaseMode.reconstruct, 16);
          baseModeSAD += SSD8x8(pVSrc, 8, cur_mb.mbChromaBaseMode.reconstruct+8, 16);

          H264CoreEncoder_Put_MBChroma_Fake_8u16s(state, curr_slice);
          baseModeSAD <<= 5;
          baseModeSAD += l*H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
        }
//        curr_slice->m_pbitstream = pBitstream; // ???
      } else if (tryResidualPrediction == 1) {
        sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
        sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
        pSetMBResidualPredictionFlag(&sGlobalMBinfo, 0);
      }

      H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);

//      pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 0);

      H264BsBase_CopyContextCABAC(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, 0);

      curr_slice->m_pbitstream = (H264BsBase* )curr_slice->fakeBitstream;
//      cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
//      cur_mb.LocalMacroblockInfo->cbp = cur_mb.LocalMacroblockInfo->cbp_bits = 0;

      if (tryResidualPrediction == 1) {
        Ipp32u sad8x8 = 0, sad8x8_nr = 0;

        pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, 1);

        if (check2TransformModes) {
          sad8x8 = baseModeSAD;
          sad8x8_nr = sad_nr;
          sLocalMBinfo0 = *cur_mb.LocalMacroblockInfo;
          sGlobalMBinfo0 = *cur_mb.GlobalMacroblockInfo;
        }

        H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, 1);
        H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
        H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);

        Ipp32s bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
        Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mb4x4.reconstruct, 16)<<5;
        baseModeSAD += d + cur_mb.lambda * bs;

        if (check2TransformModes) {
          pLocalMBinfo = cur_mb.LocalMacroblockInfo;
          pGlobalMBinfo = cur_mb.GlobalMacroblockInfo;
          cur_mb.LocalMacroblockInfo = &sLocalMBinfo0;
          cur_mb.GlobalMacroblockInfo = &sGlobalMBinfo0;
          pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 1);
          pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);

          H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);
          H264BsBase_CopyContextCABAC(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, 1);
          curr_slice->m_pbitstream = (H264BsBase* )curr_slice->fakeBitstream;

          H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, 1);
          H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
          H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);

          bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
          d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mb8x8.reconstruct, 16)<<5;
          sad8x8 += d + cur_mb.lambda * bs;

          if (sad8x8 <= baseModeSAD) {
            *pLocalMBinfo = sLocalMBinfo0;
            *pGlobalMBinfo = sGlobalMBinfo0;
            baseModeSAD = sad8x8;
          }

          sLocalMBinfo0 = sLocalMBinfo;
          sGlobalMBinfo0 = sGlobalMBinfo;
          pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 1);
          pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);


          H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);
          H264BsBase_CopyContextCABAC(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, 1);
          curr_slice->m_pbitstream = (H264BsBase* )curr_slice->fakeBitstream;

          H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, 0);
          H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
          H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);

          bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
          d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mb8x8.reconstruct, 16)<<5;
          sad8x8_nr += d + cur_mb.lambda * bs;
          if (sad8x8_nr <= baseModeSAD) {
            *pLocalMBinfo = sLocalMBinfo0;
            *pGlobalMBinfo = sGlobalMBinfo0;
            baseModeSAD = sad8x8_nr;
          }
        }

        cur_mb.LocalMacroblockInfo = &sLocalMBinfo;
        cur_mb.GlobalMacroblockInfo = &sGlobalMBinfo;

        H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);
        H264BsBase_CopyContextCABAC(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, 0);
        curr_slice->m_pbitstream = (H264BsBase* )curr_slice->fakeBitstream;

        H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, 0);
        H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
        H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);

        bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
        d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbBaseMode.reconstruct, 16)<<5;
        sad_nr += d + cur_mb.lambda * bs;
        if (sad_nr <= baseModeSAD) {
          *pLocalMBinfo = sLocalMBinfo;
          *pGlobalMBinfo = sGlobalMBinfo;
          baseModeSAD = sad_nr;
        }

        curr_slice->m_pbitstream = pBitstream;
        cur_mb.LocalMacroblockInfo = pLocalMBinfo;
        cur_mb.GlobalMacroblockInfo = pGlobalMBinfo;
/*
        else {
          // ??? currently reconstruct and transform are not used - todo for performance!!! ???
          MFX_INTERNAL_CPY(cur_mb.mbBaseMode.reconstruct, cur_mb.mb4x4.reconstruct, 256*sizeof(PIXTYPE));
          MFX_INTERNAL_CPY(cur_mb.mbBaseMode.transform, cur_mb.mb4x4.transform, 256*sizeof(COEFFSTYPE));
          MFX_INTERNAL_CPY(cur_mb.mbChromaBaseMode.reconstruct, cur_mb.mbChromaIntra.reconstruct, 256*sizeof(PIXTYPE));
          MFX_INTERNAL_CPY(cur_mb.mbChromaBaseMode.transform, cur_mb.mbChromaIntra.transform, 256*sizeof(COEFFSTYPE));
          memset((COEFFSTYPE*)cur_mb.mbChromaBaseMode.transform, 1, 256*sizeof(COEFFSTYPE));
        }
*/

      } else { // tryResidualPrediction == 0 or 2

        if (check2TransformModes) {
          sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
          sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
        }

        H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, tryResidualPrediction);
        H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
        H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);

        {
            Ipp32s bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
            Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbBaseMode.reconstruct, 16) << 5;
            best_sad = d + cur_mb.lambda * bs;
            curr_slice->m_pbitstream = pBitstream;
        }

        if (check2TransformModes) {
//        if (ref_intra && (core_enc->m_Analyse & ANALYSE_I_8x8)) {

          pLocalMBinfo = cur_mb.LocalMacroblockInfo;
          pGlobalMBinfo = cur_mb.GlobalMacroblockInfo;
          cur_mb.LocalMacroblockInfo = &sLocalMBinfo;
          cur_mb.GlobalMacroblockInfo = &sGlobalMBinfo;

          H264BsFake_Reset_8u16s(curr_slice->fakeBitstream);

          pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 1);
          pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);

          H264BsBase_CopyContextCABAC(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, 1);

          curr_slice->m_pbitstream = (H264BsBase* )curr_slice->fakeBitstream;
//          cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
//          cur_mb.LocalMacroblockInfo->cbp = cur_mb.LocalMacroblockInfo->cbp_bits = 0;

          H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(state, curr_slice, ref_intra, tryResidualPrediction);
          H264CoreEncoder_Put_MBHeader_Fake_8u16s(state, curr_slice);
          H264CoreEncoder_Put_MBLuma_Fake_8u16s(state, curr_slice);
          Ipp32s bs = H264BsFake_GetBsOffset_8u16s(curr_slice->fakeBitstream);
          Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mb8x8.reconstruct, 16)<<5;
          d += cur_mb.lambda * bs;
          curr_slice->m_pbitstream = pBitstream;

          if (d <= (Ipp32s)best_sad) {
            // ??? currently reconstruct and transform are not used - todo for performance!!! ???
            //              MFX_INTERNAL_CPY(cur_mb.mbBaseMode.reconstruct, cur_mb.mb8x8.reconstruct, 256*sizeof(PIXTYPE));
            //              MFX_INTERNAL_CPY(cur_mb.mbBaseMode.transform, cur_mb.mb8x8.transform, 256*sizeof(COEFFSTYPE));

            if (ref_intra)
              cur_mb.m_uIntraCBPBaseMode = cur_mb.m_uIntraCBP8x8;

 //not used anyway ???
            for (Ipp32s i = 0; i < 4; i++) {
              cur_mb.m_iNumCoeffsBaseMode[ i ] = cur_mb.m_iNumCoeffs8x8[i];
              cur_mb.m_iLastCoeffBaseMode[ i ] = cur_mb.m_iLastCoeff8x8[i];
            }


            *pLocalMBinfo = sLocalMBinfo;
            *pGlobalMBinfo = sGlobalMBinfo;

            best_sad = d;
          }
          cur_mb.LocalMacroblockInfo = pLocalMBinfo;
          cur_mb.GlobalMacroblockInfo = pGlobalMBinfo;

        }
        baseModeSAD += best_sad;
      }

    } else {
      pPredBuf = (ref_intra ? pRef : cur_mb.mbBaseMode.prediction);
      Ipp32s step = (ref_intra ? cur_mb.mbPitchPixels : 16);
      Ipp32s trans8x8;
      Ipp16s *pRDQM = glob_RDQM[cur_mb.lumaQP51];
      Ipp32u sad4x4t, sad8x8t, sad8x8tr, sad;
      bool checkResidualOn = (tryResidualPrediction > 0);
      bool checkResidualOff = (tryResidualPrediction < 2);

      if (core_enc->m_Analyse & ANALYSE_SAD)
      {
          baseModeSAD = sad = MAX_SAD;
          if (checkResidualOff)
          {
             baseModeSAD = SAD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
          }

          if (checkResidualOn)
          {
              sad = SAD16x16_16_ResPred(pResDiff, pPredBuf);
          }

          if (core_enc->m_Analyse & ANALYSE_ME_CHROMA) {
              PIXTYPE *pCurU = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
              PIXTYPE *pCurV = core_enc->m_pCurrentFrame->m_pVPlane + uOffset;
              PIXTYPE *pRefU = core_enc->m_pReconstructFrame->m_pUPlane + uOffset;
              PIXTYPE *pRefV = core_enc->m_pReconstructFrame->m_pVPlane + uOffset;
              PIXTYPE *pPredU = ref_intra ? pRefU : cur_mb.mbChromaBaseMode.prediction;
              PIXTYPE *pPredV = ref_intra ? pRefV : cur_mb.mbChromaBaseMode.prediction + 8;

              if (checkResidualOff)
              {
                  baseModeSAD += SAD8x8(pCurU, cur_mb.mbPitchPixels, pPredU, step);
                  baseModeSAD += SAD8x8(pCurV, cur_mb.mbPitchPixels, pPredV, step);
              }

              if (checkResidualOn)
              {
                  sad += SAD8x8_16_ResPred(pResDiffC, cur_mb.mbChromaBaseMode.prediction);
                  sad += SAD8x8_16_ResPred(pResDiffC+8, cur_mb.mbChromaBaseMode.prediction+8);
              }
          }

          if (baseModeSAD < sad)
          {
              pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, 0);
              if (check2TransformModes)
              {
                  sad4x4t  = SATD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
                  sad8x8t  = SAT8x8D(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
                  sad8x8t += SAT8x8D(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, pPredBuf + 8, step);
                  sad8x8t += SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, pPredBuf + 8 * step, step);
                  sad8x8t += SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, pPredBuf + 8 * step + 8, step);

                  trans8x8 = 1;
                  if (sad4x4t < sad8x8t + (Ipp32s)BITS_COST(2, pRDQM))
                      trans8x8 = 0;
              }
              else
              {
                  trans8x8 = RefLater8x8PackFlag;
              }
          }
          else
          {
              baseModeSAD = sad;
              pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, 1);

              if (check2TransformModes)
              {
                  __ALIGN16 COEFFSTYPE diffBuffR[256];
                  Ipp32s i, j;

                  sad8x8t = sad4x4t = 0;

                  for (i = 0; i < 16; i++)
                      for (j = 0; j < 16; j++)
                          diffBuffR[i*16 + j] = pResDiff[i*16 + j] - pPredBuf[i*16 + j];

                  for (i = 0; i < 16; i += 8)
                      for (j = 0; j < 16; j += 8)
                          sad8x8t += Hadamard8x8_16_AbsSum(&(diffBuffR[i*16 + j]));

                  for (i = 0; i < 16; i += 4)
                      for (j = 0; j < 16; j += 4)
                          sad4x4t += Hadamard4x4_16_AbsSum(&(diffBuffR[i*16 + j]));

                  trans8x8 = 1;
                  if (sad4x4t < sad8x8t + (Ipp32s)BITS_COST(2, pRDQM))
                      trans8x8 = 0;
              }
              else
              {
                  trans8x8 = RefLater8x8PackFlag;
              }
          }
      }
      else
      {
          __ALIGN16 COEFFSTYPE diffBuffR[256];
          bool calc4x4 = false;
          bool calc8x8 = false;
          Ipp32u sadc, sadcr;
          Ipp32s i, j;

          if (check2TransformModes)    calc4x4 = calc8x8 = true;
          else
          {
              if (RefLater8x8PackFlag) calc8x8 = true;
              else                     calc4x4 = true;
          }

          if (checkResidualOn)
          {
              for (i = 0; i < 16; i++)
                  for (j = 0; j < 16; j++)
                      diffBuffR[i*16 + j] = pResDiff[i*16 + j] - pPredBuf[i*16 + j];
          }

          sadc = sadcr = 0;

          if (core_enc->m_Analyse & ANALYSE_ME_CHROMA)
          {
              PIXTYPE *pCurU = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
              PIXTYPE *pCurV = core_enc->m_pCurrentFrame->m_pVPlane + uOffset;
              PIXTYPE *pRefU = core_enc->m_pReconstructFrame->m_pUPlane + uOffset;
              PIXTYPE *pRefV = core_enc->m_pReconstructFrame->m_pVPlane + uOffset;
              PIXTYPE *pPredU = ref_intra ? pRefU : cur_mb.mbChromaBaseMode.prediction;
              PIXTYPE *pPredV = ref_intra ? pRefV : cur_mb.mbChromaBaseMode.prediction + 8;

              if (checkResidualOff)
              {
                  sadc  = SATD8x8(pCurU, cur_mb.mbPitchPixels, pPredU, step);
                  sadc += SATD8x8(pCurV, cur_mb.mbPitchPixels, pPredV, step);
              }


              if (checkResidualOn)
              {
                  sadcr = SATD8x8_16_ResPred(pResDiffC, cur_mb.mbChromaBaseMode.prediction);
                  sadcr += SATD8x8_16_ResPred(pResDiffC+8, cur_mb.mbChromaBaseMode.prediction+8);
              }
          }

          baseModeSAD = sad = sad8x8t = sad8x8tr = MAX_SAD;

          if (calc4x4)
          {
              if (checkResidualOff)
              {
                  baseModeSAD= SATD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
                  baseModeSAD += sadc;
              }

              if (checkResidualOn)
              {
                  sad = 0;
                  for (i = 0; i < 16; i += 4)
                      for (j = 0; j < 16; j += 4)
                          sad += Hadamard4x4_16_AbsSum(&(diffBuffR[i*16 + j]));

                  sad += sadcr;
              }
          }

          if (calc8x8)
          {
              if (checkResidualOff)
              {
                  sad8x8t = SAT8x8D(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
                  sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, pPredBuf + 8, step);
                  sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, pPredBuf + 8 * step, step);
                  sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, pPredBuf + 8 * step + 8, step);
                  sad8x8t += sadc;
              }

              if (checkResidualOn)
              {
                  sad8x8tr = sadcr;
                  for (i = 0; i < 16; i += 8)
                      for (j = 0; j < 16; j += 8)
                          sad8x8tr += Hadamard8x8_16_AbsSum(&(diffBuffR[i*16 + j]));
              }
          }

          Ipp32s rpred = 0, rpred8x8 = 0;

          if (sad8x8tr <= sad8x8t)
          {
              rpred8x8 = 1;
              sad8x8t = sad8x8tr;
          }

          if (sad <= baseModeSAD) {
              baseModeSAD = sad;
              rpred = 1;
          }

          trans8x8 = 1;
          if (baseModeSAD < sad8x8t + (Ipp32s)BITS_COST(2, pRDQM)) {
              trans8x8 = 0;
          } else {
              baseModeSAD = sad8x8t + (Ipp32s)BITS_COST(2, pRDQM); // ???
              rpred = rpred8x8;
          }

          pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, rpred);
      }

      pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, trans8x8);

    }
    cur_mb.LocalMacroblockInfo->cost = baseModeSAD;

    curr_slice->m_use_transform_for_intra_decision = useTransformForIntraDecision;
    return baseModeSAD;
}

static void SVC_ResPredDiff(void* state, H264Slice_8u16s *curr_slice)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
    {
        Ipp32s uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
        PIXTYPE *pCurY = cur_mb.mbPtr;
        PIXTYPE *pCurU = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        PIXTYPE *pCurV = core_enc->m_pCurrentFrame->m_pVPlane + uOffset;
        Ipp16s *pResY;
        Ipp16s *pResU;
        Ipp16s *pResV;
        Ipp32s resPitchPixels = core_enc->m_WidthInMBs * 16;
        Ipp32s i, j;

        if (core_enc->m_spatial_resolution_change)
        {
            if (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) {
                pResY = core_enc->m_pYInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
                pResU = core_enc->m_pUInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
                pResV = core_enc->m_pVInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            } else {
                pResY = core_enc->m_pYInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
                pResU = core_enc->m_pUInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
                pResV = core_enc->m_pVInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            }
        }
        else
        {
            pResY = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
            pResU = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pResV = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
        }

        for (i = 0; i < 16; i++) {
            for (j = 0; j < 16; j++) {
                cur_mb.m_pResPredDiff[i*16 + j] = pCurY[j] - pResY[j];
            }
            pCurY += cur_mb.mbPitchPixels;
            pResY += resPitchPixels;
        }
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                cur_mb.m_pResPredDiffC[i*16 + j] = pCurU[2*j] - pResU[j];
                cur_mb.m_pResPredDiffC[i*16 + j + 8] = pCurV[2*j] - pResV[j];
            }
            pCurU += cur_mb.mbPitchPixels;
            pResU += resPitchPixels >> 1;
            pCurV += cur_mb.mbPitchPixels;
            pResV += resPitchPixels >> 1;
        }

    }
}

Ipp32u SVC_MB_Decision(void* state,
                       H264Slice_8u16s *curr_slice)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
    Ipp32s tryBaseMode = 0;
    Ipp32s useResPred = 0;

    if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        pGetMBCropFlag(cur_mb.GlobalMacroblockInfo))
    {
        if (curr_slice->m_adaptive_prediction_flag)
            tryBaseMode = 1;
        else if (curr_slice->m_default_base_mode_flag)
            tryBaseMode = 2;

        useResPred = curr_slice->m_default_residual_prediction_flag | curr_slice->m_adaptive_residual_prediction_flag;
        pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, curr_slice->m_default_base_mode_flag | curr_slice->m_adaptive_prediction_flag); // kta: do not need it, actually
        pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, useResPred);

        if (curr_slice->m_default_motion_prediction_flag == 0 || IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype))
        {
            cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
            cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;
        }
        else
        {
            cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0xffff;
            cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0xffff;
        }

        if (core_enc->recodeFlag == UMC::BRC_RECODE_NONE) {
            MFX_INTERNAL_CPY(&cur_mb.LocalMacroblockInfo->m_RefLayerMVs[0], cur_mb.MVs[0], sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(&cur_mb.LocalMacroblockInfo->m_RefLayerMVs[1], cur_mb.MVs[1], sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(&cur_mb.LocalMacroblockInfo->m_RefLayerRefIdxs[0], cur_mb.RefIdxs[0], sizeof(H264MacroblockRefIdxs));
            MFX_INTERNAL_CPY(&cur_mb.LocalMacroblockInfo->m_RefLayerRefIdxs[1], cur_mb.RefIdxs[1], sizeof(H264MacroblockRefIdxs));
        }
    } else {
        pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 0);
        pSetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo, 0);
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;
    }

    if (useResPred && curr_slice->m_slice_type != INTRASLICE)
        SVC_ResPredDiff(state, curr_slice);

    if (tryBaseMode) {
        if (BPREDSLICE == curr_slice->m_slice_type)
            if (tryBaseMode == 2)
                SVC_CheckMBType_BaseMode(&cur_mb); // don't change tryBaseMode from 2
            else
                tryBaseMode = SVC_CheckMBType_BaseMode(&cur_mb);
        else if (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED) { // P-slice
            if (cur_mb.RefIdxs[0]->RefIdxs[0] >= 0)
                cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER;
            else {
                tryBaseMode = 0;
                pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 0);
            }
        }
    }

    if (tryBaseMode == 0) {
        H264CoreEncoder_MB_Decision_8u16s(state, curr_slice);
    } else if (tryBaseMode == 1) {
        H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
        H264MacroblockLocalInfo sLocalMBinfo, *pLocalMBinfo;
        H264MacroblockGlobalInfo sGlobalMBinfo, *pGlobalMBinfo;
        H264MacroblockMVs bmMVs[4];         //MV L0,L1, MVDeltas 0,1
        H264MacroblockRefIdxs bmRefIdxs[2]; //RefIdx L0, L1
        Ipp32s i;

        sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
        sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;

        pGlobalMBinfo = cur_mb.GlobalMacroblockInfo;
        pLocalMBinfo = cur_mb.LocalMacroblockInfo;
        cur_mb.GlobalMacroblockInfo = &sGlobalMBinfo;
        cur_mb.LocalMacroblockInfo = &sLocalMBinfo;

        pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 1);
        SVCBaseMode_MB_Decision(state, curr_slice);
        for (i = 0; i < 4; i++)
            MFX_INTERNAL_CPY(&(bmMVs[i]), cur_mb.MVs[i], sizeof(H264MacroblockMVs));
        for (i = 0; i < 2; i++)
            MFX_INTERNAL_CPY(&(bmRefIdxs[i]), cur_mb.RefIdxs[i], sizeof(H264MacroblockRefIdxs));

        cur_mb.GlobalMacroblockInfo = pGlobalMBinfo;
        cur_mb.LocalMacroblockInfo = pLocalMBinfo;

        pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 0);
        // need to recompute ResPred for disabled BaseMode
        if (useResPred && curr_slice->m_slice_type != INTRASLICE)
            SVC_ResPredDiff(state, curr_slice);

        H264CoreEncoder_MB_Decision_8u16s(state, curr_slice);
        if (cur_mb.LocalMacroblockInfo->cost >= sLocalMBinfo.cost) {
            if (!core_enc->m_spatial_resolution_change && curr_slice->m_tcoeff_level_prediction_flag &&
                (cur_mb.GlobalMacroblockInfo->mbtype != sGlobalMBinfo.mbtype ||
                 cur_mb.LocalMacroblockInfo->intra_chroma_mode != sLocalMBinfo.intra_chroma_mode ||
                (sGlobalMBinfo.mbtype == MBTYPE_INTRA_16x16 &&
                cur_mb.LocalMacroblockInfo->intra_16x16_mode != sLocalMBinfo.intra_16x16_mode) ||
                (sGlobalMBinfo.mbtype == MBTYPE_INTRA &&
                memcmp(curr_slice->m_cur_mb.intra_types_save, cur_mb.intra_types, 16*sizeof(T_AIMode)))))
            {
                pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 0);
            } else {
                *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
                *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;

                for (i = 0; i < 4; i++)
                    MFX_INTERNAL_CPY(cur_mb.MVs[i], &(bmMVs[i]), sizeof(H264MacroblockMVs));
                for (i = 0; i < 2; i++)
                    MFX_INTERNAL_CPY(cur_mb.RefIdxs[i], &(bmRefIdxs[i]), sizeof(H264MacroblockRefIdxs));
            }
        } else {
            pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 0);
        }
    } else {
        pSetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo, 1);

        if ((core_enc->m_Analyse & (ANALYSE_RD_OPT | ANALYSE_RD_MODE)) || core_enc->m_info.transform_8x8_mode_flag) // && (pSetMBResidualPredictionFlag || IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype))
            // otherwise there's currently no use in calling this: the reconstruct/transform are not re-used anyway
            SVCBaseMode_MB_Decision(state, curr_slice);
    }

    if (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) {

        MBDissection_8u16s mbTmp = cur_mb.mbChromaBaseMode;
        cur_mb.mbChromaBaseMode = cur_mb.mbChromaIntra;
        cur_mb.mbChromaIntra = mbTmp;
        /*
        MFX_INTERNAL_CPY( cur_mb.mbChromaIntra.prediction, cur_mb.mbChromaBaseMode.prediction, 256);
        MFX_INTERNAL_CPY( cur_mb.mbChromaIntra.reconstruct, cur_mb.mbChromaBaseMode.reconstruct, 256);
        MFX_INTERNAL_CPY( cur_mb.mbChromaIntra.transform, cur_mb.mbChromaBaseMode.transform, 512);
        */
        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
            mbTmp = cur_mb.mb8x8;
            cur_mb.mb8x8 = cur_mb.mbBaseMode;
            cur_mb.mbBaseMode = mbTmp;
            cur_mb.m_uIntraCBP8x8 = cur_mb.m_uIntraCBPBaseMode;

            // not used anyway ???
            for (Ipp32s i = 0; i < 4; i++) {
                cur_mb.m_iNumCoeffs8x8[i] = cur_mb.m_iNumCoeffsBaseMode[ i ];
                cur_mb.m_iLastCoeff8x8[i] = cur_mb.m_iLastCoeffBaseMode[ i ];
            }


        } else {
            mbTmp = cur_mb.mb4x4;
            cur_mb.mb4x4 = cur_mb.mbBaseMode;
            cur_mb.mbBaseMode = mbTmp;

            // not used anyway ???
            /*
            for (Ipp32s i = 0; i < 16; i++) {
            cur_mb.m_iNumCoeffs4x4[i] = cur_mb.m_iNumCoeffsBaseMode[ i ];
            cur_mb.m_iLastCoeff4x4[i] = cur_mb.m_iLastCoeffBaseMode[ i ];
            }
            */
        }
    }
    else {
        pSetMBIBLFlag(cur_mb.GlobalMacroblockInfo, 0);
    }


    /* RD tmp for testing */

    {
        if (core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
        {
        }
        else
        {
            if ((IS_INTER_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype)) && (!IS_SKIP_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype)))
            {
            }
            else
            {
                cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] =
                    cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;
            }

        }
    }

    return cur_mb.LocalMacroblockInfo->cost;
}


void ippiCountCoeffs(const Ipp16s *coeffs, Ipp32s *numCoeffs, const Ipp16s *enc_scan, Ipp32s *lastCoeff, Ipp32s  m);

Ipp32u H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(
  void* state,
  H264Slice_8u16s *curr_slice,
  Ipp32s intra,
  Ipp32s residualPrediction)
{
  H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
  Ipp32u  uBlock;     // block number, 0 to 23
  Ipp32u  uOffset;        // to upper left corner of block from start of plane
  Ipp32u  uMBQP;          // QP of current MB
  Ipp32u  uMB;
  Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
  Ipp32u  uIntraSAD;      // intra MB SAD
  PIXTYPE*  pPredBuf;       // prediction block pointer
  PIXTYPE*  pReconBuf;       // reconstruction block pointer
  Ipp16s*   pDiffBuf;       // difference block pointer
  COEFFSTYPE* pTransformResult; // Result of the transformation.
  PIXTYPE*  pSrcPlane;      // start of plane to encode
  Ipp32s    pitchPixels;     // buffer pitch
  Ipp8u     bCoded; // coded block flag
  Ipp32s    iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
  Ipp32s    iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
  Ipp32u    uTotalCoeffs = 0;    // Used to detect single expensive coeffs.
  COEFFSTYPE* pTransRes;

  if (intra)
    intra = 1; // just in case
  H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
  Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;

  COEFFSTYPE *y_residual;
  Ipp32s stepR = core_enc->m_WidthInMBs * 32; // step in bytes
  int ii, jj;
  Ipp16s *src1;

  if (!core_enc->m_spatial_resolution_change) // kla added '!' and -Base-
  {
      y_residual = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
  }
  else
  {
      y_residual = core_enc->m_pYInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
  }

  uMB = cur_mb.uMB;

  pitchPixels = cur_mb.mbPitchPixels;
  uCBPLuma     = cur_mb.LocalMacroblockInfo->cbp_luma;
  uMBQP       = cur_mb.lumaQP;
  pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
  pTransRes = (COEFFSTYPE*)(pDiffBuf + 64);
  uIntraSAD   = 0;

  //--------------------------------------------------------------------------
  // encode Y plane blocks (0-15)
  //--------------------------------------------------------------------------

  // initialize pointers and offset
  pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
  uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
  TrellisCabacState cbSt;

  if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
    Ipp32s coeffCost, mbCost = 0;

    if( core_enc->m_info.quant_opt_level > 1)
    {
      H264BsBase_CopyContextCABAC_Trellis8x8(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
    }

    for (uBlock = 0; uBlock < 4; uBlock++)
    {
      Ipp32s idxb, idx, idxe;

      idxb = uBlock<<2;
      idxe = idxb+4;
      pPredBuf = cur_mb.mbBaseMode.prediction + xoff[4*uBlock] + yoff[4*uBlock]*16;
      pReconBuf = cur_mb.mb8x8.reconstruct + xoff[4*uBlock] + yoff[4*uBlock]*16;
      pTransformResult = &cur_mb.mb8x8.transform[uBlock*64];

      if (core_enc->m_PicParamSet->entropy_coding_mode)
      {
        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // These will be updated if the block is coded
        cur_mb.cabac_data->lumaBlock8x8[uBlock].uNumSigCoeffs = 0;
      }
      else
      {
        for (idx = idxb; idx < idxe; idx++)
        {
          curr_slice->Block_RLE[idx].uNumCoeffs = 0;
          curr_slice->Block_RLE[idx].uTrailing_Ones = 0;
          curr_slice->Block_RLE[idx].uTrailing_One_Signs = 0;
          curr_slice->Block_RLE[idx].uTotalZeros = 16;
          cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = 0;
        }
      }

      {
        {
          Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

          if (residualPrediction)
            ippiSub8x8_16u16s_C1R((const Ipp16u *)pDiffBuf, 16, (const Ipp16u *)(y_residual + xoff[4*uBlock] + yoff[4*uBlock]*(stepR >> 1)), stepR, pDiffBuf, 16);

          // forward transform and quantization, in place in pDiffBuf
          ippiTransformLuma8x8Fwd_H264_8u16s(pDiffBuf, pTransformResult);
          if (core_enc->m_info.quant_opt_level > 1)
          {
            ippiQuantLuma8x8_H264_8u16s(
              pTransformResult,
              pTransformResult,
              uMBQP,
              intra,
              enc_single_scan_8x8[is_cur_mb_field],
              core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1-intra][QP_MOD_6[uMBQP]],
              &iNumCoeffs,
              &iLastCoeff,
              curr_slice,
              &cbSt,
              core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1-intra][QP_MOD_6[uMBQP]]);
          }
          else
          {
            ippiQuantLuma8x8_H264_8u16s(
              pTransformResult,
              pTransformResult,
              QP_DIV_6[uMBQP],
              intra,
              enc_single_scan_8x8[is_cur_mb_field],
              core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1-intra][QP_MOD_6[uMBQP]],
              &iNumCoeffs,
              &iLastCoeff,
              NULL,
              NULL,
              NULL);
          }
        }

        if (!intra) { // ???
          coeffCost = iNumCoeffs
            ? CalculateCoeffsCost_8u16s(pTransformResult, 64, dec_single_scan_8x8[is_cur_mb_field])
            : 0;
          mbCost += coeffCost;
        }

        // if everything quantized to zero, skip RLE
        if (!iNumCoeffs)
        {
          // the block is empty so it is not coded
          bCoded = 0;
        }
        else
        {
          uTotalCoeffs += ((iNumCoeffs < 0) ? -(iNumCoeffs*2) : iNumCoeffs);

          // record RLE info
          if (core_enc->m_PicParamSet->entropy_coding_mode)
          {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

            CabacBlock8x8* c_data = &cur_mb.cabac_data->lumaBlock8x8[uBlock];
            c_data->uLastSignificant = iLastCoeff;
            c_data->CtxBlockCat = BLOCK_LUMA_64_LEVELS;
            c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
            c_data->uFirstCoeff = 0;
            c_data->uLastCoeff = 63;
            H264CoreEncoder_MakeSignificantLists_CABAC_8u16s(
              pTransformResult,
              dec_single_scan_8x8[is_cur_mb_field],
              c_data);

            bCoded = c_data->uNumSigCoeffs;
          }
          else
          {
            COEFFSTYPE buf4x4[4][16];
            Ipp32s i4x4;

            //Reorder 8x8 block for coding with CAVLC
            for(i4x4=0; i4x4<4; i4x4++ ) {
              Ipp32s i;
              for(i = 0; i<16; i++ )
                buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] =
                pTransformResult[dec_single_scan_8x8[is_cur_mb_field][4*i+i4x4]];
            }

            bCoded = 0;
            //Encode each block with CAVLC 4x4
            for(i4x4 = 0; i4x4<4; i4x4++ ) {
              Ipp32s i;
              iLastCoeff = 0;
              idx = idxb + i4x4;

              //Check for last coeff
              for(i = 0; i<16; i++ ) if( buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] != 0 ) iLastCoeff=i;

              ippiEncodeCoeffsCAVLC_H264_8u16s(
                buf4x4[i4x4],
                0, //Luma
                dec_single_scan[is_cur_mb_field],
                iLastCoeff,
                &curr_slice->Block_RLE[idx].uTrailing_Ones,
                &curr_slice->Block_RLE[idx].uTrailing_One_Signs,
                &curr_slice->Block_RLE[idx].uNumCoeffs,
                &curr_slice->Block_RLE[idx].uTotalZeros,
                curr_slice->Block_RLE[idx].iLevels,
                curr_slice->Block_RLE[idx].uRuns);

              bCoded += curr_slice->Block_RLE[idx].uNumCoeffs;
              cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = curr_slice->Block_RLE[idx].uNumCoeffs;
            }
          }
        }

        // update flags if block quantized to empty
        if (!bCoded){
          uCBPLuma &= ~CBP8x8Mask[uBlock];
          // update reconstruct frame with prediction
          if (residualPrediction) {
            src1 = y_residual + xoff[4*uBlock] + yoff[4*uBlock]*(stepR >> 1);
            for (ii = 0; ii < 8; ii++) {
              for (jj = 0; jj < 8; jj++) {
                Ipp16s tmp = src1[jj] + pPredBuf[jj];
                tmp = IPP_MIN(tmp, 255);
                tmp = IPP_MAX(tmp, 0);
                pReconBuf[jj] = tmp;
              }
              pReconBuf += 16;
              pPredBuf += 16;
              src1 += stepR >> 1;
            }
          } else
            Copy8x8(pPredBuf, 16, pReconBuf, 16);
        } else {
          if (residualPrediction) {
            ippiDequantResidual8x8_H264_16s(pTransformResult, pTransRes, uMBQP, core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
            ippiTransformResidualAndAdd8x8_H264_16s16s_C1I(y_residual + xoff[uBlock] + yoff[uBlock]*(stepR >> 1), pTransRes, pTransRes, stepR, 16);

            /*
            Ipp16s transRes[64];
            MFX_INTERNAL_CPY(transRes, pTransformResult, 16*sizeof( COEFFSTYPE ));
            ippiDequantTransformResidual_H264_16s_C1I(transRes, 8, NULL, ((iaNumCoeffs[uBlock] < -1) || (iaNumCoeffs[uBlock] > 0)), uMBQP);
            */

            Ipp16s *src1 = pTransRes;
            for (ii = 0; ii < 8; ii++) {
              for (jj = 0; jj < 8; jj++) {
                Ipp16s tmp = IPP_MAX(*src1, -255);
                tmp = IPP_MIN(tmp, 255);
                tmp += pPredBuf[jj];
                src1++;
                tmp = IPP_MIN(tmp, 255);
                tmp = IPP_MAX(tmp, 0);
                pReconBuf[jj] = tmp;
              }
              pReconBuf += 16;
              pPredBuf += 16;
            }
          } else {
            MFX_INTERNAL_CPY(pTransRes, pTransformResult, 64*sizeof( COEFFSTYPE ));
            ippiQuantLuma8x8Inv_H264_8u16s(pTransRes, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
            ippiTransformLuma8x8InvAddPred_H264_8u16s(pPredBuf, 16, pTransRes, pReconBuf, 16, core_enc->m_PicParamSet->bit_depth_luma);
          }
        }

        cur_mb.m_iNumCoeffs8x8[ uBlock ] = iNumCoeffs;
        cur_mb.m_iLastCoeff8x8[ uBlock ] = iLastCoeff;

      } //curr_slice->m_use_transform_for_intra_decision
      uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
    }  // for uBlock in luma plane

    if (!intra && mbCost < LUMA_COEFF_MB_8X8_MAX_COST)
      uCBPLuma = 0;
    if (intra)
      cur_mb.m_uIntraCBP8x8 = uCBPLuma;

  } else { // 4x4

    Ipp32s iaNumCoeffs[16], CoeffsCost[16] = {9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9};
    PIXTYPE *pRecon = (residualPrediction == 1 ? cur_mb.mb4x4.reconstruct : cur_mb.mbBaseMode.reconstruct);
    COEFFSTYPE *pTransform = (residualPrediction == 1 ? cur_mb.mb4x4.transform : cur_mb.mbBaseMode.transform);

    if (core_enc->m_info.quant_opt_level > 1)
    {
      H264BsBase_CopyContextCABAC_Trellis4x4(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
    }

    for (uBlock = 0; uBlock < 16; uBlock++) {
      pPredBuf = cur_mb.mbBaseMode.prediction + xoff[uBlock] + yoff[uBlock]*16;
      pTransformResult = &pTransform[uBlock*16];

      cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0; // These will be updated if the block is coded
      if (core_enc->m_PicParamSet->entropy_coding_mode) {
        cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
      } else {
        curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
        curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
        curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
        curr_slice->Block_RLE[uBlock].uTotalZeros = 16;
      }

      Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

      if (residualPrediction) {
        // !!! won't work if COEFFSTYPE != Ipp16s
        ippiSub4x4_16u16s_C1R((const Ipp16u *)pDiffBuf, 8, (const Ipp16u *)(y_residual + xoff[uBlock] + yoff[uBlock]*(stepR >> 1)), stepR, pDiffBuf, 8);
//        Diff4x4Residual(y_residual, core_enc->m_WidthInMBs * 16, pDiffBuf, 4, pDiffBuf, 4);
      }

      if( core_enc->m_info.quant_opt_level > 1 ) {
        ippiTransformQuantResidual_H264_8u16s(
          pDiffBuf,
          pTransformResult,
          uMBQP,
          &iaNumCoeffs[uBlock],
          intra,
          enc_single_scan[is_cur_mb_field],
          &iLastCoeff,
          NULL,
          curr_slice,
          0,
          &cbSt);
      }else{
        ippiTransformQuantResidual_H264_8u16s(
          pDiffBuf,
          pTransformResult,
          uMBQP,
          &iaNumCoeffs[uBlock],
          intra,
          enc_single_scan[is_cur_mb_field],
          &iLastCoeff,
          NULL,
          NULL,
          0,
          NULL);
      }

      if (!intra) { //???
        CoeffsCost[uBlock] = iaNumCoeffs[uBlock]
        ? CalculateCoeffsCost_8u16s(pTransformResult, 16, dec_single_scan[is_cur_mb_field])
          : 0;
      }
      // ??? need this
      cur_mb.m_iNumCoeffsBaseMode[ uBlock ] = iaNumCoeffs[uBlock];
      cur_mb.m_iLastCoeffBaseMode[ uBlock ] = iLastCoeff;

      // if everything quantized to zero, skip RLE
      if (!iaNumCoeffs[uBlock]) {
          uCBPLuma &= ~CBP4x4Mask[uBlock];
      } else {
        // Preserve the absolute number of coeffs.
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
          cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);

          CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
          c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
          c_data->uLastSignificant = iLastCoeff;
          c_data->CtxBlockCat = BLOCK_LUMA_LEVELS;
          c_data->uFirstCoeff = 0;
          c_data->uLastCoeff = 15;
          H264CoreEncoder_MakeSignificantLists_CABAC_8u16s(
            pTransformResult,
            dec_single_scan[is_cur_mb_field],
            c_data);

//            bCoded = c_data->uNumSigCoeffs;
        }
        else
        {
          // record RLE info
          cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);
          ippiEncodeCoeffsCAVLC_H264_8u16s(
            pTransformResult,
            0,
            dec_single_scan[is_cur_mb_field],
            iLastCoeff,
            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
            &curr_slice->Block_RLE[uBlock].uTotalZeros,
            curr_slice->Block_RLE[uBlock].iLevels,
            curr_slice->Block_RLE[uBlock].uRuns);
          cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = bCoded = curr_slice->Block_RLE[uBlock].uNumCoeffs;
        }
      }
      uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
    }


    //Skip subblock 8x8 if it cost is < 4 or skip MB if it's cost is < 5
    if (!intra) {
      Ipp32s mbCost=0;
      for( uBlock = 0; uBlock < 4; uBlock++ ){
        Ipp32s sb = uBlock*4;
        Ipp32s block8x8cost = CoeffsCost[sb] + CoeffsCost[sb+1] + CoeffsCost[sb+2] + CoeffsCost[sb+3];

        mbCost += block8x8cost;
        if (block8x8cost <= LUMA_8X8_MAX_COST && core_enc->m_info.quant_opt_level < OPT_QUANT_INTER_RD+1)
          uCBPLuma &= ~CBP8x8Mask[uBlock];
      }
      if( mbCost <= LUMA_MB_MAX_COST )
        uCBPLuma = 0;
    }

    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    for (uBlock=0; uBlock < 16; uBlock++) {
      pPredBuf = cur_mb.mbBaseMode.prediction + xoff[uBlock] + yoff[uBlock]*16;
      pReconBuf = pRecon + xoff[uBlock] + yoff[uBlock]*16;
      pTransformResult = &pTransform[uBlock*16]; // ???

      bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

      if (!bCoded){
        if (residualPrediction) {
          src1 = y_residual + xoff[uBlock] + yoff[uBlock]*(stepR >> 1);
          for (ii = 0; ii < 4; ii++) {
            for (jj = 0; jj < 4; jj++) {
              Ipp16s tmp = src1[jj] + pPredBuf[jj];
              tmp = IPP_MIN(tmp, 255);
              tmp = IPP_MAX(tmp, 0);
              pReconBuf[jj] = tmp;
            }
            pReconBuf += 16;
            pPredBuf += 16;
            src1 += stepR >> 1;
          }
        } else
          Copy4x4(pPredBuf, 16, pReconBuf, 16);

        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
        // need this ???
        cur_mb.m_iNumCoeffsBaseMode[ uBlock ] = 0;
        cur_mb.m_iLastCoeffBaseMode[ uBlock ] = 0;
      } else {


        if (residualPrediction) {
          ippiDequantResidual4x4_H264_16s(pTransformResult, pTransRes, uMBQP);
          ippiTransformResidualAndAdd_H264_16s16s_C1I(y_residual + xoff[uBlock] + yoff[uBlock]*(stepR >> 1), pTransRes, pTransRes, stepR, 8);

/*
          Ipp16s transRes[64];
          MFX_INTERNAL_CPY(transRes, pTransformResult, 16*sizeof( COEFFSTYPE ));
          ippiDequantTransformResidual_H264_16s_C1I(transRes, 8, NULL, ((iaNumCoeffs[uBlock] < -1) || (iaNumCoeffs[uBlock] > 0)), uMBQP);
*/

          Ipp16s *src1 = pTransRes;
          for (ii = 0; ii < 4; ii++) {
            for (jj = 0; jj < 4; jj++) {
              Ipp16s tmp = IPP_MAX(*src1, -255);
              tmp = IPP_MIN(tmp, 255);
              tmp += pPredBuf[jj];
              src1++;
              tmp = IPP_MIN(tmp, 255);
              tmp = IPP_MAX(tmp, 0);
              pReconBuf[jj] = tmp;
            }
            pReconBuf += 16;
            pPredBuf += 16;
          }
        } else {
          MFX_INTERNAL_CPY(pTransRes, pTransformResult, 16*sizeof( COEFFSTYPE ));
          ippiDequantTransformResidualAndAdd_H264_8u16s(
            pPredBuf,
            pTransRes,
            NULL,
            pReconBuf,
            16,
            16,
            uMBQP,
            ((iaNumCoeffs[uBlock] < -1) || (iaNumCoeffs[uBlock] > 0)),
            core_enc->m_PicParamSet->bit_depth_luma,
            NULL);
        }
      }

      // proceed to the next block
      uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
    }  // for uBlock in luma plane
  }
  cur_mb.m_uIntraCBPBaseMode = uCBPLuma;
  cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

  return 1;
}


void H264CoreEncoder_TransQuantChroma_RD_NV12_BaseMode_8u16s(
  void* state,
  H264Slice_8u16s *curr_slice,
  PIXTYPE *pSrcSep,  // separated U and V
  Ipp32s residualPrediction)
{
  H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
  Ipp32u  uBlock;         // block number, 0 to 23
  Ipp32u  uOffset;        // to upper left corner of block from start of plane
  Ipp32u  uMBQP;          // QP of current MB
  Ipp32u  uMB;
  PIXTYPE*  pSrcPlane;    // start of plane to encode
  Ipp32s    pitchPixels;  // buffer pitch
  COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
  PIXTYPE*  pPredBuf;     // prediction block pointer
  PIXTYPE*  pReconBuf;     // prediction block pointer
  PIXTYPE*  pPredBuf_copy;     // prediction block pointer
  COEFFSTYPE* pQBuf;      // quantized block pointer
  Ipp16s* pMassDiffBuf;   // difference block pointer

  Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
  Ipp32s   iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
  Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
  Ipp32s   RLE_Offset;    // Index into BlockRLE array

//  __ALIGN16 PIXTYPE pUSrc[64];
//  __ALIGN16 PIXTYPE pVSrc[64];
//  PIXTYPE * pSrc;

  PIXTYPE *pUSrc, *pVSrc;
  pUSrc = pSrcSep;
  pVSrc = pSrcSep + 64;

  H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
  Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
  EnumSliceType slice_type = curr_slice->m_slice_type;
  COEFFSTYPE *pTransformResult;
  COEFFSTYPE *pResidual;
  COEFFSTYPE *u_residual;
  COEFFSTYPE *v_residual;
  Ipp32s stepR =  core_enc->m_WidthInMBs * 8; // step in pels
  PIXTYPE *pRecon = (residualPrediction == 1 ? cur_mb.mbChromaIntra.reconstruct : cur_mb.mbChromaBaseMode.reconstruct);
//  COEFFSTYPE *pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
  COEFFSTYPE *pTransform = (residualPrediction == 1 ? cur_mb.mbChromaIntra.transform : cur_mb.mbChromaBaseMode.transform);
  Ipp32s ii, jj;
  Ipp16s *src1;

  if (!core_enc->m_spatial_resolution_change) // kla added '!' and -Base-
  {
      u_residual = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
      v_residual = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
  }
  else
  {
      u_residual = core_enc->m_pUInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
      v_residual = core_enc->m_pVInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
  }

  /* ??? !!!
write transforms to mbChromaBaseMode.transform: 2*64 AC + 2*4 DC? (256*2 bytes are allocated for that)
will need to keep this in mind when getting rid of chroma_idc>1 : need room for 128+8 coeffs(!!!)
*/

  pitchPixels = cur_mb.mbPitchPixels;
  uMBQP       = cur_mb.chromaQP;
  pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
  pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
  pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
  Ipp16s*  pTempDiffBuf;
  uMB = cur_mb.uMB;

  // initialize pointers and offset
  uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
  //    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
  uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
  cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

  // initialize pointers for the U plane blocks
  Ipp32s num_blocks = 2<<core_enc->m_PicParamSet->chroma_format_idc;
  Ipp32s startBlock;
  startBlock = uBlock = 16;
  Ipp32u uLastBlock = uBlock+num_blocks;
  Ipp32u uFinalBlock = uBlock+2*num_blocks;

/*
  {
    pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
    for(Ipp32s i=0;i<8;i++){
      Ipp32s s = 8*i;
      pUSrc[s+0] = pSrc[0];
      pVSrc[s+0] = pSrc[1];
      pUSrc[s+1] = pSrc[2];
      pVSrc[s+1] = pSrc[3];
      pUSrc[s+2] = pSrc[4];
      pVSrc[s+2] = pSrc[5];
      pUSrc[s+3] = pSrc[6];
      pVSrc[s+3] = pSrc[7];
      pUSrc[s+4] = pSrc[8];
      pVSrc[s+4] = pSrc[9];
      pUSrc[s+5] = pSrc[10];
      pVSrc[s+5] = pSrc[11];
      pUSrc[s+6] = pSrc[12];
      pVSrc[s+6] = pSrc[13];
      pUSrc[s+7] = pSrc[14];
      pVSrc[s+7] = pSrc[15];
      pSrc += pitchPixels;
    }
  }
*/

  // encode first chroma U plane then V plane
  do {
    // Adjust the pPredBuf to point at the V plane predictor when appropriate:
    // (blocks and an INTRA Y plane mode...)
    if (uBlock == uLastBlock) {
      startBlock = uBlock;
      pSrcPlane = pVSrc;
      pPredBuf = cur_mb.mbChromaBaseMode.prediction+8;
      pReconBuf = pRecon+8;
      pResidual = v_residual;
      RLE_Offset = V_DC_RLE;
      uLastBlock += num_blocks;
    } else {
      RLE_Offset = U_DC_RLE;
      pSrcPlane = pUSrc;
      pPredBuf = cur_mb.mbChromaBaseMode.prediction;
      pReconBuf = pRecon;
      pResidual = u_residual;
    }
    pPredBuf_copy = pPredBuf;

    if (residualPrediction) {
      Ipp32s i, j, m, n;
      COEFFSTYPE *pResidual0 = pResidual;
      for (i = 0; i < 2; i ++) {
        COEFFSTYPE *pRes = pResidual0;
        for (j = 0; j < 2; j ++) {
          pDCBuf[i*2+j] = 0;
          for (m = 0; m < 4; m ++) {
            for (n = 0; n < 4; n ++) {
              Ipp32s temp = pSrcPlane[m * 8 + n] - pPredBuf[m * 16 + n] - pRes[n];
              pDCBuf[i*2+j] = (Ipp16s)(pDCBuf[i*2+j] + temp);
              pMassDiffBuf[i*32+j*16+m*4+n] = (Ipp16s)temp;
            }
            pRes += stepR;
          }
          pPredBuf += 4;
          pSrcPlane += 4;
          pRes = pResidual0 + 4;
        }
        pSrcPlane += 4 * 8 - 8;
        pPredBuf += 4 * 16 - 8;
        pResidual0 += stepR << 2;
      }
      pPredBuf = pPredBuf_copy;
    } else {
      ippiSumsDiff8x8Blocks4x4_8u16s(
      pSrcPlane,    // source pels
      8,                 // source pitch
      pPredBuf,               // predictor pels
      16,
      pDCBuf,                 // result buffer
      pMassDiffBuf);
    }
    ippiTransformQuantChromaDC_H264_8u16s(
      pDCBuf,
      pQBuf,
      uMBQP,
      &iNumCoeffs,
      (slice_type == INTRASLICE),
      1,
      NULL);

    // DC values in this block if iNonEmpty is 1.
    cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);

    if (core_enc->m_PicParamSet->entropy_coding_mode){
      Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
      H264CoreEncoder_ScanSignificant_CABAC_8u16s(
        pDCBuf,
        ctxIdxBlockCat,
        4,
        dec_single_scan_p,
        &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
    }else{
      ippiEncodeChromaDcCoeffsCAVLC_H264_8u16s(
        pDCBuf,
        &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
        &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
        &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
        &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
        curr_slice->Block_RLE[RLE_Offset].iLevels,
        curr_slice->Block_RLE[RLE_Offset].uRuns);
    }
    ippiTransformDequantChromaDC_H264_8u16s(pDCBuf, uMBQP, NULL);

    //Encode croma AC
    Ipp32s coeffsCost = 0;
    for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
      pPredBuf = pPredBuf_copy + (uBlock & 2)*32 + (uBlock & 1)*4; //!!!
      cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;     // This will be updated if the block is coded
      if (core_enc->m_PicParamSet->entropy_coding_mode)
      {
        cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
      }
      else
      {
        curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
        curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
        curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
        curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
      }
      pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
      pTransformResult = pTransform + (uBlock-startBlock)*16;

      ippiTransformQuantResidual_H264_8u16s(
        pTempDiffBuf,
        pTransformResult,
        uMBQP,
        &iNumCoeffs,
        0,
        enc_single_scan[is_cur_mb_field],
        &iLastCoeff,
        NULL,
        NULL,
        0,
        NULL);
      coeffsCost += iNumCoeffs
        ? CalculateCoeffsCost_8u16s(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
        : 0;

      // if everything quantized to zero, skip RLE
      cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
      if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
      {
        // the block is empty so it is not coded
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
          CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
          c_data->uLastSignificant = iLastCoeff;
          c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
          c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
          c_data->uFirstCoeff = 1;
          c_data->uLastCoeff = 15;
          H264CoreEncoder_MakeSignificantLists_CABAC_8u16s(
            pTransformResult,
            dec_single_scan[is_cur_mb_field],
            c_data);

          cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
        }
        else
        {
          ippiEncodeCoeffsCAVLC_H264_8u16s(pTransformResult,// pDiffBuf,
            1,
            dec_single_scan[is_cur_mb_field],
            iLastCoeff,
            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
            &curr_slice->Block_RLE[uBlock].uTotalZeros,
            curr_slice->Block_RLE[uBlock].iLevels,
            curr_slice->Block_RLE[uBlock].uRuns);

          cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
        }
      }
    }

    if (coeffsCost <= CHROMA_COEFF_MAX_COST)
    {
      //Reset all ac coeffs
      for(uBlock = startBlock; uBlock < uLastBlock; uBlock++)
      {
        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
          cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
        }
        else
        {
          curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
          curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
          curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
          curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
        }
      }
    }


//    pPredBuf = pPredBuf_copy;
    Ipp8u *pReconBuf_copy = pReconBuf;
    for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
      pPredBuf = pPredBuf_copy + (uBlock & 2)*32 + (uBlock & 1)*4; //!!!
      pReconBuf = pReconBuf_copy + (uBlock & 2)*32 + (uBlock & 1)*4; //!!!
      iNumCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
      cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*iNumCoeffs;
      pTransformResult = pTransform + (uBlock-startBlock)*16;

      if (!iNumCoeffs && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ]){
        uCBPChroma &= ~CBP4x4Mask[uBlock-16];
        if (residualPrediction) {
          src1 = pResidual + (uBlock & 1)*4 + ((uBlock & 2) >> 1)*stepR;
          for (ii = 0; ii < 4; ii++) {
            for (jj = 0; jj < 4; jj++) {
              Ipp16s tmp = src1[jj] + pPredBuf[jj];
              tmp = IPP_MIN(tmp, 255);
              tmp = IPP_MAX(tmp, 0);
              pReconBuf[jj] = (PIXTYPE)tmp;
            }
            pReconBuf += 16;
            pPredBuf += 16;
            src1 += stepR;
          }
        } else
          Copy4x4(pPredBuf, 16, pReconBuf, 16);
      } else {

        if (residualPrediction) {
          if (iNumCoeffs != 0)
            ippiDequantResidual4x4_H264_16s(pTransformResult, pTransformResult, uMBQP);

          pTransformResult[0] = pDCBuf[chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]];

          src1 = pResidual + (uBlock & 1)*4 + ((uBlock & 2) >> 1)*stepR;
          ippiTransformResidualAndAdd_H264_16s16s_C1I(src1, pTransformResult,
            pTransformResult, stepR * 2, 8);

          src1 = pTransformResult;
          for (ii = 0; ii < 4; ii++) {
            for (jj = 0; jj < 4; jj++) {
              Ipp16s tmp;
              tmp = IPP_MIN(src1[jj], 255);
              tmp = IPP_MAX(tmp, -255);
              tmp += pPredBuf[jj];
              src1++;
              tmp = IPP_MIN(tmp, 255);
              tmp = IPP_MAX(tmp, 0);
              pReconBuf[jj] = (PIXTYPE)tmp;
            }
            pReconBuf += 16;
            pPredBuf += 16;
          }

        } else {
          ippiDequantTransformResidualAndAdd_H264_8u16s(
            pPredBuf,
            pTransform + (uBlock-startBlock)*16,
            &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
            pReconBuf,
            16,
            16,
            uMBQP,
            (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
            core_enc->m_SeqParamSet->bit_depth_chroma,
            NULL);
        }
      }
    }   // for uBlock in chroma plane
  } while (uBlock < uFinalBlock);
  uCBPChroma &= ~(0xffffffff<<(uBlock-16));

  cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;

  if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
    cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

/* ???
  if (cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_PCM){
    cur_mb.LocalMacroblockInfo->cbp = (cur_mb.MacroblockCoeffsInfo->chromaNC << 4);
  } else  {
    cur_mb.LocalMacroblockInfo->cbp = 0;
  }
*/
}


int SVC_CheckMBType_BaseMode(H264CurrentMacroblockDescriptor_8u16s *cur_mb)
{
    int tryBaseMode = 1;
    if (((cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_DIRECT) ||
        (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8) ||
        (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) ||
        (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_B_8x8) ||
         cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED))
    {
        /* MBTYPE_DIRECT type is forbidden for base prediction mode */
        Ipp32s block;
        H264MotionVector mv[8];

        // for each of the 8x8 blocks
        for (block=0; block<4; block++)
        {
            Ipp32s block_pos = (block >> 1) * 8 + (block & 1) * 2;
            Ipp32s the_same = 1;
            Ipp32s ind = 0;

            if (((cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8) ||
                (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) ||
                (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_B_8x8)) &&
                (cur_mb->GlobalMacroblockInfo->sbtype[block] != SBTYPE_DIRECT))
                continue;

            mv[2*block].mvx = 0;
            mv[2*block].mvy = 0;
            mv[2*block+1].mvx = 0;
            mv[2*block+1].mvy = 0;

            if (cur_mb->RefIdxs[0]->RefIdxs[block_pos] >= 0)
            {
                ind = 1;
                mv[2*block].mvx = cur_mb->MVs[LIST_0]->MotionVectors[block_pos].mvx;
                mv[2*block].mvy = cur_mb->MVs[LIST_0]->MotionVectors[block_pos].mvy;

                if ((cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 1].mvx != mv[2*block].mvx) ||
                    (cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 4].mvx != mv[2*block].mvx) ||
                    (cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 5].mvx != mv[2*block].mvx) ||
                    (cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 1].mvy != mv[2*block].mvy) ||
                    (cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 4].mvy != mv[2*block].mvy) ||
                    (cur_mb->MVs[LIST_0]->MotionVectors[block_pos + 5].mvy != mv[2*block].mvy))
                {
                    the_same = 0;
                }
            }

            {
                if (cur_mb->RefIdxs[1]->RefIdxs[block_pos] >= 0)
                {
                    ind += 2;
                    mv[2*block+1].mvx = cur_mb->MVs[LIST_1]->MotionVectors[block_pos].mvx;
                    mv[2*block+1].mvy = cur_mb->MVs[LIST_1]->MotionVectors[block_pos].mvy;

                    if ((cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 1].mvx != mv[2*block+1].mvx) ||
                        (cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 4].mvx != mv[2*block+1].mvx) ||
                        (cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 5].mvx != mv[2*block+1].mvx) ||
                        (cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 1].mvy != mv[2*block+1].mvy) ||
                        (cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 4].mvy != mv[2*block+1].mvy) ||
                        (cur_mb->MVs[LIST_1]->MotionVectors[block_pos + 5].mvy != mv[2*block+1].mvy))
                    {
                        the_same = 0;
                    }
                }
            }

//            VM_ASSERT(ind > 0);
            if (ind == 0) {
                tryBaseMode = 0;
                pSetMBBaseModeFlag(cur_mb->GlobalMacroblockInfo, 0);
                return tryBaseMode;
            }

            if (the_same)
            {
                if (ind == 1)
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_FORWARD_8x8;
                else if (ind == 2)
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_BACKWARD_8x8;
                else
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_BIDIR_8x8;
            }
            else
            {
                if (ind == 1)
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_FORWARD_4x4;
                else if (ind == 2)
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_BACKWARD_4x4;
                else
                    cur_mb->GlobalMacroblockInfo->sbtype[block] = SBTYPE_BIDIR_4x4;
            }
        }

        if (cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_DIRECT || cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED)
        {
//            cur_mb->GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;
            cur_mb->GlobalMacroblockInfo->mbtype = MBTYPE_B_8x8;

            if (((cur_mb->GlobalMacroblockInfo->sbtype[1]) == (cur_mb->GlobalMacroblockInfo->sbtype[0])) &&
                ((cur_mb->GlobalMacroblockInfo->sbtype[2]) == (cur_mb->GlobalMacroblockInfo->sbtype[0])) &&
                ((cur_mb->GlobalMacroblockInfo->sbtype[3]) == (cur_mb->GlobalMacroblockInfo->sbtype[0])) &&
                (mv[2].mvx == mv[0].mvx) && (mv[2].mvy == mv[0].mvy) &&
                (mv[4].mvx == mv[0].mvx) && (mv[4].mvy == mv[0].mvy) &&
                (mv[6].mvx == mv[0].mvx) && (mv[6].mvy == mv[0].mvy) &&
                (mv[3].mvx == mv[1].mvx) && (mv[3].mvy == mv[1].mvy) &&
                (mv[5].mvx == mv[1].mvx) && (mv[5].mvy == mv[1].mvy) &&
                (mv[7].mvx == mv[1].mvx) && (mv[7].mvy == mv[1].mvy))
            {
                if (cur_mb->GlobalMacroblockInfo->sbtype[0] == SBTYPE_FORWARD_8x8)
                    cur_mb->GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;
                else if (cur_mb->GlobalMacroblockInfo->sbtype[0] == SBTYPE_BACKWARD_8x8)
                    cur_mb->GlobalMacroblockInfo->mbtype = MBTYPE_BACKWARD;
                else if (cur_mb->GlobalMacroblockInfo->sbtype[0] == SBTYPE_BIDIR_8x8)
                    cur_mb->GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
            }
        }
    }

    return tryBaseMode;
}

}
#endif