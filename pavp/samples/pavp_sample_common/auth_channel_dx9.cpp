/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include "pavp_sample_common_dx9.h"
#include "intel_pavp_gpucp_api.h"
#include "mscrypto.h"

HRESULT SAuthChannel9_Create(
    SAuthChannel9 **_ctx,
    CCPDevice* hwDevice,
    IDirect3DDevice9Ex *pd3d9Device)
{
    HRESULT hr = S_OK;

    if (NULL == _ctx || NULL == hwDevice || NULL == pd3d9Device)
    {
        hr = E_POINTER;
        goto end;
    }
    *_ctx = NULL;

    CCPDeviceCryptoSession9 *_hwDevice = dynamic_cast<CCPDeviceCryptoSession9*>(hwDevice);
    if (NULL == _hwDevice)
    {
        hr = E_POINTER;
        goto end;
    }

    D3DCONTENTPROTECTIONCAPS contentProtectionCaps;
    _hwDevice->GetContentProtectionCaps(&contentProtectionCaps);

    D3DAUTHENTICATEDCHANNELTYPE channelType;
    if ((contentProtectionCaps.Caps & D3DCPCAPS_SOFTWARE && contentProtectionCaps.Caps & D3DCPCAPS_HARDWARE))
    {
        GUID actualKeyExchangeGuid = _hwDevice->GetPAVPKeyExchangeGUID();
        channelType = D3DKEYEXCHANGE_EPID == actualKeyExchangeGuid ? D3DAUTHENTICATEDCHANNEL_DRIVER_SOFTWARE : D3DAUTHENTICATEDCHANNEL_DRIVER_HARDWARE;
    }
    else
        channelType = contentProtectionCaps.Caps & D3DCPCAPS_SOFTWARE ? D3DAUTHENTICATEDCHANNEL_DRIVER_SOFTWARE : D3DAUTHENTICATEDCHANNEL_DRIVER_HARDWARE;
    hr = SAuthChannel9_Create4(_ctx, pd3d9Device, channelType);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create authenticated channel (hr=0x%08x)\n"), hr));
        goto end;
    } 
end:
    return hr;
}


HRESULT SAuthChannel9_Create4(
    SAuthChannel9 **_ctx,
    IDirect3DDevice9Ex *pd3d9Device, 
    D3DAUTHENTICATEDCHANNELTYPE channelType)
{
    HRESULT                     hr                  = S_OK;
    IDirect3DDevice9Video       *p3DDevice9Video = NULL;
    UINT                        CertLen             = 0;
    BYTE                        *Cert = NULL;
    BYTE                        pSignature[256];
    unsigned char               coppSigEncr[256];
    unsigned int                signatureSize = 0;
    BYTE *pbDataOut = NULL;
    DWORD cbDataOut = 0;
    D3DAUTHENTICATEDCHANNEL_CONFIGURE_OUTPUT  output;
    
    if (NULL == _ctx || NULL == pd3d9Device)
        return E_POINTER;

    SAuthChannel9 *ctx = new SAuthChannel9;
    if (NULL == ctx)
        return E_OUTOFMEMORY;
    ZeroMemory(ctx, sizeof(*ctx));

    hr = pd3d9Device->QueryInterface(
        IID_IDirect3DDevice9Video,
        (void **)&p3DDevice9Video);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to create a D3D9 video device (hr=0x%08x)\n"), hr));
        goto done;
    } 

    hr = p3DDevice9Video->CreateAuthenticatedChannel(
        channelType,
        &(ctx->authChannel),
        &(ctx->authChannelHandle));
    if (FAILED (hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling IDirect3DAuthenticatedChannel9::GetCertificateSize (hr=0x%08x)\n"), hr));
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


    // Complete the handshake.
    hr = ctx->authChannel->NegotiateKeyExchange(
        cbDataOut,
        pbDataOut
        );

    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to NegotiateKeyExchange (hr=0x%08x)\n"), hr));
        goto done;
    }

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
    D3DAUTHENTICATEDCHANNEL_CONFIGUREINITIALIZE
        *p_config_init = (D3DAUTHENTICATEDCHANNEL_CONFIGUREINITIALIZE*)pSignature;
    p_config_init->StartSequenceConfigure = ctx->uiConfigSequence;
    p_config_init->StartSequenceQuery = ctx->uiQuerySequence;
    p_config_init->Parameters.ConfigureType = D3DAUTHENTICATEDCONFIGURE_INITIALIZE;
    p_config_init->Parameters.hChannel = ctx->authChannelHandle;
    p_config_init->Parameters.SequenceNumber = ctx->uiConfigSequence++;

    hr = ComputeOMAC(
        ctx->key, // Session key
        pSignature + sizeof(D3D_OMAC),                 // Data
        sizeof(D3DAUTHENTICATEDCHANNEL_CONFIGUREINITIALIZE) - sizeof(D3D_OMAC),              // Size
        (BYTE*)pSignature);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = ctx->authChannel->Configure(
        sizeof(D3DAUTHENTICATEDCHANNEL_CONFIGUREINITIALIZE),
        pSignature,
        &output);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling IDirect3DAuthenticatedChannel9::Configure (hr=0x%08x)\n"), hr));
        goto done;
    }
