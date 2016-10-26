//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

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


