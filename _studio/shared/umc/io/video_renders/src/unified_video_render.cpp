//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#include "unified_video_render.h"
#include <umc_video_data.h>
#include <ipps.h>

namespace UMC
{

#define MODULE          "UnifiedVideoRender:"
#define FUNCTION        "Unknown"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

UnifiedVideoRenderParams::UnifiedVideoRenderParams():
    m_pDrvSpec(NULL),
    m_pDrvParams(NULL),
    m_VideoMemType(VideoDrvVideoMemLibrary),
    m_pVideoMemInfo(NULL)
{
}

/* -------------------------------------------------------------------------- */

UnifiedVideoRenderParams::UnifiedVideoRenderParams(VideoDrvSpec* pDrvSpec,
                                                   void* pDrvParams)
{
    m_pDrvSpec      = pDrvSpec;
    m_pDrvParams    = pDrvParams;
}

/* -------------------------------------------------------------------------- */

UnifiedVideoRenderParams&
UnifiedVideoRenderParams::operator=(VideoRenderParams &From)
{
    color_format    = From.color_format;

    disp.left       = From.disp.left;
    disp.right      = From.disp.right;
    disp.top        = From.disp.top;
    disp.bottom     = From.disp.bottom;

    range.left      = From.range.left;
    range.right     = From.range.right;
    range.top       = From.range.top;
    range.bottom    = From.range.bottom;

    info            = From.info;
    lFlags          = From.lFlags;

    out_data_template = From.out_data_template;

    return *this;
}

/* -------------------------------------------------------------------------- */

UnifiedVideoRender::UnifiedVideoRender(void):
    m_bDefaultRendering(false)
{
#undef  FUNCTION
#define FUNCTION "UnifiedVideoRender"
    DBG_SET ("+");
    ippsZero_8u((Ipp8u*)&m_Driver, sizeof(VideoDriver));
    DBG_SET ("-");
}

UnifiedVideoRender::~UnifiedVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "~UnifiedVideoRender"
    DBG_SET ("+");
    Close();
    DBG_SET ("-");
}

