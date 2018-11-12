// Copyright (c) 2006-2018 Intel Corporation
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

#ifndef __UNIFIED_VIDEO_RENDER_H__
#define __UNIFIED_VIDEO_RENDER_H__

#include <umc_defs.h>
#include "drv.h"
#include "base_video_render.h"

namespace UMC
{

#define FLAG_VREN_DEFAULTRECT 0x00001000

class UnifiedVideoRenderParams: public VideoRenderParams
{
    DYNAMIC_CAST_DECL(UnifiedVideoRenderParams, VideoRenderParams)

public:
    /* Default constructor. */
    UnifiedVideoRenderParams();

    /* Specific constructor. */
    UnifiedVideoRenderParams(VideoDrvSpec *pDrvSpec, void* pDrvParams);

    UnifiedVideoRenderParams &operator=(VideoRenderParams &From);

    /* Video driver specification. */
    VideoDrvSpec*           m_pDrvSpec;
    /* Video memory type to use. */
    VideoDrvVideoMemType    m_VideoMemType;
    /* Video memory information. */
    VideoDrvVideoMemInfo*   m_pVideoMemInfo;
    /* User defined parameters. */
    void*                   m_pDrvParams;
};

class UnifiedVideoRender: public BaseVideoRender
{
public:
    /* Default constructor. */
    UnifiedVideoRender(void);
    /* Destructor. */
    virtual ~UnifiedVideoRender(void);

    /* Initialize the render. */
    virtual Status Init(MediaReceiverParams* pInit);

    /*  Temporary stub, will be removed in future. */
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(&rInit); }

    /* Peek presentation of next frame, return presentation time. */
    virtual Status GetRenderFrame(Ipp64f *pTime);

    /* Functions to render a video frame. */
    virtual Status RenderFrame(void);

    /* Resize the display rectangular. */
    virtual Status ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range);

    /* Show the last rendered frame. */
    virtual Status ShowLastFrame(void);

    /* SDLVideoRender extensions. */
    virtual Status SetFullScreen(ModuleContext& rContext, bool bFullScreen);

    /* Close the render. */
    virtual Status Close(void);

    /* Get window descriptor. */
    virtual void* GetWindow(void);

    /* Run event loop in the current thread. */
    virtual Status RunEventLoop(void);

protected:
    virtual Ipp32s LockSurface(Ipp8u** vidmem);
    virtual Ipp32s UnlockSurface(Ipp8u** vidmem);

    void GetDriverBuffers(void* buffers[MAX_FRAME_BUFFERS]);

    /* Video driver. */
    VideoDriver m_Driver;
    /* Rendering rectangle in the window. */
    bool        m_bDefaultRendering;
};

} /* namespace UMC */

#endif /* __UNIFIED_VIDEO_RENDER_H__ */
