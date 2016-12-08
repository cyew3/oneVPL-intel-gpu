//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include "umc_mpeg2_brc.h"
#include "umc_video_data.h"

namespace UMC
{

static Ipp32s Val_QScale[2][32] =
{
  /* linear q_scale */
  {0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62},
  /* non-linear q_scale */
  {0, 1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22,
  24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96,104,112}
};


#if APA_MPEG2_BRC

static Ipp32s QQuality_AddI[N_QUALITY_LEVELS][N_QP_THRESHLDS+1] = {
    {0,0,0,0,0},
      {0,0,0,0,-1},
      {0,0,0,-1,-2},
      {0,0,-1,-2,-3},
      {0,-1,-2,-3,-4},
      {-1,-2,-3,-4,-5},
      {-2,-3,-4,-5,-6}
};

static Ipp32s QQuality_AddP[N_QUALITY_LEVELS][N_QP_THRESHLDS+1] = {
    {0,0,0,0,0},
      {0,0,0,0,0},
      {0,0,0,0,-1},
      {0,0,0,-1,-2},
      {0,0,-1,-2,-3},
      {0,1,-2,-3,-4},
      {-1,-2,-3,-4,-5}
};

static Ipp32s delta_qp[][N_INST_RATE_THRESHLDS+1][N_QP_THRESHLDS+1]={

  {{1,1,1,0,0,},
     {1,2,1,0,0,},
   {1,2,3,3,4,},
     {3,4,4,5,5,},
   {4,5,5,6,7,},},

  {{1,0,0,0,-1,},
     {1,1,0,0,-1,},
   {1,1,2,2,2,},
     {2,2,3,4,5,},
   {2,3,4,5,6,},},

  {{0,0,0,0,-1,},
     {1,0,0,-1,-1,},
   {1,1,1,2,2,},
     {1,2,3,4,4,},
   {2,2,3,4,4,},},

  {{0,0,0,-1,-1,},
     {0,-1,-1,-1,-1,},
   {0,0,0,0,-1,},
     {1,1,1,0,0,},
   {2,3,3,3,4,},},
// normal
  {{-1,-1,-2,-2,-3,},
     {0,-1,-1,-2,-2,},
   {0,0,0,-1,-2,},
     {1,1,0,0,0,},
   {1,2,2,3,3,},},

  {{-1,-2,-3,-3,-4,},
     {0,-1,-2,-2,-2,},
   {0,0,-1,-1,-2,},
     {1,1,0,0,-1,},
   {1,2,2,2,3,},},

  {{-2,-3,-3,-4,-4,},
     {-1,-1,-2,-3,-3,},
     {0,-1,-2,-2,-2,},
     {1,0,0,-1,-1,},
   {1,1,2,2,2,},},

  {{-2,-3,-4,-4,-5,},
     {-1,-2,-3,-3,-4,},
     {0,-1,-2,-3,-3,},
     {0,0,0,-1,-1,},
   {0,1,1,1,1,},},

  {{-2,-3,-4,-5,-7,},
     {-1,-2,-3,-4,-5,},
     {0,-1,-2,-3,-4,},
     {0,-1,-2,-2,-2,},
   {0,0,-1,-2,-3,},},

};

#define Q_THRESHOLD1 5
#define Q_THRESHOLD2 11
#define Q_THRESHOLD3 22
#define Q_THRESHOLD4 44

static Ipp32s qp_thresh[N_QP_THRESHLDS] = {
  Q_THRESHOLD1,
  Q_THRESHOLD2,
  Q_THRESHOLD3,
  Q_THRESHOLD4
};

static Ipp64f inst_rate_thresh[N_INST_RATE_THRESHLDS] = {
    0.3, 0.6, 0.9, 1.25,
};


static Ipp64f dev_thresh[N_DEV_THRESHLDS] = {
   -0.45, -0.33, -0.2, -0.1, 0.1, 0.2, 0.3, 0.4,
};

#define HRD_DELTA 256


#define FIND_INDEX_SHORT(ptr, size, value, index) { \
  for (index = 0; index < size; index++) { \
    if (ptr[index] >= value) \
      break; \
  } \
}

#define FIND_INDEX_MED(ptr, size, value, index) { \
  if (ptr[(size >> 1) - 1] < value) { \
    for (index = size >> 1; index < size; index++) { \
      if (ptr[index] >= value) \
        break; \
    } \
  } else { \
    for (index = (size >> 1) - 2; index >= 0; index--) { \
      if (ptr[index] < value) \
       break; \
    } \
    index++; \
  } \
}

//size should be even!
#define FIND_INDEX(ptr,size,value,index) { \
  Ipp32s delta, test; delta = test = size >> 1; test--;   \
  while (delta > 1) {    \
    delta >>= 1;         \
    if (ptr[test] < value) test += delta; \
    else test -= delta;  \
  } \
  if (ptr[test] < value) test++; \
  if (ptr[test] < value) test++; \
  index = test; \
}

