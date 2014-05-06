/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "ippcp.h"
#include "pavp_video_lib.h"
#include "pavp_sample_common.h"
#include "pavp_video_lib_internal.h"
#include "intel_pavp_core_api.h"


PavpEpidStatus CPAVPSession_invalidateDecodingStreamKey(
    CPAVPSession *session, 
    PAVPPathId pathId, 
    uint32 streamId)
{
    PavpEpidStatus PavpStatus = PAVP_STATUS_SUCCESS;

    InvStreamKeyInBuff    sInvKeyIn = {0};
    InvStreamKeyOutBuff   sInvKeyOut = {0};
    
    if (NULL == session)
        return PAVP_STATUS_BAD_POINTER;

    do
    {
        sInvKeyIn.Header.ApiVersion     = session->GetActualAPIVersion();
        sInvKeyIn.Header.BufferLength   = sizeof(InvStreamKeyInBuff) - sizeof(PAVPCmdHdr);
        sInvKeyIn.Header.CommandId      = CMD_INV_STREAM_KEY;
        sInvKeyIn.Header.PavpCmdStatus  = PAVP_STATUS_SUCCESS;
        sInvKeyIn.SigmaSessionId        = session->GetSessionId();
        sInvKeyIn.StreamId              = streamId;
        sInvKeyIn.MediaPathId           = pathId;

        PavpStatus = session->GetCPDevice()->ExecuteKeyExchange(
             &sInvKeyIn, sizeof(sInvKeyIn), 
             &sInvKeyOut, sizeof(sInvKeyOut));
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;
        if(PAVP_EPID_FAILURE(sInvKeyOut.Header.PavpCmdStatus) )
        {
            PavpStatus = (PavpEpidStatus)sInvKeyOut.Header.PavpCmdStatus;
            break;
        }

    } while( 0 );

    return PavpStatus;
}



PavpEpidStatus CPAVPSession_getDecodingStreamKey(
    CPAVPSession *session, 
    PAVPPathId pathId, 
    uint32 streamId, 
    uint8 *streamKey)
{
    PavpEpidStatus  PavpStatus = PAVP_STATUS_SUCCESS;

    GetStreamKeyInBuff      sGetStreamKeyIn  = {0};
    GetStreamKeyOutBuff     sGetStreamKeyOut = {0};

    if (NULL == session || NULL == streamKey)
        return PAVP_STATUS_BAD_POINTER;

    do
    {
        sGetStreamKeyIn.Header.ApiVersion       = session->GetActualAPIVersion();
        sGetStreamKeyIn.Header.BufferLength     = sizeof(GetStreamKeyInBuff) - sizeof(PAVPCmdHdr);
        sGetStreamKeyIn.Header.CommandId        = CMD_GET_STREAM_KEY;
        sGetStreamKeyIn.Header.PavpCmdStatus    = PAVP_STATUS_SUCCESS;
        sGetStreamKeyIn.SigmaSessionId          = session->GetSessionId();
        sGetStreamKeyIn.StreamId                = streamId;
        sGetStreamKeyIn.MediaPathId             = pathId;

        PavpStatus = session->GetCPDevice()->ExecuteKeyExchange(
             &sGetStreamKeyIn,sizeof(sGetStreamKeyIn), 
             &sGetStreamKeyOut, sizeof(sGetStreamKeyOut));
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;
        if(PAVP_EPID_FAILURE(sGetStreamKeyOut.Header.PavpCmdStatus) )
        {
            PavpStatus = (PavpEpidStatus)sGetStreamKeyOut.Header.PavpCmdStatus;
            break;
        }
        
        // Verify the HMAC of the stream key(s) using Mk
        HMAC hmacTmp;
        PavpStatus = session->Sign(
            (unsigned char*)&(sGetStreamKeyOut.WrappedStreamKey), 
            PAVP_EPID_STREAM_KEY_LEN, 
            hmacTmp);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;
        if(0 != memcmp(hmacTmp, sGetStreamKeyOut.WrappedStreamKeyHmac, sizeof(hmacTmp)))
        {
            PavpStatus = PAVP_STATUS_PCH_HMAC_INVALID;
            break;
        }
        
        // Decrypt Stream Key
        PavpStatus = session->Decrypt((unsigned char*)&(sGetStreamKeyOut.WrappedStreamKey), streamKey, PAVP_EPID_STREAM_KEY_LEN);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;

    } while( 0 );

    return PavpStatus;
}

