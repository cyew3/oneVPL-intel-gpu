/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

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
    virtual ~MFXD3D9Device();

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle hDeviceWindow
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool);
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc);
    virtual void Close() ;
protected:
    virtual void AdjustD3DPP(D3DPRESENT_PARAMETERS *) {};
    virtual bool IsOverlay() { return false; };
    virtual bool IsFullscreen() { return m_bModeFullscreen; };
private:
    HMODULE                     m_pLibD3D9;
    HMODULE                     m_pLibDXVA2;
    IDirect3D9Ex*               m_pD3D9Ex;
    IDirect3DDevice9Ex*         m_pD3DD9Ex;
    IDirect3DDeviceManager9*    m_pDeviceManager;
    D3DPRESENT_PARAMETERS       m_D3DPP;
    bool                        m_bModeFullscreen;
    RECT                        m_drawRect;

    HRESULT myDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex**d);
    HRESULT myDXVA2CreateDirect3DDeviceManager9(UINT* pResetToken,
                                                IDirect3DDeviceManager9** ppDeviceManager,
                                                const vm_char *pDXVA2LIBNAME = NULL);
};


#define ASSIGN_IF_NOT_ZERO(a, b) {if((b)) (a)=(b);}

class MFXD3D9DeviceEx : public MFXD3D9Device
{
public:
    MFXD3D9DeviceEx(const D3DPRESENT_PARAMETERS &D3DPP_over) :
        MFXD3D9Device()
        ,m_D3DPP_over(D3DPP_over)
    { };
protected:
        virtual void AdjustD3DPP(D3DPRESENT_PARAMETERS *pD3DPP) 
        {
            ASSIGN_IF_NOT_ZERO(pD3DPP->BackBufferWidth,            m_D3DPP_over.BackBufferWidth);
            ASSIGN_IF_NOT_ZERO(pD3DPP->BackBufferHeight,           m_D3DPP_over.BackBufferHeight);
            ASSIGN_IF_NOT_ZERO(pD3DPP->BackBufferFormat,           m_D3DPP_over.BackBufferFormat);
            ASSIGN_IF_NOT_ZERO(pD3DPP->BackBufferCount,            m_D3DPP_over.BackBufferCount);
            
            ASSIGN_IF_NOT_ZERO(pD3DPP->MultiSampleType,            m_D3DPP_over.MultiSampleType);
            ASSIGN_IF_NOT_ZERO(pD3DPP->MultiSampleQuality,         m_D3DPP_over.MultiSampleQuality);
            
            ASSIGN_IF_NOT_ZERO(pD3DPP->SwapEffect,                 m_D3DPP_over.SwapEffect);
            ASSIGN_IF_NOT_ZERO(pD3DPP->hDeviceWindow,              m_D3DPP_over.hDeviceWindow);
            ASSIGN_IF_NOT_ZERO(pD3DPP->Windowed,                   m_D3DPP_over.Windowed);
            ASSIGN_IF_NOT_ZERO(pD3DPP->EnableAutoDepthStencil,     m_D3DPP_over.EnableAutoDepthStencil);
            ASSIGN_IF_NOT_ZERO(pD3DPP->AutoDepthStencilFormat,     m_D3DPP_over.AutoDepthStencilFormat);
            ASSIGN_IF_NOT_ZERO(pD3DPP->Flags,                      m_D3DPP_over.Flags);
            
            ASSIGN_IF_NOT_ZERO(pD3DPP->FullScreen_RefreshRateInHz, m_D3DPP_over.FullScreen_RefreshRateInHz);
            ASSIGN_IF_NOT_ZERO(pD3DPP->PresentationInterval,       m_D3DPP_over.PresentationInterval);
        };

        bool IsOverlay()
        {
            return (D3DSWAPEFFECT_OVERLAY == m_D3DPP_over.SwapEffect ? true : false );
        }
private:
    D3DPRESENT_PARAMETERS m_D3DPP_over;
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif //__MFX_D3D_DEVICE_H
