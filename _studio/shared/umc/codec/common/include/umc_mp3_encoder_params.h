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

#include "umc_defs.h"
#if defined (UMC_ENABLE_MP3_AUDIO_ENCODER) || defined (UMC_ENABLE_MP3_INT_AUDIO_ENCODER)

#ifndef __UMC_MP3_ENCODER_PARAMS_H__
#define __UMC_MP3_ENCODER_PARAMS_H__

#include "umc_audio_codec.h"

enum UMC_MP3StereoMode {
  UMC_MPA_MONO,
  UMC_MPA_LR_STEREO,
  UMC_MPA_MS_STEREO,
  UMC_MPA_JOINT_STEREO
};

#define UMC_MPAENC_CBR 0
#define UMC_MPAENC_ABR 1

namespace UMC
{

class MP3EncoderParams:public AudioCodecParams {
  DYNAMIC_CAST_DECL(MP3EncoderParams, AudioCodecParams)
public:
  MP3EncoderParams(void) {
    stereo_mode = UMC_MPA_LR_STEREO;
    layer = 3;
    mode = UMC_MPAENC_CBR;
    ns_mode = 0;
    force_mpeg1 = 0;
    mc_matrix_procedure = 0;
    mc_lfe_filter_off = 0;
  };
  enum UMC_MP3StereoMode stereo_mode;
  Ipp32s layer;
  Ipp32s mode;
  Ipp32s ns_mode;
  Ipp32s force_mpeg1;
  Ipp32s mc_matrix_procedure;
  Ipp32s mc_lfe_filter_off;
};

}// namespace UMC

#endif /* __UMC_MP3_ENCODER_PARAMS_H__ */

#endif //UMC_ENABLE_XXX
