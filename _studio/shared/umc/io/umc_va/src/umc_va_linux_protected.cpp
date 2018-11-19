// Copyright (c) 2016-2019 Intel Corporation
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

#include "umc_va_base.h"

#ifdef UMC_VA_LINUX

#include "umc_va_linux_protected.h"
#include "umc_va_linux.h"

using namespace UMC;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
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
#endif

/////////////////////////////////////////////////
ProtectedVA::ProtectedVA(mfxU16 p)
    : m_protected(p)
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    , m_counterMode(0)
    , m_encryptionType(0)
    , m_encryptBegin(0)
    , m_encryptCount(0)
#endif
{
    memset(&m_bs, 0, sizeof(mfxBitstream));
}

mfxU16 ProtectedVA::GetProtected() const
{
    return m_protected;
}

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
void ProtectedVA::SetProtected(mfxU16 p)
{
    m_protected = p;
}

Status ProtectedVA::SetModes(mfxVideoParam * params)
{
    if (IS_PROTECTION_PAVP_ANY(m_protected))
    {
        mfxExtPAVPOption const * extPAVP = (mfxExtPAVPOption*)GetExtBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

        if (!extPAVP)
            return UMC_ERR_FAILED;

        m_encryptionType = extPAVP->EncryptionType;

        m_counterMode = extPAVP->CounterType;
    }
    else if (IS_PROTECTION_CENC(m_protected) || IS_PROTECTION_WIDEVINE(m_protected))
    {
        m_encryptionType = 0;
        m_counterMode = 0;
    }
    else
    {
        m_encryptionType = MFX_PAVP_AES128_CTR;
        m_counterMode = MFX_PAVP_CTR_TYPE_C;
    }

    return UMC_OK;
}

int32_t ProtectedVA::GetEncryptionMode() const
{
    return m_encryptionType;
}

int32_t ProtectedVA::GetCounterMode() const
{
    return m_counterMode;
}
#endif // #if !defined(MFX_PROTECTED_FEATURE_DISABLE)

void ProtectedVA::SetBitstream(mfxBitstream *bs)
{
    if (!bs)
        return;

    if (IS_PROTECTION_CENC(m_protected))
    {
        m_bs = *bs;
        return;
    }

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_WIDEVINE(m_protected))
        return;

    if (!bs->EncryptedData)
        return;

    if (m_encryptBegin && !memcmp(&m_bs, bs, sizeof(mfxBitstream)))
        return;

    m_bs = *bs;

    m_encryptCount = 0;
    m_encryptBegin = 0;

    mfxEncryptedData * temp = bs->EncryptedData;
    while (temp)
    {
        m_encryptCount++;
        temp = temp->Next;
    }
#endif
}

mfxBitstream * ProtectedVA::GetBitstream()
{
    return &m_bs;
}

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
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
#endif // #if !defined(MFX_PROTECTED_FEATURE_DISABLE)

#endif // UMC_VA_LINUX
