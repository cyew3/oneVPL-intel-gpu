/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: hw_utils.cpp

\* ****************************************************************************** */

#include "hw_utils.h"
#include "mfx_utils.h"
#include "mfx_check_hardware_support.h"
#if defined(_WIN32) || defined(_WIN64)
#include <initguid.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report, 
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);
DEFINE_GUID(sDXVA2_ModeH264_VLD_NoFGT,
0x1b81be68,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

mfxStatus InitializeServiceD3D9(bool isTemporalCall, mfxHDL* mfxDeviceHdl, IDirectXVideoDecoderService** pDirectXVideoService)
{
    HRESULT hr = S_OK;

    IDirect3DDeviceManager9* pDirect3DDeviceManager = (IDirect3DDeviceManager9*) *mfxDeviceHdl;
    HANDLE hDirectXHandle;

    hr = pDirect3DDeviceManager->OpenDeviceHandle(&hDirectXHandle);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    hr = pDirect3DDeviceManager->GetVideoService(hDirectXHandle,
                                                   __uuidof(IDirectXVideoDecoderService),
                                                   (void**)pDirectXVideoService);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus GetIntelDataPrivateReportD3D9(const GUID guid, DXVA2_ConfigPictureDecode & config, mfxHDL* mfxDeviceHdl)
{
    IDirectXVideoDecoderService *pDirectXVideoService = 0;
    if (!pDirectXVideoService)
    {
        mfxStatus sts = InitializeServiceD3D9(true, mfxDeviceHdl, &pDirectXVideoService);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    mfxU32 cDecoderGuids = 0;
    GUID   *pDecoderGuids = 0;
    HRESULT hr = pDirectXVideoService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);
    if (FAILED(hr))
        return MFX_WRN_PARTIAL_ACCELERATION;

    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderGuids; i++)
    {
        if (pDecoderGuids && (guid == pDecoderGuids[i]))
            isRequestedGuidPresent = true;

        if (pDecoderGuids && (DXVADDI_Intel_Decode_PrivateData_Report == pDecoderGuids[i]))
            isIntelGuidPresent = true;
    }

    if (pDecoderGuids)
    {
        CoTaskMemFree(pDecoderGuids);
    }

    if (!isRequestedGuidPresent)
        return MFX_ERR_NOT_FOUND;

    if (!isIntelGuidPresent) // if no required GUID - no acceleration at all
        return MFX_WRN_PARTIAL_ACCELERATION;

    DXVA2_ConfigPictureDecode *pConfig = 0;
    UINT cConfigurations = 0;
    DXVA2_VideoDesc VideoDesc = {0};

    hr = pDirectXVideoService->GetDecoderConfigurations(DXVADDI_Intel_Decode_PrivateData_Report, 
                                            &VideoDesc, 
                                            NULL, 
                                            &cConfigurations, 
                                            &pConfig);
    if (FAILED(hr) || pConfig == 0)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    for (mfxU32 k = 0; k < cConfigurations; k++)
    {
        if (pConfig[k].guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &pConfig[k], sizeof(DXVA2_ConfigPictureDecode));
            if (pConfig)
            {
                CoTaskMemFree(pConfig);
            }
            return MFX_ERR_NONE;
        }
    }
    
    if (pConfig)
    {
        CoTaskMemFree(pConfig);
    }

    return MFX_WRN_PARTIAL_ACCELERATION;
}

eMFXHWType GetHWTypeD3D9(const mfxU32 adapterNum, mfxHDL* mfxDeviceHdl)
{
    mfxU32 platformFromDriver = 0;

    DXVA2_ConfigPictureDecode config;
    mfxStatus sts = GetIntelDataPrivateReportD3D9(sDXVA2_ModeH264_VLD_NoFGT, config, mfxDeviceHdl);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    return MFX::GetHardwareType(adapterNum, platformFromDriver);
}

//mfxStatus GetHWTypeD3D11(const mfxU32 adapterNum, mfxHDL* mfxDeviceHdl)
//{
//    mfxU32 platformFromDriver = 0;
//
//    D3D11_VIDEO_DECODER_CONFIG config;
//    mfxStatus sts = GetIntelDataPrivateReport(sDXVA2_ModeH264_VLD_NoFGT, 0, config);
//    if (sts == MFX_ERR_NONE)
//        platformFromDriver = config.ConfigBitstreamRaw;
//
//    return MFX_ERR_NONE;
//}
#endif

eMFXHWType GetHWType(const mfxU32 adapterNum, mfxIMPL impl, mfxHDL* mfxDeviceHdl)
{
#if defined(_WIN32) || defined(_WIN64)
    if(impl & MFX_IMPL_VIA_D3D9)
        return GetHWTypeD3D9(adapterNum, mfxDeviceHdl);
    else if(impl & MFX_IMPL_VIA_D3D11)
        //return GetHWTypeD3D11(adapterNum, mfxDeviceHdl);
        ;
    else
#endif
        return MFX_HW_UNKNOWN;

    return MFX_HW_UNKNOWN;
}