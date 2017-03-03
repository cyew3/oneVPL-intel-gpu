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

#include "mf_guids.h"
#include "mf_hw_platform.h"

#include "windows.h"
#include "d3d9.h"
#include "dxva2api.h"
#include <initguid.h>
#include <mfxdefs.h>


#pragma warning(disable : 4702)


#if MFX_D3D11_SUPPORT

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);


CComPtr<ID3D11Device> HWPlatform::CreateD3D11Device(IDXGIAdapter * pAdapter) {
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    static ID3D11Device            *nullDevice = NULL;
    CComQIPtr<ID3D11Device>         pVideoDevice;
    CComPtr<ID3D11Device>           pDevice;
    CComPtr<ID3D11DeviceContext>    pD11Context;
    HRESULT                         hr = S_OK;
    static D3D_FEATURE_LEVEL        FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL               pFeatureLevelsOut;

    D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;


    if (pAdapter) // not default
    {
        type = D3D_DRIVER_TYPE_UNKNOWN; // !!!!!! See MSDN, how to create device using not default device
    }

    hr =  D3D11CreateDevice(pAdapter,    // provide real adapter
                            type,
                            NULL,
                            0,
                            FeatureLevels,
                            sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
                            D3D11_SDK_VERSION,
                            &pDevice,
                            &pFeatureLevelsOut,
                            &pD11Context);

    if (FAILED(hr))
    {
        MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] D3D11CreateDevice() failed, NO D3D11 features level hr=", "0x%x", hr);
        // lets try to create dx11 device with d9 feature level
        static D3D_FEATURE_LEVEL FeatureLevels9[] = {
            D3D_FEATURE_LEVEL_9_1,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_3 };

        hr =  D3D11CreateDevice(pAdapter,    // provide real adapter
                                D3D_DRIVER_TYPE_HARDWARE,
                                NULL,
                                0,
                                FeatureLevels9,
                                sizeof(FeatureLevels9)/sizeof(D3D_FEATURE_LEVEL),
                                D3D11_SDK_VERSION,
                                &pDevice,
                                &pFeatureLevelsOut,
                                &pD11Context);

        if (FAILED(hr)) {
            MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] D3D11CreateDevice() failed, hr=", "0x%x", hr);
            return nullDevice;
        }
    }

    return pDevice;
}

mfxStatus HWPlatform::GetIntelDataPrivateReport(ID3D11VideoDevice * device, const GUID guid, D3D11_VIDEO_DECODER_CONFIG & config)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    video_desc.Guid = DXVADDI_Intel_Decode_PrivateData_Report;
    video_desc.SampleWidth = 640;
    video_desc.SampleHeight = 480;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    mfxU32 cDecoderProfiles = device->GetVideoDecoderProfileCount();
    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr = device->GetVideoDecoderProfile(i, &decoderGuid);
        if (FAILED(hr))
        {
            continue;
        }

        if (guid == decoderGuid)
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == decoderGuid)
            isIntelGuidPresent = true;
    }

    if (!isRequestedGuidPresent) {
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() no requested GUID present");
        return MFX_ERR_NOT_FOUND;
    }

    if (!isIntelGuidPresent) {// if no required GUID - no acceleration at all
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() no Intel GUID present - PARTIAL ACCELERATION");
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    mfxU32  count = 0;
    HRESULT hr = device->GetVideoDecoderConfigCount(&video_desc, &count);
    if (FAILED(hr)) {
        MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] device->GetVideoDecoderConfigCount() failure, hr=", "0x%x", hr);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    for (mfxU32 i = 0; i < count; i++)
    {
        hr = device->GetVideoDecoderConfig(&video_desc, i, &video_config);
        if (FAILED(hr)) {
            MFX_LTRACE_2(MF_TL_GENERAL, "[HWPlatform] device->GetVideoDecoderConfig(i=,", "%d) failure, hr=0x%x", i, hr);
            return MFX_WRN_PARTIAL_ACCELERATION;
        }

        if (video_config.guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &video_config, sizeof(video_config));
            return MFX_ERR_NONE;
        }
    }

    MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] [GetIntelDataPrivateReport] returns MFX_WRN_PARTIAL_ACCELERATION");
    return MFX_WRN_PARTIAL_ACCELERATION;
}

