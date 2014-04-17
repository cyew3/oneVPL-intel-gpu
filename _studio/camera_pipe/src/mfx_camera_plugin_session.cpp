/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_session.cpp

\* ****************************************************************************** */
#if 0

#include "mfx_session.h"

mfxStatus _mfxSession::RestoreScheduler(void)
{
    if(m_pSchedulerAllocated)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    // leave the current scheduler
    if (m_pScheduler)
    {
        m_pScheduler->Release();
        m_pScheduler = NULL;
    }

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

mfxStatus _mfxSession::ReleaseScheduler(void)
{
    if(m_pScheduler)
        m_pScheduler->Release();
    
    if(m_pSchedulerAllocated)
    m_pSchedulerAllocated->Release();

    m_pScheduler = NULL;
    m_pSchedulerAllocated = NULL;
    
    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)


mfxStatus MFXInternalPseudoJoinSession(mfxSession session, mfxSession child_session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    //MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(child_session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(child_session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    try
    {
 

        //  release  the child scheduler
        mfxRes = child_session->ReleaseScheduler();
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        child_session->m_pScheduler = session->m_pScheduler;


        child_session->m_pCORE.reset(session->m_pCORE.get(), false);
        child_session->m_pOperatorCore = session->m_pOperatorCore;
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        // check errors()
        if ((NULL == session) ||
            (NULL == child_session))
        {
            return MFX_ERR_INVALID_HANDLE;
        }

        if ((NULL == session->m_pScheduler) ||
            (NULL == child_session->m_pScheduler))
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)

mfxStatus MFXInternalPseudoDisjoinSession(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    try
    {
        // detach all tasks from the scheduler
        session->m_pScheduler->WaitForTaskCompletion(session->m_pENCODE.get());
        session->m_pScheduler->WaitForTaskCompletion(session->m_pDECODE.get());
        session->m_pScheduler->WaitForTaskCompletion(session->m_pVPP.get());
        session->m_pScheduler->WaitForTaskCompletion(session->m_pENC.get());
        session->m_pScheduler->WaitForTaskCompletion(session->m_pPAK.get());
        session->m_pScheduler->WaitForTaskCompletion(session->m_plgGen.get());


        // just zeroing pointer to external scheduler (it will be released in external session close)
        session->m_pScheduler = NULL;

        mfxRes = session->RestoreScheduler();
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
          
        // core will released automatically 


    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        // check errors()
        if (NULL == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }

        if (NULL == session->m_pScheduler)
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXDisjoinSession(mfxSession session)


#endif