PavpEpidStatus CPAVPSession_getVideoTranscodingStreamKeys(
    CPAVPSession *session, 
    uint8 *decodeStreamKey, 
    uint8 *encodeStreamKey)
{
    PavpEpidStatus  PavpStatus = PAVP_STATUS_SUCCESS;

    GetStreamKeyExInBuff    sGetStreamKeyExIn  = {0};
    GetStreamKeyExOutBuff   sGetStreamKeyExOut = {0};

    if (NULL == session || NULL == decodeStreamKey || NULL == encodeStreamKey)
        return PAVP_STATUS_BAD_POINTER;

    do
    {
        sGetStreamKeyExIn.Header.ApiVersion       = session->GetActualAPIVersion();
        sGetStreamKeyExIn.Header.BufferLength     = sizeof(GetStreamKeyExInBuff) - sizeof(PAVPCmdHdr);
        sGetStreamKeyExIn.Header.CommandId        = CMD_GET_STREAM_KEY_EX;
        sGetStreamKeyExIn.Header.PavpCmdStatus    = PAVP_STATUS_SUCCESS;
        sGetStreamKeyExIn.SigmaSessionId          = session->GetSessionId();
        
        PavpStatus = session->GetCPDevice()->ExecuteKeyExchange(
             &sGetStreamKeyExIn,sizeof(sGetStreamKeyExIn), 
             &sGetStreamKeyExOut, sizeof(sGetStreamKeyExOut));
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;

        if(PAVP_EPID_FAILURE(sGetStreamKeyExOut.Header.PavpCmdStatus) )
        {
            PavpStatus = (PavpEpidStatus)sGetStreamKeyExOut.Header.PavpCmdStatus;
            break;
        }
        
        // Verify the HMAC of the stream key(s) using Mk
        HMAC hmacTmp;
        PavpStatus = session->Sign(
            (unsigned char*)&(sGetStreamKeyExOut.WrappedStreamDKey), 
            2 * PAVP_EPID_STREAM_KEY_LEN, 
            hmacTmp);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;
        if(0 != memcmp(hmacTmp, sGetStreamKeyExOut.WrappedStreamKeyHmac, sizeof(hmacTmp)))
        {
            PavpStatus = PAVP_STATUS_PCH_HMAC_INVALID;
            break;
        }
        
        // Decrypt Stream Keys
        PavpStatus = session->Decrypt((unsigned char*)&(sGetStreamKeyExOut.WrappedStreamDKey), decodeStreamKey, PAVP_EPID_STREAM_KEY_LEN);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;

        PavpStatus = session->Decrypt((unsigned char*)&(sGetStreamKeyExOut.WrappedStreamEKey), encodeStreamKey, PAVP_EPID_STREAM_KEY_LEN);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;
    } while( 0 );

    return PavpStatus;
}

CStreamEncryptor_AES128_CTR::CStreamEncryptor_AES128_CTR(
    PAVP_COUNTER_TYPE counterType,
    const uint8 encryptorConfig[16])
{
    m_rij = NULL;
    m_counterBitSize = PAVP_COUNTER_TYPE_A == counterType ? 32 : 64;
    memcpy(m_iv_ctr, encryptorConfig, 16);
    m_initialCounter = m_iv_ctr[1] & (((uint64)0xffffffffffffffff)<<(64-m_counterBitSize));
}

CStreamEncryptor_AES128_CTR::~CStreamEncryptor_AES128_CTR()
{
    PAVPSDK_SAFE_DELETE_ARRAY(m_rij);
}

PavpEpidStatus CStreamEncryptor_AES128_CTR::SetKey(const uint8* key)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == key)
        return PAVP_STATUS_BAD_POINTER; 

    if (NULL == m_rij)
    {
        int rijSize = 0;
        ippsRijndael128GetSize(&rijSize);

        m_rij = new uint8[rijSize];
        if(0 == rijSize || NULL == m_rij)
            return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;
    }
    sts = ippsRijndael128Init(key, IppsRijndaelKey128, (IppsRijndael128Spec*)m_rij);
    if (ippStsNoErr != sts)
        return PAVP_STATUS_UNKNOWN_ERROR;

    m_initialCounter = m_iv_ctr[1] & (((uint64)0xffffffffffffffff)<<(64-m_counterBitSize));

    return PAVP_STATUS_SUCCESS;
}


