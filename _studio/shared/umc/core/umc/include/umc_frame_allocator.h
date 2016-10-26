//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_FRAME_ALLOCATOR_H__
#define __UMC_FRAME_ALLOCATOR_H__

#include "ippdefs.h"
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_mutex.h"

#define FMID_INVALID 0

namespace UMC
{

class VideoDataInfo;
class FrameData;

typedef Ipp32s FrameMemID;

enum
{
    FRAME_MID_INVALID = -1
};

/*enum UMC_ALLOC_FLAGS
{
    UMC_ALLOC_PERSISTENT  = 0x01,
    UMC_ALLOC_FUNCLOCAL   = 0x02
};*/

class FrameAllocatorParams
{
     DYNAMIC_CAST_DECL_BASE(FrameAllocatorParams)

public:
    FrameAllocatorParams(){}
    virtual ~FrameAllocatorParams(){}
};

class FrameAllocator
{
    DYNAMIC_CAST_DECL_BASE(FrameAllocator)

public:
    FrameAllocator(void){}
    virtual ~FrameAllocator(void){}

    // Initiates object
    virtual Status Init(FrameAllocatorParams *) { return UMC_OK;}

    // Closes object and releases all allocated memory
    virtual Status Close() = 0;

    virtual Status Reset() = 0;

    // Allocates or reserves physical memory and returns unique ID
    // Sets lock counter to 0
    virtual Status Alloc(FrameMemID *pNewMemID, const VideoDataInfo * info, Ipp32u Flags) = 0;

    virtual Status GetFrameHandle(UMC::FrameMemID MID, void * handle) = 0;

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual const FrameData* Lock(FrameMemID MID) = 0;

    // Unlock() decreases lock counter
    virtual Status Unlock(FrameMemID MID) = 0;

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual Status IncreaseReference(FrameMemID MID) = 0;

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual Status DecreaseReference(FrameMemID MID) = 0;

protected:
    Mutex m_guard;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    FrameAllocator(const FrameAllocator &);
    FrameAllocator & operator = (const FrameAllocator &);
};

} //namespace UMC

#endif //__UMC_FRAME_ALLOCATOR_H__
