/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

//////////////////////////////////////////////////////////////////////////
//
// PresentEngine.cpp: Defines the D3DPresentEngine object.
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
#include "sample_defs.h"
#include "EVRPresenter.h"


HRESULT FindAdapter(IDirect3D9 *pD3D9, HMONITOR hMonitor, UINT *puAdapterID);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

D3DPresentEngine::D3DPresentEngine(HRESULT& hr) :
    m_hwnd(NULL),
    m_DeviceResetToken(0),
    m_pD3D9(NULL),
    m_pDevice(NULL),
    m_pDeviceManager(NULL),
    m_pSurfaceRepaint(NULL),
    m_pRenderSurface(NULL),
    m_pDXVAVPS(NULL),
    m_pDXVAVP_Left(NULL),
    m_pDXVAVP_Right(NULL),
    m_pS3DControl(NULL)
{
    SetRectEmpty(&m_rcDestRect);

    ZeroMemory(&m_VideoDesc, sizeof(m_VideoDesc));
    ZeroMemory(&m_BltParams, sizeof(m_BltParams));
    ZeroMemory(&m_Sample, sizeof(m_Sample));
    ZeroMemory(&m_Caps, sizeof(m_Caps));

    for (UINT i = 0; i < PRESENTER_BUFFER_COUNT; i++)
    {
        m_pMixerSurfaces[i] = NULL;
    }

    // Initialize DXVA structures
    DXVA2_AYUVSample16 color = {0x8000, 0x8000, 0x1000, 0xffff};

    DXVA2_ExtendedFormat format = { DXVA2_SampleProgressiveFrame,           // SampleFormat
                                    DXVA2_VideoChromaSubsampling_MPEG2,     // VideoChromaSubsampling
                                    DXVA2_NominalRange_Normal,              // NominalRange
                                    DXVA2_VideoTransferMatrix_BT709,        // VideoTransferMatrix
                                    DXVA2_VideoLighting_dim,                // VideoLighting
                                    DXVA2_VideoPrimaries_BT709,             // VideoPrimaries
                                    DXVA2_VideoTransFunc_709                // VideoTransferFunction
                                    };

    // init m_VideoDesc structure
    MSDK_MEMCPY_VAR(m_VideoDesc.SampleFormat, &format, sizeof(DXVA2_ExtendedFormat));
    m_VideoDesc.SampleWidth                  = 256;
    m_VideoDesc.SampleHeight                 = 256;
    m_VideoDesc.InputSampleFreq.Numerator    = 60;
    m_VideoDesc.InputSampleFreq.Denominator  = 1;
    m_VideoDesc.OutputFrameFreq.Numerator    = 60;
    m_VideoDesc.OutputFrameFreq.Denominator  = 1;

    // init m_BltParams structure
    MSDK_MEMCPY_VAR(m_BltParams.DestFormat, &format, sizeof(DXVA2_ExtendedFormat));
    MSDK_MEMCPY_VAR(m_BltParams.BackgroundColor, &color, sizeof(DXVA2_AYUVSample16));

    m_BltParams.BackgroundColor = color;
    m_BltParams.DestFormat      = format;

    // init m_Sample structure
    m_Sample.Start = 0;
    m_Sample.End = 1;
    m_Sample.SampleFormat = format;
    m_Sample.PlanarAlpha.Fraction = 0;
    m_Sample.PlanarAlpha.Value = 1;

    ZeroMemory(&m_DisplayMode, sizeof(m_DisplayMode));

    hr = InitializeD3D();

    if (SUCCEEDED(hr))
    {
       hr = CreateD3DDevice();
    }
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

D3DPresentEngine::~D3DPresentEngine()
{
    // switch back to 2D otherwise next switch to 3D fails,
    // use current display mode
    m_pS3DControl->SwitchTo2D(NULL);

    SAFE_RELEASE(m_pDevice);
    SAFE_RELEASE(m_pSurfaceRepaint);
    SAFE_RELEASE(m_pDeviceManager);
    SAFE_RELEASE(m_pD3D9);

    for (int i = 0; i < PRESENTER_BUFFER_COUNT; i++)
    {
        SAFE_RELEASE(m_pMixerSurfaces[i]);
    }

    SAFE_RELEASE(m_pDXVAVPS);
    SAFE_RELEASE(m_pDXVAVP_Left);
    SAFE_RELEASE(m_pDXVAVP_Right);
    SAFE_DELETE(m_pS3DControl);

    //sleep 4 sec to wait until monitor switches
    Sleep(4*1000);
}

//-----------------------------------------------------------------------------
// GetService
//
// Returns a service interface from the presenter engine.
// The presenter calls this method from inside it's implementation of
// IMFGetService::GetService.
//
// Classes that derive from D3DPresentEngine can override this method to return
// other interfaces. If you override this method, call the base method from the
// derived class.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::GetService(REFGUID guidService, REFIID riid, void** ppv)
{
    assert(ppv != NULL);

    HRESULT hr = S_OK;

    if (riid == __uuidof(IDirect3DDeviceManager9))
    {
        if (m_pDeviceManager == NULL)
        {
            hr = MF_E_UNSUPPORTED_SERVICE;
        }
        else
        {
            *ppv = m_pDeviceManager;
            m_pDeviceManager->AddRef();
        }
    }
    else
    {
        hr = MF_E_UNSUPPORTED_SERVICE;
    }

    return hr;
}


//-----------------------------------------------------------------------------
// CheckFormat
//
// Queries whether the D3DPresentEngine can use a specified Direct3D format.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::CheckFormat(D3DFORMAT format)
{
    HRESULT hr = S_OK;

    UINT uAdapter = D3DADAPTER_DEFAULT;
    D3DDEVTYPE type = D3DDEVTYPE_HAL;

    D3DDISPLAYMODE mode;
    D3DDEVICE_CREATION_PARAMETERS params;

    if (m_pDevice)
    {
        CHECK_HR(hr = m_pDevice->GetCreationParameters(&params));

        uAdapter = params.AdapterOrdinal;
        type = params.DeviceType;

    }

    CHECK_HR(hr = m_pD3D9->GetAdapterDisplayMode(uAdapter, &mode));

    CHECK_HR(hr = m_pD3D9->CheckDeviceType(uAdapter, type, mode.Format, format, TRUE));

done:
    return hr;
}

//-----------------------------------------------------------------------------
// SetVideoWindow
//
// Sets the window where the video is drawn.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::SetVideoWindow(HWND hwnd)
{
    // Assertions: EVRCustomPresenter checks these cases.
    assert(IsWindow(hwnd));
    assert(hwnd != m_hwnd);

    HRESULT hr = S_OK;

    AutoLock lock(m_ObjectLock);

    m_hwnd = hwnd;

    UpdateDestRect();

    // Recreate the device.
    hr = CreateD3DDevice();

    return hr;
}

//-----------------------------------------------------------------------------
// SetDestinationRect
//
// Sets the region within the video window where the video is drawn.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::SetDestinationRect(const RECT& rcDest)
{
    if (EqualRect(&rcDest, &m_rcDestRect))
    {
        return S_OK; // No change.
    }

    HRESULT hr = S_OK;

    AutoLock lock(m_ObjectLock);

    m_rcDestRect = rcDest;

    UpdateDestRect();

    return hr;
}

//-----------------------------------------------------------------------------
// CreateVideoSamples
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::CreateVideoSamples(IMFMediaType *pFormat, VideoSampleList& videoSampleQueue)
{
    if (m_hwnd == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    if (pFormat == NULL)
    {
        return MF_E_UNEXPECTED;
    }

    HRESULT     hr = S_OK;
    D3DCOLOR    clrBlack = D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0x00);
    IMFSample*  pVideoSample = NULL;
    HANDLE      hDevice = 0;
    UINT        nWidth(0), nHeight(0);
    IDirectXVideoProcessorService* pVideoProcessorService = NULL;

    AutoLock lock(m_ObjectLock);

    ReleaseResources();

    UpdateDestRect();

    // Get sizes for allocated surfaces
    CHECK_HR(hr = MFGetAttributeSize(pFormat, MF_MT_FRAME_SIZE, &nWidth, &nHeight));

    // Get device handle
    CHECK_HR(hr = m_pDeviceManager->OpenDeviceHandle(&hDevice));

    // Get IDirectXVideoProcessorService
    CHECK_HR(hr = m_pDeviceManager->GetVideoService(hDevice, IID_IDirectXVideoProcessorService, (void**)&pVideoProcessorService));

    // Create IDirect3DSurface9 surface
    CHECK_HR(hr = pVideoProcessorService->CreateSurface(nWidth, nHeight, PRESENTER_BUFFER_COUNT - 1, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, 0, DXVA2_VideoProcessorRenderTarget, (IDirect3DSurface9 **)&m_pMixerSurfaces, NULL));

    // Create the video samples.
    for (int i = 0; i < PRESENTER_BUFFER_COUNT; i++)
    {
        // Fill it with black.
        CHECK_HR(hr = m_pDevice->ColorFill(m_pMixerSurfaces[i], NULL, clrBlack));

        // Create the sample.
        CHECK_HR(hr = MFCreateVideoSampleFromSurface(m_pMixerSurfaces[i], &pVideoSample));

        pVideoSample->SetUINT32(MFSampleExtension_CleanPoint, 0);

        // Add it to the list.
        hr = videoSampleQueue.InsertBack(pVideoSample);
        SAFE_RELEASE(pVideoSample);
        CHECK_HR(hr);
    }

done:
    SAFE_RELEASE(pVideoProcessorService);
    m_pDeviceManager->CloseDeviceHandle(hDevice);

    if (FAILED(hr))
    {
        ReleaseResources();
    }

    return hr;
}

//-----------------------------------------------------------------------------
// ReleaseResources
//
// Released Direct3D resources used by this object.
//-----------------------------------------------------------------------------

void D3DPresentEngine::ReleaseResources()
{
    // Let the derived class release any resources it created.
    OnReleaseResources();
    SAFE_RELEASE(m_pSurfaceRepaint);

    for (int i = 0; i < PRESENTER_BUFFER_COUNT; i++)
    {
        SAFE_RELEASE(m_pMixerSurfaces[i]);
    }
}

//-----------------------------------------------------------------------------
// CheckDeviceState
//
// Tests the Direct3D device state.
//
// pState: Receives the state of the device (OK, reset, removed)
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::CheckDeviceState(DeviceState *pState)
{
    HRESULT hr = S_OK;

    AutoLock lock(m_ObjectLock);

    // Check the device state. Not every failure code is a critical failure.
    hr = m_pDevice->CheckDeviceState(m_hwnd);

    *pState = DeviceOK;

    switch (hr)
    {
    case S_OK:
    case S_PRESENT_OCCLUDED:
      case S_PRESENT_MODE_CHANGED:
        // state is DeviceOK
        hr = S_OK;
        break;

    case D3DERR_DEVICELOST:
    case D3DERR_DEVICEHUNG:
        // Lost/hung device. Destroy the device and create a new one.
        CHECK_HR(hr = CreateD3DDevice());
        *pState = DeviceReset;
        hr = S_OK;
        break;

    case D3DERR_DEVICEREMOVED:
        // This is a fatal error.
        *pState = DeviceRemoved;
        break;

    case E_INVALIDARG:
        // CheckDeviceState can return E_INVALIDARG if the window is not valid
        // We'll assume that the window was destroyed; we'll recreate the device
        // if the application sets a new window.
        hr = S_OK;
    }

done:
    return hr;
}

//-----------------------------------------------------------------------------
// PresentSample
//
// Presents a video frame.
//
// pSample:  Pointer to the sample that contains the surface to present. If
//           this parameter is NULL, the method paints a black rectangle.
// llTarget: Target presentation time.
//
// This method is called by the scheduler and/or the presenter.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::PresentSample(IMFSample* pSample, LONGLONG llTarget)
{
    HRESULT hr = S_OK;

    IMFMediaBuffer* pBuffer = NULL;
    IDirect3DSurface9* pSurface = NULL;
    LONGLONG sampleDuration=0;

    if (pSample)
    {
        // Get the buffer from the sample.
        CHECK_HR(hr = pSample->GetBufferByIndex(0, &pBuffer));

        // Get the surface from the buffer.
        CHECK_HR(hr = MFGetService(pBuffer, MR_BUFFER_SERVICE, __uuidof(IDirect3DSurface9), (void**)&pSurface));

        CHECK_HR(hr = pSample->GetSampleDuration(&sampleDuration));
    }
    else if (m_pSurfaceRepaint)
    {
        // Redraw from the last surface.
        pSurface = m_pSurfaceRepaint;
        pSurface->AddRef();
    }

    if (pSurface)
    {
        // Present the surface
        CHECK_HR(hr = PresentSurface(pSurface, (0 == sampleDuration)));

        // Store this pointer in case we need to repaint the surface
        CopyComPointer(m_pSurfaceRepaint, pSurface);
    }
    else
    {
        // No surface. All we can do is paint a black rectangle
        PaintFrameWithGDI();
    }

done:
    SAFE_RELEASE(pSurface);
    SAFE_RELEASE(pBuffer);

    if (FAILED(hr))
    {
        if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET || hr == D3DERR_DEVICEHUNG)
        {
            // We failed because the device was lost. Fill the destination rectangle.
            PaintFrameWithGDI();

            // Ignore. We need to reset or re-create the device, but this method
            // is probably being called from the scheduler thread, which is not the
            // same thread that created the device. The Reset(Ex) method must be
            // called from the thread that created the device.

            // The presenter will detect the state when it calls CheckDeviceState()
            // on the next sample.
            hr = S_OK;
        }
    }
    return hr;
}

