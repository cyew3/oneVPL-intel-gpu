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

#ifndef __DX_DRV_H__
#define __DX_DRV_H__

#include <umc_defs.h>

#ifdef UMC_ENABLE_DX_VIDEO_RENDER

#include "drv.h"
#include <ddraw.h>

#include "umc_mutex.h"

typedef enum DDVideoDrvColorFormat
{
    DXVideoDrv_YV12     = 0,
    DXVideoDrv_YUY2     = 1,
    DXVideoDrv_UYVY     = 2,
    DXVideoDrv_NV12     = 3,
    DXVideoDrv_RGB565   = 4
} DDVideoDrvColorFormat;

typedef struct DXDrv
{
    HWND                    win;
    Ipp32s                  win_width;
    Ipp32s                  win_height;

    IDirectDraw*            dd_obj;
    DDVideoDrvColorFormat   dd_fmt;
    bool                    dd_support_colorkey;
    bool                    dd_support_overlay;
    IDirectDrawSurface*     dd_srf_primary;
    IDirectDrawSurface*     dd_srf_front;
    DDSURFACEDESC           dd_srf_desc;
} DXDrv;

typedef struct DXVideoDrvParams
{
    VideoDrvParams          common;
    HWND                    win;
    bool                    use_color_key;
    COLORREF                color_key;
    DDVideoDrvColorFormat   dd_fmt;
} DXVideoDrvParams;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern VIDEO_DRV_INIT_FUNC(umc_dx_Init, driver, params);
    extern VIDEO_DRV_CLOSE_FUNC(umc_dx_Close, driver);

    extern VIDEO_DRV_CREATE_BUFFERS_FUNC(umc_dx_CreateBuffers,
                                         driver, min_b, max_b, bufs,
                                         video_mem_type, video_mem_info);
    extern VIDEO_DRV_FREE_BUFFERS_FUNC(umc_dx_FreeBuffers,
                                       driver, num_b, bufs);

    extern VIDEO_DRV_LOCK_SURFACE_FUNC(umc_dx_LockSurface,driver,srf,planes);
    extern VIDEO_DRV_UNLOCK_SURFACE_FUNC(umc_dx_UnlockSurface, driver, srf);

    extern VIDEO_DRV_RENDER_FRAME_FUNC(umc_dx_RenderFrame, driver, frame, rect);
    extern VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_dx_SetVideoMode,driver,rect,mode);

    /* Function returns <HWND>. */
    extern VIDEO_DRV_GET_WINDOW_FUNC(umc_dx_GetWindow, driver);
    extern VIDEO_DRV_RUN_EVENT_LOOP_FUNC(umc_dx_RunEventLoop, driver);

    extern const VideoDrvSpec DXVideoDrvSpec;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#define UMC_DX_GET_WINDOW(void_win) (HWND)(void_win)

#ifdef UMC_ENABLE_BLT_VIDEO_RENDER

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern VIDEO_DRV_INIT_FUNC(umc_blt_Init, driver, params);
    extern VIDEO_DRV_CLOSE_FUNC(umc_blt_Close, driver);

    extern VIDEO_DRV_CREATE_SURFACE_FUNC(umc_blt_CreateSurface, driver);
    extern VIDEO_DRV_FREE_SURFACE_FUNC(umc_blt_FreeSurface, driver, srf);

    extern VIDEO_DRV_RENDER_FRAME_FUNC(umc_blt_RenderFrame, driver, frame, rect);
    extern VIDEO_DRV_SET_VIDEO_MODE_FUNC(umc_blt_SetVideoMode,driver,rect,mode);

    extern const VideoDrvSpec BLTVideoDrvSpec;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#define UMC_BLT_GET_WINDOW(void_win) UMC_BLT_GET_WINDOW(void_win)

#endif /* defined(UMC_ENABLE_BLT_VIDEO_RENDER) */

#endif /* defined(UMC_ENABLE_DX_VIDEO_RENDER) */
#endif /* __DX_DRV_H__ */
