//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "sdl_video_render.h"

#ifdef UMC_ENABLE_SDL_VIDEO_RENDER

#include <ipps.h>

namespace UMC
{
#define MODULE          "SDLVideoRender:"
#define FUNCTION        "<Undefined Function>"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

SDLVideoRender::SDLVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "SDLVideoRender"
    DBG_SET("+");
    DBG_SET("-");
}

/* ------------------------------------------------------------------------- */

SDLVideoRender::~SDLVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "~SDLVideoRender"
    DBG_SET("+");
    Close();
    DBG_SET("-");
}

/* ------------------------------------------------------------------------- */

Status SDLVideoRender::Init(MediaReceiverParams* pInit)
{
#undef  FUNCTION
#define FUNCTION "Init"
    DBG_SET("+");
    Status                      umcRes  = UMC_OK;
    VideoRenderParams*          pParams = DynamicCast<VideoRenderParams>(pInit);
    UnifiedVideoRenderParams    UnifiedParams;
    SDLVideoDrvParams           DrvParams;

    if (NULL == pParams)
    {
        umcRes = UMC_ERR_NULL_PTR;
    }
    if (UMC_OK == umcRes)
    {
        // NOTE: next Init will be removed in future.
        if (NONE == pParams->out_data_template.GetColorFormat())
            umcRes = m_OutDataTemplate.Init(pParams->info.width,
                                            pParams->info.height,
                                            pParams->color_format);
        else
            m_OutDataTemplate = pParams->out_data_template;

        /* Checking color format to be used. */
        switch (m_OutDataTemplate.GetColorFormat())
        {
        case UYVY:
            DrvParams.sdl_fmt = SDL_UYVY_OVERLAY;
            break;
        case YUY2:
            DrvParams.sdl_fmt = SDL_YUY2_OVERLAY;
            break;
        case YV12:
            DrvParams.sdl_fmt = SDL_YV12_OVERLAY;
            break;
        default:
            umcRes = UMC_ERR_INVALID_STREAM;
            break;
        }
    }
    if (UMC_OK == umcRes)
    {
        DrvParams.common.img_width  = m_OutDataTemplate.GetWidth();
        DrvParams.common.img_height = m_OutDataTemplate.GetHeight();
        DrvParams.common.win_rect.x = pParams->disp.left;
        DrvParams.common.win_rect.y = pParams->disp.top;
        DrvParams.common.win_rect.w = pParams->disp.right  - pParams->disp.left;
        DrvParams.common.win_rect.h = pParams->disp.bottom - pParams->disp.top;
        /* Copying parameters specified in <pParams> to <UnifiedParams>. */
        UnifiedParams = *pParams;
        /* Setting SDL driver specification. */
        UnifiedParams.m_pDrvSpec    = (VideoDrvSpec*) &SDLVideoDrvSpec;
        /* Setting SDL driver parameters. */
        UnifiedParams.m_pDrvParams  = &DrvParams;
    }
    if (UMC_OK == umcRes)
    {
        umcRes = UnifiedVideoRender::Init(&UnifiedParams);
    }
    DBG_SET("-");
    return umcRes;

} // Status SDLVideoRender::Init(MediaReceiverParams* pInit)

/* ------------------------------------------------------------------------- */

Status SDLVideoRender::GetRenderFrame(Ipp64f *pTime)
{
#undef  FUNCTION
#define FUNCTION "GetRenderFrame"
    DBG_SET("+");
    /* Process SDL window events.
     * Note: there are problems with event handling from different threads on
     * some platforms (known platforms: Win32).
     */
    UnifiedVideoRender::RunEventLoop();

    DBG_SET("-");
    return BaseVideoRender::GetRenderFrame(pTime);
} // Status SDLVideoRender::GetRenderFrame(Ipp64f*)

/* ------------------------------------------------------------------------- */

} // namespace UMC

#endif // ifdef UMC_ENABLE_SDL_VIDEO_RENDER