Status UnifiedVideoRender::Init(MediaReceiverParams* pInit)
{
#undef  FUNCTION
#define FUNCTION "Init"
    DBG_SET ("+");
    Status                      umcRes  = UMC_OK;
    UnifiedVideoRenderParams*   pParams = DynamicCast<UnifiedVideoRenderParams>(pInit);

    if ((NULL == pParams) || (NULL == pParams->m_pDrvSpec))
    {
        umcRes = UMC_ERR_NULL_PTR;
    }
    if (UMC_OK == umcRes)
    {
        m_bDefaultRendering = (pParams->lFlags & FLAG_VREN_DEFAULTRECT)? true: false;
    }
    if (UMC_OK == umcRes)
    {
        if (NULL != pParams->m_pDrvSpec->drvInit)
        {
            umcRes = (Status)pParams->m_pDrvSpec->drvInit(&m_Driver,
                                                          pParams->m_pDrvParams);
        }
    }
    if (UMC_OK == umcRes)
    {
        void* buffers[MAX_FRAME_BUFFERS];

        memset(buffers, 0, (MAX_FRAME_BUFFERS)*sizeof(void*));
        /* Allocation of video buffers by a driver. */
        if (NULL != m_Driver.m_DrvSpec.drvCreateBuffers)
        {
            umcRes = (Status)m_Driver.m_DrvSpec.drvCreateBuffers(&m_Driver,
                                                                 MIN_FRAME_BUFFERS,
                                                                 MAX_FRAME_BUFFERS,
                                                                 (void**)buffers,
                                                                 pParams->m_VideoMemType,
                                                                 pParams->m_pVideoMemInfo);
        }
        /* Setting number of allocated buffers. */
        for (m_iBuffersNum = 0; m_iBuffersNum < MAX_FRAME_BUFFERS; ++m_iBuffersNum)
        {
            if (NULL == (m_Buffers[m_iBuffersNum].surface = buffers[m_iBuffersNum]))
                break;
        }
    }
    if (UMC_OK == umcRes)
    {
        umcRes = BaseVideoRender::Init(pInit);
    }
    DBG_SET ("-");
    return umcRes;
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::Close(void)
{
#undef  FUNCTION
#define FUNCTION "Close"
    DBG_SET ("+");
    void*   buffers[MAX_FRAME_BUFFERS] = {NULL};
    Ipp32s  i;

    m_SuncMut.Lock();
    GetDriverBuffers(buffers);
    if (NULL != m_Driver.m_DrvSpec.drvFreeBuffers)
    {
        m_Driver.m_DrvSpec.drvFreeBuffers(&m_Driver, m_iBuffersNum, buffers);
    }
    for (i = 0; i < m_iBuffersNum; i++) m_Buffers[i].surface = NULL;
    if (NULL != m_Driver.m_DrvSpec.drvClose)
    {
        m_Driver.m_DrvSpec.drvClose(&m_Driver);
    }
    m_SuncMut.Unlock();
    DBG_SET ("-");
    return BaseVideoRender::Close();
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::SetFullScreen(ModuleContext& /*rContext*/,
                                         bool bFullScreen)
{
#undef  FUNCTION
#define FUNCTION "SetFullScreen"
    DBG_SET ("+");
    Status              umcRes = UMC_OK;
    VideoDrvVideoMode   mode = (bFullScreen)? VideoDrvFullScreen: VideoDrvOrdinary;

    m_LockBufMut.Lock();
    m_SuncMut.Lock();
    if (UMC_OK == umcRes)
    {
        if (NULL != m_Driver.m_DrvSpec.drvSetVideoMode)
        {
            VideoDrvRect rect;

            rect.x = m_dst_rect.left;
            rect.w = m_dst_rect.right - m_dst_rect.left;
            rect.y = m_dst_rect.top;
            rect.h = m_dst_rect.bottom - m_dst_rect.top;
            umcRes = (Status)m_Driver.m_DrvSpec.drvSetVideoMode(&m_Driver,
                                                                &rect,
                                                                mode);
        }
    }
    if (VideoDrvTrue == m_Driver.m_DrvSpec.drv_rebuf)
    {
        void* buffers[MAX_FRAME_BUFFERS] = {NULL};
        if (UMC_OK == umcRes)
        {
            Ipp32s  i;
            GetDriverBuffers(buffers);
            if (NULL != m_Driver.m_DrvSpec.drvFreeBuffers)
            {
                m_Driver.m_DrvSpec.drvFreeBuffers(&m_Driver, m_iBuffersNum, buffers);
            }
            for (i = 0; i < m_iBuffersNum; i++) m_Buffers[i].surface = NULL;
        }
        if (UMC_OK == umcRes)
        {
            /* Allocation of video buffers by a driver. */
            if (NULL != m_Driver.m_DrvSpec.drvCreateBuffers)
            {
                umcRes = (Status)m_Driver.m_DrvSpec.drvCreateBuffers(&m_Driver,
                                                                     MIN_FRAME_BUFFERS,
                                                                     MAX_FRAME_BUFFERS,
                                                                     buffers,
                                                                     m_Driver.m_VideoMemType,
                                                                     &(m_Driver.m_VideoMemInfo));
            }
            /* Setting number of allocated buffers. */
            for (m_iBuffersNum = 0; m_iBuffersNum < MAX_FRAME_BUFFERS; ++m_iBuffersNum)
            {
                if (NULL == (m_Buffers[m_iBuffersNum].surface = buffers[m_iBuffersNum]))
                    break;
            }
        }
    }
    m_SuncMut.Unlock();
    m_LockBufMut.Unlock();
    DBG_SET ("-");
    return umcRes;
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range)
{
#undef  FUNCTION
#define FUNCTION "ResizeDisplay"
    DBG_SET ("+");
    Status umcRes = BaseVideoRender::ResizeDisplay(disp, range);

    if(UMC_OK == umcRes)
    {
        ModuleContext Context;
        umcRes = SetFullScreen(Context, false);
    }
    DBG_SET ("-");
    return umcRes;
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::GetRenderFrame(Ipp64f *pTime)
{
#undef  FUNCTION
#define FUNCTION "GetRenderFrame"
    DBG_SET ("+");
    DBG_SET ("-");
    return BaseVideoRender::GetRenderFrame(pTime);
}
/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::RenderFrame()
{
#undef  FUNCTION
#define FUNCTION "RenderFrame"
    DBG_SET ("+");
    Status umcRes = UMC_OK;
    VideoDrvRect rect =
    {
        m_dst_rect.left,
        m_dst_rect.top,
        m_dst_rect.right  - m_dst_rect.left,
        m_dst_rect.bottom - m_dst_rect.top
    };

    if(m_iReadIndex < 0)
        umcRes = UMC_ERR_NOT_INITIALIZED;
    if(UMC_OK == umcRes)
    {
        m_SuncMut.Lock();
        Ipp32s  iReadI  = m_iReadIndex; // wee need read index after 'unlock'
        bool    bShow   = m_bShow;
        void*   overlay = m_Buffers[m_iReadIndex].surface;
        m_SuncMut.Unlock();

        if(bShow)
        {
            if (NULL != m_Driver.m_DrvSpec.drvRenderFrame)
            {
                umcRes = (Status)m_Driver.m_DrvSpec.drvRenderFrame(&m_Driver,
                                                                   overlay,
                                                                   ((m_bDefaultRendering)? NULL: &rect));
            }
        }
        if (UMC_OK == umcRes)
        {
            m_iLastFrameIndex = iReadI;
        }
#ifdef VIDEO_DRV_USE_DEBUG
        {
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("%s: %s:"), MODULE, FUNCTION);
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("m_iReadIndex = %d, m_iBuffersNum = %d"),
                            iReadI, m_iBuffersNum);
        }
#endif
        m_hFreeBufSema.Signal();
    }
    DBG_SET ("-");
    return umcRes;
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::ShowLastFrame()
{
#undef  FUNCTION
#define FUNCTION "ShowLastFrame"
    DBG_SET ("+");
    Status umcRes = UMC_OK;
    VideoDrvRect rect =
    {
        m_dst_rect.left,
        m_dst_rect.top,
        m_dst_rect.right  - m_dst_rect.left,
        m_dst_rect.bottom - m_dst_rect.top
    };

    if(m_iLastFrameIndex < 0)
        umcRes = UMC_ERR_NOT_INITIALIZED;
    if(UMC_OK == umcRes)
    {
        m_SuncMut.Lock();
        bool    bShow   = m_bShow;
        void*   overlay = m_Buffers[m_iLastFrameIndex].surface;
        m_SuncMut.Unlock();

        if(bShow)
        {
            if(NULL != m_Driver.m_DrvSpec.drvRenderFrame)
            {
                umcRes = (Status)m_Driver.m_DrvSpec.drvRenderFrame(&m_Driver,
                                                                   overlay,
                                                                   ((m_bDefaultRendering)? NULL: &rect));
            }
        }
#ifdef VIDEO_DRV_USE_DEBUG
        {
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("%s: %s:"), MODULE, FUNCTION);
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("m_iLastFrameIndex = %d, m_iReadIndex = %d"),
                            m_iLastFrameIndex, iReadI);
        }
#endif
    }
    DBG_SET ("-");
    return umcRes;
}

