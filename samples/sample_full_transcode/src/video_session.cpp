/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

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