//-----------------------------------------------------------------------------
// private/protected methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// InitializeD3D
//
// Initializes Direct3D and the Direct3D device manager.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::InitializeD3D()
{
    HRESULT hr = S_OK;
    IGFX_DISPLAY_MODE mode = {0};

    assert(m_pD3D9 == NULL);
    assert(m_pDeviceManager == NULL);

    //// S3D part
    m_pS3DControl = CreateIGFXS3DControl();
    CHECK_HR(hr = (NULL != m_pS3DControl))

    // check if s3d supported and get a list of supported display modes
    CHECK_HR(hr = m_pS3DControl->GetS3DCaps(&m_Caps));

    // choose preferable mode
    CHECK_HR(hr = GetPreferableS3DMode(&mode));

    // switch to 3D mode
    CHECK_HR(hr = m_pS3DControl->SwitchTo3D(&mode));

    // Create Direct3D
    CHECK_HR(hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9));

    // Create the device manager
    CHECK_HR(hr = DXVA2CreateDirect3DDeviceManager9(&m_DeviceResetToken, &m_pDeviceManager));

done:
    return hr;
}

//-----------------------------------------------------------------------------
// CreateD3DDevice
//
// Creates the Direct3D device.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::CreateD3DDevice()
{
    HRESULT     hr = S_OK;
    HWND        hwnd = NULL;
    HMONITOR    hMonitor = NULL;
    UINT        uAdapterID = D3DADAPTER_DEFAULT;
    DWORD       vp = 0;

    D3DCAPS9    ddCaps;
    ZeroMemory(&ddCaps, sizeof(ddCaps));

    IDirect3DDevice9Ex* pDevice = NULL;

    // Hold the lock because we might be discarding an existing device.
    AutoLock lock(m_ObjectLock);

    if (!m_pD3D9 || !m_pDeviceManager)
    {
        return MF_E_NOT_INITIALIZED;
    }

    if (m_hwnd)
    {
        hwnd = m_hwnd;
    }
    else
    {
        hwnd = GetDesktopWindow();
    }

    IGFX_DISPLAY_MODE mode = {0};
    CHECK_HR(hr = GetPreferableS3DMode(&mode));

    // Note: The presenter creates additional swap chains to present the video frames
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = mode.ulResWidth;
    pp.BackBufferHeight = mode.ulResHeight;
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_OVERLAY;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = hwnd;
    pp.Flags = D3DPRESENTFLAG_VIDEO | D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    // Find the monitor for this window.
    if (m_hwnd)
    {
        hMonitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);

        // Find the corresponding adapter.
        CHECK_HR(hr = FindAdapter(m_pD3D9, hMonitor, &uAdapterID));
    }

    // Get the device caps for this adapter.
    CHECK_HR(hr = m_pD3D9->GetDeviceCaps(uAdapterID, D3DDEVTYPE_HAL, &ddCaps));

    if(ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
    {
        vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    }
    else
    {
        vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Create the device.
    CHECK_HR(hr = m_pD3D9->CreateDeviceEx(uAdapterID,
                                          D3DDEVTYPE_HAL,
                                          pp.hDeviceWindow,
                                          vp | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                                          &pp,
                                          NULL,
                                          &pDevice));

    // Get the adapter display mode.
    CHECK_HR(hr = m_pD3D9->GetAdapterDisplayMode(uAdapterID, &m_DisplayMode));

    CHECK_HR(hr = pDevice->ResetEx(&pp, NULL));

    CHECK_HR(hr = pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));

    // Reset the D3DDeviceManager with the new device
    CHECK_HR(hr = m_pDeviceManager->ResetDevice(pDevice, m_DeviceResetToken));

    CHECK_HR(hr = m_pS3DControl->SetDevice(m_pDeviceManager));

    // Create DXVA2 Video Processor Service.
    CHECK_HR(hr = DXVA2CreateVideoService(pDevice, IID_IDirectXVideoProcessorService, (void**)&m_pDXVAVPS));

    // Activate L channel
    CHECK_HR(hr = m_pS3DControl->SelectLeftView());

    // Create VPP device for the L channel
    CHECK_HR(hr = m_pDXVAVPS->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice, &m_VideoDesc, D3DFMT_X8R8G8B8, 1, &m_pDXVAVP_Left));

    // Activate R channel
    CHECK_HR(hr = m_pS3DControl->SelectRightView());

    // Create VPP device for the R channel
    CHECK_HR(hr = m_pDXVAVPS->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice, &m_VideoDesc, D3DFMT_X8R8G8B8, 1, &m_pDXVAVP_Right));

    SAFE_RELEASE(m_pDevice);

    m_pDevice = pDevice;
    m_pDevice->AddRef();

