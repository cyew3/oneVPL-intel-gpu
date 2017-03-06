/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: mf_hw_platform.cpp

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

//#include "mf_guids.h"
#include "mf_hw_platform.h"
//#include "mf_utils.h"

#include "windows.h"
#include "d3d9.h"
#include "dxva2api.h"
#include <initguid.h>
#include <mfxdefs.h>


#pragma warning(disable : 4702)


#if MFX_D3D11_SUPPORT

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

mfxU32 HWPlatform::m_AdapterAny = (mfxU32)ADAPTER_ANY;

HWPlatform::CachedPlatformTypes HWPlatform::m_platformForAdapter;
const PLATFORM_TYPE IGFX_UNKNOWN_PLATFORMTYPE(IGFX_UNKNOWN, IGFX_SKU_UNKNOWN);

CComPtr<ID3D11Device> HWPlatform::CreateD3D11Device(IDXGIAdapter * pAdapter) {
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    static ID3D11Device            *nullDevice = NULL;
    CComQIPtr<ID3D11Device>         pVideoDevice;
    CComPtr<ID3D11Device>           pDevice;
    CComPtr<ID3D11DeviceContext>    pD11Context;
    HRESULT                         hr = S_OK;
    static D3D_FEATURE_LEVEL        featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL               featureLevelsOut;

    D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;


    if (pAdapter) // not default
    {
        type = D3D_DRIVER_TYPE_UNKNOWN; // !!!!!! See MSDN, how to create device using not default device
    }

    for (DWORD dwCount = 0; dwCount < ARRAYSIZE(featureLevels); dwCount++)
    {
        //MFX_LTRACE_I(MF_TL_GENERAL, dwCount);

        {
            //MFX_AUTO_LTRACE(MF_TL_GENERAL, "hr = D3D11CreateDevice()");
            hr = D3D11CreateDevice(pAdapter,
                type,
                NULL,
                0,
                &featureLevels[dwCount],
                1,
                D3D11_SDK_VERSION,
                &pDevice,
                &featureLevelsOut,
                NULL);
        }

        //MFX_LTRACE_D(MF_TL_GENERAL, hr);
        //MFX_LTRACE_P(MF_TL_GENERAL, pDevice);
        //MFX_LTRACE_I(MF_TL_GENERAL, featureLevelsOut);

        if (pDevice && SUCCEEDED(hr))
            break;
    }

    return pDevice;
}


// {49761BEC-4B63-4349-A5FF-87FFDF88466}
DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
    0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

mfxStatus HWPlatform::GetIntelDataPrivateReport(ID3D11VideoDevice * device, const GUID guid, D3D11_VIDEO_DECODER_CONFIG & config)
{
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    if (device == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

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
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() no requested GUID present");
        return MFX_ERR_NOT_FOUND;
    }

    if (!isIntelGuidPresent) {// if no required GUID - no acceleration at all
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() no Intel GUID present - PARTIAL ACCELERATION");
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    mfxU32  count = 0;
    HRESULT hr = device->GetVideoDecoderConfigCount(&video_desc, &count);
    if (FAILED(hr)) {
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] device->GetVideoDecoderConfigCount() failure, hr=", "0x%x", hr);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    for (mfxU32 i = 0; i < count; i++)
    {
        hr = device->GetVideoDecoderConfig(&video_desc, i, &video_config);
        if (FAILED(hr)) {
            //MFX_LTRACE_2(MF_TL_GENERAL, "[HWPlatform] device->GetVideoDecoderConfig(i=,", "%d) failure, hr=0x%x", i, hr);
            return MFX_WRN_PARTIAL_ACCELERATION;
        }

        if (video_config.guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &video_config, sizeof(video_config));
            return MFX_ERR_NONE;
        }
    }

    //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] [GetIntelDataPrivateReport] returns MFX_WRN_PARTIAL_ACCELERATION");
    return MFX_WRN_PARTIAL_ACCELERATION;
}

#endif //d3d11

PLATFORM_TYPE HWPlatform::GetPlatformFromDriverViaD3D11(IUnknown* deviceUnknown) {
    PLATFORM_TYPE result = IGFX_UNKNOWN_PLATFORMTYPE;

#if MFX_D3D11_SUPPORT
    CComQIPtr<ID3D11VideoDevice> device (deviceUnknown);
    if (!device.p) {
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] CComQIPtr<ID3D11VideoDevice>(", "%p) failed", deviceUnknown)
        return result;
    }

    D3D11_VIDEO_DECODER_CONFIG config;
    mfxStatus sts = GetIntelDataPrivateReport(device, DXVA2_ModeH264_VLD_NoFGT, config);
    if (sts == MFX_ERR_NONE) {
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] GetPlatformFromDriverViaD3D11() returns ConfigBitstreamRaw=", "%d", config.ConfigBitstreamRaw);
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] GetPlatformFromDriverViaD3D11() returns ConfigSpatialResid8=", "%d", config.ConfigSpatialResid8);
        result.ProductFamily = static_cast<PRODUCT_FAMILY>(config.ConfigBitstreamRaw);
        result.PlatformSku   = static_cast<PLATFORM_SKU>  (config.ConfigSpatialResid8);
    }
#endif
    return result;
}

