//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2011 Intel Corporation. All Rights Reserved.
//

#ifndef _LIBMFX_ALLOCATOR_H_
#define _LIBMFX_ALLOCATOR_H_

#include <vector>
#include "mfxvideo.h"


// Internal Allocators
namespace mfxDefaultAllocator
{
    mfxStatus AllocBuffer(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    mfxStatus LockBuffer(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    mfxStatus UnlockBuffer(mfxHDL pthis, mfxMemId mid);
    mfxStatus FreeBuffer(mfxHDL pthis, mfxMemId mid);

    mfxStatus AllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response);

    struct BufferStruct
    {
        mfxHDL      allocator;
        mfxU32      id;
        mfxU32      nbytes;
        mfxU16      type;
    };
    struct FrameStruct
    {
        mfxU32          id;
        mfxFrameInfo    info;
    };
}

class mfxWideBufferAllocator
{
public:
    std::vector<mfxDefaultAllocator::BufferStruct*> m_bufHdl;
    mfxWideBufferAllocator(void);
    ~mfxWideBufferAllocator(void);
    mfxBufferAllocator bufferAllocator;
};

class mfxBaseWideFrameAllocator
{
public:
    mfxBaseWideFrameAllocator(mfxU16 type = 0);
    virtual ~mfxBaseWideFrameAllocator();
    mfxFrameAllocator       frameAllocator;
    mfxWideBufferAllocator  wbufferAllocator;
    mfxU32                  NumFrames;
    std::vector<mfxHDL>     m_frameHandles;
    // Type of this allocator
    mfxU16                  type;
};
class mfxWideSWFrameAllocator : public  mfxBaseWideFrameAllocator
{
public:
    mfxWideSWFrameAllocator(mfxU16 type);
    virtual ~mfxWideSWFrameAllocator(void) {};
};

#endif

