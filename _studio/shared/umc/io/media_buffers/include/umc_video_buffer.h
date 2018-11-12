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

#ifndef __UMC_VIDEO_BUFFER_H
#define __UMC_VIDEO_BUFFER_H

#include "umc_sample_buffer.h"

namespace UMC
{

class VideoBufferParams : public MediaBufferParams
{
    DYNAMIC_CAST_DECL(VideoBufferParams, MediaBufferParams)

public:
    // Default constructor
    VideoBufferParams(void);
    // Destructor
    virtual ~VideoBufferParams(void);

    uint32_t m_lIPDistance;         // (uint32_t) distance between I,P & B frames
    uint32_t m_lGOPSize;            // (uint32_t) size of GOP
};

class VideoBuffer : public SampleBuffer
{
    DYNAMIC_CAST_DECL(VideoBuffer, SampleBuffer)

public:
    // Default constructor
    VideoBuffer(void);
    // Destructor
    virtual ~VideoBuffer(void);

    // Initialize buffer
    virtual Status Init(MediaReceiverParams* init);
    // Lock input buffer
    virtual Status LockInputBuffer(MediaData* in);
    // Unlock input buffer
    virtual Status UnLockInputBuffer(MediaData* in, Status StreamStatus = UMC_OK);
    // Lock output buffer
    virtual Status LockOutputBuffer(MediaData* out);
    // Unlock output buffer
    virtual Status UnLockOutputBuffer(MediaData* out);
    // Release object
    virtual Status Close(void);

protected:
    // Build help pattern
    bool BuildPattern(void);

    uint32_t m_lFrameNumber;        // (uint32_t) number of current frame
    uint32_t m_lIPDistance;         // (uint32_t) distance between I,P & P frame(s)
    uint32_t m_lGOPSize;            // (uint32_t) size of GOP
    FrameType *m_pEncPattern;     // (FrameType *) pointer to array of encoding pattern

    size_t m_nImageSize;        // (size_t) size of frame(s)
};

} // namespace UMC

#endif // __UMC_VIDEO_BUFFER_H