/* -------------------------------------------------------------------------- */

// This function will be changed: LockSurface(VideoData *vd)
Ipp32s UnifiedVideoRender::LockSurface(Ipp8u** vBuf)
{
#undef  FUNCTION
#define FUNCTION "LockSurface"
    DBG_SET ("+");
    Status  umcRes  = UMC_OK;
    Ipp32s  pitch    = 0;

    if (m_iWriteIndex < 0)
        umcRes = UMC_ERR_NOT_INITIALIZED;
    if (UMC_OK == umcRes)
    {
        Ipp8u*            overlay = (Ipp8u*)m_Buffers[m_iWriteIndex].surface;
        VideoDrvPlaneInfo   planes[VIDEO_DRV_MAX_PLANE_NUMBER] =
        {
            {overlay,   0,  m_Driver.m_VideoMemInfo.m_nBufSize},
            {NULL,      0,  0}
        };

        if (NULL != m_Driver.m_DrvSpec.drvLockSurface)
        {
            umcRes = (Status)m_Driver.m_DrvSpec.drvLockSurface(&m_Driver,
                                                               overlay,
                                                               planes);
        }
        if (UMC_OK == umcRes)
        {
            if (VideoDrvVideoMemLibrary == m_Driver.m_VideoMemType)
            {
                pitch = (Ipp32s)planes[0].m_nPitch;
                if (NULL != vBuf)
                    *vBuf = (Ipp8u*)planes[0].m_pPlane;
            }
            else
            {
                /* !!! See function comment. */
                /* All information (pitches, planes) can be calculated. */
                VideoData tmp_data;

                tmp_data.SetAlignment((Ipp32s)m_Driver.m_VideoMemInfo.m_nAlignment);
                tmp_data.Init(m_Driver.img_width, m_Driver.img_height, m_OutDataTemplate.GetColorFormat());
                //tmp_data.SetSurface(overlay, m_Driver.m_VideoMemInfo.m_nBufSize);
                tmp_data.SetBufferPointer((Ipp8u*)planes[0].m_pPlane/*overlay*/, planes[0].m_nMemSize/*m_Driver.m_VideoMemInfo.m_nBufSize*/);
                pitch = (Ipp32s)tmp_data.GetPlanePitch(0);
                if (NULL != vBuf) *vBuf = (Ipp8u*)tmp_data.GetPlanePointer(0);
            }
        }
    }
    if (UMC_OK == umcRes)
    {
#ifdef  VIDEO_DRV_USE_DEBUG
        vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("%s: %s:"), MODULE, FUNCTION);
        vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("m_iWriteIndex = %d, m_iBuffersNum = %d"),
                        m_iWriteIndex, m_iBuffersNum);