done:
    SAFE_RELEASE(pDevice);
    return hr;
}

//-----------------------------------------------------------------------------
// PresentSurface
//
// Presents a surface that contains a video frame.
//
// pSurface: Pointer to the surface.

HRESULT D3DPresentEngine::PresentSurface( IDirect3DSurface9* pSurface, LONG nView /*= 0*/ )
{
    HRESULT hr = S_OK;
    RECT target;

    if (m_hwnd == NULL)
    {
        return MF_E_INVALIDREQUEST;
    }

    if (NULL == m_pDXVAVP_Left || NULL == m_pDXVAVP_Right)
    {
        return E_FAIL;
    }

    GetClientRect(m_hwnd, &target);

    m_BltParams.TargetRect =
        m_Sample.SrcRect =
            m_Sample.DstRect = target;

    m_Sample.SrcSurface =  pSurface;

    // select processor based on the view id
    IDirectXVideoProcessor* pVideoProcessor = m_pDXVAVP_Left;
    if (nView)
    {
        pVideoProcessor = m_pDXVAVP_Right;
    }

    // a new rendering surface must be retrieved not for every frame,
    // rendering frame is one for both views(L+R)
    if (0 == nView)
    {
          hr = m_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pRenderSurface);
    }

    // process the surface
    hr = pVideoProcessor->VideoProcessBlt(m_pRenderSurface, &m_BltParams, &m_Sample, 1, NULL);

    // release after both views are in it
    if (nView)
    {
        m_pRenderSurface->Release();
    }

    if (SUCCEEDED(hr) && nView)
    {
        hr = m_pDevice->PresentEx(&m_rcDestRect, &m_rcDestRect, m_hwnd, NULL, 0);
    }

    LOG_MSG_IF_FAILED(L"D3DPresentEngine::PresentSurface failed.", hr);

    return hr;
}

