//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2010 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>

#ifndef LINUX32
#pragma warning(disable: 4206)
#endif

#ifdef UMC_ENABLE_SDL_VIDEO_RENDER

#include "sdl_drv.h"
#include <stdio.h>
#include <ipps.h>

/* SDL functions return next values: */
#define UMC_SDL_ERR -1
#define UMC_SDL_OK  0

#define MODULE              "SDL(video driver)"
#define FUNCTION            "<Undefined Function>"
#define ERR_STRING(msg)     VIDEO_DRV_ERR_STRING(MODULE, FUNCTION, msg)
#define ERR_SET(err, msg)   VIDEO_DRV_ERR_SET   (MODULE, err, FUNCTION, msg)
#define WRN_SET(err, msg)   VIDEO_DRV_WRN_SET   (MODULE, err, FUNCTION, msg)
#define DBG_SET(msg)        VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

const VideoDrvSpec SDLVideoDrvSpec =
{
    VM_STRING(MODULE),
    VideoDrvTrue,
    VideoDrvVideoMemLibrary,
    umc_sdl_Init,
    umc_sdl_Close,
    umc_vdrv_CreateBuffers,
    umc_vdrv_FreeBuffers,
    umc_sdl_CreateSurface,
    umc_sdl_FreeSurface,
    umc_sdl_LockSurface,
    NULL,               // Unlock screen.
    umc_sdl_RenderFrame,
    umc_sdl_SetVideoMode,
    umc_sdl_GetWindow,
    umc_sdl_RunEventLoop
};

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_INIT_FUNC (umc_sdl_Init, driver, params)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_Init"
    vm_status           result  = VM_OK;
    Ipp32s              sdl_err = UMC_SDL_OK;
    Uint32              subsystems_init;
    SDLDrv*             drv     = (SDLDrv*)ippsMalloc_8u(sizeof(SDLDrv));
    SDLVideoDrvParams*  pParams = (SDLVideoDrvParams*) params;

    DBG_SET("+");
    if (result == VM_OK)
    {
        if (drv == NULL)
        {
            ERR_SET(VM_OPERATION_FAILED, "memory allocation");
        }
    }
    if (result == VM_OK)
    {
        if ((NULL == driver) || (NULL == pParams))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (result == VM_OK)
    {
        driver->m_pDrv          = drv;
        driver->img_width       = pParams->common.img_width;
        driver->img_height      = pParams->common.img_height;
        driver->m_VideoMemType  = VideoDrvVideoMemLibrary;
        ippsZero_8u((Ipp8u*)&(driver->m_VideoMemInfo), sizeof(VideoDrvVideoMemInfo));
        ippsCopy_8u((const Ipp8u*)&SDLVideoDrvSpec, (Ipp8u*)&(driver->m_DrvSpec), sizeof(VideoDrvSpec));
        /* Setting display rectangle (x,y - can't be set in SDL). */
        drv->win_width  = pParams->common.win_rect.w;
        drv->win_height = pParams->common.win_rect.h;
        drv->sdl_fmt    = pParams->sdl_fmt;
    }
    if (result == VM_OK)
    {
        /* SDL-Video initialization. */
        subsystems_init = SDL_WasInit(SDL_INIT_EVERYTHING);
        if (!subsystems_init)
            sdl_err = SDL_Init(SDL_INIT_VIDEO);
        else if (!(SDL_INIT_VIDEO & subsystems_init))
            sdl_err = SDL_InitSubSystem(SDL_INIT_VIDEO);
        if (sdl_err != UMC_SDL_OK)
        {
            ERR_SET(VM_OPERATION_FAILED, "SDL_Init(SubSystem)");
        }
    }
    if (result == VM_OK)
    {
        result = umc_sdl_SetVideoMode(driver,
                                      &(pParams->common.win_rect),
                                      VideoDrvOrdinary);
    }
    if (VM_OK != result)
    {
        if (drv != NULL) { ippsFree(drv); driver->m_pDrv = NULL; }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_sdl_SetVideoMode, driver, rect, mode)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_SetVideoMode"
    vm_status   result  = VM_OK;
    Uint32      uiFlags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ANYFORMAT;
    SDLDrv*     drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);

    DBG_SET("+");
    if (VM_OK == result)
    {
        if (NULL == drv)
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        if (VideoDrvFullScreen == mode) uiFlags |= SDL_FULLSCREEN;

        drv->win_width  = rect->w;
        drv->win_height = rect->h;
        drv->win = SDL_SetVideoMode(drv->win_width,drv->win_height,0,uiFlags);
        if (NULL == drv->win)
        {
            ERR_SET(VM_OPERATION_FAILED, "SDL_SetVideoMode");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_CLOSE_FUNC(umc_sdl_Close, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_Close"
    vm_status   result  = VM_OK;
    SDLDrv*     drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    Uint32      subsystems_init;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        /* Deinitialize SDL-Video. */
        subsystems_init = SDL_WasInit(SDL_INIT_EVERYTHING);
        if (SDL_INIT_EVERYTHING & (!SDL_INIT_VIDEO) & subsystems_init)
            SDL_Quit();
        else if (SDL_INIT_VIDEO & subsystems_init)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ippsFree(drv);
        driver->m_pDrv = NULL;
    }
    DBG_SET("-");
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_CREATE_SURFACE_FUNC(umc_sdl_CreateSurface, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_CreateSurface"
    vm_status   result      = VM_OK;
    void*       alloc_srf   = NULL;
    SDLDrv*     drv         = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    else
        alloc_srf = SDL_CreateYUVOverlay(driver->img_width,
                                         driver->img_height,
                                         drv->sdl_fmt,
                                         drv->win);
    DBG_SET("-");
    return alloc_srf;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_FREE_SURFACE_FUNC(umc_sdl_FreeSurface, driver, srf)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_FreeSurface"
    vm_status       result  = VM_OK;
    SDLDrv*         drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    SDL_Overlay*    overlay = (SDL_Overlay*) srf;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (NULL != overlay) SDL_FreeYUVOverlay(overlay);
    }
    DBG_SET("-");
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_LOCK_SURFACE_FUNC(umc_sdl_LockSurface, driver, srf, planes)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_LockSurface"
    vm_status       result  = VM_OK;
    SDLDrv*         drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    SDL_Overlay*    overlay = (SDL_Overlay*) srf;
    Ipp32s          i;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (SDL_LockYUVOverlay(overlay) != UMC_SDL_OK)
            WRN_SET(VM_OPERATION_FAILED, "SDL_LockYUVOverlay");
    }
    if (VM_OK == result)
    {
        /* This asertion shouldn't occured. */
        VM_ASSERT(overlay->planes <= VIDEO_DRV_MAX_PLANE_NUMBER);
        for (i = 0;  i < VIDEO_DRV_MAX_PLANE_NUMBER; ++i)
        {
            if (i < overlay->planes)
            {
                planes[i].m_pPlane = overlay->pixels[i];
                planes[i].m_nPitch = overlay->pitches[i];
            }
            else
            {
                planes[i].m_pPlane = NULL;
                planes[i].m_nPitch = 0;
                break;
            }
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_UNLOCK_SURFACE_FUNC (umc_sdl_UnlockSurface, driver, srf)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_UnlockSurface"
    vm_status       result  = VM_OK;
    SDLDrv*         drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    SDL_Overlay*    overlay = (SDL_Overlay*) srf;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        SDL_UnlockYUVOverlay(overlay);
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_RENDER_FRAME_FUNC (umc_sdl_RenderFrame, driver, frame, rect)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_RenderFrame"
    vm_status       result  = VM_OK;
    SDLDrv*         drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    SDL_Overlay*    overlay = (SDL_Overlay*) frame;
    SDL_Rect        view_port;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (NULL != rect)
        {
            view_port.x = (Sint16)rect->x;
            view_port.y = (Sint16)rect->y;
            view_port.w = (Uint16)rect->w;
            view_port.h = (Uint16)rect->h;
        }
        else
        {
            view_port.x = 0;
            view_port.y = 0;
            view_port.w = (Uint16)drv->win_width;
            view_port.h = (Uint16)drv->win_height;
        }
        if (SDL_DisplayYUVOverlay(overlay, &view_port) != UMC_SDL_OK)
        {
            ERR_SET(VM_OPERATION_FAILED, "SDL_DisplayYUVOverlay");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_GET_WINDOW_FUNC(umc_sdl_GetWindow, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_GetWindow"
    void*   win;
    SDLDrv* drv = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);

    DBG_SET("+");
    if (NULL == drv)
    {
        win = NULL;
        DBG_SET("null ptr");
    }
    else
    {
        win = drv->win;
    }
    DBG_SET("-");
    return win;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_RUN_EVENT_LOOP_FUNC(umc_sdl_RunEventLoop, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_sdl_RunEventLoop"
    vm_status       result  = VM_OK;
    SDLDrv*         drv     = (SDLDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    SDL_Event       event;
    VideoDrvRect    rect;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                break;
            case SDL_VIDEORESIZE:
                rect.x = 0;
                rect.w = event.resize.w;
                rect.y = 0;
                rect.h = event.resize.h;
                umc_sdl_SetVideoMode(driver,&rect,VideoDrvOrdinary);
                break;
            default:
                break;
            }
        }
    }
    DBG_SET("-");
    return result;
}

#endif /* defined(UMC_ENABLE_SDL_VIDEO_RENDER) */
