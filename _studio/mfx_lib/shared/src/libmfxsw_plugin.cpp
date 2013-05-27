/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: libmfxsw_plugin.cpp

\* ****************************************************************************** */

#include <mfxplugin.h>
#include <mfx_session.h>
#include <mfx_task.h>
#include <mfx_user_plugin.h>
#include <mfx_utils.h>

// static section of the file
namespace
{

VideoUSER *CreateUSERSpecificClass(mfxU32 type)
{
    // touch unreferenced parameter(s)
    type = type;

    return new VideoUSERPlugin();

} // VideoUSER *CreateUSERSpecificClass(mfxU32 type)

} // namespace

mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 type,
                                const mfxPlugin *par)
{
    mfxStatus mfxRes;

    // check error(s)
    if (0 != type)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    try
    {
        // the plugin with the same ID is already exist
        if (session->m_pUSER.get())
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        // create a new plugin's instance
        session->m_pUSER.reset(CreateUSERSpecificClass(type));
        MFX_CHECK(session->m_pUSER.get(), MFX_ERR_INVALID_VIDEO_PARAM);

        // initialize the plugin
        mfxRes = session->m_pUSER->Init(par, &(session->m_coreInt));
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pUSER.get())
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 type,

mfxStatus MFXVideoUSER_Unregister(mfxSession session, mfxU32 type)
{
    mfxStatus mfxRes;

    // check error(s)
    if (0 != type)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pUSER.get(), MFX_ERR_NOT_INITIALIZED);

    try
    {
        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pUSER.get());

        // deinitialize the plugin
        mfxRes = session->m_pUSER->Close();
        // delete the plugin's instance
        session->m_pUSER.reset();
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pUSER.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoUSER_Unregister(mfxSession session, mfxU32 type)

mfxStatus MFXVideoUSER_ProcessFrameAsync(mfxSession session,
                                         const mfxHDL *in, mfxU32 in_num,
                                         const mfxHDL *out, mfxU32 out_num,
                                         mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pUSER.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);
    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pUSER->Check(in, in_num, out, out_num, &task.entryPoint);
        // source data is OK, go forward
        if (MFX_ERR_NONE == mfxRes)
        {
            mfxU32 i;

            task.pOwner = session->m_pUSER.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pUSER->GetThreadingPolicy();
            // fill dependencies
            for (i = 0; i < in_num; i += 1)
            {
                task.pSrc[i] = in[i];
            }
            for (i = 0; i < out_num; i += 1)
            {
                task.pDst[i] = out[i];
            }

            // register input and call the task
            mfxRes = session->m_pScheduler->AddTask(task, &syncPoint);
        }

        // return pointer to synchronization point
        *syncp = syncPoint;
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pUSER.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoUSER_ProcessFrameAsync(mfxSession session,
