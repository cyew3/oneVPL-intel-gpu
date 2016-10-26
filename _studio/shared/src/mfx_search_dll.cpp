//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2012 Intel Corporation. All Rights Reserved.
//

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
