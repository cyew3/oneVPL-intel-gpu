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

//
//    This file contains class covering OSS Linux devices interface
//

#ifndef __OSS_DEV_H__
#define __OSS_DEV_H__

#ifdef UMC_ENABLE_OSS_AUDIO_RENDER

#include "linux_dev.h"

namespace UMC
{
    class OSSDev
    {
    public:
        OSSDev():
          m_uiBytesPlayed(0),
          m_uiBitsPerSample(0),
          m_uiNumOfChannels(0),
          m_uiFreq(0)
        {}

        virtual ~OSSDev() {}

        Status Init(const Ipp32u uiBitsPerSample,
                    const Ipp32u uiNumOfChannels,
                    const Ipp32u uiFreq);

        Status RenderData(Ipp8u* pbData, Ipp32u uiDataSize);
        Status Post();
        Status Reset();
        Status GetBytesPlayed(Ipp32u& ruiBytesPlayed);

        Status SetBitsPerSample(const Ipp32u uiBitsPerSample);
        Status SetNumOfChannels(const Ipp32u uiNumOfChannels);
        Status SetFreq(const Ipp32u uiFreq);

    protected:
        LinuxDev m_Device;
        Ipp32u m_uiBytesPlayed;
        Ipp32u m_uiBitsPerSample;
        Ipp32u m_uiNumOfChannels;
        Ipp32u m_uiFreq;
    };
}; // namespace UMC

#endif // UMC_ENABLE_OSS_AUDIO_RENDER

#endif // __OSS_DEV_H__

