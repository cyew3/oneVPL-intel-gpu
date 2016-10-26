//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

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
