//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
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

    Ipp32s GetEncryptionMode() const;

    Ipp32s GetCounterMode() const;

    void SetBitstream(mfxBitstream *bs);

    mfxBitstream * GetBitstream();

    mfxU32 GetBSCurrentEncrypt() const;
    void MoveBSCurrentEncrypt(mfxI32 count);
    void ResetBSCurrentEncrypt();

protected:
    mfxU16 m_protected;
    mfxBitstream m_bs;

    Ipp32s m_counterMode;
    Ipp32s m_encryptionType;

    Ipp32u m_encryptBegin;
    Ipp32u m_encryptCount;
};

}

#endif // __UMC_VA_LINUX_PROTECTED_H_

#endif // UMC_VA_LINUX
