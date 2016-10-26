//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "mfxpcp.h"
#include "mfx_utils.h"

// from pavp
#ifndef PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES
#define PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES

typedef enum
{
    // Available counter types
    PAVP_COUNTER_TYPE_A = 1,    // 32-bit counter, 96 zeroes (CTG, ELK, ILK, SNB)
    PAVP_COUNTER_TYPE_B = 2,    // 64-bit counter, 32-bit Nonce, 32-bit zero (SNB)
    PAVP_COUNTER_TYPE_C = 4     // Standard AES counter
} PAVP_COUNTER_TYPE;

typedef enum
{
    // Available encryption types
    PAVP_ENCRYPTION_NONE        = 1,
    PAVP_ENCRYPTION_AES128_CTR  = 2,
    PAVP_ENCRYPTION_AES128_CBC  = 4,
    PAVP_ENCRYPTION_AES128_ECB  = 8,
} PAVP_ENCRYPTION_TYPE;

typedef struct tagPAVP_ENCRYPTION_MODE
{
    PAVP_ENCRYPTION_TYPE    eEncryptionType;
    PAVP_COUNTER_TYPE       eCounterMode;
} PAVP_ENCRYPTION_MODE;

#endif // PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES

typedef struct _DXVA_Intel_Pavp_Protocol
{
    DXVA_EncryptProtocolHeader EncryptProtocolHeader;
    DWORD                      dwBufferSize;
    DWORD                      dwAesCounter;
} DXVA_Intel_Pavp_Protocol, *PDXVA_Intel_Pavp_Protocol;

typedef struct _DXVA_Intel_Pavp_Protocol2
{
    DXVA_EncryptProtocolHeader EncryptProtocolHeader;
    DWORD                      dwBufferSize;
    DWORD                      dwAesCounter[4];
    PAVP_ENCRYPTION_MODE       PavpEncryptionMode;
} DXVA_Intel_Pavp_Protocol2, *PDXVA_Intel_Pavp_Protocol2;

namespace UMC
{

class ProtectedVA
{
public:

    ProtectedVA(mfxU16 p = 0);

    virtual mfxU16 GetProtected() const;

    void SetProtected(mfxU16 p);

    Status SetModes(mfxVideoParam * params);

    Ipp32s GetEncryptionMode() const;

    Ipp32s GetCounterMode() const;

    void SetBitstream(mfxBitstream *bs);

    const GUID & GetEncryptionGUID() const;

    mfxBitstream * GetBitstream();


    // for MVC/SVC support
    mfxU32 GetBSCurrentEncrypt() const;
    void MoveBSCurrentEncrypt(mfxI32 count);
    void ResetBSCurrentEncrypt();

protected:
    mfxU16 m_protected;
    mfxBitstream *m_bs;

    Ipp32s m_counterMode;
    Ipp32s m_encryptionType;

    Ipp32u m_encryptBegin;
    Ipp32u m_encryptCount;
};

}

#endif // UMC_VA_DXVA
