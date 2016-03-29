/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//                     bitrate control interface
//
*/

#include <math.h>
#include "umc_video_brc.h"
#include "umc_brc.h"
#include "umc_h264_brc.h"
#include "umc_mpeg2_brc.h"
#include "umc_svc_brc.h"

namespace UMC
{

VideoBrc* CreateBRC(Ipp32u brcID)
{
  if (brcID == BRC_H264)
    return new H264BRC;
  if (brcID == BRC_MPEG2)
    return new MPEG2BRC;
  if (brcID == BRC_SVC)
    return new SVCBRC;
  else
    return new CommonBRC;

}

void DeleteBRC(VideoBrc** brc)
{
  if (*brc) {
    delete *brc;
    *brc = NULL;
  }
}

VideoBrcParams::VideoBrcParams()
{
  BRCMode = BRC_CBR;
  targetBitrate = 500000; // Kbps = 1000 bps
  maxBitrate = targetBitrate;
  HRDBufferSizeBytes = targetBitrate * 2 / 30; // ~target frame size (at 30 fps) * 16; KB = 1024 B
  HRDInitialDelayBytes = HRDBufferSizeBytes / 2;
  GOPPicSize = 20;
  GOPRefDist = 1;
  accuracy = 10;
  convergence = 500;
  frameRateExtN_1 = 0; // svc specific
}

VideoBrc::VideoBrc()
{
}

VideoBrc::~VideoBrc()
{
}


Status VideoBrc::CheckCorrectParams_MPEG2(VideoBrcParams *inBrcParams, VideoBrcParams *outBrcParams)
{
  Ipp64f bitsPerFrame;
  Status status = UMC_OK;
  Ipp32s brcMode, maxBitrate, targetBitrate, bufferSizeBytes;
  Ipp32u maxBufSizeBits, bufSizeBits;
  Ipp64f framerate;
  Ipp32s initialDelayBytes;
  Ipp64f bufFullness;
  Ipp64f gopFactor;

  if (inBrcParams == 0)
    return UMC_ERR_NULL_PTR;

  brcMode = inBrcParams->BRCMode;
  maxBitrate = inBrcParams->maxBitrate;
  targetBitrate = inBrcParams->targetBitrate;
  bufferSizeBytes = inBrcParams->HRDBufferSizeBytes;
  framerate = inBrcParams->info.framerate;
  bufFullness = inBrcParams->HRDInitialDelayBytes * 8;
  gopFactor = inBrcParams->GOPPicSize >= 100 ? 1 : 10 / sqrt((Ipp64f)inBrcParams->GOPPicSize);

  if (framerate <= 0)
    return UMC_ERR_FAILED;


  if (outBrcParams)
    *outBrcParams = *inBrcParams;

  if (static_cast<unsigned int>(bufferSizeBytes) > IPP_MAX_32U >> 3)
    bufferSizeBytes = IPP_MAX_32U >> 3;

  bufSizeBits = bufferSizeBytes << 3;
  bitsPerFrame = targetBitrate / framerate;

  maxBufSizeBits = (Ipp32u)(0xfffe * (Ipp64u)targetBitrate / 90000); // for CBR

  if (bitsPerFrame < inBrcParams->info.clip_info.width * inBrcParams->info.clip_info.height * 8 * gopFactor / (Ipp64f)500) {
    status = UMC_ERR_INVALID_PARAMS;
    bitsPerFrame = inBrcParams->info.clip_info.width * inBrcParams->info.clip_info.height * 8 * gopFactor / (Ipp64f)500;
    targetBitrate = (Ipp32s)(bitsPerFrame * framerate);
  }

  if (brcMode != BRC_CBR && brcMode != BRC_VBR) {
    status = UMC_ERR_INVALID_PARAMS;
    brcMode = BRC_VBR;
  }

  if (brcMode == BRC_VBR) {
    if (maxBitrate <= targetBitrate) {
      status = UMC_ERR_INVALID_PARAMS;
      maxBitrate = targetBitrate;
    }
    if (bufSizeBits > (Ipp32u)16384 * 0x3fffe)
      bufSizeBits = (Ipp32u)16384 * 0x3fffe;
  } else {
    if (bufSizeBits > maxBufSizeBits) {
      status = UMC_ERR_INVALID_PARAMS;
      if (maxBufSizeBits > (Ipp32u)16384 * 0x3fffe)
        maxBufSizeBits = (Ipp32u)16384 * 0x3fffe;
      if (maxBufSizeBits < (Ipp32u)(bitsPerFrame + 0.5)) { // framerate is too low for CBR
        brcMode = BRC_VBR;
        maxBitrate = targetBitrate;
      }
      if (bufFullness > maxBufSizeBits/2) { // leave mHRD.bufFullness unchanged if below mHRD.bufSize/2
        Ipp64f newBufFullness = bufFullness * maxBufSizeBits / bufSizeBits;
        if (newBufFullness < (Ipp64f)maxBufSizeBits / 2)
          newBufFullness = (Ipp64f)maxBufSizeBits / 2;
        bufFullness = newBufFullness;
      }
      bufSizeBits = maxBufSizeBits; // will be coded as (mHRD.bufSize + 16383) &~ 0x3fff
    } else {
      if (bufSizeBits > (Ipp32u)16384 * 0x3fffe) {
        status = UMC_ERR_INVALID_PARAMS;
        bufSizeBits = (Ipp32u)16384 * 0x3fffe;
      }
      if (bufFullness > (Ipp64f)bufSizeBits) {
        status = UMC_ERR_INVALID_PARAMS;
        bufFullness = bufSizeBits;
      }
    }
  }

  if (bufSizeBits < 2 * bitsPerFrame) {
    status = UMC_ERR_INVALID_PARAMS;
    bufSizeBits = (Ipp32u)(2 * bitsPerFrame);
  }

  if (brcMode == BRC_VBR)
    bufFullness = bufSizeBits; // vbv_delay = 0xffff (buffer is initially full) in case of VBR. TODO: check the possibility of VBR with vbv_delay != 0xffff

  if (bufFullness <= 0) { // not set
    status = UMC_ERR_INVALID_PARAMS;
    bufFullness = bufSizeBits / 2;
  } else if (bufFullness < bitsPerFrame) { // set but is too small
    status = UMC_ERR_INVALID_PARAMS;
    bufFullness = bitsPerFrame;
  }

  bufferSizeBytes = bufSizeBits >> 3;
  initialDelayBytes = (Ipp32u)bufFullness >> 3;

  if (outBrcParams) {
    outBrcParams->BRCMode = brcMode;
    outBrcParams->targetBitrate = targetBitrate;
    if (brcMode == BRC_VBR)
      outBrcParams->maxBitrate = maxBitrate;
    outBrcParams->HRDBufferSizeBytes = bufferSizeBytes;
    outBrcParams->HRDInitialDelayBytes = initialDelayBytes;
  }

  return status;
}

Status VideoBrc::SetPictureFlags(FrameType, Ipp32s, Ipp32s, Ipp32s, Ipp32s)
{
  return UMC_OK;
}

Status VideoBrc::GetInitialCPBRemovalDelay(Ipp32u *, Ipp32s)
{
  return UMC_OK;
}

}
