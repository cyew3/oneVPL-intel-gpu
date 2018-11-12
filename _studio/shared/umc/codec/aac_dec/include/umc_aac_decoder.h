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

#ifndef __UMC_AAC_DECODER_H__
#define __UMC_AAC_DECODER_H__

#include "umc_audio_decoder.h"
#include "umc_memory_allocator.h"
#include "umc_dynamic_cast.h"

#include "aaccmn_const.h"
#include "mp4cmn_config.h"
#include "audio_codec_params.h"
#include "aac_status.h"
#include "sbr_dec_struct.h"
#include "ps_dec_settings.h"
#include "aaccmn_adif.h"
struct _AACDec;

namespace UMC
{

// profile and level definitions for aac
// values for profile and level fields in BaseCodecParams
#define AAC_PROFILE_MAIN  1
#define AAC_PROFILE_LC    2
#define AAC_PROFILE_SSR   3
#define AAC_PROFILE_LTP   4
#define AAC_PROFILE_SBR   0x100
#define AAC_LEVEL_MAIN    0

Status DecodeESDSHeader(sAudio_specific_config* audio_config_data, 
        sBitsreamBuffer* BS);

Status  DecodeADIFSHeader( sAdif_header* audio_config_data, 
                          sBitsreamBuffer* BS);
Status  DecodeADTSfixedHeader( sAdts_fixed_header* audio_config_data, 
                               sBitsreamBuffer* BS);
Status  DecodeADTSvariableHeader( sAdts_variable_header* audio_config_data, 
                                  sBitsreamBuffer* BS);

class AACDecoderParams: public AudioDecoderParams
{
public:
    DYNAMIC_CAST_DECL(AACDecoderParams, AudioDecoderParams);

    AACDecoderParams(void)
    {
        ModeDecodeHEAACprofile = HEAAC_HQ_MODE;
        ModeDwnsmplHEAACprofile= HEAAC_DWNSMPL_ON;
        flag_SBR_support_lev   = SBR_ENABLE;
        flag_PS_support_lev    = PS_DISABLE;
        layer = -1;
    };

    Ipp32s       ModeDecodeHEAACprofile;
    Ipp32s       ModeDwnsmplHEAACprofile;
    eSBR_SUPPORT flag_SBR_support_lev;
    Ipp32s       flag_PS_support_lev;
    Ipp32s       layer;
};

class AACDecoder: public AudioDecoder
{
public:
    DYNAMIC_CAST_DECL(AACDecoder, AudioDecoder);

    enum DecodeMode
    {
        DM_UNDEF_STREAM = 0,
        DM_RAW_STREAM,
        DM_ADTS_STREAM
    };

    AACDecoder(void);
    virtual ~AACDecoder(void);

    virtual Status  Init(BaseCodecParams * init);
    virtual Status  GetFrame(MediaData * in, MediaData * out);
    virtual Status  Close();
    virtual Status  Reset();

    virtual Status  GetInfo(BaseCodecParams * info);
    virtual Status  SetParams(BaseCodecParams * params);
    virtual Status  GetDuration(Ipp32f *p_duration);

    Status FrameConstruct(MediaData *in, Ipp32s *outFrameSize);


protected:
    Status StatusAAC_2_UMC(AACStatus st);
    Status SetObjectType(AudioObjectType mType,
                         sALS_specific_config *inState);
    Status MemLock();
    Status MemUnlock();

    struct _AACDec *state;
    cAudioCodecParams params;

    Ipp64f m_pts_prev;
    Ipp32s initSubtype;
    Ipp32u adts_sw;
    MemID  stateMemId;
    MemID  objMemId;
    Ipp8u *pObjMem;

    struct
    {
        AudioStreamType    m_init_stream_type;
        size_t             m_init_config_data_size;
        DecodeMode         m_decode_mode;
        Ipp32s             m_sampling_frequency_index;

        Ipp32u             m_channel_config;
        Ipp32s             m_frame_number;
        AudioObjectType    m_audio_object_type;
        AudioStreamSubType m_stream_subtype;
    } m_info;
};

};      // namespace UMC

#endif // __UMC_AAC_DECODER_H__
