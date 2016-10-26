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

#ifndef _UMC_MPEG2_BRC_H_
#define _UMC_MPEG2_BRC_H_

#include "umc_video_encoder.h"
#include "umc_brc.h"

#define APA_MPEG2_BRC 1

#if APA_MPEG2_BRC
#include "mfxdefs.h"

#define BRC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BRC_MAX(a, b) ((a) > (b) ? (a) : (b))

#define N_INST_RATE_THRESHLDS 4
#define N_QUALITY_LEVELS 7
#define N_QP_THRESHLDS 4
#define N_DEV_THRESHLDS 8
#define N_VAR_THRESHLDS 7

#endif

namespace UMC
{

class MPEG2BRC : public CommonBRC {

public:

  MPEG2BRC();
  ~MPEG2BRC();

  // Initialize with specified parameter(s)
  virtual Status Init(BaseCodecParams *init, Ipp32s nl = 1);

  // Close all resources
  virtual Status Close();

  virtual Status Reset(BaseCodecParams *init, Ipp32s nl = 1);

  virtual Status SetParams(BaseCodecParams* params, Ipp32s tid = 0);

  virtual Status PreEncFrame(FrameType frameType, Ipp32s recode = 0, Ipp32s tid = 0);
  virtual BRCStatus PostPackFrame(FrameType picType, Ipp32s bitsEncodedFrame, Ipp32s payloadBits = 0, Ipp32s recode = 0, Ipp32s = 0);

  virtual Ipp32s GetQP(FrameType frameType, Ipp32s tid = 0);
  virtual Status SetQP(Ipp32s qp, FrameType frameType, Ipp32s tid = 0);
  virtual Status SetPictureFlags(FrameType frameType, Ipp32s picture_structure, Ipp32s repeat_first_field, Ipp32s top_field_first, Ipp32s second_field);

//  virtual Status Query(UMCExtBuffer *pStat, Ipp32u *numEntries);

protected:

  bool   mIsInit;
  Ipp64f      rc_weight[3];          // frame weight (length proportion)
  Ipp64f      rc_tagsize[3];         // bitsize target of the type
  Ipp64f      rc_tagsize_frame[3];   // bitsize target of the type
  Ipp64f      rc_tagsize_field[3];   // bitsize target of the type
  Ipp64f      rc_dev;                // bitrate deviation (sum of GOP's frame diffs)
  Ipp64f      rc_dev_saved;          // bitrate deviation (sum of GOP's frame diffs)
  Ipp64f      gopw;
  Ipp32s      block_count;
  Ipp32s      qscale[3];             // qscale codes for 3 frame types (Ipp32s!)
  Ipp32s      prsize[3];             // bitsize of previous frame of the type
  Ipp32s      prqscale[3];           // quant scale value, used with previous frame of the type
  BrcPictureFlags      prpicture_flags[3];
  BrcPictureFlags      picture_flags, picture_flags_prev, picture_flags_IP;
  Ipp32s  quantiser_scale_value;
  Ipp32s  quantiser_scale_code;
  Ipp32s  q_scale_type;
  bool  full_hw;
//  Ipp32s  mBitsDesiredFrame;

  Ipp32s GetInitQP();
  BRCStatus UpdateQuantHRD(Ipp32s bEncoded, BRCStatus sts);
  Status CheckHRDParams();
//  Status CalculatePicTargets();
  Ipp32s ChangeQuant(Ipp32s quant);

#if APA_MPEG2_BRC

  virtual Status PreEncFrameMidRange(FrameType frameType, Ipp32s recode = 0);
  virtual Status PreEncFrameFallBack(FrameType frameType, Ipp32s recode = 0);

  mfxI32  mIsFallBack;              // depending on bitrate calls PreEncFrameFallBack

  mfxI32  mQuantMax;                // max quantizer, just handy for using in different standards.
  mfxI32  mQuantMin;                // min quantizer

/*
  mfxI32  N_P_frame, N_B_frame;         // number of sP,B frames in GOP, calculated at init
  mfxI32  N_P_field, N_B_field;         // number of sP,B frames in GOP, calculated at init
  mfxI32  N_I, N_P, N_B;         // number of I,P,B frames in GOP, calculated at init
*/
  mfxI32  prev_frame_type;       // previous frame type
  mfxI32  mQualityLevel;         // quality level, from 0 to N_QUALITY_LEVELS-1, the higher - the better