#endif

MPEG2BRC::MPEG2BRC()
{
  mIsInit = 0;
  prqscale[0] = prqscale[1] = prqscale[2] = 0;
  prsize[0]   = prsize[1]   = prsize[2]   = 0;
  quantiser_scale_value = -1;
  mQuantMin = 0;
  q_scale_type = 0;
  full_hw = false;
  prev_frame_type = 0;
  mQuantMax = 0;
  mIsFallBack = 0;
  rc_dev = 0;
  quantiser_scale_code = 0;
  gopw = 0;
  picture_flags_IP = 0;
  block_count = 0;
  rc_dev_saved = 0;
  mQualityLevel = 0;
  picture_flags = 0;
  picture_flags_prev = 0;
}

MPEG2BRC::~MPEG2BRC()
{
  Close();
}

Status MPEG2BRC::CheckHRDParams()
{
  mParams.maxBitrate = (mParams.maxBitrate / 400) * 400;  // In MPEG2 bitrate is coded in units of 400 bits
  mHRD.maxBitrate = mParams.maxBitrate;
  if ((Ipp32s)mBitrate > mParams.maxBitrate)
    mBitrate = mParams.targetBitrate = mParams.maxBitrate;
  mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame = mHRD.maxBitrate / mFramerate;

  if (BRC_VBR == mRCMode) {
    if (mHRD.bufSize > (Ipp32u)16384 * 0x3fffe)
      mHRD.bufSize = (Ipp32u)16384 * 0x3fffe;
    if(!full_hw)
    //allow initial delay to be different from buffer size
    mHRD.bufFullness = mHRD.bufSize; // vbv_delay = 0xffff in case of VBR. TODO: check the possibility of VBR with vbv_delay != 0xffff
  } else { // BRC_CBR
    Ipp32u max_buf_size = (Ipp32u)(0xfffe * (Ipp64u)mHRD.maxBitrate / 90000); // vbv_delay is coded with 16 bits:
                                                                              //  it is either 0xffff everywhere (VBR) or < 0xffff
    if (mHRD.bufSize > max_buf_size) {
      if (max_buf_size > (Ipp32u)16384 * 0x3fffe)
        max_buf_size = (Ipp32u)16384 * 0x3fffe;
      if (mHRD.bufFullness > max_buf_size/2) { // leave mHRD.bufFullness unchanged if below mHRD.bufSize/2
        Ipp64f newBufFullness = mHRD.bufFullness * max_buf_size / mHRD.bufSize;
        if (newBufFullness < (Ipp64f)max_buf_size / 2)
          newBufFullness = (Ipp64f)max_buf_size / 2;
        mHRD.bufFullness = newBufFullness;
      }
      mHRD.bufSize = max_buf_size;
    } else {
      if (mHRD.bufSize > (Ipp32u)16384 * 0x3fffe)
        mHRD.bufSize = (Ipp32u)16384 * 0x3fffe;
      if (mHRD.bufFullness > (Ipp64f)mHRD.bufSize)
        mHRD.bufFullness = (Ipp64f)mHRD.bufSize;
    }
  }
  mParams.HRDBufferSizeBytes = mHRD.bufSize / 8;
  mHRD.bufSize = mParams.HRDBufferSizeBytes * 8;
  mParams.HRDInitialDelayBytes = (Ipp32s)(mHRD.bufFullness / 8);
  mHRD.bufFullness = mParams.HRDInitialDelayBytes * 8;

  if (mHRD.bufSize < mHRD.inputBitsPerFrame)
    return UMC_ERR_INVALID_PARAMS;

  return UMC_OK;
}