// LE<->BE translation of DWORD
#define SwapEndian_DW(dw)    ( (((dw) & 0x000000ff) << 24) |  (((dw) & 0x0000ff00) << 8) | (((dw) & 0x00ff0000) >> 8) | (((dw) & 0xff000000) >> 24) )
// LE<->BE translation of 8 byte big number
#define SwapEndian_8B(ptr)                                                    \
{                                                                            \
    unsigned int Temp = 0;                                                    \
    Temp = SwapEndian_DW(((unsigned int*)(ptr))[0]);                        \
    ((unsigned int*)(ptr))[0] = SwapEndian_DW(((unsigned int*)(ptr))[1]);    \
    ((unsigned int*)(ptr))[1] = Temp;                                        \
}                                                                            \


PavpEpidStatus CStreamEncryptor_AES128_CTR::Encrypt(
    const uint8* src, 
    uint8* dst, 
    uint32 size, 
    uint8* encryptorConfig)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == src || NULL == dst)
        return PAVP_STATUS_BAD_POINTER; 
    if (NULL == m_rij)
        return PAVP_STATUS_REFRESH_REQUIRED_ERROR;
    if (0 != size%16)
        return PAVP_STATUS_LENGTH_ERROR;
    
    uint64 ctrIncrement = size/16;
    uint64 counter = m_iv_ctr[1] & (((uint64)0xffffffffffffffff)<<(64-m_counterBitSize));
    SwapEndian_8B(&counter);
    if (counter < m_initialCounter)
    {

        if (m_initialCounter - counter <= ctrIncrement)
            return PAVP_STATUS_REFRESH_REQUIRED_ERROR;
    }
    else
    {
        if ((((uint64)0xffffffffffffffff)<<(64-m_counterBitSize)) - counter + m_initialCounter <= ctrIncrement)
            return PAVP_STATUS_REFRESH_REQUIRED_ERROR;
    }

    // return counter used
    memcpy(encryptorConfig, m_iv_ctr, 16);

    sts = ippsRijndael128EncryptCTR(src, dst, size, (IppsRijndael128Spec*)m_rij, (Ipp8u*)m_iv_ctr, m_counterBitSize);
    if (ippStsNoErr != sts)
        return PAVP_STATUS_UNKNOWN_ERROR;

    return PAVP_STATUS_SUCCESS;
}



CStreamEncryptor_AES128_CBC::CStreamEncryptor_AES128_CBC(const uint8 encryptorConfig[16])
{
    m_rij = NULL;
    memcpy(m_iv, encryptorConfig, 16);
}

CStreamEncryptor_AES128_CBC::~CStreamEncryptor_AES128_CBC()
{
    PAVPSDK_SAFE_DELETE_ARRAY(m_rij);
}

PavpEpidStatus CStreamEncryptor_AES128_CBC::SetKey(const uint8* key)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == key)
        return PAVP_STATUS_BAD_POINTER; 

    if (NULL == m_rij)
    {
        int rijSize = 0;
        ippsRijndael128GetSize(&rijSize);

        m_rij = new uint8[rijSize];
        if(0 == rijSize || NULL == m_rij)
            return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;
    }
    sts = ippsRijndael128Init(key, IppsRijndaelKey128, (IppsRijndael128Spec*)m_rij);
    if (ippStsNoErr != sts)
        return PAVP_STATUS_UNKNOWN_ERROR;

    return PAVP_STATUS_SUCCESS;
}

PavpEpidStatus CStreamEncryptor_AES128_CBC::Encrypt(
    const uint8* src, 
    uint8* dst, 
    uint32 size, 
    uint8* encryptorConfig)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == src || NULL == dst)
        return PAVP_STATUS_BAD_POINTER; 
    if (NULL == m_rij)
        return PAVP_STATUS_REFRESH_REQUIRED_ERROR;
    if (0 != size%16)
        return PAVP_STATUS_LENGTH_ERROR;

    sts = ippsRijndael128EncryptCBC(src, dst, size, (IppsRijndael128Spec*)m_rij, (Ipp8u*)m_iv, IppsCPPaddingNONE);
    if (ippStsNoErr != sts)
        return PAVP_STATUS_UNKNOWN_ERROR;
    memcpy(encryptorConfig, m_iv, 16);
    if (size > 16)
        memcpy(m_iv, (uint8*)dst+(size/16)-1, 16);

    return PAVP_STATUS_SUCCESS;
}


