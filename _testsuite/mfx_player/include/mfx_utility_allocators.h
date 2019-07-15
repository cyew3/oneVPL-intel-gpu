/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

/// allocators that are used withit pipiline howevere not exposed by samples API
#pragma once

#include "mfx_frame_allocator_rw.h"
#include "mfx_adapters.h"
#include "vm_interlocked.h"

class LCCheckFrameAllocator
    : public InterfaceProxy<MFXFrameAllocatorRW>
{
    typedef InterfaceProxy<MFXFrameAllocatorRW> base;
public:
    typedef base::result_type result_type;
    LCCheckFrameAllocator(std::unique_ptr<MFXFrameAllocatorRW> &&pTargetAlloc)
        : base(std::move(pTargetAlloc))
    {
        m_nLocked      = 0;
    }
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        MFX_CHECK_WITH_FNC_NAME(m_nLocked >= 0, LockFrame(m_nLocked >= 0));
        vm_interlocked_inc32((volatile Ipp32u*)&m_nLocked);
        return base::LockFrame(mid, ptr);
    }
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        vm_interlocked_dec32((volatile Ipp32u*)&m_nLocked);
        MFX_CHECK_WITH_FNC_NAME(m_nLocked >= 0, UnlockFrame(m_nLocked >= 0));
        return base::UnlockFrame(mid, ptr);
    }
protected:
    mfxI32  m_nLocked;
};

//enable flags for lock unlock calls
//expiremental, not used by default
class LockRWEnabledFrameAllocator
    : public InterfaceProxy<MFXFrameAllocatorRW>
{
    typedef  InterfaceProxy<MFXFrameAllocatorRW> base;
public:
    typedef base::result_type result_type;
    LockRWEnabledFrameAllocator(std::unique_ptr<MFXFrameAllocatorRW > &&pTarget)
        : base(std::move(pTarget))
    {
    }
    virtual mfxStatus LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockFlag)
    {
        return base::LockFrame(MFXReadWriteMid(mid, lockFlag), ptr);
    }
    virtual mfxStatus UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockFlag)
    {
        return base::UnlockFrame(MFXReadWriteMid(mid, lockFlag), ptr);
    }
};