Status MPEG2BRC::Init(BaseCodecParams *params, Ipp32s no_full_HW)
{
  Status status = UMC_OK;
  Ipp64f u_len;
  full_hw = (no_full_HW == 0);
  status = CommonBRC::Init(params);
  if (status != UMC_OK)
    return status;
  status = InitHRD();
  if (status != UMC_OK)
    return status;
  CheckHRDParams();
  mHRD.roundError = mHRD.maxBitrate / 90000;

  if (mBitrate <= 0 || mFramerate <= 0)
    return UMC_ERR_INVALID_PARAMS;

  Ipp64f ppb; //pixels per bit (~ density)
  Ipp32s i;

  mBitsDesiredFrame = (Ipp32s)((Ipp64f)mBitrate / mFramerate);

  // one can vary weights, can be added to API
  rc_weight[0] = 120;
  rc_weight[1] = 50;
  rc_weight[2] = 25;

  rc_dev = rc_dev_saved = 0; // deviation from ideal bitrate (should be Ipp32f or renewed)

  SetGOP();

  gopw = N_B_frame * rc_weight[2] + rc_weight[1] * N_P_frame + rc_weight[0];
  u_len = mBitsDesiredFrame * mGOPPicSize / gopw;
  rc_tagsize[0] = rc_tagsize_frame[0] = u_len * rc_weight[0];
  rc_tagsize[1] = rc_tagsize_frame[1] = u_len * rc_weight[1];
  rc_tagsize[2] = rc_tagsize_frame[2] = u_len * rc_weight[2];

  Ipp64f rrel = gopw / (rc_weight[0] * mGOPPicSize);
  block_count = (mParams.info.color_format == YUV444 ? 12 : (mParams.info.color_format == YUV422 ? 8 : 6));
  ppb = mParams.info.clip_info.width * mParams.info.clip_info.height * mFramerate / mBitrate * (block_count-2) / (6-2);

  qscale[0] = (Ipp32s)(6.0 * rrel * ppb); // numbers are empiric
  qscale[1] = (Ipp32s)(9.0 * rrel * ppb);
  qscale[2] = (Ipp32s)(12.0 * rrel * ppb);

  for (i = 0; i < 3; i++) {
    if (qscale[i] < 1)
      qscale[i] = 1;
    else if (qscale[i] > 63)
      qscale[i] = 63; // can be even more
    if (prqscale[i] == 0) prqscale[i] = qscale[i];
  }

  gopw = N_B_field * rc_weight[2] + rc_weight[1] * N_P_field + rc_weight[0];
  u_len = (Ipp64f)mBitsDesiredFrame * mGOPPicSize / gopw;
  rc_tagsize_field[0] = u_len * rc_weight[0];
  rc_tagsize_field[1] = u_len * rc_weight[1];
  rc_tagsize_field[2] = u_len * rc_weight[2];

#if APA_MPEG2_BRC
  mIsFallBack = 0;
  Ipp64f orig_frame_size = mParams.info.clip_info.width * mParams.info.clip_info.height *
    (mParams.info.color_format == YUV444 ? 3. : (mParams.info.color_format == YUV422 ? 2. : 1.5))*8;

  if ( mHRD.inputBitsPerFrame != 0 ){
    if ( orig_frame_size/mHRD.inputBitsPerFrame > 100) mIsFallBack = 1;
    if ( orig_frame_size/mHRD.inputBitsPerFrame < 10) mIsFallBack = 1;
  }


  mQualityLevel=5;

  if (mGOPRefDist < 1)
    mQualityLevel = 0;
  else if (mGOPRefDist == 1)
    mQualityLevel = 2;
  else {
    mQualityLevel = 2 + mGOPRefDist;
    if (mQualityLevel > (N_QUALITY_LEVELS-1)) mQualityLevel = (N_QUALITY_LEVELS-1);
  }


  /* calculating instant bitrate thresholds */
  for (i = 0; i < N_INST_RATE_THRESHLDS; i++) {
    instant_rate_thresholds[i]=((Ipp64f)mBitrate*inst_rate_thresh[i]);
  }

  /* calculating deviation from ideal fullness/bitrate thresholds */
  for (i = 0; i < N_DEV_THRESHLDS; i++) {
    deviation_thresholds[i]=((Ipp64f)mHRD.bufSize*dev_thresh[i]);
  }

  mQuantMin=1;
  mQuantMax=112;

#endif

  for (i = 0; i < 3; i++) { // just in case - will be set in PreEnc for (frameNum == 0)
    prpicture_flags[i] = BRC_FRAME;
    prsize[i] = (Ipp32s)rc_tagsize_frame[i];
  }
  picture_flags_prev = picture_flags_IP = picture_flags = BRC_FRAME;

  mIsInit = true;
  return status;
}


Status MPEG2BRC::Reset(BaseCodecParams *init, Ipp32s)
{
  return Init(init);
}


Status MPEG2BRC::Close()
{
  Status status = UMC_OK;
  if (!mIsInit)
    return UMC_ERR_NOT_INITIALIZED;

  mIsInit = false;
  return status;
}


Status MPEG2BRC::SetParams(BaseCodecParams* params, Ipp32s)
{
  return Init(params);
}


