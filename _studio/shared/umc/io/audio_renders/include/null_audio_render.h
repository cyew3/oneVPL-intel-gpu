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
//    This file contains audio render doing nothing beside returning right time
//    based on system time. You can use it for pure video (without audio)
//    streams syncronization.
//

#ifndef __NULL_AUDIO_RENDER_H__
#define __NULL_AUDIO_RENDER_H__

#include "vm_time.h"
#include "umc_structures.h"
#include "basic_audio_render.h"

namespace UMC
{
    typedef enum
    {
        UMC_1X_FFMODE = 0,
        UMC_2X_FFMODE = 1,
        UMC_4X_FFMODE = 3,
        UMC_8X_FFMODE = 7,
        UMC_16X_FFMODE = 15
    } UMC_FFMODE_MULTIPLIER;

    class NULLAudioRenderParams: public AudioRenderParams
    {
        DYNAMIC_CAST_DECL(NULLAudioRenderParams, AudioRenderParams)

    public:
        // Default constructor
        NULLAudioRenderParams(Ipp64f m_PTS = 0);

        Ipp64f          m_InitPTS;
    };

    class NULLAudioRender : public BasicAudioRender
    {
    protected:
        vm_tick         m_Freq;
        vm_tick         m_LastStartTime;
        vm_tick         m_PrePauseTime;
        bool            m_bPaused;
        Ipp64f          m_dInitialPTS;
        Ipp64f          m_uiFFModeMult;
        virtual Ipp64f  GetTimeTick();

    public:
        NULLAudioRender(Ipp64f dfDelay = 0,
                        UMC_FFMODE_MULTIPLIER mult = UMC_1X_FFMODE);
        virtual ~NULLAudioRender() {   Close();     }

        virtual Status  Init(MediaReceiverParams* pInit);
        virtual Status  SendFrame(MediaData* in);
        virtual Status  Pause(bool bPause);

        // Render manipulating
        // 0-silence 1-max, return previous volume
        virtual Ipp32f  SetVolume(Ipp32f /*volume*/);
        virtual Ipp32f  GetVolume();

        virtual Status  Close ();
        virtual Status  Reset ();

        virtual Status SetParams(MediaReceiverParams *params,
                                 Ipp32u  trickModes = UMC_TRICK_MODES_NO);
    };
} // namespace UMC

#endif // __NULL_AUDIO_RENDER_H__
