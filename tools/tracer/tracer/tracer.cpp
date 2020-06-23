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
#include "tracer.h"

void tracer_init()
{
#ifdef ANDROID // temporary hardcode for Android
    Log::SetLogLevel(LOG_LEVEL_DEFAULT);
    Log::SetLogType(LOG_LOGCAT);
#else
    std::string type = Config::GetParam("core", "type");
    if (type == std::string("console")) {
        Log::SetLogType(LOG_CONSOLE);
    } else if (type == std::string("file")) {
        Log::SetLogType(LOG_FILE);
    } else {
        // TODO: what to do with incorrect setting?
        Log::SetLogType(LOG_CONSOLE);
    }

    std::string log_level = Config::GetParam("core", "level");
    if(log_level == std::string("default")){
         Log::SetLogLevel(LOG_LEVEL_DEFAULT);
    }
    else if(log_level == std::string("short")){
        Log::SetLogLevel(LOG_LEVEL_SHORT);
    }
    else if(log_level == std::string("full")){
        Log::SetLogLevel(LOG_LEVEL_FULL);
    }
    else{
        // TODO
        Log::SetLogLevel(LOG_LEVEL_FULL);
    }
#endif
}