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

#ifdef linux

#include <stdlib.h>
#include <dlfcn.h>
#include <string>
#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "functions_table.h"

static const char* g_mfxlibsw = "/opt/intel/mediasdk/lib64/0000/0000/facke/libmfxsw64-p.so.1.8";
static const char* g_mfxlibhw = "libmfxhw32.so";

void __attribute__ ((constructor)) dll_init(void)
{
    Log::SetLogType(LOG_CONSOLE);
    Log::WriteLog(std::string("Init tracer lib \n"));
}

void SetLogType(eLogType type)
{
    Log::SetLogType(type);
}

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{
    Log::WriteLog(std::string("MFXInit \n"));
    if (!session) return MFX_ERR_NULL_PTR;

    mfxLoader* loader = (mfxLoader*)calloc(1, sizeof(mfxLoader));
    if (!loader) return MFX_ERR_MEMORY_ALLOC;

    /* Loading library trying to:
    *  1. load HW library
    *  2. load SW library
    */
    if (impl & MFX_IMPL_HARDWARE) loader->dlhandle = dlopen(g_mfxlibhw, RTLD_NOW);
    if (!loader->dlhandle) {
    if (impl & MFX_IMPL_SOFTWARE) loader->dlhandle = dlopen(g_mfxlibsw, RTLD_NOW);
    }
    if (!loader->dlhandle) return MFX_ERR_NOT_FOUND;

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
        return MFX_ERR_NOT_FOUND;
    }

    /* Initializing loaded library */
    mfxStatus mfx_res = (*(MFXInitPointer)loader->table[eMFXInit])(impl, ver, &(loader->session));
    if (MFX_ERR_NONE != mfx_res) {
        dlclose(loader->dlhandle);
        free(loader);
        return mfx_res;
    }
    *session = (mfxSession)loader;
    return MFX_ERR_NONE;
}

mfxStatus MFXClose(mfxSession session)
{
    Log::WriteLog(std::string("MFXClose \n"));
    mfxLoader* loader = (mfxLoader*)session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxStatus mfx_res = (*(MFXClosePointer)loader->table[eMFXClose])(loader->session);
    dlclose(loader->dlhandle);
    free(loader);

    return mfx_res;
}

#endif