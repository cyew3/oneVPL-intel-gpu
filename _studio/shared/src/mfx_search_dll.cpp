// Copyright (c) 2010-2018 Intel Corporation
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

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_search_dll.h"

#include <windows.h>

mfxStatus RemoveSearchPath(char *pDllSearchPath, size_t pathSize)
{
    DWORD dwRes;
    BOOL bRes;

    dwRes = GetDllDirectoryA((DWORD) pathSize, pDllSearchPath);
    if (dwRes >= pathSize)
    {
        // error happened. Terminate the string, do nothing
        pDllSearchPath[0] = 0;
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // remove the current directory from the default DLL search order
    bRes = SetDllDirectoryA("");
    if (FALSE == bRes)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;

} // mfxStatus RemoveSearchPath(char *pDllSearchPath, size_t pathSize)

void RestoreSearchPath(const char *pDllSearchPath)
{
    SetDllDirectoryA(pDllSearchPath);

} // void RestoreSearchPath(const char *pDllSearchPath)

#endif // defined(_WIN32) || defined(_WIN64)