Status MPEG2BRC::SetPictureFlags(FrameType frameType,
                                 Ipp32s picture_structure,
                                 Ipp32s repeat_first_field,
                                 Ipp32s top_field_first,
                                 Ipp32s second_field)
{
  if (frameType != B_PICTURE) {
    picture_flags_prev = picture_flags_IP;
    picture_flags_IP = picture_flags;
  }
  switch (picture_structure & PS_FRAME) {
  case (PS_TOP_FIELD):
    picture_flags = BRC_TOP_FIELD;
    break;
  case (PS_BOTTOM_FIELD):
    picture_flags = BRC_BOTTOM_FIELD;
    break;
  case (PS_FRAME):
  default:
    picture_flags = BRC_FRAME;
  }
  picture_flags |= (repeat_first_field ? BRC_REPEAT_FIRST_FIELD : 0);
  picture_flags |= (top_field_first ? BRC_TOP_FIELD_FIRST : 0);
  picture_flags |= (second_field ? BRC_SECOND_FIELD : 0);
  return UMC_OK;
}

Ipp32s MPEG2BRC::ChangeQuant(Ipp32s quant_value)
{
  Ipp32s curq = quantiser_scale_value;

  if (quant_value == quantiser_scale_value)
    return quantiser_scale_value;
  if (quant_value > 7 && quant_value <= 62) {
    q_scale_type = 0;
    quantiser_scale_code = (quant_value + 1) >> 1;
  } else { // non-linear quantizer
    q_scale_type = 1;
    if (quant_value <= 8)
      quantiser_scale_code = quant_value;
    else if (quant_value > 62)
      quantiser_scale_code = 25 + ((quant_value - 64 + 4) >> 3);
  }
  if (quantiser_scale_code < 1)
    quantiser_scale_code = 1;
  if (quantiser_scale_code > 31)
    quantiser_scale_code = 31;

  quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];

  if (quantiser_scale_value == curq) {
    if (quant_value > curq)
    {
      if (quantiser_scale_code == 31)
        return quantiser_scale_value;
      else
        quantiser_scale_code++;
    }
    if (quant_value < curq)
    {
      if (quantiser_scale_code == 1)
        return quantiser_scale_value;
      else
        quantiser_scale_code--;
    }
    quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];
  }

  return quantiser_scale_value;
}

#if APA_MPEG2_BRC

Status MPEG2BRC::PreEncFrame(FrameType frameType, Ipp32s recode, Ipp32s)
{
  Ipp32s isfield = ((picture_flags & BRC_FRAME) != BRC_FRAME);
  Ipp32s i;

  if (recode & BRC_EXT_FRAMESKIP)
    return UMC_OK;

  if (frameType == I_PICTURE) {
    if (isfield) {
      N_P = N_P_field;
      N_B = N_B_field;
      for (i = 0; i < 3; i++)
        rc_tagsize[i] = rc_tagsize_field[i];
    } else {
      N_P = N_P_frame;
      N_B = N_B_frame;
      for (i = 0; i < 3; i++)
        rc_tagsize[i] = rc_tagsize_frame[i];
    }

    if (mHRD.frameNum == 0) { // first frame
      for (i = 0; i < 3; i++) {
        if (prsize[i] == 0) prsize[i] = (Ipp32s)rc_tagsize[i];
        prpicture_flags[i] = picture_flags;
      }
    }
  }

  if (mIsFallBack) return PreEncFrameFallBack(frameType,recode);
  else return PreEncFrameMidRange(frameType,recode);
}

