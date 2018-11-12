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

#ifndef __UMC_AAC_DECODER_PARAMS_H__
#define __UMC_AAC_DECODER_PARAMS_H__

#include "umc_audio_codec.h"
#include "aaccmn_const.h"
#include "sbr_dec_struct.h"
#include "ps_dec_settings.h"

namespace UMC
{
  class   AACDecoderParams:public AudioCodecParams {
  public:
    AACDecoderParams(void) {
      ModeDecodeHEAACprofile = HEAAC_HQ_MODE;
      ModeDwnsmplHEAACprofile= HEAAC_DWNSMPL_ON;
      flag_SBR_support_lev   = SBR_ENABLE;
      flag_PS_support_lev    = PS_DISABLE;
      layer = -1;
    };
    Ipp32s ModeDecodeHEAACprofile;
    Ipp32s ModeDwnsmplHEAACprofile;
    eSBR_SUPPORT flag_SBR_support_lev;
    Ipp32s       flag_PS_support_lev;
    Ipp32s layer;
  };

}// namespace UMC

#endif /* __UMC_AAC_DECODER_PARAMS_H__ */
