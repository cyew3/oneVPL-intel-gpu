//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#include "h263.h"
#include "h263dec.h"

#pragma warning(disable : 188)  // enumerated type mixed with another type ICL

static h263_Status h263_SpatialInterpolateFrame(h263_Frame *srcFrame, h263_Frame *dstFrame, Ipp32s scal_type)
{
  IppiSize srcRoiSize;
  Ipp32s interp_type;

  switch (scal_type) {
  case H263_SCALABILITY_SPATIAL_X:
    interp_type = IPPVC_INTERP_HORIZONTAL;
    break;
  case H263_SCALABILITY_SPATIAL_Y:
    interp_type = IPPVC_INTERP_VERTICAL;
    break;
  default:
    interp_type = IPPVC_INTERP_2D;
  }

  srcRoiSize.height = srcFrame->mbPerCol * 16;
  srcRoiSize.width = srcFrame->mbPerRow * 16;

  if (ippiSpatialInterpolation_H263_8u_C1R(srcFrame->pY, srcFrame->stepY, srcRoiSize, dstFrame->pY, dstFrame->stepY, interp_type) != ippStsNoErr)
    return H263_STATUS_ERROR;

  srcRoiSize.height = srcFrame->mbPerCol * 8;
  srcRoiSize.width = srcFrame->mbPerRow * 8;

  if (ippiSpatialInterpolation_H263_8u_C1R(srcFrame->pCb, srcFrame->stepCb, srcRoiSize, dstFrame->pCb, dstFrame->stepCb, interp_type) != ippStsNoErr)
    return H263_STATUS_ERROR;
  if (ippiSpatialInterpolation_H263_8u_C1R(srcFrame->pCr, srcFrame->stepCr, srcRoiSize, dstFrame->pCr, dstFrame->stepCr, interp_type) != ippStsNoErr)
    return H263_STATUS_ERROR;

  return H263_STATUS_OK;
}

/*
//  h263_Info for decoding of Video Sequence
*/
h263_Status h263_InitVSeq(h263_Info* pInfo, Ipp32s mbPerRow, Ipp32s mbPerCol)
{
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;

  pInfo->VideoSequence.picture_time_increment_resolution = H263_PIC_TIME_INCR_RESOLUTION;

  pInfo->VideoSequence.PicIndex = 0;

  if (mbPerRow <= 0)
    mbPerRow = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  if (mbPerCol <= 0)
    mbPerCol = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol;

  pInfo->VideoSequence.cFrame.mbPerRow = pInfo->VideoSequence.rFrame.mbPerRow = pInfo->VideoSequence.nFrame.mbPerRow = pInfo->VideoSequence.bFrame.mbPerRow = mbPerRow;
  pInfo->VideoSequence.cFrame.mbPerCol = pInfo->VideoSequence.rFrame.mbPerCol = pInfo->VideoSequence.nFrame.mbPerCol = pInfo->VideoSequence.bFrame.mbPerCol = mbPerCol;
  pInfo->VideoSequence.cFrame.width = pInfo->VideoSequence.rFrame.width = pInfo->VideoSequence.nFrame.width = pInfo->VideoSequence.bFrame.width = VPic->width;
  pInfo->VideoSequence.cFrame.height = pInfo->VideoSequence.rFrame.height = pInfo->VideoSequence.nFrame.height = pInfo->VideoSequence.bFrame.height = VPic->height;

  pInfo->VideoSequence.num_of_MBs = mbPerRow * mbPerCol;

#ifdef _OMP_KARABAS
  if (pInfo->number_threads < 1 || pInfo->number_threads > h263_GetNumOfThreads())
    pInfo->number_threads = h263_GetNumOfThreads();
  pInfo->pMBinfoMT = (h263_MacroBlockMT*)ippsMalloc_8u(mbPerRow * pInfo->number_threads * sizeof(h263_MacroBlockMT));
  if(!pInfo->pMBinfoMT) return H263_STATUS_NO_MEM;
#endif // _OMP_KARABAS

  /* motion info */
  pInfo->VideoSequence.MBinfo = (h263_MacroBlock*)ippsMalloc_8u(mbPerRow*mbPerCol*sizeof(h263_MacroBlock));
  if (!pInfo->VideoSequence.MBinfo) return H263_STATUS_NO_MEM;

  /* motion info for B-frames */
  pInfo->VideoSequence.Bmv = (IppMotionVector*)ippsMalloc_8u(2*mbPerRow*sizeof(IppMotionVector));
  if (!pInfo->VideoSequence.Bmv) return H263_STATUS_NO_MEM;


  /* Advanced Intra Prediction info */
  pInfo->VideoSequence.IntraPredBuff.block = (h263_IntraPredBlock*)ippsMalloc_8u((mbPerRow + 1)*6*sizeof(h263_IntraPredBlock));
  if (!pInfo->VideoSequence.IntraPredBuff.block)
    return H263_STATUS_NO_MEM;
  {
    h263_IntraPredBlock *mbCurr = pInfo->VideoSequence.IntraPredBuff.block;
    h263_IntraPredBlock *mbA = mbCurr, *mbC = mbCurr + 6;
    Ipp32s              j;

    for (j = 0; j < mbPerRow; j ++) {
        mbCurr[0].predA = &mbA[1];   mbCurr[0].predC = &mbC[2];
        mbCurr[1].predA = &mbC[0];   mbCurr[1].predC = &mbC[3];
        mbCurr[2].predA = &mbA[3];   mbCurr[2].predC = &mbC[0];
        mbCurr[3].predA = &mbC[2];   mbCurr[3].predC = &mbC[1];
        mbCurr[4].predA = &mbA[4];   mbCurr[4].predC = &mbC[4];
        mbCurr[5].predA = &mbA[5];   mbCurr[5].predC = &mbC[5];
        mbCurr += 6;  mbA += 6;  mbC += 6;
    }
  }

    return H263_STATUS_OK;
}

