// Copyright (c) 2018-2019 Intel Corporation
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

#ifdef UMC_VA_LINUX

#ifndef __UMC_VA_LINUX_PROTECTED_H_
#define __UMC_VA_LINUX_PROTECTED_H_

#include "mfxpcp.h"
#include "mfx_utils.h"
#include "vaapi_ext_interface.h"

namespace UMC
{

class ProtectedVA
{
public:

    ProtectedVA(mfxU16 p = 0);
    virtual ~ProtectedVA()
    {
    }

    virtual mfxU16 GetProtected() const;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    void SetProtected(mfxU16 p);
    Status SetModes(mfxVideoParam * params);
    int32_t GetEncryptionMode() const;
    int32_t GetCounterMode() const;
#endif

    void SetBitstream(mfxBitstream *bs);
    mfxBitstream * GetBitstream();

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    mfxU32 GetBSCurrentEncrypt() const;
    void MoveBSCurrentEncrypt(mfxI32 count);
    void ResetBSCurrentEncrypt();
#endif

protected:
    mfxU16 m_protected;
    mfxBitstream m_bs;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    int32_t m_counterMode;
    int32_t m_encryptionType;

    uint32_t m_encryptBegin;
    uint32_t m_encryptCount;
#endif
};

}

#endif // __UMC_VA_LINUX_PROTECTED_H_

#endif // UMC_VA_LINUX
