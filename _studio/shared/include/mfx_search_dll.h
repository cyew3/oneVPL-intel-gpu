//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2012 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SEARCH_DLL_H)
#define __MFX_SEARCH_DLL_H

#include <mfxdefs.h>

// declare max buffer length for DLL path
enum
{
    MFX_MAX_DLL_PATH            = 1024
};

#if defined(_WIN32) || defined(_WIN64)

mfxStatus RemoveSearchPath(char *pDllSearchPath, size_t pathSize);
void RestoreSearchPath(const char *pDllSearchPath);

#endif // defined(_WIN32) || defined(_WIN64)

#endif // __MFX_SEARCH_DLL_H