/*
//  Free memory allocated for h263_Info
*/
h263_Status h263_FreeVSeq(h263_Info* pInfo)
{
  pInfo->VideoSequence.PicIndex = 0;

#ifdef _OMP_KARABAS
  ippsFree(pInfo->pMBinfoMT); pInfo->pMBinfoMT = NULL;
#endif

  ippsFree(pInfo->VideoSequence.MBinfo); pInfo->VideoSequence.MBinfo = NULL;
  ippsFree(pInfo->VideoSequence.IntraPredBuff.block); pInfo->VideoSequence.IntraPredBuff.block = NULL;
  ippsFree(pInfo->VideoSequence.Bmv); pInfo->VideoSequence.Bmv = NULL;

  return H263_STATUS_OK;
}

static void h263_ExpandFrameReplicate(Ipp8u *pSrcDstPlane, Ipp32s frameWidth, Ipp32s frameHeight, Ipp32s expandPels, Ipp32s step)
{
    Ipp8u   *pDst1, *pDst2, *pSrc1, *pSrc2;
    Ipp32s  i, j;
    Ipp32u  t1, t2;

    pDst1 = pSrcDstPlane + step * expandPels;
    pDst2 = pDst1 + frameWidth + expandPels;
    if (expandPels == 8) {
        for (i = 0; i < frameHeight; i ++) {
            t1 = pDst1[8] + (pDst1[8] << 8);
            t2 = pDst2[-1] + (pDst2[-1] << 8);
            t1 = (t1 << 16) + t1;
            t2 = (t2 << 16) + t2;
            ((Ipp32u*)pDst1)[0] = t1;
            ((Ipp32u*)pDst1)[1] = t1;
            ((Ipp32u*)pDst2)[0] = t2;
            ((Ipp32u*)pDst2)[1] = t2;
            pDst1 += step;
            pDst2 += step;
        }
    } else if (expandPels == 16) {
        for (i = 0; i < frameHeight; i ++) {
            t1 = pDst1[16] + (pDst1[16] << 8);
            t2 = pDst2[-1] + (pDst2[-1] << 8);
            t1 = (t1 << 16) + t1;
            t2 = (t2 << 16) + t2;
            ((Ipp32u*)pDst1)[0] = t1;
            ((Ipp32u*)pDst1)[1] = t1;
            ((Ipp32u*)pDst1)[2] = t1;
            ((Ipp32u*)pDst1)[3] = t1;
            ((Ipp32u*)pDst2)[0] = t2;
            ((Ipp32u*)pDst2)[1] = t2;
            ((Ipp32u*)pDst2)[2] = t2;
            ((Ipp32u*)pDst2)[3] = t2;
            pDst1 += step;
            pDst2 += step;
        }
    } else {
        for (i = 0; i < frameHeight; i ++) {
            ippsSet_8u(pDst1[expandPels], pDst1, expandPels);
            ippsSet_8u(pDst2[-1], pDst2, expandPels);
            pDst1 += step;
            pDst2 += step;
        }
    }
    pDst1 = pSrcDstPlane;
    pSrc1 = pSrcDstPlane + expandPels * step;
    pDst2 = pSrc1 + frameHeight * step;
    pSrc2 = pDst2 - step;
    j = frameWidth + 2 * expandPels;
    for (i = 0; i < expandPels; i ++) {
        ippsCopy_8u(pSrc1, pDst1, j);
        ippsCopy_8u(pSrc2, pDst2, j);
        pDst1 += step;
        pDst2 += step;
    }
}

/*
//  padding Picture: replication
*/

static void h263_PadFrame(h263_Frame *frame)
{
  Ipp32s  wL, hL, wC, hC;
  wL = frame->mbPerRow * 16;
  hL = frame->mbPerCol * 16;
  wC = frame->mbPerRow * 8;
  hC = frame->mbPerCol * 8;
  h263_ExpandFrameReplicate(frame->apY, wL, hL, 16, frame->stepY);
  h263_ExpandFrameReplicate(frame->apCb, wC, hC, 8, frame->stepCb);
  h263_ExpandFrameReplicate(frame->apCr, wC, hC, 8, frame->stepCr);
}

