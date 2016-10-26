//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __BLT_VIDEO_RENDER_H__
#define __BLT_VIDEO_RENDER_H__

#include <umc_defs.h>

#if defined(UMC_ENABLE_DX_VIDEO_RENDER) && defined(UMC_ENABLE_BLT_VIDEO_RENDER)

#include "unified_video_render.h"
#include "dx_video_render.h"
#include "dx_drv.h"

/* Render name: BLT video render.
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

class BLTVideoRender: public UnifiedVideoRender
{
public:
    // Default constructor
    BLTVideoRender();

    // Destuctor
    virtual ~BLTVideoRender();

    // Initialize the render, return false if failed, call Close anyway
    virtual Status Init(MediaReceiverParams* pInit);

    //  Temporary stub, will be removed in future
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(&rInit); }
};

} // namespace UMC

#endif  // defined(UMC_ENABLE_DX_VIDEO_RENDER) && defined(UMC_ENABLE_BLT_VIDEO_RENDER)
#endif  // __BLT_VIDEO_RENDER_H__
