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

#ifdef UMC_ENABLE_DX_VIDEO_RENDER

/* -------------------------------------------------------------------------- */

#include "dx_drv.h"
#include <stdio.h>
#include <ipps.h>

/* -------------------------------------------------------------------------- */

#define MODULE              "DX(video driver)"
#define FUNCTION            "<Undefined Function>"
#define ERR_STRING(msg)     VIDEO_DRV_ERR_STRING(MODULE, FUNCTION, msg)
#define ERR_SET(err, msg)   VIDEO_DRV_ERR_SET   (MODULE, err, FUNCTION, msg)
#define WRN_SET(err, msg)   VIDEO_DRV_WRN_SET   (MODULE, err, FUNCTION, msg)
#define DBG_SET(msg)        VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

/* -------------------------------------------------------------------------- */

const VideoDrvSpec DXVideoDrvSpec =
{
    VM_STRING(MODULE),
    VideoDrvFalse,
    VideoDrvVideoMemLibrary,
    umc_dx_Init,
    umc_dx_Close,
    umc_dx_CreateBuffers,
    umc_dx_FreeBuffers,
    NULL,
    NULL,
    umc_dx_LockSurface,
    umc_dx_UnlockSurface,
    umc_dx_RenderFrame,
    umc_dx_SetVideoMode,
    umc_dx_GetWindow,
    NULL
};

/* -------------------------------------------------------------------------- */

#define INIT_DIRECTDRAW_STRUCT(x) (ZeroMemory(&x, sizeof(x)), x.dwSize=sizeof(x))

/* -------------------------------------------------------------------------- */

const DDPIXELFORMAT umc_dx_overlay_formats[] =
{
    {sizeof(DDPIXELFORMAT), DDPF_FOURCC | DDPF_YUV, MAKEFOURCC('Y','V','1','2'), 12, 0, 0, 0, 0},   /* YV12 */
    {sizeof(DDPIXELFORMAT), DDPF_FOURCC | DDPF_YUV, MAKEFOURCC('Y','U','Y','2'), 16, 0, 0, 0, 0},   /* YUY2 */
    {sizeof(DDPIXELFORMAT), DDPF_FOURCC | DDPF_YUV, MAKEFOURCC('U','Y','V','Y'), 16, 0, 0, 0, 0},   /* UYVY */
    {sizeof(DDPIXELFORMAT), DDPF_FOURCC | DDPF_YUV, MAKEFOURCC('N','V','1','2'), 12, 0, 0, 0, 0},   /* NV12 */
    {sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0}                             /* RGB565 */
};

/* -------------------------------------------------------------------------- */

