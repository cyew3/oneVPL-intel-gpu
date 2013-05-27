/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: test_h264_dec_command_line.cpp

\* ****************************************************************************** */

#include "test_h264_dec_command_line.h"

CommandLine::CommandLine(void)
{
    Reset();

} // CommandLine::CommandLine(void)

CommandLine::CommandLine(char argc, const vm_char** argv)
{
    Parse(argc, argv);

} // CommandLine::CommandLine(char argc, const vm_char** argv)

void CommandLine::PrintUsage(const vm_char *pAppName)
{
    vm_string_printf(VM_STRING("%s -i <mediaFile> [-o <outYuvFlle>] [-r <refYuvFlle>] [others flags]\n"), pAppName);
    vm_string_printf(VM_STRING("where others flags are:\n"));
    vm_string_printf(VM_STRING("-n : Specifies the number of test operations. Default value is 1. (ex. -n5)\n"));
    vm_string_printf(VM_STRING("-s : Show status messages.\n"));

} // void CommandLine::PrintUsage(const vm_char *pAppName)

inline
const vm_char *GetParameter(int argc, const vm_char **argv, int &curParam)
{
    // the parameter value follows the parameter
    if (argv[curParam][2])
    {
        return (argv[curParam] + 2);
    }
    // the parameter value is a separate entry in parameter list
    else if (curParam + 1 < argc)
    {
        curParam += 1;
        return argv[curParam];
    }
    // no more parameters
    else
    {
        return (const vm_char *) 0;
    }

} // const vm_char *GetParameter(int argc, const vm_char **argv, int curParam)

void CommandLine::Parse(int argc, const vm_char **argv)
{
    int i;

    // reset variables before the parsing
    Reset();

    // run through provded parameters
    for (i = 1; i < argc; i++)
    {
        if (VM_STRING('-') == argv[i][0])
        {
            switch (argv[i][1])
            {
                // parse the source file name
            case VM_STRING('i'):
                m_pSrcFile = GetParameter(argc, argv, i);
                break;

                // parse the destination file name
            case VM_STRING('o'):
                m_pDstFile = GetParameter(argc, argv, i);
                break;

                // parse the destination file name
            case VM_STRING('r'):
                m_pRefFile = GetParameter(argc, argv, i);
                break;

                // parse the number of iterations
            case VM_STRING('n'):
                m_nIteration = vm_string_atoi(GetParameter(argc, argv, i));
                m_nIteration = IPP_MAX(m_nIteration, 1);
                break;

                // unknown parameter
            default:
                break;
            }
        }
    }

} // void CommandLine::Parse(int argc, const vm_char **argv)

void CommandLine::Reset(void)
{
    m_pSrcFile = m_pDstFile = m_pRefFile = (const vm_char *) 0;
    m_nIteration = 1;

} // void CommandLine::Reset(void)