Status MPEG2BRC::PreEncFrameMidRange(FrameType frameType, Ipp32s recode)
{
  Ipp32s q0, q1;
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
  Ipp32s (*qp_delta_ptr)[5][5] = &delta_qp[0];
  Ipp32s i,j,t;
  Status status = UMC_OK;

  if (recode < BRC_RECODE_EXT_QP) // skip in case of external recode
  if (frameType == I_PICTURE && mRCMode == BRC_CBR && mHRD.frameNum) {
    Ipp64f ip_tagsize = rc_tagsize[0];
    Ipp64f dev, adev, arc_dev;

/*
    if (BRC_FRAME != (picture_flags & 0x3))
      ip_tagsize = (rc_tagsize[0] + rc_tagsize[1]) * .5; // every second I-field became P-field
*/

    if (mHRD.bufFullness < 2 * ip_tagsize || (Ipp64f)mHRD.bufSize -  mHRD.bufFullness < mHRD.maxInputBitsPerFrame) {
      dev = mHRD.bufSize / 2 // half of vbv_buffer
        + ip_tagsize / 2                      // top to center length of I (or IP) frame
        - mHRD.bufFullness;
      if (dev * rc_dev < 0) {
        BRC_CLIP(dev, -mHRD.maxInputBitsPerFrame, ip_tagsize);
        rc_dev = dev;
      } else {
        adev = BRC_ABS(dev);
        arc_dev = BRC_ABS(rc_dev);
        if (adev > arc_dev) {
          BRC_CLIP(dev, -arc_dev - mHRD.maxInputBitsPerFrame, arc_dev + ip_tagsize);
          rc_dev = dev;
        }
      }
    }
  }

  if (recode > BRC_RECODE_NONE) {
    if (mFrameType != frameType) { // recoding due to frame type change
      Ipp32s prevIndx = (mFrameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
      qscale[prevIndx] = ChangeQuant(prqscale[prevIndx]);
    }
    return status;
  }

  q0 = qscale[indx];

  //I-frame start QP
  if (indx == 0) {
    if (q0 < Q_THRESHOLD1) {
      if (N_B > 0 && N_P > 0)
        q0 = (5*qscale[0]+qscale[0]+qscale[1]+qscale[2])>>3;
      else if (N_P > 0)
        q0 = (2*qscale[0]+qscale[0]+qscale[1])>>2;
    } else if (q0 < Q_THRESHOLD2 && N_P > 0)
      q0 = (2*qscale[0]+qscale[0]+qscale[1])>>2;

    if (q0 < Q_THRESHOLD1) {
      if (N_P > 0 && qscale[0] >= qscale[1])
        q0 = qscale[1]-1;
      if (N_B > 0 && qscale[0] >= qscale[2]){
        q0 = qscale[2]-1;
      }
    } else {
      if (N_P > 0 && qscale[0] >= qscale[1])
        q0 = (int)(0.8*(double)qscale[1]);
      if (N_B > 0 && qscale[0] >= qscale[2]){
        q0 = (int)(0.8*(double)qscale[2]);
      }
    }
  }

  //P-frames start QP
  if (indx == 1) {
    if (N_B > 0) {
      q0 = (5*qscale[1]+qscale[0]+qscale[1]+qscale[2])>>3;
      if (q0 > Q_THRESHOLD4)  {
        if (q0 > qscale[2] + 3)
          q0 -= 4;
      } else if (q0 > Q_THRESHOLD3) {
        if (q0 > qscale[2] + 2)
          q0 -= 3;
      } else if (q0 > Q_THRESHOLD2) {
        if (q0 > qscale[2] + 1)
          q0 -= 2;
      } else {
        if (q0 > qscale[2] + 0)
          q0 -= 1;
      }
    } else { // only I & P frames
      if (mHRD.bufFullness > prsize[1]*2 && mHRD.bufFullness > prsize[0]*1.5)
        q0 = (2*qscale[1]+qscale[0]+qscale[1])>>2;
    }

    if (q0 < qscale[0])
       q0++;
  }

    //B-frames start QP
  if (indx == 2) {

    if (mHRD.bufFullness > prsize[2]*2 && mHRD.bufFullness > prsize[1]*1.5)
      q0 = (2*qscale[2]+qscale[2]+qscale[1])>>2;

    if (q0 < Q_THRESHOLD2) {
      if (q0 < qscale[0])
        q0++;
    }
    if (q0 < qscale[1])
      q0++;
  }

  Ipp64f inst_bitrate = (((Ipp64f)prsize[0]+(Ipp64f)prsize[1]*N_P+(Ipp64f)prsize[2]*N_B)*mFramerate)/(Ipp64f)mGOPPicSize;
  Ipp64f deviation;

  if (mRCMode == BRC_CBR)
    deviation = (Ipp64f)mHRD.bufFullness - (Ipp64f)mHRD.bufSize/2;
  else
//    deviation = (Ipp64f)mHRD.bufFullness - (Ipp64f)mParams.HRDInitialDelayBytes*8;
    deviation = (Ipp64f)mHRD.bufFullness - (Ipp64f)mHRD.bufSize; // initial fullness = bufsize for VBR


  FIND_INDEX_MED(deviation_thresholds,N_DEV_THRESHLDS,deviation,t);
  qp_delta_ptr = &delta_qp[t];

  FIND_INDEX_SHORT(instant_rate_thresholds,N_INST_RATE_THRESHLDS,inst_bitrate,i);
  FIND_INDEX_SHORT(qp_thresh,N_QP_THRESHLDS,q0,j);
  q0 += (*qp_delta_ptr)[i][j];
  if (indx == 2 ) {
    if ( q0 < 2) q0++;
  } else {
    FIND_INDEX_SHORT(qp_thresh,N_QP_THRESHLDS,q0,j);
    if (indx == 0)
      q0 += QQuality_AddI[mQualityLevel][j];
    else if (indx == 1)
      q0 += QQuality_AddP[mQualityLevel][j];
  }

  //clip qp to a valid value
  BRC_CLIP(q0,mQuantMin,mQuantMax);

  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about changing scale code if value changes

  i = 0;
  int act_delta_qp = q0-qscale[indx];
  q1 = ChangeQuant(q0);
 //enforcing to change QP in the direction of sign of act_delta_qp
  if (act_delta_qp != 0){
    while ( i < 10 && q1 == qscale[indx] ){
      if (act_delta_qp < 0) act_delta_qp--;
      else act_delta_qp++;
      q0 = qscale[indx] + act_delta_qp;
      q1 = ChangeQuant(q0);
      i++;
    }
  }

  qscale[indx] = q1;

  return status;
}

Status MPEG2BRC::PreEncFrameFallBack(FrameType frameType, Ipp32s recode)
{
  Ipp32s q0, q2, sz1;
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
  Ipp64f target_size;
  Ipp32s wanted_size;
  Status status = UMC_OK;

  // refresh rate deviation with every new I frame
  if (recode < BRC_RECODE_EXT_QP) // skip in case of external recode
  if (frameType == I_PICTURE && mRCMode == BRC_CBR && mHRD.frameNum) {
    Ipp64f ip_tagsize = rc_tagsize[0];
    Ipp64f dev, adev, arc_dev;

/*
    if (BRC_FRAME != (picture_flags & 0x3))
      ip_tagsize = (rc_tagsize[0] + rc_tagsize[1]) * .5; // every second I-field became P-field
*/

    if (mHRD.bufFullness < 2 * ip_tagsize || (Ipp64f)mHRD.bufSize -  mHRD.bufFullness < mHRD.maxInputBitsPerFrame ) {
      dev = mHRD.bufSize / 2 // half of vbv_buffer
        + ip_tagsize / 2                      // top to center length of I (or IP) frame
        - mHRD.bufFullness;
      if (dev * rc_dev < 0) {
        BRC_CLIP(dev, -mHRD.maxInputBitsPerFrame, ip_tagsize);
        rc_dev = dev;
      } else {
        adev = BRC_ABS(dev);
        arc_dev = BRC_ABS(rc_dev);
        if (adev > arc_dev) {
          BRC_CLIP(dev, -arc_dev - mHRD.maxInputBitsPerFrame, arc_dev + ip_tagsize);
          rc_dev = dev;
        }
      }
    }
  }

  if (recode > BRC_RECODE_NONE) {
    if (mFrameType != frameType) { // recoding due to frame type change
      Ipp32s prevIndx = (mFrameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
      qscale[prevIndx] = ChangeQuant(prqscale[prevIndx]);
    }
    return status;
  }

  q0 = qscale[indx];   // proposed from post picture
  q2 = prqscale[indx]; // last used scale
  sz1 = prsize[indx];  // last coded size

  target_size = rc_tagsize[indx];
  wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);

  if (sz1 > 2*wanted_size)
    q2 = q2 * 3 / 2 + 1;
  else if (sz1 > wanted_size + 100)
    q2++;
  else if (2*sz1 < wanted_size)
    q2 = q2 * 3 / 4;
  else if (sz1 < wanted_size-100 && q2 > 2)
    q2--;

  if (rc_dev > 0) {
    q2 = IPP_MAX(q0,q2);
  } else {
    q2 = IPP_MIN(q0,q2);
  }
  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about changing scale code if value changes
  q2 = ChangeQuant(q2);

  qscale[indx] = q2;

  return status;
}

#else

Status MPEG2BRC::PreEncFrame(FrameType frameType, Ipp32s recode)
{
  Ipp32s q0, q2, sz1;
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
  Ipp64f target_size;
  Ipp32s wanted_size;
  Status status = UMC_OK;

  // refresh rate deviation with every new I frame
  if (frameType == I_PICTURE && mRCMode == BRC_CBR && mHRD.frameNum) {
    Ipp64f ip_tagsize = rc_tagsize[0];
    Ipp64f dev, adev, arc_dev;

    if (BRC_FRAME != (picture_flags & 0x3))
      ip_tagsize = (rc_tagsize[0] + rc_tagsize[1]) * .5; // every second I-field became P-field

    if (mHRD.bufFullness < 2 * ip_tagsize || (Ipp64f)mHRD.bufSize -  mHRD.bufFullness < mHRD.maxInputBitsPerFrame ) {
      dev = mHRD.bufSize / 2 // half of vbv_buffer
        + ip_tagsize / 2                      // top to center length of I (or IP) frame
        - mHRD.bufFullness;
      if (dev * rc_dev < 0) {
        BRC_CLIP(dev, -mHRD.maxInputBitsPerFrame, ip_tagsize);
        rc_dev = dev;
      } else {
        adev = BRC_ABS(dev);
        arc_dev = BRC_ABS(rc_dev);
        if (adev > arc_dev) {
          BRC_CLIP(dev, -arc_dev - mHRD.maxInputBitsPerFrame, arc_dev + ip_tagsize);
          rc_dev = dev;
        }
      }
    }
  }

  if (recode) {
    if (mFrameType != frameType) { // recoding due to frame type change
      Ipp32s prevIndx = (mFrameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
      qscale[prevIndx] = ChangeQuant(prqscale[prevIndx]);
    }
    return status;
  }

  q0 = qscale[indx];   // proposed from post picture
  q2 = prqscale[indx]; // last used scale
  sz1 = prsize[indx];  // last coded size

  target_size = rc_tagsize[indx];
  wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);

  if (sz1 > 2*wanted_size)
    q2 = q2 * 3 / 2 + 1;
  else if (sz1 > wanted_size + 100)
    q2++;
  else if (2*sz1 < wanted_size)
    q2 = q2 * 3 / 4;
  else if (sz1 < wanted_size-100 && q2 > 2)
    q2--;

  if (rc_dev > 0) {
    q2 = IPP_MAX(q0,q2);
  } else {
    q2 = IPP_MIN(q0,q2);
  }
  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about changing scale code if value changes
  q2 = ChangeQuant(q2);

  qscale[indx] = q2;

  return status;
}



#endif


Ipp32s MPEG2BRC::GetQP(FrameType frameType, Ipp32s)
{
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
  return qscale[indx];
}
Status MPEG2BRC::SetQP(Ipp32s qp, FrameType frameType, Ipp32s)
{
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));
  qscale[indx] = qp;
  return UMC_OK;
}

