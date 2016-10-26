//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VMPLUSPLUS_PTHREAD_UNIX_H__
#define __MFX_VMPLUSPLUS_PTHREAD_UNIX_H__

#if !defined(_WIN32) && !defined(_WIN64)

#include <pthread.h>

struct MfxMutexHandle
{
    pthread_mutex_t m_mutex;
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif //__MFX_VMPLUSPLUS_PTHREAD_UNIX_H__
