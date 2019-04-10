/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_base_allocator.h"
#include <vector>

struct sBuffer
{
    mfxU32      id;
    mfxU32      nbytes;
    mfxU16      type;
};

struct sFrame
{
    mfxU32          id;
    mfxFrameInfo    info;
};

struct SysMemAllocatorParams : mfxAllocatorParams
{
    MFXBufferAllocator *pBufferAllocator;
};

class SysMemFrameAllocator : public BaseFrameAllocator
{
public:
    SysMemFrameAllocator();
    virtual ~SysMemFrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus ReallocImpl(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut);

    MFXBufferAllocator *m_pBufferAllocator;
    bool m_bOwnBufferAllocator;

    std::vector<mfxFrameAllocResponse *> m_vResp;

    mfxMemId *GetMidHolder(mfxMemId mid);
};

class SysMemBufferAllocator : public MFXBufferAllocator
{
public:
    SysMemBufferAllocator();
    virtual ~SysMemBufferAllocator();
    virtual mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    virtual mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr);
    virtual mfxStatus UnlockBuffer(mfxMemId mid);
    virtual mfxStatus FreeBuffer(mfxMemId mid);
};
