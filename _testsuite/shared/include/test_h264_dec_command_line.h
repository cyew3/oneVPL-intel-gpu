/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: test_h264_dec_command_line.h

\* ****************************************************************************** */

#include "vm_strings.h"

class CommandLine
{
public:
    // Default constructor
    CommandLine(void);

    // Constructor
    CommandLine(char argc, const vm_char** argv);

    // Print the list of supported parameters
    void PrintUsage(const vm_char *pAppName);

    // Parse the command line
    void Parse(int argc, const vm_char **argv);

    // Check the status of parameter parsing
    inline
    bool IsValid(void) const { return m_pSrcFile != NULL; }
    // Is output file provided?
    inline
    bool IsOutputNeeded() const { return m_pDstFile != NULL; }
    // Is reference file provided?
    inline
    bool IsCompareNeeded() const { return m_pRefFile != NULL; }

    const vm_char *m_pSrcFile;                                  // (const vm_char *) pointer to source file name
    const vm_char *m_pDstFile;                                  // (const vm_char *) pointer to destination file name
    const vm_char *m_pRefFile;                                  // (const vm_char *) pointer to reference file name

    int m_nIteration;                                           // (int) number of test iterations

protected:

    // Reset all variables
    void Reset(void);
};
