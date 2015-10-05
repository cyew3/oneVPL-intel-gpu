/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2008-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __CAM_SYSMEM_ALLOCATOR_H__
#define __CAM_SYSMEM_ALLOCATOR_H__

#include <stdlib.h>
#include "base_allocator.h"

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

struct CamSysMemAllocatorParams : mfxAllocatorParams
{
    CamSysMemAllocatorParams()
        : mfxAllocatorParams(),
          pBufferAllocator(0), alignment(0) { }
    MFXBufferAllocator *pBufferAllocator;
    mfxI32              alignment;
};

class CamSysMemFrameAllocator: public BaseFrameAllocator
{
public:
    CamSysMemFrameAllocator();
    virtual ~CamSysMemFrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    MFXBufferAllocator *m_pBufferAllocator;
    bool m_bOwnBufferAllocator;
    mfxU32 m_alignment;
};

class CamSysMemBufferAllocator : public MFXBufferAllocator
{
public:
    CamSysMemBufferAllocator();
    virtual ~CamSysMemBufferAllocator();
    virtual mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    virtual mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr);
    virtual mfxStatus UnlockBuffer(mfxMemId mid);
    virtual mfxStatus FreeBuffer(mfxMemId mid);
};

#endif // __CAM_SYSMEM_ALLOCATOR_H__