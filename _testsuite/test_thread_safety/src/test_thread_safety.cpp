/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety.cpp

\* ****************************************************************************** */

#include "umc_thread.h"
#include "test_thread_safety.h"
#include "test_thread_safety_cmdline.h"
#include "mfx_pipeline_sync.h"

std::auto_ptr<OutputRegistrator> outReg;

struct ThreadParam
{
    ThreadParam()
        : testType(0), argc(0), argv(0), result(0), pExternalSync() {}

    mfxU32 testType;
    mfxI32 argc;
    vm_char** argv;
    mfxI32 result;
    IPipelineSynhro *pExternalSync;
};

int RunDecode(int argc, vm_char** argv, IPipelineSynhro *pExternalSync);
int RunEncode(int argc, vm_char** argv, IPipelineSynhro *pExternalSync);
int Mpeg2EncPakStarter(int argc, vm_char* argv[]);
int H264EncPakStarter(int argc, vm_char* argv[]);
int VC1EncPakStarter(int argc, vm_char* argv[]);

mfxU32 OpenOutputFiles(vm_file** fdOut, mfxU32 numThread, const vm_char* name, const vm_char* mode);
mfxU32 CompareOutputFiles(vm_file** fdOut, vm_file* fdRef, mfxU32 numThread);
Ipp32u VM_THREAD_CALLCONVENTION ThreadStarter(void *p);

#if defined(_WIN32) || defined(_WIN64)
mfxI32 _tmain(mfxI32 argc, vm_char** argv)
#else
mfxI32 main(mfxI32 argc, vm_char** argv)
#endif
{
    CommandLine cmd  (argc, argv);
    if (!cmd.IsValid())
    {
        vm_file_fprintf(vm_stderr, VM_STRING("FAILED\n"));
        CommandLine::PrintUsage(argv[0]);
        return 1;
    }

    vm_char* refFileName = cmd.GetRefFileName();
    AutoFile fdRef(refFileName ? vm_file_fopen(refFileName, VM_STRING("rb")) : (vm_file *)0);
    if (refFileName && fdRef.Extract() == 0)
    {
        vm_file_fprintf(vm_stderr, VM_STRING("FAILED\n"));
        return 1;
    }

    mfxU32 numThread = cmd.GetNumThread();
    AutoArray<UMC::Thread> thread(new UMC::Thread[numThread]);
    AutoArray<ThreadParam> param(new ThreadParam[numThread]);
    std::auto_ptr<IPipelineSynhro> init_stage_sync (new PipelineSynhro());

    AutoArrayOfFiles fdOut(numThread);
    if (OpenOutputFiles(&fdOut[0], numThread, cmd.GetOutFileName(), VM_STRING("wb")) != 0)
    {
        vm_file_fprintf(vm_stderr, VM_STRING("FAILED\n"));
        return 1;
    }

    outReg.reset(new OutputRegistrator(numThread, fdRef.Extract(), &fdOut[0], cmd.GetSyncOpt()));

    for (mfxU32 i = 0; i < numThread; i++)
    {
        param[i].testType = cmd.GetTestType();
        param[i].argc = cmd.GetArgc();
        param[i].argv = cmd.GetArgv();
        param[i].pExternalSync = init_stage_sync.get();
        thread[i].Create(ThreadStarter, &param[i]);
    }

    mfxI32 result = 0;
    for (mfxU32 i = 0; i < numThread; i++)
    {
        thread[i].Wait();
        if (param[i].result != 0)
            result = 1;
    }

    if (cmd.GetSyncOpt() == SYNC_OPT_NO_SYNC)
    {
        fdOut.CloseAll();
        if (OpenOutputFiles(&fdOut[0], numThread, cmd.GetOutFileName(), VM_STRING("rb")) != 0)
        {
            vm_file_fprintf(vm_stderr, VM_STRING("TEST: Failed to open output file\n"));
            vm_file_fprintf(vm_stderr, VM_STRING("FAILED\n"));
            return 1;
        }

        if (result == 0)
        {
            result = CompareOutputFiles(&fdOut[0], fdRef.Extract(), numThread);

            if (result == 1)
            {
                vm_file_fprintf(vm_stderr, VM_STRING("TEST: Bit exact compare error for output files\n"));
            }
        }
    }

    if (outReg->QueryStatus() || result != 0)
    {
        vm_file_fprintf(vm_stderr, VM_STRING("FAILED\n"));
        return 1;
    }
    else
    {
        vm_file_fprintf(vm_stderr, VM_STRING("OK\n"));
        return 0;
    }
}

