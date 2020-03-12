/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "mfxvideo.h"
#include "vm_strings.h"
#include "mfx_iproxy.h"

typedef void* WindowHandle;

#if ! defined(_WIN32) && ! defined(_WIN64)
  typedef void* RECT;
#endif

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
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed) = 0;
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl) = 0;
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc ) = 0;
    virtual void      Close() = 0;
};

template <>
class InterfaceProxy<IHWDevice> : public InterfaceProxyBase<IHWDevice>
{
    typedef InterfaceProxyBase<IHWDevice> base;
public:
    InterfaceProxy(IHWDevice * target )
        :base(target) {
    }
    virtual mfxStatus Init( mfxU32 nAdapter
        , WindowHandle hDeviceWindow
        , bool bIsWindowed
        , mfxU32 renderTargetFmt
        , int backBufferCount
        , const vm_char *pDvxva2LibName
        , bool isD3D9FeatureLevels = false)  {
            return m_pTarget->Init(nAdapter, hDeviceWindow, bIsWindowed, renderTargetFmt, backBufferCount, pDvxva2LibName, isD3D9FeatureLevels);
    }
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed) {
        return m_pTarget->Reset(hDeviceWindow, drawRect, bWindowed);
    }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl) {
        return m_pTarget->GetHandle(type, pHdl);
    }
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc ) {
        return m_pTarget->RenderFrame(pSrf, Palloc);
    }
    virtual void      Close() {
        m_pTarget->Close();
    }
};