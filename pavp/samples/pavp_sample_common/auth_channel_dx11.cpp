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

#include "pavp_sample_common_dx11.h"
#include <d3d9.h> // for auth channel commands in intel_pavp_gpucp_api.h
#include "intel_pavp_gpucp_api.h"

#include "mscrypto.h"

HRESULT SAuthChannel11_Create(
    SAuthChannel11 **_ctx,
    CCPDevice* hwDevice)
{
    HRESULT hr = S_OK;
    ID3D11Device *pD3D11Device = NULL;
    GUID cryptoType = GUID_NULL;
    GUID profile = GUID_NULL;

    if (NULL == _ctx || NULL == hwDevice)
    {
        hr = E_POINTER;
        goto end;
    }
    *_ctx = NULL;

    CCPDeviceD3D11Base *_hwDevice = dynamic_cast<CCPDeviceD3D11Base*>(hwDevice);
    if (NULL == _hwDevice)
    {
        hr = E_POINTER;
        goto end;
    }

    ID3D11CryptoSession *cs = _hwDevice->GetCryptoSessionPtr();
    if (NULL == cs)
    {
        hr = E_POINTER;
        goto end;
    }

    cs->GetDevice(&pD3D11Device);
    if (NULL == pD3D11Device)
    {
        hr = E_POINTER;
        goto end;
    }

    D3D11_VIDEO_CONTENT_PROTECTION_CAPS contentProtectionCaps;
    _hwDevice->GetContentProtectionCaps(&contentProtectionCaps);

    D3D11_AUTHENTICATED_CHANNEL_TYPE channelType;
    if ((contentProtectionCaps.Caps & D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE && contentProtectionCaps.Caps & D3D11_CONTENT_PROTECTION_CAPS_HARDWARE))
    {
        GUID actualKeyExchangeGuid = _hwDevice->GetKeyExchangeGUID();
        channelType = D3DKEYEXCHANGE_EPID == actualKeyExchangeGuid ? D3D11_AUTHENTICATED_CHANNEL_DRIVER_SOFTWARE : D3D11_AUTHENTICATED_CHANNEL_DRIVER_HARDWARE;
    }
    else
        channelType = contentProtectionCaps.Caps & D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE ? D3D11_AUTHENTICATED_CHANNEL_DRIVER_SOFTWARE : D3D11_AUTHENTICATED_CHANNEL_DRIVER_HARDWARE;
    hr = SAuthChannel11_Create4(_ctx, pD3D11Device, channelType);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create authenticated channel (hr=0x%08x)\n"), hr));
        goto end;
    } 

end:
    PAVPSDK_SAFE_RELEASE(pD3D11Device);
    return hr;
}


