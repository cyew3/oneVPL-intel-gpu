// Copyright (c) 2002-2018 Intel Corporation
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

#ifndef __AAC_DEC_FP_H__
#define __AAC_DEC_FP_H__

#include "aaccmn_const.h"
#include "audio_codec_params.h"
#include "mp4cmn_config.h"
#include "aac_status.h"
#include "sbr_dec_struct.h"

#include "ipps.h"
#include "ippac.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct _AACDec;
  typedef struct _AACDec AACDec;

  AACStatus aacdecReset(AACDec *state);
  AACStatus aacdecInit(AACDec *state_ptr);
  AACStatus aacdecUpdateStateMemMap(AACDec *state, Ipp32s mShift);
  AACStatus aacdecUpdateObjMemMap(AACDec *state, Ipp32s mShift);
  AACStatus aacdecGetSize(Ipp32s *pSize);
  AACStatus aacdecClose(AACDec *state);
  AACStatus aacdecGetFrame(Ipp8u  *inPointer,
                           Ipp32s *decodedBytes,
                           Ipp16s *outPointer,
                           Ipp32s inDataSize,
                           Ipp32s outBufferSize,
                           AACDec *state);
  AACStatus aacdecGetDuration(Ipp32f *p_duration,
                              AACDec *state);
  AACStatus aacdecGetInfo(cAudioCodecParams *a_info,
                          AACDec *state);

  AACStatus aacdecSetSamplingFrequency(Ipp32s sampling_frequency_index,
                                       AACDec *state);

  AACStatus aacdecSetLayer(Ipp32s layer,
                           AACDec *state);
  /* SBR tool */
  AACStatus aacdecSetSBRModeDecode(Ipp32s ModeDecodeHEAAC,
                                   AACDec *state);
  AACStatus aacdecSetSBRModeDwnsmpl(Ipp32s ModeDwnsmplHEAAC,
                                  AACDec *state);
  AACStatus aacdecSetSBRSupport(eSBR_SUPPORT flag, AACDec *state);

  /* PS tool */
  AACStatus aacdecSetPSSupport( Ipp32s flag,  AACDec *state);
  AACStatus aacdecSetAudioObjectType(enum AudioObjectType audio_object_type,
                                     AACDec *state,
                                     sALS_specific_config *inState,
                                     Ipp8u *mem,
                                     Ipp32s *size_all);
  AACStatus aacdecGetFDPSize(enum AudioObjectType audio_object_type,
                             Ipp32s *pSize);
  AACStatus aacdecSetPCE(sProgram_config_element *pce,
                         AACDec *state);
  AACStatus aacdecSetAdtsChannelConfiguration(Ipp32s adts_channel_configuration,
                                              AACDec *state);
  AACStatus aacdecInitCRC(Ipp32s crc_enable,
                          AACDec *state);
  AACStatus aacdecSetCRC(Ipp32s crc,
                         AACDec *state);
  AACStatus aacdecGetCRC(Ipp32s *crc,
                         AACDec *state);
  AACStatus aacdecGetSbrFlagPresent(Ipp32s *SbrFlagPresent,
                                    AACDec *state);
  AACStatus aacdecGetFrameSize(Ipp32s *frameSize,
                               AACDec *state);
  AACStatus aacdecGetSampleFrequency(Ipp32s *freq,
                                     AACDec *state);

  AACStatus aacdecGetNumDecodedFrames(Ipp32s *num,
                                      AACDec *state);

  AACStatus aacdecGetChannels(Ipp32s *ch,
                              AACDec *state);

  AACStatus aacdec_GetID3Len(Ipp8u *in,
                             Ipp32s inDataSize,
                             AACDec *state);

  AACStatus aacdec_SkipID3(Ipp32s inDataSize,
                           Ipp32s *skipped,
                           AACDec *state);

  AACStatus bsacdecGetFrame(Ipp8u    *inPointer,
                            Ipp32s   inDataSize,
                            Ipp32s   layersToDecode,
                            AACDec   *state);

  AACStatus bsacdecSetNumChannels(Ipp32s channelConfiguration,
                                  AACDec *state);
#ifdef __cplusplus
}
#endif

#endif
