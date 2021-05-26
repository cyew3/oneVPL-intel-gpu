// Copyright (c) 2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_hyper_encode_hw_dev_mngr.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include "mfx_session.h"
#include "libmfx_core_interface.h"
#include "mfx_hyper_encode_hw_adapter.h"

mfxStatus DeviceManagerBase::GetIMPL(mfxMediaAdapterType mediaAdapterType, mfxAccelerationMode* accelMode, mfxU32* adapterNum)
{
    for (auto idx = m_intelAdapters.Adapters; idx != m_intelAdapters.Adapters + m_intelAdapters.NumActual; ++idx)
        if (mediaAdapterType == idx->Platform.MediaAdapterType) {
            *adapterNum = idx->Number;
            *accelMode = MFX_ACCEL_MODE_VIA_D3D11;
            return MFX_ERR_NONE;
        }

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus DeviceManagerBase::PrepareAdapters()
{
    // get adapters number
    mfxU32 numAdaptersAvailable = 0;
    mfxStatus sts = HyperEncodeImpl::MFXQueryAdaptersNumber(&numAdaptersAvailable);
    MFX_CHECK_STS(sts);

    // determine available adapters
    m_intelAdaptersData.resize(numAdaptersAvailable);
    m_intelAdapters = { m_intelAdaptersData.data(), mfxU32(m_intelAdaptersData.size()), 0u };

    sts = HyperEncodeImpl::MFXQueryAdapters(nullptr, &m_intelAdapters);
    MFX_CHECK_STS(sts);

    // get appSessionAdapterType
    mfxAdapterInfo info{};
    mfxStatus mfxRes = HyperEncodeImpl::MFXQueryCorePlatform(m_appSession->m_pCORE.get(), &info);
    MFX_CHECK_STS(mfxRes);

    m_appSessionPlatform = info.Platform;

    return sts;
}

mfxStatus DeviceManagerVideo::GetHandle(mfxMediaAdapterType mediaAdapterType, mfxHDL* hdl, mfxHandleType* hdlType)
{
    if (mediaAdapterType == m_appSessionPlatform.MediaAdapterType) {
        if (m_memType == D3D11_MEMORY) {
            *hdl = (mfxHDL)m_pAppD3D11Device;
            *hdlType = MFX_HANDLE_D3D11_DEVICE;
        } else if (m_memType == D3D9_MEMORY) {
            *hdl = (mfxHDL)m_pAppD3D9DeviceManager;
            *hdlType = MFX_HANDLE_D3D9_DEVICE_MANAGER;
        } else {
            // no SYSTEM_MEMORY support
            return MFX_ERR_UNSUPPORTED;
        }
    } else {
        *hdl = (mfxHDL)m_pD3D11Device;
        *hdlType = MFX_HANDLE_D3D11_DEVICE;
    }

    return MFX_ERR_NONE;
}

mfxStatus DeviceManagerVideo::PrepareDevices()
{
    // create DxgiAdapter
    mfxStatus sts = CreateDxgiAdapter();
    MFX_CHECK_STS(sts);

    // create Dx11 device for 2nd adapter
    sts = CreateDx11Device();
    MFX_CHECK_STS(sts);

    // prepare Dx11 device for 1st adapter
    return PrepareAppDxDevice();
}

mfxStatus DeviceManagerVideo::CreateDxgiAdapter()
{
    CComPtr<IDXGIFactory2> pDXGIFactory;

    HRESULT hres = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&pDXGIFactory));
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    // get adapter number for 2nd device (not primary)
    mfxU32 adapterNum = 0xFFFF;
    for (auto idx = m_intelAdapters.Adapters; idx != m_intelAdapters.Adapters + m_intelAdapters.NumActual; ++idx)
#ifdef SINGLE_GPU_DEBUG
        if (m_appSessionPlatform.MediaAdapterType == idx->Platform.MediaAdapterType)
#else
        if (m_appSessionPlatform.MediaAdapterType != idx->Platform.MediaAdapterType)
#endif
            adapterNum = idx->Number;
    MFX_CHECK(adapterNum != 0xFFFF, MFX_ERR_DEVICE_FAILED);

    hres = pDXGIFactory->EnumAdapters(adapterNum, &m_pDxgiAdapter);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    DXGI_ADAPTER_DESC adapterDesc = {};
    hres = m_pDxgiAdapter->GetDesc(&adapterDesc);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus DeviceManagerVideo::CreateDx11Device()
{
    UINT dxFlags = 0;
    D3D_FEATURE_LEVEL pFeatureLevelsOut;
    HRESULT hres = D3D11CreateDevice(m_pDxgiAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        dxFlags,
        FeatureLevels,
        (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])),
        D3D11_SDK_VERSION,
        &m_pD3D11Device,
        &pFeatureLevelsOut,
        &m_pD3D11Ctx);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    // turn on multithreading for the DX11 context
    ID3D10Multithread* p_mt = nullptr;
    hres = m_pD3D11Ctx->QueryInterface(IID_PPV_ARGS(&p_mt));

    if (SUCCEEDED(hres) && p_mt)
        p_mt->SetMultithreadProtected(TRUE);
    else
        return MFX_ERR_DEVICE_FAILED;

    if (p_mt)
        p_mt->Release();

    return MFX_ERR_NONE;
}

mfxStatus DeviceManagerVideo::PrepareAppDxDevice()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxHDL hdl = nullptr;
    std::vector<mfxHandleType> deviceHdlTypes = { MFX_HANDLE_D3D9_DEVICE_MANAGER, MFX_HANDLE_D3D11_DEVICE };

    for (auto& deviceHdlType : deviceHdlTypes) {
        sts = m_appSession->m_pCORE.get()->GetHandle(deviceHdlType, &hdl);

        if (sts == MFX_ERR_NONE && deviceHdlType == MFX_HANDLE_D3D9_DEVICE_MANAGER) {
            m_memType = D3D9_MEMORY;
            m_pAppD3D9DeviceManager = reinterpret_cast<IDirect3DDeviceManager9*>(hdl);
            break;
        } else if (sts == MFX_ERR_NONE && deviceHdlType == MFX_HANDLE_D3D11_DEVICE) {
            m_memType = D3D11_MEMORY;
            m_pAppD3D11Device = reinterpret_cast<ID3D11Device*>(hdl);
            break;
        }
    }

    return sts;
}

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
