/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_thread.h"
#include "mfx_ihw_device.h"

class MFXHWDeviceInThread : public InterfaceProxy<IHWDevice>, private mfx_no_copy {
    MFXThread::ThreadPool &mPool;
    typedef InterfaceProxy<IHWDevice> base;
public:
    MFXHWDeviceInThread(MFXThread::ThreadPool &pool, IHWDevice *target ) 
        : base(target)
        , mPool(pool) {
    }
    
    virtual mfxStatus Init( mfxU32 nAdapter
        , WindowHandle hDeviceWindow
        , bool bIsWindowed
        , mfxU32 renderTargetFmt
        , int backBufferCount
        , const vm_char *pDvxva2LibName
        , bool isD3D9FeatureLevels = false)  {
            if (mPool.IsThreadCall()) {
                return base::Init(nAdapter, hDeviceWindow, bIsWindowed, renderTargetFmt, backBufferCount, pDvxva2LibName, isD3D9FeatureLevels);
            }
            mfxStatus sts =  mPool.Queue(
                bind_any( mem_fun_any(&base::Init)
                        , this, nAdapter, hDeviceWindow, bIsWindowed, renderTargetFmt, backBufferCount, pDvxva2LibName, isD3D9FeatureLevels))->Synhronize(MFX_INFINITE);
            return sts;
    }
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed) {
        if (mPool.IsThreadCall()) {
            return base::Reset(hDeviceWindow, drawRect, bWindowed);
        }
        mfxStatus sts =  mPool.Queue(
            bind_any( mem_fun_any(&base::Reset)
                    , this, hDeviceWindow, drawRect, bWindowed))->Synhronize(MFX_INFINITE);
        return sts;
    }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl) {
        if (mPool.IsThreadCall()) {
            return base::GetHandle(type, pHdl);
        }
        mfxStatus sts =  mPool.Queue(
            bind_any( mem_fun_any(&base::GetHandle)
            , this, type, pHdl))->Synhronize(MFX_INFINITE);
        return sts;
    }
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc ) {
        if (mPool.IsThreadCall()) {
            return base::RenderFrame(pSrf, Palloc);
        }
        mfxStatus sts =  mPool.Queue(
            bind_any( mem_fun_any(&base::RenderFrame)
            , this, pSrf, Palloc))->Synhronize(MFX_INFINITE);
        return sts;
    }
    virtual void  Close() {
        if (mPool.IsThreadCall()) {
            base::Close();
            return ;
        }
        mPool.Queue(bind_any( mem_fun_any<mfxStatus, base>(&base::Close), this))->Synhronize(MFX_INFINITE);
    }

};