#define H263_COMPARE_RESAMPLE_PARAMS(flag, VSeq, VPic) \
{ \
  Ipp32s i; \
  flag = 0; \
  for (i = 0; i < 4; i++) { \
    if ((VSeq)->warpParams[i].dx != (VPic)->warpParams[i].dx || (VSeq)->warpParams[i].dy != (VPic)->warpParams[i].dy) { \
      flag = 1; \
      break; \
    } \
  } \
  if ((VSeq)->fillMode != (VPic)->fillMode) \
    flag = 1; \
  if ((VPic)->fillMode == 0) { \
    for (i = 0; i < 3; i++) { \
      if ((VSeq)->fillColor[i] != (VPic)->fillColor[i]) { \
        flag = 1; \
        break; \
      } \
    } \
  } \
}

#define H263_UPDATE_RESAMPLE_PARAMS(VPic, VSeq) \
{ \
  Ipp32s i; \
  for (i = 0; i < 4; i++) { \
   (VSeq)->warpParams[i].dx = (VPic)->warpParams[i].dx; \
   (VSeq)->warpParams[i].dy = (VPic)->warpParams[i].dy; \
  } \
  (VSeq)->fillMode = (VPic)->fillMode; \
  if ((VPic)->fillMode == 0) { \
    for (i = 0; i < 3; i++) { \
      (VSeq)->fillColor[i] = (VPic)->fillColor[i]; \
    } \
  } \
}

static void h263_PadResampledFrame(h263_Frame *frame, Ipp32s fillMode, Ipp32s fillColor[3])
{
  Ipp32s i, j;
  Ipp8u  colorY, colorCb, colorCr;
  Ipp8u *p0, *p1;
  Ipp32s width, height;
  Ipp32u tY = 0;
  Ipp16u tCb = 0, tCr = 0;

  /* frame width and height are always divisible by 4 */

  if (fillMode == 0) {
    colorY = (Ipp8u)fillColor[0];
    colorCb = (Ipp8u)fillColor[1];
    colorCr = (Ipp8u)fillColor[2];
    tY = (colorY << 8) | colorY;
    tY = (tY << 16) | tY;
    tCb = (colorCb << 8) | colorCb;
    tCr = (colorCr << 8) | colorCr;
  } else if (fillMode == 1) {
    tY = 0x10101010; /* 16 */
    tCb = tCr = 0x8080; /* 128 */
  } else if (fillMode == 2) {
    tY = 0x80808080;
    tCb = tCr = 0x8080;
  }

  width = frame->width;
  height = frame->height;

  if (width & 15) {
    for (i = 0; i < height; i++) {
      p0 = frame->pY + width;
      if (fillMode == 3) {
        tY = (p0[-1] << 8) | p0[-1];
        tY = (tY << 16) | tY;
      }
      for (j = 0; j < (16 - (width & 15)) >> 2; j++)
        ((Ipp32u *)p0)[j] = tY;
    }

    width >>= 1;
    height >>= 1;
    for (i = 0; i < height; i++) {
      p0 = frame->pCb + width;
      p1 = frame->pCr + width;

      if (fillMode == 3) {
        tCb = (p0[-1] << 8) | p0[-1];
        tCr = (p1[-1] << 8) | p1[-1];
      }
      for (j = 0; j < (8 - (width & 7)) >> 1; j++) {
        ((Ipp16u *)p0)[j] = tCb;
        ((Ipp16u *)p1)[j] = tCr;
      }
    }
  }

  if (frame->height & 15) {
    width = frame->width;
    height = frame->height;

    p0 = frame->pY + height * frame->stepY;
    for (i = 0; i < 16 - (height & 15); i++) {
      if (fillMode == 3) {
        ippsCopy_8u(p0 - frame->stepY, p0, (width + 15) &~ 15);
      } else {
        ippsSet_8u((Ipp8u)tY, p0, (width + 15) &~ 15);
      }
      p0 += frame->stepY;
    }

    width >>= 1;
    height >>= 1;

    p0 = frame->pCb + height * frame->stepCb;
    p1 = frame->pCr + height * frame->stepCr;

    for (i = 0; i < 8 - (height & 7); i++) {
      if (fillMode == 3) {
        ippsCopy_8u(p0 - frame->stepCb, p0, (width + 7) &~ 7);
        ippsCopy_8u(p1 - frame->stepCr, p1, (width + 7) &~ 7);
      } else {
        ippsSet_8u((Ipp8u)tCb, p0, (width + 7) &~ 7);
        ippsSet_8u((Ipp8u)tCr, p1, (width + 7) &~ 7);
      }
    }
    p0 += frame->stepCb;
    p1 += frame->stepCr;
  }
}

