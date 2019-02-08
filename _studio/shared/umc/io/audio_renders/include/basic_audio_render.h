// Copyright (c) 2003-2019 Intel Corporation
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

#ifndef __BASIC_AUDIO_RENDER_H__
#define __BASIC_AUDIO_RENDER_H__

#include "umc_audio_render.h"
#include "umc_cyclic_buffer.h"
#include "umc_event.h"

namespace UMC
{
    class BasicAudioRender: public AudioRender
    {
        DYNAMIC_CAST_DECL(BasicAudioRender, AudioRender)
    public:
        BasicAudioRender();
        virtual ~BasicAudioRender();

        virtual Status Init(MediaReceiverParams* pInit);
        virtual Status Close();
        /* to feed the renderer with audio data */
        virtual Status LockInputBuffer(MediaData *in);
        virtual Status UnLockInputBuffer(MediaData *in, Status StreamStatus = UMC_OK);
        /* to reset internal buffer with audio data */
        virtual Status Reset();
        /* to stop rendering (to kill internal thread) */
        virtual Status Stop();
        /* to render the rest of audio data be kept in buffer */
        virtual Status Pause(bool pause);

        virtual Ipp64f GetTime();
        virtual Ipp64f GetDelay();

        void ThreadProc();

        virtual Status SetParams(MediaReceiverParams *params,
                                 Ipp32u trickModes = UMC_TRICK_MODES_NO);

    protected:
        LinearBuffer m_DataBuffer;
        virtual Ipp64f GetTimeTick() = 0;
        virtual Status GetTimeTick(Ipp64f pts);
        /* to render the rest of audio data be kept in buffer */
        virtual Status DrainBuffer() { return UMC_OK; }

        Ipp64f  m_dfNorm;
        vm_tick m_tStartTime;
        vm_tick m_tStopTime;
        vm_tick m_tFreq;
        Thread  m_Thread;
        bool    m_bStop;
        bool    m_bPause;

        UMC::Event m_eventSyncPoint;
        UMC::Event m_eventSyncPoint1;

        bool    m_RendererIsStopped;
        Ipp32s  m_wInitedChannels;
        Ipp32s  m_dwInitedFrequency;
        Ipp64f  m_dDynamicChannelPTS;
        Ipp32s  m_wDynamicChannels;

    }; // class BasicAudioRender

} // namespace UMC

#endif // __BASIC_AUDIO_RENDER_H__