  mfxF64  instant_rate_thresholds[N_INST_RATE_THRESHLDS]; // constant, calculated at init, thresholds for instatnt bitrate
  mfxF64  deviation_thresholds[N_DEV_THRESHLDS]; // constant, calculated at init, buffer fullness/deviation thresholds

#endif

};

class Mpeg2_BrcParams : public VideoEncoderParams
{
  DYNAMIC_CAST_DECL(Mpeg2_BrcParams, VideoEncoderParams)
public:
  Mpeg2_BrcParams():
        frameWidth  (0),
        frameHeight (0),
        maxFrameSize(0)
  {
        quant[0] = quant[1] = quant[2] = 0;
  }
  Ipp32s  frameWidth;
  Ipp32s  frameHeight;
  Ipp32s  maxFrameSize;
  Ipp16s  quant[3]; // I,P,B
};

class MPEG2BRC_CONST_QUNT : public CommonBRC {

public:

    MPEG2BRC_CONST_QUNT() {};
    ~MPEG2BRC_CONST_QUNT(){};

  // Initialize with specified parameter(s)
  virtual Status Init(BaseCodecParams *init, Ipp32s = 0)
  {
      Mpeg2_BrcParams *brcParams = DynamicCast<Mpeg2_BrcParams>(init);
      if (brcParams && brcParams->quant[0]>0 && brcParams->frameHeight > 0 && brcParams->frameWidth >0 )
      {
          ippsCopy_8u((Ipp8u*)brcParams, (Ipp8u*)&m_params, sizeof(Mpeg2_BrcParams));
          m_params.frameHeight = brcParams->frameHeight;
          m_params.frameWidth  = brcParams->frameWidth;

          m_params.maxFrameSize = brcParams->frameHeight*brcParams->frameWidth * 2;
          m_params.quant[0] =  brcParams->quant[0];
          m_params.quant[1] = (brcParams->quant[1]>0) ? brcParams->quant[1]:brcParams->quant[0];
          m_params.quant[2] = (brcParams->quant[2]>0) ? brcParams->quant[2]:brcParams->quant[0];

          return UMC_OK;
      }
      return UMC_ERR_UNSUPPORTED;
  }

  // Close all resources
  virtual Status Close() {return UMC_OK;}

  virtual Status Reset(BaseCodecParams *, Ipp32s) {return UMC_OK;}

  virtual Status SetParams(BaseCodecParams* params, Ipp32s = 0)
  {
      return Init(params);
  }
  virtual Status GetParams(BaseCodecParams* params, Ipp32s = 0)
  {
       Mpeg2_BrcParams *brcParams = DynamicCast<Mpeg2_BrcParams>(params);
       if (brcParams)
       {
          brcParams->maxFrameSize = m_params.maxFrameSize;
          brcParams->quant[0] = m_params.quant[0];
          brcParams->quant[1] = m_params.quant[1];
          brcParams->quant[2] = m_params.quant[2];

          brcParams->frameHeight = m_params.frameHeight;
          brcParams->frameWidth  = m_params.frameWidth;
       }
       else
       {
           VideoBrcParams *videoParams = DynamicCast<VideoBrcParams>(params);
           videoParams->HRDBufferSizeBytes = m_params.maxFrameSize;
           videoParams->HRDInitialDelayBytes = 0;
           videoParams->maxBitrate = 0;
           videoParams->BRCMode = BRC_VBR;

       }
       return UMC_OK;
  }


  virtual Status PreEncFrame(FrameType /*frameType*/, Ipp32s /*recode*/, Ipp32s)
  {
      return UMC_OK;
  }
  virtual BRCStatus PostPackFrame(FrameType /*picType*/, Ipp32s /*bitsEncodedFrame*/, Ipp32s /*payloadBits*/ = 0, Ipp32s /*recode*/ = 0, Ipp32s = 0)
  {
      return UMC_OK;
  }
  virtual Ipp32s GetQP(FrameType frameType, Ipp32s = 0)
  {
      switch (frameType)
      {
      case I_PICTURE:
          return m_params.quant[0];
      case P_PICTURE:
          return m_params.quant[1];
      case B_PICTURE:
          return m_params.quant[2];
      default:
          return m_params.quant[0];
      }
  }
  virtual Status SetQP(Ipp32s qp, FrameType frameType, Ipp32s = 0)
  {
      switch (frameType)
      {
      case I_PICTURE:
          return m_params.quant[0] = (Ipp16s)qp;
      case P_PICTURE:
          return m_params.quant[1] = (Ipp16s)qp;
      case B_PICTURE:
          return m_params.quant[2] = (Ipp16s)qp;
      default:
           return UMC_ERR_UNSUPPORTED;
      }
  }
  virtual Status SetPictureFlags(FrameType /*frameType*/, Ipp32s /*picture_structure*/, Ipp32s /*repeat_first_field*/, Ipp32s /*top_field_first*/, Ipp32s /*second_field*/)
  {
      return UMC_OK;
  }

private:

    Mpeg2_BrcParams m_params;

};
} // namespace UMC
#endif