BRCStatus MPEG2BRC::PostPackFrame(FrameType frameType, Ipp32s bits_encoded, Ipp32s, Ipp32s repack, Ipp32s)
{
  Ipp32s isfield = ((picture_flags & BRC_FRAME) != BRC_FRAME);
  Ipp32u picFlags = (frameType == B_PICTURE ? picture_flags : picture_flags_prev);
  Ipp32s Sts = BRC_OK;
  Ipp32s indx = (frameType == B_PICTURE ? 2 : (frameType == P_PICTURE ? 1 : 0));

  mFrameType = frameType;

  if (mParams.info.interlace_type == PROGRESSIVE) {
    mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame;
    if (picFlags & BRC_REPEAT_FIRST_FIELD) {
      mHRD.inputBitsPerFrame += mHRD.maxInputBitsPerFrame;
      if (picFlags & BRC_TOP_FIELD_FIRST)
        mHRD.inputBitsPerFrame += mHRD.maxInputBitsPerFrame;
    }
  } else {
    mHRD.inputBitsPerFrame = mHRD.maxInputBitsPerFrame / 2;
    if (!isfield) {
      if (picFlags & BRC_REPEAT_FIRST_FIELD)
        mHRD.inputBitsPerFrame *= 3;
      else
        mHRD.inputBitsPerFrame *= 2;
    } else if (frameType != B_PICTURE) {
      if ((picture_flags & BRC_SECOND_FIELD) && ((picture_flags_prev & BRC_FRAME) == BRC_FRAME) && (picture_flags_prev & BRC_REPEAT_FIRST_FIELD))
        mHRD.inputBitsPerFrame *= 2;
    }
  }

//  prsize[indx] = (Ipp32s)(bits_encoded << isfield);
  prsize[indx] = bits_encoded;
  prqscale[indx] = qscale[indx];

  prpicture_flags[indx] = picture_flags;

  Sts = UpdateAndCheckHRD(bits_encoded, repack);

  if ((repack & BRC_EXT_FRAMESKIP) || BRC_RECODE_EXT_PANIC == repack || BRC_RECODE_PANIC == repack) // no use in changing QP
    return Sts;

  if (Sts != BRC_OK) {
    Sts = UpdateQuantHRD(bits_encoded, Sts);
    return Sts;
  }

#if APA_MPEG2_BRC

  if (!mIsFallBack) {
    if (qscale[indx] < Q_THRESHOLD1) {
      if (bits_encoded > (int)(4*(int)mHRD.inputBitsPerFrame) ){
        Sts = UpdateQuantHRD(bits_encoded, BRC_ERR_BIG_FRAME);
        mHRD.frameNum--;
        if (Sts & BRC_NOT_ENOUGH_BUFFER)
          Sts = BRC_OK;
        else
          return Sts;
      }
    }

    else if (indx == 0) { //(qscale[indx] < Q_THRESHOLD2) {
      if (bits_encoded > (int)(8*(int)mHRD.inputBitsPerFrame) ){
        Sts = UpdateQuantHRD(bits_encoded, BRC_ERR_BIG_FRAME);
        mHRD.frameNum--;
        if (Sts & BRC_NOT_ENOUGH_BUFFER)
          Sts = BRC_OK;
        else
          return Sts;
      }
    }
  }
/*
  else {
    if (bits_encoded > (int)(4*(int)mHRD.bufFullness) ){
      Sts = UpdateQuantHRD(bits_encoded, BRC_ERR_BIG_FRAME);
      mHRD.frameNum--;
      if (Sts & BRC_NOT_ENOUGH_BUFFER)
        Sts = BRC_OK;
      else
        return Sts;
    }
  }
*/

#endif
/*
  rc_vbv_fullness = rc_vbv_fullness - bits_encoded;
  rc_vbv_fullness += rc_delay; //
  if(encodeInfo.rc_mode != RC_CBR && rc_vbv_fullness >= vbv_size) {
    rc_vbv_fullness = (Ipp64f) vbv_size;
    vbv_delay = 0xffff; // input
  }

  if(encodeInfo.rc_mode != RC_CBR)
    return 0;
*/

  if (repack > BRC_RECODE_PANIC) // recoding decision made outside BRC
    rc_dev = rc_dev_saved;

  Ipp32s cur_qscale = qscale[indx];
  Ipp64f target_size = rc_tagsize[indx];
//  rc_dev += bits_encoded - (isfield ? target_size/2 : target_size);
  rc_dev_saved = rc_dev;
  rc_dev += bits_encoded - target_size;

//  if (repack == BRC_RECODE_PANIC || repack == BRC_RECODE_EXT_PANIC) // do not change QP based on panic data
//    return Sts;

  Ipp32s wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);
  Ipp32s newscale;
  Ipp32s del_sc;

  newscale = cur_qscale;
