/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\* ****************************************************************************** */
#if defined(ANDROID)

#include <iostream>
#include <sstream>
#include <android/log.h>
#include "log_logcat.h"

LogLogcat::LogLogcat()
{
}

LogLogcat::~LogLogcat()
{
}

void LogLogcat::WriteLog(const std::string &log)
{

    std::stringstream str_stream;
    str_stream << log;
    std::string spase = "";
    while(true) {
        spase = "";
        std::string logstr;
        std::stringstream finalstr;
        getline(str_stream, logstr);
        if(log.find("function:") == std::string::npos && log.find(">>") == std::string::npos) spase = "    ";
        if(logstr.length() > 2) {
            finalstr << ThreadInfo::GetThreadId() << " " << Timer::GetTimeStamp() << " " << spase << logstr << "\n";
        } else {
            finalstr << logstr << "\n";
        }
        __android_log_print(ANDROID_LOG_DEBUG, "mfxtracer", "%s", finalstr.str().c_str());
        if(str_stream.eof())
            break;
    }
}

#endif //#if defined(ANDROID)