CPAVPVideo::CPAVPVideo(CCPDevice* cpDevice)
{
    m_cpDevice = cpDevice;
    m_encryptor = NULL;
    m_encryptorBlockSize = 0;

    memset(m_decryptionKey, 0, 16);
    m_decryptionKeyBitSize = 0;
}

CPAVPVideo::~CPAVPVideo()
{
    DestroyEncryptor();
}

PavpEpidStatus CPAVPVideo::CreateEncryptor(
    PAVP_ENCRYPTION_MODE mode,
    const uint8 *key,
    const uint8 *config, 
    uint32 configSize)
{
    if (NULL == key)
        return PAVP_STATUS_BAD_POINTER; 

    PAVPSDK_SAFE_DELETE(m_encryptor);
    if (PAVP_ENCRYPTION_AES128_CTR == mode.eEncryptionType ||
        PAVP_ENCRYPTION_DECE_AES128_CTR == mode.eEncryptionType)
    {
        m_encryptorBlockSize = 16;
        if (m_encryptorBlockSize != configSize)
            return PAVP_STATUS_NOT_SUPPORTED;
        m_encryptor = new CStreamEncryptor_AES128_CTR(mode.eCounterMode, config);
    }
    else if (PAVP_ENCRYPTION_AES128_CBC == mode.eEncryptionType)
    {
        m_encryptorBlockSize = 16;
        if (m_encryptorBlockSize != configSize)
            return PAVP_STATUS_NOT_SUPPORTED;
        m_encryptor = new CStreamEncryptor_AES128_CBC(config);
    }
    else
        return PAVP_STATUS_NOT_SUPPORTED;

    if (NULL == m_encryptor)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    PavpEpidStatus sts = m_encryptor->SetKey(key);
    if(PAVP_EPID_FAILURE(sts)) 
        return sts;

    memcpy(m_encryptionKey, key, 16);
    m_encryptionKeyBitSize = 8 * m_encryptorBlockSize;

    return PAVP_STATUS_SUCCESS;
}

PavpEpidStatus CPAVPVideo::ResetEncryptor(const uint8 *buf, bool isFreshness)
{
    if (isFreshness)
    {
        for (int i = 0; i < 16; i++)
            m_encryptionKey[i] ^= buf[i];
    }
    else
        memcpy (m_encryptionKey, buf, 16);
    
    return m_encryptor->SetKey(m_encryptionKey);
}

void CPAVPVideo::DestroyEncryptor()
{
    SecureZeroMemory(m_encryptionKey, m_encryptionKeyBitSize/8);
    m_encryptionKeyBitSize = 0;
    PAVPSDK_SAFE_DELETE(m_encryptor);
}

PavpEpidStatus CPAVPVideo::Encrypt(
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

PavpEpidStatus CPAVPVideo::Decrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        const void *config, 
        uint32 configSize)
{
    if (!m_isDecryptorModeExists)
        return PAVP_STATUS_NOT_SUPPORTED;
    if (0 != size%16 || 
        16 != configSize)
        return PAVP_STATUS_LENGTH_ERROR;

    IppStatus sts = ippStsNoErr;

    IppsRijndael128Spec *rij = NULL;
    int rijSize = 0;
    ippsRijndael128GetSize(&rijSize);

    rij = (IppsRijndael128Spec*)(new uint8[rijSize]);
    if(NULL == rij)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    sts = ippsRijndael128Init(m_decryptionKey, IppsRijndaelKey128, rij);
    if (ippStsNoErr != sts)
    {
        delete[] (void*)rij;
        return PAVP_STATUS_UNKNOWN_ERROR;
    }
    uint8 ctr[16];
    memcpy(ctr, config, 16);
    sts = ippsRijndael128DecryptCTR(src, dst, size, rij, (uint8*)ctr, PAVP_COUNTER_TYPE_A == m_decryptorMode.eCounterMode ? 32 : 64);
    if (ippStsNoErr != sts)
    {
        delete[] (void*)rij;
        return PAVP_STATUS_UNKNOWN_ERROR;
    }
    delete[] (void*)rij;
    return PAVP_STATUS_SUCCESS;
}


