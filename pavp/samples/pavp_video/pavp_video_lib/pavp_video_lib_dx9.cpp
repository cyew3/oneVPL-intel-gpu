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
#include <stdio.h>
#include "pavp_video_lib_dx9.h"
#include "intel_pavp_gpucp_api.h" // D3DCRYPTOTYPE_INTEL_AES128_CTR
#include "pavp_video_lib_internal.h"


CPAVPVideo_Auxiliary9::CPAVPVideo_Auxiliary9(
    CCPDeviceAuxiliary9 *cpDevice, 
    CPAVPSession *session)
        : CPAVPVideo(cpDevice)
{
    m_aux = cpDevice;
    m_session = session;
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
}

PavpEpidStatus CPAVPVideo_Auxiliary9::PreInit(
    const PAVP_ENCRYPTION_MODE *encryptorMode,
    const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;
    
    PAVP_QUERY_CAPS2 caps;

    // Verify HW support
#if 1
    // TODO: remove that workaround: Get caps fails if auxiliary interface already intialized PAVPAuxiliary9_initialize
    {
    IDirect3D9Ex        *pd3d9 = NULL;
    IDirect3DDevice9Ex    *pd3d9Device = NULL;
    IDirectXVideoDecoder*pAuxDevice = NULL;
    for(int i=0 ; i < 1; i++)
    {
        hr = CreateD3D9AndDeviceEx( &pd3d9, &pd3d9Device );
        if (FAILED(hr))
            break;
        hr = PAVPAuxiliary9_create(pd3d9Device, &pAuxDevice);
        if (FAILED(hr))
            break;
        hr = PAVPAuxiliary9_getPavpCaps2(pAuxDevice, &caps);
        if (FAILED(hr))
            break;
    }
    PAVPSDK_SAFE_RELEASE(pAuxDevice);
    PAVPSDK_SAFE_RELEASE(pd3d9Device);
    PAVPSDK_SAFE_RELEASE(pd3d9);
    }
#else
    hr = PAVPAuxiliary9_getPavpCaps2(m_aux->GetAuxPtr(), &caps);
#endif
    if (SUCCEEDED(hr))
    {
        m_encryptorMode.eEncryptionType = (NULL == encryptorMode) ?
            PAVP_ENCRYPTION_AES128_CTR : encryptorMode->eEncryptionType;

        if (PAVP_ENCRYPTION_AES128_CTR == m_encryptorMode.eEncryptionType)
        {
            UINT availableCounterTypes = caps.AvailableCounterTypes;
            if (NULL != encryptorMode)
                availableCounterTypes &= encryptorMode->eCounterMode;
            // find best avaliable counter type. Listed in preference order
            if (PAVP_COUNTER_TYPE_C & availableCounterTypes)
                m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_C;
            else if (PAVP_COUNTER_TYPE_B & availableCounterTypes)
                m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_B;
            else if (PAVP_COUNTER_TYPE_A & availableCounterTypes)
                m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_A;
            else
            {
                sts = PAVP_STATUS_NOT_SUPPORTED;
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("caps.AvailableCounterTypes is not supported (sts=0x%08x)\n"), sts));
                return sts;
            }
        }
        else
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("encryptorType requested is not supported (sts=0x%08x)\n"), sts));
            return sts;
        }

        if (NULL != decryptorMode &&
            0 == (PAVP_SESSION_TYPE_TRANSCODE & caps.AvailableSessionTypes))
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Transcoding session is not supported, decryptor can't be used\n"), sts));
            return sts;
        }
        
        if (NULL != decryptorMode)
        {
            m_isDecryptorModeExists = true;
            m_decryptorMode = *decryptorMode;
        }
        else
            m_isDecryptorModeExists = false;

    }
    else
    {
        // Old HW
        if (NULL != encryptorMode && 
            (PAVP_ENCRYPTION_AES128_CTR != encryptorMode->eEncryptionType || 
            PAVP_COUNTER_TYPE_A != encryptorMode->eCounterMode))
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("encryptorType requested is not supported (sts=0x%08x)\n"), sts));
            return sts;
        }

        m_encryptorMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
        m_encryptorMode.eCounterMode = PAVP_COUNTER_TYPE_A;

        if (NULL != decryptorMode)
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Transcoding session is not supported, decryptor can't be used\n"), sts));
            return sts;
        }
        m_isDecryptorModeExists = false;
    }

    m_isEncryptorModeExists = true;

    return sts;
}

PavpEpidStatus CPAVPVideo_Auxiliary9::Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;

    if (!m_isEncryptorModeExists)
        return PAVP_STATUS_UNDEFINED_BEHAVIOR;

    if (PAVP_SESSION_TYPE_TRANSCODE == m_aux->GetSessionType() && 
        !m_isDecryptorModeExists)
        return PAVP_STATUS_UNDEFINED_BEHAVIOR;

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


PavpEpidStatus CPAVPVideo_Auxiliary9::Close()
{
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
    DestroyEncryptor();
    return CPAVPSession_invalidateDecodingStreamKey(m_session, PAVP_VIDEO_PATH, m_streamId);
}

/*PavpEpidStatus CPAVPVideo_Auxiliary9::GetEncryptorCounterType(PAVP_COUNTER_TYPE *ctrType)
{
    if (NULL == m_encryptor)
        return PAVP_STATUS_NOT_SUPPORTED;
    *ctrType = m_ctrType;
    return PAVP_STATUS_SUCCESS;
}*/

