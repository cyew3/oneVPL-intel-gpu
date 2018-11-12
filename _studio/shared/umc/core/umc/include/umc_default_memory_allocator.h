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

#ifndef __UMC_DEFAULT_MEMORY_ALLOCATOR_H__
#define __UMC_DEFAULT_MEMORY_ALLOCATOR_H__

#include "umc_memory_allocator.h"

namespace UMC
{

struct MemoryInfo;

class DefaultMemoryAllocator : public MemoryAllocator
{
    DYNAMIC_CAST_DECL(DefaultMemoryAllocator, MemoryAllocator)

public:
    DefaultMemoryAllocator(void);
    virtual
    ~DefaultMemoryAllocator(void);

    // Initiates object
    virtual Status Init(MemoryAllocatorParams *pParams);

    // Closes object and releases all allocated memory
    virtual Status Close();

    // Allocates or reserves physical memory and return unique ID
    // Sets lock counter to 0
    virtual Status Alloc(MemID *pNewMemID, size_t Size, uint32_t Flags, uint32_t Align = 16);

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual void *Lock(MemID MID);

    // Unlock() decreases lock counter
    virtual Status Unlock(MemID MID);

    // Notifies that the data won?t be used anymore. Memory can be freed.
    virtual Status Free(MemID MID);

    // Immediately deallocates memory regardless of whether it is in use (locked) or no
    virtual Status DeallocateMem(MemID MID);

protected:
    MemoryInfo* memInfo;  // memory blocks descriptors
    int32_t      memCount; // number of allocated descriptors
    int32_t      memUsed;  // number of used descriptors
    MemID       lastMID;  // last value assigned to descriptor
};

} // namespace UMC

#endif // __UMC_DEFAULT_MEMORY_ALLOCATOR_H