PLATFORM_TYPE HWPlatform::GetPlatformFromDriverViaD3D11(mfxU32 nAdapter) {
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
#if MFX_D3D11_SUPPORT
    CComPtr<IDXGIAdapter>  pAdapter;
    CComPtr<IDXGIFactory>  pFactory;

    PLATFORM_TYPE result = IGFX_UNKNOWN_PLATFORMTYPE;

    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) (&pFactory));
    if (FAILED (hr)) {
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] CreateDXGIFactory() failed, hr=", "0x%x", hr);
        return result;
    }
    for (UINT i = (nAdapter == HWPlatform::ADAPTER_ANY) ? 0 : nAdapter; ; i++) {
        hr = pFactory->EnumAdapters(i, &pAdapter.p);
        if (FAILED(hr)) {
            //MFX_LTRACE_2(MF_TL_GENERAL, "[HWPlatform] IDXGIFactory::EnumAdapters(nAdapter=","%d) failed, hr=0x%x", i, hr);
            return result;
        }
        //check that this is Intel Adapter
        CComPtr<ID3D11Device> device ( CreateD3D11Device(pAdapter));
        result = GetPlatformFromDriverViaD3D11(device);
        if (result != IGFX_UNKNOWN_PLATFORMTYPE) {
            return result;
        }
        //single step if adapter specified as particular
        if (nAdapter != HWPlatform::ADAPTER_ANY) {
            break;
        }
    }
    //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetIntelDataPrivateReport() failed");
    return result;
#endif
    //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] GetPlatformFromDriverViaD3D11 - NO D3D11 SUPPORT");
    return result;
}

PLATFORM_TYPE HWPlatform::GetPlatfromFromDriverViaD3D9(IUnknown* device) {
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    device;
    return IGFX_UNKNOWN_PLATFORMTYPE;
}

PLATFORM_TYPE HWPlatform::GetPlatfromFromDriverViaD3D9(mfxU32 nAdapter) {
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    nAdapter;
    return IGFX_UNKNOWN_PLATFORMTYPE;
}

PLATFORM_TYPE HWPlatform::CachePlatformType(const mfxU32 adapterNum, IUnknown* device)
{
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    //MFX_LTRACE_I(MF_TL_GENERAL, adapterNum);
    //MFX_LTRACE_P(MF_TL_GENERAL, device);
    PLATFORM_TYPE result = IGFX_UNKNOWN_PLATFORMTYPE;

    try
    {
        if (device) {
            //MFX_LTRACE_1(MF_TL_GENERAL, "CachePlatformType for provided device ", "%p", device);
            m_platformForAdapter[adapterNum] = GetPlatformFromDriverViaD3D11(device);
            if (m_platformForAdapter[adapterNum] == IGFX_UNKNOWN_PLATFORMTYPE)
            {
                m_platformForAdapter[adapterNum] = GetPlatfromFromDriverViaD3D9(device);
            }
            if ((m_platformForAdapter[adapterNum] != IGFX_UNKNOWN_PLATFORMTYPE) && (m_AdapterAny == ADAPTER_ANY)) {
                m_AdapterAny = adapterNum;
            }
        }
        else {
            if (m_platformForAdapter.find(adapterNum) == m_platformForAdapter.end())
            {
                //MFX_LTRACE_1(MF_TL_GENERAL, "CachePlatformType for", "%d", adapterNum);
                m_platformForAdapter[adapterNum] = GetPlatformFromDriverViaD3D11(adapterNum);
                if (m_platformForAdapter[adapterNum] == IGFX_UNKNOWN_PLATFORMTYPE)
                {
                    m_platformForAdapter[adapterNum] = GetPlatfromFromDriverViaD3D9(adapterNum);
                }
            }
        }
        //MFX_LTRACE_3(MF_TL_GENERAL, "", "result = m_platformForAdapter[%d] = (%d,%d)",
        //    adapterNum, m_platformForAdapter[adapterNum].ProductFamily, m_platformForAdapter[adapterNum].PlatformSku);
        result = m_platformForAdapter[adapterNum];
    }
    catch(...)
    {
        //MFX_LTRACE_S(MF_TL_GENERAL, "Exception when caching platform type in std::map.");
        ATLASSERT(false);
    }
    return result;
}

//eMFXHWType GetHardwareType(const mfxU32 adapterNum, mfxU32 platformFromDriver)
PLATFORM_TYPE HWPlatform::GetPlatformType(const mfxU32 adapterNum)
{
    //MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    //MFX_LTRACE_I(MF_TL_GENERAL, adapterNum);
    PLATFORM_TYPE result = CachePlatformType((adapterNum == ADAPTER_ANY)? m_AdapterAny: adapterNum);

    switch(result.ProductFamily)
    {
    //case IGFX_CANNONLAKE:
    //    MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = CANNONLAKE");
    //    break;
    //case IGFX_APOLLOLAKE:
    //    MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = APOLLOLAKE");
    //    break;
    //case IGFX_WILLOWVIEW:
    //    MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = WILLOWVIEW");
    //    break;
    case IGFX_CHERRYVIEW:
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = CHERRYVIEW");
        break;
    case IGFX_BROADWELL:
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = BROADWELL");
        break;
    case IGFX_VALLEYVIEW:
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = VALLEYVIEW");
        break;
    case IGFX_HASWELL:
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = HASWELL");
        break;
    case IGFX_IVYBRIDGE:
        //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = IVYBRIDGE");
        break;
    default:
        if (result.ProductFamily <= IGFX_GT)
        {
            //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily = GT or older");
        }
        else if (result.ProductFamily > IGFX_CHERRYVIEW)
        {
            //MFX_LTRACE_S(MF_TL_GENERAL, "[HWPlatform] Type().ProductFamily is newer than BDW/CHV");
        }
        //MFX_LTRACE_1(MF_TL_GENERAL, "[HWPlatform] unknown platformFromDriver: ", "%d", result.ProductFamily);
    }
    return result;
}

#endif
