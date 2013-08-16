/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <tchar.h>

#include <d3d9.h>
#include <dxva2api.h>

#include "pavp_sample_common_dx9.h"
#include "intel_pavp_gpucp_api.h"

HRESULT CreateD3D9AndDeviceEx(IDirect3D9Ex ** pd3d9, IDirect3DDevice9Ex **ppd3d9DeviceEx)
{
    HRESULT                    hr = S_OK;
    D3DPRESENT_PARAMETERS    d3dPresParams;
    IDirect3DDevice9Ex        *pNewDevice = NULL;

    if( NULL == pd3d9 || NULL == ppd3d9DeviceEx)
        return E_POINTER;

    *pd3d9 = NULL;
    *ppd3d9DeviceEx = NULL;
    ZeroMemory(&d3dPresParams, sizeof(d3dPresParams));

    do
    {
        hr = Direct3DCreate9Ex(D3D_SDK_VERSION, pd3d9);
        if(FAILED(hr))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Could not create D3D9 (hr=0x%08x)\n"), hr));
            hr = E_FAIL;
            break;
        }

        d3dPresParams.BackBufferWidth    = GetSystemMetrics(SM_CXSCREEN);
        d3dPresParams.BackBufferHeight    = GetSystemMetrics(SM_CYSCREEN);
        d3dPresParams.BackBufferFormat    = D3DFMT_X8R8G8B8;
        d3dPresParams.BackBufferCount    = 1;
        d3dPresParams.SwapEffect        = D3DSWAPEFFECT_DISCARD;
        d3dPresParams.Windowed            = TRUE;

        d3dPresParams.Flags                            = D3DPRESENTFLAG_VIDEO | D3DPRESENTFLAG_RESTRICTED_CONTENT | D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER;
        d3dPresParams.FullScreen_RefreshRateInHz    = D3DPRESENT_RATE_DEFAULT;
        d3dPresParams.PresentationInterval            = D3DPRESENT_INTERVAL_ONE;
   
        hr = (*pd3d9)->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, 
                                    NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                    &d3dPresParams, NULL, &pNewDevice );
        if( FAILED(hr) )
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Could not create D3D9 device (hr=0x%08x)\n"), hr));
            pNewDevice = NULL;
            break;
        }

        *ppd3d9DeviceEx = pNewDevice;
        (*ppd3d9DeviceEx)->AddRef();
        PAVPSDK_SAFE_RELEASE(pNewDevice);
    } while( FALSE );

    if (FAILED(hr))
    {
        PAVPSDK_SAFE_RELEASE(*ppd3d9DeviceEx);
        PAVPSDK_SAFE_RELEASE(*pd3d9);
    }

    return hr;
}




HRESULT CCPDeviceCryptoSession9_create(
    IDirect3DDevice9Ex *pd3d9Device, 
    GUID profile, 
    PAVP_ENCRYPTION_TYPE encryptionType,
    DWORD capsMask, 
    CCPDevice** hwDevice, 
    HANDLE *cs9Handle,
    GUID *cryptoType)
{
    HRESULT                hr = S_OK;
    GUID PAVPKeyExchangeGUID = GUID_NULL;
    HANDLE _cs9handle;
    GUID _cryptoType = GUID_NULL;

    IDirect3DDevice9Video *p3DDevice9Video = NULL;
    IDirect3DCryptoSession9* cs9 = NULL;

    if (NULL == pd3d9Device || NULL == hwDevice)
        return E_POINTER;

    hr = pd3d9Device->QueryInterface(
        IID_IDirect3DDevice9Video,
        (void **)&p3DDevice9Video);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D9 video device (hr=0x%08x)\n"), hr));
        goto end;
    } 

    // find key exchange mode or verify requested mode is supported
    D3DCONTENTPROTECTIONCAPS contentProtectionCaps;
    if (NULL != cryptoType && GUID_NULL != *cryptoType)
    {
        _cryptoType = *cryptoType;
        hr = p3DDevice9Video->GetContentProtectionCaps(
            &_cryptoType,
            &profile,
            &contentProtectionCaps);
        if (FAILED(hr) ||
            !(D3DKEYEXCHANGE_EPID == contentProtectionCaps.KeyExchangeType &&
            (contentProtectionCaps.Caps & capsMask)))
            _cryptoType = GUID_NULL;
    }
    else
    {
        _cryptoType = PAVPCryptoSession9_findCryptoType(
            p3DDevice9Video, 
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

    hr = p3DDevice9Video->CreateCryptoSession(
        &_cryptoType, 
        &profile, 
        &cs9,
        &_cs9handle);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create crypto session (hr=0x%08x)\n"), hr));
        goto end;
    } 

    // Prefer SW content protection implementation if both HW or SW avaliable.
    *hwDevice = NULL;
    if ((contentProtectionCaps.Caps & capsMask & D3DCPCAPS_SOFTWARE) 
        ||
        // Only need to pass D3DKEYEXCHANGE_HW_PROTECT when option exists.
        // Otherwise pass D3DKEYEXCHANGE_EPID and HW or SW created according 
        // to contentProtectionCaps.Caps.
        ((contentProtectionCaps.Caps & capsMask & D3DCPCAPS_HARDWARE) && 
            !(contentProtectionCaps.Caps & D3DCPCAPS_SOFTWARE && contentProtectionCaps.Caps & D3DCPCAPS_HARDWARE)
        )
       )
        *hwDevice = new CCPDeviceCryptoSession9(cs9, _cs9handle, D3DKEYEXCHANGE_EPID, _cryptoType, profile, &contentProtectionCaps);
    else if (contentProtectionCaps.Caps & capsMask & D3DCPCAPS_HARDWARE)
        *hwDevice = new CCPDeviceCryptoSession9(cs9, _cs9handle, D3DKEYEXCHANGE_HW_PROTECT, _cryptoType, profile, &contentProtectionCaps);
    else
    {
        hr = E_NOTIMPL;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9\n")));
        goto end;
    }
    if (NULL == *hwDevice)
    {
        hr = E_OUTOFMEMORY;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCryptoSession9: out of memory\n")));
        goto end;
    }

    if (NULL != cs9Handle)
        *cs9Handle = _cs9handle;
    if (NULL != cryptoType)
        *cryptoType = _cryptoType;


end:
    if (FAILED(hr))
        PAVPSDK_SAFE_DELETE(*hwDevice);
    PAVPSDK_SAFE_RELEASE(cs9);
    PAVPSDK_SAFE_RELEASE(p3DDevice9Video);

    return hr;
}
