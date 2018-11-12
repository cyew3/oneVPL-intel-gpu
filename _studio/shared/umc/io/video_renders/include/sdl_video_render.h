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

#ifndef __SDL_VIDEO_RENDER_H__
#define __SDL_VIDEO_RENDER_H__

#include <umc_defs.h>

#ifdef UMC_ENABLE_SDL_VIDEO_RENDER

#include "unified_video_render.h"
#include "sdl_drv.h"

/* Render name: SDL video render.
 *
 * OS dependencies:
 *  - Linux;
 *  - Win32.
 *
 * Libraries dependencies:
 *  - SDL.
 *
 * For more information see SDL driver notes (sdl_drv.h), UMC documentation and
 * SDL manual.
 */

namespace UMC
{

class SDLVideoRender: public UnifiedVideoRender
{
public:
    // Default constructor
    SDLVideoRender();
    // Destructor
    virtual ~SDLVideoRender();

    // Initialize the render
    virtual Status Init(MediaReceiverParams* pInit);

    //  Temporary stub, will be removed in future
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(&rInit); }

    // Peek presentation of next frame, return presentation time
    virtual Status GetRenderFrame(Ipp64f *pTime);
};

} // namespace UMC

#endif // UMC_ENABLE_SDL_VIDEO_RENDER

#endif // __SDL_VIDEO_RENDER_H__


