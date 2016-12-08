//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "umc_va_base.h"

#ifdef UMC_VA_LINUX

#include "umc_va_linux_protected.h"
#include "umc_va_linux.h"

using namespace UMC;

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

/////////////////////////////////////////////////
ProtectedVA::ProtectedVA(mfxU16 p)
    : m_protected(p)
    , m_counterMode(0)
    , m_encryptionType(0)
    , m_encryptBegin(0)
    , m_encryptCount(0)
{
    memset(&m_bs, 0, sizeof(mfxBitstream));
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
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_PAVP_ANY(m_protected))
    {
        mfxExtPAVPOption const * extPAVP = (mfxExtPAVPOption*)GetExtBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

        if (!extPAVP)
            return UMC_ERR_FAILED;

        m_encryptionType = extPAVP->EncryptionType;

        m_counterMode = extPAVP->CounterType;
    }
    else
    {
        m_encryptionType = MFX_PAVP_AES128_CTR;
        m_counterMode = MFX_PAVP_CTR_TYPE_C;
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
    if (!bs || !bs->EncryptedData)
        return;

    if (m_encryptBegin && !memcmp(&m_bs, bs, sizeof(mfxBitstream)))
        return;

    memcpy_s(&m_bs, sizeof(mfxBitstream), bs, sizeof(mfxBitstream));
    m_encryptCount = 0;
    m_encryptBegin = 0;

    mfxEncryptedData * temp = bs->EncryptedData;
    while (temp)
    {
        m_encryptCount++;
        temp = temp->Next;
    }
}

mfxBitstream * ProtectedVA::GetBitstream()
{
    return &m_bs;
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

#endif // UMC_VA_LINUX
