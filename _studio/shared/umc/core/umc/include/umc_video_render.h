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

#ifndef ___UMC_VIDEO_RENDER_H___
#define ___UMC_VIDEO_RENDER_H___

#include <memory.h>
#include "vm_debug.h"
#include "umc_media_receiver.h"
#include "vm_semaphore.h"
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_module_context.h"
#include "umc_video_data.h"

namespace UMC
{

#if defined(_WIN32) || defined(_WIN32_WCE)

void UMCRect2Rect(UMC::sRECT& umcRect, ::RECT& rect);
void Rect2UMCRect(::RECT& rect, UMC::sRECT& umcRect);

#endif // defined(_WIN32) || defined(_WIN32_WCE)

class VideoRenderParams: public MediaReceiverParams
{
    DYNAMIC_CAST_DECL(VideoRenderParams, MediaReceiverParams)

public:
    // Default constructor
    VideoRenderParams();

    ColorFormat color_format;       // (UMC::ColorFormat) working color format
    ClipInfo    info;               // (UMC::ClipInfo) video size

    // NOTE: next parameter should replace in future 'color_format' and 'clip_info'
    VideoData   out_data_template;  // (UMC::VideoData) image information: color format, video size, etc

    UMC::sRECT   disp;               // (UMC::sRECT) display position
    UMC::sRECT   range;              // (UMC::sRECT) range of source video to display
    uint32_t      lFlags;             // (uint32_t) video render flag(s)
};

class VideoRender: public MediaReceiver
{
    DYNAMIC_CAST_DECL(VideoRender, MediaReceiver)

public:

    // Destructor
    virtual ~VideoRender()
    {
        Close();
    }

    struct sCaps
    {
        double min_stretch;
        double max_stretch;
        bool   overlay_support;
        bool   colorkey_support;
    };

    // Initialize the render
    virtual Status Init(MediaReceiverParams *) {return UMC_OK;};

    // VideoRender interface extension above MediaReceiver

    // Peek presentation of next frame, return presentation time
    virtual Status GetRenderFrame(double *time) = 0;

    // Rendering the current frame
    virtual Status RenderFrame() = 0;

    // Show the last rendered frame
    virtual Status ShowLastFrame() = 0;

    // Set/Reset full screen playback
    virtual Status SetFullScreen(ModuleContext& rContext, bool full) = 0;

    // Resize the display rectangular
    virtual Status ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range) = 0;

    // Show/Hide Surface
    virtual void ShowSurface() = 0;

    virtual void HideSurface() = 0;

    virtual Status PrepareForRePosition()
    {return UMC_OK;};

protected:
    virtual int32_t LockSurface(uint8_t **vidmem) = 0;
    virtual int32_t UnlockSurface(uint8_t**vidmem) = 0;

    VideoData  m_OutDataTemplate;
};

} // namespace UMC

#endif // ___UMC_VIDEO_RENDER_H___
