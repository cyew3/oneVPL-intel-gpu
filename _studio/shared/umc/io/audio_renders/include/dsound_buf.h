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

#ifndef __DSOUND_BUF_H__
#define __DSOUND_BUF_H__

#ifdef UMC_ENABLE_DSOUND_AUDIO_RENDER

#include <dsound.h>
#include "umc_structures.h"
#include "umc_mutex.h"

namespace UMC
{
    class DSBuffer
    {
    public:
        DSBuffer():
            m_pDS(NULL),
            m_pDSBSecondary(NULL),
            m_pDSBPrimary(NULL),
            m_dwDSBufferSize(0),
            m_dwNextWriteOffset(0),
            m_dfNorm(1),
            m_dfCompensation(0.005),
            m_hWnd(NULL),
            m_bPausedWaitingData(false)
        {}
        virtual ~DSBuffer() {    Close();    }

        Status          Init(const HWND hWnd,
                             const Ipp32u dwBufferSize,
                             const Ipp16u wChannels,
                             const Ipp32u dwFrequency,
                             const Ipp16u wBytesPerSample);
        Status          CopyDataToBuffer(Ipp8u* pucData,
                                         Ipp32u dwLength,
                                         Ipp32u& rdwBytesWrote);
        Status          CopyZerosToBuffer(Ipp32u dwLength);
        Status          GetPlayPos(Ipp32u& rdwPos);
        Status          GetWritePos(Ipp32u& rdwPos);
        Status          Pause(bool bPause);
        Ipp32f          SetVolume(Ipp32f fVolume);
        Ipp32f          GetVolume();
        Status          Reset();
        void            Close();
        virtual Status  DynamicSetParams(const Ipp16u wChannels,
                                         const Ipp32u dwFrequency,
                                         const Ipp16u /*wBytesPerSample*/);
        inline Ipp32u   GetNextWriteOffset()
         {    return m_dwNextWriteOffset;    }

        inline Ipp32f   GetCompensation()
        {    return (Ipp32f) m_dfCompensation;    }

    protected:
        LPDIRECTSOUND8      m_pDS;
        LPDIRECTSOUNDBUFFER m_pDSBSecondary;
        LPDIRECTSOUNDBUFFER m_pDSBPrimary;
        Ipp32u              m_dwDSBufferSize;
        Ipp32u              m_dwNextWriteOffset;
        Ipp64f              m_dfNorm;
        Ipp64f              m_dfCompensation;
        HWND                m_hWnd;
        bool                m_bPausedWaitingData;
        UMC::Mutex          m_MutAccess;
        Ipp32u              m_dwInitedBufferSize;
        Ipp16u              m_wInitedChannels;
        Ipp32u              m_dwInitedFrequency;
        Ipp16u              m_wInitedBytesPerSample;
    };
} // namespace UMC

#endif //#ifdef UMC_ENABLE_DSOUND_AUDIO_RENDER

#endif // __DSOUND_BUF_H__