vm_status umc_dx_InitDDObjs (VideoDriver *driver)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_InitDDObjs"
    vm_status   result  = VM_OK;
    DXDrv*      drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    DDCAPS      caps;

    DBG_SET("+");
    if (drv == NULL)
    {
        ERR_SET(VM_OPERATION_FAILED, "null ptr");
    }
    if (VM_OK == result)
    {
        /* Creating DDraw object:
         *  NULL        - indicates active display driver,
         *  drv->dd_obj - DD object what will be created,
         *  NULL        - unused now (should be set to NULL).
         */
        if (FAILED(::DirectDrawCreate(NULL, &(drv->dd_obj), NULL)))
        {
            drv->dd_obj = NULL;
            ERR_SET(VM_OPERATION_FAILED, "DirectDrawCreate");
        }
    }
    if (VM_OK == result)
    {
        INIT_DIRECTDRAW_STRUCT(caps);
        if (FAILED(drv->dd_obj->GetCaps(&caps, NULL)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::GetCaps");
        }
        drv->dd_support_overlay     = (0 != (caps.dwCaps & DDCAPS_OVERLAY));
        drv->dd_support_colorkey    = (0 != (caps.dwCaps & DDCKEYCAPS_DESTOVERLAYYUV));
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_INIT_FUNC (umc_dx_Init, driver, params)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_Init"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)ippsMalloc_8u(sizeof(DXDrv));
    DXVideoDrvParams*   pParams = (DXVideoDrvParams*) params;
    DDCOLORKEY          key;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if (NULL == drv)
        {
            ERR_SET(VM_OPERATION_FAILED, "memory allocation");
        }
    }
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == pParams))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        driver->m_pDrv          = drv;
        driver->img_width       = pParams->common.img_width;
        driver->img_height      = pParams->common.img_height;
        driver->m_VideoMemType  = VideoDrvVideoMemLibrary;
        ippsZero_8u((Ipp8u*)&(driver->m_VideoMemInfo), sizeof(VideoDrvVideoMemInfo));
        ippsCopy_8u((const Ipp8u*)&DXVideoDrvSpec, (Ipp8u*)&(driver->m_DrvSpec), sizeof(VideoDrvSpec));

        ippsZero_8u((Ipp8u*)drv, sizeof(DXDrv));
        drv->win        = pParams->win;
        drv->win_width  = pParams->common.win_rect.w;
        drv->win_height = pParams->common.win_rect.h;
        drv->dd_fmt     = pParams->dd_fmt;
    }
    if (VM_OK == result)
    {
        result = umc_dx_InitDDObjs(driver);
    }
    if (VM_OK == result)
    {
        if ((NULL == drv->dd_obj) || (false == drv->dd_support_overlay))
        {
            ERR_SET(VM_NOT_INITIALIZED, "initialization");
        }
    }
    if (VM_OK == result)
    {
        if (FAILED(drv->dd_obj->SetCooperativeLevel(drv->win, DDSCL_NORMAL)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::SetCooperativeLevel");
        }
    }
    if (VM_OK == result)
    {
        /* Creating the primary surface. */
        INIT_DIRECTDRAW_STRUCT(drv->dd_srf_desc);
        drv->dd_srf_desc.dwFlags        = DDSD_CAPS;
        drv->dd_srf_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

        if (FAILED(drv->dd_obj->CreateSurface(&(drv->dd_srf_desc),
                                              &(drv->dd_srf_primary), 0)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::CreateSurface");
        }
    }
    if (VM_OK == result)
    {
        drv->dd_support_colorkey = pParams->use_color_key;
        if (drv->dd_support_colorkey)
        {
            /* Setting color key. */
            key.dwColorSpaceLowValue    = pParams->color_key;
            key.dwColorSpaceHighValue   = pParams->color_key;
            if (FAILED(drv->dd_srf_primary->SetColorKey(DDCKEY_DESTOVERLAY,&key)))
            {
                ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::SetColorKey");
            }
        }
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
VIDEO_DRV_CREATE_BUFFERS_FUNC(umc_dx_CreateBuffers,
                              driver, min_b, max_b, bufs,
                              video_mem_type, video_mem_info)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_CreateBuffers"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    Ipp32u              bufs_num = 0, i;
    DDSURFACEDESC       ddsdOverlay;
    IDirectDrawSurface* surface = NULL;
    HRESULT             hRes = DD_OK;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == bufs))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        if ((min_b > max_b) || (0 == max_b))
        {
            ERR_SET(VM_OPERATION_FAILED, "number of buffers");
        }
    }
    if (VM_OK == result)
    {
        if (!(driver->m_DrvSpec.drv_videomem_type & video_mem_type))
        {
            ERR_SET(VM_OPERATION_FAILED,"specified video memory type is unsupported");
        }
    }
    if (VM_OK == result)
    {
        /* Copying video memory information. */
        if (NULL != video_mem_info)
        {
            ippsCopy_8u((const Ipp8u*)video_mem_info, (Ipp8u*)&(driver->m_VideoMemInfo),
                        sizeof(VideoDrvVideoMemInfo));
        }
        /* Creating overlay buffers. */
        INIT_DIRECTDRAW_STRUCT(ddsdOverlay);
        ddsdOverlay.ddsCaps.dwCaps  = DDSCAPS_OVERLAY | DDSCAPS_FLIP |
                                      DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
        ddsdOverlay.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                                      DDSD_BACKBUFFERCOUNT| DDSD_PIXELFORMAT;
        ddsdOverlay.dwWidth         = driver->img_width;
        ddsdOverlay.dwHeight        = driver->img_height;
        ddsdOverlay.ddpfPixelFormat = umc_dx_overlay_formats[drv->dd_fmt];

        /* Trying to create max_b, max_b-1, ..., min_b overlays. */
        for (bufs_num = max_b; bufs_num >= min_b; --bufs_num)
        {
            ddsdOverlay.dwBackBufferCount = bufs_num;
            hRes = drv->dd_obj->CreateSurface(&ddsdOverlay, &drv->dd_srf_front, NULL);
            if (SUCCEEDED(hRes))
                break;
        }
        if ((bufs_num < min_b) || FAILED(hRes))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::CreateSurface");
        }
    }
    if (VM_OK == result)
    {
        /* Obtaining chained overlay surfaces. */
        ippsZero_8u((Ipp8u*)bufs, max_b*sizeof(void*));
        bufs[0] = drv->dd_srf_front;
        for (i = 1; i < bufs_num; ++i)
        {
            surface = (IDirectDrawSurface*)(bufs[i-1]);
            hRes = surface->GetAttachedSurface(&ddsdOverlay.ddsCaps,
                                               (IDirectDrawSurface**)&(bufs[i]));
            if (FAILED(hRes))
            {
                ERR_SET(VM_OPERATION_FAILED, "GetAttachedSurface");
                break;
            }
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_FREE_BUFFERS_FUNC(umc_dx_FreeBuffers, driver, num_b, bufs)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_FreeBuffers"
    vm_status   result  = VM_OK;
    DXDrv*      drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    Ipp32u      i;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == drv) || (NULL == bufs))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        if (NULL != drv->dd_srf_front)
        {
            if (FAILED(drv->dd_srf_front->IsLost())) drv->dd_srf_front->Restore();
            drv->dd_srf_front->Release();
            drv->dd_srf_front = NULL;
        }
        if (NULL != drv->dd_srf_primary)
        {
            if (FAILED(drv->dd_srf_primary->IsLost())) drv->dd_srf_primary->Restore();
            drv->dd_srf_primary->Release();
            drv->dd_srf_primary = NULL;
        }
        for (i = 0; i < num_b; ++i) bufs[i] = NULL;
    }
    DBG_SET("-");
    return;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_dx_SetVideoMode,driver,rect,mode)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_SetVideoMode"
    vm_status   result  = VM_OK;
    DXDrv*      drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    Ipp32u      dwFlags;

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
        drv->win_width  = rect->w;
        drv->win_height = rect->h;
        dwFlags = (VideoDrvFullScreen == mode)? DDSCL_EXCLUSIVE     |
                                                DDSCL_FULLSCREEN    |
                                                DDSCL_SETFOCUSWINDOW:
                                                DDSCL_NORMAL;
        if (FAILED(drv->dd_obj->SetCooperativeLevel(drv->win, dwFlags)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::SetCooperativeLevel");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_CLOSE_FUNC (umc_dx_Close, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_Close"
    vm_status   result  = VM_OK;
    DXDrv*      drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (NULL != drv->dd_obj)
        {
            drv->dd_obj->Release();
            drv->dd_obj = NULL;
        }
        ippsFree(drv);
        driver->m_pDrv = NULL;
    }
    DBG_SET("-");
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_LOCK_SURFACE_FUNC(umc_dx_LockSurface,driver,srf,planes)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_LockSurface"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawSurface* overlay = (IDirectDrawSurface*)srf;
    HRESULT             hRes;
    bool                locked = false;
    Ipp32s              i;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        while (false == locked)
        {
            hRes = overlay->Lock(0, &(drv->dd_srf_desc),
                                 DDLOCK_WAIT | DDLOCK_WRITEONLY, 0);
            switch (hRes)
            {
            case DDERR_SURFACELOST:
                drv->dd_srf_primary->Restore();
                overlay->Restore();
                break;
            case DDERR_WASSTILLDRAWING:
                Sleep(0);
                break;
            case DD_OK:
                planes[0].m_pPlane      = drv->dd_srf_desc.lpSurface;
                planes[0].m_nPitch      = drv->dd_srf_desc.lPitch;
                planes[0].m_nMemSize    = drv->dd_srf_desc.dwLinearSize;
                for (i = 1; i < VIDEO_DRV_MAX_PLANE_NUMBER; ++i)
                {
                    planes[i].m_pPlane      = NULL;
                    planes[i].m_nPitch      = 0;
                    planes[i].m_nMemSize    = 0;
                }
                locked = true;
                break;
            default: /* Error occured. */
                ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::Lock");
                locked = true;
                break;
            }
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_UNLOCK_SURFACE_FUNC (umc_dx_UnlockSurface, driver, srf)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_UnlockSurface"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawSurface* overlay = (IDirectDrawSurface*)srf;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    else
    {
        if (FAILED(overlay->Unlock(drv->dd_srf_desc.lpSurface)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::Unlock");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_RENDER_FRAME_FUNC (umc_dx_RenderFrame, driver, frame, rect)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_RenderFrame"
    vm_status           result      = VM_OK;
    DXDrv*              drv         = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawSurface* overlay     = (IDirectDrawSurface*) frame;
    Ipp32u              show_cmd;
    HRESULT             hRes;
    ::RECT              src, dst_rec, dst_win, *dst = &dst_rec;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        show_cmd = (drv->dd_support_overlay)? DDOVER_SHOW : DDOVER_HIDE;
        if (DDOVER_SHOW == show_cmd && drv->dd_support_colorkey)
            show_cmd |= DDOVER_KEYDEST;

        src.left    = 0;
        src.top     = 0;
        src.right   = driver->img_width;
        src.bottom  = driver->img_height;

        /* Getting window rectangle. */
        ::POINT pt = {0,0};
        ::ClientToScreen(drv->win, &pt);
        ::GetClientRect(drv->win, &dst_win);
        dst_win.left   += pt.x;
        dst_win.right  += pt.x;
        dst_win.top    += pt.y;
        dst_win.bottom += pt.y;
        if (NULL != rect)
        {
            dst->left    = rect->x;
            dst->top     = rect->y;
            dst->right   = rect->x + rect->w;
            dst->bottom  = rect->y + rect->h;
        }
        else
        {
            dst = &dst_win;
        }

        hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                       dst, show_cmd, 0);
        if (DDERR_INVALIDRECT == hRes)
        {
            dst = &dst_win;
            /* Too big rectangle for rendering (dst) specified,
             * trying to render in whole window.
             * Note: see bug #4012 for details.
             */
            DBG_SET("Update surface: invalid rectangle");
            hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                          dst, show_cmd, 0);
        }
        if (DDERR_INVALIDRECT == hRes)
        {
            dst = NULL;
            /* Too big rectangle for rendering (dst) specified,
             * trying to use default parameter (dst = NULL).
             * Note: this shouldn't ocuured.
             */
            DBG_SET("Update surface: invalid rectangle");
            hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                          NULL, show_cmd, 0);
        }
        if (DDERR_SURFACELOST == hRes)
        {
            HRESULT hR = DD_OK;
            if (SUCCEEDED(hR) && FAILED(overlay->IsLost()))
                hR = overlay->Restore();
            if (SUCCEEDED(hR) && FAILED(drv->dd_srf_primary->IsLost()))
                hR = drv->dd_srf_primary->Restore();
            if (SUCCEEDED(hR))
            {
                hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                               dst, show_cmd, 0);
                if (DDERR_INVALIDRECT == hRes)
                {
                    dst = &dst_win;
                    DBG_SET("Update surface: invalid rectangle");
                    hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                                dst, show_cmd, 0);
                }
                if (DDERR_INVALIDRECT == hRes)
                {
                    dst = NULL;
                    DBG_SET("Update surface: invalid rectangle");
                    hRes = overlay->UpdateOverlay(&src, drv->dd_srf_primary,
                                                NULL, show_cmd, 0);
                }
            }
            else hRes = hR;
        }
        if (FAILED(hRes))
        {
            ERR_SET(VM_OPERATION_FAILED, "update surface");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_GET_WINDOW_FUNC(umc_dx_GetWindow, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_dx_GetWindow"
    void*   win;
    DXDrv*  drv = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);

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

#ifdef UMC_ENABLE_BLT_VIDEO_RENDER

#undef  MODULE
#define MODULE "BLT(DX driver extension)"

/* -------------------------------------------------------------------------- */

const VideoDrvSpec BLTVideoDrvSpec =
{
    VM_STRING(MODULE),
    VideoDrvFalse,
    VideoDrvVideoMemLibrary,
    umc_blt_Init,
    umc_blt_Close,
    umc_vdrv_CreateBuffers,
    umc_vdrv_FreeBuffers,
    umc_blt_CreateSurface,
    umc_blt_FreeSurface,
    umc_dx_LockSurface,
    umc_dx_UnlockSurface,
    umc_blt_RenderFrame,
    umc_blt_SetVideoMode,
    umc_dx_GetWindow,
    NULL
};

typedef DXDrv               BLTDrv;
typedef DXVideoDrvParams    BLTVideoDrvParams;

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_INIT_FUNC (umc_blt_Init, driver, params)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_Init"
    vm_status           result  = VM_OK;
    BLTDrv*             drv     = (BLTDrv*)ippsMalloc_8u(sizeof(BLTDrv));
    BLTVideoDrvParams*  pParams = (BLTVideoDrvParams*) params;
    IDirectDrawClipper* clipper = NULL;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if (NULL == drv)
        {
            ERR_SET(VM_OPERATION_FAILED, "memory allocation");
        }
    }
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == pParams))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        driver->m_pDrv          = drv;
        driver->img_width       = pParams->common.img_width;
        driver->img_height      = pParams->common.img_height;
        driver->m_VideoMemType  = VideoDrvVideoMemLibrary;
        ippsZero_8u((Ipp8u*)&(driver->m_VideoMemInfo), sizeof(VideoDrvVideoMemInfo));
        ippsCopy_8u((const Ipp8u*)&BLTVideoDrvSpec, (Ipp8u*)&(driver->m_DrvSpec), sizeof(VideoDrvSpec));

        ippsZero_8u((Ipp8u*)drv, sizeof(DXDrv));
        drv->win        = pParams->win;
        drv->win_width  = pParams->common.win_rect.w;
        drv->win_height = pParams->common.win_rect.h;
        drv->dd_fmt     = pParams->dd_fmt;
    }
    if (VM_OK == result)
    {
        result = umc_dx_InitDDObjs(driver);
    }
    if (VM_OK == result)
    {
        if ((NULL == drv->dd_obj) ||
            (false == drv->dd_support_overlay) && (false == drv->dd_support_colorkey))
        {
            ERR_SET(VM_NOT_INITIALIZED, "initialization");
        }
    }
    if (VM_OK == result)
    {
        if (FAILED(drv->dd_obj->SetCooperativeLevel(drv->win, DDSCL_NORMAL)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::SetCooperativeLevel");
        }
    }
    if (VM_OK == result)
    {
        /* Creating the primary surface. */
        INIT_DIRECTDRAW_STRUCT(drv->dd_srf_desc);
        drv->dd_srf_desc.dwFlags        = DDSD_CAPS;
        drv->dd_srf_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

        if (FAILED(drv->dd_obj->CreateSurface(&(drv->dd_srf_desc),
                                              &(drv->dd_srf_primary), 0)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::CreateSurface");
        }
    }
    if (VM_OK == result)
    {
        if (FAILED(drv->dd_obj->CreateClipper(0, &clipper, 0)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::CreateClipper");
        }
    }
    if (VM_OK == result)
    {
        if (FAILED(clipper->SetHWnd(0, drv->win)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDrawClipper::SetHWnd");
        }
    }
    if (VM_OK == result)
    {
        if (FAILED(drv->dd_srf_primary->SetClipper(clipper)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::SetClipper");
        }
    }
    if (VM_OK != result)
    {
        if (drv != NULL) { ippsFree(drv); driver->m_pDrv = NULL; }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_CLOSE_FUNC (umc_blt_Close, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_Close"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawClipper* clipper = NULL;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (NULL != drv->dd_srf_primary)
        {
            if (SUCCEEDED(drv->dd_srf_primary->GetClipper(&clipper)))
                clipper->Release();
        }
    }
    if (VM_OK == result)
    {
        umc_dx_Close(driver);
    }
    DBG_SET("-");
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_blt_SetVideoMode, driver, rect, mode)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_SetVideoMode"
    vm_status           result  = VM_OK;
    DXDrv*              drv     = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawClipper* clipper = NULL;

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
        if (NULL != drv->dd_srf_primary)
        {
            if (SUCCEEDED(drv->dd_srf_primary->GetClipper(&clipper)))
            {
                if (FAILED(clipper->SetHWnd(0, drv->win)))
                {
                    ERR_SET(VM_OPERATION_FAILED, "IDirectDrawClipper::SetHWnd");
                }
            }
            else
            {
                ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::GetClipper");
            }
        }
    }
    if (VM_OK == result)
    {
        umc_dx_SetVideoMode(driver, rect, mode);
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_CREATE_SURFACE_FUNC(umc_blt_CreateSurface, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_CreateSurface"
    vm_status           result      = VM_OK;
    BLTDrv*             drv         = (BLTDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    DDSURFACEDESC       ddsdOverlay;
    IDirectDrawSurface* alloc_srf   = NULL;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        /* Creating overlay buffers. */
        INIT_DIRECTDRAW_STRUCT(ddsdOverlay);
        ddsdOverlay.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
        ddsdOverlay.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
        ddsdOverlay.dwWidth         = driver->img_width;
        ddsdOverlay.dwHeight        = driver->img_height;
        ddsdOverlay.ddpfPixelFormat = umc_dx_overlay_formats[drv->dd_fmt];

        if (FAILED(drv->dd_obj->CreateSurface(&ddsdOverlay, &alloc_srf, NULL)))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDraw::CreateSurface");
        }
    }
    if (VM_OK != result)
    {
        alloc_srf = NULL;
    }
    DBG_SET("-");
    return alloc_srf;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_FREE_SURFACE_FUNC(umc_blt_FreeSurface, driver, srf)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_FreeSurface"
    vm_status           result  = VM_OK;
    BLTDrv*             drv     = (BLTDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawSurface* overlay = (IDirectDrawSurface*)srf;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        if (NULL != overlay) overlay->Release();
    }
    DBG_SET("-");
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_RENDER_FRAME_FUNC (umc_blt_RenderFrame, driver, frame, rect)
{
#undef  FUNCTION
#define FUNCTION "umc_blt_RenderFrame"
    vm_status           result      = VM_OK;
    DXDrv*              drv         = (DXDrv*)((NULL != driver)? driver->m_pDrv: NULL);
    IDirectDrawSurface* overlay     = (IDirectDrawSurface*) frame;
    HRESULT             hRes;
    ::RECT              src, dst_rec, *dst = &dst_rec;

    DBG_SET("+");
    if (NULL == drv)
    {
        ERR_SET(VM_NULL_PTR, "null ptr");
    }
    if (VM_OK == result)
    {
        src.left    = 0;
        src.top     = 0;
        src.right   = driver->img_width;
        src.bottom  = driver->img_height;
        if (NULL != rect)
        {
            dst->left    = rect->x;
            dst->top     = rect->y;
            dst->right   = rect->x + rect->w;
            dst->bottom  = rect->y + rect->h;
        }
        else
        {
            // render to current window
            ::POINT pt = {0,0};
            ::ClientToScreen(drv->win, &pt);
            ::GetClientRect(drv->win, dst);
            dst->left   += pt.x;
            dst->right  += pt.x;
            dst->top    += pt.y;
            dst->bottom += pt.y;
        }
        hRes = drv->dd_srf_primary->Blt(dst, overlay, &src, DDBLT_WAIT, 0);
        if (DDERR_INVALIDRECT == hRes)
        {
            DBG_SET("Blit surface: invalid rectangle");
            hRes = drv->dd_srf_primary->Blt(NULL, overlay, &src, DDBLT_WAIT, 0);
        }
        if (DDERR_SURFACELOST == hRes)
        {
            while (DDERR_SURFACELOST == hRes)
            {
                if (FAILED(overlay->IsLost()))
                    overlay->Restore();
                if (FAILED(drv->dd_srf_primary->IsLost()))
                    drv->dd_srf_primary->Restore();
                hRes = drv->dd_srf_primary->Blt(dst, overlay, &src, DDBLT_WAIT, 0);
                if (DDERR_INVALIDRECT == hRes)
                {
                    DBG_SET("Blit surface: invalid rectangle");
                        hRes = drv->dd_srf_primary->Blt(NULL, overlay, &src, DDBLT_WAIT, 0);
                }
                Sleep(5);
            }
        }
        if (FAILED(hRes))
        {
            ERR_SET(VM_OPERATION_FAILED, "IDirectDrawSurface::Blt");
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

#endif /* defined(UMC_ENABLE_BLT_VIDEO_RENDER) */

#endif // defined(UMC_ENABLE_DX_VIDEO_RENDER)