HRESULT SAuthChannel11_Create4(
    SAuthChannel11 **_ctx,
    ID3D11Device* device, 
    D3D11_AUTHENTICATED_CHANNEL_TYPE channelType)
{
    HRESULT                     hr                  = S_OK;
    ID3D11DeviceContext* context = NULL;
    ID3D11VideoDevice *videoDevice = NULL;
    UINT                        CertLen             = 0;
    BYTE                        *Cert = NULL;
    BYTE                        pSignature[256];
    unsigned char               coppSigEncr[256];
    unsigned int                signatureSize = 0;
    BYTE *pbDataOut = NULL;
    DWORD cbDataOut = 0;
    D3D11_AUTHENTICATED_CONFIGURE_OUTPUT  output;

    if (NULL == _ctx || NULL == device)
        return E_POINTER;

    SAuthChannel11 *ctx = new SAuthChannel11;
    if (NULL == ctx)
        return E_OUTOFMEMORY;
    ZeroMemory(ctx, sizeof(*ctx));

    hr = device->QueryInterface(
        IID_ID3D11VideoDevice,
        (void **)&videoDevice);    
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D11 video device (hr=0x%08x)\n"), hr));
        goto done;
    } 

    hr = videoDevice->CreateAuthenticatedChannel(
        channelType,
        &(ctx->authChannel));
    if (FAILED (hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling ID3D11VideoDevice::CreateAuthenticatedChannel (hr=0x%08x)\n"), hr));
        goto done;
    }
    if (NULL == ctx->authChannel)
    {
        hr = E_FAIL;
        goto done;
    }

    hr = ctx->authChannel->GetCertificateSize(&CertLen);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling IDirect3DAuthenticatedChannel9::GetCertificateSize (hr=0x%08x)\n"), hr));
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
    hr = ctx->authChannel->GetCertificate(
        CertLen,
        &Cert[0]);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling IDirect3DAuthenticatedChannel9::GetCertificate (hr=0x%08x)\n"), hr));
        goto done;
    }

    signatureSize = sizeof(coppSigEncr);

    hr = BCryptGenRandom(NULL, (PUCHAR)ctx->key, sizeof(ctx->key), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptGenRandom (hr=0x%08x)\n"), hr));
        goto done;
    }
    hr = RSAES_OAEP_Encrypt(
            &Cert[0],
            CertLen,
            ctx->key,
            sizeof(ctx->key),
            &pbDataOut,
            &cbDataOut);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling RSAES_OAEP_Encrypt (hr=0x%08x)\n"), hr));
        goto done;
    }


    {
        device->GetImmediateContext(&context);
        hr = context->QueryInterface(
            IID_ID3D11VideoContext,
            (void **)&ctx->videoContext);
        PAVPSDK_SAFE_RELEASE(context);
    }
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D11 video context (hr=0x%08x)\n"), hr));
        goto done;
    } 

    // Complete the handshake.
    hr = ctx->videoContext->NegotiateAuthenticatedChannelKeyExchange(
        ctx->authChannel,
        cbDataOut,
        pbDataOut
        );

    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to NegotiateAuthenticatedChannelKeyExchange (hr=0x%08x)\n"), hr));
        goto done;
    }

    // Get authenticated channel handle
    HANDLE authChannelHandle = NULL;
    ctx->authChannel->GetChannelHandle(&authChannelHandle);


    // Call configure to initialize the authenticated channel
    hr = BCryptGenRandom(NULL, (PUCHAR)&ctx->uiConfigSequence, sizeof(UINT), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptGenRandom (hr=0x%08x)\n"), hr));
        goto done;
    }
    hr = BCryptGenRandom(NULL, (PUCHAR)&ctx->uiQuerySequence, sizeof(UINT), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptGenRandom (hr=0x%08x)\n"), hr));
        goto done;
    }
    ZeroMemory(pSignature, signatureSize);
    D3D11_AUTHENTICATED_CONFIGURE_INITIALIZE_INPUT
        *p_config_init = (D3D11_AUTHENTICATED_CONFIGURE_INITIALIZE_INPUT*)pSignature;
    p_config_init->StartSequenceConfigure = ctx->uiConfigSequence;
    p_config_init->StartSequenceQuery = ctx->uiQuerySequence;
    p_config_init->Parameters.ConfigureType = D3D11_AUTHENTICATED_CONFIGURE_INITIALIZE;
    p_config_init->Parameters.hChannel = authChannelHandle;
    p_config_init->Parameters.SequenceNumber = ctx->uiConfigSequence++;

    hr = ComputeOMAC(
        ctx->key, // Session key
        pSignature + sizeof(D3D11_OMAC),                 // Data
        sizeof(D3D11_AUTHENTICATED_CONFIGURE_INITIALIZE_INPUT) - sizeof(D3D11_OMAC),              // Size
        (BYTE*)pSignature);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = ctx->videoContext->ConfigureAuthenticatedChannel(
        ctx->authChannel,
        sizeof(D3D11_AUTHENTICATED_CONFIGURE_INITIALIZE_INPUT),
        pSignature,
        &output);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling ID3D11VideoContext::ConfigureAuthenticatedChannel (hr=0x%08x)\n"), hr));
        goto done;
    }
