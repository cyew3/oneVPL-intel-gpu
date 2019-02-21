// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "umc_vc1_dec_thread.h"
#include "vm_event.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_job.h"



namespace UMC
{

VC1ThreadDecoder::VC1ThreadDecoder()
{
    m_pMemoryAllocator = NULL;
    m_pStore = NULL;
    m_pJobSlice = NULL;

    m_bQuit = false;
    m_bStartDecoding = false;

    m_Status = 0;
    m_iNumber = 0;

}
VC1ThreadDecoder::~VC1ThreadDecoder(void)
{
    Release();

}
void VC1ThreadDecoder::Release(void)
{
    if (m_pJobSlice)
    {
        m_pJobSlice->~VC1TaskProcessor();

        if (m_pJobSlice != &m_TaskProcessor)
        {
            delete m_pJobSlice;
            m_pJobSlice = NULL;
        }
    }

    // threading tools
    if (m_hThread.joinable())
    {
        m_bQuit = true;

        m_hThread.join();
    }

    m_bQuit = false;

}
Status VC1ThreadDecoder::Init(VC1Context* pContext,
                              int32_t iNumber,
                              VC1TaskStore* pTaskStore,
                              MemoryAllocator* pMemoryAllocator,
                              VC1TaskProcessor* pExternalProcessor)
{
    // release object before initialization
    Release();

    // save thread number(s)
    m_iNumber = iNumber;
    m_pMemoryAllocator = pMemoryAllocator;

    // save pointer to TaskStore
    m_pStore = pTaskStore;

    if (!pExternalProcessor)
       m_pJobSlice = &m_TaskProcessor;
   else
       m_pJobSlice = pExternalProcessor;

    if (NULL == m_pJobSlice)
        return UMC_ERR_ALLOC;

    if (!pExternalProcessor)
    {
        if (UMC_OK != m_TaskProcessor.Init(pContext,iNumber, (VC1TaskStoreSW *)pTaskStore,m_pMemoryAllocator))
            return UMC_ERR_INIT;
    }

    // threading tools
    m_bQuit = false;
#if defined (_WIN32) && (_DEBUG)
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).setThreadToDebug(GetCurrentThreadId());
#endif
#endif

    return UMC_OK;

}

Status VC1ThreadDecoder::process(void)
{
    return m_pJobSlice->process();
}

} // namespace UMC
#endif