//  wanted_size >>= isfield;
  if (bits_encoded > wanted_size)
    newscale++;
  if (bits_encoded > 2*wanted_size)
    newscale = cur_qscale * 3 / 2 + 1;
  if (bits_encoded < wanted_size && cur_qscale>2)
    newscale--;
  if (2*bits_encoded < wanted_size)
    newscale = cur_qscale * 3 / 4;

  if (repack == BRC_RECODE_EXT_QP) {
    if (newscale > cur_qscale) // don't decrease QP in case of external QP
        qscale[indx] = ChangeQuant(newscale);
    return Sts;
  }

  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about to change scale code if value changes
  newscale = ChangeQuant(newscale);

#if APA_MPEG2_BRC
  if (mIsFallBack) {
#endif
    if (frameType == I_PICTURE) {
      if (newscale + 1 > qscale[1])
        qscale[1] = newscale+1;
      if (newscale + 2 > qscale[2])
        qscale[2] = newscale+2;
    } else if (frameType == P_PICTURE) {
      if (newscale < qscale[0]) {
        del_sc = qscale[0] - newscale;
        qscale[0] -= del_sc/2;
        newscale = qscale[0];
        newscale = ChangeQuant(newscale);
      }
      if (newscale+1 > qscale[2])
        qscale[2] = newscale + 1;
    } else {
      if (newscale < qscale[1]) {
        del_sc = qscale[1] - newscale;
        qscale[1] -= del_sc/2;
        newscale = qscale[1];
        newscale = ChangeQuant(newscale);
        if( qscale[1] < qscale[0])
          qscale[0] = qscale[1];
      }
    }
#if APA_MPEG2_BRC
  } else {
    if (frameType == P_PICTURE) {
      if (newscale < qscale[0]) {
        del_sc = qscale[0] - newscale;
        newscale = qscale[0] - del_sc/2;
        newscale = ChangeQuant(newscale);
      }
    } else if (frameType == B_PICTURE) {
      if (newscale < qscale[1]) {
        del_sc = qscale[1] - newscale;
        newscale = qscale[1] - del_sc/2;
        newscale = ChangeQuant(newscale);
      }
    }
  }
