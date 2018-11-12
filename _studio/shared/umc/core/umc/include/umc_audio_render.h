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

#ifndef __UMC_AUDIO_RENDER_H__
#define __UMC_AUDIO_RENDER_H__

#include "vm_time.h"
#include "umc_structures.h"
#include "umc_media_data.h"
#include "umc_module_context.h"
#include "umc_media_receiver.h"

/*
//  Class:       AudioRender
//
//  Notes:       Base abstract class of audio render. Class describes
//               the high level interface of abstract audio render.
//               All system specific (Windows (DirectSound, WinMM), Linux(SDL, etc)) must be implemented in
//               derevied classes.
*/

namespace UMC
{

class AudioRenderParams: public MediaReceiverParams
{
    DYNAMIC_CAST_DECL(AudioRenderParams, MediaReceiverParams)

public:
    // Default constructor
    AudioRenderParams(void);
    // Destructor
    virtual ~AudioRenderParams(void){};

    AudioStreamInfo  info;                                      // (AudioStreamInfo) common fields which necessary to initialize decoder
    ModuleContext *pModuleContext;                              // (ModuleContext *) platform dependent context
};

class AudioRender: public MediaReceiver
{
    DYNAMIC_CAST_DECL(AudioRender, MediaReceiver)
public:
    // Default constructor
    AudioRender(void){}
    // Destructor
    virtual ~AudioRender(void){}

    // Send new portion of data to render
    virtual Status SendFrame(MediaData *in) = 0;

    // Pause(Resume) playing of soundSend new portion of data to render
    virtual Status Pause(bool pause) = 0;

    // Volume manipulating
    virtual float SetVolume(float volume) = 0;
    virtual float GetVolume(void) = 0;

    // Audio Reference Clock
    virtual double GetTime(void) = 0;

    // Estimated value of device latency
    virtual double GetDelay(void) = 0;

    virtual Status SetParams(MediaReceiverParams *pMedia,
                             uint32_t  trickModes = UMC_TRICK_MODES_NO)
    {
        (void)pMedia;
        (void)trickModes;

        return UMC_ERR_NOT_IMPLEMENTED;
    }

};

} // namespace UMC

#endif /* __UMC_AUDIO_RENDER_H__ */
