//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2011 Intel Corporation. All Rights Reserved.
//

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
