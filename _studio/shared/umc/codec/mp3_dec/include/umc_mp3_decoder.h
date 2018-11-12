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

#ifndef __UMC_MP3DEC_FP_H__
#define __UMC_MP3DEC_FP_H__

#include "vm_types.h"
#include "audio_codec_params.h"
#include "mp3_status.h"
#include "umc_audio_decoder.h"
#include "mp3dec_own.h" 
struct _MP3Dec;

namespace UMC
{

#define MP3_PROFILE_MPEG1  1
#define MP3_PROFILE_MPEG2  2

#define MP3_LEVEL_LAYER1   1
#define MP3_LEVEL_LAYER2   2
#define MP3_LEVEL_LAYER3   3

Status  DecodeMP3Header( Ipp8u *inPointer, Ipp32s inDataSize, MP3Dec_com *state);

class MP3DecoderParams: public AudioDecoderParams
{
public:
    DYNAMIC_CAST_DECL(MP3DecoderParams, AudioDecoderParams)

    MP3DecoderParams(void)
    {
        mc_lfe_filter_off = 0;
        synchro_mode      = 0;
    };

    Ipp32s mc_lfe_filter_off;
    Ipp32s synchro_mode;
};

class MP3Decoder : public AudioDecoder
{
public:
    DYNAMIC_CAST_DECL(MP3Decoder, AudioDecoder)

    MP3Decoder(void);
    virtual ~MP3Decoder(void);

    virtual Status  Init(BaseCodecParams* init);
    virtual Status  Close(void);
    virtual Status  Reset(void);

    virtual Status  GetFrame(MediaData* in, MediaData* out);
    virtual Status  GetInfo(BaseCodecParams* info);
    virtual Status  SetParams(BaseCodecParams*)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    };

    virtual Status  GetDuration(Ipp32f* p_duration);
    static Status StatusMP3_2_UMC(MP3Status st);
    Status FrameConstruct(MediaData *in, Ipp32s *outFrameSize,  int *bitRate, Ipp32s *outID3HeaderSize, unsigned int *p_RawFrameSize);

protected:
    struct _MP3Dec*   state;
    cAudioCodecParams params;
    Ipp64f            m_pts_prev;
    MemID             stateMemId;
    Ipp32s            m_mc_lfe_filter_off;
    Ipp32s            m_syncro_mode;
    Ipp32s            m_initialized;

    
    Status MemLock();
    Status MemUnlock();
};

}

#endif
