/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/

#include "pavpsdk_defs.h"

#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8

#include <tchar.h>
#include <d3d11.h>

#include "pavp_sample_common_dx11.h"
#include <d3d9.h> // auth channel commands in intel_pavp_gpucp_api.h
#include "intel_pavp_gpucp_api.h"


HRESULT CCPDeviceD3D11_create(
    ID3D11Device* device, 
    GUID profile,
    PAVP_ENCRYPTION_TYPE encryptionType,
    DWORD capsMask, 
    CCPDevice** hwDevice, 
    GUID *cryptoType)
{
    HRESULT                hr = S_OK;
    GUID _cryptoType = GUID_NULL;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS contentProtectionCaps = {0};

    ID3D11VideoDevice *videoDevice = NULL;
    ID3D11CryptoSession* cs = NULL;
    ID3D11DeviceContext* context = NULL; 
    ID3D11VideoContext *videoContext = NULL;

    if (NULL == device || NULL == hwDevice)
        return E_POINTER;

    hr = device->QueryInterface(
        IID_ID3D11VideoDevice,
        (void **)&videoDevice);    
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D11 video device (hr=0x%08x)\n"), hr));
        goto end;
    } 

    // find key exchange mode or verify requested mode is supported
    if (NULL != cryptoType && GUID_NULL != *cryptoType)
    {
        _cryptoType = *cryptoType;
        hr = videoDevice->GetContentProtectionCaps(
            &_cryptoType,
            &profile,
            &contentProtectionCaps);
        if (FAILED(hr))
            _cryptoType = GUID_NULL;
        uint32 i = 0;
        for(i = 0; i < contentProtectionCaps.KeyExchangeTypeCount; i++)
        {
            GUID keyExchangeIt = GUID_NULL;
            videoDevice->CheckCryptoKeyExchange(&_cryptoType, &profile, i, &keyExchangeIt); 
            if (D3DKEYEXCHANGE_EPID == keyExchangeIt)
                break;
        }
        if (!(i < contentProtectionCaps.KeyExchangeTypeCount &&
            (contentProtectionCaps.Caps & capsMask)))
            _cryptoType = GUID_NULL;
    }
    else
    {
        _cryptoType = PAVPD3D11_findCryptoType(
            videoDevice, 
            profile,
            encryptionType,
            D3DKEYEXCHANGE_EPID,
            capsMask,
            &contentProtectionCaps);
    }
    if (GUID_NULL == _cryptoType)
    {
        hr = E_FAIL;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can't find content protection caps.\n")));
        goto end;
    }

    hr = videoDevice->CreateCryptoSession(
        &_cryptoType, 
        &profile,
        &D3DKEYEXCHANGE_EPID,
        &cs);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create crypto session (hr=0x%08x)\n"), hr));
        goto end;
    } 

    {
        device->GetImmediateContext(&context);
        hr = context->QueryInterface(
            IID_ID3D11VideoContext,
            (void **)&videoContext);    
        PAVPSDK_SAFE_RELEASE(context);
    }
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D11 video context (hr=0x%08x)\n"), hr));
        goto end;
    } 

    // Prefer SW content protection implementation if both HW or SW avaliable.
    *hwDevice = NULL;
    if ((contentProtectionCaps.Caps & capsMask & D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE) 
        ||
        // Only need to pass D3DKEYEXCHANGE_HW_PROTECT when option exists.
        // Otherwise pass D3DKEYEXCHANGE_EPID and HW or SW created according 
        // to contentProtectionCaps.Caps.
        ((contentProtectionCaps.Caps & capsMask & D3D11_CONTENT_PROTECTION_CAPS_HARDWARE) && 
            !(contentProtectionCaps.Caps & D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE && contentProtectionCaps.Caps & D3D11_CONTENT_PROTECTION_CAPS_HARDWARE)
        )
       )
        *hwDevice = new CCPDeviceD3D11(cs, videoContext, D3DKEYEXCHANGE_EPID, &contentProtectionCaps);
    else if (contentProtectionCaps.Caps & capsMask & D3D11_CONTENT_PROTECTION_CAPS_HARDWARE)
        *hwDevice = new CCPDeviceD3D11(cs, videoContext, D3DKEYEXCHANGE_HW_PROTECT, &contentProtectionCaps);
    else
    {
        hr = E_NOTIMPL;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceD3D11\n")));
        goto end;
    }
    if (NULL == *hwDevice)
    {
        hr = E_OUTOFMEMORY;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCD3D11: out of memory\n")));
        goto end;
    }

    if (NULL != cryptoType)
        *cryptoType = _cryptoType;

end:
    if (FAILED(hr))
        PAVPSDK_SAFE_DELETE(*hwDevice);
    PAVPSDK_SAFE_RELEASE(videoDevice);
    PAVPSDK_SAFE_RELEASE(cs);
    PAVPSDK_SAFE_RELEASE(videoContext);

    return hr;
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602))