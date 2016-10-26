//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2013 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_vm++_pthread.h"

MfxMutex::MfxMutex(void)
{
    InitializeCriticalSection(&m_handle.m_CritSec);
}

MfxMutex::~MfxMutex(void)
{
    DeleteCriticalSection(&m_handle.m_CritSec);
}

mfxStatus MfxMutex::Lock(void)
{
    EnterCriticalSection(&m_handle.m_CritSec);
    return MFX_ERR_NONE;
}

mfxStatus MfxMutex::Unlock(void)
{
    LeaveCriticalSection(&m_handle.m_CritSec);
    return MFX_ERR_NONE;
}

bool MfxMutex::TryLock(void)
{
    return (TryEnterCriticalSection(&m_handle.m_CritSec))? true: false;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
