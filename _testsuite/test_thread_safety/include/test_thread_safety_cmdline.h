/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_cmdline.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_CMDLINE_H__
#define __TEST_THREAD_SAFETY_CMDLINE_H__

#include "test_thread_safety_utils.h"

class CommandLine
{
public:
    static void PrintUsage(const vm_char* app);

    CommandLine(mfxI32 argc, vm_char** argv);
    bool IsValid() const { return m_valid; }

    mfxU32 GetTestType() const { return m_testType; }
    mfxU32 GetNumThread() const { return m_numThread; }
    mfxU32 GetSyncOpt() const { return m_syncOpt; }
    vm_char** GetArgv() { return &m_argv[0]; }
    mfxI32 GetArgc() const { return m_argc; }
    vm_char* GetRefFileName() const { return m_refFileName; }
    vm_char* GetOutFileName() const { return m_outFileName; }

protected:
    static const vm_char OptO[];
    static const vm_char OptT1[];
    static const vm_char CodecJpeg[];
    static const vm_char CodecH264[];
    static const vm_char CodecMpeg2[];
    static const vm_char CodecVc1[];
    static const vm_char CodecHevc[];
    static const vm_char CodecH263[];
    static const vm_char CodecVP8[];
    static const vm_char CodecVP9[];
private:
    AutoArray<vm_char *> m_argv;
    mfxI32 m_argc;
    mfxU32 m_testType;
    mfxU32 m_numThread;
    mfxU32 m_syncOpt;
    vm_char* m_refFileName;
    vm_char* m_outFileName;
    bool m_valid;
};

#endif //__TEST_THREAD_SAFETY_CMDLINE_H__
