/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
//
//          Bitrate control
//
*/


#include "umc_defs.h"

#ifndef _UMC_H264_BRC_H_
#define _UMC_H264_BRC_H_

#include "umc_video_encoder.h"
#include "umc_brc.h"

#define SetQuantB() \
  mQuantB = ((mQuantP + mQuantPrev) * 563 >> 10) + 1; \
  BRC_CLIP(mQuantB, 1, mQuantMax)

namespace UMC
{

class H264BRC : public CommonBRC {

public:

  H264BRC();
  ~H264BRC();

  // Initialize with specified parameter(s)
  virtual Status Init(BaseCodecParams *init, Ipp32s nl = 1);

  // Close all resources
  virtual Status Close();

  virtual Status Reset(BaseCodecParams *init, Ipp32s nl = 1);

  virtual Status SetParams(BaseCodecParams* params, Ipp32s tid = 0);

  virtual BRCStatus PostPackFrame(FrameType picType, Ipp32s bitsEncodedFrame, Ipp32s payloadBits, Ipp32s recode = 0, Ipp32s poc = 0);

  virtual Ipp32s GetQP(FrameType frameType, Ipp32s tid = 0);
  virtual Status SetQP(Ipp32s qp, FrameType frameType, Ipp32s tid = 0);

  virtual Status SetPictureFlags(FrameType frameType, Ipp32s picture_structure, Ipp32s repeat_first_field = 0, Ipp32s top_field_first = 0, Ipp32s second_field = 0);
//  virtual Status Query(UMCExtBuffer *pStat, Ipp32u *numEntries);
  virtual Status GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode = 0);

protected:
  bool   mIsInit;
//  Ipp32s  mBitsDesiredFrame;
  Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;
  Ipp32s  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantMin, mQuantPrev, mQuantOffset, mQPprev;
  Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
  Ipp64f  mRCqa, mRCfa, mRCqa0;
  Ipp64f  mRCfa_short;
  Ipp32s  mQuantIprev, mQuantPprev, mQuantBprev;
  Ipp32s  mBitsEncoded;
  Ipp32s  mBitDepth;
  BrcPictureFlags  mPictureFlags, mPictureFlagsPrev;
  Ipp32s mRecode;
  Ipp32s GetInitQP();
  BRCStatus UpdateQuant(Ipp32s bEncoded, Ipp32s totalPicBits);
  BRCStatus UpdateQuantHRD(Ipp32s bEncoded, BRCStatus sts, Ipp32s payloadBits = 0);
  Status InitHRD();
  Ipp64u mMaxBitsPerPic, mMaxBitsPerPicNot0;
  Ipp32s mSceneChange;
  Ipp32s mBitsEncodedP, mBitsEncodedPrev;
  Ipp32s mPoc, mSChPoc;
  Ipp32u mMaxBitrate;
  Ipp64s mBF, mBFsaved;
};

} // namespace UMC
#endif