static h263_Status h263_Resample(h263_Info* pInfo, h263_Frame *srcFr, h263_Frame *dstFr, Ipp32s implicit)
{
  h263_VideoPicture* VPic = &pInfo->VideoSequence.VideoPicture;
  IppMotionVector *warps = VPic->warpParams;
  IppiSize  srcRoi, dstRoi;

  srcRoi.height = srcFr->height;
  srcRoi.width = srcFr->width;
  dstRoi.height = dstFr->height;
  dstRoi.width = dstFr->width;

  if (implicit || (warps[0].dx | warps[0].dy | warps[1].dx | warps[1].dy | warps[2].dx | warps[2].dy | warps[3].dx | warps[3].dy) == 0) {
    if (srcFr->width == 2*dstFr->width && srcFr->height == 2*dstFr->height) {
      ippiDownsampleFour_H263_8u_C1R(srcFr->pY, srcFr->stepY, srcRoi, dstFr->pY, dstFr->stepY, VPic->rtype);
      srcRoi.height >>= 1;
      srcRoi.width >>= 1;
      ippiDownsampleFour_H263_8u_C1R(srcFr->pCb, srcFr->stepCb, srcRoi, dstFr->pCb, dstFr->stepCb, VPic->rtype);
      ippiDownsampleFour_H263_8u_C1R(srcFr->pCr, srcFr->stepCr, srcRoi, dstFr->pCr, dstFr->stepCr, VPic->rtype);
    } else if (2*srcFr->width == dstFr->width && 2*srcFr->height == dstFr->height) {
      ippiUpsampleFour_H263_8u_C1R(srcFr->pY, srcFr->stepY, srcRoi, dstFr->pY, dstFr->stepY, VPic->rtype, VPic->fillMode == 3 ? -1 : VPic->fillColor[0]);
      srcRoi.height >>= 1;
      srcRoi.width >>= 1;
      ippiUpsampleFour_H263_8u_C1R(srcFr->pCb, srcFr->stepCb, srcRoi, dstFr->pCb, dstFr->stepCb, VPic->rtype, VPic->fillMode == 3 ? -1 : VPic->fillColor[1]);
      ippiUpsampleFour_H263_8u_C1R(srcFr->pCr, srcFr->stepCr, srcRoi, dstFr->pCr, dstFr->stepCr, VPic->rtype, VPic->fillMode == 3 ? -1 : VPic->fillColor[2]);
    } else {
      ippiResample_H263_8u_P3R(srcFr->pY, srcFr->stepY, srcRoi, srcFr->pCb, srcFr->stepCb, srcFr->pCr, srcFr->stepCr,
                               dstFr->pY, dstFr->stepY, dstRoi, dstFr->pCb, dstFr->stepCb, dstFr->pCr, dstFr->stepCr,
                               warps, VPic->wda, VPic->rtype, VPic->fillMode, VPic->fillColor);
    }
  } else {
    ippiResample_H263_8u_P3R(srcFr->pY, srcFr->stepY, srcRoi, srcFr->pCb, srcFr->stepCb, srcFr->pCr, srcFr->stepCr,
                             dstFr->pY, dstFr->stepY, dstRoi, dstFr->pCb, dstFr->stepCb, dstFr->pCr, dstFr->stepCr,
                             warps, VPic->wda, VPic->rtype, VPic->fillMode, VPic->fillColor);
  }

  if ((dstFr->width | dstFr->height) & 15)
    h263_PadResampledFrame(dstFr, VPic->fillMode, VPic->fillColor);

  h263_PadFrame(dstFr);

  H263_UPDATE_RESAMPLE_PARAMS(VPic, &pInfo->VideoSequence);

  return H263_STATUS_OK;
}
Ipp32s h263_Check_Resize(h263_Info* pInfo)
{
  h263_VideoPicture* VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s mbpR = VPic->MacroBlockPerRow, mbpC = VPic->MacroBlockPerCol;
  Ipp32s newSize = (mbpR + 2*H263_NUM_EXT_MB) * (mbpC + 2*H263_NUM_EXT_MB);
  Ipp32s oldSize;
  h263_Frame *frame, *refFrame;
  Ipp32s resizeMask = 0, msk;
  h263_EnhancedLayer *enh_layer = NULL;
  Ipp32s modiFlag;

  VPic->implicit_resample = 0;

  if (VPic->MacroBlockPerRow * VPic->MacroBlockPerCol > pInfo->VideoSequence.num_of_MBs)
    resizeMask |= 0x100;

  switch (VPic->picture_coding_type) {
  case H263_PIC_TYPE_I:
    frame = &pInfo->VideoSequence.rFrame;
    oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
    if (newSize > oldSize)
      resizeMask |= 1;
    break;
  case H263_PIC_TYPE_P:
    frame = &pInfo->VideoSequence.rFrame;
    oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
    if (newSize > oldSize)
      resizeMask |= 1;

    if (!VPic->modes.resample && // VPic->plusptype &&
      (VPic->width != pInfo->VideoSequence.cFrame.width || VPic->height != pInfo->VideoSequence.cFrame.height))
      VPic->implicit_resample = 1;

    if (VPic->modes.resample || VPic->implicit_resample) {
      frame = &pInfo->VideoSequence.bFrame; // ref frame will be resampled into this one
      oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
      if (newSize > oldSize)
        resizeMask |= 4;
    }
    break;

  case H263_PIC_TYPE_PB:
  case H263_PIC_TYPE_iPB:
    frame = &pInfo->VideoSequence.rFrame;
    oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
    if (newSize > oldSize)
      resizeMask |= 1;

    if (pInfo->VideoSequence.Bpics_to_show > 0) {
      frame = &pInfo->VideoSequence.bFrame;
      msk = 4;
    } else {
      frame = &pInfo->VideoSequence.nFrame;
      msk = 2;
    }
    oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
    if (newSize > oldSize)
      resizeMask |= msk;

    if (!VPic->modes.resample && // VPic->plusptype &&
      (VPic->width != pInfo->VideoSequence.cFrame.width || VPic->height != pInfo->VideoSequence.cFrame.height))
      VPic->implicit_resample = 1;

    if (VPic->modes.resample || VPic->implicit_resample) {
      frame = &pInfo->VideoSequence.aFrame;
      oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
      if (newSize > oldSize)
        resizeMask |= 8;
    }
    break;

  case H263_PIC_TYPE_B:
    if (VPic->enh_layer_num == 1) {
      if (pInfo->VideoSequence.Bpics_to_show > 0) {
        frame = &pInfo->VideoSequence.bFrame;
        msk = 4;
      } else {
        frame = &pInfo->VideoSequence.nFrame;
        msk = 2;
      }
      oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
      if (newSize > oldSize)
        resizeMask |= msk;
    } else {
      enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
      if (enh_layer == NULL)
        resizeMask |= 0xF0;
      else { // paranoid: what if enhanced layer frame size has changed
        frame = &enh_layer->n_Frame;
        oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
        if (newSize > oldSize)
          resizeMask |= 0x20;
      }
    }

    if (VPic->enh_layer_num == 1 || VPic->ref_layer_num == 1)
      refFrame = &pInfo->VideoSequence.cFrame;
    else
      refFrame = &pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2]->c_Frame;

    if (!VPic->modes.resample && // VPic->plusptype &&
      (VPic->width != refFrame->width || VPic->height != refFrame->height)) {
        VPic->implicit_resample = 1;
        VPic->wda = 3;
        VPic->fillMode = 3;
        h263_Zero4MV(VPic->warpParams);
      }

    if (VPic->modes.resample || VPic->implicit_resample) {
      H263_COMPARE_RESAMPLE_PARAMS(modiFlag, &pInfo->VideoSequence, VPic);
      if (modiFlag) {
        if (VPic->enh_layer_num == 1) {
          frame = &pInfo->VideoSequence.aFrame;
          msk = 8;
        } else {
          frame = &enh_layer->a_Frame;
          msk = 0x80;
        }
        oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
        if (newSize > oldSize)
          resizeMask |= msk;
      }
    }
    break;

  case H263_PIC_TYPE_EI:
    enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
    if (enh_layer == NULL)
      resizeMask |= 0xF0;
    else {
      frame = &enh_layer->r_Frame;
      oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
      if (newSize > oldSize)
        resizeMask |= 0x10;
    }
    break;

  case H263_PIC_TYPE_EP:
    enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
    /* layers can not start with EP frame */
    if (enh_layer == NULL)
      return -1;
    frame = &enh_layer->r_Frame;
    oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
    if (newSize > oldSize)
      resizeMask |= 0x10;

    if (!VPic->modes.resample && // VPic->plusptype &&
      (VPic->width != enh_layer->c_Frame.width || VPic->height != enh_layer->c_Frame.height))
      VPic->implicit_resample = 1;

    if (VPic->modes.resample || VPic->implicit_resample) {
      frame = &enh_layer->a_Frame;
      oldSize = (frame->mbPerRow + 2*H263_NUM_EXT_MB) * (frame->mbPerCol + 2*H263_NUM_EXT_MB);
      if (newSize > oldSize)
        resizeMask |= 0x80;
    }
    break;
  default:
    return -1;
  }

  return resizeMask;
}

