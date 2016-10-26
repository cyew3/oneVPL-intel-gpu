//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

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

    pDirectXVideoService->Release();
    pDirectXVideoService = 0;

    return MFX_WRN_PARTIAL_ACCELERATION;
}

mfxStatus GetIntelDataPrivateReportD3D11(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config, mfxHDL* mfxDeviceHdl)
{
    if(!mfxDeviceHdl || !*mfxDeviceHdl)
        return MFX_WRN_PARTIAL_ACCELERATION;
    //HRESULT hRes = S_OK;

    //static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    //D3D_FEATURE_LEVEL pFeatureLevelsOut;

    //hRes = D3D11CreateDevice(NULL,
    //    D3D_DRIVER_TYPE_HARDWARE,
    //    NULL,
    //    0,
    //    FeatureLevels,
    //    sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
    //    D3D11_SDK_VERSION,
    //    pD3D11Device,
    //    &pFeatureLevelsOut,
    //    pD3D11DeviceContext);

    //if (FAILED(hRes))
    //{
    //    return MFX_ERR_DEVICE_FAILED;
    //}

    ID3D11Device* pD11Device = (ID3D11Device*) *mfxDeviceHdl;
    //CComPtr<ID3D11Device> pD11Device = (ID3D11Device*) mfxDeviceHdl;
    CComQIPtr<ID3D11VideoDevice> pD11VideoDevice = pD11Device;
    //ID3D11VideoDevice* pD11VideoDevice = (ID3D11VideoDevice*) *mfxDeviceHdl;

    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    video_desc.Guid = DXVADDI_Intel_Decode_PrivateData_Report;
    video_desc.SampleWidth = par ? par->mfx.FrameInfo.Width : 640;
    video_desc.SampleHeight = par ? par->mfx.FrameInfo.Height : 480;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    if (!video_desc.SampleWidth)
    {
        video_desc.SampleWidth = 640;
    }

    if (!video_desc.SampleHeight)
    {
        video_desc.SampleHeight = 480;
    }
    if(!pD11VideoDevice)
        return MFX_WRN_PARTIAL_ACCELERATION;
    mfxU32 cDecoderProfiles = pD11VideoDevice->GetVideoDecoderProfileCount();
    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr = pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
        if (FAILED(hr))
        {
            continue;
        }

        if (guid == decoderGuid)
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == decoderGuid)
            isIntelGuidPresent = true;
    }

    if (!isRequestedGuidPresent)
        return MFX_ERR_NOT_FOUND;

    if (!isIntelGuidPresent) // if no required GUID - no acceleration at all
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxU32  count = 0;
    HRESULT hr = pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
    if (FAILED(hr))
        return MFX_WRN_PARTIAL_ACCELERATION;

    for (mfxU32 i = 0; i < count; i++)
    {
        hr = pD11VideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
        if (FAILED(hr))
            return MFX_WRN_PARTIAL_ACCELERATION;

        if (video_config.guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &video_config, sizeof(D3D11_VIDEO_DECODER_CONFIG));
            return MFX_ERR_NONE;
        }
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

eMFXHWType GetHWTypeD3D11(const mfxU32 adapterNum, mfxHDL* mfxDeviceHdl)
{
    mfxU32 platformFromDriver = 0;

    D3D11_VIDEO_DECODER_CONFIG config;
    mfxStatus sts = GetIntelDataPrivateReportD3D11(sDXVA2_ModeH264_VLD_NoFGT, 0, config, mfxDeviceHdl);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    return MFX::GetHardwareType(adapterNum, platformFromDriver);
}
#endif

#if (defined(LINUX32) || defined(LINUX64))
typedef struct drm_i915_getparam {
    int param;
    int *value;
} drm_i915_getparam_t;

typedef struct {
    int device_id;
    eMFXHWType platform;
} mfx_device_item;

/* list of legal dev ID for Intel's graphics
 * Copied form i915_drv.c from linux kernel 3.9-rc7
 * */
