/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"
#include "pavp_dx9.h"
#include "intel_pavp_gpucp_api.h"

bool PAVPCryptoSession9_isPossible()
{
    OSVERSIONINFO  versionInfo;
    BOOL           bRet;

    ZeroMemory(&versionInfo, sizeof(OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    bRet = GetVersionEx(&versionInfo);
    if (bRet == FALSE)
        return false;

    // PAVP via GPUCP supported starting from Windows 7 
    return (versionInfo.dwMajorVersion >= 6) &&
        (versionInfo.dwMinorVersion >= 1) &&
        (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
}

GUID PAVPCryptoSession9_findCryptoType(
    IDirect3DDevice9Video *p3DDevice9Video, 
    GUID profile,
    PAVP_ENCRYPTION_TYPE encryptionType,
    GUID keyExchangeType,
    DWORD capsMask,
    D3DCONTENTPROTECTIONCAPS *caps)
{
    PAVPSDK_ASSERT(NULL != p3DDevice9Video);
    if (NULL == p3DDevice9Video)
        return GUID_NULL;

    HRESULT hr = S_OK;
    GUID cryptoType = GUID_NULL;
    D3DCONTENTPROTECTIONCAPS contentProtectionCaps;
    for (;;)
    {
        if (PAVP_ENCRYPTION_AES128_CTR == encryptionType)
        {
            cryptoType = D3DCRYPTOTYPE_AES128_CTR;
            hr = p3DDevice9Video->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &contentProtectionCaps);
            if (!FAILED(hr) && 
                keyExchangeType == contentProtectionCaps.KeyExchangeType &&
                (contentProtectionCaps.Caps & capsMask))
                break;

            cryptoType = D3DCRYPTOTYPE_INTEL_AES128_CTR;
            hr = p3DDevice9Video->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &contentProtectionCaps);
            if (!FAILED(hr) && 
                keyExchangeType == contentProtectionCaps.KeyExchangeType &&
                (contentProtectionCaps.Caps & capsMask))
                break;
        }
        else if (PAVP_ENCRYPTION_AES128_CBC == encryptionType)
        {
            cryptoType = D3DCRYPTOTYPE_INTEL_AES128_CBC;
            hr = p3DDevice9Video->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &contentProtectionCaps);
            if (!FAILED(hr) && 
                keyExchangeType == contentProtectionCaps.KeyExchangeType &&
                (contentProtectionCaps.Caps & capsMask))
            {
                break;
            }
        }
        else if (PAVP_ENCRYPTION_DECE_AES128_CTR == encryptionType)
        {
            cryptoType = D3DCRYPTOTYPE_INTEL_DECE_AES128_CTR;
            hr = p3DDevice9Video->GetContentProtectionCaps(
                &cryptoType,
                &profile,
                &contentProtectionCaps);
            if (!FAILED(hr) && 
                keyExchangeType == contentProtectionCaps.KeyExchangeType &&
                (contentProtectionCaps.Caps & capsMask))
                break;
        }

        cryptoType = GUID_NULL;
        break;
    }
    if (GUID_NULL != cryptoType && NULL != caps)
        *caps = contentProtectionCaps;

    return cryptoType;
}
CCPDeviceCryptoSession9::CCPDeviceCryptoSession9(
    IDirect3DCryptoSession9* cs9, 
    HANDLE cs9Handle,
    GUID PAVPKeyExchangeGUID,
    GUID cryptoType,
    GUID profile,
    const D3DCONTENTPROTECTIONCAPS *caps): 
        m_cs9(cs9), 
        m_cs9Handle(cs9Handle), 
        m_PAVPKeyExchangeGUID(PAVPKeyExchangeGUID),
        m_cryptoType(cryptoType),
        m_profile(profile)
{
    memcpy(&m_caps, caps, sizeof (m_caps));
    if (NULL != m_cs9)
        m_cs9->AddRef();
}

CCPDeviceCryptoSession9::~CCPDeviceCryptoSession9()
{
    PAVPSDK_SAFE_RELEASE(m_cs9);
}


void CCPDeviceCryptoSession9::GetContentProtectionCaps(D3DCONTENTPROTECTIONCAPS *caps)
{
    memcpy(caps, &m_caps, sizeof (*caps));
}


PavpEpidStatus CCPDeviceCryptoSession9::ExecuteKeyExchange(
    const void *pDataIn,
    uint32 uiSizeIn,
    void *pOutputData,
    uint32 uiSizeOut)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT hr = S_OK;
    GPUCP_CRYPTOSESSION_PAVP_KEYEXCHANGE    KeyExchangeData = {0};
    PAVP_EPID_EXCHANGE_PARAMS               KeyExchangeParams = {0};
    
    if (NULL == m_cs9)
        return PAVP_STATUS_BAD_POINTER;

    KeyExchangeData.PAVPKeyExchangeGUID = m_PAVPKeyExchangeGUID; 
    KeyExchangeData.DataSize = sizeof(PAVP_EPID_EXCHANGE_PARAMS);
    KeyExchangeData.pKeyExchangeParams = (void*)&KeyExchangeParams;

    KeyExchangeParams.pInput = (BYTE*)pDataIn;
    KeyExchangeParams.ulInputSize = uiSizeIn;
    KeyExchangeParams.pOutput = (BYTE*)pOutputData;
    KeyExchangeParams.ulOutputSize = uiSizeOut;

    hr = m_cs9->NegotiateKeyExchange(sizeof(KeyExchangeData), (BYTE*)&KeyExchangeData);
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("IDirect3DCryptoSession9::NegotiateKeyExchange failed for PAVP key exchange (hr=0x%08x).\n"), hr));
        return PAVP_STATUS_UNKNOWN_ERROR;
    }

    return sts;
}



PavpEpidStatus CCPDeviceCryptoSession9::ExecuteFunc(
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
    
    if (NULL == m_cs9)
        return PAVP_STATUS_BAD_POINTER;

    KeyExchangeData.PAVPKeyExchangeGUID = GUID_INTEL_PROPRIETARY_FUNCTION; 
    KeyExchangeData.DataSize = sizeof(GPUCP_PROPRIETARY_FUNCTION_PARAMS);
    KeyExchangeData.pKeyExchangeParams = (void*)&PropFunctionParams;

    PropFunctionParams.uiPavpFunctionId = uiFuncID;
    PropFunctionParams.pInput = (void*)pDataIn;
    PropFunctionParams.uiInputSize = uiSizeIn;
    PropFunctionParams.pOutput = (void*)pOutputData;
    PropFunctionParams.uiOutputSize = uiSizeOut;

    hr = m_cs9->NegotiateKeyExchange(sizeof(KeyExchangeData), (BYTE*)&KeyExchangeData); 
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("IDirect3DCryptoSession9::NegotiateKeyExchange failed for GUID_INTEL_PROPRIETARY_FUNCTION %d (hr=0x%08x).\n"), uiFuncID, hr));
        return PAVP_STATUS_UNKNOWN_ERROR;
    }

    return sts;
}
