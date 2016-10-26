//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include <mfxvideo.h>
#include <mfx_session.h>
#include <mfx_trace.h>
#include <mfx_utils.h>

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFX_SyncOperation");
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, wait);

    try
    {
        // call the function
        mfxRes = session->m_pScheduler->Synchronize(syncp, wait);

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_ABORTED;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (NULL == syncp)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);

    return mfxRes;

} // mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)