done:
    PAVPSDK_SAFE_RELEASE(p3DDevice9Video);
    PAVPSDK_SAFE_DELETE_ARRAY(pbDataOut);
    PAVPSDK_SAFE_DELETE_ARRAY(Cert);
    if (!FAILED(hr))
        *_ctx = ctx;
    else
    {
        PAVPSDK_SAFE_DELETE(ctx);
        SAuthChannel9_Destroy(ctx);
    }

    return hr;
}


HRESULT SAuthChannel9_ConfigureCryptoSession(
    SAuthChannel9 *ctx,
    HANDLE cryptoSessionHandle,
    HANDLE DXVA2DecodeHandle)
{
    BYTE                                                buffer[256];
    D3DAUTHENTICATEDCHANNEL_QUERY_INPUT                 input;
    D3DAUTHENTICATEDCHANNEL_QUERYDEVICEHANDLE_OUTPUT*   p_query_device;
    D3DAUTHENTICATEDCHANNEL_CONFIGURE_OUTPUT            output;
    D3DAUTHENTICATEDCHANNEL_CONFIGURECRYPTOSESSION*     p_config_crypto_session;
    HANDLE hDevice = NULL;
    HRESULT hr;

    if (NULL == ctx)
        return E_POINTER;

    // Query for the device handle
    ZeroMemory(buffer, sizeof(buffer));
    input.hChannel = ctx->authChannelHandle;
    input.QueryType = D3DAUTHENTICATEDQUERY_DEVICEHANDLE;
    input.SequenceNumber = ctx->uiQuerySequence++;
    hr = ctx->authChannel->Query(sizeof(input), &input, sizeof(D3DAUTHENTICATEDCHANNEL_QUERYDEVICEHANDLE_OUTPUT), buffer);
    if (FAILED(hr))
        return hr;
    p_query_device = (D3DAUTHENTICATEDCHANNEL_QUERYDEVICEHANDLE_OUTPUT*) buffer;
    hDevice = p_query_device->DeviceHandle;

    // Call configure to set the crypto session
    ZeroMemory(buffer, sizeof(buffer));
    p_config_crypto_session = (D3DAUTHENTICATEDCHANNEL_CONFIGURECRYPTOSESSION*) buffer;
    p_config_crypto_session->CryptoSessionHandle = cryptoSessionHandle;
    p_config_crypto_session->DXVA2DecodeHandle = DXVA2DecodeHandle;
    p_config_crypto_session->DeviceHandle = hDevice;
    p_config_crypto_session->Parameters.ConfigureType = D3DAUTHENTICATEDCONFIGURE_CRYPTOSESSION;
    p_config_crypto_session->Parameters.hChannel = ctx->authChannelHandle;
    p_config_crypto_session->Parameters.SequenceNumber = ctx->uiConfigSequence++;
    hr = ComputeOMAC(
        ctx->key,
        buffer + sizeof(D3D_OMAC),
        sizeof(D3DAUTHENTICATEDCHANNEL_CONFIGURECRYPTOSESSION) - sizeof(D3D_OMAC),
        (BYTE*)buffer);
    if (FAILED(hr))
        return hr;
    hr = ctx->authChannel->Configure(sizeof(D3DAUTHENTICATEDCHANNEL_CONFIGURECRYPTOSESSION), buffer, &output);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling IDirect3DAuthenticatedChannel9::Configure (hr=0x%08x)\n"), hr));
        return hr;
    }

    return hr;
}

void SAuthChannel9_Destroy(SAuthChannel9 *ctx)
{
    if (NULL != ctx)
    {
        PAVPSDK_SAFE_RELEASE(ctx->authChannel);
        SecureZeroMemory(ctx, sizeof(SAuthChannel9));
        PAVPSDK_SAFE_DELETE(ctx);
    }
}
