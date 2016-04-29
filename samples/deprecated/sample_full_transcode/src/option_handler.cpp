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

#include "option_handler.h"
#include "sample_defs.h"
#include <iostream>

using namespace detail;

size_t OptionHandlerBase::Handle(const msdk_string & str) {
    //second attempt to parse this value
    if (m_bHandled) {
        MSDK_TRACE_ERROR( MSDK_STRING("Duplicate option found : ") << str );
        return 0;
    }
    size_t stpos = str.find(GetName());
    if (stpos != 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("not suitable option for this handler: ") << str );
        return 0;
    }
    stpos += GetName().size();
    if (stpos > str.size() || str[stpos] != MSDK_CHAR(' ') || stpos + 1 > str.size() || str[stpos+1]==MSDK_CHAR('-')) {
        MSDK_TRACE_ERROR( MSDK_STRING("option \"")<<GetName()<<MSDK_STRING("\" should have exactly one argument: ") << str );
        return 0;
    }
    stpos ++;
    size_t next_opt_pos = str.find(MSDK_STRING(" -"), stpos);
    m_value = str.substr(stpos, next_opt_pos-stpos);

    //check against delimiters - suitable for non string arguments
    if (m_value.empty() || (!CanHaveDelimiters() && (m_value.find_first_of(MSDK_CHAR(' ')) != msdk_string::npos))) {
        MSDK_TRACE_ERROR( MSDK_STRING("option \"")<<GetName()<<MSDK_STRING("\" should have exactly one argument: ") << str );
        return 0;
    }
    m_bHandled = true;
    return next_opt_pos == msdk_string::npos ? GetName().size() + m_value.size() + 1 : next_opt_pos + 1;
}

size_t ArgHandler<bool>::Handle(const msdk_string & str) {
    //second attempt to parse this value
    m_bHandled = true;
    size_t stpos = str.find(GetName());
    size_t next_opt_pos = str.find(MSDK_STRING(" -"), stpos);
    return next_opt_pos == msdk_string::npos ? GetName().size() + m_value.size() : next_opt_pos + 1;
}