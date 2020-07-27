/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_output.cpp

\* ****************************************************************************** */

#include <stdexcept> // std::invalid_argument()
#include "test_thread_safety.h"
#include "test_thread_safety_output.h"

OutputRegistrator::OutputRegistrator(mfxU32 numWriter, vm_file* fdRef, vm_file** fdOut, mfxU32 syncOpt)
: m_counterMutex()
, m_data(new Data[numWriter])
, m_numWriter(numWriter)
, m_numRegistered(0)
, m_isLastRegistered(false)
, m_compareStatus(OK)
, m_fdRef(fdRef)
, m_fdOut(fdOut)
, m_syncOpt(syncOpt)
{
    if (numWriter + HandleBase < 0)
    {
        vm_file_fprintf(vm_stderr, VM_STRING("TEST: numWriter %u is too big for HandleBase %llu\n"),
            numWriter, HandleBase);
        throw std::invalid_argument("numWriter is too big");
    }
    memset(&m_data[0], 0, numWriter * sizeof(m_data[0]));
}

mfxHDL OutputRegistrator::Register()
{
    std::unique_lock<std::mutex> guard(m_counterMutex);
    m_isLastRegistered = false;
    if (m_numRegistered == m_numWriter)
        return 0;

    m_numRegistered++;
    mfxHDL m_handle = (mfxHDL)(mfxU64)(HandleBase + m_numRegistered);

    if (m_syncOpt == SYNC_OPT_PER_WRITE) {
        if (m_numRegistered != m_numWriter)
            m_condVar.wait(guard, [this] { return m_isLastRegistered; });
        else {
            m_isLastRegistered = true;
            m_condVar.notify_all();
        }
    }
    return m_handle;
}

mfxI32 OutputRegistrator::UnRegister(mfxHDL handle)
{
    return CommitData(handle, nullptr, 0);
}

mfxI32 OutputRegistrator::CommitData(mfxHDL handle, void* ptr, mfxU32 len)
{
    if (m_syncOpt == SYNC_OPT_PER_WRITE)
    {
        std::unique_lock<std::mutex> guard(m_counterMutex);
        m_numRegistered--;
        m_isLastRegistered = false;
        m_data[m_numRegistered].ptr = ptr;
        m_data[m_numRegistered].len = len;

        if (m_numRegistered != 0)
            m_condVar.wait(guard, [this]{ return m_isLastRegistered; });    // wait until all threads encode current frame
        else
        {
            if (m_compareStatus == OK)
                m_compareStatus = Compare();
            memset(&m_data[0], 0, m_numWriter * sizeof(m_data[0]));
            m_isLastRegistered = true;
            m_condVar.notify_all();    // all threads encode current frame
        }

        if (m_numRegistered >= m_numWriter)
            VM_ASSERT(!"the number of registered threads is greater than the number of available threads");

        if (ptr == 0)    // If thread was unregistered (e.g. due to gpu hang) there is no need to wait this thread
            vm_file_fprintf(vm_stderr, VM_STRING("TEST: handle %p unregistered.\n"), handle);
        else
            m_numRegistered++;
    }

    mfxU32 intHdl = (mfxU32)((mfxU64)handle - HandleBase);
    if (m_fdOut[intHdl - 1] && ptr)
    {
        if (vm_file_write(ptr, 1, len, m_fdOut[intHdl - 1]) != len)
            return -1;
        //flushfilebuffers is very slow for mapped memory, decided not to use that
        //fflush((FILE*)m_fdOut[intHdl - 1]->fd);
    }

    return 0;
}

mfxU32 OutputRegistrator::Compare() const
{
    for (mfxU32 i = 1; i < m_numWriter; i++)
    {
        if (m_data[0].len != m_data[i].len)
            return ERR_CMP;
        if ((m_data[0].ptr == 0) != (m_data[i].ptr == 0))
            return ERR_CMP;
        if ((m_data[0].ptr != 0) && memcmp(m_data[0].ptr, m_data[i].ptr, m_data[0].len) != 0)
            return ERR_CMP;
    }

    if (m_fdRef && m_data[0].ptr)
    {
        const mfxU32 BUF_SIZE = 100;
        mfxU8 buf[BUF_SIZE];
        for (mfxU32 i = 0; i < m_data[0].len; i += BUF_SIZE)
        {
            size_t toCmp = IPP_MIN(BUF_SIZE, m_data[0].len - i);
            size_t read = vm_file_fread(buf, 1, toCmp, m_fdRef);
            if (read != toCmp)
                return ERR_CMP;
            if (memcmp((mfxU8 *)m_data[0].ptr + i, buf, toCmp) != 0)
                return ERR_CMP;
        }
    }

    return OK;
}
