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

#ifndef __UMC_AAC_ENCODER_H__
#define __UMC_AAC_ENCODER_H__

#include "umc_media_data.h"
#include "umc_audio_encoder.h"

#include "aaccmn_const.h"

struct _AACEnc;

namespace UMC
{

class AACEncoderParams: public AudioEncoderParams
{
public:
    DYNAMIC_CAST_DECL(AACEncoderParams, AudioEncoderParams)

    AACEncoderParams(void)
    {
        audioObjectType    = AOT_AAC_LC;
        auxAudioObjectType = AOT_UNDEF;

        stereo_mode  = UMC_AAC_LR_STEREO;
        outputFormat = UMC_AAC_ADTS;
        ns_mode = 0;
    };

    enum AudioObjectType audioObjectType;
    enum AudioObjectType auxAudioObjectType;

    enum UMC_AACStereoMode  stereo_mode;
    enum UMC_AACOuputFormat outputFormat;
    Ipp32s ns_mode;
};

/* /////////////////////////////////////////////////////////////////////////////
//  Class:       AACEncoder
//
//  Notes:       Implementation of AAC encoder.
//
*/

class AACEncoder : public AudioEncoder
{
public:
    DYNAMIC_CAST_DECL(AACEncoder, AudioEncoder)

    AACEncoder(void);
    virtual ~AACEncoder(void);
    
    virtual Status Init(BaseCodecParams* init);
    virtual Status Close();

    virtual Status GetFrame(MediaData* in, MediaData* out);

    virtual Status GetInfo(BaseCodecParams* init);

    virtual Status SetParams(BaseCodecParams* params) { params = params; return UMC_ERR_NOT_IMPLEMENTED;};
    virtual Status Reset() { return UMC_ERR_NOT_IMPLEMENTED;};
    virtual Status GetDuration(Ipp32f* p_duration);

    bool CheckBitRate(Ipp32s br, Ipp32s& idx);

protected:
    struct _AACEnc *state;
    Ipp32s m_channel_number;
    Ipp32s m_sampling_frequency;
    Ipp32s m_bitrate;
    Ipp32s m_adtsProfile;
    Ipp32s m_adtsID;
    enum AudioObjectType    m_audioObjectType;
    enum UMC_AACOuputFormat m_outputFormat;

  /* HEAAC profile: auxiliary AOT */
    enum AudioObjectType m_auxAudioObjectType;

    MemID  stateMemId;

    Status MemLock();
    Status MemUnlock();

    Ipp64f m_pts_prev;
};

}// namespace UMC

#endif /* __UMC_AAC_ENCODER_H__ */