#endif
    }
    DBG_SET ("-");
    return pitch;
}

/* -------------------------------------------------------------------------- */

Ipp32s UnifiedVideoRender::UnlockSurface(Ipp8u** /*vBuf*/)
{
#undef  FUNCTION
#define FUNCTION "UnlockSurface"
    DBG_SET ("+");
    Status  umcRes  = UMC_OK;
    Ipp32s  ret_val = 0;

    if (m_iWriteIndex < 0)
    {
        umcRes = UMC_ERR_NOT_INITIALIZED;
    }
    if (UMC_OK == umcRes)
    {
        if (NULL != m_Driver.m_DrvSpec.drvUnlockSurface)
        {
            void *overlay = m_Buffers[m_iWriteIndex].surface;
            umcRes = (Status)m_Driver.m_DrvSpec.drvUnlockSurface(&m_Driver, overlay);
        }
    }
    if (UMC_OK == umcRes)
    {
#ifdef VIDEO_DRV_USE_DEBUG
        {
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("%s: %s:"), MODULE, FUNCTION);
            vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("m_iWriteIndex = %d, m_iBuffersNum = %d\n"),
                            m_iWriteIndex, m_iBuffersNum);
        }
#endif
        ret_val = 1;
    }
    else
        ret_val = 0;
    DBG_SET ("-");
    return ret_val;
}

/* -------------------------------------------------------------------------- */

void UnifiedVideoRender::GetDriverBuffers(void* buffers[MAX_FRAME_BUFFERS])
{
#undef  FUNCTION
#define FUNCTION "GetDriverBuffers"
    DBG_SET ("+");
    Ipp32s  i;

    ippsZero_8u((Ipp8u*)buffers, (MAX_FRAME_BUFFERS)*sizeof(void*));
    for (i = 0; i < m_iBuffersNum; ++i) buffers[i] = m_Buffers[i].surface;
    DBG_SET ("-");
}

/* -------------------------------------------------------------------------- */

void* UnifiedVideoRender::GetWindow(void)
{
#undef  FUNCTION
#define FUNCTION "GetWindow"
    DBG_SET ("+");
    void*   ret_val = NULL;

    if (NULL != m_Driver.m_DrvSpec.drvGetWindow)
    {
        ret_val = m_Driver.m_DrvSpec.drvGetWindow(&m_Driver);
    }
    DBG_SET ("-");
    return ret_val;
}

/* -------------------------------------------------------------------------- */

Status UnifiedVideoRender::RunEventLoop(void)
{
#undef  FUNCTION
#define FUNCTION "RunEventLoop"
    DBG_SET ("+");
    Status umcRes = UMC_OK;

    if (UMC_OK == umcRes)
    {
        if (NULL != m_Driver.m_DrvSpec.drvRunEventLoop)
        {
            umcRes = (Status)m_Driver.m_DrvSpec.drvRunEventLoop(&m_Driver);
        }
    }
    DBG_SET ("-");
    return umcRes;
}

} /* namespace UMC */
