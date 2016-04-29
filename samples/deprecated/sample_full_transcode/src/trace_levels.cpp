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

#include "mfx_samples_config.h"

#include <algorithm>
#include "exceptions.h"
#include <iostream>
#include "sample_defs.h"
#include "trace_levels.h"

std::vector<msdk_string> FillStringLevels();

std::vector<msdk_string> TraceLevels::g_strLevels = FillStringLevels();

std::vector<msdk_string> FillStringLevels() {
    std::vector<msdk_string> v;
    v.push_back(MSDK_STRING("silent"));
    v.push_back(MSDK_STRING("critical"));
    v.push_back(MSDK_STRING("error"));
    v.push_back(MSDK_STRING("warning"));
    v.push_back(MSDK_STRING("info"));
    v.push_back(MSDK_STRING("debug"));
    return v;
}

namespace {
    TraceLevelTmpl<MSDK_TRACE_LEVEL_SILENT>  log_level_silent;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_CRITICAL>  log_level_critical;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_ERROR>  log_level_error;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_WARNING>  log_level_warning;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_INFO>  log_level_info;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_DEBUG>  log_level_debug;
}

TraceLevel & TraceLevel :: FromString(const msdk_string & str) {
    std::vector<msdk_string>::iterator i =
        std::find(TraceLevels::g_strLevels.begin(), TraceLevels::g_strLevels.end(), str);
    if (TraceLevels::g_strLevels.end() == i) {
        MSDK_TRACE_ERROR(MSDK_STRING("Unknown logger level: ") << str);
        throw UnknownLoggerLevelError();
    }
    MsdkTraceLevel idx = (MsdkTraceLevel)(i - TraceLevels::g_strLevels.begin() - 1);
    switch (idx) {
        case MSDK_TRACE_LEVEL_SILENT :
            return log_level_silent;
        case MSDK_TRACE_LEVEL_CRITICAL :
            return log_level_critical;
        case MSDK_TRACE_LEVEL_ERROR :
            return log_level_error;
        case MSDK_TRACE_LEVEL_WARNING :
            return log_level_warning;
        case MSDK_TRACE_LEVEL_INFO :
            return log_level_info;
        case MSDK_TRACE_LEVEL_DEBUG :
            return log_level_debug;

        default:
            return log_level_info;
    }
}
