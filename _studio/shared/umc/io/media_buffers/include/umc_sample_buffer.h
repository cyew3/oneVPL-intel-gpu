// Copyright (c) 2003-2019 Intel Corporation
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

#ifndef __UMC_SAMPLE_BUFFER_H
#define __UMC_SAMPLE_BUFFER_H

#include "umc_media_buffer.h"

namespace UMC
{

// forward declaration of used types
class SampleInfo;

class SampleBuffer : public MediaBuffer
{
    DYNAMIC_CAST_DECL(SampleBuffer, MediaBuffer)

public:
    // Default constructor
    SampleBuffer(void);
    // Destructor
    virtual
    ~SampleBuffer(void);

    // Initialize buffer
    virtual
    Status Init(MediaReceiverParams* init);

    // Lock input buffer
    virtual
    Status LockInputBuffer(MediaData* in);
    // Unlock input buffer
    virtual
    Status UnLockInputBuffer(MediaData* in, Status StreamStatus = UMC_OK);

    // Lock output buffer
    virtual
    Status LockOutputBuffer(MediaData* out);
    // Unlock output buffer
    virtual
    Status UnLockOutputBuffer(MediaData* out);

    // Stop buffer
    virtual
    Status Stop(void);

    // Release object
    virtual
    Status Close(void);

    // Reset object
    virtual
    Status Reset(void);

    // Dump current state via vm_debug_trace
    virtual
    Status DumpState();

protected:
    std::mutex m_synchro;                                         // (std::mutex) synchro object

    uint8_t *m_pbAllocatedBuffer;                                 // (uint8_t *) pointer to allocated buffer
    MemID m_midAllocatedBuffer;                                 // (MemID) memory ID of allocated buffer
    size_t m_lAllocatedBufferSize;                              // (size_t) size of allocated buffer

    uint8_t *m_pbBuffer;                                          // (uint8_t *) pointer to allocated buffer
    size_t m_lBufferSize;                                       // (size_t) size of using buffer

    uint8_t *m_pbFree;                                            // (uint8_t *) pointer to free space
    size_t m_lFreeSize;                                         // (size_t) size of free space
    size_t m_lInputSize;                                        // (size_t) size of input data portion

    uint8_t *m_pbUsed;                                            // (uint8_t *) pointer to used space
    size_t m_lUsedSize;                                         // (size_t) size of used space

    SampleInfo *m_pSamples;                                     // (SampleInfo *) queue of filled sample info

    bool m_bEndOfStream;                                        // (bool) end of stream reached
    bool m_bQuit;                                               // (bool) end of stream reached and buffer is empty

    MediaBufferParams m_Params;                                 // (MediaBufferParams) parameters of current buffer
};

} // namespace UMC

#endif // __UMC_SAMPLE_BUFFER_H