done:
    PAVPSDK_SAFE_RELEASE(videoDevice);
    PAVPSDK_SAFE_DELETE_ARRAY(pbDataOut);
    PAVPSDK_SAFE_DELETE_ARRAY(Cert);
    if (!FAILED(hr))
        *_ctx = ctx;
    else
    {
        PAVPSDK_SAFE_DELETE(ctx);
        SAuthChannel11_Destroy(ctx);
    }

    return hr;
}


HRESULT SAuthChannel11_ConfigureCryptoSession(
    SAuthChannel11 *ctx,
    HANDLE cryptoSessionHandle,
    HANDLE decoderHandle)
{
    BYTE                                                buffer[256];
    D3D11_AUTHENTICATED_QUERY_INPUT                 input;
    D3D11_AUTHENTICATED_QUERY_DEVICE_HANDLE_OUTPUT*     p_query_device;
    D3D11_AUTHENTICATED_CONFIGURE_OUTPUT                output;
    D3D11_AUTHENTICATED_CONFIGURE_CRYPTO_SESSION_INPUT* p_config_crypto_session;
    HANDLE hDevice = NULL;
    HRESULT hr;

    if (NULL == ctx)
        return E_POINTER;

    // Get authenticated channel handle
    HANDLE authChannelHandle = NULL;
    ctx->authChannel->GetChannelHandle(&authChannelHandle);

    // Query for the device handle
    ZeroMemory(buffer, sizeof(buffer));
    input.hChannel = authChannelHandle;
    input.QueryType = D3D11_AUTHENTICATED_QUERY_DEVICE_HANDLE;
    input.SequenceNumber = ctx->uiQuerySequence++;
    hr = ctx->videoContext->QueryAuthenticatedChannel(
        ctx->authChannel,
        sizeof(input), 
        &input, 
        sizeof(D3D11_AUTHENTICATED_QUERY_DEVICE_HANDLE_OUTPUT), 
        buffer);
    if (FAILED(hr))
        return hr;
    p_query_device = (D3D11_AUTHENTICATED_QUERY_DEVICE_HANDLE_OUTPUT*) buffer;
    hDevice = p_query_device->DeviceHandle;

    // Call configure to set the crypto session
    ZeroMemory(buffer, sizeof(buffer));
    p_config_crypto_session = (D3D11_AUTHENTICATED_CONFIGURE_CRYPTO_SESSION_INPUT*) buffer;
    p_config_crypto_session->DecoderHandle = decoderHandle;
    p_config_crypto_session->CryptoSessionHandle = cryptoSessionHandle;
    p_config_crypto_session->DeviceHandle = hDevice;
    p_config_crypto_session->Parameters.ConfigureType = D3D11_AUTHENTICATED_CONFIGURE_CRYPTO_SESSION;
    p_config_crypto_session->Parameters.hChannel = authChannelHandle;
    p_config_crypto_session->Parameters.SequenceNumber = ctx->uiConfigSequence++;
    hr = ComputeOMAC(
        ctx->key,
        buffer + sizeof(D3D11_OMAC),
        sizeof(D3D11_AUTHENTICATED_CONFIGURE_CRYPTO_SESSION_INPUT) - sizeof(D3D11_OMAC),
        (BYTE*)buffer);
    if (FAILED(hr))
        return hr;
    hr = ctx->videoContext->ConfigureAuthenticatedChannel(
        ctx->authChannel,
        sizeof(D3D11_AUTHENTICATED_CONFIGURE_CRYPTO_SESSION_INPUT), 
        buffer, 
        &output);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling ID3D11VideoContext::Configure (hr=0x%08x)\n"), hr));
        return hr;
    }

    return hr;
}

void SAuthChannel11_Destroy(SAuthChannel11 *ctx)
{
    if (NULL != ctx)
    {
        PAVPSDK_SAFE_RELEASE(ctx->authChannel);
        PAVPSDK_SAFE_RELEASE(ctx->videoContext);
        SecureZeroMemory(ctx, sizeof(SAuthChannel11));
        PAVPSDK_SAFE_DELETE(ctx);
    }
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602))