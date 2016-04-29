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

#include "video_session.h"
#include "pipeline_factory.h"
#include "itransform.h"

MFXVideoSessionExt::MFXVideoSessionExt(PipelineFactory& factory)
        : m_factory(factory) {

}

void MFXVideoSessionExt::SetDeviceAndAllocator() {
    mfxIMPL impl;
    std::auto_ptr<mfxAllocatorParams> alloc_params;
    mfxStatus sts = QueryIMPL(&impl);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoSession::QueryIMPL, sts=") << sts);
        throw DecodeQueryIMPLError();
    }

    //cut implementation basetype
    impl ^= impl & 0xff;
    mfxHDL hdl;
    switch (impl) {
        case MFX_IMPL_VIA_D3D11:
            CreateDevice(ALLOC_IMPL_D3D11_MEMORY);
            m_pDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl);
            sts = SetHandle(MFX_HANDLE_D3D11_DEVICE, hdl);
            m_pAllocator.reset(m_factory.CreateFrameAllocator(ALLOC_IMPL_D3D11_MEMORY));
            alloc_params.reset(m_factory.CreateAllocatorParam(m_pDevice.get(), ALLOC_IMPL_D3D11_MEMORY));
            break;
        case MFX_IMPL_VIA_VAAPI:
        case MFX_IMPL_VIA_D3D9:
            CreateDevice(ALLOC_IMPL_D3D9_MEMORY);
#if defined(_WIN32) || defined(_WIN64)
            m_pDevice->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, &hdl);
            sts = SetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, hdl);
#else
            m_pDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            sts = SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
#endif
            m_pAllocator.reset(m_factory.CreateFrameAllocator(ALLOC_IMPL_D3D9_MEMORY));
            alloc_params.reset(m_factory.CreateAllocatorParam(m_pDevice.get(), ALLOC_IMPL_D3D9_MEMORY));
            break;
        default:
            m_pAllocator.reset(m_factory.CreateFrameAllocator(ALLOC_IMPL_SYSTEM_MEMORY));
            alloc_params.reset(m_factory.CreateAllocatorParam(NULL, ALLOC_IMPL_SYSTEM_MEMORY));
    }
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("SetHanldle failed, sts=") << sts);
        throw VideoSessionExtSetHandleError();
    }

    sts = m_pAllocator->Init(alloc_params.get());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXFrameAllocator::Init failed, sts=") << sts);
        throw VideoSessionExtAllocatorInitError();
    }

    sts = SetFrameAllocator(m_pAllocator.get());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXFrameAllocator::SetFrameAllocator failed, sts=") << sts);
        throw VideoSessionExtSetFrameAllocatorError();
    }
}

void MFXVideoSessionExt::CreateDevice(AllocatorImpl impl) {
    if (impl != ALLOC_IMPL_SYSTEM_MEMORY) {
        m_pDevice.reset(m_factory.CreateHardwareDevice(impl));
        mfxStatus sts = m_pDevice->Init(0, 1, 0);
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("CreateHardwareDevice failed, sts=") << sts);
            throw VideoSessionExtHWDeviceInitError();
        }
    }
}
