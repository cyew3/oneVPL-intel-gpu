/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_cmdline.cpp

\* ****************************************************************************** */

#include "vm_sys_info.h"
#include "test_thread_safety.h"
#include "test_thread_safety_cmdline.h"

const vm_char CommandLine::OptO[] = VM_STRING("-o\0 default_output_file_name_for_output.1234567890");
const vm_char CommandLine::OptT1[] = VM_STRING("-t\0 1");
const vm_char CommandLine::CodecJpeg[] = VM_STRING("-jpeg");
const vm_char CommandLine::CodecH264[] = VM_STRING("-h264");
const vm_char CommandLine::CodecMpeg2[] = VM_STRING("-m2");
const vm_char CommandLine::CodecVc1[] = VM_STRING("-vc1");
const vm_char CommandLine::CodecHevc[] = VM_STRING("-h265");
const vm_char CommandLine::CodecH263[] = VM_STRING("-h263");
const vm_char CommandLine::CodecVP8[] = VM_STRING("-vp8");
const mfxU32 MAX_NUM_THREAD = 100;

#define MAKE_PAIR(name)\
{\
VM_STRING(#name), TEST_##name\
}

mfxU32 String2TestType(const vm_char* s)
{
    static struct _unnamed01{
        vm_char name[64];
        mfxU32 id;
    } TestTypeStrings [] =
    {
        MAKE_PAIR(JPEGDECODE),
        MAKE_PAIR(H264DECODE),
        MAKE_PAIR(MPEG2DECODE),
        MAKE_PAIR(MVCDECODE),
        MAKE_PAIR(SVCDECODE),
        MAKE_PAIR(HEVCDECODE),
        MAKE_PAIR(HEVCENCODE),
        MAKE_PAIR(VC1DECODE),
        MAKE_PAIR(VP8DECODE),
        MAKE_PAIR(VP9DECODE),
        MAKE_PAIR(H263DECODE),
        MAKE_PAIR(JPEGENCODE),
        MAKE_PAIR(H264ENCODE),
        MAKE_PAIR(MPEG2ENCODE),
        MAKE_PAIR(MVCENCODE),
        MAKE_PAIR(VC1ENCODE),
        MAKE_PAIR(VP8ENCODE),
        MAKE_PAIR(H263ENCODE)
    };

    mfxU32 i;
    for (i = 0; i < sizeof(TestTypeStrings) / sizeof(TestTypeStrings[0]); i++)
    {
        if (vm_string_strcmp(s, TestTypeStrings[i].name) == 0)
        {
            return TestTypeStrings[i].id;
        }
    }
    return TEST_UNDEF;
}

void CommandLine::PrintUsage(const vm_char* app)
{
    vm_string_printf(
        VM_STRING("%s -testType <mfxComponent> ")
        VM_STRING("-testNum <numThread> ")
        VM_STRING("-syncOpt <syncOption> ")
        VM_STRING("<any mfx_player params for DECODE or mfx_transcoder params for ENCODE>\n"), app);
    vm_string_printf(
        VM_STRING("mfxComponent:\n")
        VM_STRING("  JPEGDECODE\n")
        VM_STRING("  JPEGENCODE\n")
        VM_STRING("  H264DECODE\n")
        VM_STRING("  H264ENCODE\n")
        VM_STRING("  MPEG2DECODE\n")
        VM_STRING("  MPEG2ENCODE\n")
        VM_STRING("  MVCDECODE\n")
        VM_STRING("  MVCENCODE\n")
        VM_STRING("  SVCDECODE\n")
        VM_STRING("  HEVCDECODE\n")
        VM_STRING("  HEVCENCODE\n")
        VM_STRING("  VC1DECODE\n")
        VM_STRING("  VC1ENCODE\n")
        VM_STRING("  VP8DECODE\n")
        VM_STRING("  VP8ENCODE\n")
        VM_STRING("  H263DECODE\n")
        VM_STRING("  H263ENCODE\n")
        VM_STRING("  VP9DECODE\n"));
    vm_string_printf(
        VM_STRING("numThread:\n")
        VM_STRING("  n - number of components working in parallel\n")
        VM_STRING("  0 - auto (number of cores)\n"));
    vm_string_printf(
        VM_STRING("syncOption:\n")
        VM_STRING("  0 - no syncronization between concurrent threads\n")
        VM_STRING("  1 - syncronization at every file-write operation\n"));
}

CommandLine::CommandLine(mfxI32 argc, vm_char** argv)
: m_argv(new vm_char*[argc + 5])
, m_argc(0)
, m_testType(TEST_UNDEF)
, m_numThread(0)
, m_syncOpt(SYNC_OPT_NO_SYNC)
, m_refFileName(0)
, m_outFileName(0)
, m_valid(false)
{
    if (argc < 3)
    {
        VM_ASSERT(!"too few parameters");
        return;
    }

    memset(&m_argv[0], 0, sizeof(vm_char *) * (argc + 5));

    // copy executable's name
    m_argv[m_argc++] = argv[0];

    for (mfxI32 i = 1; i < argc; i++)
    {
        // screen options: -testType, -testNum, -syncOpt, -cmp, -o
        if (vm_string_strcmp(argv[i], VM_STRING("-testType")) == 0)
        {
            if (++i < argc)
            {
                m_testType = String2TestType(argv[i]);
            }
        }
        else if (vm_string_strcmp(argv[i], VM_STRING("-testNum")) == 0)
        {
            if (++i < argc)
            {
                m_numThread = IPP_MAX(0, IPP_MIN(vm_string_atoi(argv[i]), (int)MAX_NUM_THREAD));
                if (m_numThread == 0)
                {
                    m_numThread = vm_sys_info_get_cpu_num();
                }
            }
        }
        else if (vm_string_strcmp(argv[i], VM_STRING("-syncOpt")) == 0)
        {
            if (++i < argc)
            {
                m_syncOpt = vm_string_atoi(argv[i]);
            }
        }
        else if (vm_string_strcmp(argv[i], VM_STRING("-cmp")) == 0)
        {
            if (++i < argc)
            {
                m_refFileName = argv[i];
            }
        }
        else if (vm_string_strcmp(argv[i], VM_STRING("-o")) == 0)
        {
            if (++i < argc)
            {
                // save output file name
                // actually there will be as many output files as threads run
                // names will be <outFileName>.<numThread>
                m_outFileName = argv[i];
            }
        }
        else if (vm_string_strncmp(argv[i], VM_STRING("-o:"), 3) == 0)
        {
            m_argv[m_argc++] = argv[i];
            if (++i < argc)
            {
                // save output file name
                // actually there will be as many output files as threads run
                // names will be <outFileName>.<numThread>
                m_outFileName = argv[i];
            }
        }
        else
        {
            // copy option
            m_argv[m_argc++] = argv[i];
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                m_argv[m_argc++] = argv[++i];
            }
        }
    }

    switch (m_testType)
    {
        case TEST_JPEGENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecJpeg);
            break;
        case TEST_H264ENCODE:
        case TEST_MVCENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecH264);
            break;
        case TEST_MPEG2ENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecMpeg2);
            break;
        case TEST_VC1ENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecVc1);
            break;
        case TEST_HEVCENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecHevc);
            break;
        case TEST_H263ENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecH263);
            break;
        case TEST_VP8ENCODE:
            m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::CodecVP8);
            break;
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
            break;
        default:
            VM_ASSERT(!"bad test type");
            return;
    }

    m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::OptT1);
    m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::OptT1) + 4;

    m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::OptO);
    m_argv[m_argc++] = const_cast<vm_char *>(CommandLine::OptO) + 4;

    // validate parameters
    m_valid = true;
}