//-----------------------------------------------------------------------------
// PaintFrameWithGDI
//
// Fills the destination rectangle with black.
//-----------------------------------------------------------------------------

void D3DPresentEngine::PaintFrameWithGDI()
{
    HDC hdc = GetDC(m_hwnd);

    if (hdc)
    {
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));

        if (hBrush)
        {
            FillRect(hdc, &m_rcDestRect, hBrush);
            DeleteObject(hBrush);
        }

        ReleaseDC(m_hwnd, hdc);
    }
}

//-----------------------------------------------------------------------------
// UpdateDestRect
//
// Updates the target rectangle by clipping it to the video window's client
// area.
//
// Called whenever the application sets the video window or the destination
// rectangle.
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::UpdateDestRect()
{
    if (m_hwnd == NULL)
    {
        return S_FALSE;
    }


    RECT rcView;
    GetClientRect(m_hwnd, &rcView);

    // Clip the destination rectangle to the window's client area.
    if (m_rcDestRect.right > rcView.right)
    {
        m_rcDestRect.right = rcView.right;
    }

    if (m_rcDestRect.bottom > rcView.bottom)
    {
        m_rcDestRect.bottom = rcView.bottom;
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// GetPreferableS3DMode
//
// Returns index of preferable s3d mode
//-----------------------------------------------------------------------------

HRESULT D3DPresentEngine::GetPreferableS3DMode(IGFX_DISPLAY_MODE *mode)
{
    ULONG pref_idx = 0;
    CheckPointer(m_Caps.S3DSupportedModes, E_POINTER);
    for (ULONG i = 0; i<m_Caps.ulNumEntries; i++)
    {
        if (Less(m_Caps.S3DSupportedModes[pref_idx], m_Caps.S3DSupportedModes[i])) pref_idx = i;
    }

    *mode = m_Caps.S3DSupportedModes[pref_idx];
    return S_OK;
}


//-----------------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FindAdapter
//
// Given a handle to a monitor, returns the ordinal number that D3D uses to
// identify the adapter.
//-----------------------------------------------------------------------------

HRESULT FindAdapter(IDirect3D9 *pD3D9, HMONITOR hMonitor, UINT *puAdapterID)
{
    HRESULT hr = E_FAIL;
    UINT cAdapters = 0;
    UINT uAdapterID = (UINT)-1;

    cAdapters = pD3D9->GetAdapterCount();
    for (UINT i = 0; i < cAdapters; i++)
    {
        HMONITOR hMonitorTmp = pD3D9->GetAdapterMonitor(i);

        if (hMonitorTmp == NULL)
        {
            break;
        }
        if (hMonitorTmp == hMonitor)
        {
            uAdapterID = i;
            break;
        }
    }

    if (uAdapterID != (UINT)-1)
    {
        *puAdapterID = uAdapterID;
        hr = S_OK;
    }
    return hr;
}

//-----------------------------------------------------------------------------
// Less
//
// Compares display modes to choose preferable s3d mode
//-----------------------------------------------------------------------------

bool D3DPresentEngine::Less(const IGFX_DISPLAY_MODE &l, const IGFX_DISPLAY_MODE& r)
{
    if (r.ulResWidth >= 0xFFFF || r.ulResHeight >= 0xFFFF || r.ulRefreshRate >= 0xFFFF)
        return false;

         if (l.ulResWidth < r.ulResWidth) return true;
    else if (l.ulResHeight < r.ulResHeight) return true;
    else if (l.ulRefreshRate < r.ulRefreshRate) return true;

    return false;
}
