// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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