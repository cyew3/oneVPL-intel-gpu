//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __DX_VIDEO_RENDER_H__
#define __DX_VIDEO_RENDER_H__

#include <umc_defs.h>

#ifdef UMC_ENABLE_DX_VIDEO_RENDER

#include "unified_video_render.h"
#include "dx_drv.h"

/* Render name: DX video render.
 *
 * OS dependencies:
 *  - Windows.
 *
 * Libraries dependencies:
 *  - DirectDraw.
 *
 * For more information see DX manual (www.msdn.com).
 */

namespace UMC
{

//typedef DXVideoRenderParams WindowsVideoRenderParams

class DXVideoRenderParams: public VideoRenderParams
{
    DYNAMIC_CAST_DECL(DXVideoRenderParams, VideoRenderParams)

public:
    // Default constructor
    DXVideoRenderParams();

    HWND m_hWnd;
    COLORREF m_ColorKey;
};

class DXVideoRender: public UnifiedVideoRender
{
public:
    // Default constructor
    DXVideoRender(void);
    // Destructor
    virtual ~DXVideoRender(void);

    // Initialize the render, return false if failed, call Close anyway
    virtual Status Init(MediaReceiverParams* pInit);

    //  Temporary stub, will be removed in future
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(&rInit); }
};

} // namespace UMC

#endif // defined(UMC_ENABLE_DX_VIDEO_RENDER)

#endif // __DX_VIDEO_RENDER_H__
