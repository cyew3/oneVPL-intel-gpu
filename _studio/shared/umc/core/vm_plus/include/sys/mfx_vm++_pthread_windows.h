//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VMPLUSPLUS_PTHREAD_WINDOWS_H__
#define __MFX_VMPLUSPLUS_PTHREAD_WINDOWS_H__

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <process.h>

struct MfxMutexHandle
{
    CRITICAL_SECTION m_CritSec;
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif //__MFX_VMPLUSPLUS_PTHREAD_WINDOWS_H__
