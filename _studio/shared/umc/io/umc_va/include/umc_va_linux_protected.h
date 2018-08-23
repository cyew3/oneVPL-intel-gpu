//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

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

    void SetProtected(mfxU16 p);

    Status SetModes(mfxVideoParam * params);

    int32_t GetEncryptionMode() const;

    int32_t GetCounterMode() const;

    void SetBitstream(mfxBitstream *bs);

    mfxBitstream * GetBitstream();

    mfxU32 GetBSCurrentEncrypt() const;
    void MoveBSCurrentEncrypt(mfxI32 count);
    void ResetBSCurrentEncrypt();

protected:
    mfxU16 m_protected;
    mfxBitstream m_bs;

    int32_t m_counterMode;
    int32_t m_encryptionType;

    uint32_t m_encryptBegin;
    uint32_t m_encryptCount;
};

}

#endif // __UMC_VA_LINUX_PROTECTED_H_

#endif // UMC_VA_LINUX
