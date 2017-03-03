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
#include "sample_utils.h"

class TraceLevels {
public:
    static std::vector<msdk_string> g_strLevels;
};

inline msdk_ostream & operator << (msdk_ostream & os, const TraceLevels &/*levels*/) {
    for (size_t i =0; i < TraceLevels::g_strLevels.size(); i++) {
        os << TraceLevels::g_strLevels[i] << MSDK_CHAR(' ');
    }
    return os;
}

class TraceLevel {
public:
    static TraceLevel & FromString(const msdk_string &);

    virtual MsdkTraceLevel ToTraceLevel() = 0;
    virtual const msdk_string & ToString() = 0;
};


template <MsdkTraceLevel level >
class TraceLevelTmpl  : public TraceLevel{
public:
    virtual MsdkTraceLevel ToTraceLevel() {
        return level;
    }
    virtual const msdk_string & ToString() {
        return TraceLevels::g_strLevels[(size_t)(level + MSDK_TRACE_LEVEL_SILENT)];
    }
};

