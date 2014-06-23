/*********************************************************************************

Copyright (C) 2012-2013 Intel Corporation.  All rights reserved.

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

*********************************************************************************/

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdlib.h>
#include <dlfcn.h>

#include <string>

#include "mfxvideo.h"
#include "../config/config.h"
#include "../dumps/dump.h"
#include "../loggers/log.h"
#include "../loggers/timer.h"
#include "functions_table.h"
#include <exception>
#include <iostream>

static const char* g_mfxlib = NULL;

void __attribute__ ((constructor)) dll_init(void)
{
#ifdef ANDROID // temporary hardcode for Android
    Log::SetLogLevel(LOG_LEVEL_DEFAULT);
    Log::SetLogType(LOG_LOGCAT);
    Log::WriteLog("mfx_tracer: dll_init: +");
    g_mfxlib = "/system/lib/libmfxhw32.so";
    Log::WriteLog("mfx_tracer: dll_init: -");
#else
    try{
        Timer t;
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

        Log::WriteLog("mfx_tracer: dll_init: +");

        std::string lib = Config::GetParam("core", "lib");
        g_mfxlib = lib.c_str();

        Log::WriteLog("mfx_tracer: lib=" + lib);

        Log::WriteLog("mfx_tracer: dll_init: - " + TimeToString(t.GetTime()) + " \n\n");
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
#endif
}

void SetLogType(eLogType type)
{
    Log::SetLogType(type);
}

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{
    try{
        Log::WriteLog(std::string("function: MFXInit(mfxIMPL impl=" + ToString(impl) + ", mfxVersion *ver=" + ToString(ver) + ", mfxSession *session=" + ToString(session) + ") +"));
        if (!session) {
            Log::WriteLog(dump_mfxVersion("ver", ver));
            Log::WriteLog(dump_mfxSession("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NULL_PTR));
            return MFX_ERR_NULL_PTR;
        }

        mfxLoader* loader = (mfxLoader*)calloc(1, sizeof(mfxLoader));
        if (!loader) {
            Log::WriteLog(dump_mfxVersion("ver", ver));
            Log::WriteLog(dump_mfxSession("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_MEMORY_ALLOC));
            return MFX_ERR_MEMORY_ALLOC;
        }
        loader->dlhandle = dlopen(g_mfxlib, RTLD_NOW);
        if (!loader->dlhandle){
            Log::WriteLog(dump_mfxVersion("ver", ver));
            Log::WriteLog(dump_mfxSession("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }
        /* Loading functions table */
        int i;
        mfxFunctionPointer proc;
        for (i = 0; i < eFunctionsNum; ++i) {
            proc = (mfxFunctionPointer)dlsym(loader->dlhandle, g_mfxFuncTable[i].name);
            /* NOTE: on Android very first call to dlsym may fail */
            if (!proc) proc = (mfxFunctionPointer)dlsym(loader->dlhandle, g_mfxFuncTable[i].name);
            if (!proc) break;
            loader->table[i] = proc;
        }
        if (i < eFunctionsNum) {
            dlclose(loader->dlhandle);
            free(loader);
            Log::WriteLog(dump_mfxVersion("ver", ver));
            Log::WriteLog(dump_mfxSession("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }

        Log::WriteLog(dump_mfxIMPL("impl", impl));
        Log::WriteLog(dump_mfxVersion("ver", ver));
        Log::WriteLog(dump_mfxSession("session", loader->session));
        /* Initializing loaded library */
        Timer t;
        mfxStatus mfx_res = (*(MFXInitPointer)loader->table[eMFXInit])(impl, ver, &(loader->session));
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXInit called");
        if (MFX_ERR_NONE != mfx_res) {
            dlclose(loader->dlhandle);
            free(loader);
            Log::WriteLog(dump_mfxVersion("ver", ver));
            Log::WriteLog(dump_mfxSession("session", *session));
            Log::WriteLog(dump_mfxStatus("status", mfx_res));
            return mfx_res;
        }
        *session = (mfxSession)loader;
        Log::WriteLog(dump_mfxIMPL("impl", impl));
        Log::WriteLog(dump_mfxVersion("ver", ver));
        Log::WriteLog(dump_mfxSession("session", loader->session));
        Log::WriteLog(std::string("function: MFXInit(" + elapsed + ", " + dump_mfxStatus("status", mfx_res) + ") - \n\n"));
        return MFX_ERR_NONE;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXClose(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXClose(mfxSession session=" + ToString(session) + ") +");
        mfxLoader* loader = (mfxLoader*)session;

        if (!loader){
            Log::WriteLog(dump_mfxSession("session", session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_INVALID_HANDLE));
            return MFX_ERR_INVALID_HANDLE;
        }
        Log::WriteLog(dump_mfxSession("session", session));
        Timer t;
        mfxStatus mfx_res = (*(MFXClosePointer)loader->table[eMFXClose])(loader->session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXClose called");
        dlclose(loader->dlhandle);
        free(loader);
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXClose(" + elapsed + ", " + dump_mfxStatus("status", mfx_res) + ") - \n\n");
        return mfx_res;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

#endif // #if !defined(_WIN32) && !defined(_WIN64)
