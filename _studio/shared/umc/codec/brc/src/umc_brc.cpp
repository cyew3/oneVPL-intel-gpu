//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_brc.h"

namespace UMC
{

CommonBRC::CommonBRC()
{
  mIsInit = 0;
  mBitrate = 0;
  mFramerate = 30;
  mRCMode = BRC_CBR;
  mGOPPicSize = 20;
  mGOPRefDist = 1;
  memset(&mHRD, 0, sizeof(mHRD));
  N_B_frame = 0;
  N_B = 0;
  N_P = 0;
  N_P_field = 0;
  N_B_field = 0;
  mQuantUpdated = 0;
  mBitsDesiredFrame = 0;
  N_P_frame = 0;
  brc = NULL;
  mFrameType = NONE_PICTURE;
}

CommonBRC::~CommonBRC()
{
  Close();
}

Status CommonBRC::InitHRD()
{
  Ipp32s bitsPerFrame;
  if (mFramerate <= 0)
    return UMC_ERR_INVALID_PARAMS;

  bitsPerFrame = (Ipp32s)(mBitrate / mFramerate);

  if (BRC_CBR == mRCMode)
    mParams.maxBitrate = mParams.targetBitrate;
  if (mParams.maxBitrate < mParams.targetBitrate)
    mParams.maxBitrate = 1200 * 240000; // max for H.264 level 5.1, profile <= EXTENDED

  if (mParams.HRDBufferSizeBytes <= 0)
    mParams.HRDBufferSizeBytes = IPP_MAX_32S/2/8;
  if (mParams.HRDBufferSizeBytes * 8 < (bitsPerFrame << 1))
    mParams.HRDBufferSizeBytes = (bitsPerFrame << 1) / 8;

  if (mParams.HRDInitialDelayBytes <= 0)
    mParams.HRDInitialDelayBytes = (BRC_CBR == mRCMode ? mParams.HRDBufferSizeBytes/2 : mParams.HRDBufferSizeBytes);
  if (mParams.HRDInitialDelayBytes * 8 < bitsPerFrame)
    mParams.HRDInitialDelayBytes = bitsPerFrame / 8;
  if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
    mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;

  mHRD.bufSize = mParams.HRDBufferSizeBytes * 8;
  mHRD.maxBitrate = mParams.maxBitrate;
  mHRD.bufFullness = mParams.HRDInitialDelayBytes * 8;
  mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;

  mHRD.underflowQuant = -1;
  mHRD.frameNum = 0;

  return UMC_OK;
}

Status CommonBRC::Init(BaseCodecParams *params, Ipp32s)
{
  Status status = UMC_OK;
  VideoEncoderParams *videoParams = DynamicCast<VideoEncoderParams>(params);
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);

  if (videoParams != 0) {
    if (videoParams->info.clip_info.width <= 0 || videoParams->info.clip_info.height <= 0) {
      return UMC_ERR_INVALID_PARAMS;
    }
    if (NULL != brcParams) { // BRC parameters
      mParams = *brcParams;

      mBitrate = (Ipp32u)mParams.targetBitrate;

      if (mParams.frameRateExtN && mParams.frameRateExtD)
          mFramerate = (Ipp64f) mParams.frameRateExtN /  mParams.frameRateExtD;
      else
          mFramerate = mParams.info.framerate;

      mRCMode = mParams.BRCMode;

      if (mParams.GOPPicSize > 0)
        mGOPPicSize = mParams.GOPPicSize;
      else
        mParams.GOPPicSize = mGOPPicSize;
      if (mParams.GOPRefDist > 0 && mParams.GOPRefDist < mParams.GOPPicSize)
        mGOPRefDist = mParams.GOPRefDist;
      else
        mParams.GOPRefDist = mGOPRefDist;
    } else {
      *(VideoEncoderParams*) &mParams = *videoParams;
      mBitrate = mParams.info.bitrate;
      mFramerate = mParams.info.framerate;
    }
  } else if (params != 0) {
    return UMC_ERR_INVALID_PARAMS;
//    *(BaseCodecParams*)&mParams = *params;
  } else {
    if (!mIsInit)
      return UMC_ERR_NULL_PTR;
    else {
      // reset
      return UMC_OK;
    }
  }

  if (mBitrate <= 0 || mFramerate <= 0)
    return UMC_ERR_INVALID_PARAMS;
  mBitsDesiredFrame = (Ipp32s)((Ipp64f)mBitrate / mFramerate);
  if (mBitsDesiredFrame <= 0)
    return UMC_ERR_INVALID_PARAMS;
  mIsInit = true;
  return status;
}


