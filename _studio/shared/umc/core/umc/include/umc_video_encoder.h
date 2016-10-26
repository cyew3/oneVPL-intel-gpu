//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VIDEO_ENCODER_H__
#define __UMC_VIDEO_ENCODER_H__

#include "umc_base_codec.h"
#include "umc_par_reader.h"

namespace UMC
{
#ifndef UMC_RESTRICTED_CODE_PAR_EXT
#ifdef PARAM_EXTENSION_1
enum RateControlMode {
  RC_UNDEFINED = 0,
  RC_CBR       = 1,      // Constant bit rate
  RC_VBR       = 2,      // Unrestricted variable bit rate
  RC_RVBR      = 3,      // Restricted variable bit rate
  RC_UVBR      = RC_VBR  // Unrestricted variable bit rate
};
#define QP_COUNT 3
#endif // PARAM_EXTENSION_1
#endif // UMC_RESTRICTED_CODE_PAR_EXT

class VideoEncoderParams : public BaseCodecParams
{
    DYNAMIC_CAST_DECL(VideoEncoderParams, BaseCodecParams)
public:
    // Constructor
    VideoEncoderParams() :
        numEncodedFrames(0),
        qualityMeasure(51)
    {
      info.clip_info.width     = 0;
      info.clip_info.height    = 0;
      info.color_format        = YUV420;
      info.bitrate             = 0;
      info.aspect_ratio_width  = 1;
      info.aspect_ratio_height = 1;
      info.framerate           = 30;
      info.duration            = 0;
      info.interlace_type      = PROGRESSIVE;
      info.stream_type         = UNDEF_VIDEO;
      info.stream_subtype      = UNDEF_VIDEO_SUBTYPE;
      info.streamPID           = 0;

#ifndef UMC_RESTRICTED_CODE_PAR_EXT
#ifdef PARAM_EXTENSION_1
      // should be initialized to appropriate in derived classes
      IPdistance = -1;
      GOPsize    = -1;
      maxDelayTime = 500;
      rcMode     = RC_CBR;
      Ipp32s i;
      for ( i=0; i<QP_COUNT; i++ ) quantVBR[i] = -1;
#endif // PARAM_EXTENSION_1
#endif // UMC_RESTRICTED_CODE_PAR_EXT
    }
    // Destructor
    virtual ~VideoEncoderParams(void){}
    // Read parameter from file
    virtual Status ReadParamFile(const vm_char * /*ParFileName*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    VideoStreamInfo info;               // (VideoStreamInfo) compressed video info
    Ipp32s          numEncodedFrames;   // (Ipp32s) number of encoded frames
#ifndef UMC_RESTRICTED_CODE_PAR_EXT
#ifdef PARAM_EXTENSION_1
    Ipp32s          IPdistance;         // Distance between reference frames
    Ipp32s          GOPsize;            // GOP size or maximum key-frame distance
    Ipp32s          maxDelayTime;       // Hypothetic decoder delay in msec
    RateControlMode rcMode;             // Rate control mode, default RC_CBR
    Ipp32s          quantVBR[QP_COUNT]; // Quantizers for VBR modes
#endif // PARAM_EXTENSION_1
#endif // UMC_RESTRICTED_CODE_PAR_EXT

    // additional controls
    Ipp32s qualityMeasure;      // per cent, represent quantization precision
};

/******************************************************************************/

class VideoEncoder : public BaseCodec
{
    DYNAMIC_CAST_DECL(VideoEncoder, BaseCodec)
public:
    // Destructor
    virtual ~VideoEncoder() {};
};

/******************************************************************************/

// reads parameters from ParamList to VideoEncoderParams
Status ReadParamList(VideoEncoderParams* par, ParamList* lst);

// information about parameters for VideoEncoderParams
extern const ParamList::OptionInfo VideoEncoderOptions[];

} // end namespace UMC

#endif /* __UMC_VIDEO_ENCODER_H__ */
