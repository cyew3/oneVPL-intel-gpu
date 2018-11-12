// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __UMC_AAC_ENCODER_PARAMS_H__
#define __UMC_AAC_ENCODER_PARAMS_H__

#include "umc_audio_codec.h"
#include "ippdefs.h"

enum UMC_AACStereoMode {
  UMC_AAC_MONO,
  UMC_AAC_LR_STEREO,
  UMC_AAC_MS_STEREO,
  UMC_AAC_JOINT_STEREO
};

namespace UMC
{
#include "aaccmn_const.h"

class AACEncoderParams:public AudioCodecParams {
  DYNAMIC_CAST_DECL(AACEncoderParams, AudioCodecParams)
public:
  AACEncoderParams(void) {
    audioObjectType = AOT_AAC_LC;
    auxAudioObjectType = AOT_UNDEF;

    stereo_mode = UMC_AAC_LR_STEREO;
    outputFormat = UMC_AAC_ADTS;
    ns_mode = 0;
  };
  enum AudioObjectType audioObjectType;
  enum AudioObjectType auxAudioObjectType;

  enum UMC_AACStereoMode  stereo_mode;
  enum UMC_AACOuputFormat outputFormat;
  Ipp32s ns_mode;
};

}// namespace UMC

#endif /* __UMC_AAC_ENCODER_PARAMS_H__ */

