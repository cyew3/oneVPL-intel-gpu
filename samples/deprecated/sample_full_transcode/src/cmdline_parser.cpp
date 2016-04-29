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

#include "cmdline_parser.h"
#include "sample_defs.h"
#include <functional>
#include <iostream>

bool CmdLineParser::Parse(const msdk_string & params) {
    msdk_string str_remained(params);
    msdk_string str_head;
    bool bHandlerFound = true;

    for (; bHandlerFound && !str_remained.empty(); )
    {
        size_t delimiter_pos = str_remained.find_first_of(MSDK_CHAR(' '));
        str_head = str_remained.substr(0, delimiter_pos);

        bHandlerFound = false;
        for (std::vector <OptionHandler*>::iterator i = m_options.begin(); i != m_options.end(); i++) {
            if ((*i)->GetName() == str_head) {
                m_handled[str_head] = *i;

                size_t nAccepted = (*i)->Handle(str_remained);

                if (!nAccepted) {
                    return false;
                }
                str_remained = str_remained.substr(nAccepted);
                bHandlerFound = true;
                break;
            }
        }
        if (!bHandlerFound) {
            MSDK_TRACE_ERROR(MSDK_STRING("Unknown option: ") << str_head);
        }
    }

    return bHandlerFound;
}

//returns handler for particular option
const OptionHandler & CmdLineParser::at(const msdk_string & option_name) const {

    if (!IsPresent(option_name)) {
        if (!IsRegistered(option_name)) {
            MSDK_TRACE_ERROR( MSDK_STRING("CommandLineParser - option: ")<<option_name<<MSDK_STRING(" not registered"));
            throw NonRegisteredOptionError();
        }
        MSDK_TRACE_ERROR( MSDK_STRING("CommandLineParser - option: ")<<option_name<<MSDK_STRING(" not parsed"));
        throw NonParsedOptionError();
    }
    return *(m_handled.find(option_name)->second);
}
