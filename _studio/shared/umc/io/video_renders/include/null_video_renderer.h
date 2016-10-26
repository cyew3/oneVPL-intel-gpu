//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __NULL_VIDEO_RENDERER_H__
#define __NULL_VIDEO_RENDERER_H__

#include "unified_video_render.h"
#include "null_drv.h"

namespace UMC
{

class NULLVideoRender: public UnifiedVideoRender
{
public:
    // Default constructor
    NULLVideoRender(void);
    // Destructor
    virtual ~NULLVideoRender(void);

    // Initialize the render, return false if failed, call Close anyway
    virtual Status Init(MediaReceiverParams* pInit);

    //  Temporary stub, will be removed in future
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(&rInit); }
};

} // namespace UMC

#endif // __NULL_VIDEO_RENDERER_H__
