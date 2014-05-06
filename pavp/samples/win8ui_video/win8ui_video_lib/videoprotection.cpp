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

CVideoProtection_CS11::CVideoProtection_CS11(CCPDeviceD3D11Base *cpDevice)
    :CVideoProtection(), m_d3d11CpDevice(cpDevice)
{
    m_encryptor = NULL;
}

CVideoProtection_CS11::~CVideoProtection_CS11()
{
    PAVPSDK_SAFE_DELETE(m_encryptor);
}

PavpEpidStatus CVideoProtection_CS11::Init(
    const void *encryptorConfig, 
    uint32 encryptorConfigSize)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;
    GUID cryptoType = GUID_NULL;

    m_d3d11CpDevice->GetContentProtectionCaps(&m_contentProtectionCaps);
    m_d3d11CpDevice->GetCryptoSessionPtr()->GetCryptoType(&cryptoType);

    if (16 != m_contentProtectionCaps.BlockAlignmentSize ||
        16 != encryptorConfigSize ||
        D3D11_CRYPTO_TYPE_AES128_CTR != cryptoType)
        return PAVP_STATUS_NOT_SUPPORTED;

    hr = CCPDeviceD3D11_NegotiateStreamKey(
        m_d3d11CpDevice,
        m_encryptionKey, 
        sizeof(m_encryptionKey));
    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptGenRandom (hr=0x%08x)\n"), hr));
        return PAVP_STATUS_UNKNOWN_ERROR;
    }
    
    PAVPSDK_SAFE_DELETE(m_encryptor);

    m_encryptorBlockSize = m_contentProtectionCaps.BlockAlignmentSize;
    m_encryptor = new CStreamEncryptor_AES128_CTR(PAVP_COUNTER_TYPE_C, (const uint8*)encryptorConfig);
    if (NULL == m_encryptor)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    sts = m_encryptor->SetKey(m_encryptionKey);
    if(PAVP_EPID_FAILURE(sts)) 
        return sts;

    m_encryptionKeyBitSize = 8 * m_encryptorBlockSize;

    return sts;
}

PavpEpidStatus CVideoProtection_CS11::EncryptionKeyRefresh()
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    if (D3D11_CONTENT_PROTECTION_CAPS_FRESHEN_SESSION_KEY && m_contentProtectionCaps.Caps)
    {
        uint8 freshness[16];
        m_d3d11CpDevice->GetVideoContextPtr()->
            StartSessionKeyRefresh(m_d3d11CpDevice->GetCryptoSessionPtr(), sizeof(freshness), freshness);
        m_d3d11CpDevice->GetVideoContextPtr()->
            FinishSessionKeyRefresh(m_d3d11CpDevice->GetCryptoSessionPtr());
        for (int i = 0; i < sizeof(freshness); i++)
            m_encryptionKey[i] ^= freshness[i];
        return m_encryptor->SetKey(m_encryptionKey);
    }
    else
        return PAVP_STATUS_NOT_SUPPORTED;
}

PavpEpidStatus CVideoProtection_CS11::Encrypt(
    const uint8* src, 
    uint8* dst, 
    uint32 size, 
    void *encryptorConfig, 
    uint32 encryptorConfigSize)
{
    if (NULL == m_encryptor)
        return PAVP_STATUS_NOT_SUPPORTED;
    if (0 == m_encryptorBlockSize || 
        0 != size%m_encryptorBlockSize || 
        encryptorConfigSize != m_encryptorBlockSize)
        return PAVP_STATUS_LENGTH_ERROR;
    return m_encryptor->Encrypt(src, dst, size, (uint8*)encryptorConfig);
}

PavpEpidStatus CVideoProtection_CS11::Close()
{
    SecureZeroMemory(m_encryptionKey, sizeof(m_encryptionKey));
    m_encryptionKeyBitSize = 0;
    PAVPSDK_SAFE_DELETE(m_encryptor);
    return PAVP_STATUS_SUCCESS;
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602))