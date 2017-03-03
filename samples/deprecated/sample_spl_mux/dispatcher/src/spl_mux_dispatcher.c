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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
const wchar_t* defaultSplDLLName = L"sample_spl_mux.dll";
const wchar_t* defaultMuxDLLName = L"sample_spl_mux.dll";
#else
#include <dlfcn.h>
const char* defaultSplDLLName = "libsample_spl_mux.so";
const char* defaultMuxDLLName = "libsample_spl_mux.so";
#endif // #if defined(_WIN32) || defined(_WIN64)

#include <mfxsplmux.h>

typedef void (MFX_CDECL * splmuxFunctionPointer)(void);

#undef FUNCTION
#define FUNCTION(spl_or_mux, return_value, func_name, formal_param_list, actual_param_list) #func_name ,

static const char *SplAPIFunc[] =
{
    #include "spl_exposed_functions_has_impl.h"
};

static const char *MuxAPIFunc[] =
{
    #include "mux_exposed_functions_has_impl.h"
};

#undef FUNCTION
#define FUNCTION(spl_or_mux, return_value, func_name, formal_param_list, actual_param_list)\
    typedef mfxStatus (MFX_CDECL  * func_name##_ptr) formal_param_list ;

#include "spl_exposed_functions_has_impl.h"
#include "mux_exposed_functions_has_impl.h"


#undef FUNCTION
#define FUNCTION(spl_or_mux, return_value, func_name, formal_param_list, actual_param_list) e##func_name,


enum eSplFunc
{
    #include "spl_exposed_functions_has_impl.h"
    eSplFuncTotal
};

enum eMuxFunc
{
    #include "mux_exposed_functions_has_impl.h"
    eMuxFuncTotal
};

typedef void * smModuleHandle;

typedef struct {
    splmuxFunctionPointer callTable[eSplFuncTotal];
    smModuleHandle hModule;
    void* spl;
} retMfxSpl;

typedef struct {
    splmuxFunctionPointer callTable[eMuxFuncTotal];
    smModuleHandle hModule;
    void* mux;
} retMfxMux;

#if defined(_WIN32) || defined(_WIN64)

void UnLoadSplMuxModule(smModuleHandle hModule)
{
    if (hModule)
    {
        FreeLibrary(hModule);
        hModule = (smModuleHandle) 0;
    }
}

mfxStatus LoadSplFunctions(retMfxSpl *mfxSpl)
{
    int i;

    mfxSpl->hModule = LoadLibraryExW(defaultSplDLLName,NULL,0);
    if (!mfxSpl->hModule)
    {
        return MFX_ERR_NULL_PTR;
    }

    for (i = 0; i < _countof(SplAPIFunc); i += 1) {
        splmuxFunctionPointer hdl = (splmuxFunctionPointer) GetProcAddress((HMODULE) mfxSpl->hModule, SplAPIFunc[i]);
        if (!hdl)
        {
            UnLoadSplMuxModule(mfxSpl);
            return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            mfxSpl->callTable[i] = hdl;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus LoadMuxFunctions(retMfxMux *mfxMux)
{
    int i;

    mfxMux->hModule = LoadLibraryExW(defaultMuxDLLName,NULL,0);
    if (!mfxMux->hModule)
    {
        return MFX_ERR_NULL_PTR;
    }

    for (i = 0; i < _countof(MuxAPIFunc); i += 1) {
        splmuxFunctionPointer hdl = (splmuxFunctionPointer) GetProcAddress((HMODULE) mfxMux->hModule, MuxAPIFunc[i]);
        if (!hdl)
        {
            UnLoadSplMuxModule(mfxMux);
            return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            mfxMux->callTable[i] = hdl;
        }
    }

    return MFX_ERR_NONE;
}

#else

void UnLoadSplMuxModule(smModuleHandle hModule)
{
    if (hModule)
    {
        dlclose(hModule);
        hModule = (smModuleHandle) 0;
    }
}

mfxStatus LoadSplFunctions(retMfxSpl *mfxSpl)
{
    int i;

    mfxSpl->hModule = dlopen(defaultSplDLLName, RTLD_LAZY);
    if (!mfxSpl->hModule)
    {
        char *error_msg = dlerror();
        if (error_msg)
            printf("dlopen failed: %s\n", error_msg);
        return MFX_ERR_NULL_PTR;
    }

    for (i = 0; i < (sizeof(SplAPIFunc) / sizeof(SplAPIFunc[0])); i += 1) {
        splmuxFunctionPointer hdl = (splmuxFunctionPointer) dlsym(mfxSpl->hModule, SplAPIFunc[i]);
        if (dlerror())
        {
            UnLoadSplMuxModule(mfxSpl->hModule);
            return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            mfxSpl->callTable[i] = hdl;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus LoadMuxFunctions(retMfxMux *mfxMux)
{
    int i;

    mfxMux->hModule = dlopen(defaultMuxDLLName, RTLD_LAZY);
    if (!mfxMux->hModule)
    {
        char *error_msg = dlerror();
        if (error_msg)
            printf("dlopen failed: %s\n", error_msg);
        return MFX_ERR_NULL_PTR;
    }

    for (i = 0; i < (sizeof(MuxAPIFunc) / sizeof(MuxAPIFunc[0])); i += 1) {
        splmuxFunctionPointer hdl = (splmuxFunctionPointer) dlsym(mfxMux->hModule, MuxAPIFunc[i]);
        if (dlerror())
        {
            UnLoadSplMuxModule(mfxMux->hModule);
            return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            mfxMux->callTable[i] = hdl;
        }
    }

    return MFX_ERR_NONE;
}

#endif

////////////////////////////////////

mfxStatus MFXSplitter_Init(mfxDataIO *data_io, mfxSplitter *spl)
{
    retMfxSpl *r = NULL;
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;

    if (spl == NULL)
        return MFX_ERR_NULL_PTR;

    *spl = 0;

    r = (retMfxSpl*) malloc(sizeof(retMfxSpl));
    if (r == NULL)
        return MFX_ERR_MEMORY_ALLOC;
    memset((void*)r, 0, sizeof(retMfxSpl));

    mfxRes = LoadSplFunctions(r);
    if (mfxRes == MFX_ERR_NONE)
    {
        MFXSplitter_Init_ptr pFunc = (MFXSplitter_Init_ptr) r->callTable[eMFXSplitter_Init];
        if (pFunc)
        {
            mfxRes = pFunc(data_io, (mfxSplitter*)&r->spl);
            if (mfxRes == MFX_ERR_NONE)
            {
                *spl = (mfxSplitter)r;
            }
        }
    }
    if (mfxRes != MFX_ERR_NONE)
    {
        free(r);
    }
    return mfxRes;
}

mfxStatus MFXSplitter_Close(mfxSplitter spl)
{
    retMfxSpl* r;
    mfxStatus mfxRes = MFX_ERR_NONE;
    r = (retMfxSpl*) spl;
    if (r != NULL)
    {
        if ((r->hModule))
        {
            MFXSplitter_Close_ptr pFunc = (MFXSplitter_Close_ptr) r->callTable[eMFXSplitter_Close];
            if (pFunc)
            {
                mfxRes =  pFunc (r->spl);
            }
        }
        UnLoadSplMuxModule(r->hModule);
        free(r);
        //TODO: fix double close call
    }
    return mfxRes;
}

////////////////////////////////////

mfxStatus MFXMuxer_Init(mfxStreamParams* par, mfxDataIO *data_io, mfxMuxer *mux)
{
    retMfxMux *r = NULL;
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;

    if (mux == NULL)
        return MFX_ERR_NULL_PTR;

    *mux = 0;

    r = (retMfxMux*) malloc(sizeof(retMfxMux));
    if (r == NULL)
        return MFX_ERR_MEMORY_ALLOC;
    memset((void*)r, 0, sizeof(retMfxMux));

    mfxRes = LoadMuxFunctions(r);
    if (mfxRes == MFX_ERR_NONE)
    {
        MFXMuxer_Init_ptr pFunc = (MFXMuxer_Init_ptr) r->callTable[eMFXMuxer_Init];
        if (pFunc)
        {
            mfxRes = pFunc(par, data_io, (mfxMuxer*)&r->mux);
            if (mfxRes == MFX_ERR_NONE)
            {
                *mux = (mfxMuxer)r;
            }
        }
    }

    if (mfxRes != MFX_ERR_NONE)
    {
        free(r);
    }
    return mfxRes;
}

mfxStatus MFXMuxer_Close(mfxMuxer mux)
{
    retMfxMux* r;
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;
    r = (retMfxMux*) mux;
    if (r == NULL)
        return MFX_ERR_NULL_PTR;

    if ((r->hModule))
    {
        MFXMuxer_Close_ptr pFunc = (MFXMuxer_Close_ptr) r->callTable[eMFXMuxer_Close];
        if (pFunc)
        {
            mfxRes = pFunc(r->mux);
        }
    }
    UnLoadSplMuxModule(r->hModule);
    free(r);
    return mfxRes;
}

#undef FUNCTION
#define FUNCTION(spl_or_mux, return_value, func_name, formal_param_list, actual_param_list) \
    return_value func_name formal_param_list \
{ \
    mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE; \
    if (((spl_or_mux*)_0) == 0) \
        return mfxRes; \
    if (((spl_or_mux*)_0)->hModule) \
    { \
        func_name##_ptr pFunc = (func_name##_ptr) (((spl_or_mux*)_0)->callTable[e##func_name]); \
        if (pFunc) \
        { \
            mfxRes = pFunc actual_param_list; \
        } \
    } \
    return mfxRes; \
}

#include "spl_exposed_functions_list.h"
#include "mux_exposed_functions_list.h"
