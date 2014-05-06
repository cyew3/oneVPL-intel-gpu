/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/

#include "pavpsdk_defs.h"
extern const int __PAVP_DX11__=1;//make library not empty
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8

#include "pavp_dx11.h"

#include <d3d9.h> // todo: remove when intel_pavp_gpucp_api.h have all dx9 independent code
#include "intel_pavp_gpucp_api.h"

bool _IsKeyExhangeSupported(
    const GUID *keyExchangeType,
    ID3D11VideoDevice *videoDevice, 
    uint32 keyExchangeTypeCount,
    const GUID *cryptoType,
    const GUID *profile)
{
    PAVPSDK_ASSERT(NULL != keyExchangeType);
    PAVPSDK_ASSERT(NULL != videoDevice);
    PAVPSDK_ASSERT(NULL != cryptoType);
    PAVPSDK_ASSERT(NULL != profile);

    uint32 i = 0;
    for(i = 0; i < keyExchangeTypeCount; i++)
    {
        GUID keyExchangeIt = GUID_NULL;
        videoDevice->CheckCryptoKeyExchange(cryptoType, profile, i, &keyExchangeIt); 
        if (*keyExchangeType == keyExchangeIt)
            break;
    }
    if (i < keyExchangeTypeCount)
        return true;
    return false;
}

GUID PAVPD3D11_findCryptoType(
    ID3D11VideoDevice *videoDevice, 
    GUID profile, 
    PAVP_ENCRYPTION_TYPE encryptionType,
    GUID keyExchangeType,
    DWORD capsMask,
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps)
{
    PAVPSDK_ASSERT(NULL != videoDevice);
    if (NULL == videoDevice)
        return GUID_NULL;

    HRESULT hr = S_OK;
    GUID cryptoType = GUID_NULL;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS _caps;
    for (;;)
    {
        if (PAVP_ENCRYPTION_AES128_CTR == encryptionType)
        {
            cryptoType = D3D11_CRYPTO_TYPE_AES128_CTR;
            hr = videoDevice->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &_caps);
            if (!FAILED(hr) &&
                _IsKeyExhangeSupported(&keyExchangeType, videoDevice, _caps.KeyExchangeTypeCount, &cryptoType, &profile) &&
                (_caps.Caps & capsMask))
                break;

            cryptoType = D3DCRYPTOTYPE_INTEL_AES128_CTR;
            hr = videoDevice->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &_caps);
            if (!FAILED(hr) && 
                _IsKeyExhangeSupported(&keyExchangeType, videoDevice, _caps.KeyExchangeTypeCount, &cryptoType, &profile) &&
                (_caps.Caps & capsMask))
                break;
        }
        else if (PAVP_ENCRYPTION_DECE_AES128_CTR == encryptionType)
        {
            cryptoType = D3DCRYPTOTYPE_INTEL_DECE_AES128_CTR;
            hr = videoDevice->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &_caps);
            if (!FAILED(hr) && 
                _IsKeyExhangeSupported(&keyExchangeType, videoDevice, _caps.KeyExchangeTypeCount, &cryptoType, &profile) &&
                (_caps.Caps & capsMask))
                break;
        }

        cryptoType = GUID_NULL;
        break;
    }
    if (GUID_NULL != cryptoType && NULL != caps)
        *caps = _caps;

    return cryptoType;
}



CCPDeviceD3D11Base::CCPDeviceD3D11Base(
    ID3D11CryptoSession* cs, 
    ID3D11VideoContext* videoContext, 
    GUID keyExchangeGUID,
    const D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps): 
        m_cs(cs), 
        m_videoContext(videoContext), 
        m_keyExchangeGUID(keyExchangeGUID)
{
    memcpy(&m_caps, caps, sizeof (m_caps));
    if (NULL != m_cs)
        m_cs->AddRef();
    if (NULL != m_videoContext)
        m_videoContext->AddRef();
}

CCPDeviceD3D11Base::~CCPDeviceD3D11Base()
{
    PAVPSDK_SAFE_RELEASE(m_cs);
    PAVPSDK_SAFE_RELEASE(m_videoContext);
}

