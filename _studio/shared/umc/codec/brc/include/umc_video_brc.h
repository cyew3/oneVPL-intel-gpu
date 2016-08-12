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

#ifndef _UMC_VIDEO_BRC_H_
#define _UMC_VIDEO_BRC_H_

#include "umc_video_encoder.h"

namespace UMC
{

enum eBrcPictureFlags
{
  BRC_TOP_FIELD = 1,
  BRC_BOTTOM_FIELD = 2,
  BRC_FRAME = BRC_TOP_FIELD | BRC_BOTTOM_FIELD,
  BRC_REPEAT_FIRST_FIELD = 4,
  BRC_TOP_FIELD_FIRST = 8,
  BRC_SECOND_FIELD = 12
};

typedef Ipp32s BrcPictureFlags;

#define BRC_BYTES_IN_KBYTE 1000
#define BRC_BITS_IN_KBIT 1000

enum BRCMethod
{
  BRC_CBR = 0,
  BRC_VBR,
  BRC_AVBR,
};

enum BRCAlgorithm
{
  BRC_COMMON = -1,
  BRC_H264,
  BRC_MPEG2,
  BRC_SVC
};

enum eBRCStatus
{
  BRC_ERROR                   = -1,
  BRC_OK                      = 0x0,
  BRC_ERR_BIG_FRAME           = 0x1,
  BRC_BIG_FRAME               = 0x2,
  BRC_ERR_SMALL_FRAME         = 0x4,
  BRC_SMALL_FRAME             = 0x8,
  BRC_NOT_ENOUGH_BUFFER       = 0x10
};

enum eBRCRecode
{
  BRC_RECODE_NONE           = 0,
  BRC_RECODE_QP             = 1,
  BRC_RECODE_PANIC          = 2,
  BRC_RECODE_EXT_QP         = 3,
  BRC_RECODE_EXT_PANIC      = 4,
  BRC_EXT_FRAMESKIP         = 16
};

typedef Ipp32s BRCStatus;

class VideoBrcParams : public VideoEncoderParams {
  DYNAMIC_CAST_DECL(VideoBrcParams, VideoEncoderParams)
public:
  VideoBrcParams();

  Ipp32s  HRDInitialDelayBytes;
  Ipp32s  HRDBufferSizeBytes;

  Ipp32s  targetBitrate;
  Ipp32s  maxBitrate;
  Ipp32s  BRCMode;

  Ipp32s GOPPicSize;
  Ipp32s GOPRefDist;
  Ipp32u frameRateExtD;
  Ipp32u frameRateExtN;
  Ipp32u frameRateExtN_1;

  Ipp16u accuracy;
  Ipp16u convergence;
};

class VideoBrc {

public:

  VideoBrc();
  virtual ~VideoBrc();

  // Initialize with specified parameter(s)
  virtual Status Init(BaseCodecParams *init, Ipp32s numTempLayers = 1) = 0;

  // Close all resources
  virtual Status Close() = 0;

  virtual Status Reset(BaseCodecParams *init, Ipp32s numTempLayers = 1) = 0;

  virtual Status SetParams(BaseCodecParams* params, Ipp32s tid = 0) = 0;
  virtual Status GetParams(BaseCodecParams* params, Ipp32s tid = 0) = 0;
  virtual Status GetHRDBufferFullness(Ipp64f *hrdBufFullness, Ipp32s recode = 0, Ipp32s tid = 0) = 0;
  virtual Status PreEncFrame(FrameType frameType, Ipp32s recode = 0, Ipp32s tid = 0) = 0;
  virtual BRCStatus PostPackFrame(FrameType picType, Ipp32s bitsEncodedFrame, Ipp32s payloadBits = 0, Ipp32s recode = 0, Ipp32s poc = 0) = 0;

  virtual Ipp32s GetQP(FrameType frameType, Ipp32s tid = -1) = 0;
  virtual Status SetQP(Ipp32s qp, FrameType frameType, Ipp32s tid = 0) = 0;

//  virtual Status ScaleRemovalDelay(Ipp64f removalDelayScale) = 0;
  virtual Status SetPictureFlags(FrameType frameType, Ipp32s picture_structure, Ipp32s repeat_first_field = 0, Ipp32s top_field_first = 0, Ipp32s second_field = 0);

  virtual Status GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) = 0;

  static Status CheckCorrectParams_MPEG2(VideoBrcParams *inBrcParams, VideoBrcParams *outBrcParams = NULL);

  virtual Status GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode = 0);

protected:
  //VideoBrc *brc;
  VideoBrcParams mParams;
};

} // namespace UMC
#endif

