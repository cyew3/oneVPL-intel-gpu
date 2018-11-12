// Copyright (c) 2006-2018 Intel Corporation
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

#ifndef __UMC_DEFAULT_FRAME_ALLOCATOR_H__
#define __UMC_DEFAULT_FRAME_ALLOCATOR_H__

#include "umc_frame_allocator.h"
#include "umc_default_memory_allocator.h"

#include <vector>

namespace UMC
{

class FrameInformation;

class DefaultFrameAllocator : public FrameAllocator
{
    DYNAMIC_CAST_DECL(DefaultFrameAllocator, FrameAllocator)

public:
    DefaultFrameAllocator(void);
    virtual ~DefaultFrameAllocator(void);

    // Initiates object
    virtual Status Init(FrameAllocatorParams *pParams);

    // Closes object and releases all allocated memory
    virtual Status Close();

    virtual Status Reset();

    // Allocates or reserves physical memory and returns unique ID
    // Sets lock counter to 0
    virtual Status Alloc(FrameMemID *pNewMemID, const VideoDataInfo * info, uint32_t flags);

    virtual Status GetFrameHandle(UMC::FrameMemID mid, void * handle);

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual const FrameData* Lock(FrameMemID mid);

    // Unlock() decreases lock counter
    virtual Status Unlock(FrameMemID mid);

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual Status IncreaseReference(FrameMemID mid);

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual Status DecreaseReference(FrameMemID mid);

protected:

    Status Free(FrameMemID mid);

    std::vector<FrameInformation *> m_frames;

};

} // namespace UMC

#endif // __UMC_DEFAULT_FRAME_ALLOCATOR_H__