mfxU32 OpenOutputFiles(vm_file** fdOut, mfxU32 numThread, const vm_char* name, const vm_char* mode)
{
    if (name)
    {
        vm_char nameA[MAX_PATH + 1];
        size_t len = IPP_MIN(vm_string_strlen(name), MAX_PATH - 4);
        memcpy(nameA, name, len * sizeof(vm_char));

        for (mfxU32 i = 0; i < numThread; i++)
        {
            vm_string_snprintf(nameA + len, 4, VM_STRING(".%02d"), i);
            fdOut[i] = vm_file_fopen(nameA, mode);
            if (!fdOut[i])
                return 1;
        }
    }

    return 0;
}

mfxI64 GetLength(vm_file* file)
{
    mfxI64 pos = vm_file_ftell(file);
    if (vm_file_fseek(file, 0, VM_FILE_SEEK_END) != 0)
    {
        return -1;
    }

    mfxI64 length = (mfxI64)vm_file_ftell(file);
    vm_file_fseek(file, pos, VM_FILE_SEEK_SET);
    return length;
}

mfxU32 CompareOutputFiles(vm_file** fdOut, vm_file* fdRef, mfxU32 numThread)
{
    if (fdOut[0])
    {
        // check length of files
        mfxI64 fileLen = GetLength(fdOut[0]);
        vm_file_fseek(fdOut[0], 0, VM_FILE_SEEK_SET);
        for (mfxU32 i = 1; i < numThread; i++)
        {
            if (!fdOut[i])
            {
                return 1;
            }

            if (GetLength(fdOut[i]) != fileLen)
            {
                return 1;
            }

            vm_file_fseek(fdOut[i], 0, VM_FILE_SEEK_SET);
        }

        const mfxU32 BUF_SIZE = 1024;
        mfxU8 buf0[BUF_SIZE];
        mfxU8 buf1[BUF_SIZE];
        for (mfxI64 off = 0; off < fileLen; off += BUF_SIZE)
        {
            size_t toRead = IPP_MIN(BUF_SIZE, fileLen - off);
            if (vm_file_fread(buf0, 1, toRead, fdOut[0]) != toRead)
                return 1;

            if (fdRef)
            {
                if (vm_file_fread(buf1, 1, toRead, fdRef) != toRead)
                    return 1;
                if (memcmp(buf0, buf1, toRead) != 0)
                    return 1;
            }

            for (mfxU32 i = 1; i < numThread; i++)
            {
                if (vm_file_fread(buf1, 1, toRead, fdOut[i]) != toRead)
                    return 1;
                if (memcmp(buf0, buf1, toRead) != 0)
                    return 1;
            }
        }
    }
    else
    {
        for (mfxU32 i = 1; i < numThread; i++)
            if (fdOut[i])
                return 1;
    }

    return 0;
}

Ipp32u VM_THREAD_CALLCONVENTION ThreadStarter(void *p)
{
    if (!p)
        return 1;

    ThreadParam* param = (ThreadParam *)p;

    try
    {
        switch (param->testType)
        {

        case TEST_JPEGDECODE:
        case TEST_H264DECODE:
        case TEST_MPEG2DECODE:
        case TEST_MVCDECODE:
        case TEST_SVCDECODE:
        case TEST_VC1DECODE:
        case TEST_HEVCDECODE:
        case TEST_VP8DECODE:
        case TEST_VP9DECODE:
        case TEST_H263DECODE:
            param->result = RunDecode(param->argc, param->argv, param->pExternalSync);
            break;
        case TEST_JPEGENCODE:
        case TEST_H264ENCODE:
        case TEST_MPEG2ENCODE:
        case TEST_MVCENCODE:
        case TEST_HEVCENCODE:
        case TEST_VC1ENCODE:
        case TEST_VP8ENCODE:
        case TEST_H263ENCODE:
        case TEST_VP9ENCODE:
            param->result = RunEncode(param->argc, param->argv, param->pExternalSync);
            break;
        default:
            param->result = 1;
        }
    }
    catch (...)
    {
        param->result = 1;
    }

    return param->result;
}
