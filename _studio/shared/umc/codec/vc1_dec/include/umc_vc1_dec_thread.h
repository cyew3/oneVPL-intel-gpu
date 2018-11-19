// Copyright (c) 2004-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_THREAD_H_
#define __UMC_VC1_DEC_THREAD_H_

#include "vm_types.h"
#include "umc_structures.h"
#include "umc_vc1_dec_job.h"
#include "umc_va_base.h"

#include <new>

namespace UMC
{

#pragma pack(1)

class VideoAccelerator;
class VC1TaskStore;
class VC1ThreadDecoder
{
    friend class VC1VideoDecoder;
public:
    // Default constructor
    VC1ThreadDecoder();
    // Destructor
    virtual ~VC1ThreadDecoder();

    // Initialize slice decoder
    virtual Status Init(VC1Context* pContext,
                        int32_t iNumber,
                        VC1TaskStore* pTaskStore,
                        MemoryAllocator* pMemoryAllocator,
                        VC1TaskProcessor* pExternalProcessor);

    // Decode picture segment
    virtual Status process(void);
    int32_t  getThreadID()
    {
        return this->m_iNumber;
    }

protected:
    // Release slice decoder
    void Release(void);

    //
    // Threading tools
    //


    vm_thread m_hThread;                                        // (vm_thread) handle to asynchronously working thread

    int32_t m_iNumber;                                           // (int32_t) ordinal number of decoder


    volatile
    bool m_bQuit;                                               // (bool) quit flag for additional thread(s)
    Status m_Status;                                            // (Status) async return value
    bool m_bStartDecoding;

private:
    MemoryAllocator*   m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator
    VC1TaskProcessorUMC   m_TaskProcessor;
    VC1TaskProcessor*  m_pJobSlice;
    VC1TaskStore*      m_pStore;
};

#pragma pack()

} // namespace UMC

#endif // __umc_vc1_dec_thread_H
#endif
