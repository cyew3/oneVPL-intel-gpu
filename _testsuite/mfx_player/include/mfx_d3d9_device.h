/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_D3D_DEVICE_H
#define __MFX_D3D_DEVICE_H

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_ihw_device.h"
#include <windows.h>
#include <d3d9.h>
#include <dxva2api.h>

class MFXD3D9Device : public IHWDevice
{
public:
    MFXD3D9Device();
    ~MFXD3D9Device();

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle hDeviceWindow
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool);
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc);
    virtual void Close() ;
    
private:
    HMODULE                     m_pLibD3D9;
    HMODULE                     m_pLibDXVA2;
    IDirect3D9*                 m_pD3D9;
    IDirect3DDevice9*           m_pD3DD9;
    IDirect3DDeviceManager9*    m_pDeviceManager;
    D3DPRESENT_PARAMETERS       m_D3DPP;

    IDirect3D9* myDirect3DCreate9(UINT SDKVersion);
    HRESULT myDXVA2CreateDirect3DDeviceManager9(UINT* pResetToken,
                                                IDirect3DDeviceManager9** ppDeviceManager,
                                                const vm_char *pDXVA2LIBNAME = NULL);
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif //__MFX_D3D_DEVICE_H