/**************************************************************************/
/*                    Main "interface" function                           */
/**************************************************************************/

h263_Status h263_DecodeVideoPicture(h263_Info* pInfo)
{
  h263_Status status = H263_STATUS_OK;
  Ipp64s      pic_time;
  h263_VideoPicture* VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s temp_ref_delta;
  Ipp32s time_factor;
  h263_EnhancedLayer *enh_layer = NULL, *ref_layer = NULL;
  h263_Frame *refFrame, *lowFrame, *curFrame, *auxFrame;

  if (pInfo->VideoSequence.Ppics_to_show > 0) {  /* two P-frames/parts already pending */
    pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.rFrame;
    pInfo->VideoSequence.Ppics_to_show--;
    return H263_STATUS_OK;
  }

  /* set picture time */
  if (pInfo->VideoSequence.PicIndex != 0) {
    if (VPic->picture_coding_type != H263_PIC_TYPE_B)
      temp_ref_delta = VPic->temporal_reference - pInfo->VideoSequence.prevP_temporal_reference;
    else
      temp_ref_delta = VPic->temporal_reference - pInfo->VideoSequence.ref_temporal_reference;
  } else
    temp_ref_delta = 0;

  if (temp_ref_delta < 0) {
    if (VPic->PCF)
      temp_ref_delta += 1024;
    else
      temp_ref_delta += 256;
  }

  /* real time = pInfo->VideoSequence.pic_time/1800000 */
  if (VPic->prev_clock_divisor) {
    time_factor = VPic->prev_clock_divisor * (1000 + VPic->prev_clock_conversion_code);
  } else {
    time_factor = H263_DEFAULT_FRAME_INTERVAL;
  }

  if (VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB)
    pInfo->VideoSequence.pic_time_pb = pInfo->VideoSequence.prevP_pic_time + VPic->temporal_reference_B * time_factor;

  if (VPic->picture_coding_type != H263_PIC_TYPE_B) {
    pic_time = pInfo->VideoSequence.pic_time = pInfo->VideoSequence.prevP_pic_time + temp_ref_delta * time_factor;

    pInfo->VideoSequence.ref_temporal_reference = pInfo->VideoSequence.prevP_temporal_reference;
    pInfo->VideoSequence.prevP_temporal_reference = VPic->temporal_reference;

    pInfo->VideoSequence.ref_pic_time = pInfo->VideoSequence.prevP_pic_time;
    pInfo->VideoSequence.prevP_pic_time = pInfo->VideoSequence.pic_time;
  } else
    pic_time = pInfo->VideoSequence.ref_pic_time + temp_ref_delta * time_factor;

  switch (VPic->picture_coding_type) {
  case H263_PIC_TYPE_I :
    /* set new video frame */
    if (pInfo->VideoSequence.PicIndex == 0) {
      pInfo->VideoSequence.vFrame = NULL;
    } else {
      h263_SWAP(h263_Frame, pInfo->VideoSequence.rFrame, pInfo->VideoSequence.cFrame);
      if (pInfo->VideoSequence.Bpics_to_show > 0) {
        pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.nFrame;
        pInfo->VideoSequence.Bpics_to_show--;
        pInfo->VideoSequence.Ppics_to_show++;
      } else
        pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.rFrame;
    }
#ifdef _OMP_KARABAS
    if (pInfo->number_threads > 1)
      status = h263_DecodeFrame_I_MT(pInfo);
    else
#endif
    status = h263_DecodeFrame_I(pInfo);
    h263_PadFrame(&pInfo->VideoSequence.cFrame);

    pInfo->VideoSequence.cFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type;
    /* save current frame time */
    pInfo->VideoSequence.cFrame.time = pic_time;

    /* set past and future time for B-Plane */
    pInfo->VideoSequence.rTime = pInfo->VideoSequence.nTime;
    pInfo->VideoSequence.nTime = pic_time;
    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_I);
    break;
  case H263_PIC_TYPE_P :
    /* set new video frame */
    h263_SWAP(h263_Frame, pInfo->VideoSequence.rFrame, pInfo->VideoSequence.cFrame);
    if (pInfo->VideoSequence.Bpics_to_show > 0) {
      pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.nFrame;
      pInfo->VideoSequence.Bpics_to_show--;
      pInfo->VideoSequence.Ppics_to_show++;
    } else
      pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.rFrame;

    if (VPic->modes.resample || VPic->implicit_resample) {
      h263_Resample(pInfo, &pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.bFrame, VPic->implicit_resample);
      refFrame = &pInfo->VideoSequence.bFrame;
    } else
      refFrame = &pInfo->VideoSequence.rFrame;

    pInfo->VideoSequence.refFrame = refFrame;

#ifdef _OMP_KARABAS
    if (pInfo->number_threads > 1)
      status = h263_DecodeFrame_P_MT(pInfo, refFrame);
    else
#endif
    status = h263_DecodeFrame_P(pInfo, refFrame);
    h263_PadFrame(&pInfo->VideoSequence.cFrame);

    pInfo->VideoSequence.cFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type;
    /* save current frame time */
    pInfo->VideoSequence.cFrame.time = pic_time;

    /* set past and future time for B-Frame */
    pInfo->VideoSequence.rTime = pInfo->VideoSequence.nTime;
    pInfo->VideoSequence.nTime = pic_time;
    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_P);
    break;
  case H263_PIC_TYPE_PB :
  case H263_PIC_TYPE_iPB :

    /* set new video frame */
    h263_SWAP(h263_Frame, pInfo->VideoSequence.rFrame, pInfo->VideoSequence.cFrame);

    if (pInfo->VideoSequence.Bpics_to_show > 0) {
      h263_SWAP(h263_Frame, pInfo->VideoSequence.nFrame, pInfo->VideoSequence.bFrame);
      pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.bFrame;
      pInfo->VideoSequence.Ppics_to_show++;
    } else {
      pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.rFrame;
      pInfo->VideoSequence.Bpics_to_show++;
    }

    if (VPic->modes.resample || VPic->implicit_resample) {
      h263_Resample(pInfo, &pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.aFrame, VPic->implicit_resample);
      refFrame = &pInfo->VideoSequence.aFrame;
    } else
      refFrame = &pInfo->VideoSequence.rFrame;

    pInfo->VideoSequence.refFrame = refFrame;

