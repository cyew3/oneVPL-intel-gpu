//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#ifdef _DEBUG
#define VM_DEBUG
#endif

#include "umc_va_base.h"

#if defined(UMC_VA_DXVA)

#include "umc_va_dxva2_protected.h"
#include "umc_va_dxva2.h"
//#include "intel_pavp_api.h"

using namespace UMC;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
// from pavp
const GUID DXVA2_Intel_Pavp = 
{ 0x7460004, 0x7533, 0x4e1a, { 0xbd, 0xe3, 0xff, 0x20, 0x6b, 0xf5, 0xce, 0x47 } };

const GUID D3D11_CRYPTO_TYPE_AES128_CTR =
{0x9b6bd711, 0x4f74, 0x41c9, {0x9e, 0x7b, 0xb, 0xe2, 0xd7, 0xd9, 0x3b, 0x4f }};

static mfxExtBuffer* GetExtBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
        {
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
        }
    }

    return 0;
}
#endif // MFX_PROTECTED_FEATURE_DISABLE

/////////////////////////////////////////////////
ProtectedVA::ProtectedVA(mfxU16 p)
    : m_bs(0)
    , m_counterMode(PAVP_COUNTER_TYPE_A)
    , m_encryptionType(PAVP_ENCRYPTION_AES128_CTR) //(PAVP_ENCRYPTION_NONE)
    , m_encryptBegin(0)
    , m_encryptCount(0)
{
    m_protected = p;
}

mfxU16 ProtectedVA::GetProtected() const
{
    return m_protected;
}

void ProtectedVA::SetProtected(mfxU16 p)
{
    m_protected = p;
}

Status ProtectedVA::SetModes(mfxVideoParam * params)
{
#ifndef MFX_PROTECTED_FEATURE_DISABLE
    if (IS_PROTECTION_PAVP_ANY(m_protected))
    {
        mfxExtPAVPOption * pavpOpt = (mfxExtPAVPOption*)GetExtBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

        if (!pavpOpt)
            return UMC_ERR_FAILED;

        m_encryptionType = (pavpOpt->EncryptionType == MFX_PAVP_AES128_CBC) ? PAVP_ENCRYPTION_AES128_CBC : 
            ((pavpOpt->EncryptionType == MFX_PAVP_AES128_CTR) ? PAVP_ENCRYPTION_AES128_CTR : PAVP_ENCRYPTION_NONE);

        m_counterMode = (pavpOpt->CounterType == MFX_PAVP_CTR_TYPE_B) ? PAVP_COUNTER_TYPE_B : 
            ((pavpOpt->CounterType == MFX_PAVP_CTR_TYPE_C) ? PAVP_COUNTER_TYPE_C : PAVP_COUNTER_TYPE_A);
    }
    else if (IS_PROTECTION_WIDEVINE(m_protected))
    {
        m_encryptionType = 0;
        m_counterMode = 0;
    }
    else
    {
        m_encryptionType = PAVP_ENCRYPTION_AES128_CTR;
        m_counterMode = PAVP_COUNTER_TYPE_C;
    }
#else
    params = params;
#endif
    return UMC_OK;
}

Ipp32s ProtectedVA::GetEncryptionMode() const
{
    return m_encryptionType;
}

Ipp32s ProtectedVA::GetCounterMode() const
{
    return m_counterMode;
}


void ProtectedVA::SetBitstream(mfxBitstream *bs)
{
    if (m_encryptBegin && m_bs == bs)
        return;

    m_bs = bs;
    m_encryptCount = 0;
    m_encryptBegin = 0;

    if (!m_bs || !m_bs->EncryptedData)
        return;

    mfxEncryptedData * temp = bs->EncryptedData;
    while (temp)
    {
        m_encryptCount++;
        temp = temp->Next;
    }
}

const GUID & ProtectedVA::GetEncryptionGUID() const
{
#ifndef MFX_PROTECTED_FEATURE_DISABLE
    if (IS_PROTECTION_PAVP_ANY(m_protected))
        return ::DXVA2_Intel_Pavp;
    else if (MFX_PROTECTION_GPUCP_AES128_CTR == m_protected)
        /* Guid for DirectX 9 would likely be D3DCRYPTOTYPE_AES128_CTR but drivers up until 15.31 
        are only supports RSAES_OAEP through DX11*/
        return ::D3D11_CRYPTO_TYPE_AES128_CTR;
    else
#endif
        return GUID_NULL;
}

mfxBitstream * ProtectedVA::GetBitstream()
{
    return m_bs;
}

mfxU32 ProtectedVA::GetBSCurrentEncrypt() const
{
    return m_encryptBegin;
}

void ProtectedVA::ResetBSCurrentEncrypt()
{
    m_encryptBegin = 0;
}

void ProtectedVA::MoveBSCurrentEncrypt(mfxI32 count)
{
    m_encryptBegin += count;

    if (m_encryptBegin >= m_encryptCount)
    {
        ResetBSCurrentEncrypt();
    }
}

#endif // #if defined(UMC_VA_DXVA)
