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

#include "umc_media_buffer.h"
#include "umc_default_memory_allocator.h"

namespace UMC
{

MediaBufferParams::MediaBufferParams(void)
{
    m_prefInputBufferSize = 0;
    m_numberOfFrames = 0;
    m_prefOutputBufferSize = 0;

    m_pMemoryAllocator = 0;

} // MediaBufferParams::MediaBufferParams(void)

MediaBuffer::MediaBuffer(void)
{
    m_pMemoryAllocator = 0;
    m_pAllocated = 0;

} // MediaBuffer::MediaBuffer(void)

MediaBuffer::~MediaBuffer(void)
{
    Close();

} // MediaBuffer::~MediaBuffer(void)

Status MediaBuffer::Init(MediaReceiverParams *pInit)
{
    MediaBufferParams *pParams = DynamicCast<MediaBufferParams> (pInit);

    // check error(s)
    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    // release the object before initialization
    MediaBuffer::Close();

    // use the external memory allocator
    if (pParams->m_pMemoryAllocator)
    {
        m_pMemoryAllocator = pParams->m_pMemoryAllocator;
    }
    // allocate default memory allocator
    else
    {
        m_pAllocated = new DefaultMemoryAllocator();
        if (NULL == m_pAllocated)
            return UMC_ERR_ALLOC;

        m_pMemoryAllocator = m_pAllocated;
    }

    return UMC_OK;

} // Status MediaBuffer::Init(MediaReceiverParams *pInit)

Status MediaBuffer::Close(void)
{
    if (m_pAllocated)
        delete m_pAllocated;

    m_pMemoryAllocator = NULL;
    m_pAllocated = NULL;

    return UMC_OK;

} // Status MediaBuffer::Close(void)

} // namespace UMC
