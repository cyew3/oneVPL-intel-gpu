/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

File Name: fast_copy.cpp

\* ****************************************************************************** */

#include "mf_utils.h"
#include "mf_fast_copy.h"
#include "ippi.h"

static HRESULT usless = 0;

FastCopy::FastCopy(void)
{
    m_mode = FAST_COPY_UNSUPPORTED;
    m_bCopyQuit = ippFalse;
    m_numThreads = 0;
} // FastCopy::FastCopy(void)

FastCopy::~FastCopy(void)
{
    Release();

} // FastCopy::~FastCopy(void)

mfxStatus FastCopy::Initialize(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU64 pFeatureMask;
    mfxU32 i = 0;

    // release object before allocation
    Release();

    if (MFX_ERR_NONE == sts)
    {
        ippGetCpuFeatures(&pFeatureMask, NULL);

        // streaming is on
        m_mode = FAST_COPY_SSE41;
        m_numThreads = 2;
    }
    if (MFX_ERR_NONE == sts && m_numThreads > 1)
    {
        m_pThreads.resize(m_numThreads - 1);
        m_tasks.resize(m_numThreads - 1);

        if (m_pThreads.empty() || m_tasks.empty()) sts = MFX_ERR_MEMORY_ALLOC;
    }
    // initialize events
    HRESULT hr = S_OK;
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        hr = m_tasks[i].EventStart.Init();
        if (FAILED(hr))
        {
            sts = MFX_ERR_MEMORY_ALLOC;
            break;
        }

        hr = m_tasks[i].EventEnd.Init();
        if ( FAILED(hr))
        {
            sts = MFX_ERR_MEMORY_ALLOC;
            break;
        }

        m_tasks[i].pbCopyQuit = &m_bCopyQuit;
    }
    // run threads
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        if (FAILED(m_pThreads[i].Create(CopyByThread, (void *)(&m_tasks[i]))))
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
} // mfxStatus FastCopy::Initialize(void)

mfxStatus FastCopy::Release(void)
{
    m_bCopyQuit = ippTrue;

    if ((m_numThreads > 1) && !m_tasks.empty() && m_pThreads.empty())
    {
        // set event
        for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
        {
            m_tasks[i].EventStart.Signal();
            m_pThreads[i].Wait();
        }
    }

    m_numThreads = 0;
    m_mode = FAST_COPY_UNSUPPORTED;

    m_pThreads.clear();
    m_tasks.clear();

    m_bCopyQuit = ippFalse;

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Release(void)

mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, const IppiSize &inroi)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    if (NULL == pDst || NULL == pSrc)
    {
        return MFX_ERR_NULL_PTR;
    }

    mfxU32 partSize = inroi.height / m_numThreads;
    mfxU32 rest = inroi.height % m_numThreads;
    IppiSize roi = {inroi.width, partSize};

    // distribute tasks
    for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
    {
        m_tasks[i].pS = pSrc + i * (partSize * srcPitch);
        m_tasks[i].pD = pDst + i * (partSize * dstPitch);
        m_tasks[i].srcPitch = srcPitch;
        m_tasks[i].dstPitch = dstPitch;
        m_tasks[i].roi = roi;

        m_tasks[i].EventStart.Signal();
    }

    if (rest != 0)
    {
        roi.height = rest;
    }

    pSrc = pSrc + (m_numThreads - 1) * (partSize * srcPitch);
    pDst = pDst + (m_numThreads - 1) * (partSize * dstPitch);

    {
        MFX_AUTO_LTRACE(MF_TL_PERF, "ippiCopyManaged_8u_C1R");
        ippiCopyManaged_8u_C1R(pSrc, srcPitch, pDst, dstPitch, roi, IPP_NONTEMPORAL_STORE);
    }

    Synchronize();

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)


eFAST_COPY_MODE FastCopy::GetSupportedMode(void) const
{
    return m_mode;

} // eFAST_COPY_MODE FastCopy::GetSupportedMode(void) const

mfxStatus FastCopy::Synchronize(void)
{
    if (m_mode == FAST_COPY_SSE41)
    {
        for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
        {
            m_tasks[i].EventEnd.Wait();
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Synchronize(void)

// thread function
mfxU32 MY_THREAD_CALLCONVENTION FastCopy::CopyByThread(void *arg)
{
    FC_TASK *task = (FC_TASK *)arg;

    // wait to event
    task->EventStart.Wait();

    while (!*task->pbCopyQuit)
    {
        mfxU32 srcPitch = task->srcPitch;
        mfxU32 dstPitch = task->dstPitch;
        IppiSize roi = task->roi;

        Ipp8u *pSrc = task->pS;
        Ipp8u *pDst = task->pD;

        {
            MFX_AUTO_LTRACE(MF_TL_PERF, "ippiCopyManaged_8u_C1R");
            ippiCopyManaged_8u_C1R(pSrc, srcPitch, pDst, dstPitch, roi, IPP_NONTEMPORAL_STORE);
        }

        // done copy
        task->EventEnd.Signal();

        // wait for the next frame
        task->EventStart.Wait();
    }

    return 0;
} // mfxU32 __stdcall FastCopy::CopyByThread(void *arg)