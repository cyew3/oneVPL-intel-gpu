/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mf_dxva_support.h"

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*--------------------------------------------------------------------*/

HRESULT MFCreateMfxVideoSession(IMfxVideoSession** ppSession)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    IMfxVideoSession* pSession = NULL;

    CHECK_POINTER(ppSession, E_POINTER);

    SAFE_NEW(pSession, IMfxVideoSession);
    CHECK_EXPRESSION(pSession, E_OUTOFMEMORY);
    pSession->AddRef();
    *ppSession = pSession;
    return S_OK;
}

/*--------------------------------------------------------------------*/
// IMfxVideoSession class

IMfxVideoSession::IMfxVideoSession(void) :
    m_nRefCount(0)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    DllAddRef();
}

IMfxVideoSession::~IMfxVideoSession(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    Close();
    DllRelease();
}

/*--------------------------------------------------------------------*/
// IUnknown methods


ULONG IMfxVideoSession::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG IMfxVideoSession::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT IMfxVideoSession::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!ppv) return E_POINTER;
    if (iid == IID_IUnknown)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IUnknown");
        *ppv = this;
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    AddRef();
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*--------------------------------------------------------------------*/
// MFDeviceBase class

MFDeviceBase::MFDeviceBase()
    : m_pFrameAllocator(NULL)
    , m_bShareAllocator(false)
{
}

MFFrameAllocator* MFDeviceBase::GetFrameAllocator(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFFrameAllocator* pFrameAllocator = NULL;

    if (m_pFrameAllocator && m_bShareAllocator)
    {
        m_pFrameAllocator->AddRef();
        pFrameAllocator = m_pFrameAllocator;
        DllAddRef();
    }
    MFX_LTRACE_P(MF_TL_GENERAL, pFrameAllocator);
    return pFrameAllocator;
}

/*--------------------------------------------------------------------*/

void MFDeviceBase::ReleaseFrameAllocator(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    DllRelease();
}

/*--------------------------------------------------------------------*/
// MFDeviceDXVA class

MFDeviceDXVA::MFDeviceDXVA(void) :
    m_bInitialized(false),
    m_Mutex(m_mutCreateRes, _T(MF_D3D_MUTEX)),
    m_pD3D(NULL),
    m_pDevice(NULL),
    m_pDeviceManager(NULL),
    m_pDeviceDXVA(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    memset(&m_PresentParams, 0, sizeof(D3DPRESENT_PARAMETERS));
    DllAddRef();
}

/*--------------------------------------------------------------------*/

MFDeviceDXVA::~MFDeviceDXVA(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    DXVASupportClose();
    DXVASupportCloseWrapper();
    DllRelease();
}

/*--------------------------------------------------------------------*/

#if MFX_D3D11_SUPPORT
HRESULT MFDeviceDXVA::DXVASupportInit(void)
{
    UINT nAdapter = D3DADAPTER_DEFAULT;
#else
HRESULT MFDeviceDXVA::DXVASupportInit(UINT nAdapter)
{
#endif
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    UINT reset_token = 0;

    CHECK_POINTER(!m_pDeviceDXVA, E_FAIL);
    CHECK_EXPRESSION(!m_bInitialized, E_FAIL);
    CHECK_RESULT(m_mutCreateRes, S_OK, m_mutCreateRes);
    m_Mutex.Lock();
    if (SUCCEEDED(hr))
    {
        m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
        if (NULL == m_pD3D) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        POINT point = {0, 0};
        HWND hWindow = WindowFromPoint(point);

        // TODO: check this
        m_PresentParams.BackBufferFormat           = D3DFMT_X8R8G8B8;
        m_PresentParams.BackBufferCount            = 1;
        m_PresentParams.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
        m_PresentParams.hDeviceWindow              = hWindow;
        m_PresentParams.Windowed                   = true;
        m_PresentParams.Flags                      = D3DPRESENTFLAG_VIDEO | D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        m_PresentParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        m_PresentParams.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

        hr = m_pD3D->CreateDevice(nAdapter, D3DDEVTYPE_HAL, hWindow,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                  D3DCREATE_FPU_PRESERVE |
                                  D3DCREATE_MULTITHREADED,
                                  &m_PresentParams, &m_pDevice);
    }
    m_Mutex.Unlock();
    if (SUCCEEDED(hr)) hr = DXVA2CreateDirect3DDeviceManager9(&reset_token, &m_pDeviceManager);
    if (SUCCEEDED(hr)) hr = m_pDeviceManager->ResetDevice(m_pDevice, reset_token);
    if (SUCCEEDED(hr)) m_bInitialized = true;
    else DXVASupportClose();

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*--------------------------------------------------------------------*/

void MFDeviceDXVA::DXVASupportClose(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    SAFE_RELEASE(m_pFrameAllocator);
    SAFE_RELEASE(m_pDeviceManager);
    SAFE_RELEASE(m_pDevice);
    m_Mutex.Lock();
    SAFE_RELEASE(m_pD3D);
    m_Mutex.Unlock();

    memset(&m_PresentParams, 0, sizeof(D3DPRESENT_PARAMETERS));

    m_bInitialized = false;
}

/*--------------------------------------------------------------------*/

HRESULT MFDeviceDXVA::DXVASupportInitWrapper(IMFDeviceDXVA* pDeviceDXVA)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_POINTER(pDeviceDXVA, E_POINTER);

    // deinitializing built-in DXVA device
    DXVASupportClose();

    SAFE_RELEASE(m_pDeviceDXVA);
    pDeviceDXVA->AddRef();
    m_pDeviceDXVA = pDeviceDXVA;

    CComQIPtr<IDirect3DDeviceManager9> deviceMgr(m_pDeviceDXVA->GetDeviceManager());
    //ref counter already increased
    m_pDeviceManager  = deviceMgr.p;
    m_pFrameAllocator = m_pDeviceDXVA->GetFrameAllocator();

    CHECK_POINTER(m_pDeviceManager, E_POINTER);
    CHECK_POINTER(m_pFrameAllocator, E_POINTER);

    return S_OK;
}

/*--------------------------------------------------------------------*/

void MFDeviceDXVA::DXVASupportCloseWrapper(void)
{
    SAFE_RELEASE(m_pDeviceManager);
    SAFE_RELEASE(m_pFrameAllocator);
    if (m_pDeviceDXVA) m_pDeviceDXVA->ReleaseFrameAllocator();
    SAFE_RELEASE(m_pDeviceDXVA);
}

/*--------------------------------------------------------------------*/

IUnknown* MFDeviceDXVA::GetDeviceManager(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    IDirect3DDeviceManager9* pDeviceManager = NULL;

    if (m_pDeviceManager)
    {
        m_pDeviceManager->AddRef();
        pDeviceManager = m_pDeviceManager;
    }
    MFX_LTRACE_P(MF_TL_GENERAL, pDeviceManager);
    return pDeviceManager;
}