const mfx_device_item listLegalDevIDs[] = {
    /*IVB*/
    { 0x0156, MFX_HW_IVB },   /* GT1 mobile */
    { 0x0166, MFX_HW_IVB },   /* GT2 mobile */
    { 0x0152, MFX_HW_IVB },   /* GT1 desktop */
    { 0x0162, MFX_HW_IVB },   /* GT2 desktop */
    { 0x015a, MFX_HW_IVB },   /* GT1 server */
    { 0x016a, MFX_HW_IVB },   /* GT2 server */
    /*HSW*/
    { 0x0402, MFX_HW_HSW },   /* GT1 desktop */
    { 0x0412, MFX_HW_HSW },   /* GT2 desktop */
    { 0x0422, MFX_HW_HSW },   /* GT2 desktop */
    { 0x041e, MFX_HW_HSW },   /* Core i3-4130 */
    { 0x040a, MFX_HW_HSW },   /* GT1 server */
    { 0x041a, MFX_HW_HSW },   /* GT2 server */
    { 0x042a, MFX_HW_HSW },   /* GT2 server */
    { 0x0406, MFX_HW_HSW },   /* GT1 mobile */
    { 0x0416, MFX_HW_HSW },   /* GT2 mobile */
    { 0x0426, MFX_HW_HSW },   /* GT2 mobile */
    { 0x0C02, MFX_HW_HSW },   /* SDV GT1 desktop */
    { 0x0C12, MFX_HW_HSW },   /* SDV GT2 desktop */
    { 0x0C22, MFX_HW_HSW },   /* SDV GT2 desktop */
    { 0x0C0A, MFX_HW_HSW },   /* SDV GT1 server */
    { 0x0C1A, MFX_HW_HSW },   /* SDV GT2 server */
    { 0x0C2A, MFX_HW_HSW },   /* SDV GT2 server */
    { 0x0C06, MFX_HW_HSW },   /* SDV GT1 mobile */
    { 0x0C16, MFX_HW_HSW },   /* SDV GT2 mobile */
    { 0x0C26, MFX_HW_HSW },   /* SDV GT2 mobile */
    { 0x0A02, MFX_HW_HSW },   /* ULT GT1 desktop */
    { 0x0A12, MFX_HW_HSW },   /* ULT GT2 desktop */
    { 0x0A22, MFX_HW_HSW },   /* ULT GT2 desktop */
    { 0x0A0A, MFX_HW_HSW },   /* ULT GT1 server */
    { 0x0A1A, MFX_HW_HSW },   /* ULT GT2 server */
    { 0x0A2A, MFX_HW_HSW },   /* ULT GT2 server */
    { 0x0A06, MFX_HW_HSW },   /* ULT GT1 mobile */
    { 0x0A16, MFX_HW_HSW },   /* ULT GT2 mobile */
    { 0x0A26, MFX_HW_HSW },   /* ULT GT2 mobile */
    { 0x0D02, MFX_HW_HSW },   /* CRW GT1 desktop */
    { 0x0D12, MFX_HW_HSW },   /* CRW GT2 desktop */
    { 0x0D22, MFX_HW_HSW },   /* CRW GT2 desktop */
    { 0x0D0A, MFX_HW_HSW },   /* CRW GT1 server */
    { 0x0D1A, MFX_HW_HSW },   /* CRW GT2 server */
    { 0x0D2A, MFX_HW_HSW },   /* CRW GT2 server */
    { 0x0D06, MFX_HW_HSW },   /* CRW GT1 mobile */
    { 0x0D16, MFX_HW_HSW },   /* CRW GT2 mobile */
    { 0x0D26, MFX_HW_HSW },   /* CRW GT2 mobile */
    /* this dev IDs added per HSD 5264859 request  */
    { 0x040B, MFX_HW_HSW }, /*HASWELL_B_GT1 *//* Reserved */
    { 0x041B, MFX_HW_HSW }, /*HASWELL_B_GT2*/
    { 0x042B, MFX_HW_HSW }, /*HASWELL_B_GT3*/
    { 0x040E, MFX_HW_HSW }, /*HASWELL_E_GT1*//* Reserved */
    { 0x041E, MFX_HW_HSW }, /*HASWELL_E_GT2*/
    { 0x042E, MFX_HW_HSW }, /*HASWELL_E_GT3*/

    { 0x0C0B, MFX_HW_HSW }, /*HASWELL_SDV_B_GT1*/ /* Reserved */
    { 0x0C1B, MFX_HW_HSW }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2B, MFX_HW_HSW }, /*HASWELL_SDV_B_GT3*/
    { 0x0C0E, MFX_HW_HSW }, /*HASWELL_SDV_B_GT1*//* Reserved */
    { 0x0C1E, MFX_HW_HSW }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2E, MFX_HW_HSW }, /*HASWELL_SDV_B_GT3*/

    { 0x0A0B, MFX_HW_HSW }, /*HASWELL_ULT_B_GT1*/ /* Reserved */
    { 0x0A1B, MFX_HW_HSW }, /*HASWELL_ULT_B_GT2*/
    { 0x0A2B, MFX_HW_HSW }, /*HASWELL_ULT_B_GT3*/
    { 0x0A0E, MFX_HW_HSW }, /*HASWELL_ULT_E_GT1*/ /* Reserved */
    { 0x0A1E, MFX_HW_HSW }, /*HASWELL_ULT_E_GT2*/
    { 0x0A2E, MFX_HW_HSW }, /*HASWELL_ULT_E_GT3*/

    { 0x0D0B, MFX_HW_HSW }, /*HASWELL_CRW_B_GT1*/ /* Reserved */
    { 0x0D1B, MFX_HW_HSW }, /*HASWELL_CRW_B_GT2*/
    { 0x0D2B, MFX_HW_HSW }, /*HASWELL_CRW_B_GT3*/
    { 0x0D0E, MFX_HW_HSW }, /*HASWELL_CRW_E_GT1*/ /* Reserved */
    { 0x0D1E, MFX_HW_HSW }, /*HASWELL_CRW_E_GT2*/
    { 0x0D2E, MFX_HW_HSW }, /*HASWELL_CRW_E_GT3*/

    /* VLV */
    { 0x0f30, MFX_HW_VLV },   /* VLV mobile */
    { 0x0f31, MFX_HW_VLV },   /* VLV mobile */
    { 0x0f32, MFX_HW_VLV },   /* VLV mobile */
    { 0x0f33, MFX_HW_VLV },   /* VLV mobile */
    { 0x0157, MFX_HW_VLV },
    { 0x0155, MFX_HW_VLV },

    /* BDW */
    /*GT3: */
    { 0x162D, MFX_HW_BDW},
    { 0x162A, MFX_HW_BDW},
    /*GT2: */
    { 0x161D, MFX_HW_BDW},
    { 0x161A, MFX_HW_BDW},
    /* GT1: */
    { 0x160D, MFX_HW_BDW},
    { 0x160A, MFX_HW_BDW},
    /* BDW-ULT */
    /* (16x2 - ULT, 16x6 - ULT, 16xB - Iris, 16xE - ULX) */
    /*GT3: */
    { 0x162E, MFX_HW_BDW},
    { 0x162B, MFX_HW_BDW},
    { 0x1626, MFX_HW_BDW},
    { 0x1622, MFX_HW_BDW},
    /* GT2: */
    { 0x161E, MFX_HW_BDW},
    { 0x161B, MFX_HW_BDW},
    { 0x1616, MFX_HW_BDW},
    { 0x1612, MFX_HW_BDW},
    /* GT1: */
    { 0x160E, MFX_HW_BDW},
    { 0x160B, MFX_HW_BDW},
    { 0x1606, MFX_HW_BDW},
    { 0x1602, MFX_HW_BDW}
};

