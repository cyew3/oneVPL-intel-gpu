//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#ifndef _UMC_SVC_BRC_H_
#define _UMC_SVC_BRC_H_

#include "umc_h264_video_encoder.h"
#ifndef _UMC_VIDEO_BRC_H_
#include "umc_video_brc.h"
#endif

/*
typedef struct _BRCSVC_Params
{
} BRCSVC_Params;
*/

#define BRC_CLIP(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define BRC_CLIPL(a, l) if (a < (l)) a = l
#define BRC_CLIPR(a, r) if (a > (r)) a = r
#define BRC_ABS(a) ((a) >= 0 ? (a) : -(a))

namespace UMC
{

typedef struct _BRCSVC_HRD_State
{
    Ipp32u bufSize;
    Ipp64f bufFullness;
    Ipp64f prevBufFullness;
    Ipp64f maxBitrate;
    Ipp64f inputBitsPerFrame;
    Ipp64f maxInputBitsPerFrame;
    Ipp64f minBufFullness;
    Ipp64f maxBufFullness;
    Ipp32s frameNum;
    Ipp32s minFrameSize;
    Ipp32s maxFrameSize;
    Ipp64s mBF;
    Ipp64s mBFsaved;
} BRCSVC_HRDState;

typedef struct _BRC_SVCLayer_State
{
    Ipp32s  mBitsDesiredFrame;
    Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;
    Ipp32s  mQuant, mQuantI, mQuantP, mQuantB, mQuantPrev, mQuantOffset, mQPprev;
    Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
    Ipp64f  mRCqa, mRCfa, mRCqa0;
    Ipp32s  mQuantIprev, mQuantPprev, mQuantBprev;
    Ipp32s  mQuantUpdated;
    Ipp32s mBitsEncodedP, mBitsEncodedPrev;
} BRC_SVCLayer_State;


class SVCBRC : public VideoBrc {

public:

  SVCBRC();
  virtual ~SVCBRC();

  // Initialize with specified parameter(s)
  Status Init(BaseCodecParams *init, Ipp32s numTemporalLayers = 1);
  Status InitLayer(VideoBrcParams *params, Ipp32s tid);
  Status InitHRDLayer(Ipp32s tid);

  // Close all resources
  Status Close();

  Status Reset(BaseCodecParams *init, Ipp32s numTemporalLayers = 1);

  Status SetParams(BaseCodecParams* params, Ipp32s tid = 0);
  Status GetParams(BaseCodecParams* params, Ipp32s tid = 0);
  Status GetHRDBufferFullness(Ipp64f *hrdBufFullness, Ipp32s recode = 0, Ipp32s tid = 0);
  Status PreEncFrame(FrameType frameType, Ipp32s recode = 0, Ipp32s tid = 0);
  BRCStatus PostPackFrame(FrameType frameType, Ipp32s bitsEncodedFrame, Ipp32s payloadBits = 0, Ipp32s recode = 0, Ipp32s poc = 0);
  BRCStatus UpdateAndCheckHRD(Ipp32s tid, Ipp32s bitsEncodedFrame, Ipp32s payloadBits, Ipp32s recode);

  Ipp32s GetQP(FrameType frameType, Ipp32s tid = -1);
  Status SetQP(Ipp32s qp, FrameType frameType, Ipp32s tid);

  Status SetPictureFlags(FrameType frameType, Ipp32s picture_structure, Ipp32s repeat_first_field = 0, Ipp32s top_field_first = 0, Ipp32s second_field = 0);

  Status GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits);

//  Status GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s tid, Ipp32s recode = 0);

protected:

  VideoBrcParams mParams[MAX_TEMP_LEVELS];
  bool   mIsInit;
  BRCSVC_HRDState mHRD[MAX_TEMP_LEVELS];
  BRC_SVCLayer_State mBRC[MAX_TEMP_LEVELS];
  Ipp32s mNumTemporalLayers;
/*
  Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;
  Ipp32s  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantPrev, mQuantOffset, mQPprev;
  Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
  Ipp64f  mRCqa, mRCfa, mRCqa0;
  Ipp32s  mQuantIprev, mQuantPprev, mQuantBprev;
*/
  Ipp32s mTid; // current frame temporal layer ID
  Ipp32s mRCMode;
  Ipp32s mQuant;
  Ipp32s mQuantI, mQuantP, mQuantB;
  FrameType mFrameType;
  Ipp32s  mBitsEncoded;
  BrcPictureFlags  mPictureFlags, mPictureFlagsPrev;
  Ipp32s mQuantUpdated;
  Ipp32s mQuantUnderflow;
  Ipp32s mQuantOverflow;

  Ipp32s mRecodeInternal;
  Ipp32s GetInitQP();
  BRCStatus UpdateQuant(Ipp32s tid, Ipp32s bEncoded, Ipp32s totalPicBits);
//  BRCStatus UpdateQuant_ScCh(Ipp32s bEncoded, Ipp32s totalPicBits);
  BRCStatus UpdateQuantHRD(Ipp32s tid, Ipp32s bEncoded, BRCStatus sts, Ipp32s payloadBits = 0);
  Status InitHRD();

  Ipp32s mQuantMax;
  Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
  Ipp64f  mRCqa, mRCfa, mRCqa0;

  Ipp32s mMaxFrameSize, mMinFrameSize;
  Ipp32s mOversize, mUndersize;

//  Ipp64u mMaxBitsPerPic, mMaxBitsPerPicNot0;
/*
  Ipp32s mSceneChange;
  Ipp32s mBitsEncodedP, mBitsEncodedPrev;
  Ipp32s mPoc, mSChPoc;
  Ipp32s mRCfapMax, mRCqapMax;
  Ipp32u mMaxBitrate;
  Ipp64f mRCfsize;
  Ipp32s mScChFrameCnt;
*/

/*
  Ipp64f instant_rate_thresholds[N_INST_RATE_THRESHLDS]; // constant, calculated at init, thresholds for instant bitrate
  Ipp64f deviation_thresholds[N_DEV_THRESHLDS]; // constant, calculated at init, buffer fullness/deviation thresholds
  Ipp64f sizeRatio[3], sizeRatio_field[3], sRatio[3];
*/

};

} // namespace UMC
#endif

