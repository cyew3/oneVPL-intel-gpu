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