/* END: IOCTLs definitions */

static
eMFXHWType getPlatformType (VADisplay pVaDisplay)
{
    /* This is value by default */
    eMFXHWType retPlatformType = MFX_HW_UNKNOWN;
    int fd = 0, i = 0, listSize = 0;
    int devID = 0;
    int ret = 0;
    drm_i915_getparam_t gp;
    VADisplayContextP pDisplayContext_test = NULL;
    VADriverContextP  pDriverContext_test = NULL;

    pDisplayContext_test = (VADisplayContextP) pVaDisplay;
    pDriverContext_test  = pDisplayContext_test->pDriverContext;
    fd = *(int*) pDriverContext_test->drm_state;

    /* Now as we know real authenticated fd of VAAPI library,
     * we can call ioctl() to kernel mode driver,
     * get device ID and find out platform type
     * */
    gp.param = I915_PARAM_CHIPSET_ID;
    gp.value = &devID;

    ret = ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
    if (!ret)
    {
        listSize = (sizeof(listLegalDevIDs)/sizeof(mfx_device_item));
        for (i = 0; i < listSize; ++i)
        {
            if (listLegalDevIDs[i].device_id == devID)
            {
                retPlatformType = listLegalDevIDs[i].platform;
                break;
            }
        }
    }

    return retPlatformType;
} // eMFXHWType getPlatformType (VADisplay pVaDisplay)
#endif

eMFXHWType GetHWType(const mfxU32 adapterNum, mfxIMPL impl, mfxHDL& mfxDeviceHdl)
{
#if defined(_WIN32) || defined(_WIN64)
    if(MFX_IMPL_VIA_D3D9 == (impl & 0xF00))
        return GetHWTypeD3D9(adapterNum, &mfxDeviceHdl);
    else if(MFX_IMPL_VIA_D3D11 == (impl & 0xF00))
        return GetHWTypeD3D11(adapterNum, &mfxDeviceHdl);
#elif (defined(LINUX32) || defined(LINUX64))
    return getPlatformType ( /*(VADisplay)*/ mfxDeviceHdl);
#endif

    return MFX_HW_UNKNOWN;
}