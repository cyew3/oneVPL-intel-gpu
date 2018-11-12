// Copyright (c) 2001-2018 Intel Corporation
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

#ifndef __SDL_DRV_H__
#define __SDL_DRV_H__

#include <umc_defs.h>

#ifdef UMC_ENABLE_SDL_VIDEO_RENDER

#include "drv.h"
#include <SDL/SDL.h>

typedef struct SDLDrv
{
    SDL_Surface*    win;
    Ipp32s          win_width;
    Ipp32s          win_height;

    Uint32          sdl_fmt;
} SDLDrv;

typedef struct SDLVideoDrvParams
{
    VideoDrvParams  common;     /* common parameters */
    Uint32          sdl_fmt;
} SDLVideoDrvParams;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern VIDEO_DRV_INIT_FUNC(umc_sdl_Init, driver, params);
    extern VIDEO_DRV_CLOSE_FUNC(umc_sdl_Close, driver);

    extern VIDEO_DRV_CREATE_SURFACE_FUNC(umc_sdl_CreateSurface, driver);
    extern VIDEO_DRV_FREE_SURFACE_FUNC(umc_sdl_FreeSurface, driver, srf);

    extern VIDEO_DRV_LOCK_SURFACE_FUNC(umc_sdl_LockSurface,driver,srf,planes);
    extern VIDEO_DRV_UNLOCK_SURFACE_FUNC(umc_sdl_UnlockSurface, driver, srf);

    extern VIDEO_DRV_RENDER_FRAME_FUNC(umc_sdl_RenderFrame, driver, frame, rect);
    extern VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_sdl_SetVideoMode,driver,rect,mode);

    /* Function returns <SDL_Surface*>. */
    extern VIDEO_DRV_GET_WINDOW_FUNC(umc_sdl_GetWindow, driver);
    extern VIDEO_DRV_RUN_EVENT_LOOP_FUNC(umc_sdl_RunEventLoop, driver);

    extern const VideoDrvSpec SDLVideoDrvSpec;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#define UMC_SDL_GET_WINDOW(void_win) (SDL_Surface*)(void_win)

#endif /* defined(UMC_ENABLE_SDL_VIDEO_RENDER) */

#endif /* __SDL_DRV_H__ */

