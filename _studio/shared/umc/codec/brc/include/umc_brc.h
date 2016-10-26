//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#ifndef _UMC_BRC_H_
#define _UMC_BRC_H_

#include "umc_video_encoder.h"
#include "umc_video_brc.h"

#define BRC_CLIP(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define BRC_CLIPL(a, l) if (a < (l)) a = l
#define BRC_CLIPR(a, r) if (a > (r)) a = r
#define BRC_ABS(a) ((a) >= 0 ? (a) : -(a))

namespace UMC
{


typedef struct _BRC_HRDState
{
  Ipp32u bufSize;
  Ipp64f bufFullness;
  Ipp64f prevBufFullness;
  Ipp64f maxBitrate;
  Ipp64f inputBitsPerFrame;
  Ipp64f maxInputBitsPerFrame;
  Ipp32s frameNum;
  Ipp32s minFrameSize;
  Ipp32s maxFrameSize;
  Ipp32s underflowQuant;
  Ipp64f roundError;
} BRC_HRDState;


class BrcParams : public VideoEncoderParams {
  DYNAMIC_CAST_DECL(BrcParams, VideoEncoderParams)
public:
  BrcParams();

};

class CommonBRC : public VideoBrc {

public:

  CommonBRC();
  virtual ~CommonBRC();

  // Initialize with specified parameter(s)
  virtual Status Init(BaseCodecParams *init, Ipp32s nl = 1);

  // Close all resources
  virtual Status Close();

  virtual Status Reset(BaseCodecParams *init, Ipp32s) 
  {
      return Init(init);
  };

  virtual Status SetParams(BaseCodecParams* params, Ipp32s)
  {
      return Init(params);
  };

  virtual Status GetParams(BaseCodecParams* params, Ipp32s tid = 0);

  virtual Status PreEncFrame(FrameType frameType, Ipp32s recode = 0, Ipp32s tid = 0);

//  virtual BRCStatus PostPackFrame(MediaData *inData, Ipp32s bitsEncodedFrame, Ipp32s recode = 0);
  virtual BRCStatus PostPackFrame(FrameType picType, Ipp32s bitsEncodedFrame, Ipp32s payloadBits = 0, Ipp32s recode = 0, Ipp32s poc = 0);
  virtual Ipp32s GetQP(FrameType picType, Ipp32s tid = 0);
  virtual Status SetQP(Ipp32s qp, FrameType picType, Ipp32s tid = 0);

//  virtual Status ScaleRemovalDelay(Ipp64f removalDelayScale = 1.0);
  virtual Status GetHRDBufferFullness(Ipp64f *hrdBufFullness, Ipp32s recode = 0, Ipp32s tid = 0);
  virtual Status GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode = 0);


  Status GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) {
    if (minFrameSizeInBits)
      *minFrameSizeInBits = mHRD.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = mHRD.maxFrameSize;
    return UMC_OK;
  };

protected:

  void *brc; // can be a ptr to either common or codec-specific BRC (?)

  bool   mIsInit;

  Status InitHRD();
  BRCStatus UpdateAndCheckHRD(Ipp32s frameBits, Ipp32s recode);
  void SetGOP();

  Ipp32s mRCMode;
  Ipp32u mBitrate;
  Ipp64f mFramerate;
  BRC_HRDState mHRD;

  FrameType mFrameType;
  Ipp32s mQuantUpdated;

  Ipp32s mGOPPicSize;
  Ipp32s mGOPRefDist;

  Ipp32s  mBitsDesiredFrame;
  Ipp32s  N_P_frame, N_B_frame;         // number of sP,B frames in GOP, calculated at init
  Ipp32s  N_P_field, N_B_field;         // number of sP,B frames in GOP, calculated at init
  Ipp32s  N_P, N_B;                    // number of P,B frames in GOP, calculated at init
};

} // namespace UMC
#endif
