/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_mft_hevcvd.cpp

Purpose: contains registration data for HEVC decoder MFT.

*********************************************************************************/

#include "mf_utils.h"
#include "mfx_mft_hevcvd.h"

/*------------------------------------------------------------------------------*/

//#undef MF_TRACE_OFILE_NAME
//#define MF_TRACE_OFILE_NAME "C:\\mfx_mft_mjpgvd.log"

/*------------------------------------------------------------------------------*/

mfxStatus FillHevcDecoderParams(mfxVideoParam* pVideoParams)
{
    CHECK_POINTER(pVideoParams, MFX_ERR_NULL_PTR);

    pVideoParams->mfx.NumThread = (mfxU16)mf_get_cpu_num();
    pVideoParams->mfx.CodecId   = MFX_CODEC_HEVC;
    pVideoParams->mfx.ExtendedPicStruct = 1; // TODO: [AB] - What is this?
    return MFX_ERR_NONE;
}

//attributes stored in registry for hardware mft, requires to be visible as device in HCK studio
HRESULT GetHMFTRegAttributes(IMFAttributes* pAttributes)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    // checking errors
    CHECK_POINTER(pAttributes, E_POINTER);
    // setting attributes required for ASync plug-ins
    hr = pAttributes->SetUINT32(MF_TRANSFORM_ASYNC, TRUE);
    if (SUCCEEDED(hr))
        hr = pAttributes->SetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, TRUE);
    if (SUCCEEDED(hr))
        hr = pAttributes->SetString(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute, _T("VEN_8086"));
    if (SUCCEEDED(hr))
        hr = pAttributes->SetString(MFT_ENUM_HARDWARE_URL_Attribute, MFT_ENUM_HARDWARE_URL_STRING);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

static const GUID_info g_InputTypes[] =
{
    { MFMediaType_Video, MFVideoFormat_HEVC }, //'HEVC' High Efficiency Video Codec


};

static const GUID_info g_OutputTypes[] =
{
    { MFMediaType_Video, MFVideoFormat_NV12 },
    { MFMediaType_Video, MFVideoFormat_NV12_MFX }
};

static ClassRegData g_RegData =
{
    (GUID*)&CLSID_MF_HEVCDecFilter,
    TEXT(MFX_MF_PLUGIN_NAME),
    REG_AS_VIDEO_DECODER,
    CreateVDecPlugin,
    FillHevcDecoderParams,
    NULL,
    (GUID_info*)g_InputTypes, ARRAY_SIZE(g_InputTypes),
    (GUID_info*)g_OutputTypes, ARRAY_SIZE(g_OutputTypes),
    g_DecoderRegFlags
};

/*------------------------------------------------------------------------------*/

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID /*lpReserved*/)
{
    BOOL ret = false;

    ret = myDllMain(hModule, ul_reason_for_call, &g_RegData, 1);
    if (DLL_PROCESS_DETACH == ul_reason_for_call)
    {
        SAFE_RELEASE(g_RegData.pAttributes);
    }
    return ret;
}
