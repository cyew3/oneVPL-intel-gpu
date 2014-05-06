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
#include "mscrypto.h"
#include "win8ui_video_lib.h"

extern "C" HRESULT CCPDeviceD3D11_create4(
    ID3D11Device* device, 
    const GUID *cryptoType,
    const GUID *profile,
    const GUID *keyExchangeType,
    CCPDevice** hwDevice)
{
    HRESULT hr = S_OK;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS contentProtectionCaps = {0};

    ID3D11VideoDevice *videoDevice = NULL;
    ID3D11CryptoSession* cs = NULL;
    ID3D11DeviceContext* context = NULL; 
    ID3D11VideoContext *videoContext = NULL;

    if (NULL == device || NULL == hwDevice || NULL == keyExchangeType)
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
    hr = videoDevice->GetContentProtectionCaps(
        cryptoType,
        profile,
        &contentProtectionCaps);
    uint32 i = 0;
    for(i = 0; i < contentProtectionCaps.KeyExchangeTypeCount; i++)
    {
        GUID keyExchangeIt = GUID_NULL;
        videoDevice->CheckCryptoKeyExchange(cryptoType, profile, i, &keyExchangeIt); 
        if (*keyExchangeType == keyExchangeIt)
            break;
    }
    if (0 == contentProtectionCaps.KeyExchangeTypeCount || 
        i >= contentProtectionCaps.KeyExchangeTypeCount)
    {
        hr = E_FAIL;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can't find content protection caps.\n")));
        goto end;
    }

    hr = videoDevice->CreateCryptoSession(
        cryptoType, 
        profile,
        keyExchangeType,
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

    *hwDevice = new CCPDeviceD3D11Base(cs, videoContext, *keyExchangeType, &contentProtectionCaps);
    if (NULL == *hwDevice)
    {
        hr = E_OUTOFMEMORY;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create CCPDeviceCD3D11: out of memory\n")));
        goto end;
    }

end:
    if (FAILED(hr))
        PAVPSDK_SAFE_DELETE(*hwDevice);
    PAVPSDK_SAFE_RELEASE(videoDevice);
    PAVPSDK_SAFE_RELEASE(cs);
    PAVPSDK_SAFE_RELEASE(videoContext);

    return hr;
}


HRESULT CCPDeviceD3D11_NegotiateStreamKey(
    CCPDeviceD3D11Base *cpDevice,
    const uint8 *streamKey, 
    uint32 streamKeySize)
{
    HRESULT                     hr                  = S_OK;
    ID3D11CryptoSession *cs = NULL;
    UINT                        CertLen             = 0;
    BYTE                        *Cert = NULL;
    BYTE *pbDataOut = NULL;
    DWORD cbDataOut = 0;

    if (NULL == cpDevice)
        return E_POINTER;
    cs = cpDevice->GetCryptoSessionPtr();

    hr = cs->GetCertificateSize(&CertLen);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling ID3D11CryptoSession::GetCertificateSize (hr=0x%08x)\n"), hr));
        goto done;
    }
    if (CertLen <= 0)
    {
        hr = E_FAIL;
        goto done;
    }

    Cert = new BYTE[CertLen];
    if (NULL == Cert)
    {
        hr = E_FAIL;
        goto done;
    }

    //memset(Cert, 0, CertLen);

    // Get Certificate
    hr = cs->GetCertificate(
        CertLen,
        &Cert[0]);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling ID3D11CryptoSession::GetCertificate (hr=0x%08x)\n"), hr));
        goto done;
    }

    hr = BCryptGenRandom(NULL, (PUCHAR)streamKey, streamKeySize, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptGenRandom (hr=0x%08x)\n"), hr));
        goto done;
    }
    hr = RSAES_OAEP_Encrypt(
            &Cert[0],
            CertLen,
            streamKey,
            streamKeySize,
            &pbDataOut,
            &cbDataOut);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling RSAES_OAEP_Encrypt (hr=0x%08x)\n"), hr));
        goto done;
    }
    // Complete the handshake.
    hr = cpDevice->GetVideoContextPtr()->NegotiateCryptoSessionKeyExchange(
        cs,
        cbDataOut,
        pbDataOut);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to NegotiateCryptoSessionKeyExchange (hr=0x%08x)\n"), hr));
        goto done;
    }

done:
    PAVPSDK_SAFE_DELETE_ARRAY(pbDataOut);
    PAVPSDK_SAFE_DELETE_ARRAY(Cert);

    return hr;
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602))