#ifdef _OMP_KARABAS
    if (pInfo->number_threads > 1)
      status = h263_DecodeFrame_P_MT(pInfo, refFrame);
    else
#endif
    status = h263_DecodeFrame_P(pInfo, refFrame);
    h263_PadFrame(&pInfo->VideoSequence.cFrame);

    pInfo->VideoSequence.cFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type;
    pInfo->VideoSequence.nFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type; /* ??? */
    /* save current frame time */
    pInfo->VideoSequence.cFrame.time = pic_time;
    pInfo->VideoSequence.nFrame.time = pInfo->VideoSequence.pic_time_pb;

    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_PB);

    /* set past and future time for B-Frames */
    pInfo->VideoSequence.rTime = pInfo->VideoSequence.nTime;
    pInfo->VideoSequence.nTime = pic_time;
    break;
  case H263_PIC_TYPE_B :

    pInfo->VideoSequence.TRB = (Ipp32s)(pic_time - pInfo->VideoSequence.rTime);
    pInfo->VideoSequence.TRD = (Ipp32s)(pInfo->VideoSequence.nTime - pInfo->VideoSequence.rTime);

    if (VPic->enh_layer_num == 1) {
      if (pInfo->VideoSequence.Bpics_to_show > 0)
        h263_SWAP(h263_Frame, pInfo->VideoSequence.bFrame, pInfo->VideoSequence.nFrame);
      refFrame = &pInfo->VideoSequence.rFrame;
      curFrame = &pInfo->VideoSequence.nFrame;
      auxFrame = &pInfo->VideoSequence.aFrame;
    } else {
      enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
      curFrame = &enh_layer->n_Frame;
      auxFrame = &enh_layer->a_Frame;
      if (VPic->ref_layer_num == 1)
        refFrame = &pInfo->VideoSequence.rFrame;
      else {
       ref_layer = pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2];
       refFrame = &ref_layer->r_Frame;
      }
    }

    if (VPic->modes.resample || VPic->implicit_resample) {
      Ipp32s modiFlag;
      H263_COMPARE_RESAMPLE_PARAMS(modiFlag, &pInfo->VideoSequence, VPic);
      if (modiFlag) {
        h263_Resample(pInfo, refFrame, auxFrame, VPic->implicit_resample);
        refFrame = auxFrame;
      } else
        refFrame = (VPic->enh_layer_num == 1 ? pInfo->VideoSequence.refFrame : &enh_layer->a_Frame);
    }

    status = h263_DecodeFrame_B(pInfo, enh_layer, refFrame);

    if (VPic->enh_layer_num == 1) {
      pInfo->VideoSequence.nFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type;
      pInfo->VideoSequence.nFrame.time = pic_time;
      if (pInfo->VideoSequence.Bpics_to_show > 0) {
        /* check what goes first: B (nFrame) or B-part (bFrame) */
        if (pic_time < pInfo->VideoSequence.bFrame.time) /* B frame is first */
          h263_SWAP(h263_Frame, pInfo->VideoSequence.bFrame, pInfo->VideoSequence.nFrame);
        pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.bFrame;
      } else
        pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.nFrame;
    } else {
      enh_layer->n_Frame.type = VPic->picture_coding_type;
      enh_layer->n_Frame.time = pic_time;
      enh_layer->v_Frame = &enh_layer->n_Frame;
    }

    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_B);
    break;

  case H263_PIC_TYPE_EI :
    enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
    h263_SWAP(h263_Frame, enh_layer->r_Frame, enh_layer->c_Frame);
    if (enh_layer->picIndex)
      enh_layer->v_Frame = &enh_layer->r_Frame;
    else
      enh_layer->v_Frame = NULL;

    lowFrame = (VPic->ref_layer_num == 1 ? &pInfo->VideoSequence.cFrame :
                                           &pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2]->c_Frame);

    if (VPic->format.scalability_type == H263_SCALABILITY_SNR) {
      enh_layer->l_Frame = lowFrame;
    } else {
      enh_layer->l_Frame = &enh_layer->n_Frame;
      if ((status = h263_SpatialInterpolateFrame(lowFrame, enh_layer->l_Frame, VPic->format.scalability_type)) != H263_STATUS_OK) {
        h263_Error("Error when doing spatially interpolation");
        return status;
      }
    }

    status = h263_DecodeFrame_EI(pInfo, enh_layer);

    h263_PadFrame(&enh_layer->c_Frame);

    enh_layer->c_Frame.type = VPic->picture_coding_type;
    enh_layer->c_Frame.time = pic_time;

    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_EI);
    break;

  case H263_PIC_TYPE_EP :
    enh_layer = pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2];
    h263_SWAP(h263_Frame, enh_layer->r_Frame, enh_layer->c_Frame);
    enh_layer->v_Frame = &enh_layer->r_Frame;

    if (VPic->modes.resample || VPic->implicit_resample) {
      h263_Resample(pInfo, &enh_layer->r_Frame, &enh_layer->a_Frame, VPic->implicit_resample);
      refFrame = &enh_layer->a_Frame;
    } else
      refFrame = &enh_layer->r_Frame;

    lowFrame = (VPic->ref_layer_num == 1 ? &pInfo->VideoSequence.cFrame :
                                           &pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2]->c_Frame);

    if (VPic->format.scalability_type == H263_SCALABILITY_SNR) {
      enh_layer->l_Frame = lowFrame;
    } else {
      enh_layer->l_Frame = &enh_layer->n_Frame;
      if ((status = h263_SpatialInterpolateFrame(lowFrame, enh_layer->l_Frame, VPic->format.scalability_type)) != H263_STATUS_OK) {
        h263_Error("Error when doing spatial interpolation");
        return status;
      }
    }

    status = h263_DecodeFrame_EP(pInfo, enh_layer, refFrame);

    h263_PadFrame(&enh_layer->c_Frame);

    enh_layer->c_Frame.type = VPic->picture_coding_type;
    enh_layer->c_Frame.time = pic_time;

    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_EP);
    break;

  case H263_PIC_TYPE_P_UNREF : // Sorenson 263 only
    /* set new video frame */
    h263_SWAP(h263_Frame, pInfo->VideoSequence.bFrame, pInfo->VideoSequence.nFrame);
    pInfo->VideoSequence.vFrame = &pInfo->VideoSequence.nFrame;

    h263_SWAP(h263_Frame, pInfo->VideoSequence.cFrame, pInfo->VideoSequence.nFrame);
    refFrame = &pInfo->VideoSequence.nFrame;
