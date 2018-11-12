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

#ifndef __UMC_AUDIO_ENCODER_H__
#define __UMC_AUDIO_ENCODER_H__

#include "umc_structures.h"
#include "umc_base_codec.h"
#include "umc_media_data.h"

namespace UMC
{

class AudioEncoderParams : public BaseCodecParams
{
public:
    DYNAMIC_CAST_DECL(AudioEncoderParams, BaseCodecParams)

    AudioEncoderParams(void) {}

    AudioStreamInfo m_info;   // audio stream info
};

class AudioEncoder : public BaseCodec
{
public:
    DYNAMIC_CAST_DECL(AudioEncoder, BaseCodec)

    AudioEncoder(void)
    {
        m_frame_num = 0;
    };

    virtual Status GetDuration(float *pDuration)
    {
        pDuration[0] = (float)-1.0;
        return UMC_ERR_NOT_IMPLEMENTED;
    }

protected:
    uint32_t m_frame_num;
};

}

#endif
