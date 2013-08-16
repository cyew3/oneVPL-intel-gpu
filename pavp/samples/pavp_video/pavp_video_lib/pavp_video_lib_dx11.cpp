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
#include <stdio.h>
#include "pavp_video_lib_dx11.h"
#include "pavp_video_lib_internal.h"

#include <d3d9.h>
#include "intel_pavp_gpucp_api.h" // D3DCRYPTOTYPE_INTEL_AES128_CTR



CPAVPVideo_D3D11::CPAVPVideo_D3D11(CCPDeviceD3D11 *cpDevice, CPAVPSession *session)
    :CPAVPVideo(cpDevice)
{
    m_d3d11CpDevice = cpDevice;
    m_session = session;
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
}

PavpEpidStatus CPAVPVideo_D3D11::PreInit(const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;

    // Deduct encryptorMode
    GUID cryptoType = GUID_NULL;
    m_d3d11CpDevice->GetCryptoSessionPtr()->GetCryptoType(&cryptoType);
    m_isEncryptorModeExists = true;
    if (D3D11_CRYPTO_TYPE_AES128_CTR == cryptoType)
    {
        m_encryptorMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
        m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_C;
    }
    else if (D3DCRYPTOTYPE_INTEL_AES128_CTR == cryptoType)
    {
        m_encryptorMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
        m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_A;
    }
    else if (D3DCRYPTOTYPE_INTEL_AES128_CBC == cryptoType)
    {
        m_encryptorMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CBC;
        m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_C;
    }
    else 
    {
        m_isEncryptorModeExists = false;
        sts = PAVP_STATUS_NOT_SUPPORTED;
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Crypto session cryptoType not supported\n")));
        return sts;
    }

    if (NULL != decryptorMode)
    {
        m_isDecryptorModeExists = true;
        m_decryptorMode = *decryptorMode;
    }
    else
        m_isDecryptorModeExists = false;

    return sts;
}


PavpEpidStatus CPAVPVideo_D3D11::Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;

    if (!m_isEncryptorModeExists)
        return PAVP_STATUS_UNDEFINED_BEHAVIOR;

    m_d3d11CpDevice->GetContentProtectionCaps(&m_contentProtectionCaps);

    uint8 streamEncryptionKey[PAVP_EPID_STREAM_KEY_LEN];
    if (m_isDecryptorModeExists)
    {
        if (0 != streamId)
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Transcoding session supports only streamId 0 (sts=0x%08x)\n"), sts));
            return sts;
        }
        sts = CPAVPSession_getVideoTranscodingStreamKeys(m_session, streamEncryptionKey, m_decryptionKey);
    }
    else
    {
        sts = CPAVPSession_getDecodingStreamKey(m_session, PAVP_VIDEO_PATH, streamId, streamEncryptionKey);
    }
    if (PAVP_EPID_FAILURE(sts))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to get stream key/keys (sts=0x%08x)\n"), sts));
        return sts;
    }
    m_streamId = streamId;

    sts = CreateEncryptor(m_encryptorMode, streamEncryptionKey, (const uint8*)encryptorConfig, encryptorConfigSize);
    if (PAVP_EPID_FAILURE(sts))
        return sts;

    return sts;
}


PavpEpidStatus CPAVPVideo_D3D11::EncryptionKeyRefresh()
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    if (D3D11_CONTENT_PROTECTION_CAPS_FRESHEN_SESSION_KEY && m_contentProtectionCaps.Caps)
    {
        uint8 freshness[PAVP_EPID_STREAM_KEY_LEN];
        m_d3d11CpDevice->GetVideoContextPtr()->
            StartSessionKeyRefresh(m_d3d11CpDevice->GetCryptoSessionPtr(), PAVP_EPID_STREAM_KEY_LEN, freshness);
        m_d3d11CpDevice->GetVideoContextPtr()->
            FinishSessionKeyRefresh(m_d3d11CpDevice->GetCryptoSessionPtr());
        return ResetEncryptor(freshness, true);
    }
    else
    {
        uint8 encryptionKey[16];
        sts = CPAVPSession_getDecodingStreamKey(m_session, PAVP_VIDEO_PATH, m_streamId, encryptionKey);
        if (PAVP_EPID_FAILURE(sts))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed to get stream key/keys (sts=0x%08x)\n"), sts));
            return PAVP_STATUS_UNKNOWN_ERROR;
        }
        return ResetEncryptor(encryptionKey, false);
    }
}


PavpEpidStatus CPAVPVideo_D3D11::Close()
{
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
    DestroyEncryptor();
    return CPAVPSession_invalidateDecodingStreamKey(m_session, PAVP_VIDEO_PATH, m_streamId);
}

#endif //(NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602))