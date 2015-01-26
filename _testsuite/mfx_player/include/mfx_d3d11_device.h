/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#if MFX_D3D11_SUPPORT

#include "mfx_ihw_device.h"
#include <windows.h>
#include <d3d11.h>
#include <atlbase.h>

class MFXD3D11Device : public IHWDevice
{
public:
    MFXD3D11Device();
    ~MFXD3D11Device();

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle hDeviceWindow
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool isD3D9FeatureLevels);
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc);
    virtual void      Close();

protected:
    mfxStatus CreateVideoProcessor(mfxFrameSurface1 * pSrf);

    virtual HRESULT D3D11CreateDeviceWrapper(
        IDXGIAdapter* pAdapter,
        D3D_DRIVER_TYPE DriverType,
        HMODULE Software,
        UINT Flags,
        CONST D3D_FEATURE_LEVEL* pFeatureLevels,
        UINT FeatureLevels,
        UINT SDKVersion,
        ID3D11Device** ppDevice,
        D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext)
    {
        return D3D11CreateDevice(pAdapter,
            DriverType,
            Software,
            Flags,
            pFeatureLevels,
            FeatureLevels,
            SDKVersion,
            ppDevice,
            pFeatureLevel,
            ppImmediateContext);
    }

    DXGI_FORMAT                             m_format;
    CComPtr<ID3D11Device>                   m_pD3D11Device;
    CComPtr<ID3D11DeviceContext>            m_pD3D11Ctx;
    CComQIPtr<ID3D11VideoDevice>            m_pDX11VideoDevice;
    CComQIPtr<ID3D11VideoContext>           m_pVideoContext;
    CComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnum;
    CComPtr<ID3D11VideoProcessor>           m_pVideoProcessor;
    CComQIPtr<IDXGIDevice1>                 m_pDXGIDev;
    CComQIPtr<IDXGIAdapter1>                m_pAdapter;
    CComPtr<IDXGIFactory1>                  m_pDXGIFactory;
    CComPtr<IDXGISwapChain>                 m_pSwapChain;
};

#endif