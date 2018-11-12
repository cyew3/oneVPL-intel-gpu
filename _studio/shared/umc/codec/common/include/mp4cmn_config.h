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

#ifndef __MP4CMN_CONFIG_H__
#define __MP4CMN_CONFIG_H__

#include "mp4cmn_pce.h"

typedef struct {
  Ipp32u sampFreq;
  Ipp32u numSamples;
  Ipp32u headerSize;
  Ipp32u trailerSize;
  Ipp32s numChannels;
  Ipp32s chNeededLen;
  Ipp32s fileType;
  Ipp32s resolution;
  Ipp32s floating;
  Ipp32s msbFirst;
  Ipp32s frameLength;
  Ipp32s ra;
  Ipp32s raFlag;
  Ipp32s raUnitNum;
  Ipp32s raTotNumUnits;
  Ipp32s adaptOrder;
  Ipp32s coefTable;
  Ipp32s LongTermPrediction;
  Ipp32s maxOrder;
  Ipp32s blockSwitching;
  Ipp32s bgmcMode;
  Ipp32s sbPart;
  Ipp32s jointStereo;
  Ipp32s mcCoding;
  Ipp32s chanConfig;
  Ipp32s chanSort;
  Ipp32s crcEnabled;
  Ipp32u crc;
  Ipp32s RLSLMS;
  Ipp32s auxDataEnabled;
  Ipp32s chanConfigInfo;
  Ipp32s numFrames;
  Ipp32s lastFrameLength;
  sBitsreamBuffer savedBS;
} sALS_specific_config;

typedef struct
{
    Ipp32s frameLengthFlag;
    Ipp32s dependsOnCoreCoder;
    Ipp32s coreCoderDelay;

    Ipp32s extensionFlag;

    Ipp32s numOfSubFrame;
    Ipp32s layerLength;

    sProgram_config_element pce;

} sGA_specific_config;

typedef struct
{
    /// AudioSpecificConfig;
    Ipp32s sbrPresentFlag;
    Ipp32s psPresentFlag;
    Ipp32s audioObjectType;
    Ipp32s samplingFrequencyIndex;
    Ipp32s samplingFrequency;
    Ipp32s channelConfiguration;
    Ipp32s extensionAudioObjectType;
    Ipp32s extensionSamplingFrequencyIndex;
    Ipp32s extensionSamplingFrequency;
    Ipp32s epConfig;

    sALS_specific_config ALSSpecificConfig;
    sGA_specific_config GASpecificConfig;

} sAudio_specific_config;

#ifdef  __cplusplus
extern "C" {
#endif

Ipp32s get_channels_number(sAudio_specific_config * p_data);
Ipp32s get_sampling_frequency(sAudio_specific_config * p_data, Ipp32s bHEAAC);
Ipp32s get_sampling_frequency_index(sAudio_specific_config * p_data);

Ipp32s dec_audio_specific_config(sAudio_specific_config * p_data,sBitsreamBuffer * p_bs);
Ipp32s dec_ga_specific_config(sGA_specific_config * p_data,sBitsreamBuffer * p_bs,
                              /* Ipp32s samplingFrequencyIndex, */ Ipp32s channelConfiguration, Ipp32s audioObjectType);
Ipp32s enc_audio_specific_config(sAudio_specific_config * p_data,sBitsreamBuffer * p_bs);
Ipp32s enc_ga_specific_config(sGA_specific_config * p_data,sBitsreamBuffer * p_bs /*, Ipp32s channelConfiguration */);

Ipp32s decALSSpecificConfig(sALS_specific_config *state,
                            sBitsreamBuffer *pBS);

/***********************************************************************

    Unpack function(s) (support(s) alternative bitstream representation)

***********************************************************************/

Ipp32s unpack_audio_specific_config(sAudio_specific_config * p_data,Ipp8u **pp_bitstream, Ipp32s *p_offset);
Ipp32s unpack_ga_specific_config(sGA_specific_config * p_data,Ipp8u **pp_bitstream, Ipp32s *p_offset,
                                 /* Ipp32s samplingFrequencyIndex, */ Ipp32s channelConfiguration /*, Ipp32s audioObjectType */);

#ifdef  __cplusplus
}
#endif

#endif//__MP4CMN_CONFIG_H__
