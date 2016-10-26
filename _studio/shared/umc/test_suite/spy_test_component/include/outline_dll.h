//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __OUTLINE_DLL_H__
#define __OUTLINE_DLL_H__

#include "outline.h"
#include "outline_factory.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

OutlineFactoryAbstract * GetOutlineFactory()
{
#if defined(_WIN32) || defined(_WIN64)
    HMODULE hModule;

#ifdef _DEBUG
    hModule = ::LoadLibraryEx(_T("outline_xerces3_d.dll"),NULL,0);
#else
    hModule = ::LoadLibraryEx(_T("outline_xerces3.dll"),NULL,0);
#endif // _DEBUG
    
    if (!hModule)
        return 0;

    FARPROC func = GetProcAddress(hModule, "GetOutlineFactory");

#else // defined(_WIN32) || defined(_WIN64)
    typedef double (*func_ptr)(double);
    func_ptr func;

#ifdef _DEBUG

#if defined(__APPLE__)
    void* lib_handle = dlopen("liboutline_d.dylib", RTLD_LAZY);
#else
    void* lib_handle = dlopen("liboutline_d.so", RTLD_LAZY);
#endif // #if defined(__APPLE__)

#else

#if defined(__APPLE__)
    void* lib_handle = dlopen("liboutline.dylib", RTLD_LAZY);
#else
    void* lib_handle = dlopen("liboutline.so", RTLD_LAZY);
#endif // #if defined(__APPLE__)

#endif // #ifdef _DEBUG
    if(lib_handle == NULL) {
#if defined(_DEBUG)
              printf("LIBOUTLINE!: %s\n",dlerror());
#endif
              return 0;
    }

    func = (func_ptr)dlsym(lib_handle, "GetOutlineFactory");
#endif // defined(_WIN32) || defined(_WIN64)

    typedef OutlineFactoryAbstract * (*GetOutlineFactory)();
    OutlineFactoryAbstract * factory = ((GetOutlineFactory)func)();
    return factory;
}

#endif // __OUTLINE_DLL_H__