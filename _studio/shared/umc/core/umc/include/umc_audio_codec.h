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

#ifndef __UMC_AUDIO_CODEC_H__
#define __UMC_AUDIO_CODEC_H__

#include "umc_structures.h"
#include "umc_base_codec.h"
#include "umc_media_data.h"

namespace UMC
{

class AudioCodecParams : public BaseCodecParams
{
    DYNAMIC_CAST_DECL(AudioCodecParams, BaseCodecParams)
public:
    AudioCodecParams();

    AudioStreamInfo m_info_in;                                  // (AudioStreamInfo) original audio stream info
    AudioStreamInfo m_info_out;                                 // (AudioStreamInfo) output audio stream info

    uint32_t m_frame_num;                                         // (uint32_t) keeps number of processed frames
};

/******************************************************************************/

class AudioCodec : public BaseCodec
{
    DYNAMIC_CAST_DECL(AudioCodec, BaseCodec)

public:

    // Default constructor
    AudioCodec(void){};
    // Destructor
    virtual ~AudioCodec(void){};

    virtual Status GetDuration(float *p_duration)
    {
        p_duration[0] = (float)-1.0;
        return UMC_ERR_NOT_IMPLEMENTED;
    }

protected:

    uint32_t m_frame_num;                                         // (uint32_t) keeps number of processed frames.
};

/******************************************************************************/

class AudioData: public MediaData
{
    DYNAMIC_CAST_DECL(AudioData, MediaData)

public:
    AudioStreamInfo m_info;
    AudioData()
    {}

};

} // namespace UMC

#endif /* __UMC_AUDIO_CODEC_H__ */