#ifdef _OMP_KARABAS
    if (pInfo->number_threads > 1)
      status = h263_DecodeFrame_P_MT(pInfo, refFrame);
    else
#endif
      status = h263_DecodeFrame_P(pInfo, refFrame);
    pInfo->VideoSequence.cFrame.type = pInfo->VideoSequence.VideoPicture.picture_coding_type;
    /* save current frame time */
    pInfo->VideoSequence.cFrame.time = pic_time;

    h263_SWAP(h263_Frame, pInfo->VideoSequence.cFrame, pInfo->VideoSequence.nFrame);
    h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic_P);
    break;
  }

  pInfo->VideoSequence.prev_pic_type[VPic->enh_layer_num - 1] = VPic->picture_coding_type;
  h263_StatisticInc(&pInfo->VideoSequence.Statistic.nPic);

  return status;
}

void h263_CopyMacroBlocks(h263_Frame *rFrame, h263_Frame *cFrame, Ipp32s mbPerRow, Ipp32s rowNum, Ipp32s colNum, Ipp32s n)
{
  Ipp32s  i, stepYr, stepYc, stepCbr, stepCbc, stepCrr, stepCrc;
  Ipp8u   *pYc, *pCbc, *pCrc, *pYr, *pCbr, *pCrr;

  if (n <= 0)
    return;
  stepYc = cFrame->stepY;
  stepCbc = cFrame->stepCb;
  stepCrc = cFrame->stepCr;
  stepYr = rFrame->stepY;
  stepCbr = rFrame->stepCb;
  stepCrr = rFrame->stepCr;
  pYc = cFrame->pY + (rowNum * stepYc + colNum) * 16;
  pCbc = cFrame->pCb + (rowNum * stepCbc + colNum) * 8;
  pCrc = cFrame->pCr + (rowNum * stepCrc + colNum) * 8;
  pYr = rFrame->pY + (rowNum * stepYr + colNum) * 16;
  pCbr = rFrame->pCb + (rowNum * stepCbr + colNum) * 8;
  pCrr = rFrame->pCr + (rowNum * stepCrr + colNum) * 8;
  for (i = rowNum * mbPerRow + colNum; i < rowNum * mbPerRow + colNum + n; i ++) {
    ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
    ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
    ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);
    if ((i + 1) % mbPerRow == 0) {
      pYc += (2 * H263_NUM_EXT_MB + 1) * 16 + (stepYc << 4) - stepYc;
      pCbc += (2 * H263_NUM_EXT_MB + 1) * 8 + (stepCbc << 3) - stepCbc;
      pCrc += (2 * H263_NUM_EXT_MB + 1) * 8 + (stepCrc << 3) - stepCrc;
      pYr += (2 * H263_NUM_EXT_MB + 1) * 16 + (stepYr << 4) - stepYr;
      pCbr += (2 * H263_NUM_EXT_MB + 1) * 8 + (stepCbr << 3) - stepCbr;
      pCrr += (2 * H263_NUM_EXT_MB + 1) * 8 + (stepCrr << 3) - stepCrr;
    } else {
      pYc += 16; pCrc += 8; pCbc += 8;
      pYr += 16; pCrr += 8; pCbr += 8;
    }
  }
}
