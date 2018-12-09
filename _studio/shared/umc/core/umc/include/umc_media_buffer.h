// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_MEDIA_BUFFER_H__
#define __UMC_MEDIA_BUFFER_H__

#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_media_receiver.h"
#include "umc_memory_allocator.h"

#include <mutex>

namespace UMC
{

class MediaBufferParams : public MediaReceiverParams
{
    DYNAMIC_CAST_DECL(MediaBufferParams, MediaReceiverParams)

public:
    // Default constructor
    MediaBufferParams(void);

    size_t m_prefInputBufferSize;                               // (size_t) preferable size of input potion(s)
    uint32_t m_numberOfFrames;                                    // (uint32_t) minimum number of data potion in buffer
    size_t m_prefOutputBufferSize;                              // (size_t) preferable size of output potion(s)

    MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator object
};

class MediaBuffer : public MediaReceiver
{
    DYNAMIC_CAST_DECL(MediaBuffer, MediaReceiver)

public:

    // Constructor
    MediaBuffer(void);
    // Destructor
    virtual
    ~MediaBuffer(void);

    // Lock output buffer
    virtual
    Status LockOutputBuffer(MediaData *out) = 0;
    // Unlock output buffer
    virtual
    Status UnLockOutputBuffer(MediaData *out) = 0;

    // Initialize the buffer
    virtual
    Status Init(MediaReceiverParams *pInit);

    // Release object
    virtual
    Status Close(void);

protected:

    MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator object
    MemoryAllocator *m_pAllocated;                              // (MemoryAllocator *) owned pointer to memory allocator object
};

} // end namespace UMC

#endif // __UMC_MEDIA_BUFFER_H__
