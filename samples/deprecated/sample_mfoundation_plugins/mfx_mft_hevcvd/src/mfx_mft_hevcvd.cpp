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

/*********************************************************************************

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
