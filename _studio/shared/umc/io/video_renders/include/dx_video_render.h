// Copyright (c) 2003-2018 Intel Corporation
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
