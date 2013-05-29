/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "mfxvideo.h"
#include "vm_strings.h"

typedef void* WindowHandle;

class IHWDevice
{
public:
    virtual ~IHWDevice(){}
    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle hDeviceWindow
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool isD3D9FeatureLevels = false) = 0;
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, bool bWindowed) = 0;
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl) = 0;
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc ) = 0;
    virtual void      Close() = 0;
};