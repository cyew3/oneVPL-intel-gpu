/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_timeout.h"

//max wait timeout for SyncOperation functions and other waiting routines
#if defined(DEBUG) || defined (_DEBUG)
    #define MAX_SYNC_WAIT      300000u // 5 min
#else
    #define MAX_SYNC_WAIT      60000u // 60 sec
#endif

namespace {
    mfxU32 g_TimeoutGeneric = MAX_SYNC_WAIT;
    mfxU32 g_TimeoutFFS = MAX_SYNC_WAIT;
}

template<>
mfxU32 & TimeoutVal<PIPELINE_TIMEOUT_GENERIC>::g_val = g_TimeoutGeneric;

template<>
mfxU32 & TimeoutVal<PIPELINE_TIMEOUT_FFS>::g_val = g_TimeoutFFS;

void SetTimeout(mfxI32 timeout_id, mfxU32 timeout_val)
{
    switch (timeout_id)
    {
    case PIPELINE_TIMEOUT_GENERIC :
        TimeoutVal<PIPELINE_TIMEOUT_GENERIC>::g_val = timeout_val;
        break;
    case PIPELINE_TIMEOUT_FFS :
        TimeoutVal<PIPELINE_TIMEOUT_FFS>::g_val = timeout_val;
        break;
    }
}
