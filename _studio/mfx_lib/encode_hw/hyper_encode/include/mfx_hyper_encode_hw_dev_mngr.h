// Copyright (c) 2021 Intel Corporation
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

#ifndef _MFX_HYPER_ENCODE_HW_DEV_MNGR_H_
#define _MFX_HYPER_ENCODE_HW_DEV_MNGR_H_

#include "mfx_common.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include "atlbase.h"
#include "d3d9.h"
#include "d3d11_4.h"
#include "dxva2api.h"
#include "libmfx_allocator.h"

//#define SINGLE_GPU_DEBUG

typedef enum {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY = 0x01,
    D3D11_MEMORY = 0x02,
} mfxMemType;

static D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
};

class DeviceManagerBase
{
public:
    DeviceManagerBase(mfxSession session, mfxStatus* sts)
        :m_appSession(session)
    {
        *sts = PrepareAdapters();
    }
    virtual ~DeviceManagerBase()
    {
        m_intelAdaptersData.clear();
    }

    virtual mfxStatus GetIMPL(mfxU16 mediaAdapterType, mfxAccelerationMode* accelMode, mfxU32* adapterNum);
    virtual mfxStatus GetHandle(mfxU16 mediaAdapterType, mfxHDL* hdl, mfxHandleType* hdlType) = 0;
    virtual mfxFrameAllocator* GetInternalAllocator() = 0;

public:
    mfxPlatform m_appSessionPlatform;

protected:
    mfxStatus PrepareAdapters();

protected:
    mfxMemType m_memType;
    mfxSession m_appSession;
    mfxAdaptersInfo m_intelAdapters;
    std::vector<mfxAdapterInfo> m_intelAdaptersData;
};

class DeviceManagerSys: public DeviceManagerBase
{
public:
    DeviceManagerSys(mfxSession session, mfxStatus* sts)
        :DeviceManagerBase(session, sts)
    {
        m_memType = SYSTEM_MEMORY;
    }
    virtual ~DeviceManagerSys() {}

    mfxStatus GetHandle(mfxU16, mfxHDL*, mfxHandleType*) override
    {
        return MFX_ERR_NONE;
    }
    mfxFrameAllocator* GetInternalAllocator() override
    {
        return nullptr;
    }
};

class DeviceManagerVideo : public DeviceManagerBase
{
public:
    DeviceManagerVideo(mfxSession session, mfxStatus* sts)
        :DeviceManagerBase(session, sts)
    {
        if (*sts == MFX_ERR_NONE)
            *sts = PrepareDevices();
        if (*sts == MFX_ERR_NONE) {
            m_bufferAllocator.bufferAllocator.pthis = &m_bufferAllocator;
            m_pFrameAllocator.reset(new mfxWideSWFrameAllocator(MFX_MEMTYPE_SYSTEM_MEMORY));
            mfxBaseWideFrameAllocator* pAlloc = m_pFrameAllocator.get();
            // set frame allocator
            pAlloc->frameAllocator.pthis = pAlloc;
            // set buffer allocator for current frame single allocator
            pAlloc->wbufferAllocator.bufferAllocator = m_bufferAllocator.bufferAllocator;
        }
    }
    virtual ~DeviceManagerVideo()
    {
        m_pFrameAllocator.reset();
    }

    mfxStatus GetHandle(mfxU16 mediaAdapterType, mfxHDL* hdl, mfxHandleType* hdlType) override;

    mfxFrameAllocator* GetInternalAllocator() override
    {
        return &m_pFrameAllocator.get()->frameAllocator;
    }

protected:
    mfxStatus PrepareDevices();
    mfxStatus CreateDxgiAdapter();
    mfxStatus CreateDx11Device();
    mfxStatus PrepareAppDxDevice();

protected:
    CComPtr<IDXGIAdapter> m_pDxgiAdapter;
    CComPtr<ID3D11Device> m_pD3D11Device;
    CComPtr<ID3D11Device> m_pAppD3D11Device;
    CComPtr<ID3D11DeviceContext> m_pD3D11Ctx;
    CComPtr<IDirect3DDeviceManager9> m_pAppD3D9DeviceManager;

    mfxWideBufferAllocator m_bufferAllocator;
    std::unique_ptr<mfxBaseWideFrameAllocator> m_pFrameAllocator;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_DEV_MNGR_H_