void CCPDeviceD3D11Base::GetContentProtectionCaps(D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps)
{
    memcpy(caps, &m_caps, sizeof (*caps));
}


PavpEpidStatus CCPDeviceD3D11Base::ExecuteKeyExchange(
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT hr = S_OK;

    if (NULL == m_videoContext)
        return PAVP_STATUS_BAD_POINTER;
    if (NULL != pOutputData || 0 != uiSizeOut)
        return PAVP_STATUS_NOT_SUPPORTED;

    hr = m_videoContext->NegotiateCryptoSessionKeyExchange(m_cs, uiSizeIn, (void*)pDataIn); 
    if (FAILED(hr))
        return PAVP_STATUS_UNKNOWN_ERROR;

    return sts;
}

PavpEpidStatus CCPDeviceD3D11Base::ExecuteFunc(
    uint32 uiFuncID,
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    return PAVP_STATUS_NOT_SUPPORTED;
}



CCPDeviceD3D11::CCPDeviceD3D11(
    ID3D11CryptoSession* cs, 
    ID3D11VideoContext* videoContext, 
    GUID PAVPKeyExchangeGUID,
    const D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps)
        :CCPDeviceD3D11Base(cs, videoContext, PAVPKeyExchangeGUID, caps)
{
}


PavpEpidStatus CCPDeviceD3D11::ExecuteKeyExchange(
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT hr = S_OK;
    GPUCP_CRYPTOSESSION_PAVP_KEYEXCHANGE    KeyExchangeData = {0};
    PAVP_EPID_EXCHANGE_PARAMS               KeyExchangeParams = {0};
    
    if (NULL == m_videoContext)
        return PAVP_STATUS_BAD_POINTER;

    KeyExchangeData.PAVPKeyExchangeGUID = m_keyExchangeGUID; 
    KeyExchangeData.DataSize = sizeof(PAVP_EPID_EXCHANGE_PARAMS);
    KeyExchangeData.pKeyExchangeParams = (void*)&KeyExchangeParams;

    KeyExchangeParams.pInput = (BYTE*)pDataIn;
    KeyExchangeParams.ulInputSize = uiSizeIn;
    KeyExchangeParams.pOutput = (BYTE*)pOutputData;
    KeyExchangeParams.ulOutputSize = uiSizeOut;

    hr = m_videoContext->NegotiateCryptoSessionKeyExchange(m_cs, sizeof(KeyExchangeData), (BYTE*)&KeyExchangeData); 
    if (FAILED(hr))
        return PAVP_STATUS_UNKNOWN_ERROR;

    return sts;
}

PavpEpidStatus CCPDeviceD3D11::ExecuteFunc(
    uint32 uiFuncID,
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT hr = S_OK;
    GPUCP_CRYPTOSESSION_PAVP_KEYEXCHANGE    KeyExchangeData = {0};
    GPUCP_PROPRIETARY_FUNCTION_PARAMS       PropFunctionParams = {0};
    
    if (NULL == m_videoContext)
        return PAVP_STATUS_BAD_POINTER;

    KeyExchangeData.PAVPKeyExchangeGUID = GUID_INTEL_PROPRIETARY_FUNCTION; 
    KeyExchangeData.DataSize = sizeof(GPUCP_PROPRIETARY_FUNCTION_PARAMS);
    KeyExchangeData.pKeyExchangeParams = (void*)&PropFunctionParams;

    PropFunctionParams.uiPavpFunctionId = uiFuncID;
    PropFunctionParams.pInput = (void*)pDataIn;
    PropFunctionParams.uiInputSize = uiSizeIn;
    PropFunctionParams.pOutput = (void*)pOutputData;
    PropFunctionParams.uiOutputSize = uiSizeOut;

    hr = m_videoContext->NegotiateCryptoSessionKeyExchange(m_cs, sizeof(KeyExchangeData), (BYTE*)&KeyExchangeData); 
    if (FAILED(hr))
        return PAVP_STATUS_UNKNOWN_ERROR;

    return sts;
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8