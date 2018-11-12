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

#ifndef __FW_AUDIO_RENDER_H__
#define __FW_AUDIO_RENDER_H__

#ifdef UMC_ENABLE_FW_AUDIO_RENDER

#include "umc_structures.h"
#include "basic_audio_render.h"
#include "wav_file.h"

namespace UMC
{
    class FWAudioRenderParams: public AudioRenderParams
    {
        DYNAMIC_CAST_DECL(FWAudioRenderParams, AudioRenderParams)

    public:
        // Default constructor
        FWAudioRenderParams();

        vm_char *pOutFile;
    };

    class FWAudioRender :public BasicAudioRender
    {
    public:
        FWAudioRender();
        virtual ~FWAudioRender() {   Close();     }

        virtual Status Init(MediaReceiverParams* pInit);
        virtual Status SendFrame(MediaData* in);
        virtual Status Pause(bool pause);
        virtual Status Close (void);
        virtual Status Reset (void);

        // Render manipulating
        // 0-silence 1-max, return previous volume
        virtual Ipp32f SetVolume(Ipp32f volume);
        virtual Ipp32f GetVolume();

    protected:
        virtual Ipp64f GetTimeTick(void);

        WavFile        m_wav_file;
        WavFile::Info  m_wav_info;
        Ipp64f         m_dfStartPTS;
        vm_tick        m_tickStartTime;
        vm_tick        m_tickPrePauseTime;
        vm_char        pOutFileName[MAXIMUM_PATH];
    };
};  //  namespace UMC

#endif

#endif // __FW_AUDIO_RENDER_H__
