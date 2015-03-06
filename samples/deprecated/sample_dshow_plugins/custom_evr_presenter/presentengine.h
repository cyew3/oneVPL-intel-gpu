/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except mtIn accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/

//////////////////////////////////////////////////////////////////////////
//
// PresentEngine.h: Defines the D3DPresentEngine object.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//
//////////////////////////////////////////////////////////////////////////

#pragma once

//-----------------------------------------------------------------------------
// D3DPresentEngine class
//
// This class creates the Direct3D device, allocates Direct3D surfaces for
// rendering, and presents the surfaces. This class also owns the Direct3D
// device manager and provides the IDirect3DDeviceManager9 interface via
// GetService.
//
// The goal of this class is to isolate the EVRCustomPresenter class from
// the details of Direct3D as much as possible.
//-----------------------------------------------------------------------------

#include "igfx_s3dcontrol.h"

const DWORD PRESENTER_BUFFER_COUNT = 3;

class D3DPresentEngine : public SchedulerCallback
{
public:

    // State of the Direct3D device.
    enum DeviceState
    {
        DeviceOK,
        DeviceReset,    // The device was reset OR re-created.
        DeviceRemoved,  // The device was removed.
    };

    D3DPresentEngine(HRESULT& hr);
    virtual ~D3DPresentEngine();

    // GetService: Returns the IDirect3DDeviceManager9 interface.
    // (The signature is identical to IMFGetService::GetService but
    // this object does not derive from IUnknown.)
    virtual HRESULT GetService(REFGUID guidService, REFIID riid, void** ppv);
    virtual HRESULT CheckFormat(D3DFORMAT format);

    // Video window / destination rectangle:
    // This object implements a sub-set of the functions defined by the
    // IMFVideoDisplayControl interface. However, some of the method signatures
    // are different. The presenter's implementation of IMFVideoDisplayControl
    // calls these methods.
    HRESULT SetVideoWindow(HWND hwnd);
    HWND    GetVideoWindow() const { return m_hwnd; }
    HRESULT SetDestinationRect(const RECT& rcDest);
    RECT    GetDestinationRect() const { return m_rcDestRect; };

    HRESULT CreateVideoSamples(IMFMediaType *pFormat, VideoSampleList& videoSampleQueue);
    void    ReleaseResources();

    HRESULT CheckDeviceState(DeviceState *pState);
    HRESULT PresentSample(IMFSample* pSample, LONGLONG llTarget);

    UINT    RefreshRate() const { return m_DisplayMode.RefreshRate; }

protected:
    HRESULT InitializeD3D();
    HRESULT CreateD3DDevice();
    HRESULT UpdateDestRect();

    HRESULT GetPreferableS3DMode(IGFX_DISPLAY_MODE *mode);
    static bool Less (const IGFX_DISPLAY_MODE &l, const IGFX_DISPLAY_MODE& r);

    // A derived class can override these handlers to allocate any additional D3D resources.
    virtual HRESULT OnCreateVideoSamples(D3DPRESENT_PARAMETERS& pp) { return S_OK; }
    virtual void    OnReleaseResources() { }

    virtual HRESULT PresentSurface(IDirect3DSurface9* pSurface, LONG nView = 0);
    virtual void    PaintFrameWithGDI();

protected:
    UINT                        m_DeviceResetToken;     // Reset token for the D3D device manager.

    HWND                        m_hwnd;                 // Application-provided destination window.
    RECT                        m_rcDestRect;           // Destination rectangle.
    D3DDISPLAYMODE              m_DisplayMode;          // Adapter's display mode.

    CritSec                     m_ObjectLock;           // Thread lock for the D3D device.

    // COM interfaces
    IDirect3D9Ex                *m_pD3D9;
    IDirect3DDevice9Ex          *m_pDevice;
    IDirect3DDeviceManager9     *m_pDeviceManager;        // Direct3D device manager.
    IDirect3DSurface9           *m_pSurfaceRepaint;       // Surface for repaint requests.

    IGFXS3DControl              *m_pS3DControl;
    IGFX_S3DCAPS                m_Caps;

    // various structures for DXVA2 calls
    DXVA2_VideoDesc                 m_VideoDesc;
    DXVA2_VideoProcessBltParams     m_BltParams;
    DXVA2_VideoSample               m_Sample;

    IDirectXVideoProcessorService   *m_pDXVAVPS;            // Service required to create video processors
    IDirectXVideoProcessor          *m_pDXVAVP_Left;        // Left channel processor
    IDirectXVideoProcessor          *m_pDXVAVP_Right;       // Right channel processor
    IDirect3DSurface9               *m_pRenderSurface;      // The surface which is passed to render
    IDirect3DSurface9               *m_pMixerSurfaces[PRESENTER_BUFFER_COUNT]; // The surfaces, which are used by mixer

private: // disallow copy and assign
    D3DPresentEngine(const D3DPresentEngine&);
    void operator=(const D3DPresentEngine&);
};
