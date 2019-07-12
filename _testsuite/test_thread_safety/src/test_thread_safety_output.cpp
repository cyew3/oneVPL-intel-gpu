/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_output.cpp

\* ****************************************************************************** */

#include <stdexcept> // std::invalid_argument()
#include "test_thread_safety.h"
#include "test_thread_safety_output.h"

OutputRegistrator::OutputRegistrator(mfxU32 numWriter, vm_file* fdRef, vm_file** fdOut, mfxU32 syncOpt)
: m_counterMutex()
, m_allIn()
, m_allOut()
, m_data(new Data[numWriter])
, m_numWriter(numWriter)
, m_numRegistered(0)
, m_numUnregistered(0)
, m_numCommit(0)
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
    m_allIn.Init(1, 0);
    m_allOut.Init(1, 0);

    memset(&m_data[0], 0, numWriter * sizeof(m_data[0]));
}

mfxHDL OutputRegistrator::Register()
{
    std::lock_guard<std::mutex> guard(m_counterMutex);
    if (m_numRegistered == m_numWriter)
        return 0;

    m_numRegistered++;
    if (m_numRegistered == m_numWriter)
        m_allOut.Set(); // allow commits

    return (mfxHDL)(mfxU64)(m_numRegistered + HandleBase);
}

mfxI32 OutputRegistrator::UnRegister(mfxHDL handle)
{
    return CommitData(handle, nullptr, 0);
}

mfxI32 OutputRegistrator::CommitData(mfxHDL handle, void* ptr, mfxU32 len)
{
    if (!IsRegistered(handle))
    {
        vm_file_fprintf(vm_stderr, VM_STRING("TEST: invalid handle %p is out of range [%p; %p]\n"),
                        handle, (mfxHDL)(HandleBase + 1), (mfxHDL)(HandleBase + m_numWriter));
        return -1;
    }
    if (m_syncOpt == SYNC_OPT_PER_WRITE)
    {
        m_allOut.Wait();

        bool last = false;

        {
            std::lock_guard<std::mutex> guard(m_counterMutex);
            if (m_numCommit >= m_numWriter)
            {
                vm_file_fprintf(vm_stderr, VM_STRING("TEST: attempt of invalid commit with "
                                                     "handle %p: commit out of bound\n"), handle);
                return -1;
            }
            m_data[m_numCommit].ptr = ptr;
            m_data[m_numCommit].len = len;
            m_numCommit++;
            last = (m_numCommit >= m_numRegistered);
        }

        if (last)
        {
            if (m_compareStatus == OK)
                m_compareStatus = Compare();
            memset(&m_data[0], 0, m_numWriter * sizeof(m_data[0]));
            m_allOut.Reset();
            m_allIn.Set();
        }
        else
        {
            m_allIn.Wait();
        }

        {
            std::unique_lock<std::mutex> guard(m_counterMutex);
            m_numCommit--;
            last = (m_numCommit == 0);
        }

        if (last)
        {
            m_allIn.Reset();
            m_allOut.Set();
        }
    }

    {
        std::unique_lock<std::mutex> guard(m_counterMutex);
        if (ptr == 0)
        {
            // came here from UnRegister
            if (m_numRegistered <= m_numUnregistered)
            {
                vm_file_fprintf(vm_stderr, VM_STRING(
                                    "TEST: %u writers registred, while %u unregistred"
                                    " already. Can't unregister another one.\n"),
                                m_numRegistered, m_numUnregistered);
                return -1;
            }

            m_numUnregistered++;
            if (m_numRegistered == m_numUnregistered)
            {
                // last writer unregistered, restore initial state
                m_numUnregistered = 0;
                m_numRegistered = 0;
            }
        }
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

bool OutputRegistrator::IsRegistered(mfxHDL handle) const
{
    // a <= x <= b is equivalent to x - a <= b - a for unsigned comparison,
    // see Warren, Hacker's Delight (2nd Edition), 4 - 1
    return  (mfxU64)handle > HandleBase && (mfxU64)handle - (HandleBase + 1) <= m_numWriter + HandleBase - (HandleBase + 1);
}
