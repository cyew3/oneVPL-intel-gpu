//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_thread.h"
#include "vm_thread.h"
#include "vm_event.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_job.h"



namespace UMC
{

VC1ThreadDecoder::VC1ThreadDecoder() : m_hThread()
{
    m_pMemoryAllocator = NULL;
    m_pStore = NULL;
    m_pJobSlice = NULL;
    vm_thread_set_invalid(&m_hThread);
    vm_event_set_invalid(&m_hStartProcessing);
    vm_event_set_invalid(&m_hDoneProcessing);

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
    if (vm_thread_is_valid(&m_hThread))
    {
        m_bQuit = true;
        vm_event_signal(&m_hStartProcessing);

        vm_thread_wait(&m_hThread);
        vm_thread_close(&m_hThread);
    }

    vm_event_destroy(&m_hStartProcessing);
    vm_event_destroy(&m_hDoneProcessing);

    m_bQuit = false;

}
Status VC1ThreadDecoder::Init(VC1Context* pContext,
                              Ipp32s iNumber,
                              VC1TaskStore* pTaskStore,
                              VideoAccelerator* va,
                              MemoryAllocator* pMemoryAllocator,
                              VC1TaskProcessor* pExternalProcessor)
{
    static Ipp32u mask[4] = {2,32,64,128};
    static Ipp32u proc_mask[4] = {1,2,1,2};

    // release object before initialization
    Release();

    // save thread number(s)
    m_iNumber = iNumber;
    m_pMemoryAllocator = pMemoryAllocator;

    // save pointer to TaskStore
    m_pStore = pTaskStore;
#if defined (UMC_VA_DXVA)
    // create segment decoder
    m_pJobSlice = &m_TaskProcessor;
#else
   if (!pExternalProcessor)
       m_pJobSlice = &m_TaskProcessor;
   else
       m_pJobSlice = pExternalProcessor;

#endif

    if (NULL == m_pJobSlice)
        return UMC_ERR_ALLOC;

    if (!pExternalProcessor)
    {
        if (UMC_OK != m_TaskProcessor.Init(pContext,iNumber,pTaskStore,m_pMemoryAllocator))
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

Status VC1ThreadDecoder::StartProcessing(void)
{
    m_bStartDecoding = true;

    if (0 == vm_event_is_valid(&m_hStartProcessing))
        return UMC_ERR_FAILED;

    vm_event_reset(&m_hDoneProcessing);
    vm_event_signal(&m_hStartProcessing);

    return UMC_OK;

}

Status VC1ThreadDecoder::WaitForEndOfProcessing(void)
{

    if (0 == vm_event_is_valid(&m_hDoneProcessing))
        return UMC_ERR_FAILED;

    m_pStore->WakeUP();

    if (m_bStartDecoding)
        vm_event_wait(&m_hDoneProcessing);

    m_bStartDecoding = false;

    return m_Status;

}

Status VC1ThreadDecoder::WaitAndStop(void)
{
    if (0 == vm_event_is_valid(&m_hDoneProcessing))
        return UMC_ERR_FAILED;

    if (0 == vm_event_is_valid(&m_hStartProcessing))
        return UMC_ERR_FAILED;

    m_pStore->WakeUP();

    if (m_bStartDecoding)
        vm_event_wait(&m_hDoneProcessing);

    m_bStartDecoding = false;

    return UMC_OK;

}

Status VC1ThreadDecoder::process(void)
{
    return m_pJobSlice->process();
}
Ipp32u VC1ThreadDecoder::DecodingThreadRoutine(void *p)
{
    VC1ThreadDecoder *pObj = (VC1ThreadDecoder *) p;

    // check error(s)
    if (NULL == p)
        return 0x0baad;

    {
        vm_event (&hStartProcessing)(pObj->m_hStartProcessing);
        vm_event (&hDoneProcessing)(pObj->m_hDoneProcessing);

        // wait for begin decoding
        vm_event_wait(&hStartProcessing);

        while (false == pObj->m_bQuit)
        {
            pObj->m_Status = pObj->process();

            // set done event
            vm_event_signal(&hDoneProcessing);

            // wait for begin decoding
            vm_event_wait(&hStartProcessing);
        }
    }

    return 0x0264dec0 + pObj->m_iNumber;

}

} // namespace UMC
#endif