Status CommonBRC::GetParams(BaseCodecParams* params, Ipp32s)
{
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
  VideoEncoderParams *videoParams = DynamicCast<VideoEncoderParams>(params);
  if (NULL != brcParams) {
    *brcParams = mParams;
  } else if (NULL != videoParams) {
    *params = *(VideoEncoderParams*)&mParams;
  } else {
    *params = *(BaseCodecParams*)&mParams;
  }
  return UMC_OK;
};

Status CommonBRC::Close()
{
  Status status = UMC_OK;
  if (!mIsInit)
    return UMC_ERR_NOT_INITIALIZED;
  mIsInit = false;
  return status;
}


Status CommonBRC::PreEncFrame(FrameType, Ipp32s, Ipp32s)
{
  return UMC_OK;
}

Status CommonBRC::PostPackFrame(FrameType, Ipp32s, Ipp32s, Ipp32s, Ipp32s)
{
  return UMC_OK;
}

Ipp32s CommonBRC::GetQP(FrameType, Ipp32s)
{
  return UMC_OK;
}

Status CommonBRC::SetQP(Ipp32s, FrameType, Ipp32s)
{
  return UMC_OK;
}

BRCStatus CommonBRC::UpdateAndCheckHRD(Ipp32s frameBits, Ipp32s recode)
{
  BRCStatus ret = BRC_OK;
  Ipp64f bufFullness;

  if (!(recode & (BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
    mHRD.prevBufFullness = mHRD.bufFullness;
    mHRD.underflowQuant = -1;
  } else { // frame is being recoded - restore buffer state
    mHRD.bufFullness = mHRD.prevBufFullness;
  }

  mHRD.maxFrameSize = (Ipp32s)(mHRD.bufFullness - mHRD.roundError);
  mHRD.minFrameSize = (mRCMode == BRC_VBR ? 0 : (Ipp32s)(mHRD.bufFullness + 1 + mHRD.roundError + mHRD.inputBitsPerFrame - mHRD.bufSize));
  if (mHRD.minFrameSize < 0)
    mHRD.minFrameSize = 0;

  bufFullness = mHRD.bufFullness - frameBits;

  if (bufFullness < 1 + mHRD.roundError) {
    bufFullness = mHRD.inputBitsPerFrame;
    ret = BRC_ERR_BIG_FRAME;
    if (bufFullness > (Ipp64f)mHRD.bufSize) // possible in VBR mode if at all (???)
      bufFullness = (Ipp64f)mHRD.bufSize;
  } else {
    bufFullness += mHRD.inputBitsPerFrame;
    if (bufFullness > (Ipp64f)mHRD.bufSize - mHRD.roundError) {
      bufFullness = (Ipp64f)mHRD.bufSize - mHRD.roundError;
      if (mRCMode != BRC_VBR)
        ret = BRC_ERR_SMALL_FRAME;
    }
  }
  if (BRC_OK == ret)
    mHRD.frameNum++;
  else if ((recode & BRC_EXT_FRAMESKIP) || BRC_RECODE_EXT_PANIC == recode || BRC_RECODE_PANIC == recode) // no use in changing QP
    ret |= BRC_NOT_ENOUGH_BUFFER;

  mHRD.bufFullness = bufFullness;

  return ret;
}

/*
Status CommonBRC::ScaleRemovalDelay(Ipp64f removalDelayScale)
{
  if (removalDelayScale <= 0)
    return UMC_ERR_INVALID_PARAMS;

//  mHRD.maxInputBitsPerFrame = (Ipp32u)((mHRD.maxBitrate / mFramerate) * removalDelayScale); // == mHRD.inputBitsPerFrame for CBR
  mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame * removalDelayScale; //???

  return UMC_OK;
}
*/

Status CommonBRC::GetHRDBufferFullness(Ipp64f *hrdBufFullness, Ipp32s recode, Ipp32s)
{
  *hrdBufFullness = (recode & (BRC_EXT_FRAMESKIP - 1)) ? mHRD.prevBufFullness : mHRD.bufFullness;

  return UMC_OK;
}

void CommonBRC::SetGOP()
{
  if (mGOPRefDist < 1) {
    N_P = 0;
    N_B = 0;
  } else if (mGOPRefDist == 1) {
    N_P = mGOPPicSize - 1;
    N_B = 0;
  } else {
    N_P = (mGOPPicSize/mGOPRefDist) - 1;
    N_B = (mGOPRefDist - 1) * (mGOPPicSize/mGOPRefDist);
  }
  N_P_frame = N_P;
  N_B_frame = N_B;

  N_P_field = N_P_frame * 2 + 1; // 2nd field of I is coded as P
  N_B_field = N_B_frame * 2;
}

Status CommonBRC::GetInitialCPBRemovalDelay(Ipp32u*, Ipp32s)
{
  return UMC_OK;
}

}

