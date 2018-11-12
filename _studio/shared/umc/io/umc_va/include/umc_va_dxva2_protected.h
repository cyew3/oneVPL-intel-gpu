// Copyright (c) 2006-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    int32_t GetEncryptionMode() const;

    int32_t GetCounterMode() const;

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

    int32_t m_counterMode;
    int32_t m_encryptionType;

    uint32_t m_encryptBegin;
    uint32_t m_encryptCount;
};

}

#endif // UMC_VA_DXVA
