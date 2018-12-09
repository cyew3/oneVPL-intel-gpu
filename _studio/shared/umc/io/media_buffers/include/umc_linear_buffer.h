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

#ifndef __UMC_LINEAR_BUFFER_H
#define __UMC_LINEAR_BUFFER_H

#include "umc_media_buffer.h"

namespace UMC
{

class SampleInfo
{
public:
    double m_dTime;                                             // (double) PTS of media sample
    double m_dTimeAux;                                          // (double) Additional time stamp, it can be interpreted as DTS of end time
    FrameType m_FrameType;                                      // (FrameType) Frame type
    size_t m_lDataSize;                                         // (size_t) media sample size
    size_t m_lBufferSize;                                       // (size_t) media sample buffer size
    uint8_t *m_pbData;                                            // (uint8_t *) pointer to data
    SampleInfo *m_pNext;                                        // (SampleInfo *) pointer to next media sample info
};

class LinearBuffer : public MediaBuffer
{
    DYNAMIC_CAST_DECL(LinearBuffer, MediaBuffer)

public:
    // Default constructor
    LinearBuffer(void);
    // Destructor
    virtual
    ~LinearBuffer(void);

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

protected:
    std::mutex m_synchro;                                         // (vm_mutex) synchro object

    uint8_t *m_pbAllocatedBuffer;                                 // (uint8_t *) pointer to allocated buffer
    MemID m_midAllocatedBuffer;                                 // (MemID) memory ID of allocated buffer
    size_t m_lAllocatedBufferSize;                              // (size_t) size of allocated buffer

    uint8_t *m_pbBuffer;                                          // (uint8_t *) pointer to allocated buffer
    size_t m_lBufferSize;                                       // (size_t) size of using buffer

    uint8_t *m_pbFree;                                            // (uint8_t *) pointer to free space
    size_t m_lFreeSize;                                         // (size_t) size of free space
    size_t m_lInputSize;                                        // (size_t) size of input data potion

    uint8_t *m_pbUsed;                                            // (uint8_t *) pointer to used space
    size_t m_lUsedSize;                                         // (size_t) size of used space
    size_t m_lDummySize;                                        // (size_t) size of dummy size at end of buffer
    size_t m_lOutputSize;                                       // (size_t) size of output data potion

    SampleInfo *m_pSamples;                                     // (SampleInfo *) queue of filled sample info
    SampleInfo *m_pFreeSampleInfo;                              // (SampleInfo *) queue of free sample info structures

    bool m_bEndOfStream;                                        // (bool) end of stream reached
    bool m_bQuit;                                               // (bool) end of stream reached and buffer is empty

    MediaBufferParams m_Params;                                 // (MediaBufferParams) parameters of current buffer

    SampleInfo m_Dummy;                                         // (SampleInfo) sample to handle data gaps
};

} // namespace UMC

#endif // __UMC_LINEAR_BUFFER_H
