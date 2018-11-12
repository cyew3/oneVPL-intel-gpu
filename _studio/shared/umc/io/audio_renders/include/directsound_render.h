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

#ifndef __DIRECTSOUND_RENDER_H__
#define __DIRECTSOUND_RENDER_H__

#include "umc_defs.h"

#ifdef UMC_ENABLE_DSOUND_AUDIO_RENDER

#include <dsound.h>
#include "umc_structures.h"
#include "basic_audio_render.h"
#include "pts_buf.h"
#include "dsound_buf.h"

namespace UMC
{
    class DSoundAudioRender:public BasicAudioRender
    {
    public:
        DSoundAudioRender();
        virtual ~DSoundAudioRender(void);

        virtual Status  Init(MediaReceiverParams* pInit);
        virtual Status  SendFrame(MediaData* in);
        virtual Status  Pause(bool);

        // Render manipulating
        virtual Ipp32f  SetVolume(Ipp32f volume);   /* 0-silence 1-max, return previous volume */
        virtual Ipp32f  GetVolume();

        virtual Status  Close (void);
        virtual Status  Reset (void);

        virtual Status  SetParams(MediaReceiverParams *params,
                                  Ipp32u  trickModes = UMC_TRICK_MODES_NO);
    protected:
        static const Ipp32u m_cuiNumOfFrames;
        virtual Status  DrainBuffer();

        HANDLE          m_hTimer;
        BufPTSArray     m_AudioPtsArray;
        DSBuffer        m_DSBuffer;
        Ipp32u          m_dwSampleAlign;

        virtual Ipp64f  GetTimeTick(void);
    };  //class DirectSoundRender

};  //  namespace UMC

#endif //  __DIRECTSOUND_RENDER_H__

#endif
