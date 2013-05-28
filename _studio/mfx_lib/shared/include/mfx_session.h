/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: mfx_session.h

\* ****************************************************************************** */

#if !defined(_MFX_SESSION_H)
#define _MFX_SESSION_H

#include <memory>

// base mfx headers
#include <mfxdefs.h>
#include <mfxstructures.h>
#include <mfxvideo++int.h>
#include <mfxplugin.h>
#include "mfx_common.h"

// private headers
#include <mfx_interface_scheduler.h>
#include <libmfx_core_operation.h>

// WARNING: please do not change the type of _mfxSession.
// It is declared as 'struct' in the main header.h



struct _mfxSession
{
    // Constructor
    _mfxSession(const mfxU32 adapterNum);
    // Destructor
    ~_mfxSession(void);

    // Clear state
    void Clear(void);

    // Initialize the session
    mfxStatus Init(mfxIMPL implInterface, mfxVersion *ver);

    // Attach to the original scheduler
    mfxStatus RestoreScheduler(void);

    // Declare session's components
    std::auto_ptr<VideoCORE> m_pCORE;
    std::auto_ptr<VideoENCODE> m_pENCODE;
    std::auto_ptr<VideoDECODE> m_pDECODE;
    std::auto_ptr<VideoVPP> m_pVPP;
    std::auto_ptr<VideoENC> m_pENC;
    std::auto_ptr<VideoPAK> m_pPAK;
    std::auto_ptr<VideoBRC> m_pBRC;
    std::auto_ptr<VideoUSER> m_pUSER;

    // Wrapper of interface for core object
    mfxCoreInterface m_coreInt;

    // Current implementation platform ID
    eMFXPlatform m_currentPlatform;
    // Current working HW adapter
    const
    mfxU32 m_adapterNum;
    // Current working interface (D3D9 or so)
    mfxIMPL m_implInterface;

    // Pointer to the scheduler interface being used
    MFXIScheduler *m_pScheduler;
    // Priority of the given session instance
    mfxPriority m_priority;
    // actual version of API
    mfxVersion  m_version;

    MFXIPtr<OperatorCORE> m_pOperatorCore;

    // if there are no Enc HW capabilities but HW library is used
    bool m_bIsHWENCSupport;

    // if there are no Dec HW capabilities but HW library is used
    bool m_bIsHWDECSupport;


    inline
    bool IsParentSession(void)
    {
        // parent session has multiple references to its scheduler.
        // regular session has 2 references to the scheduler.
        // child session has only 1 reference to it.
        return (2 < m_pSchedulerAllocated->GetNumRef());
    }

    inline
    bool IsChildSession(void)
    {
        // child session has different references to active and allocated
        // scheduler. regular session has 2 references to the scheduler.
        // child session has only 1 reference to it.
        return (1 == m_pSchedulerAllocated->GetNumRef());
    }


protected:
    // Release the object
    void Release(void);

    // this variable is used to deteremine
    // if the object really owns the scheduler.
    MFXIUnknown *m_pSchedulerAllocated;                         // (MFXIUnknown *) pointer to the scheduler allocated

private:
    // Assignment operator is forbidden
    _mfxSession & operator = (const _mfxSession &);
};

//
// DEFINES FOR IMPLICIT FUNCTIONS IMPLEMENTATION
//

#undef FUNCTION_IMPL
#define FUNCTION_IMPL(component, func_name, formal_param_list, actual_param_list) \
mfxStatus MFXVideo##component##_##func_name formal_param_list \
{ \
    mfxStatus mfxRes; \
    try \
    { \
        if (0 == session) \
        { \
            mfxRes = MFX_ERR_INVALID_HANDLE; \
        } \
        /* the absent components caused many issues in application. \
        check the pointer to avoid extra exceptions */ \
        else if (0 == session->m_p##component.get()) \
        { \
            mfxRes = MFX_ERR_NOT_INITIALIZED; \
        } \
        else \
        { \
            /* call the codec's method */ \
            mfxRes = session->m_p##component->func_name actual_param_list; \
        } \
    } \
    /* handle error(s) */ \
    catch(MFX_CORE_CATCH_TYPE) \
    { \
        /* set the default error value */ \
        mfxRes = MFX_ERR_NULL_PTR; \
        if (0 == session) \
        { \
            mfxRes = MFX_ERR_INVALID_HANDLE; \
        } \
        else if (0 == session->m_p##component.get()) \
        { \
            mfxRes = MFX_ERR_NOT_INITIALIZED; \
        } \
    } \
    return mfxRes; \
}

#undef FUNCTION_RESET_IMPL
#define FUNCTION_RESET_IMPL(component, func_name, formal_param_list, actual_param_list) \
mfxStatus MFXVideo##component##_##func_name formal_param_list \
{ \
    mfxStatus mfxRes; \
    try \
    { \
        if (0 == session) \
        { \
            mfxRes = MFX_ERR_INVALID_HANDLE; \
        } \
        /* the absent components caused many issues in application. \
        check the pointer to avoid extra exceptions */ \
        else if (0 == session->m_p##component.get()) \
        { \
            mfxRes = MFX_ERR_NOT_INITIALIZED; \
        } \
        else \
        { \
            /* wait until all tasks are processed */ \
            session->m_pScheduler->WaitForTaskCompletion(session->m_p##component.get()); \
            /* call the codec's method */ \
            mfxRes = session->m_p##component->func_name actual_param_list; \
        } \
    } \
    /* handle error(s) */ \
    catch(MFX_CORE_CATCH_TYPE) \
    { \
        /* set the default error value */ \
        mfxRes = MFX_ERR_NULL_PTR; \
        if (0 == session) \
        { \
            mfxRes = MFX_ERR_INVALID_HANDLE; \
        } \
        else if (0 == session->m_p##component.get()) \
        { \
            mfxRes = MFX_ERR_NOT_INITIALIZED; \
        } \
    } \
    return mfxRes; \
}

#endif // _MFX_SESSION_H
