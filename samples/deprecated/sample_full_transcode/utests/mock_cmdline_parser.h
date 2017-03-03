/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "cmdline_parser.h"
#include <gmock/gmock.h>

class MockHandler : public OptionHandler {
public:
    MOCK_CONST_METHOD2(PrintHelp, void(msdk_ostream & os, size_t));
    MOCK_METHOD1(Handle, size_t(const msdk_string & ));
    MOCK_CONST_METHOD0(GetName, msdk_string());
    MOCK_CONST_METHOD1(GetValue, msdk_string(int));
    MOCK_CONST_METHOD0(Exist, bool ());
    MOCK_CONST_METHOD0(CanHaveDelimiters, bool ());
};

class MockCmdLineParser : public CmdLineParser {
public:
    MockCmdLineParser() :
        CmdLineParser(MSDK_STRING(""),MSDK_STRING("")) {

    }
    MOCK_METHOD1(RegisterOption, void (const OptionHandler &));
    MOCK_METHOD1(Parse, bool (const msdk_string & params));
    MOCK_CONST_METHOD1(at, const OptionHandler & (const msdk_string & option));
    MOCK_CONST_METHOD2(PrintHelp, void (std::ostream &, size_t ));
    MOCK_CONST_METHOD1(IsPresent, bool (const msdk_string & option));
};