#endif


  qscale[indx] = newscale;

  return Sts;
}

BRCStatus MPEG2BRC::UpdateQuantHRD(Ipp32s bits_encoded, BRCStatus sts)
{
  Ipp32s quant, quant_prev;
  Ipp32s indx = (mFrameType == B_PICTURE ? 2 : (mFrameType == P_PICTURE ? 1 : 0));
  Ipp64f qs;
  Ipp32s wantedBits = (sts == BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize : mHRD.minFrameSize);
  if (wantedBits == 0) // KW-inspired safety check
  {
      wantedBits = 1;
  }

  quant = qscale[indx];
  quant_prev = quant;

  if (sts & BRC_ERR_BIG_FRAME)
    mHRD.underflowQuant = quant;

  qs = (Ipp64f)bits_encoded/wantedBits;
  quant = (Ipp32s)(quant * qs + 0.5);

  if (quant == quant_prev)
    quant += (sts == BRC_ERR_BIG_FRAME ? 1 : -1);

  if (quant < quant_prev) {
    if (quant <= mHRD.underflowQuant)
      quant = mHRD.underflowQuant + 1;
  }
  quant = ChangeQuant(quant);

  if (quant == quant_prev || quant <= mHRD.underflowQuant) {
    return (sts | BRC_NOT_ENOUGH_BUFFER);
  }

  qscale[indx] = quant;
  return sts;
}
}

