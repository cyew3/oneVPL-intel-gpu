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

#ifndef __UMC_AUDIO_DATA_H__
#define __UMC_AUDIO_DATA_H__

#include "umc_defs.h"
#include "umc_media_data.h"

#include "ippdefs.h"

namespace UMC
{

class AudioData : public MediaData
{
public:
    DYNAMIC_CAST_DECL(AudioData, MediaData)

    AudioData(void);
    virtual ~AudioData(void);

    // Initialize. Only remembers audio characteristics for future.
    virtual Status Init(uint32_t iChannels, uint32_t iSampleFrequency, uint32_t iBitPerSample);

    // Initialize. Only copy audio characteristics from existing data for future.
    virtual Status Init(AudioData *pData);

    // Allocate buffer for audio data.
    virtual Status Alloc(size_t iLenght);

    // Release audio data and all internal memory. Inherited.
    virtual Status Close(void);

    // Set buffer pointer, assign all pointers. Inherited.
    virtual Status SetBufferPointer(uint8_t *pBuffer, size_t nSize);

    // Set common Alignment
    Status SetAlignment(uint32_t iAlignment);

     // Copy actual audio data
    Status Copy(AudioData *pDstData);

     // Copy structures data (just pointers)
    void operator=(const AudioData& par);

public:
    uint32_t m_iChannels;         // number of audio channels
    uint32_t m_iSampleFrequency;  // sample rate in Hz
    uint32_t m_iBitPerSample;     // 0 if compressed
    uint32_t m_iChannelMask;      // channel mask

protected:
    uint32_t m_iAlignment;        // default 1

};

}

#endif
