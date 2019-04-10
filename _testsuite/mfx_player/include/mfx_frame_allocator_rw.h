/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_base_allocator.h"
#include "mfx_iproxy.h"
#include "mfx_d3d11_allocator.h"

//rw version with updated public API
class MFXFrameAllocatorRW
    : public MFXFrameAllocator
{
public :
    //read means SW surfaces filled with actual data, write means gained memory can be written
    virtual mfxStatus LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockflag /*MFXReadWriteMid::read|write*/) = 0;
    //write means data should be reflected 
    virtual mfxStatus UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 writeflag /*MFXReadWriteMid::write|0*/) = 0;
};

template <>
class InterfaceProxy<MFXFrameAllocatorRW>
    : public InterfaceProxyBase<MFXFrameAllocatorRW>
{
    typedef InterfaceProxyBase<MFXFrameAllocatorRW> base;
public:
    InterfaceProxy(std::auto_ptr<MFXFrameAllocatorRW>& pTarget)
        : base(pTarget)
    {
    }
    virtual mfxStatus Init(mfxAllocatorParams *pParams)
    {
        return m_pTarget->Init(pParams);
    }
    virtual mfxStatus Close()
    {
        return m_pTarget->Close();
    }
    virtual mfxStatus ReallocFrame(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut)
    {
        return m_pTarget->ReallocFrame(midIn, info, memType, midOut);
    }
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
    {
        return m_pTarget->AllocFrames(request, response);
    }
    virtual mfxStatus LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockflag)
    {
        return m_pTarget->LockFrameRW(mid, ptr, lockflag);
    }
    virtual mfxStatus UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 writeflag)
    {
        return m_pTarget->UnlockFrameRW(mid, ptr, writeflag);
    }
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        return m_pTarget->LockFrame(mid, ptr);
    }
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        return m_pTarget->UnlockFrame(mid, ptr);
    }
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle)
    {
        return m_pTarget->GetFrameHDL(mid, handle);
    }
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response)
    {
        return m_pTarget->FreeFrames(response);
    }
};

class AllocatorAdapterRW : public MFXFrameAllocatorRW
{
public:
    AllocatorAdapterRW(MFXFrameAllocatorRW * allocator);
    virtual ~AllocatorAdapterRW();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus ReallocFrame(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memTypemfx, mfxMemId *midOut);
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response);

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

    //read means SW surfaces filled with actual data, write means gained memory can be written
    virtual mfxStatus LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockflag /*MFXReadWriteMid::read|write*/);
    //write means data should be reflected
    virtual mfxStatus UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 writeflag /*MFXReadWriteMid::write|0*/);

protected:
    MFXFrameAllocatorRW * m_allocator;

    typedef std::pair<mfxMemId, mfxMemId> MidPair;
    std::vector<MidPair> m_mids;
};
