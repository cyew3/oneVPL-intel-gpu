/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_output.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_OUTPUT_H__
#define __TEST_THREAD_SAFETY_OUTPUT_H__

#include <memory>
#include <mutex>
#include "umc_event.h"
#include "mfxstructures.h"
#include "test_thread_safety_utils.h"

struct StopThreadException
{
};

class OutputRegistrator
{
public:
    OutputRegistrator(mfxU32 numWriter, vm_file* fdRef, vm_file** fdOut, mfxU32 syncOpt);
    mfxHDL Register();
    mfxI32 CommitData(mfxHDL handle, void* ptr, mfxU32 len);
    mfxI32 UnRegister(mfxHDL handle);
    bool IsRegistered(mfxHDL handle) const;
    mfxU32 QueryStatus() const { return m_compareStatus; }

protected:
    enum { OK = 0, ERR_CMP = 1, ERR_FATAL };
    struct Data { const void* ptr; mfxU32 len; };
    mfxU32 Compare() const;

private:
    std::mutex m_counterMutex;
    UMC::Event m_allIn;
    UMC::Event m_allOut;
    AutoArray<Data> m_data;
    // expected number of writers, writers can be from same or different threads
    const mfxU32 m_numWriter;
    // number of currently registered writers
    mfxU32 m_numRegistered;
    // number of writers that was unregistered;
    // before unregistration, a writer must be registered
    mfxU32 m_numUnregistered;
    mfxU32 m_numCommit;
    mfxU32 m_compareStatus;
    vm_file* m_fdRef;
    vm_file** m_fdOut;
    const mfxU32 m_syncOpt;

    static const mfxU64 HandleBase = 0xff000000;
};

extern std::unique_ptr<OutputRegistrator> outReg;

#endif //__TEST_THREAD_SAFETY_OUTPUT_H__
