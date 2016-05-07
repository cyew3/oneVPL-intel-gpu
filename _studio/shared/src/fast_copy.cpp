/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.

File Name: fast_copy.cpp

\* ****************************************************************************** */

#include "fast_copy.h"
#include "ippi.h"
#include "mfx_trace.h"

IppBool FastCopy::m_bCopyQuit = ippFalse;

FastCopy::FastCopy(void)
{
    m_mode = FAST_COPY_UNSUPPORTED;
    m_pThreads = NULL;
    m_tasks = NULL;

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
        m_numThreads = 1;
    }
    if (MFX_ERR_NONE == sts)
    {
        m_pThreads = new UMC::Thread[m_numThreads];
        m_tasks = new FC_TASK[m_numThreads];

        if (!m_pThreads || !m_tasks) sts = MFX_ERR_MEMORY_ALLOC;
    }
    // initialize events
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        if (UMC::UMC_OK != m_tasks[i].EventStart.Init(0, 0))
        {
            sts = MFX_ERR_UNKNOWN;
        }
        if (UMC::UMC_OK != m_tasks[i].EventEnd.Init(0, 0))
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }
    // run threads
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        if (UMC::UMC_OK != m_pThreads[i].Create(CopyByThread, (void *)(m_tasks + i)))
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
} // mfxStatus FastCopy::Initialize(void)

mfxStatus FastCopy::Release(void)
{
    m_bCopyQuit = ippTrue;

    if ((m_numThreads > 1) && m_tasks && m_pThreads)
    {
        // set event
        for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
        {
            m_tasks[i].EventStart.Set();
            m_pThreads[i].Wait();
        }
    }

    m_numThreads = 0;
    m_mode = FAST_COPY_UNSUPPORTED;

    if (m_pThreads)
    {
        delete [] m_pThreads;
        m_pThreads = NULL;
    }

    if (m_tasks)
    {
        delete [] m_tasks;
        m_tasks = NULL;
    }

    m_bCopyQuit = ippFalse;

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Release(void)

mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy::Copy");

    if (NULL == pDst || NULL == pSrc)
    {
        return MFX_ERR_NULL_PTR;
    }

    mfxU32 partSize = roi.height / m_numThreads;
    mfxU32 rest = roi.height % m_numThreads;

    roi.height = partSize;

    // distribute tasks
    for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
    {
        m_tasks[i].pS = pSrc + i * (partSize * srcPitch);
        m_tasks[i].pD = pDst + i * (partSize * dstPitch);
        m_tasks[i].srcPitch = srcPitch;
        m_tasks[i].dstPitch = dstPitch;
        m_tasks[i].roi = roi;

        m_tasks[i].EventStart.Set();
    }

    if (rest != 0)
    {
        roi.height = rest;
    }

    pSrc = pSrc + (m_numThreads - 1) * (partSize * srcPitch);
    pDst = pDst + (m_numThreads - 1) * (partSize * dstPitch);

    ippiCopyManaged_8u_C1R(pSrc, srcPitch, pDst, dstPitch, roi, 2);

    Synchronize();

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)
#ifndef LINUX64
mfxStatus FastCopy::Copy(mfxU16 *pDst, mfxU32 dstPitch, mfxU16 *pSrc, mfxU32 srcPitch, IppiSize roi, Ipp8u lshift, Ipp8u rshift)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy::Copy");

    if (NULL == pDst || NULL == pSrc)
    {
        return MFX_ERR_NULL_PTR;
    }

    //mfxU32 partSize = roi.height / m_numThreads;
    //mfxU32 rest = roi.height % m_numThreads;

    //roi.height = partSize;
    /*
    // distribute tasks
    for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
    {
        m_tasks[i].pS = pSrc + i * (partSize * srcPitch);
        m_tasks[i].pD = pDst + i * (partSize * dstPitch);
        m_tasks[i].srcPitch = srcPitch;
        m_tasks[i].dstPitch = dstPitch;
        m_tasks[i].roi = roi;

        m_tasks[i].EventStart.Set();
    }

    if (rest != 0)
    {
        roi.height = rest;
    }
    */
//    pSrc = pSrc + (m_numThreads - 1) * (partSize * srcPitch);
//    pDst = pDst + (m_numThreads - 1) * (partSize * dstPitch);

    ippiCopyManaged_8u_C1R((Ipp8u*)pSrc, srcPitch, (Ipp8u*)pDst, dstPitch, roi, 2);
    if(rshift)
        ippiRShiftC_16u_C1IR(rshift, (Ipp16u*)pDst, dstPitch,roi);
    if(lshift)
        ippiLShiftC_16u_C1IR(lshift, (Ipp16u*)pDst, dstPitch,roi);
//    Synchronize();

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)
#endif

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
mfxU32 __STDCALL FastCopy::CopyByThread(void *arg)
{
    FC_TASK *task = (FC_TASK *)arg;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ThreadName=FastCopy");

    // wait to event
    task->EventStart.Wait();

    while (!m_bCopyQuit)
    {
        mfxU32 srcPitch = task->srcPitch;
        mfxU32 dstPitch = task->dstPitch;
        IppiSize roi = task->roi;

        Ipp8u *pSrc = task->pS;
        Ipp8u *pDst = task->pD;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ippiCopyManaged");
            ippiCopyManaged_8u_C1R(pSrc, srcPitch, pDst, dstPitch, roi, 2);
        }

        // done copy
        task->EventEnd.Set();

        // wait for the next frame
        task->EventStart.Wait();
    }

    return 0;

} // mfxU32 __stdcall FastCopy::CopyByThread(void *arg)