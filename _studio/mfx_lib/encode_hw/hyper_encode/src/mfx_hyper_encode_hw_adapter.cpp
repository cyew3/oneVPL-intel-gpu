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

#include "mfx_hyper_encode_hw_adapter.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include <algorithm>

#include "mfx_dxva2_device.h"
#include "mfx_session.h"
#include "libmfx_core_interface.h"

static inline bool is_iGPU(const mfxAdapterInfo& adapter_info)
{
    return adapter_info.Platform.MediaAdapterType == MFX_MEDIA_INTEGRATED;
}

static inline bool is_dGPU(const mfxAdapterInfo& adapter_info)
{
    return adapter_info.Platform.MediaAdapterType == MFX_MEDIA_DISCRETE;
}

static inline bool iGPU_priority(mfxAdapterInfo& l, mfxAdapterInfo& r)
{
    if (is_iGPU(l) && is_iGPU(r) || is_dGPU(l) && is_dGPU(r))
        return true;

    if (is_iGPU(l) && is_dGPU(r))
        return true;

    // The only combination left is_dGPU(l) && is_iGPU(r))
    return false;
}

static mfxStatus PrepareAdaptersInfo(const mfxComponentInfo* info, std::vector<mfxAdapterInfo>& vec, mfxAdaptersInfo& adapters)
{
    // No suitable adapters on system to handle user's workload
    if (vec.empty()) {
        adapters.NumActual = 0;
        return MFX_ERR_NOT_FOUND;
    }

    if (info)
        // Rearrange in priority order
        std::sort(vec.begin(), vec.end(), iGPU_priority);

    mfxU32 num_to_copy = (std::min)(mfxU32(vec.size()), adapters.NumAlloc);
    for (mfxU32 i = 0; i < num_to_copy; ++i)
        adapters.Adapters[i] = vec[i];
    adapters.NumActual = num_to_copy;

    if (vec.size() > adapters.NumAlloc)
        return MFX_WRN_OUT_OF_RANGE;

    return MFX_ERR_NONE;
}

static inline bool QueryAdapterInfo(mfxU32 adapter_n, mfxU32& VendorID, mfxU32& DeviceID)
{
    MFX::DXVA2Device dxvaDevice;

    if (/*!dxvaDevice.InitD3D9(adapter_n) && */!dxvaDevice.InitDXGI1(adapter_n))
        return false;

    VendorID = dxvaDevice.GetVendorID();
    DeviceID = dxvaDevice.GetDeviceID();

    return true;
}

mfxStatus DummySession::Init(mfxU32 adapter_n, mfxSession* dummy_session)
{
    mfxInitializationParam param;
    param.AccelerationMode = MFX_ACCEL_MODE_VIA_D3D11;
    param.VendorImplID = adapter_n;
    param.NumExtParam = 0;
    param.ExtParam = nullptr;

    return MFXInitialize(param, dummy_session);
}

mfxStatus DummySession::Close(mfxSession dummy_session)
{
    return MFXClose(dummy_session);
}

mfxStatus HyperEncodeImpl::MFXQueryAdapters(mfxComponentInfo* input_info, mfxAdaptersInfo* adapters)
{
    MFX_CHECK_NULL_PTR1(adapters);

    std::vector<mfxAdapterInfo> obtained_info;
    obtained_info.reserve(adapters->NumAlloc);

    mfxU32 adapter_n = 0, VendorID, DeviceID;

    for (;;) {
        if (!QueryAdapterInfo(adapter_n, VendorID, DeviceID))
            break;

        ++adapter_n;

        if (VendorID != INTEL_VENDOR_ID)
            continue;

        // Check if requested capabilities are supported
        mfxSession dummy_session;

        mfxStatus sts = DummySession::Init(adapter_n - 1, &dummy_session);

        if (sts != MFX_ERR_NONE)
            continue;

        mfxAdapterInfo info{};

        mfxU32 version = MFX_VERSION_MAJOR * 1000 + MFX_VERSION_MINOR;
        if (version >= 1019) {
            IVideoCore_API_1_19* pInt = (IVideoCore_API_1_19*)dummy_session->m_pCORE.get()->QueryCoreInterface(MFXICORE_API_1_19_GUID);
            if (pInt == nullptr) {
                DummySession::Close(dummy_session);
                continue;
            }

            sts = pInt->QueryPlatform(&info.Platform);
            if (sts != MFX_ERR_NONE) {
                DummySession::Close(dummy_session);
                continue;
            }
        } else {
            // for API versions greater than 1.19 Device id is set inside QueryPlatform call
            info.Platform.DeviceId = static_cast<mfxU16>(DeviceID);
        }

        info.Number = adapter_n - 1;
        obtained_info.push_back(info);

        DummySession::Close(dummy_session);
    }

    return PrepareAdaptersInfo(input_info, obtained_info, *adapters);
}

mfxStatus HyperEncodeImpl::MFXQueryAdaptersNumber(mfxU32* num_adapters)
{
    MFX_CHECK_NULL_PTR1(num_adapters);

    mfxU32 intel_adapter_count = 0, VendorID, DeviceID;

    for (mfxU32 cur_adapter = 0; ; ++cur_adapter) {
        if (!QueryAdapterInfo(cur_adapter, VendorID, DeviceID))
            break;

        if (VendorID == INTEL_VENDOR_ID)
            ++intel_adapter_count;
    }

    *num_adapters = intel_adapter_count;

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeImpl::MFXQuerySecondAdapter(mfxU32 used_adapter, mfxI32* found_adapter)
{
    MFX_CHECK_NULL_PTR1(found_adapter);

    mfxU32 adapter = 0, VendorID, DeviceID;

    for (mfxU32 cur_adapter = 0; ; ++cur_adapter) {
        if (!QueryAdapterInfo(cur_adapter, VendorID, DeviceID))
            break;

        if (cur_adapter == used_adapter)
            continue;

        if (VendorID == INTEL_VENDOR_ID) {
            adapter = cur_adapter;
            break;
        }
    }

    *found_adapter = adapter;

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