PavpEpidStatus CPAVPVideo::EncryptionKeyRefresh()
{
    PavpEpidStatus PavpStatus = PAVP_STATUS_SUCCESS;
    
    if (NULL == m_encryptor)
        return PAVP_STATUS_NOT_SUPPORTED;

    PAVP_GET_FRESHNESS_PARAMS freshness;
    PavpStatus = m_cpDevice->ExecuteFunc(PAVP_GET_FRESHNESS, NULL, 0, &freshness, sizeof(freshness));
    if(PAVP_EPID_FAILURE(PavpStatus)) 
        return PavpStatus;
    PavpStatus = m_cpDevice->ExecuteFunc(PAVP_SET_FRESHNESS, NULL, 0, NULL, 0);
    if(PAVP_EPID_FAILURE(PavpStatus)) 
        return PavpStatus;

    for (int i = 0; i < 16; i++)
        m_encryptionKey[i] ^= ((uint8*)(freshness.Freshness))[i];
    return m_encryptor->SetKey(m_encryptionKey);
}


PavpEpidStatus CPAVPVideo::DecryptionKeyRefresh()
{
    PavpEpidStatus PavpStatus = PAVP_STATUS_SUCCESS;
    
    if (!m_isDecryptorModeExists)
        return PAVP_STATUS_NOT_SUPPORTED;

    PAVP_GET_FRESHNESS_PARAMS freshness;
    PavpStatus = m_cpDevice->ExecuteFunc(PAVP_GET_FRESHNESS_ENCRYPT, NULL, 0, &freshness, sizeof(freshness));
    if(PAVP_EPID_FAILURE(PavpStatus)) 
        return PavpStatus;

    for (int i = 0; i < 16; i++)
        m_decryptionKey[i] ^= ((uint8*)(&freshness.Freshness))[i];

    return m_cpDevice->ExecuteFunc(PAVP_SET_FRESHNESS_ENCRYPT, NULL, 0, NULL, 0);
}

PavpEpidStatus CPAVPVideo::SetKey(const uint8 *encryptionKey, const uint8 *decryptionKey)
{
    IppStatus sts = ippStsNoErr;
    PavpEpidStatus PavpStatus = PAVP_STATUS_SUCCESS;

    if (NULL == m_encryptor)
        return PAVP_STATUS_NOT_SUPPORTED;


    IppsRijndael128Spec *rij = NULL;
    int rijSize = 0;
    ippsRijndael128GetSize(&rijSize);

    rij = (IppsRijndael128Spec*)(new uint8[rijSize]);
    if(NULL == rij)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    PAVP_SET_STREAM_KEY_PARAMS setStreamKeyParams;
    for (;;)
    {
        sts = ippsRijndael128Init(m_encryptionKey, IppsRijndaelKey128, rij);
        if (ippStsNoErr != sts)
            break;
        if (NULL != encryptionKey)
        {
            sts = ippsRijndael128EncryptECB(encryptionKey, (Ipp8u*)(setStreamKeyParams.EncryptedDecryptKey), 16, rij, IppsCPPaddingNONE);
            if (ippStsNoErr != sts)
                break;
            setStreamKeyParams.StreamType = PAVP_SET_KEY_DECRYPT;
        }
        if (NULL != decryptionKey)
        {
            sts = ippsRijndael128EncryptECB(decryptionKey, (Ipp8u*)(setStreamKeyParams.EncryptedEncryptKey), 16, rij, IppsCPPaddingNONE);
            if (ippStsNoErr != sts)
                break;
            setStreamKeyParams.StreamType = NULL != encryptionKey ? PAVP_SET_KEY_BOTH : PAVP_SET_KEY_ENCRYPT;
        }
        break;
    }
    SecureZeroMemory(rij, rijSize);
    PAVPSDK_SAFE_DELETE_ARRAY(rij);
    if (ippStsNoErr != sts)
        return PAVP_STATUS_UNKNOWN_ERROR;

    PavpStatus = m_cpDevice->ExecuteFunc(PAVP_SET_STREAM_KEY, &setStreamKeyParams, sizeof(setStreamKeyParams), NULL, 0);
    if(PAVP_EPID_FAILURE(PavpStatus)) 
        return PavpStatus;
    
    if (NULL != encryptionKey)
    {
        PavpStatus = ResetEncryptor(encryptionKey, false);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            return PavpStatus;
        memcpy(m_encryptionKey, encryptionKey, 16);
    }
    if (NULL != decryptionKey)
        memcpy(m_decryptionKey, decryptionKey, 16);

    return PAVP_STATUS_SUCCESS;
}
