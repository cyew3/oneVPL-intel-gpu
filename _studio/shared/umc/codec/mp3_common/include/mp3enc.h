// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __MP3ENC_H__
#define __MP3ENC_H__

#include "audio_codec_params.h"
#include "mp3_status.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  MP3_CHANNEL_STEREO             = 0x2,
  MP3_CHANNEL_CENTER             = 0x4,
  MP3_CHANNEL_SURROUND_MONO      = 0x8,
  MP3_CHANNEL_SURROUND_STEREO    = 0x10,
  MP3_CHANNEL_SURROUND_STEREO_P2 = 0x20,
  MP3_CHANNEL_LOW_FREQUENCY      = 0x40
} MP3ChannelMask;

struct _MP3Enc;
typedef struct _MP3Enc MP3Enc;

enum MP3StereoMode {
  MPA_MONO,
  MPA_LR_STEREO,
  MPA_MS_STEREO,
  MPA_JOINT_STEREO
};

#define MPAENC_CBR 0
#define MPAENC_ABR 1

MP3Status mp3encUpdateMemMap(MP3Enc *state, Ipp32s shift);
MP3Status mp3encInit(MP3Enc *state_ptr,
                     Ipp32s sampling_frequency,
                     Ipp32s stereo,
                     Ipp32s channel_mask,
                     Ipp32s layer,
                     Ipp32s bitrate,
                     Ipp32s br_mode,
                     Ipp32s stereo_mode,
                     Ipp32s ns_mode,
                     Ipp32s force_mpeg1,
                     Ipp32s dematrix_procedure,
                     Ipp32s mc_lfe_filter,
                     Ipp32s *size);
MP3Status mp3encClose(MP3Enc *state);
MP3Status mp3encGetFrame(Ipp16s *inPointer, Ipp32s *encodedBytes,
                         Ipp8u *outPointer, MP3Enc *state);
MP3Status mp3encGetSlotSize(MP3Enc *state, Ipp32s *slot_size);

MP3Status mp3encGetMpegId(MP3Enc *state, Ipp32s *id);
MP3Status mp3encGetUpsample(MP3Enc *state, Ipp32s *upsample);

MP3Status mp3encGetFrameSize(Ipp32s *frameSize, Ipp32s id, Ipp32s layer, Ipp32s upsample);
Ipp32s mp3enc_checkBitRate(Ipp32s id, Ipp32s layer, Ipp32s stereo, Ipp32s br, Ipp32s *ind);

MP3Status mp3iencUpdateMemMap(MP3Enc *state, size_t shift);
MP3Status mp3iencInit(MP3Enc *state_ptr,
                      Ipp32s sampling_frequency,
                      Ipp32s stereo,
                      Ipp32s layer,
                      Ipp32s bitrate,
                      Ipp32s br_mode,
                      Ipp32s stereo_mode,
                      Ipp32s ns_mode,
                      Ipp32s *size);
MP3Status mp3iencClose(MP3Enc *state);
MP3Status mp3iencGetFrame(Ipp16s *inPointer, Ipp32s *encodedBytes,
                          Ipp8u *outPointer, MP3Enc *state);
MP3Status mp3iencGetSlotSize(MP3Enc *state, Ipp32s *slot_size);

#ifdef __cplusplus
}
#endif

#endif
