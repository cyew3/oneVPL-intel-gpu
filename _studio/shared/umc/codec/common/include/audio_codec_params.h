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

#ifndef __AUDIO_CODEC_PARAMS_H__
#define __AUDIO_CODEC_PARAMS_H__

#include "ippdefs.h"

enum UMC_MP3StereoMode
{
    UMC_MPA_MONO,
    UMC_MPA_LR_STEREO,
    UMC_MPA_MS_STEREO,
    UMC_MPA_JOINT_STEREO
};

#define UMC_MPAENC_CBR 0
#define UMC_MPAENC_ABR 1

typedef enum
{
    UNDEF_AUD             = 0x00000000,
    PCM_AUD               = 0x00000001,
    LPCM_AUD              = 0x00000002,
    AC3_AUD               = 0x00000004,
    TWINVQ_AUD            = 0x00000008,

    MPEG1_AUD             = 0x00000100,
    MPEG2_AUD             = 0x00000200,
    MPEG_AUD_LAYER1       = 0x00000010,
    MPEG_AUD_LAYER2       = 0x00000020,
    MPEG_AUD_LAYER3       = 0x00000040,

    MP1L1_AUD             = MPEG1_AUD|MPEG_AUD_LAYER1,
    MP1L2_AUD             = MPEG1_AUD|MPEG_AUD_LAYER2,
    MP1L3_AUD             = MPEG1_AUD|MPEG_AUD_LAYER3,
    MP2L1_AUD             = MPEG2_AUD|MPEG_AUD_LAYER1,
    MP2L2_AUD             = MPEG2_AUD|MPEG_AUD_LAYER2,
    MP2L3_AUD             = MPEG2_AUD|MPEG_AUD_LAYER3,

    VORBIS_AUD            = 0x00000400,
    AAC_AUD               = 0x00000800
} cAudioStreamType;

typedef enum
{
    UNDEF_AUD_SUBTYPE    = 0x00000000,
    AAC_LC_PROF          = 0x00000001,
    AAC_LTP_PROF         = 0x00000002,
    AAC_MAIN_PROF        = 0x00000004,
    AAC_SSR_PROF         = 0x00000008,
    AAC_HE_PROF          = 0x00000010,
    AAC_ALS_PROF         = 0x00000020
} cAudioStreamSubType;

typedef struct
{
    Ipp32s channels;                                      // (Ipp32s) number of audio channels
    Ipp32s sample_frequency;                              // (Ipp32s) sample rate in Hz
    Ipp32s bitrate;                                       // (Ipp32u) bitstream in bps
    Ipp32s bitPerSample;                                  // (Ipp32u) 0 if compressed

    Ipp64f duration;                                      // (Ipp64f) duration of the stream

    cAudioStreamType stream_type;                         // (AudioStreamType) general type of stream
    cAudioStreamSubType stream_subtype;                   // (AudioStreamSubType) minor type of stream

    Ipp32s channel_mask;                                  // (Ipp32u) channel mask
} cAudioStreamInfo;

typedef struct
{
    Ipp32s is_valid;
    Ipp32s m_SuggestedInputSize;
    Ipp32s m_SuggestedOutputSize;
    cAudioStreamInfo m_info_in;                           // (AudioStreamInfo) original audio stream info
    cAudioStreamInfo m_info_out;                          // (AudioStreamInfo) output audio stream info

    Ipp32s m_frame_num;                                   // (Ipp32u) keeps number of processed frames
} cAudioCodecParams;

#endif