#endif //d3d11

HWPlatform::PRODUCT_FAMILY HWPlatform::GetPlatformFromDriverViaD3D11(mfxU32 nAdapter) {
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
#if MFX_D3D11_SUPPORT
    CComPtr<IDXGIAdapter>  pAdapter;
    CComPtr<IDXGIFactory>  pFactory;

    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) (&pFactory));
    if (FAILED (hr)) {
        MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] CreateDXGIFactory() failed, hr=", "0x%x", hr);
        return IGFX_UNKNOWN;
    }
    mfxStatus sts = MFX_ERR_NONE;
        for (UINT i = nAdapter == HWPlatform::ADAPTER_ANY ? 0 : nAdapter; ; i++) {
            hr = pFactory->EnumAdapters(i, &pAdapter);
            if (FAILED(hr)) {
                MFX_LTRACE_2(MF_TL_GENERAL, "[HWPlatform] IDXGIFactory::EnumAdapters(nAdapter=","%d) failed, hr=0x%x", i, hr);
                return IGFX_UNKNOWN;
            }
            //check that this is Intel Adapter
            CComQIPtr<ID3D11VideoDevice> device ( CreateD3D11Device(pAdapter));
        if (!device.p) {
            MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] CreateD3D11Device(nAdapter=","%d) failed", i)
            return IGFX_UNKNOWN;
        }

        D3D11_VIDEO_DECODER_CONFIG config;
        sts = GetIntelDataPrivateReport(device, DXVA2_ModeH264_VLD_NoFGT, config);
        if (sts == MFX_ERR_NONE) {
            MFX_LTRACE_2(MF_TL_GENERAL, "[HWPlatform] GetPlatformFromDriverViaD3D11(nAdapter = ", "%d) returns %d", i, config.ConfigBitstreamRaw);
            return static_cast<PRODUCT_FAMILY>(config.ConfigBitstreamRaw);
        }
        //single step if adapter specified as particular
        if (nAdapter == HWPlatform::ADAPTER_ANY) {
            break;
        }
    }
    MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() failed, sts=", "%d", sts);
    return IGFX_UNKNOWN;
#endif
    MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetPlatformFromDriverViaD3D11 - NO D3D11 SUPPORT");
    return IGFX_UNKNOWN;
}

HWPlatform::PRODUCT_FAMILY HWPlatform::GetPlatfromFromDriverViaD3D9(mfxU32 nAdapter) {
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    nAdapter;
    return IGFX_UNKNOWN;
}

std::map<mfxU32, mfxU32> HWPlatform::m_platformForAdapter;

//eMFXHWType GetHardwareType(const mfxU32 adapterNum, mfxU32 platformFromDriver)
HWPlatform::HWType HWPlatform::Type(const mfxU32 adapterNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (m_platformForAdapter.find(adapterNum) == m_platformForAdapter.end()) {
        MFX_LTRACE_1(MF_TL_GENERAL, "CachePlatformType for", "%d", adapterNum);
        m_platformForAdapter[adapterNum] = GetPlatformFromDriverViaD3D11(adapterNum);
        if (m_platformForAdapter[adapterNum] == IGFX_UNKNOWN) {
            m_platformForAdapter[adapterNum] = GetPlatfromFromDriverViaD3D9(adapterNum);
        }
    }

    switch(m_platformForAdapter[adapterNum])
    {
    case IGFX_HASWELL:
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type() = HASWELL");
        return HSW_ULT;
    case IGFX_IVYBRIDGE:
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type() = IVYBRIDGE");
        return IVB;
    case IGFX_EAGLELAKE_G:
    case IGFX_IRONLAKE_G:
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type() = LAKE");
        return LAKE;
    case IGFX_VALLEYVIEW:
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type() = VALLEYVIEW");
        return VLV;
    case IGFX_SKYLAKE:
        MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type() = SKYLAKE");
        return SCL;
    case IGFX_BROADWELL:
        return BDW;
    }
    MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] unknown platformFromDriver: ", "%d", m_platformForAdapter[adapterNum]);
    return UNKNOWN;
}
