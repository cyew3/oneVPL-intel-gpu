/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mf_utils.h"
#include "mf_frame_allocator.h"
/*--------------------------------------------------------------------*/
// MFFrameAllocator class

MFFrameAllocator::MFFrameAllocator(IMFAllocatorHelper * pActual)
    : m_nRefCount(0)
    , m_pAllocator(pActual)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
}

MFXFrameAllocator* MFFrameAllocator::GetMFXAllocator()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    CHECK_POINTER(m_pAllocator.get(), NULL);

    return m_pAllocator->GetAllocator();
}
IUnknown * MFFrameAllocator::GetDeviceManager()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    CHECK_POINTER(m_pAllocator.get(), NULL);
    return m_pAllocator->GetDeviceDxGI();
}

/*--------------------------------------------------------------------*/
// IUnknown methods

ULONG MFFrameAllocator::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFFrameAllocator::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFFrameAllocator::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!ppv) return E_POINTER;
    if (iid == IID_IUnknown)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IUnknown");
        *ppv = this;
        
        AddRef();
    }
    else if (iid == IID_IDirect3DDeviceManager9)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IDirect3DDeviceManager9");
        CComQIPtr<IDirect3DDeviceManager9> iDeviceManager (GetDeviceManager());
        CHECK_POINTER(iDeviceManager.p, E_POINTER);
     
        *ppv = iDeviceManager.Detach();
    }
    else if (iid == IID_IDirectXVideoDecoderService)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IDirectXVideoDecoderService");
        CHECK_POINTER(m_pAllocator.get(), E_POINTER);

        CComQIPtr<IDirectXVideoDecoderService> iDecoderService (m_pAllocator->GetProcessorService());
        CHECK_POINTER(iDecoderService.p, E_POINTER);

        *ppv = iDecoderService.Detach();
    }
#if MFX_D3D11_SUPPORT
    else if (iid == IID_ID3D11Device)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "ID3D11Device");
        CComQIPtr<ID3D11Device> iD3D11Device (GetDeviceManager());
        CHECK_POINTER(iD3D11Device.p, E_POINTER);

        *ppv = iD3D11Device.Detach();
    }
#endif
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}
