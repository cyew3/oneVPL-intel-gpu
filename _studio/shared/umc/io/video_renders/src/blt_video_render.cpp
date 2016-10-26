//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "blt_video_render.h"

#if defined(UMC_ENABLE_DX_VIDEO_RENDER) && defined(UMC_ENABLE_BLT_VIDEO_RENDER)

namespace UMC
{

/* ------------------------------------------------------------------------- */

#define MODULE          "BLTVideoRender:"
#define FUNCTION        "<Undefined Function>"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

/* ------------------------------------------------------------------------- */

BLTVideoRender::BLTVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "BLTVideoRender"
    DBG_SET("+");
    DBG_SET("-");
}

/* ------------------------------------------------------------------------- */

BLTVideoRender::~BLTVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "~BLTVideoRender"
    DBG_SET("+");
    Close();
    DBG_SET("-");
}

/* ------------------------------------------------------------------------- */

Status BLTVideoRender::Init(MediaReceiverParams* pInit)
{
#undef  FUNCTION
#define FUNCTION "Init"
    DBG_SET("+");
    Status                      umcRes  = UMC_OK;
    DXVideoRenderParams*        pParams = DynamicCast<DXVideoRenderParams>(pInit);
    UnifiedVideoRenderParams    UnifiedParams;
    DXVideoDrvParams            DrvParams;

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
        case YV12:
            DrvParams.dd_fmt = DXVideoDrv_YV12;
            break;
        case YUY2:
            DrvParams.dd_fmt = DXVideoDrv_YUY2;
            break;
        case UYVY:
            DrvParams.dd_fmt = DXVideoDrv_UYVY;
            break;
        case NV12:
            DrvParams.dd_fmt = DXVideoDrv_NV12;
            break;
        case RGB565:
            DrvParams.dd_fmt = DXVideoDrv_RGB565;
            break;
        default:
            umcRes = UMC_ERR_INVALID_STREAM;
            break;
        }
    }
    if (UMC_OK == umcRes)
    {
        DrvParams.use_color_key     = (FLAG_VREN_USECOLORKEY & pParams->lFlags)? true: false;
        DrvParams.color_key         = pParams->m_ColorKey;
        DrvParams.win               = pParams->m_hWnd;
        DrvParams.common.img_width  = m_OutDataTemplate.GetWidth();
        DrvParams.common.img_height = m_OutDataTemplate.GetHeight();
        DrvParams.common.win_rect.x = pParams->disp.left;
        DrvParams.common.win_rect.y = pParams->disp.top;
        DrvParams.common.win_rect.w = pParams->disp.right  - pParams->disp.left;
        DrvParams.common.win_rect.h = pParams->disp.bottom - pParams->disp.top;
        /* Copying parameters specified in <pParams> to <UnifiedParams>. */
        UnifiedParams = *pParams;
        /* Setting SDL driver specification. */
        UnifiedParams.m_pDrvSpec    = (VideoDrvSpec*) &BLTVideoDrvSpec;
        /* Setting SDL driver parameters. */
        UnifiedParams.m_pDrvParams  = &DrvParams;
    }
    if (UMC_OK == umcRes)
    {
        umcRes = UnifiedVideoRender::Init(&UnifiedParams);
    }
    DBG_SET("-");
    return umcRes;

} // Status BLTVideoRender::Init(MediaReceiverParams* pInit)

/* ------------------------------------------------------------------------- */

} // namespace UMC

#endif // defined(UMC_ENABLE_DX_VIDEO_RENDER) && defined(UMC_ENABLE_BLT_VIDEO_RENDER)
