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

#ifndef __UMC_MODULE_CONTEXT_H__
#define __UMC_MODULE_CONTEXT_H__

#include "umc_dynamic_cast.h"
#include "umc_defs.h"
#if defined(_WIN32) || defined(_WIN32_WCE)
#include <windows.h>
#endif // defined(_WIN32) || defined(_WIN32_WCE)

namespace UMC
{

class ModuleContext
{
    DYNAMIC_CAST_DECL_BASE(ModuleContext)
public:

    virtual ~ModuleContext(){}
};

#if defined(_WIN32) || defined(_WIN32_WCE)

class HWNDModuleContext : public ModuleContext
{
    DYNAMIC_CAST_DECL(HWNDModuleContext, ModuleContext)

public:
    HWND m_hWnd;
    COLORREF m_ColorKey;
};

#endif // defined(_WIN32) || defined(_WIN32_WCE)

class LocalReaderContext : public ModuleContext
{
    DYNAMIC_CAST_DECL(LocalReaderContext, ModuleContext)

public:
    LocalReaderContext()
    {
        memset(m_szFileName, 0, UMC::MAXIMUM_PATH);
    }

    vm_char m_szFileName[UMC::MAXIMUM_PATH];
};

class RemoteReaderContext : public ModuleContext
{
    DYNAMIC_CAST_DECL(RemoteReaderContext, ModuleContext)

public:
    RemoteReaderContext()
    {
        memset(m_szServerName, 0, UMC::MAXIMUM_PATH);
        m_uiPortNumber = 0;
        m_transmissionMode = 0;
    }

    uint32_t m_uiPortNumber;        // listening port number
    uint32_t m_transmissionMode;    // choosed type of transmission;

    vm_char m_szServerName[UMC::MAXIMUM_PATH];     // IP addres string
};

}  //  namespace UMC

#endif // __UMC_MODULE_CONTEXT_H__