PavpEpidStatus CCPDevice_SetWindowPosition(CCPDevice *cpDevice, HWND hwnd)
{
    HRESULT hr;
    HMONITOR hm;
    MONITORINFO im;
    PAVP_SET_WINDOW_POSITION_PARAMS wpp;

    if (!GetWindowRect(hwnd, &wpp.WindowPosition))
        return PAVP_STATUS_UNKNOWN_ERROR;
    if (!GetClientRect(hwnd, &wpp.VisibleContent))
        return PAVP_STATUS_UNKNOWN_ERROR;

    im.cbSize=sizeof(MONITORINFO);
    hm = MonitorFromRect(&wpp.WindowPosition,MONITOR_DEFAULTTONULL);
    GetMonitorInfo(hm, &im);

    if(wpp.WindowPosition.bottom >= im.rcWork.bottom)
    {
        wpp.VisibleContent.bottom = im.rcWork.bottom - wpp.WindowPosition.top;
        wpp.WindowPosition.bottom = im.rcWork.bottom;
    }
    if(wpp.WindowPosition.top <= im.rcWork.top)
    {
        wpp.VisibleContent.top = im.rcWork.top - wpp.WindowPosition.top;
        wpp.WindowPosition.top = im.rcWork.top;
    }
    if(wpp.WindowPosition.left <= im.rcWork.left)
    {
        wpp.VisibleContent.left = im.rcWork.left - wpp.WindowPosition.left;
        wpp.WindowPosition.left = im.rcWork.left;
    }
    if(wpp.WindowPosition.right  >= im.rcWork.right)
    {
        wpp.VisibleContent.right = im.rcWork.right - wpp.WindowPosition.left;
        wpp.WindowPosition.right = im.rcWork.right-1;
    }

    wpp.hdcMonitorId = GetDC(hwnd);

    // notify the driver of the window position
    hr = cpDevice->ExecuteFunc(PAVP_SET_WINDOW_POSITION, 
                (void *)&wpp, sizeof(wpp), 
                NULL, 0 );
    if( FAILED(hr) )
        return PAVP_STATUS_SET_WINDOW_POSITION_FAILED;

    return PAVP_STATUS_SUCCESS;
}


CPAVPVideo_CryptoSession9::CPAVPVideo_CryptoSession9(CCPDeviceCryptoSession9 *cpDevice, CPAVPSession *session)
    :CPAVPVideo(cpDevice)
{
    m_cs9Device = cpDevice;
    m_session = session;
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
}

PavpEpidStatus CPAVPVideo_CryptoSession9::PreInit(const PAVP_ENCRYPTION_MODE *decryptorMode)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;

    // Deduct encryptorMode
    GUID cryptoType = m_cs9Device->GetCryptoType();
    m_isEncryptorModeExists = true;
    if (D3DCRYPTOTYPE_AES128_CTR == cryptoType)
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
    else if (D3DCRYPTOTYPE_INTEL_DECE_AES128_CTR == cryptoType)
    {
        m_encryptorMode.eEncryptionType = PAVP_ENCRYPTION_DECE_AES128_CTR;
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


PavpEpidStatus CPAVPVideo_CryptoSession9::Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize)
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;
    HRESULT    hr = S_OK;

    if (!m_isEncryptorModeExists)
        return PAVP_STATUS_UNDEFINED_BEHAVIOR;

    m_cs9Device->GetContentProtectionCaps(&m_contentProtectionCaps);

    uint8 streamEncryptionKey[PAVP_EPID_STREAM_KEY_LEN];
    if (m_isDecryptorModeExists)
    {
        if (0 != streamId)
        {
            sts = PAVP_STATUS_NOT_SUPPORTED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Transcoding session supports only streamId 0 (sts=0x%08x)\n"), sts));
            return sts;
        }
        sts = getVideoTranscodingStreamKeys(streamEncryptionKey, m_decryptionKey);
    }
    else
    {
        sts = getDecodingStreamKey(streamId, streamEncryptionKey);
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


PavpEpidStatus CPAVPVideo_CryptoSession9::EncryptionKeyRefresh()
{
    HRESULT hr = S_OK;
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;

    if (D3DCPCAPS_FRESHENSESSIONKEY && m_contentProtectionCaps.Caps)
    {
        uint8 freshness[PAVP_EPID_STREAM_KEY_LEN];
        hr = m_cs9Device->GetCryptoSessionPtr()->StartSessionKeyRefresh(freshness, PAVP_EPID_STREAM_KEY_LEN);
        if (FAILED(hr))
            return PAVP_STATUS_UNKNOWN_ERROR;
        hr = m_cs9Device->GetCryptoSessionPtr()->FinishSessionKeyRefresh();
        if (FAILED(hr))
            return PAVP_STATUS_UNKNOWN_ERROR;
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


PavpEpidStatus CPAVPVideo_CryptoSession9::Close()
{
    m_isDecryptorModeExists = false;
    m_isEncryptorModeExists = false;
    DestroyEncryptor();
    return CPAVPSession_invalidateDecodingStreamKey(m_session, PAVP_VIDEO_PATH, m_streamId);
}

PavpEpidStatus CPAVPVideo_CryptoSession9::getVideoTranscodingStreamKeys(
    uint8 *decodeStreamKey, 
    uint8 *encodeStreamKey)
{
    return CPAVPSession_getVideoTranscodingStreamKeys(m_session, decodeStreamKey, encodeStreamKey);
}

PavpEpidStatus CPAVPVideo_CryptoSession9::getDecodingStreamKey(
    uint32 streamId, 
    uint8 *streamKey)
{
    return CPAVPSession_getDecodingStreamKey(m_session, PAVP_VIDEO_PATH, streamId, streamKey);
}
